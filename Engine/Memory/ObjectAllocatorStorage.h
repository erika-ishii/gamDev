/*********************************************************************************************
 \file      ObjectAllocatorStorage.h
 \par       SofaSpuds
 \author    <your name here> - Primary Author, 100%

 \brief     Declares ObjectAllocatorStorage, a lightweight wrapper that owns an
            ObjectAllocator instance and exposes a simple allocate/free interface.

 \details   ObjectAllocatorStorage is a convenience owner for ObjectAllocator that:
            - Constructs and stores an ObjectAllocator with a common default configuration.
            - Provides accessors to the underlying allocator for stats/debugging.
            - Exposes Allocate()/Free() forwarding functions for callers that only need
              raw memory blocks.

            Design notes:
            - This wrapper stores the allocator by value, so its lifetime is tied to
              the storage object (RAII).
            - Alignment defaults to alignof(std::max_align_t) to support allocating most
              standard types safely (unless a type requires stricter alignment).
            - Debug mode can be enabled at construction to activate patterns/pad checks.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <cstddef>
#include "Memory/ObjectAllocator.h"

namespace Framework
{
    /*************************************************************************************
      \brief Small helper that owns an ObjectAllocator and provides a simplified API.
      \details
        - Holds an ObjectAllocator by value (RAII ownership).
        - Intended for systems that want allocator lifetime management without needing to
          manually allocate/free pages or manage global/static allocators.
        - Provides both direct allocator access (Allocator()) and convenience forwarding
          methods (Allocate/Free).
    *************************************************************************************/
    class ObjectAllocatorStorage
    {
    public:
        /*********************************************************************************
          \brief Construct storage with an ObjectAllocator configured for a fixed object size.
          \param objectSize      Size in bytes for each allocatable block.
          \param objectsPerPage  Number of blocks per page (defaults to DEFAULT_OBJECTS_PER_PAGE).
          \param maxPages        Maximum pages to allocate (defaults to DEFAULT_MAX_PAGES, 0=unlimited).
          \param debugOn         Enables allocator debug features (patterns/pad checks).
          \details
            - Uses OAConfig with:
                UseCPPMemManager = false (allocator active)
                PadBytes         = 0
                HeaderBlocks     = 0
                Alignment        = alignof(std::max_align_t)
            - alignof(std::max_align_t) is a safe default for general-purpose allocations.
        *********************************************************************************/
        explicit ObjectAllocatorStorage(unsigned objectSize,
            unsigned objectsPerPage = DEFAULT_OBJECTS_PER_PAGE,
            unsigned maxPages = DEFAULT_MAX_PAGES,
            bool debugOn = false)
            : allocator_(objectSize, OAConfig(false, objectsPerPage, maxPages, debugOn, 0, 0, alignof(std::max_align_t)))
        {
        }

        /*********************************************************************************
          \brief Get a mutable reference to the underlying ObjectAllocator.
          \return Reference to the owned allocator instance.
          \note  Useful for querying stats, enabling/disabling debug state, etc.
        *********************************************************************************/
        ObjectAllocator& Allocator() { return allocator_; }

        /*********************************************************************************
          \brief Get a const reference to the underlying ObjectAllocator.
          \return Const reference to the owned allocator instance.
        *********************************************************************************/
        const ObjectAllocator& Allocator() const { return allocator_; }

        /*********************************************************************************
          \brief Allocate one block from the underlying allocator.
          \return Pointer to a raw memory block sized to objectSize (user region).
          \throws OAException on allocation failure (e.g., out of pages/memory).
        *********************************************************************************/
        void* Allocate() { return allocator_.Allocate(); }

        /*********************************************************************************
          \brief Return a block to the underlying allocator.
          \param ptr Pointer previously returned by Allocate().
          \throws OAException on invalid frees (bad address/boundary/double free/corruption).
        *********************************************************************************/
        void Free(void* ptr) { allocator_.Free(ptr); }

    private:
        /*********************************************************************************
          \brief Owned allocator instance (RAII).
          \details
            - Stored by value so its destructor releases pages automatically when
              ObjectAllocatorStorage goes out of scope.
        *********************************************************************************/
        ObjectAllocator allocator_;
    };
}
