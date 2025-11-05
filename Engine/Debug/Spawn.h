/*********************************************************************************************
 \file      Spawn.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares debug UI utilities for spawning objects in ImGui. Provides the
            SpawnSettings struct for configuring spawn parameters and the DrawSpawnPanel
            function which renders an ImGui panel for interactive spawning.

 \details   It exposes ImGui controls that allow developers to specify position, size, color, and count, 
            and then spawn objects at runtime. Intended for use in debug/editor builds to quickly test
            spawning behavior


 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <filesystem>
#include <string>

// Minimal, prefab-agnostic spawn UI for ImGui.
// Usage:
//   1) #include "Debug/Spawn.h" in Game.cpp
//   2) Call mygame::DrawSpawnPanel() each frame (between BeginFrame/EndFrame)

namespace mygame {

    /*****************************************************************************************
      \struct SpawnSettings
      \brief Encapsulates configurable parameters for spawning objects via ImGui.

      Fields include:
      - Position (x, y) in normalized coordinates (0..1 if renderer uses NDC).
      - Rotation (rot) in radians.
      - Size (w, h) for rectangles.
      - Radius for circles.
      - RGBA tint for renderables.
      - Batch spawn settings (count, step offsets).

      This struct is used internally by the spawn panel to control spawn behavior.
    *****************************************************************************************/
    struct SpawnSettings {
        // always available (normalized 0..1 if your renderer uses NDC-like coords)
        float x{ 0.5f }, y{ 0.5f };   ///< Normalized position
        float rot{ 0.0f };            ///< Rotation in radians

        // rect (RenderComponent)
        float w{ 0.5f }, h{ 0.5f };   ///< Width/height for rectangle-based components

        // circle (CircleRenderComponent)
        float radius{ 0.08f };        ///< Radius for circle-based components
        
        bool overridePrefabSize{ false }; ///< When true, allow overriding prefab width/height

        // tint for anything renderable
        float rgba[4]{ 1.f, 1.f, 1.f, 1.f }; ///< RGBA color tint

        bool  overridePrefabCollider{ false }; // if false -> keep prefab collider
        float rbWidth{ 0.5f }, rbHeight{ 0.5f };
        float rbVelX{ 0.f }, rbVelY{ 0.f };

        // batch
        int   count{ 1 };             ///< Number of instances to spawn in batch
        float stepX{ 0.05f }, stepY{ 0.0f }; ///< Step offset applied per batch instance

        //Enemy use
        int attackDamage{ 10 }; // add this
        float attack_speed{ 1.0f }; // optionally also add speed

        //EnemyHealth
        int enemyHealth{};
        int enemyMaxhealth{};

        //player use
        int attackDamagep{};
        float attack_speedp{};

        //playerHealth
        int playerHealth{};
        int playerMaxhealth{};

        // override toggles (default: inherit from prefab JSON)
        bool overridePrefabTransform{ false };
        bool overridePrefabCircle{ false };
        bool overrideSpriteTexture{ false };      // 
        bool overrideEnemyAttack{ false };        // for damage/attack_speed
        bool overrideEnemyHealth{ false };        // for health fields
        bool overridePrefabVelocity{ false };     // velocities 
        bool overridePlayerAttack{ false };
        bool overridePlayerHealth{ false };
    };

    /*****************************************************************************************
      \brief Draws the "Spawn" ImGui panel and performs spawning actions when triggered.

      This function displays controls for configuring SpawnSettings and spawns objects
      when the user interacts with the UI. Should be called once per frame within
      ImGui’s frame lifecycle.

      Example usage:
      \code
        // inside game loop
        ImGuiLayer::BeginFrame();
        mygame::DrawSpawnPanel();
        ImGuiLayer::EndFrame();
      \endcode
    *****************************************************************************************/
    void DrawSpawnPanel();
    void SetSpawnPanelAssetsRoot(const std::filesystem::path& root);
    void UseSpriteFromAsset(const std::filesystem::path& relativePath);
    void ClearSpriteTexture();

    const std::string& CurrentSpriteTextureKey();
    unsigned CurrentSpriteTextureHandle();

    const std::string& ActiveLayerName();
    bool IsLayerIsolationEnabled();
    bool ShouldRenderLayer(const std::string& layerName);

} // namespace mygame
