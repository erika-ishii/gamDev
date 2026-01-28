/*********************************************************************************************
 \file      ComponentPool.h
 \par       SofaSpuds
 \author    <your name here> - Primary Author, 100%

 \brief     Declares ComponentPool<T>, a templated pooled allocator for GameComponent types.

 \details   ComponentPool<T> provides a per-component-type memory pool backed by
            ObjectAllocatorStorage. It supports:
            - Fast allocation of fixed-size component objects using placement new.
            - Safe reclamation through ComponentHandle / ComponentHandleT<T> with a
              custom ComponentDeleter callback.
            - A shared allocator per component type (one pool per T), created lazily.

            Allocation model:
            - Storage() holds a function-local static ObjectAllocatorStorage configured for T.
            - CreateRaw(...) allocates raw bytes from the pool and constructs T in-place.
            - Create(...) returns a generic ComponentHandle (polymorphic GameComponent*).
            - CreateTyped(...) returns a typed handle ComponentHandleT<T> for convenience.

            Destruction model:
            - ComponentDeleter stores a function pointer to Destroy plus an optional user pointer.
            - Destroy(GameComponent*, void*) calls ~T() explicitly, then returns memory to the pool.
            - This is required because the allocator manages raw storage and does not call destructors.

            Configuration notes:
            - Storage() uses:
                objectsPerPage = 64
                maxPages        = 0 (unlimited growth)
                debugOn         = false
              You can tweak these defaults per performance/memory needs.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <new>
#include <utility>
#include "Memory/ObjectAllocatorStorage.h"
#include "Composition/Component.h"

namespace Framework
{
    /*************************************************************************************
      \brief Templated pooled allocator for component type T.
      \tparam T Concrete component type (must derive from GameComponent).
      \details
        - Provides a per-type pool (one static allocator per T) for fast allocations.
        - Integrates with the engine's ComponentHandle / ComponentDeleter ownership model.
        - Uses placement new for construction and explicit destructor calls for destruction.
    *************************************************************************************/
    template <typename T>
    class ComponentPool
    {
    public:
        /*********************************************************************************
          \brief Allocate raw storage and construct a component instance from the pool.
          \tparam Args Constructor argument pack for T.
          \param args  Arguments forwarded to T's constructor.
          \return Raw pointer to the newly constructed T.
          \details
            - Allocates a fixed-size block from Storage().
            - Constructs T in-place using placement new.
            - The returned pointer must be reclaimed by calling Destroy (directly or via a handle).
        *********************************************************************************/
        template <typename... Args>
        static T* CreateRaw(Args&&... args)
        {
            void* storage = Storage().Allocate();
            return new (storage) T(std::forward<Args>(args)...);
        }

        /*********************************************************************************
          \brief Create a pooled component and return it as a generic ComponentHandle.
          \tparam Args Constructor argument pack for T.
          \param args  Arguments forwarded to T's constructor.
          \return ComponentHandle owning the created component.
          \details
            - Allocates and constructs T via CreateRaw.
            - Wraps it into ComponentHandle with a ComponentDeleter that calls Destroy.
            - On handle destruction/reset, the object is destructed and memory returns to the pool.
        *********************************************************************************/
        template <typename... Args>
        static ComponentHandle Create(Args&&... args)
        {
            T* instance = CreateRaw(std::forward<Args>(args)...);
            return ComponentHandle(instance, ComponentDeleter{ &Destroy, nullptr });
        }

        /*********************************************************************************
          \brief Create a pooled component and return it as a typed ComponentHandleT<T>.
          \tparam Args Constructor argument pack for T.
          \param args  Arguments forwarded to T's constructor.
          \return ComponentHandleT<T> owning the created component.
          \details
            - Same as Create(), but preserves the concrete type in the handle.
            - Useful for component-owned sub-objects or cases where typed access is required.
        *********************************************************************************/
        template <typename... Args>
        static ComponentHandleT<T> CreateTyped(Args&&... args)
        {
            T* instance = CreateRaw(std::forward<Args>(args)...);
            return ComponentHandleT<T>(instance, ComponentDeleter{ &Destroy, nullptr });
        }

        /*********************************************************************************
          \brief Get a ComponentDeleter suitable for objects allocated by this pool.
          \return ComponentDeleter configured to call ComponentPool<T>::Destroy.
          \details
            - Useful when a component is constructed elsewhere but should be reclaimed by this pool.
            - The second field (user pointer) is currently unused and set to nullptr.
        *********************************************************************************/
        static ComponentDeleter Deleter()
        {
            return ComponentDeleter{ &Destroy, nullptr };
        }

    private:
        /*********************************************************************************
          \brief Get the shared allocator storage for component type T.
          \return Reference to a lazily-created ObjectAllocatorStorage.
          \details
            - One storage instance is created per T (template static).
            - Configured for:
                objectSize      = sizeof(T)
                objectsPerPage  = 64
                maxPages        = 0 (unlimited)
                debugOn         = false
        *********************************************************************************/
        static ObjectAllocatorStorage& Storage()
        {
            static ObjectAllocatorStorage storage(sizeof(T), 64, 0, false);
            return storage;
        }

        /*********************************************************************************
          \brief Destroy callback used by ComponentDeleter to reclaim pooled components.
          \param component Base pointer to the component (expected to be a T allocated by this pool).
          \param           User data pointer (unused; reserved for future use).
          \details
            - Safe on nullptr.
            - Calls T's destructor explicitly via static_cast<T*>(component)->~T().
            - Returns the raw storage to the pool allocator via Storage().Free(component).
          \warning
            - Passing a pointer not allocated by this pool (or not actually a T) is undefined behavior.
        *********************************************************************************/
        static void Destroy(GameComponent* component, void*)
        {
            if (!component)
                return;
            static_cast<T*>(component)->~T();
            Storage().Free(component);
        }
    };
}
