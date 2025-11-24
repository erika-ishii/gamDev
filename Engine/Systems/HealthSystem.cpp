#include "HealthSystem.h"
#include "Factory/Factory.h"
#include "Component/SpriteAnimationComponent.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string_view>

namespace Framework
{
    namespace
    {
        /*****************************************************************************************
         \brief  Find the index of a named animation on a SpriteAnimationComponent (case-insensitive).

         \param anim
                Pointer to the SpriteAnimationComponent.
         \param desired
                Name of the animation we want to find.

         \return
                Index of the animation if found, otherwise -1.
        *****************************************************************************************/
        int FindAnimationIndex(SpriteAnimationComponent* anim, std::string_view desired)
        {
            if (!anim)
                return -1;

            auto equalsIgnoreCase = [](std::string_view a, std::string_view b)
                {
                    if (a.size() != b.size())
                        return false;

                    for (std::size_t i = 0; i < a.size(); ++i)
                    {
                        if (std::tolower(static_cast<unsigned char>(a[i])) !=
                            std::tolower(static_cast<unsigned char>(b[i])))
                            return false;
                    }
                    return true;
                };

            for (std::size_t i = 0; i < anim->animations.size(); ++i)
            {
                if (equalsIgnoreCase(anim->animations[i].name, desired))
                    return static_cast<int>(i);
            }

            return -1;
        }

        /*****************************************************************************************
         \brief  Helper to safely switch an animation by name if it exists on the given object.

         \param goc
                Game object that owns the SpriteAnimationComponent.
         \param name
                Animation name we want to set as active.

         \details
                Does nothing if the animation component or requested animation is missing.
        *****************************************************************************************/
        void PlayAnimationIfAvailable(GOC* goc, std::string_view name)
        {
            if (!goc)
                return;

            auto* anim =
                goc->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
            if (!anim)
                return;

            const int idx = FindAnimationIndex(anim, name);
            if (idx >= 0 && idx != anim->ActiveAnimationIndex())
            {
                anim->SetActiveAnimation(idx);
            }
        }

        /*****************************************************************************************
         \brief  Compute the duration (in seconds) of a given animation based on its frame range
                 and FPS settings.

         \param anim
                Pointer to the SpriteAnimationComponent.
         \param name
                Name of the animation whose duration we want to calculate.

         \return
                Duration of the animation in seconds, or 0.0f if it cannot be determined.
        *****************************************************************************************/
        float AnimationDuration(SpriteAnimationComponent* anim, std::string_view name)
        {
            if (!anim)
                return 0.0f;

            const int idx = FindAnimationIndex(anim, name);
            if (idx < 0 || idx >= static_cast<int>(anim->animations.size()))
                return 0.0f;

            const auto& sheet = anim->animations[static_cast<std::size_t>(idx)];

            const int total = std::max(1, sheet.config.totalFrames);
            const int start = std::clamp(sheet.config.startFrame, 0, total - 1);
            const int end = (sheet.config.endFrame >= 0)
                ? std::clamp(sheet.config.endFrame, start, total - 1)
                : (total - 1);

            const int frameCount = end - start + 1;
            if (sheet.config.fps <= 0.0f)
                return 0.0f;

            // duration = number of frames in the animation / frames per second
            return static_cast<float>(frameCount) / sheet.config.fps;
        }

        /*****************************************************************************************
         \brief  Check if a named animation has finished playing (for non-looping animations).

         \param anim
                Pointer to the SpriteAnimationComponent.
         \param name
                Name of the animation we want to inspect (case-insensitive).

         \return
                True if the animation exists, is non-looping, and its current frame has reached
                or surpassed the configured end frame. Returns false if the animation is missing
                or still in progress.
        *****************************************************************************************/
        bool IsAnimationFinished(SpriteAnimationComponent* anim, std::string_view name)
        {
            if (!anim)
                return false;

            const int idx = FindAnimationIndex(anim, name);
            if (idx < 0 || idx >= static_cast<int>(anim->animations.size()))
                return false;

            const auto& sheet = anim->animations[static_cast<std::size_t>(idx)];
            if (sheet.config.loop)
                return false; // looping animations never "finish"

            const int total = std::max(1, sheet.config.totalFrames);
            const int start = std::clamp(sheet.config.startFrame, 0, total - 1);
            const int end = (sheet.config.endFrame >= 0)
                ? std::clamp(sheet.config.endFrame, start, total - 1)
                : (total - 1);

            const int current = std::clamp(sheet.currentFrame, 0, total - 1);
            return current >= end;
        }
    } // anonymous namespace

    HealthSystem::HealthSystem(gfx::Window& win)
        : window(&win)
    {
    }

    void HealthSystem::RefreshTrackedObjects()
    {
        for (auto& [id, goc] : FACTORY->Objects())
        {
            if (!goc)
                continue;

            auto* enemyHealth =
                goc->GetComponentType<EnemyHealthComponent>(
                    ComponentTypeId::CT_EnemyHealthComponent);

            auto* playerHealth =
                goc->GetComponentType<PlayerHealthComponent>(
                    ComponentTypeId::CT_PlayerHealthComponent);

            if (!(enemyHealth || playerHealth))
                continue;

            // Skip if we're already tracking this object
            if (std::find(gameObjectIds.begin(), gameObjectIds.end(), id) != gameObjectIds.end())
                continue;

            gameObjectIds.push_back(id);
        }
    }

