#include "system_globals.h"


typedef struct { uint32_t kind; uint32_t size; void* data; } object_db_entry;

typedef struct {
    uint32_t        object_id;
    uint8_t         state;                         /* slot_empty / slot_used / slot_dead */
    uint8_t         face_type;
    object_db_entry entries[OBJECT_DB_MAX_KINDS];
} object_db_slot;

enum { slot_empty = 0, slot_used = 1, slot_dead = 2 };
enum { first_capacity = 64, max_block_bytes = 64 * 1024 * 1024 };

static object_db_slot* s_slots = NULL;
static uint32_t s_capacity = 0;       /* power of two */
static uint32_t s_used     = 0;
static uint32_t s_dead     = 0;

typedef struct { uint32_t vertex_size, vertex_count, index_count, reserved; } mesh_header;

/* ------------------------------------------------------------------------- */
/* hash table                                                                 */
/* ------------------------------------------------------------------------- */
static uint32_t hash_id(uint32_t id) { return id * 2654435761u; }

static object_db_slot* find_slot(uint32_t id)
{
    if (!s_slots) return NULL;
    const uint32_t mask = s_capacity - 1;
    uint32_t i = hash_id(id) & mask;
    for (uint32_t n = 0; n < s_capacity; n++, i = (i + 1) & mask) {
        object_db_slot* s = &s_slots[i];
        if (s->state == slot_empty) return NULL;
        if (s->state == slot_used && s->object_id == id) return s;
    }
    return NULL;
}

static bool rebuild(uint32_t new_capacity)
{
    object_db_slot* fresh = (object_db_slot*)calloc(new_capacity, sizeof(object_db_slot));
    if (!fresh) return false;

    const uint32_t mask = new_capacity - 1;
    for (uint32_t k = 0; k < s_capacity; k++) {
        if (s_slots[k].state != slot_used) continue;
        uint32_t i = hash_id(s_slots[k].object_id) & mask;
        while (fresh[i].state != slot_empty) i = (i + 1) & mask;
        fresh[i] = s_slots[k];
    }
    free(s_slots);
    s_slots    = fresh;
    s_capacity = new_capacity;
    s_dead     = 0;
    return true;
}

static object_db_slot* get_or_create_slot(uint32_t id)
{
    object_db_slot* found = find_slot(id);
    if (found) return found;

    if (s_capacity == 0) {
        s_slots = (object_db_slot*)calloc(first_capacity, sizeof(object_db_slot));
        if (!s_slots) return NULL;
        s_capacity = first_capacity;
    } else if ((uint64_t)(s_used + s_dead + 1) * 10u > (uint64_t)s_capacity * 7u) {
        uint32_t want = (s_used * 2 + 2 > first_capacity) ? s_used * 2 + 2 : first_capacity;
        uint32_t cap = first_capacity;
        while (cap < want) cap <<= 1;
        if (!rebuild(cap)) return NULL;
    }

    const uint32_t mask = s_capacity - 1;
    uint32_t i = hash_id(id) & mask;
    while (s_slots[i].state == slot_used) i = (i + 1) & mask;

    object_db_slot* s = &s_slots[i];
    if (s->state == slot_dead) s_dead--;
    memset(s, 0, sizeof(*s));
    s->state     = slot_used;
    s->object_id = id;
    s_used++;
    return s;
}

static void release_slot(object_db_slot* s)
{
    for (int i = 0; i < s->face_type; i++) free(s->entries[i].data);
    memset(s, 0, sizeof(*s));
    s->state = slot_dead;
    s_used--;
    s_dead++;
}

/* ------------------------------------------------------------------------- */
/* api                                                                        */
/* ------------------------------------------------------------------------- */
void start_object_database(void)
{
    shutdown_object_database();
}

