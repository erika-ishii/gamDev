/*********************************************************************************************
 \file      ObjectAllocator.cpp
 \par       SofaSpuds
 \author    <your name here> - Primary Author, 100%

 \brief     Implements the ObjectAllocator fixed-size, page-based memory manager.

 \details   This implementation provides a page allocator for same-sized blocks with:
            - A singly-linked free list stored inside freed blocks (GenericObject).
            - A page list where each page stores a next-page pointer at its start.
            - Optional debugging support:
                * Fill patterns for allocated/unallocated/freed memory.
                * Pad bytes around blocks to detect over/under writes.
                * Optional alignment padding between blocks.
            - Optional bypass mode (UseCPPMemManager_) that delegates to operator new/delete.

            Allocation model:
            - Pages are allocated as contiguous byte arrays sized by OAStats_.PageSize_.
            - Blocks are laid out after the page's next pointer and LeftAlignSize_.
            - Each block layout (byte-wise) is:
                [HeaderBlocks_][LeftPad][UserBytes(ObjectSize_)][RightPad][InterAlign(optional)]

            Free model:
            - Free(ptr) validates ptr belongs to a page, is block-aligned, not double-freed,
              and (in debug mode) pad bytes are intact.
            - Freed blocks are pushed back to FreeList_.

            Notes:
            - This allocator returns raw memory. It does not construct or destruct objects.
              Clients should use placement new and explicit destructor calls as needed.
            - Exceptions are reported using OAException with detailed error codes.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
//---------------------------------------------------------------------------
#include "ObjectAllocator.h"

#include <cstddef>
#include <cstring>

namespace
{
    /*************************************************************************************
      \brief Align a value upward to the nearest multiple of alignment.
      \param value     The input size/value to align.
      \param alignment The alignment boundary (0 means no alignment).
      \return The smallest value >= input that is a multiple of alignment.
      \details
        - Used to compute LeftAlignSize_, InterAlignSize_, and per-block stride (BlockSize_).
        - If alignment is 0, the value is returned unchanged.
    *************************************************************************************/
    unsigned AlignUp(unsigned value, unsigned alignment)
    {
        if (alignment == 0)
            return value;
        unsigned remainder = value % alignment;
        if (remainder == 0)
            return value;
        return value + (alignment - remainder);
    }
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Construct an ObjectAllocator instance with a fixed object size and configuration.
  \param ObjectSize Size in bytes of each allocatable object.
  \param config     Configuration controlling pages, padding, headers, debug, alignment, etc.
  \throws OAException
          - E_NO_MEMORY if initial page allocation fails.
          - E_NO_PAGES  if MaxPages is reached during required growth (depending on usage).
  \details
    - Stores the configuration and initializes statistics.
    - Computes the raw block layout:
        header + pad + object + pad
      then optionally aligns:
        - LeftAlignSize_ aligns the first block after the page header (next pointer).
        - InterAlignSize_ aligns the spacing between subsequent blocks.
      and sets BlockSize_ (stride between blocks).
    - Computes OAStats_.PageSize_ as:
        sizeof(void*) + LeftAlignSize_ + (BlockSize_ * ObjectsPerPage_)
    - Allocates the first page to seed the free list (unless UseCPPMemManager_ is true).
