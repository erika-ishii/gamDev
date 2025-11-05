/*********************************************************************************************
 \file      LogicSystem.h
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Author, 10%
 \brief     Game logic coordinator: level load/refresh, input-driven updates, animation.
 \details   Owns the GameObjectFactory and level objects, advances player/enemy state each
            frame, updates sprite animation, exposes simple collision snapshots, and
            bridges to HitBoxSystem.
 \copyright
            All content �2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/


#pragma once

#include "Factory/Factory.h"
#include "Composition/PrefabManager.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/PlayerComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/PlayerHealthComponent.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"
#include "Physics/Dynamics/RigidBodyComponent.h"
#include <Serialization/JsonSerialization.h>
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
            int  frame{ 0 };
            int  columns{ 1 };
            int  rows{ 1 };
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
        GameObjectFactory* Factory() const { return factory.get(); }
        const std::vector<GOC*>& LevelObjects() const { return levelObjects; }
        const AnimationInfo& Animation() const { return animInfo; }
        const CollisionInfo& Collision() const { return collisionInfo; }
        float                           PlayerBaseWidth()  const { return rectBaseW; }
        float                           PlayerBaseHeight() const { return rectBaseH; }
        float                           PlayerScale()      const { return rectScale; }
        bool                            GetPlayerWorldPosition(float& outX, float& outY) const;
        int                             ScreenWidth()  const { return screenW; }
        int                             ScreenHeight() const { return screenH; }
        GOC* FindAnyAlivePlayer();
        HitBoxSystem* hitBoxSystem = nullptr;
        ///@}

        /*! \brief System name for diagnostics/profiling. */
        std::string GetName() override { return "LogicSystem"; }

    private:
        enum class AnimState { Idle, Run };
        struct AnimConfig { int cols; int rows; int frames; float fps; };

        const AnimConfig& CurrentConfig() const;
        bool  IsAlive(GOC* obj) const;
        void  CachePlayerSize();
        void  RefreshLevelReferences();
        void  UpdateAnimation(float dt, bool wantRun);

        gfx::Window* window;
        InputSystem& input;

        std::unique_ptr<GameObjectFactory> factory;
        std::vector<GOC*>                levelObjects;

        GOC* player{ nullptr };
        GOC* collisionTarget{ nullptr };

        float                            rectScale{ 1.f };
        float                            rectBaseW{ 0.5f };
        float                            rectBaseH{ 0.5f };

        AnimState                        animState{ AnimState::Idle };
        AnimConfig                       idleConfig{ 5,1,5,6.f };
        AnimConfig                       runConfig{ 8,1,8,10.f };
        int                              frame{ 0 };
        float                            frameClock{ 0.f };
        AnimationInfo                    animInfo{};

        CollisionInfo                    collisionInfo{};

        int                              screenW{ 800 };
        int                              screenH{ 600 };

        bool                             captured{ false };
        bool                             crashTestLatched{ false };
        std::unique_ptr<CrashLogger>     crashLogger;
    };

} // namespace Framework