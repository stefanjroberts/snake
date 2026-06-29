
#include "memory.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

#if MEMORY_DEBUG_LEVEL >= 1
#include "logger.h"

struct MemoryStats
{
    u64 totalAllocated;
    u64 totalAllocatedByTag[MEM_TAG_MAX];
};

static struct MemoryStats memory_stats; // TODO: Find a suitable solution for static global management
#endif

#if MEMORY_DEBUG_LEVEL == 2
struct DebugMemPrefix
{
    u64 size;
    enum MemoryTag memtag;
};
#endif

void platform_memory_init()
{
#if MEMORY_DEBUG_LEVEL >= 1

    char *pointer = (char *)&memory_stats;
    for (i32 i = 0; i < sizeof(memory_stats); i++)
    {
        pointer[i] = 0;
    }

#endif

    return;
}

void *platform_alloc(u64 size, enum MemoryTag memory_tag)
{

#if MEMORY_DEBUG_LEVEL >= 1

    if (memory_tag == MEM_TAG_UNKNOWN)
    {
        PWARN("Memory allocated with unknown memory tag");
    }
    if (memory_tag == MEM_TAG_MAX)
    {
        PERROR("Memory allocated with invalid memory tag");
    }
    if (memory_tag == MEM_TAG_TEST)
    {
        PWARN("Memory allocated with test tag");
    }

    memory_stats.totalAllocated += size;
    memory_stats.totalAllocatedByTag[memory_tag] += size;

#endif

#if MEMORY_DEBUG_LEVEL == 2

    struct DebugMemPrefix *memory = (struct DebugMemPrefix *)malloc(size + sizeof(struct DebugMemPrefix));
    memory[0].size = size;
    memory[0].memtag = memory_tag;
    return (void *)(memory + 1);

#else

    void *memory = malloc(size);
    return memory;

#endif
}

void platform_free(void *memory, u64 size, enum MemoryTag memory_tag)
{

#if MEMORY_DEBUG_LEVEL >= 1

    if (memory_tag == MEM_TAG_UNKNOWN)
    {
        PWARN("Memory freed with unknown memory tag");
    }
    if (memory_tag >= MEM_TAG_MAX)
    {
        PERROR("Memory freed with invalid memory tag");
    }
    if (memory_tag == MEM_TAG_TEST)
    {
        PWARN("Memory freed with test tag");
    }

    memory_stats.totalAllocated -= size;
    memory_stats.totalAllocatedByTag[memory_tag] -= size;

#endif

#if MEMORY_DEBUG_LEVEL == 2

    struct DebugMemPrefix memprefix = ((struct DebugMemPrefix *)memory)[-1];
    PASSERT(size == memprefix.size);
    if (memprefix.memtag != memory_tag)
    {
        PERROR("Memory tag mismatch in platform_free");
    }
    free(memory - sizeof(struct DebugMemPrefix));

#else

    free(memory);

#endif
}

char *open_file(const char *filename, u32 *file_size)
{
    FILE *file_pointer;
    file_pointer = fopen(filename, "rb");
    if (file_pointer)
    {
        fseek(file_pointer, 0L, SEEK_END);
        *file_size = ftell(file_pointer);
        fseek(file_pointer, 0L, SEEK_SET);

        char *file_data = (char *)platform_alloc(*file_size, MEM_TAG_FILE_ACCESS);
        fread(file_data, 1, *file_size, file_pointer);
        fclose(file_pointer);

        return (file_data);
    }
    else
    {
        PERROR("Failed to open file");
        return 0;
    }
}

void close_file(char *file, u32 file_size)
{
    platform_free(file, file_size, MEM_TAG_FILE_ACCESS);
}