void shutdown_object_database(void)
{
    for (uint32_t i = 0; i < s_capacity; i++)
        if (s_slots[i].state == slot_used)
            for (int k = 0; k < s_slots[i].face_type; k++) free(s_slots[i].entries[k].data);
    free(s_slots);
    s_slots = NULL;
    s_capacity = s_used = s_dead = 0;
}

bool object_db_set(uint32_t object_id, enum_object_data_kind kind, const void* data, uint32_t size)
{
    if (object_id == 0 || kind == OBJECT_DATA_NONE || size > max_block_bytes) return false;

    object_db_slot* s = get_or_create_slot(object_id);
    if (!s) return false;

    object_db_entry* e = NULL;
    for (int i = 0; i < s->face_type; i++)
        if (s->entries[i].kind == (uint32_t)kind) { e = &s->entries[i]; break; }

    if (!e) {
        if (s->face_type >= OBJECT_DB_MAX_KINDS) {
            if (s->face_type == 0) release_slot(s);
            return false;
        }
        e = &s->entries[s->face_type];
        e->kind = (uint32_t)kind;
        e->size = 0;
        e->data = NULL;
    }

    void* block = calloc(1, size ? size : 1);
    if (!block) { if (s->face_type == 0) release_slot(s); return false; }
    if (data && size) memcpy(block, data, size);

    free(e->data);
    e->data = block;
    e->size = size;
    if (e == &s->entries[s->face_type]) s->face_type++;
    return true;
}

void* object_db_get(uint32_t object_id, enum_object_data_kind kind, uint32_t* out_size)
{
    object_db_slot* s = find_slot(object_id);
    if (!s) return NULL;
    for (int i = 0; i < s->face_type; i++) {
        if (s->entries[i].kind == (uint32_t)kind) {
            if (out_size) *out_size = s->entries[i].size;
            return s->entries[i].data;
        }
    }
    return NULL;
}

bool object_db_remove(uint32_t object_id, enum_object_data_kind kind)
{
    object_db_slot* s = find_slot(object_id);
    if (!s) return false;
    for (int i = 0; i < s->face_type; i++) {
        if (s->entries[i].kind != (uint32_t)kind) continue;
        free(s->entries[i].data);
        for (int k = i; k < s->face_type - 1; k++) s->entries[k] = s->entries[k + 1];
        s->face_type--;
        memset(&s->entries[s->face_type], 0, sizeof(object_db_entry));
        if (s->face_type == 0) release_slot(s);
        return true;
    }
    return false;
}

void object_db_remove_object(uint32_t object_id)
{
    object_db_slot* s = find_slot(object_id);
    if (s) release_slot(s);
}

bool     object_db_has_object(uint32_t object_id) { return find_slot(object_id) != NULL; }
uint32_t object_db_object_count(void)             { return s_used; }

/* ------------------------------------------------------------------------- */
/* mesh blob: [mesh_header][vertices][indices]                                */
/* ------------------------------------------------------------------------- */
bool object_db_set_mesh(uint32_t object_id, const void* vertices, uint32_t vertex_size, uint32_t vertex_count,
                        const uint32_t* indices, uint32_t index_count)
{
    if (!vertices || !indices || vertex_size == 0 || (vertex_size % 4) != 0) return false;

    const uint64_t vbytes = (uint64_t)vertex_size * vertex_count;
    const uint64_t ibytes = (uint64_t)sizeof(uint32_t) * index_count;
    const uint64_t total  = sizeof(mesh_header) + vbytes + ibytes;
    if (total > max_block_bytes) return false;

    uint8_t* buf = (uint8_t*)malloc((size_t)total);
    if (!buf) return false;

    mesh_header h = { vertex_size, vertex_count, index_count, 0 };
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), vertices, (size_t)vbytes);
    memcpy(buf + sizeof(h) + vbytes, indices, (size_t)ibytes);

    bool ok = object_db_set(object_id, OBJECT_DATA_MESH_BLOB, buf, (uint32_t)total);
    free(buf);
    return ok;
}

