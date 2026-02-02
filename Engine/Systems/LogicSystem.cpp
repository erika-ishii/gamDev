/*********************************************************************************************
 \file      LogicSystem.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Author, 10%
            elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 20%
            yimo kong (yimo.kong@digipen.edu)      - Author, 10%
            Ho Jun (h.jun@digipen.edu) - Author, 20%

 \brief     Core gameplay loop and input-driven logic for the sample sandbox.
 \details   This module owns high-level game state orchestration:
            - Factory lifetime: component registration, prefab loading/unloading, level create/destroy.
            - Player references: discovery, cached size for scale operations, animation state machine.
            - Input mapping: WASD movement, Q/E rotation, Z/X scale, R reset, Shift accelerator.
            - HitBoxSystem integration: spawns short-lived attack boxes towards cursor on LMB.
            - Crash logging utilities: F9 forces a safe, logged crash for robustness testing.
            - Collision "debug info": builds AABBs for player/target to visualize or check overlap.

            Performance & stability:
            * Uses TryGuard::Run to isolate Update() logic and attribute errors with a tag.
            * Minimizes per-frame object lookups by caching player/targets (validated via IsAlive()).
            * Time-based animation frame stepping decoupled from render rate via fps in AnimConfig.

            Conventions:
            * Screen coordinates are mapped to world space via RenderSystem::ScreenToWorld().
            * Layering, physics, and rendering are handled by their respective systems; LogicSystem
              manipulates components (Transform/Render/RigidBody) but does not own them.
©2025 DigiPen Institute of Technology Singapore. All rights reserved.
*********************************************************************************************/
#include "Common/CRTDebug.h"
#include "Systems/LogicSystem.h"
#include "Core/PathUtils.h"
#include "Systems/RenderSystem.h"      // for ScreenToWorld / camera-based world mapping
#include "Debug/Selection.h"
#include "Debug/Spawn.h"
#include "Systems/VfxHelpers.h"
#include "Memory/GameObjectPool.h"
#include "Resource_Asset_Manager/Resource_Manager.h"

#include <cctype>
#include <string>
#include <string_view>
#include <csignal>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <Debug/UndoStack.h>

#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif

namespace Framework {

    /*****************************************************************************************
      \brief Construct a LogicSystem bound to a window and input system.
      \param window  Reference to the active gfx::Window (dimension queries, etc.).
      \param input   Reference to the engine-wide InputSystem.
    *****************************************************************************************/
    LogicSystem::LogicSystem(gfx::Window& window, InputSystem& input)
        : window(&window), input(input) {
    }

    /// Defaulted virtual destructor; shuts down via Shutdown().
    LogicSystem::~LogicSystem() = default;

    /*****************************************************************************************
      \brief Get the currently active animation configuration (idle / run / attacks).
    *****************************************************************************************/
    const LogicSystem::AnimConfig& LogicSystem::CurrentConfig() const
    {
        return ConfigForState(animState);
    }

    const LogicSystem::AnimConfig& LogicSystem::ConfigForState(AnimState state) const
    {
        switch (state)
        {
        case AnimState::Idle:      return idleConfig;
        case AnimState::Run:       return runConfig;
        case AnimState::Attack1:   return attackConfigs[0];
        case AnimState::Attack2:   return attackConfigs[1];
        case AnimState::Attack3:   return attackConfigs[2];
        case AnimState::Throw:     return throwConfig;
        case AnimState::Knockback: return knockbackConfig;
        case AnimState::Death:     return deathConfig;
        }
        // Fallback
        return idleConfig;
    }

    bool LogicSystem::IsAttackState(AnimState state) const
    {
        return state == AnimState::Attack1 ||
            state == AnimState::Attack2 ||
            state == AnimState::Attack3 ||
            state == AnimState::Throw;
    }

    void LogicSystem::SetAnimState(AnimState newState)
    {
        if (animState == newState)
        {
            ApplyAnimationStateToComponent(newState);
            return;
        }

        animState = newState;
        frame = 0;
        frameClock = 0.f;

        ApplyAnimationStateToComponent(newState);
    }

    LogicSystem::AnimState LogicSystem::AttackStateForIndex(int comboIndex) const
    {
        // Wrap combo index into [0,2]
        const int wrapped = ((comboIndex - 1) % 3 + 3) % 3;
        switch (wrapped)
        {
        case 0:  return AnimState::Attack1;
        case 1:  return AnimState::Attack2;
        default: return AnimState::Attack3;
        }
    }

    float LogicSystem::AttackDurationForState(AnimState state) const
    {
        if (!IsAttackState(state))
            return 0.f;
        const SpriteAnimationComponent* anim = nullptr;
        if (IsAlive(player))
        {
            anim = player->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        }

        const AnimConfig cfg = ConfigFromSpriteSheet(anim, state);

        if (cfg.fps <= 0.f)
            return 0.f;

        return static_cast<float>(cfg.frames) / cfg.fps;
    }

