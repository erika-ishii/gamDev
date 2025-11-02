/*********************************************************************************************
 \file      Spawn.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 80%
            erika.ishii (erika.ishii@digipen.edu) - Author, 80%

 \brief     Implements a debug ImGui panel for spawning prefabs at runtime. Provides
            interactive controls for position, size, color, texture, and batch spawning.
            Integrates with the engine’s prefab and factory systems.

 \details   This module allows developers to spawn prefabs interactively during runtime
            for testing and debugging. Prefabs are drawn from PrefabManager’s registry
            (master_copies). The user selects a prefab type, configures its parameters
            (transform, render, circle, sprite), and spawns instances via ImGui. Supports
            batch spawning with configurable offsets.

 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Debug/Spawn.h"

#include "imgui.h"

// #include "Debug/Perf.h"
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#  define NOMINMAX
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
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Ai/DecisionTreeDefault.h"

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

namespace mygame {
    /// Currently selected sprite texture key (shared across panel sessions).
    static std::string sSpriteTexKey;
    static unsigned sSpriteTextureId = 0;
    static std::filesystem::path sAssetsRoot;
    static bool gLevelFilesInitialized = false;
    static std::vector<std::string> gLevelFiles;
    static int gSelectedLevelIndex = 0;
    static char gLevelNameBuffer[128] = "level";
    static std::string gLevelStatusMessage;
    static bool gLevelStatusIsError = false;
    static const std::filesystem::path kLevelDirectory("../../Data_Files");
    static std::string gActiveLayer = "Default";
    static bool gIsolateActiveLayer = false;
    static char gLayerInputBuffer[64] = "Default";

    // New: cache of layers detected in level files + last synced level
    static std::unordered_map<std::string, std::vector<std::string>> gLevelLayers;
    static std::string gLastLayerSynchronizedLevel;

    using namespace Framework;

    namespace {
        std::string ToLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool IsTextureFile(const std::filesystem::path& path) {
            const auto lower = ToLower(path.extension().string());
            return lower == ".png" || lower == ".jpg" || lower == ".jpeg";
        }

        std::string TrimCopy(std::string value) {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                [&](unsigned char c) { return !isSpace(c); }));
            value.erase(std::find_if(value.rbegin(), value.rend(),
                [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
            return value;
        }

        std::string NormalizeLayerUi(std::string_view name) {
            std::string trimmed = TrimCopy(std::string{ name });
            if (trimmed.empty())
                return "Default";
            return trimmed;
        }

        bool ContainsLevelKeyword(const std::string& name) {
            std::string lower;
            lower.resize(name.size());
            std::transform(name.begin(), name.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower.find("level") != std::string::npos;
        }

        // Read unique layer names directly from a level JSON on disk (without spawning)
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
                // cache layers for quick UI population
                gLevelLayers[filename] = ExtractLayersFromLevel(path);
            }

            std::sort(gLevelFiles.begin(), gLevelFiles.end());
            if (gSelectedLevelIndex >= static_cast<int>(gLevelFiles.size()))
                gSelectedLevelIndex = gLevelFiles.empty() ? 0 : static_cast<int>(gLevelFiles.size() - 1);
        }

        // Keep gActiveLayer + input buffer consistent and ensure the layer exists in manager
        void ApplyActiveLayer(std::string_view name) {
            gActiveLayer = NormalizeLayerUi(name);
            std::snprintf(gLayerInputBuffer, sizeof(gLayerInputBuffer), "%s", gActiveLayer.c_str());
            if (FACTORY) FACTORY->Layers().EnsureLayer(gActiveLayer);
        }

        void EnsureLevelLayersCached(const std::string& levelKey, const std::filesystem::path& levelPath) {
            if (levelKey.empty() || levelPath.empty()) return;
            if (gLevelLayers.find(levelKey) == gLevelLayers.end())
                gLevelLayers[levelKey] = ExtractLayersFromLevel(levelPath);
        }

        // Prefer a non-Default layer as the initial active layer
        std::string ChooseDefaultLayerForLevel(const std::string& levelKey) {
            if (!levelKey.empty()) {
                if (auto it = gLevelLayers.find(levelKey); it != gLevelLayers.end()) {
                    for (const auto& nm : it->second) {
                        if (NormalizeLayerUi(nm) != "Default")
                            return NormalizeLayerUi(nm);
                    }
                    if (!it->second.empty())
                        return NormalizeLayerUi(it->second.front());
                }
            }
            return "Default";
        }

        void SyncActiveLayerWithLevel(const std::string& levelKey) {
            if (levelKey.empty()) return;
            if (gLastLayerSynchronizedLevel == levelKey) return; // already synced
            ApplyActiveLayer(ChooseDefaultLayerForLevel(levelKey));
            gLastLayerSynchronizedLevel = levelKey;
        }

        std::filesystem::path LevelFilePath(const std::string& filename) {
            return kLevelDirectory / filename;
        }

        bool IsMasterObject(GOC* obj) {
            for (auto const& kv : master_copies) {
                if (kv.second.get() == obj)
                    return true;
            }
            return false;
        }

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
    } // namespace


    /*************************************************************************************
      \brief Helper to spawn a single prefab and apply current SpawnSettings.
      \param prefab The name of the prefab to clone.
      \param s      The spawn settings (position, size, color, etc.).
      \param index  Index used for batch offsets.
    *************************************************************************************/
    static void SpawnOnePrefab(const char* prefab, SpawnSettings const& s, int index) {
        GOC* obj = ClonePrefab(prefab);
        if (!obj) return;

        const float x = s.x + s.stepX * index;
        const float y = s.y + s.stepY * index;

        // Apply Transform settings
        if (auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent)) {
            tr->x = x; tr->y = y; tr->rot = s.rot;
        }
        // Apply Rectangle render settings
        if (auto* rc = obj->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent)) {
            if (s.overridePrefabSize) {
                rc->w = s.w; rc->h = s.h;
            }
            rc->r = s.rgba[0]; rc->g = s.rgba[1]; rc->b = s.rgba[2]; rc->a = s.rgba[3];
        }
        // Apply Circle render settings
        if (auto* cc = obj->GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent)) {
            cc->radius = s.radius;
            cc->r = s.rgba[0]; cc->g = s.rgba[1]; cc->b = s.rgba[2]; cc->a = s.rgba[3];
        }

        // Apply Sprite settings
        if (auto* sp = obj->GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent)) {
            if (!sSpriteTexKey.empty()) {
                sp->texture_key = sSpriteTexKey;
                sp->texture_id = Resource_Manager::getTexture(sSpriteTexKey);
            }
        }

        // Player/Enemy bits (if present)
        if (auto* player = obj->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent)) { (void)player; }

        if (auto* enemy = obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent)) { (void)enemy; }

        if (auto* health = obj->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent)) {
            health->health = health->maxhealth;
        }

        if (auto* attack = obj->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent)) {
            attack->damage = s.attackDamage;
            attack->attack_speed = s.attack_speed;
        }

        if (auto* type = obj->GetComponentType<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent)) {
            type->Etype = Framework::EnemyTypeComponent::EnemyType::physical;
        }

        if (auto* ai = obj->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent)) {
            if (!ai->tree) ai->tree = CreateDefaultEnemyTree(obj);
        }

        // Important: set layer on the new object
        obj->SetLayerName(gActiveLayer);
    }

    void SetSpawnPanelAssetsRoot(const std::filesystem::path& root) {
        if (root.empty()) {
            sAssetsRoot.clear();
            return;
        }
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(root, ec);
        sAssetsRoot = ec ? root : canonical;
    }

    void UseSpriteFromAsset(const std::filesystem::path& relativePath) {
        if (relativePath.empty() || sAssetsRoot.empty())
            return;

        std::filesystem::path relative = relativePath;
        if (relative.is_absolute()) {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(relative, ec);
            if (ec)
                return;
            relative = canonical.lexically_relative(sAssetsRoot);
        }

        if (relative.empty())
            return;

        std::error_code ec;
        auto absolute = std::filesystem::weakly_canonical(sAssetsRoot / relative, ec);
        if (ec)
            absolute = sAssetsRoot / relative;

        if (!std::filesystem::exists(absolute))
            return;

        if (!std::filesystem::is_regular_file(absolute))
            return;

        if (!IsTextureFile(absolute))
            return;

        std::string key = relative.generic_string();
        if (key.empty())
            return;

        if (Resource_Manager::resources_map.find(key) == Resource_Manager::resources_map.end())
            Resource_Manager::load(key, absolute.string());

        unsigned handle = Resource_Manager::getTexture(key);
        if (handle != 0) {
            sSpriteTexKey = key;
            sSpriteTextureId = handle;
        }
    }

    void ClearSpriteTexture() {
        sSpriteTexKey.clear();
        sSpriteTextureId = 0;
    }

    const std::string& CurrentSpriteTextureKey() {
        return sSpriteTexKey;
    }

    unsigned CurrentSpriteTextureHandle() {
        return sSpriteTextureId;
    }

    const std::string& ActiveLayerName() {
        return gActiveLayer;
    }

    bool IsLayerIsolationEnabled() {
        return gIsolateActiveLayer;
    }

    bool ShouldRenderLayer(const std::string& layerName) {
        if (!gIsolateActiveLayer)
            return true;
        return NormalizeLayerUi(layerName) == NormalizeLayerUi(gActiveLayer);
    }

    /// Panel state (persists across frames).
    static std::string gSelectedPrefab = "Rect"; ///< Default prefab choice.
    static SpawnSettings gS;                     ///< Live settings bound to ImGui controls.
    static bool gPendingPrefabSizeSync = true;   ///< Sync flag to copy prefab dimensions into the panel.
    bool opened = true;
    float x = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float y = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    static std::string gClearPrefab = "Rect"; ///< Default prefab type to clear.
    static GOC* gSelectedObject = nullptr;
    static std::string gSelectedPrefabToClear = gSelectedPrefab;

    /*************************************************************************************
      \brief Draws the "Spawn" ImGui panel and handles prefab spawning actions.
    *************************************************************************************/
    void DrawSpawnPanel() {
        //ImGui::SetNextWindowSize(ImVec2(x / 4, y/2.5 ), ImGuiCond_Once);

        ImGui::Begin("Spawn", &opened);
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

        // Resolve master prefab...
        GOC* master = nullptr;
        if (auto it = master_copies.find(gSelectedPrefab); it != master_copies.end())
            master = it->second.get();

        if (!master) {
            ImGui::TextDisabled("Missing master for '%s'", gSelectedPrefab.c_str());
            ImGui::End();
            return;
        }

        // --- Layer UI ---

        // Build a merged set of layer names:
        // - From LayerManager (runtime)
        // - From currently loaded level (if any)
        // - From currently selected level in the combo (for preview before load)
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

        // From the currently selected level in dropdown (helps before any load)
        if (layerSet.empty() && !gLevelFiles.empty()) {
            const std::string& sel = gLevelFiles[gSelectedLevelIndex];
            if (auto it = gLevelLayers.find(sel); it != gLevelLayers.end()) {
                for (auto const& nm : it->second)
                    layerSet.insert(NormalizeLayerUi(nm));
            }
        }

        // Always include the current active + Default
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

        // Editable name field; applied by button press
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

        const bool hasTransform =
            (master->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent) != nullptr);
        auto* masterRender = master->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent);
        const bool hasRender = (masterRender != nullptr);
        const bool hasCircle =
            (master->GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent) != nullptr);
        //const bool hasSprite =
        //    (master->GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent) != nullptr);
        if (gPendingPrefabSizeSync && masterRender) {
            gS.w = masterRender->w;
            gS.h = masterRender->h;
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
            ImGui::DragFloat("x", &gS.x, 0.005f, 0.0f, 1.0f);
            ImGui::DragFloat("y", &gS.y, 0.005f, 0.0f, 1.0f);
            ImGui::DragFloat("rot (rad)", &gS.rot, 0.01f, -3.14159f, 3.14159f);
        }

        // === Rectangle Controls ===
        if (hasRender) {
            ImGui::SeparatorText("Rect"); // Section header

            // When the checkbox is toggled OFF, copy prefab size back
            if (ImGui::Checkbox("Override prefab size", &gS.overridePrefabSize)) {
                if (!gS.overridePrefabSize && masterRender) {
                    gS.w = masterRender->w;
                    gS.h = masterRender->h;
                }
            }

            const bool disableSizeControls = !gS.overridePrefabSize;
            if (disableSizeControls) ImGui::BeginDisabled();

            ImGui::DragFloat("w", &gS.w, 0.005f, 0.01f, 1.0f); // Adjust rectangle width
            ImGui::DragFloat("h", &gS.h, 0.005f, 0.01f, 1.0f); // Adjust rectangle height

            if (disableSizeControls) ImGui::EndDisabled();
        }

        // === Circle Controls ===
        if (hasCircle) {
            ImGui::SeparatorText("Circle");
            ImGui::DragFloat("radius", &gS.radius, 0.005f, 0.01f, 1.0f);
        }

        // === Color Controls ===
        if (hasRender || hasCircle) {
            ImGui::SeparatorText("Color");
            ImGui::ColorEdit4("rgba", gS.rgba);
        }

        // === Batch Settings ===
        ImGui::SeparatorText("Batch");
        ImGui::DragInt("count", &gS.count, 1, 1, 500);
        ImGui::DragFloat("stepX", &gS.stepX, 0.005f);
        ImGui::DragFloat("stepY", &gS.stepY, 0.005f);

        // === Level Save / Load ===
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

        // If we have a last loaded level, ensure its layer cache is filled and sync active layer
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
                        obj->Destroy();
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

        if (!gLevelStatusMessage.empty()) {
            ImVec4 color = gLevelStatusIsError ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f)
                : ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
            ImGui::TextColored(color, "%s", gLevelStatusMessage.c_str());
        }

        // Keep the clear selection in sync if it references a prefab that no longer exists.
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

        // === Action Buttons ===
        if (ImGui::Button("Spawn")) {
            for (int i = 0; i < gS.count; ++i)
                SpawnOnePrefab(gSelectedPrefab.c_str(), gS, i);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selected Prefab") && !gSelectedPrefabToClear.empty()) {
            auto toKill = CollectNonMasterObjects();
            toKill.erase(std::remove_if(toKill.begin(), toKill.end(),
                [](GOC* obj) { return obj->GetObjectName() != gSelectedPrefabToClear; }), toKill.end());

            for (auto* o : toKill) o->Destroy();
            FACTORY->Update(0.0f);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear All (keep masters)")) {
            auto toKill = CollectNonMasterObjects();
            for (auto* o : toKill) o->Destroy();
            FACTORY->Update(0.0f);
        }

        // === Object Count ===
        ImGui::SeparatorText("Counts");
        size_t totalObjs = Framework::FACTORY ? Framework::FACTORY->Objects().size() : 0;
        ImGui::Text("Total objects:   %zu", totalObjs);

        ImGui::End();
    }
} // namespace mygame
