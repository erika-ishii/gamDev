/*********************************************************************************************
 \file      PlayerHUD.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Implements the PlayerHUDComponent responsible for rendering the player UI including
            health splash, facial expression icons, and animated health bottles.
 \details   Responsibilities:
            - Loads UI textures (health splash, happy/upset face, full/empty bottle sprites).
            - Tracks player health via PlayerHealthComponent and maps it to 0–5 bottle count.
            - Handles break animations when health decreases and restores bottles when health
              increases.
            - Draws HUD in screen-space using orthographic projection.
            - Supports prefab cloning and integrates with the ECS component architecture.
            - Used by HealthSystem::draw() to render HUD per player each frame.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "PlayerHUD.h"

#include "Component/PlayerHealthComponent.h"
#include "Core/PathUtils.h"
#include "Resource_Manager/Resource_Manager.h"

#include <algorithm>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    namespace
    {
        /*****************************************************************************************
         \brief Helper to load a texture through Resource_Manager using a resolved asset path.
         \param name  Key used to store/retrieve the texture.
         \param path  Relative path inside the assets directory.
         \return The loaded OpenGL texture ID, or 0 if loading failed.
        *****************************************************************************************/
        unsigned LoadTexture(const char* name, const char* path)
        {
            std::string resolved = ResolveAssetPath(path).string();
            if (!Resource_Manager::load(name, resolved))
            {
                std::cout << "[HUD] Failed to load " << name << "\n";
                return 0u;
            }
            return Resource_Manager::getTexture(name);
        }
    }

    /*****************************************************************************************
     \brief Called when the HUD component is first attached to its owner.
            Initializes references, loads textures, and synchronizes bottle state to health.
    *****************************************************************************************/
    void PlayerHUDComponent::initialize()
    {
        health = GetOwner()
            ? GetOwner()->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent)
            : nullptr;

        LoadTextures();
        ResetBottles();
        SyncFromHealth();
    }

    /*****************************************************************************************
     \brief Receives messages from other components or systems. Not used for HUD.
    *****************************************************************************************/
    void PlayerHUDComponent::SendMessage(Message& m)
    {
        (void)m;
    }

    /*****************************************************************************************
     \brief Deserialize HUD data from JSON. Currently unused since HUD is static.
    *****************************************************************************************/
    void PlayerHUDComponent::Serialize(ISerializer& s)
    {
        (void)s;
    }

    /*****************************************************************************************
     \brief Clone this HUD component when duplicating a prefab or GameObjectComposition.
     \return A new PlayerHUDComponent instance with copied bottle + health display state.
    *****************************************************************************************/
    std::unique_ptr<GameComponent> PlayerHUDComponent::Clone() const
    {
        auto copy = std::make_unique<PlayerHUDComponent>();
        copy->displayedHealth = displayedHealth;
        copy->bottles = bottles;
        return copy;
    }

    /*****************************************************************************************
     \brief Loads all HUD textures: splash, faces, full bottle, broken bottle animation sheet.
    *****************************************************************************************/
    void PlayerHUDComponent::LoadTextures()
    {
        texSplash = LoadTexture("hud_splash", "Textures/UI/Health Bar/Health_splash.png");
        texFaceHappy = LoadTexture("hud_face_happy", "Textures/UI/Health Bar/Health_HappyFace.png");
        texFaceUpset = LoadTexture("hud_face_upset", "Textures/UI/Health Bar/Health_UpsetFace.png");
        texBottleFull = LoadTexture("hud_bottle", "Textures/UI/Health Bar/Health_Life.png");
        texBottleBreak = LoadTexture("hud_bottle_break", "Textures/UI/Health Bar/Broken_Life_VFX_Sprite.png");
    }

    /*****************************************************************************************
     \brief Resets the internal bottle states (used when HUD initializes or full sync occurs).
    *****************************************************************************************/
    void PlayerHUDComponent::ResetBottles()
    {
        for (auto& b : bottles)
        {
            b.isBroken = false;
            b.isVisible = true;
            b.breakAnimTimer = 0.0f;
        }
    }

    /*****************************************************************************************
     \brief Synchronizes bottle visibility based on current player health.
            Converts numeric health → number of filled bottles (0–5).
    *****************************************************************************************/
    void PlayerHUDComponent::SyncFromHealth()
    {
        if (!health)
            return;

        const int maxHealth = std::max(1, health->playerMaxhealth);
        const int currentHealth = std::clamp(health->playerHealth, 0, maxHealth);

        int bottleCount = (currentHealth * 5) / maxHealth;  // 5 bottles total

        displayedHealth = currentHealth;
        ResetBottles();

        for (int i = 0; i < 5; ++i)
        {
            bottles[i].isBroken = i >= bottleCount;
            bottles[i].isVisible = !bottles[i].isBroken;
        }
    }

    /*****************************************************************************************
     \brief Updates the HUD each frame: bottle break animations, regains, and visibility.
     \param dt Delta time in seconds.
    *****************************************************************************************/
    void PlayerHUDComponent::Update(float dt)
    {
        if (!health)
            return;

        const int maxHealth = std::max(1, health->playerMaxhealth);
        const int currentHealth = std::clamp(health->playerHealth, 0, maxHealth);

        int oldBottleCount = (displayedHealth * 5) / maxHealth;
        int newBottleCount = (currentHealth * 5) / maxHealth;

        // Player lost health → break bottles with animation
        if (newBottleCount < oldBottleCount)
        {
            for (int i = oldBottleCount - 1; i >= newBottleCount; --i)
            {
                auto& b = bottles[i];
                if (!b.isBroken)
                {
                    b.isBroken = true;
                    b.breakAnimTimer = BREAK_ANIM_DURATION;
                }
            }
        }

        // Player regained health → restore bottles immediately
        if (newBottleCount > oldBottleCount)
        {
            for (int i = 0; i < newBottleCount; ++i)
            {
                auto& b = bottles[i];
                b.isBroken = false;
                b.isVisible = true;
                b.breakAnimTimer = 0.0f;
            }
        }

        displayedHealth = currentHealth;

        // Update bottle break timers → hide bottle after animation finishes
        for (auto& b : bottles)
        {
            if (b.breakAnimTimer > 0.0f)
            {
                b.breakAnimTimer -= dt;
                if (b.breakAnimTimer <= 0.0f)
                {
                    b.breakAnimTimer = 0.0f;
                    b.isVisible = false;
                }
            }
        }
    }

    /*****************************************************************************************
     \brief Draws the HUD in screen-space: health splash, facial icon, and bottle indicators.
     \param screenW Window width in pixels.
     \param screenH Window height in pixels.
    *****************************************************************************************/
    void PlayerHUDComponent::Draw(int screenW, int screenH)
    {
        using namespace gfx;

        // Splash background
        float startX = 20.0f;
        float startY = static_cast<float>(screenH) - 130.0f;
        float splashW = 350.0f;
        float splashH = 150.0f;

        if (texSplash)
            Graphics::renderSpriteUI(texSplash, startX, startY, splashW, splashH, 1, 1, 1, 1, screenW, screenH);
        // Compute health percentage
        float healthPercent = 0.0f;
        if (health && health->playerMaxhealth > 0)
            healthPercent = (static_cast<float>(displayedHealth) / health->playerMaxhealth) * 100.0f;

        // Choose face based on percentage
        unsigned faceTex = (healthPercent >= 50.0f) ? texFaceHappy : texFaceUpset;

        float faceSize = 110.0f;
        float faceX = startX + 10.0f;
        float faceY = startY + 20.0f;

        if (faceTex)
            Graphics::renderSpriteUI(faceTex, faceX, faceY, faceSize, faceSize, 1, 1, 1, 1, screenW, screenH);

        // Bottle positions
        float bottleStartX = faceX + faceSize + 40.0f;
        float bottleY = faceY + 20.0f;
        float bottleW = 40.0f;
        float bottleH = 80.0f;
        float bottleSpacing = 35.0f;

        // Setup screen-space ortho projection
        glm::mat4 uiOrtho = glm::ortho(0.0f, static_cast<float>(screenW),
            0.0f, static_cast<float>(screenH),
            -1.0f, 1.0f);

        Graphics::setViewProjection(glm::mat4(1.0f), uiOrtho);

        // Draw bottles
        for (int i = 0; i < static_cast<int>(bottles.size()); i++)
        {
            float xPos = bottleStartX + (i * (bottleW + bottleSpacing));
            const auto& b = bottles[static_cast<std::size_t>(i)];

            // Break animation frames
            if (b.breakAnimTimer > 0.0f)
            {
                float timePercent = 1.0f - (b.breakAnimTimer / BREAK_ANIM_DURATION);
                int frame = static_cast<int>(timePercent * BREAK_FRAMES);
                frame = std::clamp(frame, 0, BREAK_FRAMES - 1);

                if (texBottleBreak)
                    Graphics::renderSpriteFrame(
                        texBottleBreak,
                        xPos + bottleW / 2, bottleY + bottleH / 2,
                        0.0f,
                        bottleW, bottleH,
                        frame, 3, 1);
            }
            // Full bottle
            else if (!b.isBroken)
            {
                if (texBottleFull)
                    Graphics::renderSpriteFrame(
                        texBottleFull,
                        xPos + bottleW / 2, bottleY + bottleH / 2,
                        0.0f,
                        bottleW, bottleH,
                        0, 1, 1);
            }
        }

        Graphics::resetViewProjection();
    }

} // namespace Framework
