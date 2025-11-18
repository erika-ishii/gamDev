/*********************************************************************************************
 \file      PhysicSystem.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Declares a lightweight 2D physics system (AABB moves/collisions + hitboxes).
 \details   Steps rigid bodies with velocity-based integration, resolves simple AABB
            collisions (axis-separated) against scene geometry, and processes enemy
            hitboxes against player AABBs for one-shot damage application. Designed as
            an engine subsystem driven by SystemManager (Initialize → Update(dt) → Shutdown).
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include "Common/System.h"
#include "LogicSystem.h"
#include "Physics/Collision/Collision.h"
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Composition/Composition.h"

namespace Framework {

    class LogicSystem;

    /*************************************************************************************
      \class  PhysicSystem
      \brief  Minimal physics step for 2D games: kinematic update + simple collisions.
      \note   Works with LogicSystem/Factory to iterate objects and query components.
    *************************************************************************************/
    class PhysicSystem : public Framework::ISystem {
    public:
        /*************************************************************************
          \brief  Construct with a reference to the game logic system.
        *************************************************************************/
        explicit PhysicSystem(LogicSystem& logic);

        /*************************************************************************
          \brief  Initialize physics state/resources (no-op by default).
        *************************************************************************/
        void Initialize() override;

        /*************************************************************************
          \brief  Advance physics one frame (integrate + collide + hitboxes).
          \param  dt  Delta time in seconds.
        *************************************************************************/
        void Update(float dt) override;

        /*************************************************************************
          \brief  Release physics resources (no-op by default).
        *************************************************************************/
        void Shutdown() override;

        /*************************************************************************
          \brief  System name for diagnostics/profiling.
        *************************************************************************/
        std::string GetName() override { return "PhysicSystem"; }

    private:
        LogicSystem& logic;  //!< Access to scene objects/components.
    };

} // namespace Framework
