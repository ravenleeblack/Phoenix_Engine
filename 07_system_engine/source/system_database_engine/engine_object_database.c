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

bool set_object_mesh_data(uint32_t object_id, const void* vertices, uint32_t vertex_size, uint32_t vertex_count,
                          const uint32_t* indices, uint32_t index_count)
{
    uint32_t vertex_bytes = vertex_size * vertex_count;
    uint32_t index_bytes = sizeof(uint32_t) * index_count;
    uint32_t blob_size = sizeof(uint32_t) * 4u + vertex_bytes + index_bytes;
    uint32_t* blob = malloc(blob_size);
    if (!blob) return false;

    blob[0] = vertex_size;
    blob[1] = vertex_count;
    blob[2] = index_count;
    blob[3] = 0;
    memcpy(blob + 4, vertices, vertex_bytes);
    memcpy((uint8_t*)(blob + 4) + vertex_bytes, indices, index_bytes);
    bool ok = set_object_data(object_id, object_data_mesh_blob, blob, blob_size);
    free(blob);
    return ok;
}

bool get_object_mesh_data(uint32_t object_id, const void** vertices, uint32_t* vertex_size, uint32_t* vertex_count,
                          const uint32_t** indices, uint32_t* index_count)
{
    uint32_t blob_size = 0;
    uint32_t* blob = get_object_data(object_id, object_data_mesh_blob, &blob_size);
    if (!blob || blob_size < sizeof(uint32_t) * 4u) return false;

    if (vertex_size) *vertex_size = blob[0];
    if (vertex_count) *vertex_count = blob[1];
    if (index_count) *index_count = blob[2];
    if (vertices) *vertices = blob + 4;
    if (indices) *indices = (const uint32_t*)((uint8_t*)(blob + 4) + blob[0] * blob[1]);
    return true;
}