    void HealthSystem::Initialize()
    {
        // Track by ID instead of raw pointers to avoid dangling references.
        gameObjectIds.clear();
        deathTimers.clear();

        RefreshTrackedObjects();
    }

    void HealthSystem::Update(float dt)
    {
        RefreshTrackedObjects();
        gameObjectIds.erase(
            std::remove_if(
                gameObjectIds.begin(),
                gameObjectIds.end(),
                [this, dt](GOCId id) -> bool
                {
                    // Re-resolve each frame; if gone, drop it safely.
                    GOC* goc = FACTORY->GetObjectWithId(id);
                    if (!goc)
                    {
                        // Object was destroyed elsewhere; clean up any death timer.
                        deathTimers.erase(id);
                        return true;
                    }

                    // ---------------------------------------------------------
                    // Enemy health handling (plays death animation before destroy)
                    // ---------------------------------------------------------
                    if (auto* enemyHealth =
                        goc->GetComponentType<EnemyHealthComponent>(
                            ComponentTypeId::CT_EnemyHealthComponent))
                    {
                        if (enemyHealth->enemyHealth <= 0)
                        {
                            // Default death animation name; can be extended per-enemy type if
                            // future enemies need unique death clips (e.g., "water_death").
                            constexpr std::string_view deathAnimName = "death";

                            float& timer = deathTimers[id];
                            auto* anim = goc->GetComponentType<SpriteAnimationComponent>(
                                ComponentTypeId::CT_SpriteAnimationComponent);

                            // First frame after "death" ¨C trigger death animation and compute duration.
                            if (timer <= 0.0f)
                            {
                                PlayAnimationIfAvailable(goc, deathAnimName);

                                // Use animation length if available; otherwise fall back to a minimum.
                                timer = std::max(AnimationDuration(anim, deathAnimName), 0.2f);
                            }
                            else
                            {
                                // Count down until we actually destroy the object.
                                timer = std::max(0.0f, timer - dt);
                            }

                            const bool hasAnimation = anim && FindAnimationIndex(anim, deathAnimName) >= 0;
                            const bool animationFinished = hasAnimation
                                ? IsAnimationFinished(anim, deathAnimName)
                                : true; // if no animation, rely on timer only

                            // Destroy only after both the timer has elapsed and the animation has
                            // completed its final frame (ensures death pose is visible briefly).
                            if (timer <= 0.0f && animationFinished)
                            {
                                FACTORY->Destroy(goc);
                                deathTimers.erase(id);

                                std::cout << "[HealthSystem] Enemy "
                                    << goc->GetId()
                                    << " destroyed.\n";
                                return true; // remove from tracked IDs
                            }

                            // Keep the object for now so the death animation can finish.
                            return false;
                        }

                        // Enemy is still alive; make sure we don't keep a stale timer.
                        deathTimers.erase(id);
                    }

                    // ---------------------------------------------------------
                    // Player health handling (plays death animation before destroy)
                    // ---------------------------------------------------------
                    if (auto* playerHealth =
                        goc->GetComponentType<PlayerHealthComponent>(
                            ComponentTypeId::CT_PlayerHealthComponent))
                    {
                        constexpr std::string_view deathAnimName = "death";
                        auto* anim = goc->GetComponentType<SpriteAnimationComponent>(
                            ComponentTypeId::CT_SpriteAnimationComponent);

                        if (!playerHealth->isDead)
                        {
                            if (playerHealth->isInvulnerable)
                            {
                                playerHealth->invulnTime -= dt;
                                if (playerHealth->invulnTime <= 0.0f)
                                {
                                    playerHealth->invulnTime = 0.0f;
                                    playerHealth->isInvulnerable = false;
                                    std::cout << "[PlayerHealthComponent] Invulnerability ended.\n";
                                }
                            }
                        }

                        if (playerHealth->playerHealth <= 0)
                        {
                            float& timer = deathTimers[id];

                            if (!playerHealth->isDead)
                            {
                                playerHealth->isDead = true;

                                PlayAnimationIfAvailable(goc, deathAnimName);
                                timer = std::max(AnimationDuration(anim, deathAnimName), 0.2f);
                            }
                            else
                            {
                                timer = std::max(0.0f, timer - dt);
                            }

                            const bool hasAnimation = anim && FindAnimationIndex(anim, deathAnimName) >= 0;
                            const bool animationFinished = hasAnimation
                                ? IsAnimationFinished(anim, deathAnimName)
                                : true; // if no animation, rely on timer only

                            if (timer <= 0.0f && animationFinished)
                            {
                                FACTORY->Destroy(goc);
                                deathTimers.erase(id);

                                std::cout << "[HealthSystem] Player "
                                    << goc->GetId()
                                    << " destroyed.\n";
                                return true; // remove from tracked IDs
                            }

                            // Keep player around until animation finishes
                            return false;
                        }

                        // Player is alive; clear any stale death timers / flags.
                        deathTimers.erase(id);
                        playerHealth->isDead = false;
                    }

                    // Keep tracking this ID.
                    return false;
                }),
            gameObjectIds.end());
    }

    void HealthSystem::draw()
    {
        // Currently no UI/visuals for health. Rendering of health bars or
        // damage indicators could be added here in the future.
    }

    void HealthSystem::Shutdown()
    {
        gameObjectIds.clear();
        deathTimers.clear();
    }

} // namespace Framework
