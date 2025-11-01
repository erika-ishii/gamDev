
#include "Systems/LogicSystem.h"

namespace Framework {
    LogicSystem::LogicSystem(gfx::Window& window, InputSystem& input)
        : window(&window), input(input) {
    }

    LogicSystem::~LogicSystem() = default;

    const LogicSystem::AnimConfig& LogicSystem::CurrentConfig() const
    {
        return animState == AnimState::Run ? runConfig : idleConfig;
    }


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

    void LogicSystem::RefreshLevelReferences()
    {
        if (!factory)
            return;

        levelObjects = factory->LastLevelObjects();

        if (!IsAlive(player))
            player = nullptr;
        if (!player)
        {
            for (auto* obj : levelObjects)
            {
                if (obj && obj->GetObjectName() == "Player")
                {
                    player = obj;
                    break;
                }
            }
        }

        if (!IsAlive(collisionTarget))
            collisionTarget = nullptr;
        if (!collisionTarget)
        {
            for (auto* obj : levelObjects)
            {
                if (obj && obj->GetObjectName() == "rect")
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
    void LogicSystem::Initialize()
    {
        crashLogger = std::make_unique<CrashLogger>(std::string("../../logs"),
            std::string("crash.log"),
            std::string("ENGINE/CRASH"));
        g_crashLogger = crashLogger.get();
        std::cout << "[CrashLog] " << g_crashLogger->LogPath() << "\n";

        InstallTerminateHandler();
        InstallSignalHandlers();

        factory = std::make_unique<GameObjectFactory>();
        RegisterComponent(TransformComponent);
        RegisterComponent(RenderComponent);
        RegisterComponent(CircleRenderComponent);
        RegisterComponent(SpriteComponent);
        RegisterComponent(RigidBodyComponent);
        RegisterComponent(PlayerComponent);
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

        std::cout << "\n=== Controls ===\n"
            << "WASD: Move | Q/E: Rotate | Z/X: Scale | R: Reset\n"
            << "A/D held => Run animation, otherwise Idle\n"
            << "F1: Toggle Performance Overlay (FPS & timings)\n"
            << "=======================================\n";
    }

    void LogicSystem::Update(float dt)
    {
        TryGuard::Run([&] {
            if (factory)
                factory->Update(dt);

            RefreshLevelReferences();

            if (!player)
                return;

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

            if (tr)
            {
                if (input.IsKeyPressed(GLFW_KEY_Q)) tr->rot += rotSpeed * dt * accel;
                if (input.IsKeyPressed(GLFW_KEY_E)) tr->rot -= rotSpeed * dt * accel;
                if (tr->rot > 3.14159265f)  tr->rot -= 6.28318530f;
                if (tr->rot < -3.14159265f) tr->rot += 6.28318530f;
                if (input.IsKeyPressed(GLFW_KEY_R)) tr->rot = 0.f;
            }

            if (rc)
            {
                if (input.IsKeyPressed(GLFW_KEY_X)) rectScale *= (1.f + scaleRate * dt * accel);
                if (input.IsKeyPressed(GLFW_KEY_Z)) rectScale *= (1.f - scaleRate * dt * accel);
                rectScale = std::clamp(rectScale, 0.25f, 4.0f);
                if (input.IsKeyPressed(GLFW_KEY_R)) rectScale = 1.f;
                rc->w = rectBaseW * rectScale;
                rc->h = rectBaseH * rectScale;
            }

            if (rb && tr)
            {
                rb->velX = 0.0f;
                rb->velY = 0.0f;

                if (input.IsKeyHeld(GLFW_KEY_D)) rb->velX = 1.f;
                if (input.IsKeyHeld(GLFW_KEY_A)) rb->velX = -1.f;
                if (input.IsKeyHeld(GLFW_KEY_W)) rb->velY = 1.f;
                if (input.IsKeyHeld(GLFW_KEY_S)) rb->velY = -1.f;
            }

            const bool wantRun = input.IsKeyHeld(GLFW_KEY_A) ||
                input.IsKeyHeld(GLFW_KEY_D) ||
                input.IsKeyHeld(GLFW_KEY_W) ||
                input.IsKeyHeld(GLFW_KEY_S) ||
                input.IsKeyHeld(GLFW_KEY_LEFT) ||
                input.IsKeyHeld(GLFW_KEY_RIGHT) ||
                input.IsKeyHeld(GLFW_KEY_UP) ||
                input.IsKeyHeld(GLFW_KEY_DOWN);

            UpdateAnimation(dt, wantRun);

            collisionInfo.playerValid = false;
            collisionInfo.targetValid = false;

            if (tr && rb)
            {
                collisionInfo.player = AABB(tr->x, tr->y, rb->width, rb->height);
                collisionInfo.playerValid = true;
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
    }
}