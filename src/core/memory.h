#pragma once

#include "defines.h"

#define MEMORY_DEBUG_LEVEL 1
// LEVEL 0: Passthrough to malloc and free
// LEVEL 1: Record memory profile with no validation
// LEVEL 3: Validate memory allocation and freeing

enum MemoryTag
{
    MEM_TAG_UNKNOWN,      // Default memory tag, should not be used
    MEM_TAG_INIT,         // Memory allocated during initialisation, shouldn't stick around
    MEM_TAG_RENDERER,     // Memory allocated for use by the renderer
    MEM_TAG_FILE_ACCESS,  // Memory allocated for file reading and writing
    MEM_TAG_EVENT_SYSTEM, // Memory allocated for the event system

    MEM_TAG_TEST, // Special tag for testing purposes
    MEM_TAG_MAX   // Invalid memory tag, use as a delimiter only
};

void platform_memory_init();
void *platform_alloc(u64 size, enum MemoryTag memory_tag);
void platform_free(void *memory, u64 size, enum MemoryTag memory_tag);
char *open_file(const char *filename, u32 *file_size);
void close_file(char *file, u32 file_size);