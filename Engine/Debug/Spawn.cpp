/*********************************************************************************************
 \file      Spawn.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 80%
            erika.ishii (erika.ishii@digipen.edu) - Author, 20%

 \brief     Implements a debug ImGui panel for spawning prefabs at runtime. Provides
            interactive controls for position, size, color, texture, and batch spawning.
            Integrates with the engine’s prefab and factory systems.

 \details   This module allows developers to spawn prefabs interactively during runtime
            for testing and debugging. Prefabs are drawn from PrefabManager’s registry
            (master_copies). The user selects a prefab type, configures its parameters
            (transform, render, circle, sprite), and spawns instances via ImGui. Supports
            batch spawning with configurable offsets.
            Additionally:
              - Layer-aware spawning and an “isolate layer” toggle for selective rendering.
              - Sprite hookup via drag&drop from the Content Browser (texture key + handle).
              - Level quick save/list/load, with on-disk layer discovery to pre-populate UI.

            Design notes:
              * All filesystem ops use error_code variants where possible (no exceptions).
              * Never mutates master_copies; all spawns are clones into the live scene.
              * UI state is kept in module-static variables to persist across frames.

 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Debug/Spawn.h"
#include "Core/PathUtils.h"
#include "Selection.h"
#include "Debug/UndoStack.h"
#include "imgui.h"

// #include "Debug/Perf.h"
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#if defined(APIENTRY)
#  undef APIENTRY
#endif
#include <Windows.h>

#ifdef SendMessage
#  undef SendMessage   // prevent collisions with your ECS messaging system
#endif

// Engine & game headers
#include "Factory/Factory.h"                    // FACTORY, GOC, ComponentTypeId
#include "Composition/PrefabManager.h"          // master_copies, ClonePrefab

// Component types you currently support
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Resource_Manager/Resource_Manager.h"

// Player & Enemy Components
#include "Component/PlayerComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/PlayerHealthComponent.h"

#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Ai/DecisionTreeDefault.h"
#include "Physics/Dynamics/RigidBodyComponent.h"

#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <cstdio>
#include "Serialization/JsonSerialization.h"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif

namespace mygame {

    //=====================================================================================
    // Persistent panel state (module-level, survives across frames)
    //=====================================================================================

    /// Currently selected sprite texture key (dragged in from Content Browser).
    static std::string sSpriteTexKey;
    /// OpenGL texture handle for quick preview (0 if none).
    static unsigned sSpriteTextureId = 0;
    /// Texture key selected for rectangle-only prefabs.
    static std::string sRectangleTexKey;
    /// OpenGL texture handle preview for rectangle textures.
    static unsigned sRectangleTextureId = 0;
    /// Canonical project assets root for resolving relative asset keys.
    static std::filesystem::path sAssetsRoot;

    /// Whether we've built the level file list once this session.
    static bool gLevelFilesInitialized = false;
    /// Sorted list of candidate level files.
    static std::vector<std::string> gLevelFiles;
    /// Selected index into the level list.
    static int gSelectedLevelIndex = 0;
    /// Input buffer for “Level Name”.
    static char gLevelNameBuffer[128] = "level";
    /// Transient status line (text + isError flag) for level operations.
    static std::string gLevelStatusMessage;
    static bool gLevelStatusIsError = false;
    /// Directory where level JSON files are located (relative to executable).
    static const std::filesystem::path kLevelDirectory(Framework::ResolveDataPath(""));

    /// Active layer name used for newly spawned objects.
    static std::string gActiveLayer = "Default";
    /// If true, only render the active layer (downstream systems may consult ShouldRenderLayer()).
    static bool gIsolateActiveLayer = false;
    /// Editable buffer for layer name.
    static char gLayerInputBuffer[64] = "Default";

    /// Cache of layer names discovered in level files: filename -> layers.
    static std::unordered_map<std::string, std::vector<std::string>> gLevelLayers;
    /// Last level for which we synced the default active layer.
    static std::string gLastLayerSynchronizedLevel;

    using namespace Framework;

    //=====================================================================================
    // Anonymous helpers (formatting, normalization, IO-safe utilities)
    //=====================================================================================
    namespace {

        /*************************************************************************************
          \brief  Lowercase-copy utility (ASCII).
          \param  value Input string.
          \return Lowercased copy.
        *************************************************************************************/
        std::string ToLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        /*************************************************************************************
          \brief  Heuristic check for texture file extensions.
          \param  path Filesystem path to test.
          \return true if extension is .png/.jpg/.jpeg (case-insensitive).
        *************************************************************************************/
        bool IsTextureFile(const std::filesystem::path& path) {
            const auto lower = ToLower(path.extension().string());
            return lower == ".png" || lower == ".jpg" || lower == ".jpeg";
        }

        /*************************************************************************************
          \brief  Trim whitespace from both ends of a string (copying variant).
          \param  value String to trim.
          \return Trimmed copy.
        *************************************************************************************/
        std::string TrimCopy(std::string value) {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                [&](unsigned char c) { return !isSpace(c); }));
            value.erase(std::find_if(value.rbegin(), value.rend(),
                [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
            return value;
        }

        /*************************************************************************************
          \brief  Normalize a layer name for UI + engine (trim; fallback to "Default").
        *************************************************************************************/
        std::string NormalizeLayerUi(std::string_view name) {
            std::string trimmed = TrimCopy(std::string{ name });
            if (trimmed.empty())
                return "Default";
            return trimmed;
        }

        /*************************************************************************************
          \brief  Quick filter: treat a file as a level only if its name contains "level".
        *************************************************************************************/
        bool ContainsLevelKeyword(const std::string& name) {
            std::string lower;
            lower.resize(name.size());
            std::transform(name.begin(), name.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower.find("level") != std::string::npos;
        }

        /*************************************************************************************
          \brief  Read unique layer names directly from a level JSON file on disk.
          \param  levelPath Full path to the level .json.
          \return Sorted vector of unique layer names (always includes "Default").
        *************************************************************************************/
        std::vector<std::string> ExtractLayersFromLevel(const std::filesystem::path& levelPath) {
            std::unordered_set<std::string> unique;

            JsonSerializer s{};
            if (s.Open(levelPath.string()) && s.IsGood()) {
                if (s.EnterObject("Level")) {
                    if (s.EnterArray("GameObjects")) {
                        const size_t n = s.ArraySize();
                        for (size_t i = 0; i < n; ++i) {
                            if (!s.EnterIndex(i)) continue; // now at GameObjects[i]
                            std::string layer;
                            if (s.HasKey("layer")) s.ReadString("layer", layer);
                            unique.insert(NormalizeLayerUi(layer));
                            s.ExitObject();
                        }
                        s.ExitArray();
                    }
                    s.ExitObject();
                }
            }

            unique.insert("Default"); // always present
            std::vector<std::string> out(unique.begin(), unique.end());
            std::sort(out.begin(), out.end());
            return out;
        }

        /*************************************************************************************
          \brief  Scan level directory to refresh file list and per-file layer cache.
        *************************************************************************************/
        void RefreshLevelFileList() {
            gLevelFiles.clear();
            gLevelLayers.clear();

            std::error_code ec;
            if (!std::filesystem::exists(kLevelDirectory, ec))
                return;

            for (auto const& entry : std::filesystem::directory_iterator(kLevelDirectory, ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                const auto& path = entry.path();
                if (path.extension() != ".json") continue;
                const std::string filename = path.filename().string();
                if (!ContainsLevelKeyword(filename)) continue;

                gLevelFiles.push_back(filename);
                // Cache layers to pre-populate the UI.
                gLevelLayers[filename] = ExtractLayersFromLevel(path);
            }

            std::sort(gLevelFiles.begin(), gLevelFiles.end());
            if (gSelectedLevelIndex >= static_cast<int>(gLevelFiles.size()))
                gSelectedLevelIndex = gLevelFiles.empty() ? 0 : static_cast<int>(gLevelFiles.size() - 1);
        }

        /*************************************************************************************
          \brief  Set active layer and ensure it exists in the LayerManager.
          \param  name Desired layer name (will be normalized).
        *************************************************************************************/
        void ApplyActiveLayer(std::string_view name) {
            gActiveLayer = NormalizeLayerUi(name);
            std::snprintf(gLayerInputBuffer, sizeof(gLayerInputBuffer), "%s", gActiveLayer.c_str());
            if (FACTORY) FACTORY->Layers().EnsureLayer(gActiveLayer);
        }

        /*************************************************************************************
          \brief  Populate cache for a level's layers if missing.
        *************************************************************************************/
        void EnsureLevelLayersCached(const std::string& levelKey, const std::filesystem::path& levelPath) {
            if (levelKey.empty() || levelPath.empty()) return;
            if (gLevelLayers.find(levelKey) == gLevelLayers.end())
                gLevelLayers[levelKey] = ExtractLayersFromLevel(levelPath);
        }

        /*************************************************************************************
          \brief  Choose a reasonable default UI layer for a given level.
          \return "Default" if present; otherwise the first available layer name.
        *************************************************************************************/
        std::string ChooseDefaultLayerForLevel(const std::string& key) {
            auto it = gLevelLayers.find(key);
            if (it == gLevelLayers.end()) return "Default";
            bool hasDefault = false;
            for (auto& nm : it->second) if (NormalizeLayerUi(nm) == "Default") hasDefault = true;
            return hasDefault ? "Default" : (it->second.empty() ? "Default" : NormalizeLayerUi(it->second.front()));
        }

        /*************************************************************************************
          \brief  Sync the UI's active layer once per level selection.
        *************************************************************************************/
        void SyncActiveLayerWithLevel(const std::string& levelKey) {
            if (levelKey.empty()) return;
            if (gLastLayerSynchronizedLevel == levelKey) return; // already synced
            ApplyActiveLayer(ChooseDefaultLayerForLevel(levelKey));
            gLastLayerSynchronizedLevel = levelKey;
        }

        /*************************************************************************************
          \brief  Construct absolute path to a level JSON from a filename.
        *************************************************************************************/
        std::filesystem::path LevelFilePath(const std::string& filename) {
            return kLevelDirectory / filename;
        }

        /*************************************************************************************
          \brief  Check whether an object pointer refers to a master prefab template.
        *************************************************************************************/
        bool IsMasterObject(GOC* obj) {
            for (auto const& kv : master_copies) {
                if (kv.second.get() == obj)
                    return true;
            }
            return false;
        }

        /*************************************************************************************
          \brief  Gather scene objects excluding master templates.
          \return Vector of non-master GOC*.
        *************************************************************************************/
        std::vector<GOC*> CollectNonMasterObjects() {
            std::vector<GOC*> result;
            if (!FACTORY)
                return result;
            result.reserve(FACTORY->Objects().size());
            for (auto& [id, objPtr] : FACTORY->Objects()) {
                (void)id;
                auto* obj = objPtr.get();
                if (!obj) continue;
                if (IsMasterObject(obj)) continue;
                result.push_back(obj);
            }
            return result;
        }

        /*************************************************************************************
         \brief  Destroy an object while recording the deletion for undo.
         \param  obj Pointer to the object to delete (ignored if null).
       *************************************************************************************/
        void DestroyWithUndo(GOC* obj)
        {
            if (!obj)
                return;
            mygame::editor::RecordObjectDeleted(*obj);
            // Prefer the factory's destroy path so deferred cleanup stays consistent with
             // editor expectations (ID recycling, selection clearing, etc.). Fallback to
             // the object's own destroy in case the factory isn't available (defensive).
            if (Framework::FACTORY)
                Framework::FACTORY->Destroy(obj);
            else
                obj->Destroy();
        }
    } // namespace

       /*************************************************************************************
      \brief  Apply SpawnSettings to an existing object.
      \param  obj   Target object (already created).
      \param  s     Spawn settings (position/size/circle/sprite/rigidbody/player/enemy).
      \param  index Batch index (for step offsets when spawning new).
      \param  applyTransformAndLayer
                    If true, apply transform + step offsets and (in caller) layer.
                    If false, keep existing transform and layer.
    *************************************************************************************/
    static void ApplySpawnSettingsToObject(GOC& obj,
        SpawnSettings const& s,
        int index,
        bool applyTransformAndLayer)
    {
        // Transform: only for new spawns (we don't want to teleport existing instances)
        if (auto* tr = obj.GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent)) {
            if (applyTransformAndLayer && s.overridePrefabTransform) {
                tr->x = s.x + s.stepX * index;
                tr->y = s.y + s.stepY * index;
                tr->rot = s.rot;
            }

            if (applyTransformAndLayer) {
                tr->x += s.stepX * index;
                tr->y += s.stepY * index;
            }
        }

        auto* spriteComp = obj.GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent);

        // Rectangle render: override size, color, and optional texture when requested
        if (auto* rc = obj.GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent)) {
            if (s.overridePrefabSize) {
                rc->w = s.w;
                rc->h = s.h;
            }

            // Always push tint from SpawnSettings (for both new and existing)
            rc->r = s.rgba[0];
            rc->g = s.rgba[1];
            rc->b = s.rgba[2];
            rc->a = s.rgba[3];

            if (s.overridePrefabVisible) {
                rc->visible = s.visible;
            }

            // Rectangle-only texture override when prefab has no SpriteComponent
            if (!sRectangleTexKey.empty() && !spriteComp) {
                rc->texture_key = sRectangleTexKey;
                rc->texture_id = Resource_Manager::getTexture(sRectangleTexKey);
            }
        }

        // Circle: inherit JSON unless override is ON
        if (auto* cc = obj.GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent)) {
            if (s.overridePrefabCircle) {
                cc->radius = s.radius;
            }
            cc->r = s.rgba[0];
            cc->g = s.rgba[1];
            cc->b = s.rgba[2];
            cc->a = s.rgba[3];
        }

        // Sprite: only override if a texture key has been chosen
        if (spriteComp) {
            if (!sSpriteTexKey.empty()) {
                spriteComp->texture_key = sSpriteTexKey;
                spriteComp->texture_id = Resource_Manager::getTexture(sSpriteTexKey);
            }
        }

        // RigidBody: velocity and collider overrides
        if (auto* rb = obj.GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent)) {
            if (s.overridePrefabVelocity) {
                rb->velX = s.rbVelX;
                rb->velY = s.rbVelY;
            }
            if (s.overridePrefabCollider) {
                rb->width = s.rbWidth;
                rb->height = s.rbHeight;
            }
        }

        // Enemy Attack override
        if (auto* atk = obj.GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent)) {
            if (s.overrideEnemyAttack) {
                atk->damage = s.attackDamage;
                atk->attack_speed = s.attack_speed;
            }
        }

        // Enemy Health override
        if (auto* eh = obj.GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent)) {
            if (s.overrideEnemyHealth) {
                eh->enemyMaxhealth = s.enemyMaxhealth;
                eh->enemyHealth = s.enemyHealth;
            }
            else {
                // keep JSON max, set current = max
                eh->enemyHealth = eh->enemyMaxhealth;
            }
        }

        // Player health
        if (auto* health = obj.GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent)) {
            if (s.overridePlayerHealth) {
                health->playerHealth = s.playerHealth;
                health->playerMaxhealth = s.playerMaxhealth;
            }
            else {
                health->playerHealth = health->playerMaxhealth;
            }
        }

        // Player attack
        if (auto* attack = obj.GetComponentType<PlayerAttackComponent>(ComponentTypeId::CT_PlayerAttackComponent)) {
            if (s.overridePlayerAttack) {
                attack->damage = s.attackDamagep;
                attack->attack_speed = s.attack_speedp;
            }
        }

  

        // NOTE: layer is *not* changed here. For new spawns we still set layer in SpawnOnePrefab().
    }
 
    /*************************************************************************************
     \brief  Helper to spawn a single prefab and apply current SpawnSettings.
     \param  prefab Name of the prefab to clone (must exist in master_copies).
     \param  s      Spawn settings (position/size/circle/sprite/rigidbody/player/enemy).
     \param  index  Batch index (applied to stepX/stepY offsets).
     \note   Newly spawned object is assigned to the current active layer.
   *************************************************************************************/
    static void SpawnOnePrefab(const char* prefab, SpawnSettings const& s, int index) {
        GOC* obj = ClonePrefab(prefab);
        if (!obj) return;

        // For new objects: full application (including transform offsets)
        ApplySpawnSettingsToObject(*obj, s, index, /*applyTransformAndLayer*/ true);

        // Assign layer on creation
        obj->SetLayerName(gActiveLayer);


        // Track the creation so the editor undo stack can remove it if requested.
        mygame::editor::RecordObjectCreated(*obj);
    }

    /*************************************************************************************
      \brief  Set the assets root used to resolve relative sprite paths dropped into the UI.
      \param  root Absolute or relative path to the project's assets directory.
      \note   Path is canonicalized when possible (weakly_canonical).
    *************************************************************************************/
    void SetSpawnPanelAssetsRoot(const std::filesystem::path& root) {
        if (root.empty()) {
            sAssetsRoot.clear();
            return;
        }
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(root, ec);
        sAssetsRoot = ec ? root : canonical;
    }

    /*************************************************************************************
     \brief  Internal helper to resolve a texture key + GL handle from a drag-drop path.
      \param  relativePath Path relative to the assets root (absolute paths are rebased).
      \param  outKey       Destination string for the resolved key.
      \param  outHandle    Destination handle for the GL texture id.
      \return true if a texture was successfully resolved/loaded; false otherwise.
    *************************************************************************************/
    static bool LoadTextureSelection(const std::filesystem::path& relativePath,
        std::string& outKey, unsigned& outHandle) {
        if (relativePath.empty() || sAssetsRoot.empty())
            return false;

        std::filesystem::path relative = relativePath;
        if (relative.is_absolute()) {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(relative, ec);
            if (ec)
                return false;
            relative = canonical.lexically_relative(sAssetsRoot);
        }

        if (relative.empty())
            return false;

        std::error_code ec;
        auto absolute = std::filesystem::weakly_canonical(sAssetsRoot / relative, ec);
        if (ec)
            absolute = sAssetsRoot / relative;

        if (!std::filesystem::exists(absolute))
            return false;
        if (!std::filesystem::is_regular_file(absolute))
            return false;
        if (!IsTextureFile(absolute))
            return false;

        std::string key = relative.generic_string();
        if (key.empty())
            return false;

        if (Resource_Manager::resources_map.find(key) == Resource_Manager::resources_map.end())
            Resource_Manager::load(key, absolute.string());

        unsigned handle = Resource_Manager::getTexture(key);
        if (handle == 0)
            return false;

        outKey = std::move(key);
        outHandle = handle;
        return true;
    }

    /*************************************************************************************
      \brief  Use a texture from the Content Browser for sprite override.
      \param  relativePath Path relative to the assets root (accepts absolute; will rebase).
      \note   Loads the texture into Resource_Manager if not present; updates preview handle.
    *************************************************************************************/
    void UseSpriteFromAsset(const std::filesystem::path& relativePath) {
        (void)LoadTextureSelection(relativePath, sSpriteTexKey, sSpriteTextureId);
    }

    /*************************************************************************************
      \brief  Use a texture from the Content Browser for rectangle overrides.
      \param  relativePath Path relative to the assets root (accepts absolute; will rebase).
    *************************************************************************************/
    static void UseRectangleTextureFromAsset(const std::filesystem::path& relativePath) {
        (void)LoadTextureSelection(relativePath, sRectangleTexKey, sRectangleTextureId);
    }

    /*************************************************************************************
      \brief  Clear the current sprite override (key + preview handle).
    *************************************************************************************/
    void ClearSpriteTexture() {
        sSpriteTexKey.clear();
        sSpriteTextureId = 0;
    }

    /*************************************************************************************
     \brief  Clear the current rectangle override (key + preview handle).
   *************************************************************************************/
    static void ClearRectangleTexture() {
        sRectangleTexKey.clear();
        sRectangleTextureId = 0;
    }

    /*************************************************************************************
      \brief  Get the currently selected sprite texture key.
      \return Texture key string (empty if none).
    *************************************************************************************/
    const std::string& CurrentSpriteTextureKey() {
        return sSpriteTexKey;
    }

    /*************************************************************************************
      \brief  Get the GL handle of the current sprite texture (for preview).
      \return OpenGL texture id (0 if none).
    *************************************************************************************/
    unsigned CurrentSpriteTextureHandle() {
        return sSpriteTextureId;
    }

    /*************************************************************************************
      \brief  Get the UI’s active layer name (used on spawn).
    *************************************************************************************/
    const std::string& ActiveLayerName() {
        return gActiveLayer;
    }

    /*************************************************************************************
      \brief  Check if “isolate active layer” mode is enabled.
      \return true to render only ActiveLayerName(); false to render all layers.
    *************************************************************************************/
    bool IsLayerIsolationEnabled() {
        return gIsolateActiveLayer;
    }

    /*************************************************************************************
      \brief  Helper consulted by rendering systems to decide if a layer should draw.
      \param  layerName Candidate layer for rendering.
      \return true if drawing is permitted given the isolate toggle; false otherwise.
    *************************************************************************************/
    bool ShouldRenderLayer(const std::string& layerName) {
        if (!gIsolateActiveLayer)
            return true;
        return NormalizeLayerUi(layerName) == NormalizeLayerUi(gActiveLayer);
    }

    //=====================================================================================
    // Panel-local UI state
    //=====================================================================================

    /// Default prefab choice shown in the combo box.
    static std::string gSelectedPrefab = "Rect";
    /// Live settings bound to ImGui controls.
    static SpawnSettings gS;
    /// Sync flag to copy prefab dimensions into the panel once upon selection change.
    static bool gPendingPrefabSizeSync = true;

    // Window open flag and initial sizing anchors (optional).
    bool  opened = true;
    float x = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float y = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    /// Prefab type to clear when “Clear Selected Prefab” is pressed.
    static std::string gClearPrefab = "Rect";
    /// Optional selected object reference (unused externally; kept for future tools).
    static GOC* gSelectedObject = nullptr;
    /// Mirrors the prefab to clear in the dropdown.
    static std::string gSelectedPrefabToClear = gSelectedPrefab;

    /*************************************************************************************
      \brief  Draw the Spawn panel UI and perform actions (spawn/clear/save/load).
      \note   Must be called every frame while the tools UI is visible.
    *************************************************************************************/
    void DrawSpawnPanel() {
        //ImGui::SetNextWindowSize(ImVec2(x / 4, y/2.5 ), ImGuiCond_Once);
        ImGui::Begin("Spawn", &opened);

        // One-time level list population and LastLevelName bootstrap.
        if (!gLevelFilesInitialized) {
            RefreshLevelFileList();
            gLevelFilesInitialized = true;
            if (FACTORY && !FACTORY->LastLevelName().empty()) {
                std::snprintf(gLevelNameBuffer, IM_ARRAYSIZE(gLevelNameBuffer), "%s",
                    FACTORY->LastLevelName().c_str());
            }
        }

        // === Prefab Dropdown ===
        {
            const char* preview = gSelectedPrefab.c_str();
            if (ImGui::BeginCombo("Prefab", preview)) {
                for (auto const& kv : master_copies) {
                    bool sel = (kv.first == gSelectedPrefab);
                    if (ImGui::Selectable(kv.first.c_str(), sel)) { // Select prefab
                        if (gSelectedPrefab != kv.first) {
                            gSelectedPrefab = kv.first;
                            gPendingPrefabSizeSync = true;
                        }
                    }
                    if (sel) ImGui::SetItemDefaultFocus();        // Keep focus on selected prefab
                }
                ImGui::EndCombo();
            }
        }

        // Resolve master prefab reference
        GOC* master = nullptr;
        if (auto it = master_copies.find(gSelectedPrefab); it != master_copies.end())
            master = it->second.get();

        if (!master) {
            ImGui::TextDisabled("Missing master for '%s'", gSelectedPrefab.c_str());
            ImGui::End();
            return;
        }

        // --- Layer UI -------------------------------------------------------
        // Merge candidate layer names from runtime + on-disk levels + current selection
        std::unordered_set<std::string> layerSet;

        if (FACTORY) {
            for (auto const& nm : FACTORY->Layers().LayerNames())
                layerSet.insert(NormalizeLayerUi(nm));
        }

        // From last loaded level
        std::string currentLevelKey;
        if (FACTORY) {
            auto const& lastPath = FACTORY->LastLevelPath();
            if (!lastPath.empty())
                currentLevelKey = lastPath.filename().string();
        }
        if (!currentLevelKey.empty()) {
            if (auto it = gLevelLayers.find(currentLevelKey); it != gLevelLayers.end()) {
                for (auto const& nm : it->second)
                    layerSet.insert(NormalizeLayerUi(nm));
            }
        }

        // From the currently selected level in dropdown (before any load)
        if (layerSet.empty() && !gLevelFiles.empty()) {
            const std::string& sel = gLevelFiles[gSelectedLevelIndex];
            if (auto it = gLevelLayers.find(sel); it != gLevelLayers.end()) {
                for (auto const& nm : it->second)
                    layerSet.insert(NormalizeLayerUi(nm));
            }
        }

        // Ensure we have at least the active layer + Default
        layerSet.insert(NormalizeLayerUi(gActiveLayer));
        layerSet.insert("Default");

        std::vector<std::string> layerNames(layerSet.begin(), layerSet.end());
        std::sort(layerNames.begin(), layerNames.end());

        std::string normalizedActive = NormalizeLayerUi(gActiveLayer);

        if (ImGui::BeginCombo("Active Layer", normalizedActive.c_str())) {
            for (const auto& layerName : layerNames) {
                bool selected = (layerName == normalizedActive);
                if (ImGui::Selectable(layerName.c_str(), selected)) {
                    ApplyActiveLayer(layerName);
                    normalizedActive = gActiveLayer;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Editable layer field; applied by button press
        ImGui::InputTextWithHint("Layer Name", "Default", gLayerInputBuffer, sizeof(gLayerInputBuffer));

        if (ImGui::Button("Apply Layer")) {
            std::string fromInput = NormalizeLayerUi(gLayerInputBuffer);
            ApplyActiveLayer(fromInput);
            normalizedActive = gActiveLayer;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset##LayerSelection")) {
            ApplyActiveLayer("Default");
            normalizedActive = gActiveLayer;
        }

        ImGui::Checkbox("Render only active layer", &gIsolateActiveLayer);

        // Component presence flags for conditional UI
        const bool hasTransform =
            (master->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent) != nullptr);
        auto* masterRender = master->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent);
        const bool hasRender = (masterRender != nullptr);
        auto* masterCircle = master->GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent);
        const bool hasCircle = (masterCircle != nullptr);
        const bool hasRigidBody =
            (master->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent) != nullptr);
        const bool hasEnemyAttack =
            (master->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent) != nullptr);
        const bool hasEnemyHealth =
            (master->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent) != nullptr);
        const bool hasPlayerAttack =
            (master->GetComponentType<PlayerAttackComponent>(ComponentTypeId::CT_PlayerAttackComponent) != nullptr);
        const bool hasPlayerHealth =
            (master->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent) != nullptr);

        // One-time sync from master to panel when prefab changes
        if (gPendingPrefabSizeSync) {
            if (auto* trm = master->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent)) {
                gS.x = trm->x; gS.y = trm->y; gS.rot = trm->rot;
            }
            if (auto* ccm = master->GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent)) {
                gS.radius = ccm->radius;
            }
            if (auto* atm = master->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent)) {
                gS.attackDamage = atm->damage;
                gS.attack_speed = atm->attack_speed;
            }
            if (auto* ehm = master->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent)) {
                gS.enemyMaxhealth = ehm->enemyMaxhealth;
                gS.enemyHealth = ehm->enemyMaxhealth; // start current at max
            }
            if (auto* mrb = master->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent)) {
                gS.rbWidth = mrb->width; gS.rbHeight = mrb->height;
                gS.rbVelX = mrb->velX;  gS.rbVelY = mrb->velY;
            }
            if (auto* rc = master->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent)) {
                gS.w = rc->w; gS.h = rc->h;
                gS.visible = rc->visible;
                gS.overridePrefabVisible = false;
            }
            if (auto* atm = master->GetComponentType<PlayerAttackComponent>(ComponentTypeId::CT_PlayerAttackComponent)) {
                gS.attackDamagep = atm->damage;
                gS.attack_speedp = atm->attack_speed;
            }
            if (auto* ehm = master->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent)) {
                gS.playerMaxhealth = ehm->playerMaxhealth;
                gS.playerHealth = ehm->playerMaxhealth; // start current at max
            }
            gPendingPrefabSizeSync = false;
        }

        const bool hasSprite =
            (master->GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent) != nullptr);

        // === Sprite Controls ===
        if (hasSprite) {
            ImGui::SeparatorText("Sprite");
            const char* previewLabel = sSpriteTexKey.empty() ? "<drop texture>" : sSpriteTexKey.c_str();
            ImGui::Text("Texture: %s", previewLabel);

            ImVec2 avail = ImGui::GetContentRegionAvail();
            float previewEdge = std::min(128.0f, avail.x);
            ImVec2 previewSize(previewEdge, previewEdge);

            ImGui::PushID("SpritePreview");
            if (sSpriteTextureId != 0) {
                ImGui::Image((ImTextureID)(void*)(intptr_t)sSpriteTextureId, previewSize, ImVec2(0, 1), ImVec2(1, 0));
            }
            else {
                ImGui::Button("Drop Texture Here", previewSize);
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_PATH")) {
                    if (payload->Data && payload->DataSize > 0) {
                        std::string relative(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                        UseSpriteFromAsset(relative);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();

            if (sSpriteTextureId != 0) {
                if (ImGui::Button("Clear Sprite Texture"))
                    ClearSpriteTexture();
            }
            else {
                ImGui::TextDisabled("Drag from the Content Browser or drop files into the editor window.");
            }
        }

        // === Transform Controls ===
        if (hasTransform) {
            ImGui::SeparatorText("Transform");
            ImGui::Checkbox("Override prefab transform", &gS.overridePrefabTransform);
            if (!gS.overridePrefabTransform) ImGui::BeginDisabled();
            ImGui::DragFloat("x", &gS.x, 0.005f, 0.0f, 1.0f);
            ImGui::DragFloat("y", &gS.y, 0.005f, 0.0f, 1.0f);
            ImGui::DragFloat("rot", &gS.rot, 0.01f, -3.14159f, 3.14159f);
            if (!gS.overridePrefabTransform) ImGui::EndDisabled();
        }

        // === Rectangle Controls ===
        if (hasRender) {
            ImGui::SeparatorText("Size");
            // When toggled OFF, copy prefab size back
            if (ImGui::Checkbox("Override prefab size", &gS.overridePrefabSize)) {
                if (!gS.overridePrefabSize && masterRender) {
                    gS.w = masterRender->w;
                    gS.h = masterRender->h;
                }
            }

            const bool disableSizeControls = !gS.overridePrefabSize;
            if (disableSizeControls) ImGui::BeginDisabled();
            ImGui::DragFloat("w", &gS.w, 0.005f, 0.01f, 1.0f);
            ImGui::DragFloat("h", &gS.h, 0.005f, 0.01f, 1.0f);
            if (disableSizeControls) ImGui::EndDisabled();
            //Visibility controls-
                ImGui::SeparatorText("Visibility");
            if (ImGui::Checkbox("Override prefab visibility", &gS.overridePrefabVisible)) {
                if (!gS.overridePrefabVisible && masterRender) {
                    // When turning override OFF, reset to prefab's visibility
                    gS.visible = masterRender->visible;
                }
            }

            if (!gS.overridePrefabVisible) ImGui::BeginDisabled();
            ImGui::Checkbox("Visible", &gS.visible);
            if (!gS.overridePrefabVisible) ImGui::EndDisabled();

            if (!hasSprite) {
                ImGui::SeparatorText("Texture");
                const bool hasRectTexture = !sRectangleTexKey.empty();
                const char* previewLabel = hasRectTexture ? sRectangleTexKey.c_str() : "<drop texture>";
                ImGui::Text("Texture: %s", previewLabel);

                ImVec2 avail = ImGui::GetContentRegionAvail();
                float previewEdge = std::min(128.0f, avail.x);
                ImVec2 previewSize(previewEdge, previewEdge);

                ImGui::PushID("RectangleTexturePreview");
                if (hasRectTexture && sRectangleTextureId != 0) {
                    ImGui::Image((ImTextureID)(void*)(intptr_t)sRectangleTextureId, previewSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                else {
                    ImGui::Button("Drop Texture Here", previewSize);
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_PATH")) {
                        if (payload->Data && payload->DataSize > 0) {
                            std::string relative(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                            UseRectangleTextureFromAsset(relative);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::PopID();

                if (hasRectTexture && sRectangleTextureId != 0) {
                    if (ImGui::Button("Clear Rectangle Texture")) {
                        ClearRectangleTexture();
                    }
                }
                else {
                    ImGui::TextDisabled("Drag from the Content Browser or drop files into the editor window.");
                }
            }
        }

        // === Circle Controls ===
        if (hasCircle) {
            ImGui::SeparatorText("Circle");
            ImGui::Checkbox("Override prefab circle", &gS.overridePrefabCircle);
            if (!gS.overridePrefabCircle) ImGui::BeginDisabled();
            ImGui::DragFloat("radius", &gS.radius, 0.005f, 0.01f, 1.0f);
            if (!gS.overridePrefabCircle) ImGui::EndDisabled();
        }

        // === RigidBody Controls ===
        if (hasRigidBody) {
            ImGui::SeparatorText("RigidBody");

            // Collider override
            if (ImGui::Checkbox("Override prefab collider", &gS.overridePrefabCollider)) {
                if (!gS.overridePrefabCollider) {
                    if (auto* mrb = master->GetComponentType<RigidBodyComponent>(
                        ComponentTypeId::CT_RigidBodyComponent)) {
                        gS.rbWidth = mrb->width;
                        gS.rbHeight = mrb->height;
                    }
                }
            }
            if (!gS.overridePrefabCollider) ImGui::BeginDisabled();
            ImGui::DragFloat("Collider Width", &gS.rbWidth, 0.005f, 0.01f, 2.0f);
            ImGui::DragFloat("Collider Height", &gS.rbHeight, 0.005f, 0.01f, 2.0f);
            if (!gS.overridePrefabCollider) ImGui::EndDisabled();

            ImGui::Separator();

            // Velocity override (independent)
            if (ImGui::Checkbox("Override prefab velocity", &gS.overridePrefabVelocity)) {
                if (!gS.overridePrefabVelocity) {
                    if (auto* mrb = master->GetComponentType<RigidBodyComponent>(
                        ComponentTypeId::CT_RigidBodyComponent)) {
                        gS.rbVelX = mrb->velX;
                        gS.rbVelY = mrb->velY;
                    }
                }
            }
            if (!gS.overridePrefabVelocity) ImGui::BeginDisabled();
            ImGui::DragFloat("Velocity X", &gS.rbVelX, 0.01f, -100.f, 100.f);
            ImGui::DragFloat("Velocity Y", &gS.rbVelY, 0.01f, -100.f, 100.f);
            if (!gS.overridePrefabVelocity) ImGui::EndDisabled();
        }

        // === Enemy Attack ===
        if (hasEnemyAttack) {
            ImGui::SeparatorText("Enemy Attack");
            ImGui::Checkbox("Override prefab attack", &gS.overrideEnemyAttack);
            if (!gS.overrideEnemyAttack) ImGui::BeginDisabled();
            ImGui::DragInt("Damage", &gS.attackDamage, 1, 0, 100000);
            ImGui::DragFloat("Attack Speed (s)", &gS.attack_speed, 0.01f, 0.01f, 10.0f);
            if (!gS.overrideEnemyAttack) ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::TextDisabled("(lower = faster)");
        }

        // === Enemy Health ===
        if (hasEnemyHealth) {
            ImGui::SeparatorText("Enemy Health");
            ImGui::Checkbox("Override prefab health", &gS.overrideEnemyHealth);
            if (!gS.overrideEnemyHealth) ImGui::BeginDisabled();
            ImGui::DragInt("Health", &gS.enemyHealth, 1, 0, 100000);
            ImGui::DragInt("HealthMax", &gS.enemyMaxhealth, 1, 0, 100000);
            if (!gS.overrideEnemyHealth) ImGui::EndDisabled();
        }

        // === Player Attack ===
        if (hasPlayerAttack) {
            ImGui::SeparatorText("Player Attack");
            ImGui::Checkbox("Override prefab attack", &gS.overridePlayerAttack);
            if (!gS.overridePlayerAttack) ImGui::BeginDisabled();
            ImGui::DragInt("Damage", &gS.attackDamagep, 1, 0, 100000);
            ImGui::DragFloat("Attack Speed (s)", &gS.attack_speedp, 0.01f, 0.01f, 10.0f);
            if (!gS.overridePlayerAttack) ImGui::EndDisabled();
            ImGui::SameLine(); ImGui::TextDisabled("(lower = faster)");
        }

        // === Player Health ===
        if (hasPlayerHealth) {
            ImGui::SeparatorText("Player Health");
            ImGui::Checkbox("Override prefab health", &gS.overridePlayerHealth);
            if (!gS.overridePlayerHealth) ImGui::BeginDisabled();
            ImGui::DragInt("Health", &gS.playerHealth, 1, 0, 100000);
            ImGui::DragInt("HealthMax", &gS.playerMaxhealth, 1, 0, 100000);
            if (!gS.overridePlayerHealth) ImGui::EndDisabled();
        }

        // === Color ===
        if (hasRender || hasCircle) {
            ImGui::SeparatorText("Color");
            ImGui::ColorEdit4("rgba", gS.rgba);
        }

        // === Batch ===
        ImGui::SeparatorText("Batch");
        ImGui::DragInt("count", &gS.count, 1, 1, 500);
        ImGui::DragFloat("stepX", &gS.stepX, 0.005f);
        ImGui::DragFloat("stepY", &gS.stepY, 0.005f);

        // === Levels ===
        ImGui::SeparatorText("Levels");
        ImGui::InputText("Level Name", gLevelNameBuffer, IM_ARRAYSIZE(gLevelNameBuffer));

        ImGui::SameLine();
        if (ImGui::Button("Save Level")) {
            std::string levelName = TrimCopy(gLevelNameBuffer);
            if (levelName.empty()) {
                gLevelStatusMessage = "Level name cannot be empty";
                gLevelStatusIsError = true;
            }
            else {
                std::string filename = levelName;
                if (filename.find('.') == std::string::npos)
                    filename += ".json";
                std::filesystem::path levelPath = LevelFilePath(filename);
                std::string levelLabel = std::filesystem::path(filename).stem().string();
                bool saved = FACTORY->SaveLevel(levelPath.string(), levelLabel);
                if (saved) {
                    gLevelStatusMessage = "Saved level to " + levelPath.string();
                    gLevelStatusIsError = false;
                    RefreshLevelFileList();
                }
                else {
                    gLevelStatusMessage = "Failed to save level to " + levelPath.string();
                    gLevelStatusIsError = true;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh Level List")) {
            RefreshLevelFileList();
        }

        // Fill cache and sync active layer with last loaded level
        if (FACTORY) {
            const auto& lastPath = FACTORY->LastLevelPath();
            if (!lastPath.empty()) {
                const std::string key = lastPath.filename().string();
                EnsureLevelLayersCached(key, lastPath);
                SyncActiveLayerWithLevel(key);
            }
        }

        if (!gLevelFiles.empty()) {
            const char* preview = gLevelFiles[gSelectedLevelIndex].c_str();
            if (ImGui::BeginCombo("Available Levels", preview)) {
                for (size_t i = 0; i < gLevelFiles.size(); ++i) {
                    bool selected = (static_cast<int>(i) == gSelectedLevelIndex);
                    if (ImGui::Selectable(gLevelFiles[i].c_str(), selected))
                        gSelectedLevelIndex = static_cast<int>(i);
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Show the layer names found in the selected level for quick verification
            const std::string& selectedLevel = gLevelFiles[gSelectedLevelIndex];
            if (auto it = gLevelLayers.find(selectedLevel); it != gLevelLayers.end()) {
                ImGui::Spacing();
                ImGui::Text("Layers in %s:", selectedLevel.c_str());
                ImGui::Indent();
                if (!it->second.empty()) {
                    for (auto const& layerName : it->second)
                        ImGui::BulletText("%s", layerName.c_str());
                }
                else {
                    ImGui::TextDisabled("(none)");
                }
                ImGui::Unindent();
            }

            if (ImGui::Button("Load Selected Level")) {
                const std::string selected = gLevelFiles[gSelectedLevelIndex];
                std::filesystem::path levelPath = LevelFilePath(selected);
                std::error_code ec;
                if (!std::filesystem::exists(levelPath, ec)) {
                    gLevelStatusMessage = "Level file not found: " + levelPath.string();
                    gLevelStatusIsError = true;
                }
                else {
                    auto toKill = CollectNonMasterObjects();
                    for (auto* obj : toKill)
                        DestroyWithUndo(obj);
                    FACTORY->Update(0.0f);

                    FACTORY->CreateLevel(levelPath.string());
                    // After loading, cache layers and sync the UI's active layer
                    gLevelLayers[selected] = ExtractLayersFromLevel(levelPath);
                    gLastLayerSynchronizedLevel.clear();
                    SyncActiveLayerWithLevel(selected);

                    size_t count = FACTORY->LastLevelObjects().size();
                    gLevelStatusMessage = "Loaded level from " + levelPath.string() +
                        " (" + std::to_string(count) + " objects)";
                    gLevelStatusIsError = false;
                }
            }
        }
        else {
            ImGui::TextDisabled("No level files found in %s", kLevelDirectory.string().c_str());
        }

        // Status line (green for success/info, red for error)
        if (!gLevelStatusMessage.empty()) {
            ImVec4 color = gLevelStatusIsError ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
                : ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
            ImGui::TextColored(color, "%s", gLevelStatusMessage.c_str());
        }

        // Keep the clear selection valid
        if (!master_copies.empty()) {
            if (master_copies.find(gSelectedPrefabToClear) == master_copies.end())
                gSelectedPrefabToClear = master_copies.begin()->first;
        }
        else {
            gSelectedPrefabToClear.clear();
        }

        // === Clear Prefab Selection ===
        if (!gSelectedPrefabToClear.empty()) {
            const char* clearPreview = gSelectedPrefabToClear.c_str();
            if (ImGui::BeginCombo("Clear Prefab", clearPreview)) {
                for (auto const& kv : master_copies) {
                    bool sel = (kv.first == gSelectedPrefabToClear);
                    if (ImGui::Selectable(kv.first.c_str(), sel))
                        gSelectedPrefabToClear = kv.first;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else {
            ImGui::TextDisabled("No prefabs available to clear");
        }

        // === Actions ===
        if (ImGui::Button("Spawn")) {
            for (int i = 0; i < gS.count; ++i)
                SpawnOnePrefab(gSelectedPrefab.c_str(), gS, i);
        }

        // NEW: Apply current SpawnSettings to all existing instances of this prefab
        ImGui::SameLine();
        if (ImGui::Button("Apply to Existing")) {
            auto all = CollectNonMasterObjects();
            for (auto* obj : all) {
                if (!obj) continue;

                // We treat "instances of this prefab" as objects whose name matches the prefab key
                if (obj->GetObjectName() != gSelectedPrefab)
                    continue;

                // index is 0 because we don't want stepX/stepY to move existing instances.
                // applyTransformAndLayer = false to keep their position/rotation/layer.
                ApplySpawnSettingsToObject(*obj, gS, /*index*/ 0, /*applyTransformAndLayer*/ false);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selected Prefab") && !gSelectedPrefabToClear.empty()) {
            auto toKill = CollectNonMasterObjects();
            toKill.erase(std::remove_if(toKill.begin(), toKill.end(),
                [](GOC* obj) { return obj->GetObjectName() != gSelectedPrefabToClear; }),
                toKill.end());

            for (auto* o : toKill) DestroyWithUndo(o);
            FACTORY->Update(0.0f);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear All (keep masters)")) {
            auto toKill = CollectNonMasterObjects();
            for (auto* o : toKill) DestroyWithUndo(o);
            FACTORY->Update(0.0f);
            mygame::ClearSelection();
        }

        // === Object Count ===
        ImGui::SeparatorText("Counts");
        size_t totalObjs = Framework::FACTORY ? Framework::FACTORY->Objects().size() : 0;
        ImGui::Text("Total objects:   %zu", totalObjs);

        ImGui::End();
    }
} // namespace mygame
