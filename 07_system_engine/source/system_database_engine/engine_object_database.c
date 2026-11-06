#include "engine_globals.h"

typedef struct {
    uint32_t object_id;
    enum_object_data_kind kind;
    uint32_t size;
    void* data;
} object_database_entry;

object_database_entry object_database_entries[object_db_max_kinds * 4096];
uint32_t object_database_entry_count = 0;

static object_database_entry* find_object_database_entry(uint32_t object_id, enum_object_data_kind kind)
{
    for (uint32_t i = 0; i < object_database_entry_count; i++) {
        if (object_database_entries[i].object_id == object_id && object_database_entries[i].kind == kind)
            return &object_database_entries[i];
    }
    return NULL;
}

void start_object_database(void)
{
    shutdown_object_database();
}

void shutdown_object_database(void)
{
    for (uint32_t i = 0; i < object_database_entry_count; i++)
        free(object_database_entries[i].data);
    memset(object_database_entries, 0, sizeof(object_database_entries));
    object_database_entry_count = 0;
}

bool set_object_data(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size)
{
    object_database_entry* entry = find_object_database_entry(object_id, kind);
    if (!entry) {
        if (object_database_entry_count >= sizeof(object_database_entries) / sizeof(object_database_entries[0]))
            return false;
        entry = &object_database_entries[object_database_entry_count++];
        entry->object_id = object_id;
        entry->kind = kind;
    }
    free(entry->data);
    entry->data = size ? malloc(size) : NULL;
    if (size && !entry->data) return false;
    if (size && data) memcpy(entry->data, data, size);
    entry->size = size;
    return true;
}

void* get_object_data(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size)
{
    object_database_entry* entry = find_object_database_entry(object_id, kind);
    if (!entry) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (out_size) *out_size = entry->size;
    return entry->data;
}

bool remove_object_data(uint32_t object_id, enum_object_data_kind kind)
{
    for (uint32_t i = 0; i < object_database_entry_count; i++) {
        if (object_database_entries[i].object_id == object_id && object_database_entries[i].kind == kind) {
            free(object_database_entries[i].data);
            object_database_entries[i] = object_database_entries[object_database_entry_count - 1];
            object_database_entry_count--;
            return true;
        }
    }
    return false;
}

void remove_object_data_all(uint32_t object_id)
{
    uint32_t i = 0;
    while (i < object_database_entry_count) {
        if (object_database_entries[i].object_id == object_id) {
            free(object_database_entries[i].data);
            object_database_entries[i] = object_database_entries[object_database_entry_count - 1];
            object_database_entry_count--;
        } else {
            i++;
        }

    }
}

bool object_db_set(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size)
{
    return set_object_data(object_id, kind, data, size);
}

void* object_db_get(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size)
{
    return get_object_data(object_id, kind, out_size);
}

bool object_db_remove(uint32_t object_id, enum_object_data_kind kind)
{
    return remove_object_data(object_id, kind);
}

void object_db_remove_object(uint32_t object_id)
{
    remove_object_data_all(object_id);
}