*************************************************************************************/
ObjectAllocator::ObjectAllocator(unsigned ObjectSize, const OAConfig& config) noexcept(false)
    : Config_(config)
{
    OAStats_.ObjectSize_ = ObjectSize;

    if (Config_.UseCPPMemManager_)
    {
        return;
    }

    unsigned align = Config_.Alignment_;
    unsigned header = Config_.HeaderBlocks_;
    unsigned pad = Config_.PadBytes_;

    unsigned rawBlockSize = header + pad + ObjectSize + pad;
    if (align > 0)
    {
        Config_.LeftAlignSize_ = AlignUp(static_cast<unsigned>(sizeof(void*)), align) - static_cast<unsigned>(sizeof(void*));
        unsigned alignedBlockSize = AlignUp(rawBlockSize, align);
        Config_.InterAlignSize_ = alignedBlockSize - rawBlockSize;
        BlockSize_ = alignedBlockSize;
    }
    else
    {
        BlockSize_ = rawBlockSize;
    }

    OAStats_.PageSize_ = static_cast<unsigned>(sizeof(void*)) + Config_.LeftAlignSize_ +
        (BlockSize_ * Config_.ObjectsPerPage_);

    AllocateNewPage();
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Destroy the allocator and release all allocated pages.
  \note  Never throws.
  \details
    - If UseCPPMemManager_ is enabled, there is no internal page storage to free.
    - Otherwise, walks PageList_ where each page begins with a next-page pointer.
    - Frees each page as a byte array and clears internal list heads.
*************************************************************************************/
ObjectAllocator::~ObjectAllocator() noexcept
{
    if (Config_.UseCPPMemManager_)
    {
        return;
    }

    void* page = PageList_;
    while (page)
    {
        void* next = *reinterpret_cast<void**>(page);
        delete[] reinterpret_cast<unsigned char*>(page);
        page = next;
    }

    PageList_ = nullptr;
    FreeList_ = nullptr;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Allocate one object-sized block and return a pointer to its user region.
  \return Pointer to the usable object bytes (excludes headers/padding).
  \throws OAException
          - E_NO_MEMORY if system allocation fails (in UseCPPMemManager_ mode).
          - E_NO_PAGES  if MaxPages is reached and no blocks remain.
          - E_NO_MEMORY if allocating a new page fails when growing.
  \details
    - In UseCPPMemManager_ mode, delegates to ::operator new(ObjectSize_).
    - Otherwise:
        1) If FreeList_ is empty, attempt to allocate a new page (subject to MaxPages_).
        2) Pop a block from FreeList_.
        3) Compute user pointer as:
             block + HeaderBlocks_ + PadBytes_
        4) In debug mode, write pad patterns and fill user memory as ALLOCATED_PATTERN.
        5) Update OAStats_ counters (allocations, in-use, free count, peak usage).
*************************************************************************************/
void* ObjectAllocator::Allocate() noexcept(false)
{
    if (Config_.UseCPPMemManager_)
    {
        try
        {
            void* mem = ::operator new(OAStats_.ObjectSize_);
            ++OAStats_.Allocations_;
            ++OAStats_.ObjectsInUse_;
            if (OAStats_.ObjectsInUse_ > OAStats_.MostObjects_)
                OAStats_.MostObjects_ = OAStats_.ObjectsInUse_;
            return mem;
        }
        catch (...)
        {
            throw OAException(OAException::E_NO_MEMORY, "operator new failed");
        }
    }

    if (!FreeList_)
    {
        if (Config_.MaxPages_ != 0 && OAStats_.PagesInUse_ >= Config_.MaxPages_)
            throw OAException(OAException::E_NO_PAGES, "maximum page count reached");
        AllocateNewPage();
    }

    GenericObject* block = FreeList_;
    FreeList_ = FreeList_->Next;

    unsigned char* user = reinterpret_cast<unsigned char*>(block) + Config_.HeaderBlocks_ + Config_.PadBytes_;

    if (Config_.DebugOn_)
    {
        std::memset(user - Config_.PadBytes_, PAD_PATTERN, Config_.PadBytes_);
        std::memset(user + OAStats_.ObjectSize_, PAD_PATTERN, Config_.PadBytes_);
        std::memset(user, ALLOCATED_PATTERN, OAStats_.ObjectSize_);
    }

    ++OAStats_.Allocations_;
    ++OAStats_.ObjectsInUse_;
    --OAStats_.FreeObjects_;
    if (OAStats_.ObjectsInUse_ > OAStats_.MostObjects_)
        OAStats_.MostObjects_ = OAStats_.ObjectsInUse_;

    return user;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Free a previously allocated block back to the allocator.
  \param Object Pointer previously returned by Allocate() (user region pointer).
  \throws OAException
          - E_BAD_ADDRESS    if the pointer does not belong to any allocator page.
          - E_BAD_BOUNDARY   if the pointer is within a page but not block-aligned.
          - E_MULTIPLE_FREE  if the block is already present in the free list.
          - E_CORRUPTED_BLOCK if pad bytes are not intact (debug mode).
  \details
    - In UseCPPMemManager_ mode, delegates to ::operator delete(Object).
    - Otherwise:
        1) Null pointers are ignored.
        2) Convert user pointer back to block start:
             Object - PadBytes_ - HeaderBlocks_
        3) Verify the block lies within some page and on a BlockSize_ boundary.
        4) Detect multiple free via IsOnFreeList(block).
        5) In debug mode, validate pad bytes using PadsAreIntact(Object).
        6) In debug mode, fill user bytes with FREED_PATTERN.
        7) Push the block onto FreeList_ and update statistics.
