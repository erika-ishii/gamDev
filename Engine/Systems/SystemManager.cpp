/*********************************************************************************************
 \file      SystemManager.cpp
 \par       SofaSpuds
 \author    
 \brief     Frame orchestrator for engine systems with lightweight per-system profiling.
 \details   Manages an ordered list of systems and drives their lifecycle:
			InitializeAll(), per-frame UpdateAll(dt), DrawAll(), and ShutdownAll().
			Each Update/Draw step is timed with std::chrono::high_resolution_clock and
			reported via Debug/Perf::RecordSystemTiming(name, ms). The destructor
			guards teardown by invoking ShutdownAll() if needed.
 \copyright
			All content ©2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/


#include "SystemManager.h"
#include <chrono>
#include "Debug/Perf.h"

namespace Framework
{

    /***************************************************************************************
      \brief  Initialize every registered system in order.
      \note   Assumes each system pointer in `systems` is valid and non-null.
    ***************************************************************************************/
    void SystemManager::IntializeAll()
    {
        for (auto& sys : systems) {
            sys->Initialize();
        }
    }

    /***************************************************************************************
      \brief  Update every registered system and record per-system elapsed time (ms).
      \param  dt  Delta time in seconds for this frame (simulation step).
      \note   Uses high_resolution_clock; timings are forwarded to RecordSystemTiming().
    ***************************************************************************************/
    void SystemManager::UpdateAll(float dt)
    {
        using clock = std::chrono::high_resolution_clock;
        for (auto& sys : systems) {
            const auto start = clock::now();
            sys->Update(dt);
            const double elapsedMs =
                std::chrono::duration<double, std::milli>(clock::now() - start).count();
            Framework::RecordSystemTiming(sys->GetName(), elapsedMs);
        }
    }

    /***************************************************************************************
      \brief  Draw every registered system and record per-system elapsed time (ms).
      \note   Draw timings are also forwarded to RecordSystemTiming() with the system name.
    ***************************************************************************************/
    void SystemManager::DrawAll()
    {
        using clock = std::chrono::high_resolution_clock;
        for (auto& sys : systems) {
            const auto start = clock::now();
            sys->draw();
            const double elapsedMs =
                std::chrono::duration<double, std::milli>(clock::now() - start).count();
            Framework::RecordSystemTiming(sys->GetName(), elapsedMs);
        }
    }

    /***************************************************************************************
      \brief  Shutdown every registered system, then clear the container.
      \note   Safe to call multiple times; subsequent calls will be no-ops after clear().
    ***************************************************************************************/
    void SystemManager::ShutdownAll()
    {
        for (auto& sys : systems) {
            sys->Shutdown();
        }
        systems.clear();
    }

    /***************************************************************************************
      \brief  Ensures systems are shut down if the owner forgets to call ShutdownAll().
    ***************************************************************************************/
    SystemManager::~SystemManager()
    {
        ShutdownAll();
    }

} // namespace Framework