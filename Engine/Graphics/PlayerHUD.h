/*********************************************************************************************
 \file      PlayerHUD.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Declares the PlayerHUDComponent which renders the players health HUD including:
            the health splash background, facial expression icon, and the animated bottle bar.
 \details   Responsibilities:
            - Maps PlayerHealthComponent values (0100%) into a 05 bottle display.
            - Handles bottle break animations when player health decreases.
            - Restores bottles when health increases.
            - Loads and stores all HUD textures through Resource_Manager.
            - Draws the HUD in screen-space UI coordinates using graphics API helpers.
            - Supports cloning so the HUD works correctly with prefabs and the editor.
            Used by HealthSystem::draw() to render the HUD each frame for the player.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Graphics/Graphics.hpp"
#include <array>

namespace Framework
{
    class PlayerHealthComponent;

    /*****************************************************************************************
     \class PlayerHUDComponent
     \brief Component that manages all UI elements related to the players health display.
     \details
        - Displays a background health splash texture.
        - Selects a happy or upset face depending on the remaining health.
        - Displays 05 bottles representing fractional health.
        - Plays break-animation frames when bottles disappear.
        - Works entirely in screen-space (UI) coordinates.
    *****************************************************************************************/
    class PlayerHUDComponent : public GameComponent
    {
    public:
        /*************************************************************************************
         \brief Initialize component: grab PlayerHealthComponent, load textures,
                and sync internal bottle state.
        **************************************************************************************/
        void initialize() override;

        /*************************************************************************************
         \brief Message handler (unused for HUD components).
        **************************************************************************************/
        void SendMessage(Message& m) override;

        /*************************************************************************************
         \brief Deserialize values from JSON (currently unused; HUD is static).
        **************************************************************************************/
        void Serialize(ISerializer& s) override;

        /*************************************************************************************
         \brief Clone this HUD component when duplicating a prefab or GameObject.
        **************************************************************************************/
        ComponentHandle Clone() const override;

        /*************************************************************************************
         \brief Update HUD state (bottle break animations, health transitions).
         \param dt Delta time in seconds.
        **************************************************************************************/
        void Update(float dt);

        /*************************************************************************************
         \brief Render the HUD in UI screen-space.
         \param screenW Window width in pixels.
         \param screenH Window height in pixels.
        **************************************************************************************/
        void Draw(int screenW, int screenH);

    private:
        // -------------------------
        // Textures used by the HUD
        // -------------------------
        unsigned texSplash = 0;       ///< Background splash behind the HUD.
        unsigned texFaceHappy = 0;    ///< Happy face shown at high health.
        unsigned texFaceUpset = 0;    ///< Upset face shown at low health.
        unsigned texBottleFull = 0;   ///< Full bottle representing health.
        unsigned texBottleBreak = 0;  ///< Sprite sheet for bottle break animation
        unsigned texBottleBroken{ 0 }; //< break bottle when health is lost

        int displayedHealth = 100;    ///< The health value last interpreted into bottle counts.
        PlayerHealthComponent* health = nullptr; ///< Cached pointer to player's health component.

        /*************************************************************************************
         \struct BottleState
         \brief Stores the state of a single health bottle.
        **************************************************************************************/
        struct BottleState
        {
            bool isBroken = false;        ///< True if bottle is broken / missing.
            float breakAnimTimer = 0.0f;  ///< Time remaining for break animation.
            bool isVisible = true;        ///< Visibility flag after animation finishes.
        };

        std::array<BottleState, 5> bottles{}; ///< The 5-bottle health bar.

        // -------------------------
        // Animation timing
        // -------------------------
        static constexpr float BREAK_ANIM_DURATION = 0.4f; ///< Bottle break anim length.
        static constexpr int BREAK_FRAMES = 3;              ///< Number of frames in break sheet.



        // -------------------------
        // Internal helpers
        // -------------------------
        /*************************************************************************************
         \brief Load all textures needed for the HUD using Resource_Manager.
        **************************************************************************************/
        void LoadTextures();

        /*************************************************************************************
         \brief Reset bottle states (used on init and health sync).
        **************************************************************************************/
        void ResetBottles();

        /*************************************************************************************
         \brief Synchronize bottle visibility from the player's current health.
        **************************************************************************************/
        void SyncFromHealth();
    };

} // namespace Framework
