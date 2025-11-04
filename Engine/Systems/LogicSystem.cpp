
#include "Systems/LogicSystem.h"
#include <cctype>
#include <string_view>

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
        RegisterComponent(PlayerAttackComponent);
        RegisterComponent(PlayerHealthComponent);
        RegisterComponent(HurtBoxComponent);

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
            float normalizedX{};
            float normalizedY{};
            if (tr && rc) {
                normalizedX = (float)((mouse.x / window->Width()) * 2.0 - 1.0);
                normalizedY = (float)((mouse.y / window->Height()) * -2.0 + 1.0);
                // rc->w is the image flipping thingamajic
                if (normalizedX > tr->x)
                    rc->w = std::abs(rc->w);
                else if (normalizedX < tr->x)
                    rc->w = -std::abs(rc->w);

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

                auto* hurtbox = player->GetComponentType<Framework::HurtBoxComponent>(
                    Framework::ComponentTypeId::CT_HurtBoxComponent);

                static float hurtTimer = hurtbox->duration; // Cooldown timer for attack

                if (hurtbox && tr && rc)
                {
                    if (input.IsMousePressed(GLFW_MOUSE_BUTTON_LEFT))
                    {
                        float dx = normalizedX - tr->x;
                        float dy = normalizedY - tr->y;

                        float len = std::sqrt(dx * dx + dy * dy);
                        if (len > 0.0001f)
                        {
                            dx /= len;
                            dy /= len;
                        }

                        float offset = 0.05f; // This is the offset of where the hurtbox will spawn
                        float spawnX = tr->x + dx * (std::abs(rc->w) * 0.5f + hurtbox->width * 0.5f + offset);
                        float spawnY = tr->y + dy * (rc->h * 0.5f + hurtbox->height * 0.5f + offset);

                        hurtbox->spawnX = spawnX;
                        hurtbox->spawnY = spawnY;

                        hurtbox->ActivateHurtBox();

                        // for debugging
                        std::cout << "Hurtbox spawned at (" << spawnX << ", " << spawnY << ")\n";
                    }

                    if (hurtbox->active)
                    {
                        hurtTimer -= dt;
                        if (hurtTimer <= 0.0f)
                        {
                            hurtbox->DeactivateHurtBox();
                            hurtTimer = hurtbox->duration;
                        }
                    }
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