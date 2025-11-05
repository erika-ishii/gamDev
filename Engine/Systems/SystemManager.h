/*********************************************************************************************
 \file      SystemManager.h
 \par       SofaSpuds
 \author  
 \brief     Declares the frame orchestrator that drives all engine systems.
 \details   Owns an ordered list of ISystem instances and coordinates their lifecycle:
            IntializeAll(), per-frame UpdateAll(dt), DrawAll(), and ShutdownAll().
            Systems are registered via a typed factory (RegisterSystem<T>), stored as
            unique_ptr<ISystem>, and executed in insertion order. Copy/move is disabled
            to avoid accidental duplication of owned subsystems.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include <memory>
#include <vector>
#include <type_traits>
#include "Common/System.h"

namespace Framework {

    /*************************************************************************************
      \class  SystemManager
      \brief  Owns and orchestrates all engine systems in a fixed execution order.
      \note   Non-copyable and non-movable to preserve single ownership semantics.
    *************************************************************************************/
    class SystemManager {
    public:
        //brief Default construct an empty manager.
        SystemManager() = default;

        ///brief Ensures ShutdownAll() on destruction.
        ~SystemManager();



        //brief Disallow copy construction.
        SystemManager(const SystemManager&) = delete;
        //brief Disallow copy assignment.
        SystemManager& operator=(const SystemManager&) = delete;

        //brief Disallow move construction.
        SystemManager(SystemManager&&) = delete;
        //brief Disallow move assignment.
        SystemManager& operator=(SystemManager&&) = delete;

     

        //brief Initialize every registered system in insertion order.
        void IntializeAll();

        //brief Update every registered system with the given delta time (seconds).
        //param dt Simulation timestep in seconds.
        void UpdateAll(float dt);

        /// \brief Draw every registered system in insertion order.
        void DrawAll();

        /// \brief Shutdown every registered system and clear ownership.
        void ShutdownAll();

  

        /**
         * \brief  Create and register a system of type T, forwarding constructor args.
         * \tparam T    A concrete type deriving from ISystem.
         * \tparam Args Argument pack forwarded to T’s constructor.
         * \param  args Constructor arguments for T.
         * \return Pointer to the newly created system owned by the manager.
         * \note   Systems are executed in the order they are registered.
         */
        template<typename T, typename... Args>
        T* RegisterSystem(Args&&... args) {
            static_assert(std::is_base_of_v<ISystem, T>, "System must derive from ISystem");
            auto sys = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = sys.get();
            systems.emplace_back(std::move(sys));
            return ptr;
        }

    private:
        //brief Owned systems executed in insertion order.
        std::vector<std::unique_ptr<ISystem>> systems{};
    };

} // namespace Framework