bool object_db_get_mesh(uint32_t object_id, const void** vertices, uint32_t* vertex_size, uint32_t* vertex_count,
                        const uint32_t** indices, uint32_t* index_count)
{
    uint32_t size = 0;
    const uint8_t* buf = (const uint8_t*)object_db_get(object_id, OBJECT_DATA_MESH_BLOB, &size);
    if (!buf || size < sizeof(mesh_header)) return false;

    mesh_header h;
    memcpy(&h, buf, sizeof(h));
    const uint64_t need = sizeof(h) + (uint64_t)h.vertex_size * h.vertex_count + (uint64_t)sizeof(uint32_t) * h.index_count;
    if (need != size) return false;

    if (vertices)     *vertices     = buf + sizeof(h);
    if (vertex_size)  *vertex_size  = h.vertex_size;
    if (vertex_count) *vertex_count = h.vertex_count;
    if (indices)      *indices      = (const uint32_t*)(const void*)(buf + sizeof(h) + (size_t)h.vertex_size * h.vertex_count);
    if (index_count)  *index_count  = h.index_count;
    return true;
}

/* ------------------------------------------------------------------------- */
/* file: "PHXO" | version | object face_type | { id, kind face_type, { kind, size, bytes } }                   */
/* ------------------------------------------------------------------------- */
#define OBJECT_DB_MAGIC   0x4F584850u      /* "PHXO" */
#define OBJECT_DB_VERSION 1u

bool object_db_save(const char* file_name)
{
    char path[512], tmp[520];
    if (!settings_db_make_path(file_name, path, sizeof(path))) return false;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE* f = fopen(tmp, "wb");
    if (!f) return false;

    uint32_t head[3] = { OBJECT_DB_MAGIC, OBJECT_DB_VERSION, s_used };
    bool ok = fwrite(head, sizeof(head), 1, f) == 1;

    for (uint32_t i = 0; ok && i < s_capacity; i++) {
        const object_db_slot* s = &s_slots[i];
        if (s->state != slot_used) continue;
        uint32_t rec[2] = { s->object_id, s->face_type };
        ok = fwrite(rec, sizeof(rec), 1, f) == 1;
        for (int k = 0; ok && k < s->face_type; k++) {
            uint32_t eh[2] = { s->entries[k].kind, s->entries[k].size };
            ok = fwrite(eh, sizeof(eh), 1, f) == 1;
            if (ok && eh[1]) ok = fwrite(s->entries[k].data, eh[1], 1, f) == 1;
        }
    }
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(tmp); return false; }

    remove(path);
    return rename(tmp, path) == 0;
}

bool object_db_load(const char* file_name)
{
    char path[512];
    if (!settings_db_make_path(file_name, path, sizeof(path))) return false;

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint32_t head[3];
    if (fread(head, sizeof(head), 1, f) != 1 || head[0] != OBJECT_DB_MAGIC || head[1] != OBJECT_DB_VERSION) { fclose(f); return false; }

    bool ok = true;
    for (uint32_t n = 0; ok && n < head[2]; n++) {
        uint32_t rec[2];
        if (fread(rec, sizeof(rec), 1, f) != 1 || rec[1] > OBJECT_DB_MAX_KINDS) { ok = false; break; }
        for (uint32_t k = 0; k < rec[1]; k++) {
            uint32_t eh[2];
            if (fread(eh, sizeof(eh), 1, f) != 1 || eh[1] > max_block_bytes) { ok = false; break; }
            void* buf = calloc(1, eh[1] ? eh[1] : 1);
            if (!buf) { ok = false; break; }
            if (eh[1] && fread(buf, eh[1], 1, f) != 1) { free(buf); ok = false; break; }
            object_db_set(rec[0], (enum_object_data_kind)eh[0], buf, eh[1]);
            free(buf);
        }
    }
    fclose(f);
    return ok;
}