    void LogicSystem::ForceAttackState(int comboIndex)
    {
        comboStep = ((comboIndex - 1) % 3 + 3) % 3 + 1;   // store 1..3 for bookkeeping
        AnimState nextState = AttackStateForIndex(comboStep);
        SetAnimState(nextState);
        attackTimer = AttackDurationForState(nextState);
    }

    void LogicSystem::BeginComboAttack()
    {
        comboStep = (comboStep % 3) + 1;   // 1 -> 2 -> 3 -> 1 ...
        ForceAttackState(comboStep);
    }

    void LogicSystem::BeginThrowAttack()
    {
        SetAnimState(AnimState::Throw);
        attackTimer = AttackDurationForState(AnimState::Throw);
    }

    LogicSystem::AnimationInfo::Mode LogicSystem::ModeForState(AnimState state) const
    {
        switch (state)
        {
        case AnimState::Run:       return AnimationInfo::Mode::Run;
        case AnimState::Attack1:   return AnimationInfo::Mode::Attack1;
        case AnimState::Attack2:   return AnimationInfo::Mode::Attack2;
        case AnimState::Attack3:   return AnimationInfo::Mode::Attack3;
        case AnimState::Throw:     return AnimationInfo::Mode::Throw;
        case AnimState::Knockback: return AnimationInfo::Mode::Knockback;
        case AnimState::Death:     return AnimationInfo::Mode::Death;
        case AnimState::Idle:
        default:                 return AnimationInfo::Mode::Idle;
        }
    }

    std::string_view LogicSystem::AnimNameForState(AnimState state) const
    {
        switch (state)
        {
        case AnimState::Run:       return "run";
        case AnimState::Attack1:   return "attack1";
        case AnimState::Attack2:   return "attack2";
        case AnimState::Attack3:   return "attack3";
        case AnimState::Throw:     return "throw";
        case AnimState::Knockback: return "knockback";
        case AnimState::Death:     return "death";
        case AnimState::Idle:
        default:                 return "idle";
        }
    }

    int LogicSystem::AnimationIndexForState(const SpriteAnimationComponent* comp, AnimState state) const
    {
        if (!comp)
            return -1;

        auto equalsIgnoreCase = [](std::string_view a, std::string_view b)
            {
                if (a.size() != b.size())
                    return false;
                for (std::size_t i = 0; i < a.size(); ++i)
                {
                    unsigned char c1 = static_cast<unsigned char>(a[i]);
                    unsigned char c2 = static_cast<unsigned char>(b[i]);
                    if (std::tolower(c1) != std::tolower(c2))
                        return false;
                }
                return true;
            };

        const std::string_view desired = AnimNameForState(state);
        for (std::size_t i = 0; i < comp->animations.size(); ++i)
        {
            if (equalsIgnoreCase(comp->animations[i].name, desired))
                return static_cast<int>(i);
        }

        return -1;
    }

    LogicSystem::AnimConfig LogicSystem::ConfigFromSpriteSheet(const SpriteAnimationComponent* comp, AnimState state) const
    {
        AnimConfig cfg = ConfigForState(state);
        if (!comp)
            return cfg;

        const int index = AnimationIndexForState(comp, state);
        if (index < 0 || index >= static_cast<int>(comp->animations.size()))
            return cfg;

        const auto& sheet = comp->animations[static_cast<std::size_t>(index)];
        cfg.cols = std::max(1, sheet.config.columns);
        cfg.rows = std::max(1, sheet.config.rows);
        cfg.frames = std::max(1, sheet.config.totalFrames);
        cfg.fps = sheet.config.fps;
        return cfg;
    }

    void LogicSystem::ApplyAnimationStateToComponent(AnimState state)
    {
        if (!IsAlive(player))
            return;

        auto* anim = player->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        if (!anim)
            return;

        const int index = AnimationIndexForState(anim, state);
        if (index >= 0 && index != anim->ActiveAnimationIndex())
            anim->SetActiveAnimation(index);
    }

    /*****************************************************************************************
      \brief Check if a given object pointer still exists in the factory.
      \param obj Candidate object pointer.
      \return true if obj is found among factory->Objects(); false otherwise.
      \note   Useful to invalidate cached pointers when levels reload or objects are destroyed.
    *****************************************************************************************/
    bool LogicSystem::IsAlive(GOC* obj) const
    {
        if (!obj || !factory)
            return false;
        for (auto& [id, ptr] : factory->Objects())
        {
            (void)id;
            if (ptr.get() == obj)
                return true;
        }
        return false;
    }

    /*****************************************************************************************
      \brief Find the first alive object with a PlayerComponent.
    *****************************************************************************************/
    GOC* LogicSystem::FindAnyAlivePlayer()
    {
        if (!factory)
            return nullptr;

        for (auto& [id, ptr] : factory->Objects())
        {
            if (auto* obj = ptr.get())
            {
                if (obj->GetComponentType<PlayerComponent>(ComponentTypeId::CT_PlayerComponent))
                    return obj;
            }
        }
        return nullptr;
    }

