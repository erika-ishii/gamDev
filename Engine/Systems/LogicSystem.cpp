/*********************************************************************************************
 \file      LogicSystem.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Author, 10%
           
 \brief     Core gameplay loop and input-driven logic for the sample sandbox.
 \details   This module owns high-level game state orchestration:
            - Factory lifetime: component registration, prefab loading/unloading, level create/destroy.
            - Player references: discovery, cached size for scale operations, animation state machine.
            - Input mapping: WASD movement, Q/E rotation, Z/X scale, R reset, Shift accelerator.
            - HitBoxSystem integration: spawns short-lived attack boxes towards cursor on LMB.
            - Crash logging utilities: F9 forces a safe, logged crash for robustness testing.
            - Collision �debug info�: builds AABBs for player/target to visualize or check overlap.

            Performance & stability:
            * Uses TryGuard::Run to isolate Update() logic and attribute errors with a tag.
            * Minimizes per-frame object lookups by caching player/targets (validated via IsAlive()).
            * Time-based animation frame stepping decoupled from render rate via fps in AnimConfig.

            Conventions:
            * Screen coordinates are in normalized device space (-1..+1) for mouse mapping.
            * Layering, physics, and rendering are handled by their respective systems; LogicSystem
              manipulates components (Transform/Render/RigidBody) but does not own them.
 \copyright
            All content �2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Systems/LogicSystem.h"
#include <cctype>
#include <string_view>
#include <csignal>

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
      \brief Get the currently active animation configuration (idle vs run).
      \return const AnimConfig& Either runConfig or idleConfig depending on AnimState.
    *****************************************************************************************/
    const LogicSystem::AnimConfig& LogicSystem::CurrentConfig() const
    {
        return animState == AnimState::Run ? runConfig : idleConfig;
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
      \brief Cache the player�s base rectangle width/height once for scale operations.
      \note  Called after player is discovered; resets rectScale to 1.f and marks captured=true.
    *****************************************************************************************/
    GOC* LogicSystem::FindAnyAlivePlayer()
    {
        if (!factory)
            return nullptr;

        for (auto& [id, ptr] : factory->Objects())
        {
            if (auto* obj = ptr.get())
            {
                // Check if this object has a PlayerComponent
                if (obj->GetComponentType<PlayerComponent>(
                    ComponentTypeId::CT_PlayerComponent))
                {
                    return obj; // return the first alive player found
                }
            }
        }
        return nullptr;
    }
    void LogicSystem::CachePlayerSize()
    {
        if (!player)
            return;

        if (auto* rc = player->GetComponentType<Framework::RenderComponent>(
            Framework::ComponentTypeId::CT_RenderComponent))
        {
            rectBaseW = rc->w;
            rectBaseH = rc->h;
            rectScale = 1.f;
            captured = true;
        }
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

        levelObjects = factory->LastLevelObjects();

        if (!IsAlive(player))
            player = nullptr;
        if (!player)
        {
            player = FindAnyAlivePlayer();
            if (player)
            {
                std::cout << "[LogicSystem] Player re-assigned to another alive instance: "
                    << player->GetObjectName() << "\n";
                captured = false; // force CachePlayerSize() again
            }
        }
        if (player && !captured)
            CachePlayerSize();

        if (!IsAlive(collisionTarget))
            collisionTarget = nullptr;

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

        if (player && !captured)
        {
            CachePlayerSize();
        }
    }

    /*****************************************************************************************
      \brief Step the sprite-sheet animation based on desired run/idle state.
      \param dt      Delta time (seconds).
      \param wantRun Whether the input implies running (vs idle).
      \details
        - Switching state resets frame and local clock.
        - frameClock accumulates fps-scaled time; increments frame when >= 1.0.
        - animInfo is updated for the renderer to pick the correct UV cell.
    *****************************************************************************************/
    void LogicSystem::UpdateAnimation(float dt, bool wantRun)
    {
        AnimState desired = wantRun ? AnimState::Run : AnimState::Idle;
        if (desired != animState)
        {
            animState = desired;
            frame = 0;
            frameClock = 0.f;
        }

        const AnimConfig& cfg = CurrentConfig();
        frameClock += dt * cfg.fps;
        while (frameClock >= 1.f)
        {
            frameClock -= 1.f;
            frame = (frame + 1) % cfg.frames;
        }

        animInfo.frame = frame;
        animInfo.columns = cfg.cols;
        animInfo.rows = cfg.rows;
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
        if (!player)
            return false;

        auto* tr = player->GetComponentType<Framework::TransformComponent>(
            Framework::ComponentTypeId::CT_TransformComponent);
        if (!tr)
            return false;

        outX = tr->x;
        outY = tr->y;
        return true;
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
        crashLogger = std::make_unique<CrashLogger>(std::string("../../logs"),
            std::string("crash.log"),
            std::string("ENGINE/CRASH"));
        g_crashLogger = crashLogger.get();
        std::cout << "[CrashLog] " << g_crashLogger->LogPath() << "\n";
        std::cout << "[CrashLog] Press F9 to force a crash-test (logs to file + logcat).\n";
        std::cout << "[CrashLog] Android builds mirror to ENGINE/CRASH in logcat.\n";

        InstallTerminateHandler();
        InstallSignalHandlers();

        factory = std::make_unique<GameObjectFactory>();
        RegisterComponent(TransformComponent);
        RegisterComponent(RenderComponent);
        RegisterComponent(CircleRenderComponent);
        RegisterComponent(SpriteComponent);
        RegisterComponent(RigidBodyComponent);
        RegisterComponent(PlayerComponent);
        RegisterComponent(PlayerAttackComponent);
        RegisterComponent(PlayerHealthComponent);
        RegisterComponent(HitBoxComponent);

        RegisterComponent(EnemyComponent);
        RegisterComponent(EnemyAttackComponent);
        RegisterComponent(EnemyDecisionTreeComponent);
        RegisterComponent(EnemyHealthComponent);
        RegisterComponent(EnemyTypeComponent);
        FACTORY = factory.get();
        LoadPrefabs();

        auto playerPrefab = std::string("../../Data_Files/player.json");
        std::cout << "[Prefab] Player path = " << std::filesystem::absolute(playerPrefab)
            << "  exists=" << std::filesystem::exists(playerPrefab) << "\n";

        levelObjects = factory->CreateLevel("../../Data_Files/level.json");

        RefreshLevelReferences();

        WindowConfig cfg = LoadWindowConfig("../../Data_Files/window.json");
        screenW = cfg.width;
        screenH = cfg.height;

        // Build HitBoxSystem after references are valid.
        hitBoxSystem = new HitBoxSystem(*this);
        hitBoxSystem->Initialize();

        std::cout << "\n=== Controls ===\n"
            << "WASD: Move | Q/E: Rotate | Z/X: Scale | R: Reset\n"
            << "A/D held => Run animation, otherwise Idle\n"
            << "F1: Toggle Performance Overlay (FPS & timings)\n"
            << "F9: Trigger crash logging test (SIGABRT)\n"
            << "=======================================\n";
    }

    /*****************************************************************************************
      \brief Per-frame update: input handling, physics intent, animation stepping, hitbox spawn,
             collision AABB bookkeeping, and crash-test handling.
      \param dt Delta time (seconds).
      \details
        - Press F9 to generate a deliberate, logged crash (latched to prevent spam).
        - Delegates physics/AI progression to factory->Update(dt).
        - Player rotation: Q/E (�90 deg/s), clamp to [-pi, +pi], R to reset.
        - Player scale: Z/X (�rate), clamped to [0.25, 4.0], R to reset.
        - Velocity intent from WASD mapped into RigidBody (actual motion elsewhere).
        - Sprite �flip� via rc->w sign compared against mouse X for simple facing.
        - LMB spawns a timed HitBox in the look direction (towards cursor).
    *****************************************************************************************/
    void LogicSystem::Update(float dt)
    {
        TryGuard::Run([&] {
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

            if (factory)
                factory->Update(dt);

            RefreshLevelReferences();

            if (hitBoxSystem)
                hitBoxSystem->Update(dt);

            if (!player)
                return;

            auto mouse = input.Manager().GetMouseState();

            auto* tr = player->GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent);
            auto* rc = player->GetComponentType<Framework::RenderComponent>(
                Framework::ComponentTypeId::CT_RenderComponent);
            auto* rb = player->GetComponentType<Framework::RigidBodyComponent>(
                Framework::ComponentTypeId::CT_RigidBodyComponent);

            const float rotSpeed = DegToRad(90.f);
            const float scaleRate = 1.5f;
            const bool shift = input.IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                input.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);
            const float accel = shift ? 3.f : 1.f;

            // Rotation controls (Q/E), clamped to [-pi, +pi], R to reset.
            if (tr)
            {
                if (input.IsKeyPressed(GLFW_KEY_Q)) tr->rot += rotSpeed * dt * accel;
                if (input.IsKeyPressed(GLFW_KEY_E)) tr->rot -= rotSpeed * dt * accel;
                if (tr->rot > 3.14159265f)  tr->rot -= 6.28318530f;
                if (tr->rot < -3.14159265f) tr->rot += 6.28318530f;
                if (input.IsKeyPressed(GLFW_KEY_R)) tr->rot = 0.f;
            }

            // Scaling controls (Z/X), clamped; R resets scale and size.
            if (rc)
            {
                if (input.IsKeyPressed(GLFW_KEY_X)) rectScale *= (1.f + scaleRate * dt * accel);
                if (input.IsKeyPressed(GLFW_KEY_Z)) rectScale *= (1.f - scaleRate * dt * accel);
                rectScale = std::clamp(rectScale, 0.25f, 4.0f);
                if (input.IsKeyPressed(GLFW_KEY_R)) rectScale = 1.f;
                rc->w = rectBaseW * rectScale;
                rc->h = rectBaseH * rectScale;
            }

            // Map mouse to NDC (-1..+1) for simple facing/aiming computations.
            float normalizedX{};
            float normalizedY{};
            if (tr && rc) {
                normalizedX = (float)((mouse.x / window->Width()) * 2.0 - 1.0);
                normalizedY = (float)((mouse.y / window->Height()) * -2.0 + 1.0);

                // Flip sprite by width sign based on mouse relative position.
                if (normalizedX > tr->x)
                    rc->w = std::abs(rc->w);
                else if (normalizedX < tr->x)
                    rc->w = -std::abs(rc->w);
            }

            // Velocity intent set on RigidBody; an external system integrates it.
            if (rb && tr)
            {
                rb->velX = 0.0f;
                rb->velY = 0.0f;

                if (input.IsKeyHeld(GLFW_KEY_D)) rb->velX = 1.f;
                if (input.IsKeyHeld(GLFW_KEY_A)) rb->velX = -1.f;
                if (input.IsKeyHeld(GLFW_KEY_W)) rb->velY = 1.f;
                if (input.IsKeyHeld(GLFW_KEY_S)) rb->velY = -1.f;
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

            UpdateAnimation(dt, wantRun);

            // Collision debug info (player vs a target rect)
            collisionInfo.playerValid = false;
            collisionInfo.targetValid = false;

            if (tr && rb)
            {
                collisionInfo.player = AABB(tr->x, tr->y, rb->width, rb->height);
                collisionInfo.playerValid = true;

                // LMB spawns a short-lived hit box in the facing direction (towards cursor).
                if (input.IsMousePressed(GLFW_MOUSE_BUTTON_LEFT) && hitBoxSystem)
                {
                    float dx = normalizedX - tr->x;
                    float dy = normalizedY - tr->y;

                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len > 0.0001f)
                    {
                        dx /= len;
                        dy /= len;
                    }

                    // Offset places the hitbox slightly ahead of the player bounds.
                    float offset = 0.05f;
                    float spawnX = tr->x + dx * (std::abs(rc->w) * 0.5f + 0.25f + offset);
                    float spawnY = tr->y + dy * (rc->h * 0.5f + 0.25f + offset);

                    float hitboxWidth = 0.5f;
                    float hitboxHeight = 0.5f;
                    float hitboxDamage = 1.0f;
                    float hitboxDuration = 0.1f;

                    hitBoxSystem->SpawnHitBox(player, spawnX, spawnY, hitboxWidth, hitboxHeight, hitboxDamage, hitboxDuration);

                    // Debug print for visibility.
                    std::cout << "Hurtbox spawned at (" << spawnX << ", " << spawnY << ")\n";
                }
            }

            if (collisionTarget)
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

    /*****************************************************************************************
      \brief Reload the current level (or a default one) and reset cached state.
             - Destroys all live objects, recreates the level, clears cached pointers/state,
               then refreshes references and caches player size again.
    *****************************************************************************************/
    void LogicSystem::ReloadLevel()
    {
        if (!factory)
            return;

        std::filesystem::path levelPath = factory->LastLevelPath();
        if (levelPath.empty())
            levelPath = "../../Data_Files/level.json";

        for (auto const& [id, obj] : factory->Objects())
        {
            (void)id;
            if (obj)
                factory->Destroy(obj.get());
        }
        factory->Update(0.0f);

        levelObjects = factory->CreateLevel(levelPath.string());

        player = nullptr;
        collisionTarget = nullptr;
        captured = false;
        rectScale = 1.f;
        rectBaseW = 0.5f;
        rectBaseH = 0.5f;
        animState = AnimState::Idle;
        frame = 0;
        frameClock = 0.f;
        animInfo = AnimationInfo{};
        collisionInfo = CollisionInfo{};

        RefreshLevelReferences();
        CachePlayerSize();
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
} // namespace Framework
