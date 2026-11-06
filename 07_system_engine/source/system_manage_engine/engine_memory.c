#include "engine_globals.h"


struct memory_stats 
{
    unsigned long long total_allocated;
    unsigned long long tagged_allocations[MEMORY_TAG_MAX_TAGS];
};

static const char* memory_tag_strings[MEMORY_TAG_MAX_TAGS] = {
    "UNKNOWN    ",
    "ARRAY      ",
    "DARRAY     ",
    "DICT       ",
    "RING_QUEUE ",
    "BST        ",
    "STRING     ",
    "APPLICATION",
    "JOB        ",
    "TEXTURE    ",
    "MAT_INST   ",
    "RENDERER   ",
    "GAME       ",
    "TRANSFORM  ",
    "ENTITY     ",
    "ENTITY_NODE",
    "SCENE      "
};

static struct memory_stats stats;

// FIXED: Clean inline helper to resolve the missing linker reference directly
char* string_duplicate(const char* str)
{
    if (!str) {
        return NULL;
    }
    
    size_t length = strlen(str) + 1;
    char* duplicate = (char*)malloc(length);
    
    if (duplicate) {
        memcpy(duplicate, str, length);
    }
    
    return duplicate;
}

void initialize_memory() 
{
    memset(&stats, 0, sizeof(stats));
}

void shutdown_memory() {
}

void* kallocate(unsigned long long size, memory_tag tag) 
{
    if (tag == MEMORY_TAG_UNKNOWN) {
        phoenix_warn("RENDERER", "kallocate called using MEMORY_TAG_UNKNOWN. Re-class this allocation.");
    }

    stats.total_allocated += size;
    stats.tagged_allocations[tag] += size;

    void* block = malloc(size);
    if (block) {
        memset(block, 0, size);
    }
    return block;
}

void kfree(void* block, unsigned long long size, memory_tag tag) 
{
    if (!block) {
        return;
    }

    if (tag == MEMORY_TAG_UNKNOWN) {
        phoenix_warn("RENDERER", "kfree called using MEMORY_TAG_UNKNOWN. Re-class this allocation.");
    }

    if (stats.total_allocated >= size) stats.total_allocated -= size;
    if (stats.tagged_allocations[tag] >= size) stats.tagged_allocations[tag] -= size;

    free(block);
}

void* kzero_memory(void* block, unsigned long long size) 
{
    return memset(block, 0, size);
}

void* kcopy_memory(void* dest, const void* source, unsigned long long size) {
    return memcpy(dest, source, size);
}

void* kset_memory(void* dest, int value, unsigned long long size) {
    return memset(dest, value, size);
}

char* get_memory_usage_str() 
{
    const double gib = 1024.0 * 1024.0 * 1024.0;
    const double mib = 1024.0 * 1024.0;
    const double kib = 1024.0;

    char buffer[8000] = "System memory use (tagged):\n";
    size_t offset = strlen(buffer); 

    for (int i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) 
    {
        char unit[4] = "XiB";
        double amount = 1.0;
        unsigned long long allocated = stats.tagged_allocations[i];

        if (allocated >= gib) {
            unit[0] = 'G';
            amount = (double)allocated / gib;
        } else if (allocated >= mib) {
            unit[0] = 'M';
            amount = (double)allocated / mib;
        } else if (allocated >= kib) {
            unit[0] = 'K';
            amount = (double)allocated / kib;
        } else {
            unit[0] = 'B';
            unit[1] = 0;
            amount = (double)allocated;
        }

        if (offset >= 8000) {
            break; 
        }

        int length = snprintf(buffer + offset, 8000 - offset, "  %s: %.2f%s\n", memory_tag_strings[i], amount, unit);
        
        if (length > 0) {
            offset += length;
        }
    }
    
    char* out_string = string_duplicate(buffer);
    return out_string;
}
