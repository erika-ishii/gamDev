/*********************************************************************************************
 \file      Spawn.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

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
//Player Component
#include "Component/PlayerComponent.h"
//Enemy Component
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Ai/DecisionTreeDefault.h"

#include <vector>
#include <string>

#include <algorithm>
#include <unordered_map>
namespace mygame {
    /// Currently selected sprite texture key (shared across panel sessions).
    static std::string sSpriteTexKey;
    static bool gLevelFilesInitialized = false;
    static std::vector<std::string> gLevelFiles;
    static int gSelectedLevelIndex = 0;
    static char gLevelNameBuffer[128] = "level";
    static std::string gLevelStatusMessage;
    static bool gLevelStatusIsError = false;
    static const std::filesystem::path kLevelDirectory("../../Data_Files");
    using namespace Framework;
    namespace {
        std::string TrimCopy(std::string value) {
            auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                [&](unsigned char c) { return !isSpace(c); }));
            value.erase(std::find_if(value.rbegin(), value.rend(),
                [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
            return value;
        }

        bool ContainsLevelKeyword(const std::string& name) {
            std::string lower;
            lower.resize(name.size());
            std::transform(name.begin(), name.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower.find("level") != std::string::npos;
        }

        void RefreshLevelFileList() {
            gLevelFiles.clear();
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
            }
            std::sort(gLevelFiles.begin(), gLevelFiles.end());
            if (gSelectedLevelIndex >= static_cast<int>(gLevelFiles.size()))
                gSelectedLevelIndex = gLevelFiles.empty() ? 0 : static_cast<int>(gLevelFiles.size() - 1);
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
    }


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
            rc->w = s.w; rc->h = s.h;
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

        // TODO: add new component setters here when needed.
        //Player Components
        if (auto* player = obj->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent)){(void)player;}
       
        //Enemy Components
        if (auto* enemy = obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
        {(void)enemy;}
        
        if (auto* health = obj->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent))
        {health->health= health->maxhealth;}
        
        if (auto* attack = obj->GetComponentType<EnemyAttackComponent>(ComponentTypeId::CT_EnemyAttackComponent)) 
        {attack->damage = s.attackDamage; attack->attack_speed = s.attack_speed;}
        
        if (auto* type = obj->GetComponentType<EnemyTypeComponent>(ComponentTypeId::CT_EnemyTypeComponent))
        {type->Etype = Framework::EnemyTypeComponent::EnemyType::physical;}
        
        if (auto* ai = obj->GetComponentType<EnemyDecisionTreeComponent>(ComponentTypeId::CT_EnemyDecisionTreeComponent))
        {if (!ai->tree) ai->tree = CreateDefaultEnemyTree(obj);}
  

    }

    /// Panel state (persists across frames).
    static std::string gSelectedPrefab = "Rect"; ///< Default prefab choice.
    static SpawnSettings gS;                     ///< Live settings bound to ImGui controls.
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
        ImGui::SetNextWindowSize(ImVec2(x/4, y/4), ImGuiCond_Once);
   
        ImGui::Begin("Spawn", &opened);   // Opens the "Spawn" debug window
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
            if (ImGui::BeginCombo("Prefab", preview)) {   // Dropdown to pick which prefab to spawn
                for (auto const& kv : master_copies) {
                    bool sel = (kv.first == gSelectedPrefab);
                    if (ImGui::Selectable(kv.first.c_str(), sel)) // Select prefab
                        gSelectedPrefab = kv.first;
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
            ImGui::TextDisabled("Missing master for '%s'", gSelectedPrefab.c_str()); // Warn if prefab missing
            ImGui::End();
            return;
        }

        const bool hasTransform =
            (master->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent) != nullptr);
        const bool hasRender =
            (master->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent) != nullptr);
        const bool hasCircle =
            (master->GetComponentType<CircleRenderComponent>(ComponentTypeId::CT_CircleRenderComponent) != nullptr);
        //const bool hasSprite =
        //    (master->GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent) != nullptr);

        // === Sprite Controls ===
        //if (hasSprite) {
        //    ImGui::SeparatorText("Sprite");   // Section header
        //    const char* preview = sSpriteTexKey.empty() ? "<none>" : sSpriteTexKey.c_str();
        //    if (ImGui::BeginCombo("Texture", preview)) {  // Dropdown to select texture
        //        for (auto const& kv : Resource_Manager::resources_map) {
        //            if (kv.second.type != Resource_Manager::Resource_Type::Graphics) continue;
        //            bool sel = (kv.first == sSpriteTexKey);
        //            if (ImGui::Selectable(kv.first.c_str(), sel)) // Select a texture
        //                sSpriteTexKey = kv.first;
        //            if (sel) ImGui::SetItemDefaultFocus();        // Keep focus on selected texture
        //        }
        //        ImGui::EndCombo();
        //    }
       // }

        // === Transform Controls ===
        if (hasTransform) {
            ImGui::SeparatorText("Transform");                // Section header
            ImGui::DragFloat("x", &gS.x, 0.005f, 0.0f, 1.0f); // Adjust X position
            ImGui::DragFloat("y", &gS.y, 0.005f, 0.0f, 1.0f); // Adjust Y position
            ImGui::DragFloat("rot (rad)", &gS.rot, 0.01f, -3.14159f, 3.14159f); // Adjust rotation in radians
        }

        // === Rectangle Controls ===
        if (hasRender) {
            ImGui::SeparatorText("Rect");                     // Section header
            ImGui::DragFloat("w", &gS.w, 0.005f, 0.01f, 1.0f); // Adjust rectangle width
            ImGui::DragFloat("h", &gS.h, 0.005f, 0.01f, 1.0f); // Adjust rectangle height
        }

        // === Circle Controls ===
        if (hasCircle) {
            ImGui::SeparatorText("Circle");                   // Section header
            ImGui::DragFloat("radius", &gS.radius, 0.005f, 0.01f, 1.0f); // Adjust circle radius
        }

        // === Color Controls ===
        if (hasRender || hasCircle) {
            ImGui::SeparatorText("Color");                    // Section header
            ImGui::ColorEdit4("rgba", gS.rgba);               // Color picker for RGBA
        }

        // === Batch Settings ===
        ImGui::SeparatorText("Batch");                        // Section header
        ImGui::DragInt("count", &gS.count, 1, 1, 500);        // Number of prefabs to spawn
        ImGui::DragFloat("stepX", &gS.stepX, 0.005f);         // Step offset in X between prefabs
        ImGui::DragFloat("stepY", &gS.stepY, 0.005f);         // Step offset in Y between prefabs


        // === Level Save / Load ===
        ImGui::SeparatorText("Levels");
        if (ImGui::InputText("Level Name", gLevelNameBuffer, IM_ARRAYSIZE(gLevelNameBuffer))) {
            // strip trailing whitespace in buffer interactions will be handled by TrimCopy when saving
        }

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
        if (ImGui::Button("Spawn")) {                         // Button to spawn prefabs
            for (int i = 0; i < gS.count; ++i)
                SpawnOnePrefab(gSelectedPrefab.c_str(), gS, i);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selected Prefab") && !gSelectedPrefabToClear.empty()) {         // Button to clear spawned objects of the selected prefab
            auto toKill = CollectNonMasterObjects();
            toKill.erase(std::remove_if(toKill.begin(), toKill.end(),
                [](GOC* obj) { return obj->GetObjectName() != gSelectedPrefabToClear; }), toKill.end());

            for (auto* o : toKill) o->Destroy();              // Destroy selected prefab instances
            FACTORY->Update(0.0f);                            // Apply destruction immediately
        }


        ImGui::SameLine();

        if (ImGui::Button("Clear All (keep masters)")) {      // Button to clear spawned objects (but keep master prefabs)
            auto toKill = CollectNonMasterObjects();
            for (auto* o : toKill) o->Destroy();              // Destroy non-master prefabs
            FACTORY->Update(0.0f);                            // Apply destruction
        }



        // === Object Count ===
        ImGui::SeparatorText("Counts");                       // Section header
        size_t totalObjs = Framework::FACTORY ? Framework::FACTORY->Objects().size() : 0;
        ImGui::Text("Total objects:   %zu", totalObjs);       // Display total number of objects

        // Framework::DrawInCurrentWindow();

        ImGui::End();                                         // Close ImGui window
    }
} // namespace mygame