*************************************************************************************/
void ObjectAllocator::Free(void* Object) noexcept(false)
{
    if (Config_.UseCPPMemManager_)
    {
        ::operator delete(Object);
        ++OAStats_.Deallocations_;
        if (OAStats_.ObjectsInUse_ > 0)
            --OAStats_.ObjectsInUse_;
        return;
    }

    if (!Object)
        return;

    unsigned char* block = reinterpret_cast<unsigned char*>(Object) - Config_.PadBytes_ - Config_.HeaderBlocks_;

    bool onPage = false;
    void* page = PageList_;
    while (page)
    {
        unsigned char* pageStart = reinterpret_cast<unsigned char*>(page);
        unsigned char* firstBlock = FirstBlockOnPage(pageStart);
        unsigned char* pageEnd = pageStart + OAStats_.PageSize_;

        if (block >= firstBlock && block < pageEnd)
        {
            onPage = true;
            unsigned ptrdiff = static_cast<unsigned>(block - firstBlock);
            if (ptrdiff % BlockSize_ != 0)
                throw OAException(OAException::E_BAD_BOUNDARY, "block is not aligned to boundary");

            break;
        }

        page = *reinterpret_cast<void**>(page);
    }

    if (!onPage)
        throw OAException(OAException::E_BAD_ADDRESS, "block is not within any page");

    if (IsOnFreeList(block))
        throw OAException(OAException::E_MULTIPLE_FREE, "block already freed");

    if (Config_.DebugOn_ && !PadsAreIntact(reinterpret_cast<unsigned char*>(Object)))
        throw OAException(OAException::E_CORRUPTED_BLOCK, "pad bytes corrupted");

    if (Config_.DebugOn_)
        std::memset(Object, FREED_PATTERN, OAStats_.ObjectSize_);

    GenericObject* node = reinterpret_cast<GenericObject*>(block);
    node->Next = FreeList_;
    FreeList_ = node;

    ++OAStats_.Deallocations_;
    --OAStats_.ObjectsInUse_;
    ++OAStats_.FreeObjects_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Invoke a callback for each block that is currently considered "in use".
  \param fn Callback function called as fn(userPtr, objectSize).
  \return Number of in-use blocks reported.
  \details
    - Iterates every page and every block on each page.
    - A block is treated as "in use" if it is NOT present on FreeList_.
    - The callback receives the user-region pointer (skipping headers/pads).
*************************************************************************************/
unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const
{
    if (!fn)
        return 0;

    unsigned count = 0;
    void* page = PageList_;
    while (page)
    {
        unsigned char* pageStart = reinterpret_cast<unsigned char*>(page);
        unsigned char* block = FirstBlockOnPage(pageStart);

        for (unsigned i = 0; i < Config_.ObjectsPerPage_; ++i)
        {
            unsigned char* user = block + Config_.HeaderBlocks_ + Config_.PadBytes_;
            if (!IsOnFreeList(block))
            {
                fn(user, OAStats_.ObjectSize_);
                ++count;
            }
            block += BlockSize_;
        }

        page = *reinterpret_cast<void**>(page);
    }

    return count;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Validate pages for pad-byte corruption and report corrupted blocks.
  \param fn Callback function called as fn(userPtr, objectSize) for each corrupted block.
  \return Number of corrupted blocks found.
  \details
    - If debug is off, returns 0 immediately (no pad validation performed).
    - Scans all blocks on all pages and checks PadsAreIntact(userPtr).
    - If pad bytes differ from PAD_PATTERN, the block is reported as corrupted.
*************************************************************************************/
unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const
{
    if (!fn)
        return 0;

    unsigned count = 0;
    if (!Config_.DebugOn_)
        return 0;

    void* page = PageList_;
    while (page)
    {
        unsigned char* pageStart = reinterpret_cast<unsigned char*>(page);
        unsigned char* block = FirstBlockOnPage(pageStart);

        for (unsigned i = 0; i < Config_.ObjectsPerPage_; ++i)
        {
            unsigned char* user = block + Config_.HeaderBlocks_ + Config_.PadBytes_;
            if (!PadsAreIntact(user))
            {
                fn(user, OAStats_.ObjectSize_);
                ++count;
            }
            block += BlockSize_;
        }

        page = *reinterpret_cast<void**>(page);
    }

    return count;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Release pages that are entirely free (contain no allocated blocks).
  \return Number of pages released.
  \details
    - A page is considered empty if every block on the page is currently on FreeList_.
    - For each empty page:
        1) Remove all of its blocks from the global FreeList_.
        2) Unlink the page from PageList_.
        3) delete[] the page memory.
        4) Update statistics (PagesInUse_, FreeObjects_).
    - If UseCPPMemManager_ is enabled, no pages exist and 0 is returned.