    /*****************************************************************************************
      \brief Refresh references after level load or object churn.
             - Updates levelObjects with LastLevelObjects()
             - Finds/validates player
             - Finds/validates a default collision target ("rect", case-insensitive)
             - Caches player size if needed
    *****************************************************************************************/
    void LogicSystem::RefreshLevelReferences()
    {
        if (!factory)
            return;

        // Rebuild the level object cache every frame so we only keep alive objects.
        // The previous implementation grabbed the snapshot returned by
        // GameObjectFactory::LastLevelObjects(), which is only updated when a level is
        // loaded/saved. Once gameplay started, pointers to objects that were destroyed
        // (e.g. the player being killed by enemies) remained inside levelObjects even
        // though the underlying memory had been freed. Systems like HitBoxSystem
        // iterate this list every frame and dereference each pointer to query
        // components. Walking into enemies would quickly destroy either the player or
        // an enemy, leaving a dangling pointer behind and eventually causing an access
        // violation when the stale pointer was dereferenced. Rebuilding the cache from
        // the factory’s current ownership map guarantees we only keep valid objects.
        levelObjects.clear();
        for (auto const& [id, obj] : factory->Objects())
        {
            (void)id;
            if (obj)
                levelObjects.push_back(obj.get());
        }

        if (!IsAlive(player))
            player = nullptr;
        if (!player)
        {
            player = FindAnyAlivePlayer();
            if (player)
            {
                std::cout << "[LogicSystem] Player re-assigned to another alive instance: "
                    << player->GetObjectName() << "\n";
            }
        }

        if (!IsAlive(collisionTarget))
            collisionTarget = nullptr;

        gateController.SetPlayer(player);

        auto nameEqualsIgnoreCase = [](const std::string& lhs, std::string_view rhs)
            {
                if (lhs.size() != rhs.size())
                    return false;
                for (std::size_t i = 0; i < lhs.size(); ++i)
                {
                    unsigned char c1 = static_cast<unsigned char>(lhs[i]);
                    unsigned char c2 = static_cast<unsigned char>(rhs[i]);
                    if (std::tolower(c1) != std::tolower(c2))
                        return false;
                }
                return true;
            };

        if (!collisionTarget)
        {
            for (auto* obj : levelObjects)
            {
                if (obj && nameEqualsIgnoreCase(obj->GetObjectName(), "rect"))
                {
                    collisionTarget = obj;
                    break;
                }
            }
        }

        gateController.RefreshGateReference(levelObjects);
    }

