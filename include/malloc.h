#ifndef GUARD_ALLOC_H
#define GUARD_ALLOC_H


#define FREE_AND_SET_NULL(ptr)          \
{                                       \
    Free(ptr);                          \
    ptr = NULL;                         \
}

#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

#ifdef PLATFORM_PC
// The heap lives in the GBA's EWRAM (0x02000000, reserved by the PC HAL), as
// on hardware: code checks heap pointers against the memory map (bg.c
// IsTileMapOutsideWram). Four times larger: structures that hold pointers
// grow on a 64-bit host.
#define HEAP_SIZE (0x1C000 * 4)
#define gHeap ((u8 *)EWRAM_START)
#else
#define HEAP_SIZE 0x1C000
extern u8 gHeap[HEAP_SIZE];
#endif

void *Alloc(u32 size);
void *AllocZeroed(u32 size);
void Free(void *pointer);
void InitHeap(void *heapStart, u32 heapSize);

#endif // GUARD_ALLOC_H