*************************************************************************************/
unsigned ObjectAllocator::FreeEmptyPages(void)
{
    if (Config_.UseCPPMemManager_)
        return 0;

    unsigned released = 0;
    void* previous = nullptr;
    void* page = PageList_;
    while (page)
    {
        unsigned char* pageStart = reinterpret_cast<unsigned char*>(page);
        unsigned char* block = FirstBlockOnPage(pageStart);
        bool allFree = true;
        for (unsigned i = 0; i < Config_.ObjectsPerPage_; ++i)
        {
            if (!IsOnFreeList(block))
            {
                allFree = false;
                break;
            }
            block += BlockSize_;
        }

        void* next = *reinterpret_cast<void**>(page);

        if (allFree)
        {
            unsigned char* pageBytes = reinterpret_cast<unsigned char*>(page);
            unsigned char* blockStart = FirstBlockOnPage(pageBytes);
            for (unsigned i = 0; i < Config_.ObjectsPerPage_; ++i)
            {
                GenericObject* node = reinterpret_cast<GenericObject*>(blockStart);
                GenericObject* prevFree = nullptr;
                GenericObject* current = FreeList_;
                while (current)
                {
                    if (current == node)
                    {
                        if (prevFree)
                            prevFree->Next = current->Next;
                        else
                            FreeList_ = current->Next;
                        break;
                    }
                    prevFree = current;
                    current = current->Next;
                }
                blockStart += BlockSize_;
            }

            if (previous)
                *reinterpret_cast<void**>(previous) = next;
            else
                PageList_ = next;

            delete[] pageBytes;
            ++released;
            --OAStats_.PagesInUse_;
            OAStats_.FreeObjects_ -= Config_.ObjectsPerPage_;
        }
        else
        {
            previous = page;
        }

        page = next;
    }

    return released;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Indicates whether extra-credit features are implemented.
  \return true if extra-credit requirements are met.
  \details
    - In this implementation, returns true (FreeEmptyPages and alignment logic exist).
*************************************************************************************/
bool ObjectAllocator::ImplementedExtraCredit(void)
{
    return true;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Enable or disable debug behavior at runtime.
  \param State True to enable debug fills/validation, false to disable.
*************************************************************************************/
void ObjectAllocator::SetDebugState(bool State)
{
    Config_.DebugOn_ = State;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Get the head pointer of the allocator's internal free list.
  \return Opaque pointer to FreeList_.
  \note  Intended for testing/debugging only.
*************************************************************************************/
const void* ObjectAllocator::GetFreeList(void) const
{
    return FreeList_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Get the head pointer of the allocator's internal page list.
  \return Opaque pointer to PageList_.
  \note  Intended for testing/debugging only.
*************************************************************************************/
const void* ObjectAllocator::GetPageList(void) const
{
    return PageList_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Get a copy of the current configuration.
  \return OAConfig by value.
*************************************************************************************/
OAConfig ObjectAllocator::GetConfig(void) const
{
    return Config_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Get a copy of the current allocator statistics.
  \return OAStats by value.
*************************************************************************************/
OAStats ObjectAllocator::GetStats(void) const
{
    return OAStats_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Allocate one new page, link it into PageList_, and push all blocks onto FreeList_.
  \throws OAException
          - E_NO_MEMORY if allocation of the page byte array fails.
  \details
    - Allocates a raw byte buffer of OAStats_.PageSize_.
    - Stores the previous PageList_ pointer in the first sizeof(void*) bytes.
    - Computes the first block location using FirstBlockOnPage().
    - For each block:
        - Pushes the block onto FreeList_ (LIFO).
        - In debug mode:
            * Fills user bytes with UNALLOCATED_PATTERN.
            * Fills pad bytes with PAD_PATTERN.
            * Fills alignment padding bytes with ALIGN_PATTERN (if configured).
    - Updates PagesInUse_ and FreeObjects_ statistics.
*************************************************************************************/
void ObjectAllocator::AllocateNewPage(void)
{
    unsigned char* page = nullptr;
    try
    {
        page = new unsigned char[OAStats_.PageSize_];
    }
    catch (...)
    {
        throw OAException(OAException::E_NO_MEMORY, "failed to allocate page");
    }

    std::memset(page, 0, OAStats_.PageSize_);

    *reinterpret_cast<void**>(page) = PageList_;
    PageList_ = page;

    unsigned char* block = FirstBlockOnPage(page);

    if (Config_.DebugOn_)
    {
        unsigned char* alignStart = page + sizeof(void*);
        if (Config_.LeftAlignSize_ > 0)
            std::memset(alignStart, ALIGN_PATTERN, Config_.LeftAlignSize_);
    }

    for (unsigned i = 0; i < Config_.ObjectsPerPage_; ++i)
    {
        GenericObject* node = reinterpret_cast<GenericObject*>(block);
        node->Next = FreeList_;
        FreeList_ = node;

        unsigned char* user = block + Config_.HeaderBlocks_ + Config_.PadBytes_;
        if (Config_.DebugOn_)
        {
            std::memset(user, UNALLOCATED_PATTERN, OAStats_.ObjectSize_);
            std::memset(user - Config_.PadBytes_, PAD_PATTERN, Config_.PadBytes_);
            std::memset(user + OAStats_.ObjectSize_, PAD_PATTERN, Config_.PadBytes_);
            if (Config_.InterAlignSize_ > 0)
            {
                unsigned char* inter = block + (Config_.HeaderBlocks_ + Config_.PadBytes_ +
                    OAStats_.ObjectSize_ + Config_.PadBytes_);
                std::memset(inter, ALIGN_PATTERN, Config_.InterAlignSize_);
            }
        }

        block += BlockSize_;
    }

    ++OAStats_.PagesInUse_;
    OAStats_.FreeObjects_ += Config_.ObjectsPerPage_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Compute the pointer to the first block within a page.
  \param page Pointer to the beginning of the page allocation.
  \return Pointer to the first block start (block header region).
  \details
    - Skips the page header (sizeof(void*) bytes) where the next-page pointer is stored.
    - Skips LeftAlignSize_ bytes used to align the first block as requested.
*************************************************************************************/
unsigned char* ObjectAllocator::FirstBlockOnPage(unsigned char* page) const
{
    return page + sizeof(void*) + Config_.LeftAlignSize_;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Check whether a block pointer appears in the current free list.
  \param object Pointer to a block start (not the user pointer).
  \return true if the pointer matches a node in FreeList_, false otherwise.
  \details
    - Linear scan over FreeList_.
    - Used to detect double frees and determine "in use" status during scans.
*************************************************************************************/
bool ObjectAllocator::IsOnFreeList(const void* object) const
{
    const GenericObject* current = FreeList_;
    while (current)
    {
        if (current == object)
            return true;
        current = current->Next;
    }
    return false;
}

//---------------------------------------------------------------------------
/*************************************************************************************
  \brief Validate left and right pad bytes around a user pointer.
  \param object Pointer to the user region (the same pointer returned by Allocate()).
  \return true if all pad bytes match PAD_PATTERN (or PadBytes_ == 0), false otherwise.
  \details
    - Checks the left pad region immediately preceding the user bytes.
    - Checks the right pad region immediately following the user bytes.
*************************************************************************************/
bool ObjectAllocator::PadsAreIntact(const unsigned char* object) const
{
    const unsigned char* leftPad = object - Config_.PadBytes_;
    for (unsigned i = 0; i < Config_.PadBytes_; ++i)
    {
        if (leftPad[i] != PAD_PATTERN)
            return false;
    }

    const unsigned char* rightPad = object + OAStats_.ObjectSize_;
    for (unsigned i = 0; i < Config_.PadBytes_; ++i)
    {
        if (rightPad[i] != PAD_PATTERN)
            return false;
    }
    return true;
}