    /*****************************************************************************************
      \brief Step the sprite-sheet animation based on desired state (idle/run/attacks).
      \param dt      Delta time (seconds).
      \param wantRun Whether the input implies running (vs idle).
    *****************************************************************************************/
    void LogicSystem::UpdateAnimation(float dt, bool wantRun)
    {
        SpriteAnimationComponent* animComp = nullptr;
        if (IsAlive(player))
        {
            animComp = player->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        }
        auto* rb = IsAlive(player)
            ? player->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent)
            : nullptr;
        auto* health = IsAlive(player)
            ? player->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent)
            : nullptr;

        if (rb && rb->knockbackTime > 0.0f)
            rb->knockbackTime = std::max(0.0f, rb->knockbackTime - dt);

        const bool playerDead = health && health->playerHealth <= 0;

        if (playerDead)
        {
            SetAnimState(AnimState::Death);
        }
        else if (rb && rb->knockbackTime > 0.0f)
        {
            if (animState != AnimState::Knockback)
            {
                pendingThrow.active = false;
                throwRequestQueued = false;
                attackTimer = 0.f;
            }
            SetAnimState(AnimState::Knockback);
        }
        // If we are in an attack animation, let it run to completion.
        else if (IsAttackState(animState))
        {
            attackTimer -= dt;
            if (attackTimer <= 0.f)
            {
                attackTimer = 0.f;
                AnimState desired = wantRun ? AnimState::Run : AnimState::Idle;
                SetAnimState(desired);
            }
        }
        else
        {
            AnimState desired = wantRun ? AnimState::Run : AnimState::Idle;
            SetAnimState(desired);
        }

        const AnimConfig cfg = ConfigFromSpriteSheet(animComp, animState);
        frameClock += dt * cfg.fps;
        while (frameClock >= 1.f)
        {
            frameClock -= 1.f;
            frame = (frame + 1) % cfg.frames;
        }

        animInfo.frame = frame;
        animInfo.columns = cfg.cols;
        animInfo.rows = cfg.rows;
        animInfo.mode = ModeForState(animState);
        animInfo.running = (animState == AnimState::Run);
    }

    /*****************************************************************************************
      \brief Get the player's world position (if available).
      \param outX [out] Player world X.
      \param outY [out] Player world Y.
      \return true if player and TransformComponent are present; false otherwise.
    *****************************************************************************************/
    bool LogicSystem::GetPlayerWorldPosition(float& outX, float& outY) const
    {
        if (!IsAlive(player))
            return false;

        auto* tr = player->GetComponentType<Framework::TransformComponent>(
            Framework::ComponentTypeId::CT_TransformComponent);
        if (!tr)
            return false;

        outX = tr->x;
        outY = tr->y;
        return true;
    }

    std::filesystem::path LogicSystem::resolveData(std::string_view name) const
    {
        return Framework::ResolveDataPath(std::filesystem::path(name));
    }

    /*****************************************************************************************
      \brief Initialize the game logic systems and world.
             - Sets up crash logging (file + logcat mirror).
             - Installs terminate/signal handlers.
             - Instantiates factory; registers components; loads prefabs; creates initial level.
             - Discovers player and caches initial size; loads window config.
             - Builds HitBoxSystem and prints control help.
    *****************************************************************************************/
    void LogicSystem::Initialize()
    {
        auto crashLogDir = Framework::GetUserDocumentsDir() / "BloodyGoodCurry" / "logs";
        crashLogger = std::make_unique<CrashLogger>(crashLogDir.string(),
            std::string("crash.log"),
            std::string("ENGINE/CRASH"));
        g_crashLogger = crashLogger.get();
        std::cout << "[CrashLog] " << g_crashLogger->LogPath() << "\n";
#ifndef NDEBUG
        std::cout << "[CrashLog] Press F9 to force a crash-test (logs to file + logcat).\n";
#endif
        std::cout << "[CrashLog] Android builds mirror to ENGINE/CRASH in logcat.\n";

        InstallTerminateHandler();
        InstallSignalHandlers();

        factory = std::make_unique<GameObjectFactory>();
        RegisterComponent(TransformComponent);
        RegisterComponent(RenderComponent);
        RegisterComponent(CircleRenderComponent);
        RegisterComponent(GlowComponent);
        RegisterComponent(SpriteComponent);
        RegisterComponent(RigidBodyComponent);
        RegisterComponent(PlayerComponent);
        RegisterComponent(PlayerAttackComponent);
        RegisterComponent(PlayerHealthComponent);
        RegisterComponent(HitBoxComponent);
        RegisterComponent(SpriteAnimationComponent);
        RegisterComponent(EnemyComponent);
        RegisterComponent(EnemyAttackComponent);
        RegisterComponent(EnemyDecisionTreeComponent);
        RegisterComponent(EnemyHealthComponent);
        RegisterComponent(EnemyTypeComponent);
        RegisterComponent(AudioComponent);
        RegisterComponent(ZoomTriggerComponent);
        RegisterComponent(GateTargetComponent);
        RegisterComponent(PlayerHUDComponent);
        FACTORY = factory.get();
        gateController.SetFactory(factory.get());
        LoadPrefabs();

        auto playerPrefab = resolveData("player.json");
        std::cout << "[Prefab] Player path = " << std::filesystem::absolute(playerPrefab)
            << "  exists=" << std::filesystem::exists(playerPrefab) << "\n";
        std::filesystem::path startLevelPath = resolveData("level_RealTutorial.json");

#if SOFASPUDS_ENABLE_EDITOR
        const std::string& startLevelName = mygame::SelectedStartLevel();
        if (!startLevelName.empty())
        {
            std::filesystem::path requestedPath(startLevelName);
            startLevelPath = requestedPath.is_absolute() ? requestedPath : resolveData(startLevelName);
        }
#endif

        levelObjects = factory->CreateLevel(startLevelPath.string());

        const bool hasAnimatedStore = std::any_of(levelObjects.begin(), levelObjects.end(), [](Framework::GOC* obj) {
            if (!obj)
                return false;

            const std::string& name = obj->GetObjectName();
            return name == "HawkerStoreAnimated" || name == "Hawker_Store_Animated";
            });

        if (!hasAnimatedStore)
        {
            auto storePath = resolveData("hawker_store_animated.json");
            if (!std::filesystem::exists(storePath))
            {
                std::cerr << "[Prefab] Missing hawker_store_animated.json at " << storePath << "\n";
            }
            else if (auto* store = factory->Create(storePath.string()))
            {
                if (auto* tr = store->GetComponentType<Framework::TransformComponent>(
                    Framework::ComponentTypeId::CT_TransformComponent))
                {
                    tr->x = -0.5f;
                    tr->y = -0.9f;
                    tr->rot = 0.0f;
                }

                if (auto* render = store->GetComponentType<Framework::RenderComponent>(
                    Framework::ComponentTypeId::CT_RenderComponent))
                {
                    render->visible = true;
                }

                levelObjects.push_back(store);
            }
            else
            {
                std::cerr << "[Prefab] Failed to spawn Hawker_Store_Animated from " << storePath << "\n";
            }
        }
        RefreshLevelReferences();

        enemiesAlive = 0;
        for (auto* obj : levelObjects)
        {
            if (!obj) continue;

            if (obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
                enemiesAlive++;
        }

        WindowConfig cfg = LoadWindowConfig(resolveData("window.json").string());
        screenW = cfg.width;
        screenH = cfg.height;

        // Build HitBoxSystem after references are valid.
        if (hitBoxSystem)
        {
            hitBoxSystem->Shutdown();
            delete hitBoxSystem;
            hitBoxSystem = nullptr;
        }

        // Build HitBoxSystem after references are valid.
        hitBoxSystem = new HitBoxSystem(*this);
        hitBoxSystem->Initialize();

        // ---------------------------------------------------------------------
        // Preload Fire Enemy textures to avoid hitches when they first spawn.
        // ---------------------------------------------------------------------
        Resource_Manager::load(
            "fire_idle",
            Framework::ResolveAssetPath("Textures/Character/Fire Enemy_Sprite/Idle_Sprite.png").string()
        );
        Resource_Manager::load(
            "fire_attack",
            Framework::ResolveAssetPath("Textures/Character/Fire Enemy_Sprite/Fire Attack_Sprite.png").string()
        );
        Resource_Manager::load(
            "fire_projectile",
            Framework::ResolveAssetPath("Textures/Character/Fire Enemy_Sprite/FireProjectileSprite.png").string()
        );
        Resource_Manager::load(
            "fire_knockback",
            Framework::ResolveAssetPath("Textures/Character/Fire Enemy_Sprite/Knockback_Sprite.png").string()
        );
        Resource_Manager::load(
            "fire_death",
            Framework::ResolveAssetPath("Textures/Character/Fire Enemy_Sprite/Death_Sprite.png").string()
        );

        std::cout << "\n=== Controls ===\n"
            << "WASD: Move | Q/E: Rotate | Z/X: Scale | R: Reset\n"
            << "A/D held => Run animation, otherwise Idle\n"
            << "Left Mouse: Melee combo (3-hit)\n"
            << "Right Mouse: Throw projectile\n"
            << "F1: Toggle Performance Overlay (FPS & timings)\n"
#ifndef NDEBUG
            << "F9: Trigger crash logging test (SIGABRT)\n"
#endif
            << "=======================================\n";
    }

    /*****************************************************************************************
      \brief Per-frame update: input handling, physics intent, animation stepping, hitbox spawn,
             collision AABB bookkeeping, and crash-test handling.
      \param dt Delta time (seconds).
    *****************************************************************************************/
    void LogicSystem::Update(float dt)
    {
        TryGuard::Run([&] {
#ifndef NDEBUG
            bool triggerCrash = input.IsKeyPressed(GLFW_KEY_F9);
            if (triggerCrash && !crashTestLatched) {
                crashTestLatched = true;
                if (g_crashLogger) {
                    auto line = g_crashLogger->WriteWithStack("manual_trigger", "key=F9|stage=pre_abort");
                    g_crashLogger->Mirror(line);
                }
                std::cout << "[CrashLog] Deliberate crash requested via F9.\n";
                std::raise(SIGABRT);
            }
            else if (!triggerCrash) {
                crashTestLatched = false;
            }
#endif
            if (factory)
                factory->Update(dt);

            // Keep references fresh each frame in case of spawns/deletions.
            RefreshLevelReferences();

            RecountEnemies();

            if (input.IsKeyPressed(GLFW_KEY_P))
                std::cout << "Number of enemies alive: " << enemiesAlive << "\n";

            std::vector<GOC*> finishedVfx;

            auto AdvanceSpriteAnimations = [&](float step)
                {
                    if (!factory)
                        return;

                    for (auto& [id, objPtr] : factory->Objects())
                    {
                        (void)id;
                        auto* obj = objPtr.get();
                        if (!obj)
                            continue;

                        auto* anim = obj->GetComponentType<Framework::SpriteAnimationComponent>(
                            Framework::ComponentTypeId::CT_SpriteAnimationComponent);
                        if (!anim || (!anim->HasFrames() && !anim->HasSpriteSheets()))
                            continue;

                        anim->Advance(step);

                        auto* sprite = obj->GetComponentType<Framework::SpriteComponent>(
                            Framework::ComponentTypeId::CT_SpriteComponent);
                        if (!sprite)
                            continue;

                        if (anim->HasSpriteSheets())
                        {
                            auto sample = anim->CurrentSheetSample();
                            if (!sample.textureKey.empty())
                                sprite->texture_key = sample.textureKey;
                            if (sample.texture)
                                sprite->texture_id = sample.texture;
                        }
                        else
                        {
                            size_t frameIndex = anim->CurrentFrameIndex();
                            if (frameIndex >= anim->frames.size())
                                continue;

                            const auto& frame = anim->frames[frameIndex];
                            sprite->texture_key = frame.texture_key;
                            unsigned tex = anim->ResolveFrameTexture(frameIndex);
                            if (tex)
                                sprite->texture_id = tex;
                        }
                        if (IsImpactVfxObject(obj))
                        {
                            if (auto* active = anim->ActiveAnimation())
                            {
                                if (!active->config.loop && active->name == "impact")
                                {
                                    const int total = std::max(1, active->config.totalFrames);
                                    const int start = std::clamp(active->config.startFrame, 0, total - 1);
                                    const int end = (active->config.endFrame >= 0)
                                        ? std::clamp(active->config.endFrame, start, total - 1)
                                        : total - 1;

                                    const int current = std::clamp(active->currentFrame, 0, total - 1);
                                    if (current >= end)
                                        finishedVfx.push_back(obj);
                                }
                            }
                        }
                    }
                };

            AdvanceSpriteAnimations(dt);

            for (auto* vfx : finishedVfx)
            {
                if (factory)
                    factory->Destroy(vfx);
            }

            // --- Enemy loop: reacts to player's ACTIVE hitbox if both sides are valid ---
            for (auto* obj : levelObjects)
            {
                if (!obj) continue;

                if (obj->GetObjectName() == "Enemy")
                {
                    auto* rb = obj->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
                    auto* tr = obj->GetComponentType<TransformComponent>(ComponentTypeId::CT_TransformComponent);
                    if (!(rb && tr)) continue;

                    AABB enemyBox(tr->x, tr->y, rb->width, rb->height);

                    if (IsAlive(player))
                    {
                        auto* attack = player->GetComponentType<PlayerAttackComponent>(ComponentTypeId::CT_PlayerAttackComponent);
                        if (attack && attack->hitbox && attack->hitbox->active)
                        {
                            AABB playerHitBox(
                                attack->hitbox->spawnX,
                                attack->hitbox->spawnY,
                                attack->hitbox->width,
                                attack->hitbox->height
                            );

                            if (Collision::CheckCollisionRectToRect(playerHitBox, enemyBox))
                            {
                                std::cout << "Enemy hit by player at (" << tr->x << ", " << tr->y << ")\n";
                                attack->hitbox->DeactivateHurtBox();
                            }
                        }
                    }
                }
            }

            if (hitBoxSystem)
                hitBoxSystem->Update(dt);

            // If player pointer is gone this frame, bail out from player-driven logic.
            if (!IsAlive(player)) {
                player = nullptr;
                return;
            }

            auto mouse = input.Manager().GetMouseState();

            auto* tr = player->GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent);
            auto* rc = player->GetComponentType<Framework::RenderComponent>(
                Framework::ComponentTypeId::CT_RenderComponent);
            auto* rb = player->GetComponentType<Framework::RigidBodyComponent>(
                Framework::ComponentTypeId::CT_RigidBodyComponent);
            auto* attack = player->GetComponentType<Framework::PlayerAttackComponent>(
                Framework::ComponentTypeId::CT_PlayerAttackComponent);
            auto* audio = player->GetComponentType<Framework::AudioComponent>
                (Framework::ComponentTypeId::CT_AudioComponent);

            // --- Mouse to world: use RenderSystem camera for consistent world-space aiming ---
            float mouseWorldX = 0.0f;
            float mouseWorldY = 0.0f;
            bool  mouseInsideViewport = false;

            // Direction from player -> mouse in world space (normalized)
            float aimDirX = 0.0f;
            float aimDirY = 0.0f;

            if (tr)
            {
                if (auto* rs = RenderSystem::Get())
                {
                    if (rs->ScreenToWorld(mouse.x, mouse.y,
                        mouseWorldX, mouseWorldY, mouseInsideViewport)
                        && mouseInsideViewport)
                    {
                        const float dx = mouseWorldX - tr->x;
                        const float dy = mouseWorldY - tr->y;
                        const float lenSq = dx * dx + dy * dy;
                        if (lenSq > 1e-6f)
                        {
                            const float invLen = 1.0f / std::sqrt(lenSq);
                            aimDirX = dx * invLen;
                            aimDirY = dy * invLen;
                        }

                        // Flip sprite by width sign based on world-space direction.
                        if (rc && aimDirX != 0.0f)
                        {
                            if (aimDirX >= 0.0f)
                                rc->w = std::abs(rc->w);
                            else
                                rc->w = -std::abs(rc->w);
                        }
                    }
                }
            }

            auto* playerHealth =
                player->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent);

            // Velocity intent set on RigidBody; an external system integrates it.
            if (rb && tr && playerHealth && !playerHealth->isDead)
            {
                if (rb->lungeTime > 0.0f)
                {
                    rb->lungeTime -= dt;
                    if (rb->lungeTime <= 0.0f)
                    {
                        rb->velX = 0.0f;
                        rb->lungeTime = 0.0f;
                    }
                }
                else
                {
                    float forwardX = (rc) ? ((rc->w >= 0.0f) ? 1.0f : -1.0f) : 1.0f;
                    float speedModifier = 1.0f;
                    if ((input.IsKeyHeld(GLFW_KEY_D) && forwardX < 0) ||
                        (input.IsKeyHeld(GLFW_KEY_A) && forwardX > 0))
                    {
                        speedModifier = 0.75f; 
                    }
                    if (input.IsKeyHeld(GLFW_KEY_D)) rb->velX = std::max(rb->velX, 1.f * speedModifier);
                    if (input.IsKeyHeld(GLFW_KEY_A)) rb->velX = std::min(rb->velX, -1.f * speedModifier);

                    if (!input.IsKeyHeld(GLFW_KEY_A) && !input.IsKeyHeld(GLFW_KEY_D))
                        rb->velX *= rb->dampening;
                    if (input.IsKeyHeld(GLFW_KEY_W)) rb->velY = std::max(rb->velY, 1.f);
                    if (input.IsKeyHeld(GLFW_KEY_S)) rb->velY = std::min(rb->velY, -1.f);
                    if (!input.IsKeyHeld(GLFW_KEY_W) && !input.IsKeyHeld(GLFW_KEY_S))
                        rb->velY *= rb->dampening;
                }
   
            }

            // Running state if any movement keys are held (arrow keys supported too).
            const bool wantRun = input.IsKeyHeld(GLFW_KEY_A) ||
                input.IsKeyHeld(GLFW_KEY_D) ||
                input.IsKeyHeld(GLFW_KEY_W) ||
                input.IsKeyHeld(GLFW_KEY_S) ||
                input.IsKeyHeld(GLFW_KEY_LEFT) ||
                input.IsKeyHeld(GLFW_KEY_RIGHT) ||
                input.IsKeyHeld(GLFW_KEY_UP) ||
                input.IsKeyHeld(GLFW_KEY_DOWN);

            // Update PlayerAttackComponent (handles hitbox lifetime)
            if (attack && tr && !playerHealth->isDead)
            {
                attack->Update(dt, tr);
            }

            if (throwCooldownTimer > 0.0f)
            {
                throwCooldownTimer = std::max(0.0f, throwCooldownTimer - dt);
            }
            if (input.IsMousePressed(GLFW_MOUSE_BUTTON_RIGHT))
            {
                throwRequestQueued = true;
            }
            if (input.IsMouseReleased(GLFW_MOUSE_BUTTON_RIGHT))
            {
                throwRequestQueued = false;
            }

            // Handle attack input: spawn through PlayerAttackComponent only (single source of truth).
            if (playerHealth && !playerHealth->isDead && input.IsMousePressed(GLFW_MOUSE_BUTTON_LEFT) && attack && tr && rc)
            {
                // Only spawn if we have a valid direction (mouse in viewport & not exactly on player).
                if (aimDirX != 0.0f || aimDirY != 0.0f)
                {
                    // Determine left/right direction
                    float dirX = (mouseWorldX > tr->x) ? 1.0f : -1.0f;
                    rb->velX = dirX * 0.1f;        // speed
                    rb->lungeTime = 0.15f;         // duration
                    auto attackTr = *tr;
                    const float offset = 0.05f;
                    const float halfW = std::abs(rc->w) * 0.5f;
                    const float halfH = rc->h * 0.5f;

                    attackTr.x = tr->x + aimDirX * (halfW + offset);
                    attackTr.y = tr->y + aimDirY * (halfH + offset);

                    hitBoxSystem->SpawnHitBox(player,
                        attackTr.x, attackTr.y,
                        0.1f, 0.1f,
                        1.0f, 0.2f,
                        HitBoxComponent::Team::Player);

                    std::cout << "Hurtbox spawned at (" << attackTr.x << ", " << attackTr.y << ")\n";
                    // Start / advance melee combo animation (Attack1,2,3 cycling)
                    BeginComboAttack();
                }

            }
            else if (playerHealth && !playerHealth->isDead && throwRequestQueued && attack && tr && rc)
            {
                const bool canThrow = throwCooldownTimer <= 0.0f && !pendingThrow.active && !IsAttackState(animState);

                // Only spawn if we have a valid direction (mouse in viewport & not exactly on player).
                if (canThrow && (aimDirX != 0.0f || aimDirY != 0.0f))
                {
                    auto attackTr = *tr;
                    const float offset = 0.05f;
                    const float halfW = std::abs(rc->w) * 0.5f;
                    const float halfH = rc->h * 0.5f;

                    attackTr.x = tr->x + aimDirX * (halfW + offset);
                    attackTr.y = tr->y + aimDirY * (halfH + offset);

                    pendingThrow.active = true;
                    pendingThrow.spawnX = attackTr.x;
                    pendingThrow.spawnY = attackTr.y;
                    pendingThrow.dirX = aimDirX;
                    pendingThrow.dirY = aimDirY;

                    BeginThrowAttack();
                    throwCooldownTimer = std::max(throwCooldownTimer, AttackDurationForState(AnimState::Throw));
                    throwRequestQueued = false;
                }
            }

            // Finally, advance the main character animation (idle/run/attack combo)
            UpdateAnimation(dt, wantRun);

            if (pendingThrow.active && attackTimer <= 0.0f)
            {
                if (hitBoxSystem)
                {
                    hitBoxSystem->SpawnProjectile(player,
                        pendingThrow.spawnX, pendingThrow.spawnY,
                        pendingThrow.dirX, pendingThrow.dirY,
                        0.3f,
                        0.1f, 0.1f,
                        1.0f, 5.f, HitBoxComponent::Team::Thrown);

                    std::cout << "Hurtbox spawned at (" << pendingThrow.spawnX << ", " << pendingThrow.spawnY << ")\n";
                }
                if (audio)
                {
                    audio->TriggerSound("GrappleShoot");
                }
                pendingThrow.active = false;
            }

            gateController.UpdateGateUnlockState();
            std::string targetLevel;
            if (gateController.ShouldTransitionOnPlayerContact(pendingLevelTransition, targetLevel))
            {
                pendingLevelTransition = true;
                std::filesystem::path targetPath(targetLevel);
                if (!targetPath.is_absolute())
                {
                    targetPath = resolveData(targetLevel);
                }
                LoadLevelAndResetState(targetPath);
            }

            // Collision debug info (player vs a target rect)
            collisionInfo.playerValid = false;
            collisionInfo.targetValid = false;

            if (tr && rb)
            {
                collisionInfo.player = AABB(tr->x, tr->y, rb->width, rb->height);
                collisionInfo.playerValid = true;
            }

            if (IsAlive(collisionTarget))
            {
                auto* tr2 = collisionTarget->GetComponentType<Framework::TransformComponent>(
                    Framework::ComponentTypeId::CT_TransformComponent);
                auto* rb2 = collisionTarget->GetComponentType<Framework::RigidBodyComponent>(
                    Framework::ComponentTypeId::CT_RigidBodyComponent);
                if (tr2 && rb2)
                {
                    collisionInfo.target = AABB(tr2->x, tr2->y, rb2->width, rb2->height);
                    collisionInfo.targetValid = true;
                }
            }
            }, "LogicSystem::Update");
    }

    void LogicSystem::LoadLevelAndResetState(const std::filesystem::path& levelPath)
    {
        if (!factory)
            return;

        for (auto const& [id, obj] : factory->Objects())
        {
            (void)id;
            if (obj)
                factory->Destroy(obj.get());
        }
        factory->Update(0.0f);

        {
            const unsigned pagesFreed = Framework::GameObjectPool::Storage().Allocator().FreeEmptyPages();
            if (pagesFreed > 0) {
                std::cout << "[Allocator] FreeEmptyPages trimmed " << pagesFreed
                    << " empty pages after level unload.\n";
            }
        }

        levelObjects = factory->CreateLevel(levelPath.string());

        player = nullptr;
        collisionTarget = nullptr;
        pendingLevelTransition = false;
        animState = AnimState::Idle;
        frame = 0;
        frameClock = 0.f;
        attackTimer = 0.f;
        comboStep = 0;
        animInfo = AnimationInfo{};
        collisionInfo = CollisionInfo{};
        gateController.Reset();
        gateController.SetPlayer(nullptr);

        RefreshLevelReferences();
        enemiesAlive = 0;
        for (auto* obj : levelObjects)
        {
            if (!obj) continue;

            if (obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
                enemiesAlive++;
        }

    }

    /*****************************************************************************************
       \brief Reload the current level (or a default one) and reset cached state.
             - Destroys all live objects, recreates the level, clears cached pointers/state,
               then refreshes references.
    *****************************************************************************************/
    void LogicSystem::ReloadLevel()
    {
        if (!factory)
            return;

        std::filesystem::path levelPath = factory->LastLevelPath();
        if (levelPath.empty())
            levelPath = resolveData("level.json");

        LoadLevelAndResetState(levelPath);
    }

    /*****************************************************************************************
      \brief Shutdown and release owned systems/resources.
             - Clears references, shuts down factory and unloads prefabs.
             - Tears down crash logger and HitBoxSystem.
    *****************************************************************************************/
    void LogicSystem::Shutdown()
    {
        levelObjects.clear();
        collisionTarget = nullptr;
        player = nullptr;
        gateController.Reset();
        gateController.SetFactory(nullptr);

        if (factory) {
            factory->Shutdown();
            factory.reset();
        }
        UnloadPrefabs();

        if (crashLogger)
        {
            g_crashLogger = nullptr;
            crashLogger.reset();
        }

        if (hitBoxSystem)
        {
            hitBoxSystem->Shutdown();
            delete hitBoxSystem;
            hitBoxSystem = nullptr;
        }
    }


    void LogicSystem::RecountEnemies()
    {
        int count = 0;

        for (auto* obj : levelObjects)
        {
            if (!obj)
                continue;

            if (obj->GetComponentType<EnemyComponent>(ComponentTypeId::CT_EnemyComponent))
            {
                auto* health = obj->GetComponentType<EnemyHealthComponent>(ComponentTypeId::CT_EnemyHealthComponent);

                if (!health || !health->isDead)
                    count++;
            }
        }

        enemiesAlive = count;
    }
} // namespace Framework
