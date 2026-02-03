/*********************************************************************************************
 \file      LogicSystem.h
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Author, 10%
            yimo kong (yimo.kong@digipen.edu)      - Author, 10%
 \brief     Game logic coordinator: level load/refresh, input-driven updates, animation.
 \details   Owns the GameObjectFactory and level objects, advances player/enemy state each
            frame, updates sprite animation, exposes simple collision snapshots, and
            bridges to HitBoxSystem.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include "Factory/Factory.h"
#include "Composition/PrefabManager.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/GlowComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/SpriteAnimationComponent.h"
#include "Component/PlayerComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/PlayerHealthComponent.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Graphics/PlayerHUD.h"
#include "Component/GateTargetComponent.h"
#include "Component/ZoomTriggerComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include <Serialization/JsonSerialization.h>
#include "Logic/GateController.h"
#include "InputSystem.h"
#include "HitBoxSystem.h"
#include "Config/WindowConfig.h"
#include "Debug/CrashLogger.hpp"
#include "Graphics/Window.hpp"
#include "Physics/Collision/Collision.h"
#include "Component/HitBoxComponent.h"
#include "../../Sandbox/MyGame/MathUtils.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <memory>
#include <vector>

namespace gfx { class Window; }
class CrashLogger;

namespace Framework {

    class InputsSyetm;   //!< (kept to match existing fwd-decl)
    class GameObjectFactory;
    class HitBoxSystem;

    /*!
     * \class  LogicSystem
     * \brief  Central gameplay loop driver for level objects and player/enemy logic.
     * \note   Integrates input, animation ticking, simple collision exports, and level I/O.
     */
    class LogicSystem : public Framework::ISystem {
    public:
        /*! \brief Lightweight snapshot of current sprite animation state. */
        struct AnimationInfo {
            enum class Mode { Idle, Run, Attack1, Attack2, Attack3, Throw, Knockback, Death };

            int  frame{ 0 };
            int  columns{ 1 };
            int  rows{ 1 };
            Mode mode{ Mode::Idle };
            bool running{ false };
        };

        /*! \brief Minimal AABB snapshot for debugging/render integration. */
        struct CollisionInfo {
            bool playerValid{ false };
            bool targetValid{ false };
            AABB player{ 0.f,0.f,0.f,0.f };
            AABB target{ 0.f,0.f,0.f,0.f };
        };

        /*! \brief Construct with a target window and input system. */
        LogicSystem(gfx::Window& window, InputSystem& input);
        /*! \brief Destroy owned subsystems and logging resources. */
        ~LogicSystem() override;

        /*! \brief Set up factory, load level, cache references. */
        void Initialize() override;

        /*! \brief Per-frame gameplay update (input, animation, bookkeeping). */
        void Update(float dt) override;

        /*! \brief Release level/factory resources. */
        void Shutdown() override;

        /*! \brief Reload current level and refresh object references. */
        void ReloadLevel();

        /*! \name Accessors */
        ///@{
        GameObjectFactory* Factory()       const { return factory.get(); }
        const std::vector<GOC*>& LevelObjects()  const { return levelObjects; }
        const AnimationInfo& Animation()     const { return animInfo; }
        const CollisionInfo& Collision()     const { return collisionInfo; }
        bool                       GetPlayerWorldPosition(float& outX, float& outY) const;
        int                        ScreenWidth()   const { return screenW; }
        int                        ScreenHeight()  const { return screenH; }
        GOC* FindAnyAlivePlayer();
        HitBoxSystem* hitBoxSystem = nullptr;
        ///@}

        /*! \brief System name for diagnostics/profiling. */
        std::string GetName() override { return "LogicSystem"; }

        int                                  enemiesAlive{ 0 };
    private:

        std::filesystem::path resolveData(std::string_view name) const;

        // Extended to support combo attacks.
        enum class AnimState { Idle, Run, Attack1, Attack2, Attack3, Throw, Knockback, Death };

        struct AnimConfig {
            int   cols;
            int   rows;
            int   frames;
            float fps;
        };

        const AnimConfig& CurrentConfig() const;
        const AnimConfig& ConfigForState(AnimState state) const;
        bool                 IsAttackState(AnimState state) const;
        void                 SetAnimState(AnimState newState);
        void                 BeginComboAttack();
        void                 BeginThrowAttack();
        void                 ForceAttackState(int comboIndex);
        AnimState            AttackStateForIndex(int comboIndex) const;
        float                AttackDurationForState(AnimState state) const;
        AnimationInfo::Mode  ModeForState(AnimState state) const;
        std::string_view     AnimNameForState(AnimState state) const;
        int                  AnimationIndexForState(const SpriteAnimationComponent* comp, AnimState state) const;
        AnimConfig           ConfigFromSpriteSheet(const SpriteAnimationComponent* comp, AnimState state) const;
        void                 ApplyAnimationStateToComponent(AnimState state);

        bool                 IsAlive(GOC* obj) const;
        void                 RefreshLevelReferences();
        void                 LoadLevelAndResetState(const std::filesystem::path& levelPath);
        void                 UpdateAnimation(float dt, bool wantRun);
        void                 RecountEnemies();

        struct PendingThrow
        {
            bool  active{ false };
            float spawnX{ 0.0f };
            float spawnY{ 0.0f };
            float dirX{ 0.0f };
            float dirY{ 0.0f };
        };

        gfx::Window* window;
        InputSystem& input;

        std::unique_ptr<GameObjectFactory>   factory;
        std::vector<GOC*>                    levelObjects;

        GOC* player{ nullptr };
        GOC* collisionTarget{ nullptr };
        GateController                       gateController;

        AnimState                            animState{ AnimState::Idle };
        AnimConfig                           idleConfig{ 5,1,5,6.f };
        AnimConfig                           runConfig{ 8,1,8,10.f };
        AnimConfig                           attackConfigs[3]{ {13,1,13,12.f}, {8,1,8,12.f}, {9,1,9,12.f} };
        AnimConfig                           throwConfig{ 14,1,14,18.f };
        AnimConfig                           knockbackConfig{ 4,1,4,5.f };
        AnimConfig                           deathConfig{ 8,1,8,8.f };
        int                                  frame{ 0 };
        float                                frameClock{ 0.f };
        float                                attackTimer{ 0.f };
        int                                  comboStep{ 0 };
        float                                knockbackAnimTimer{ 0.0f };

        AnimationInfo                        animInfo{};
        CollisionInfo                        collisionInfo{};

        int                                  screenW{ 800 };
        int                                  screenH{ 600 };

        bool                                 crashTestLatched{ false };
        bool                                 pendingLevelTransition{ false };
        std::unique_ptr<CrashLogger>         crashLogger;

        PendingThrow                         pendingThrow{};
        float                                throwCooldownTimer{ 0.0f };
        bool                                 throwRequestQueued{ false };

        float                                runParticleTimer{ 0.0f };
    };

} // namespace Framework
