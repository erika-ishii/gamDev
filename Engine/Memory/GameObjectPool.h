/*********************************************************************************************
 \file      GameObjectPool.h
 \par       SofaSpuds
 \author    <your name here> - Primary Author, 100%

 \brief     Declares GameObjectPool, a pooled allocation utility for GameObjectComposition (GOC)
            instances using the custom ObjectAllocator system.

 \details   GameObjectPool centralizes allocation and reclamation of GOC objects by:
            - Owning (or exposing) a shared ObjectAllocatorStorage configured for GOC size.
            - Providing CreateRaw() for callers that need a raw pointer without RAII handling.
            - Providing Create() which returns a std::unique_ptr with a custom deleter so
              pooled objects automatically return to the pool when the handle is destroyed.
            - Providing Destroy() to explicitly return a raw GOC pointer back to the pool.

            Ownership model:
            - GameObjectHandle is a unique_ptr<GOC, GameObjectDeleter>.
            - The custom deleter routes deletion to GameObjectPool::Destroy() instead of
              calling delete, ensuring memory returns to the allocator.
            - This pool typically serves as the backing allocator for the Factory, so most
              live objects are owned by systems as handles rather than raw pointers.

            Notes:
            - Pool allocation generally requires placement new and explicit destructor calls
              in the implementation (not shown in this header).
            - Storage() returns a shared allocator so all GOCs are allocated uniformly
              (helpful for performance and avoiding fragmentation).

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <memory>
#include "Composition/Composition.h"
#include "Memory/ObjectAllocatorStorage.h"

namespace Framework
{
    /*************************************************************************************
      \brief Custom deleter used by GameObjectHandle to return objects to the pool.
      \details
        - Replaces the default delete behavior of std::unique_ptr.
        - The implementation should call GameObjectPool::Destroy(object) so the object's
          destructor runs (if needed) and the memory is returned to the allocator.
    *************************************************************************************/
    struct GameObjectDeleter {
        /*********************************************************************************
          \brief Delete operation for pooled GOC instances.
          \param object Pointer to the GOC to reclaim.
          \details
            - Must be safe on nullptr.
            - Should never call ::delete directly; pooled objects must go through the pool.
        *********************************************************************************/
        void operator()(GOC* object) const;
    };

    /*************************************************************************************
      \brief RAII handle for pooled GOC instances.
      \details
        - When the handle goes out of scope or is reset, GameObjectDeleter is invoked.
        - This ensures pooled objects are reclaimed correctly without manual Free() calls.
    *************************************************************************************/
    using GameObjectHandle = std::unique_ptr<GOC, GameObjectDeleter>;

    /*************************************************************************************
      \brief Pool allocator interface for GameObjectComposition (GOC) instances.
      \details
        - Provides a shared ObjectAllocatorStorage configured for allocating GOCs.
        - Supports both raw allocation (CreateRaw) and RAII allocation (Create).
        - Provides an explicit Destroy for systems that temporarily hold raw pointers.
      \note
        - This class is purely static; it is meant to behave like a global pool service.
    *************************************************************************************/
    class GameObjectPool
    {
    public:
        /*********************************************************************************
          \brief Get the shared allocator storage used for all GOC allocations.
          \return Reference to the ObjectAllocatorStorage managing GOC blocks.
          \details
            - Typically implemented with a function-local static storage to ensure a single
              allocator instance across the program.
        *********************************************************************************/
        static ObjectAllocatorStorage& Storage();

        /*********************************************************************************
          \brief Allocate a raw GOC without registering it in the factory.
          \return Raw pointer to a newly allocated (and typically constructed) GOC.
          \details
            - Intended for callers that need a temporary object or will wrap it into a
              handle later.
            - Caller is responsible for eventually returning the object to the pool
              (either via Destroy() or by wrapping in GameObjectHandle).
        *********************************************************************************/
        static GOC* CreateRaw();

        /*********************************************************************************
          \brief Allocate a pooled GOC wrapped in an RAII handle.
          \return GameObjectHandle that owns the GOC and returns it to the pool on destruction.
          \details
            - Preferred API for safety: prevents leaks and guarantees reclamation.
            - The custom deleter routes cleanup to GameObjectPool::Destroy().
        *********************************************************************************/
        static GameObjectHandle Create();

        /*********************************************************************************
          \brief Destroy a pooled GOC and return its memory to the allocator.
          \param object Raw pointer to the object to destroy (may be nullptr).
          \details
            - Should call the GOC destructor and release internal component resources.
            - Must return the underlying memory block to the shared allocator storage.
        *********************************************************************************/
        static void Destroy(GOC* object);
    };
}
