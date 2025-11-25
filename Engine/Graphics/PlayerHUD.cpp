#include "PlayerHUD.h"

#include "Component/PlayerHealthComponent.h"
#include "Core/PathUtils.h"
#include "Resource_Manager/Resource_Manager.h"

#include <algorithm>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace Framework
{
    namespace
    {
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

    void PlayerHUDComponent::initialize()
    {
        health = GetOwner()
            ? GetOwner()->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent)
            : nullptr;

        LoadTextures();
        ResetBottles();
        SyncFromHealth();
    }

    void PlayerHUDComponent::SendMessage(Message& m)
    {
        (void)m;
    }

    void PlayerHUDComponent::Serialize(ISerializer& s)
    {
        (void)s;
    }

    std::unique_ptr<GameComponent> PlayerHUDComponent::Clone() const
    {
        auto copy = std::make_unique<PlayerHUDComponent>();
        copy->displayedHealth = displayedHealth;
        copy->bottles = bottles;
        return copy;
    }
    void PlayerHUDComponent::LoadTextures()
    {
        texSplash = LoadTexture("hud_splash", "Textures/UI/Health Bar/Health_splash.png");
        texFaceHappy = LoadTexture("hud_face_happy", "Textures/UI/Health Bar/Health_HappyFace.png");
        texFaceUpset = LoadTexture("hud_face_upset", "Textures/UI/Health Bar/Health_UpsetFace.png");
        texBottleFull = LoadTexture("hud_bottle", "Textures/UI/Health Bar/Health_Life.png");
        texBottleBreak = LoadTexture("hud_bottle_break", "Textures/UI/Health Bar/Broken_Life_VFX_Sprite.png");
    }

    void PlayerHUDComponent::ResetBottles()
    {
        for (auto& b : bottles)
        {
            b.isBroken = false;
            b.isVisible = true;
            b.breakAnimTimer = 0.0f;
        }
    }

    void PlayerHUDComponent::SyncFromHealth()
    {
        if (!health)
            return;

        const int maxHealth = std::max(1, health->playerMaxhealth);
        const int currentHealth = std::clamp(health->playerHealth, 0, maxHealth);

        // percentage ? bottle count
        int bottleCount = (currentHealth * 5) / maxHealth;  // 0–5 bottles

        displayedHealth = currentHealth;
        ResetBottles();

        for (int i = 0; i < 5; ++i)
        {
            bottles[i].isBroken = i >= bottleCount;
            bottles[i].isVisible = !bottles[i].isBroken;
        }
    }

    void PlayerHUDComponent::Update(float dt)
    {

        if (!health)
            return;

        const int maxHealth = std::max(1, health->playerMaxhealth);
        const int currentHealth = std::clamp(health->playerHealth, 0, maxHealth);

        int oldBottleCount = (displayedHealth * 5) / maxHealth;
        int newBottleCount = (currentHealth * 5) / maxHealth;

        // Losing bottles
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

        // Regaining bottles
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

        // update break animation timers
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

    void PlayerHUDComponent::Draw(int screenW, int screenH)
    {
        using namespace gfx;

        float startX = 20.0f;
        float startY = static_cast<float>(screenH) - 130.0f;

        float splashW = 350.0f;
        float splashH = 150.0f;

        if (texSplash)
            Graphics::renderSpriteUI(texSplash, startX, startY, splashW, splashH, 1, 1, 1, 1, screenW, screenH);

        unsigned faceTex = (displayedHealth >= 60) ? texFaceHappy : texFaceUpset;

        float faceSize = 110.0f;
        float faceX = startX + 10.0f;
        float faceY = startY + 20.0f;

        if (faceTex)
            Graphics::renderSpriteUI(faceTex, faceX, faceY, faceSize, faceSize, 1, 1, 1, 1, screenW, screenH);

        float bottleStartX = faceX + faceSize + 40.0f;
        float bottleY = faceY + 20.0f;
        float bottleW = 40.0f;
        float bottleH = 80.0f;
        float bottleSpacing = 35.0f;

        glm::mat4 uiOrtho = glm::ortho(0.0f, static_cast<float>(screenW), 0.0f, static_cast<float>(screenH), -1.0f, 1.0f);
        Graphics::setViewProjection(glm::mat4(1.0f), uiOrtho);

        for (int i = 0; i < static_cast<int>(bottles.size()); i++)
        {
            float xPos = bottleStartX + (i * (bottleW + bottleSpacing));
            const auto& b = bottles[static_cast<std::size_t>(i)];

            if (b.breakAnimTimer > 0.0f)
            {
                float timePercent = 1.0f - (b.breakAnimTimer / BREAK_ANIM_DURATION);
                int frame = static_cast<int>(timePercent * BREAK_FRAMES);
                frame = std::clamp(frame, 0, BREAK_FRAMES - 1);

                if (texBottleBreak)
                    Graphics::renderSpriteFrame(texBottleBreak, xPos + bottleW / 2, bottleY + bottleH / 2, 0.0f, bottleW, bottleH, frame, 3, 1);
            }
            else if (!b.isBroken)
            {
                if (texBottleFull)
                    Graphics::renderSpriteFrame(texBottleFull, xPos + bottleW / 2, bottleY + bottleH / 2, 0.0f, bottleW, bottleH, 0, 1, 1);
            }
        }

        Graphics::resetViewProjection();
    }
}