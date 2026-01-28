/*********************************************************************************************
 \file      GameObjectPool.cpp
 \par       SofaSpuds
 \author    <your name here> - Primary Author, 100%

 \brief     Implements GameObjectPool, a pooled allocation service for GameObjectComposition (GOC).

 \details   This module provides a shared ObjectAllocatorStorage configured for GOC allocations
            and implements allocation/deallocation helpers:

            - Storage(): Returns a function-local static ObjectAllocatorStorage that owns the
              ObjectAllocator instance for all GOCs.
            - CreateRaw(): Allocates raw memory from the pool and constructs a GOC using
              placement new.
            - Create(): Wraps CreateRaw() in a GameObjectHandle (std::unique_ptr with custom
              deleter) so reclamation is automatic.
            - Destroy(): Explicitly runs the GOC destructor and returns the underlying memory
              back to the pool.
            - GameObjectDeleter: Forwards deletion requests from GameObjectHandle to Destroy().

            Ownership notes:
            - Memory is allocated via the custom allocator (not system new/delete).
            - Destruction must explicitly call ~GOC() before returning memory to the pool,
              because allocator Free() only returns raw storage.

            Configuration notes:
            - Storage() uses:
                objectsPerPage = 128
                maxPages        = 0 (unlimited)
                debugOn         = false

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Memory/GameObjectPool.h"

#include <new>

namespace Framework
{
    /*************************************************************************************
      \brief Get the shared allocator storage used for all GOC allocations.
      \return Reference to the singleton ObjectAllocatorStorage for GOC blocks.
      \details
        - Uses a function-local static to ensure exactly one allocator instance exists.
        - Configured for:
            * objectSize      = sizeof(GOC)
            * objectsPerPage  = 128
            * maxPages        = 0 (unlimited growth)
            * debugOn         = false
        - All Create/Destroy operations route through this storage.
    *************************************************************************************/
    ObjectAllocatorStorage& GameObjectPool::Storage()
    {
        static ObjectAllocatorStorage storage(sizeof(GOC), 128, 0, false);
        return storage;
    }

    /*************************************************************************************
      \brief Allocate and construct a raw GOC using pooled memory.
      \return Raw pointer to a newly constructed GOC.
      \details
        - Requests a raw block from the pool allocator via Storage().Allocate().
        - Constructs the GOC in-place with placement new:
            new (storage) GOC()
        - The returned pointer must eventually be reclaimed via GameObjectPool::Destroy()
          (directly or via GameObjectHandle).
    *************************************************************************************/
    GOC* GameObjectPool::CreateRaw()
    {
        // Allocate from the pool and placement-new the GOC.
        void* storage = Storage().Allocate();
        return new (storage) GOC();
    }

    /*************************************************************************************
      \brief Allocate a GOC and return it as an RAII handle.
      \return GameObjectHandle owning the created GOC.
      \details
        - Creates a raw GOC via CreateRaw().
        - Wraps it in a std::unique_ptr with GameObjectDeleter so that when the handle
          is destroyed/reset, the object is returned to the pool automatically.
    *************************************************************************************/
    GameObjectHandle GameObjectPool::Create()
    {
        return GameObjectHandle(CreateRaw(), GameObjectDeleter{});
    }

    /*************************************************************************************
      \brief Destroy a pooled GOC and return its memory block to the allocator.
      \param object Pointer to the pooled GOC (may be nullptr).
      \details
        - Safe to call with nullptr (no-op).
        - Explicitly calls the GOC destructor:
            object->~GOC()
          because the allocator manages raw bytes and does not invoke destructors.
        - Returns the memory to the pool via Storage().Free(object).
    *************************************************************************************/
    void GameObjectPool::Destroy(GOC* object)
    {
        if (!object)
            return;
        // Run destructor explicitly, then recycle the storage.
        object->~GOC();
        Storage().Free(object);
    }

    /*************************************************************************************
      \brief Custom deleter used by GameObjectHandle to reclaim pooled GOCs.
      \param object Pointer to the pooled GOC to destroy.
      \details
        - Forwards to GameObjectPool::Destroy(object).
        - Ensures std::unique_ptr does not call ::delete on pooled objects.
    *************************************************************************************/
    void GameObjectDeleter::operator()(GOC* object) const
    {
        GameObjectPool::Destroy(object);
    }
}
