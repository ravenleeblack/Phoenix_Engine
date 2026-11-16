#include "system_globals.h"
#include "system_skin_gpu.h"
#include "system_shader_profile.h"
#include "system_textureshop_skins.h"
#include <time.h>

/* Texture Shop skin library: the model behind the Skin Maker window (draw_system_textureshop_skinmaker.c).
   A skin is what an object's skin_ref points at. This file owns the list of skins, edits, assign / clear, the GPU copy
   (system_skin_gpu.c) and the .sprof files: Export writes assets/skins/<name>.sprof, start loads every file in that folder.
   The skin stays in the editable texshop form in memory and is converted to / from shader_profile only at the file boundary. */

#define TEXSHOP_SKIN_FOLDER FILE_ARCHIVE_ROOT_FOLDER "\\skins"
#define TEXSHOP_SKIN_EXT    ".sprof"

texshop_state g_texshop;

static shader_profile s_profile;       /* scratch for export / import. about 25 KB, kept off the stack */

const char* const k_texshop_output_name[texshop_out_count]   = { "Color", "Normal", "Rough", "Metal", "Height" };
const char        k_texshop_output_letter[texshop_out_count] = { 'C', 'N', 'R', 'M', 'H' };
const char* const k_texshop_blend_name[texshop_blend_count]  = { "Lerp", "Multiply", "Overlay", "Add" };
const char* const k_texshop_mask_name[texshop_mask_count]    = { "Texture", "Paint", "Height", "Slope", "Curvature" };

/* texshop mask -> profile_mask_type (same table as phoenix_engine.md 23.6) */
static const uint8_t k_mask_to_profile[texshop_mask_count] = {
    PROFILE_MASK_TEXTURE_CHANNEL, PROFILE_MASK_PAINT_MAP, PROFILE_MASK_HEIGHT_RANGE, PROFILE_MASK_SLOPE, PROFILE_MASK_CURVATURE
};

void set_texshop_status(const char* text) { snprintf(g_texshop.status, sizeof(g_texshop.status), "%s", text); }

/* ------------------------------------------------------------------------- */
/* hashing: everything that is exported. a changed hash = GPU copy rebuild + the * mark in the list. */
/* ------------------------------------------------------------------------- */
static uint32_t hash_texshop_u32(uint32_t h, uint32_t vert_to_edge_array)
{
    for (int b = 0; b < 4; b++) { h ^= (vert_to_edge_array >> (b * 8)) & 0xFFu; h *= 16777619u; }
    return h;
}

static uint32_t hash_texshop_float(uint32_t h, float vert_to_edge_array)
{
    uint32_t bits;
    memcpy(&bits, &vert_to_edge_array, sizeof(bits));
    return hash_texshop_u32(h, bits);
}

/* names and the obj_vert_selected layer are not in it, so renaming or selecting never rebuilds anything.
   masks are in it (they are exported) even though the shader does not read them yet: a rebuild on a mask edit is cheap. */
static uint32_t hash_texshop_skin(const texshop_skin* s)
{
    uint32_t h = 2166136261u;
    for (const char* c = s->map_file; *c; c++) h = hash_texshop_u32(h, (uint32_t)(unsigned char)*c);   /* a new map rebuilds the GPU copy */
    h = hash_texshop_u32(h, (uint32_t)s->layer_count);
    for (int i = 0; i < s->layer_count; i++) {
        const texshop_layer* l = &s->layers[i];
        for (int k = 0; k < 4; k++) h = hash_texshop_float(h, l->color[k]);
        h = hash_texshop_float(h, l->opacity);
        h = hash_texshop_u32(h, l->channel_mask);
        h = hash_texshop_u32(h, (uint32_t)l->blend);
        h = hash_texshop_u32(h, l->triplanar ? 1u : 0u);
        h = hash_texshop_u32(h, (uint32_t)l->mask_count);
        for (int mesh = 0; mesh < l->mask_count; mesh++) h = hash_texshop_u32(h, (uint32_t)l->masks[mesh]);
    }
    return h;
}

bool texshop_skin_is_dirty(const texshop_skin* s) { return hash_texshop_skin(s) != s->saved_hash; }

/* ------------------------------------------------------------------------- */
/* names and files                                                           */
/* ------------------------------------------------------------------------- */
static bool skin_name_taken(const char* name)
{
    for (int i = 0; i < g_texshop.skin_count; i++)
        if (_stricmp(g_texshop.skins[i].name, name) == 0) return true;      /* Windows file names ignore case */
    return false;
}

/* "base", "base 2", "base 3" ... so two skins never write the same file */
static void make_unique_skin_name(char* out, const char* base)
{
    char root[texshop_name_len];
    snprintf(root, sizeof(root), "%.23s", base);
    snprintf(out, texshop_name_len, "%s", root);
    for (int n = 2; skin_name_taken(out) && n < 1000; n++) {
        char suffix[8];
        const int sl = snprintf(suffix, sizeof(suffix), " %d", n);
        snprintf(out, texshop_name_len, "%.*s%s", 23 - sl, root, suffix);
    }
}

/* full path of a skin's file. a full path matters: the recycle bin delete below permanently deletes relative ones. */
static void skin_file_path(const char* name, char* out, size_t cap)
{
    char file[texshop_name_len + 1];
    snprintf(file, sizeof(file), "%s", name);
    for (char* p = file; *p; p++)
        if (strchr("\\/:*?\"<>|", *p)) *p = '_';

    char rel[260];
    snprintf(rel, sizeof(rel), TEXSHOP_SKIN_FOLDER "\\%s" TEXSHOP_SKIN_EXT, file);
    const DWORD n = GetFullPathNameA(rel, (DWORD)cap, out, NULL);
    if (n == 0 || n >= (DWORD)cap) snprintf(out, cap, "%s", rel);
}

static void recycle_skin_file(const char* name)
{
    char path[260];
    skin_file_path(name, path, sizeof(path));
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;

    char from[262];                                                 /* double-null terminated list */
    memset(from, 0, sizeof(from));
    snprintf(from, sizeof(from) - 1, "%s", path);

    SHFILEOPSTRUCTA op;
    memset(&op, 0, sizeof(op));
    op.wFunc  = FO_DELETE;
    op.pFrom  = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;   /* recycle bin, like the Files window */
    SHFileOperationA(&op);
}

/* ------------------------------------------------------------------------- */
/* texshop_skin <-> shader_profile                                           */
/* ------------------------------------------------------------------------- */
static void reset_texshop_layer(texshop_layer* l, const char* name, uint32_t channels)
{
    memset(l, 0, sizeof(*l));
    snprintf(l->name, sizeof(l->name), "%.23s", name);
    l->channel_mask = channels;
    l->blend = texshop_blend_lerp;
    l->color[0] = l->color[1] = l->color[2] = 0.55f;   /* the empty-skin grey */
    l->color[3] = 1.0f;
    l->opacity = 1.0f;
}

static void fill_texshop_skin(texshop_skin* s, const char* name)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%.23s", name);
    reset_texshop_layer(&s->layers[0], "base",
                        texshop_bit(texshop_out_color) | texshop_bit(texshop_out_normal) | texshop_bit(texshop_out_rough));
    s->layer_count = 1;
}

/* what the file holds today: colour (in color_tint), opacity, which outputs, blend, projection, masks. Texture sources, mask
   ranges and node graphs are not in the Skin Maker yet, so the file carries constants for those. */
static void skin_to_profile(const texshop_skin* s, shader_profile* p)
{
    shader_profile_defaults(p, PROFILE_KIND_SKIN);
    p->id = s->id;
    snprintf(p->name, sizeof(p->name), "%s", s->name);

    skin_profile_data* d = &p->u.skin;
    memset(d, 0, sizeof(*d));
    d->shading_model = PROFILE_SHADING_STANDARD;
    d->layer_count   = (uint8_t)s->layer_count;

    for (int i = 0; i < s->layer_count && i < PROFILE_MAX_LAYERS; i++) {
        const texshop_layer* l = &s->layers[i];
        skin_layer* o = &d->layers[i];

        snprintf(o->name, sizeof(o->name), "%s", l->name);
        o->enabled      = true;
        o->channel_mask = l->channel_mask & ((1u << texshop_out_count) - 1u);
        for (int k = 0; k < PROFILE_OUT_COUNT; k++) {
            o->src[k].kind      = PROFILE_SRC_CONSTANT;
            o->src[k].tex_slot  = PROFILE_TEX_NONE;
            o->src[k].channel   = PROFILE_CH_RGBA;
            o->src[k].triplanar = l->triplanar;
            o->src[k].tiling[0] = o->src[k].tiling[1] = 1.0f;
            o->blend[k]         = (uint8_t)l->blend;
        }
        o->value[PROFILE_OUT_ROUGHNESS] = 0.5f;
        for (int k = 0; k < 4; k++) o->color_tint[k] = l->color[k];
        o->opacity      = l->opacity;
        o->rough_mul    = 1.0f;
        o->height_scale = 1.0f;

        o->mask_count = (uint8_t)l->mask_count;
        for (int mesh = 0; mesh < l->mask_count && mesh < PROFILE_MAX_MASKS; mesh++) {
            profile_mask* pm = &o->masks[mesh];
            pm->type     = k_mask_to_profile[l->masks[mesh]];
            pm->source   = PROFILE_MASK_SRC_PROFILE_INPUT;
            pm->tex_slot = PROFILE_TEX_NONE;
            pm->channel  = PROFILE_CH_R;
            pm->lo       = 0.0f;
            pm->hi       = 1.0f;
            pm->contrast = 1.0f;
        }
    }

    /* the base color map: texture input 0 carries the PNG's file name, layer 0's color reads it */
    if (s->map_file[0] && d->layer_count > 0) {
        snprintf(p->tex_input_name[0], sizeof(p->tex_input_name[0]), "%s", s->map_file);
        d->layers[0].src[PROFILE_OUT_COLOR].kind     = PROFILE_SRC_TEXTURE;
        d->layers[0].src[PROFILE_OUT_COLOR].tex_slot = 0;
    }
}

static void profile_to_skin(const shader_profile* p, texshop_skin* s)
{
    fill_texshop_skin(s, p->name);
    s->id = p->id;

    const skin_profile_data* d = &p->u.skin;
    int face_type = d->layer_count;
    if (face_type > texshop_max_layers) face_type = texshop_max_layers;
    if (face_type < 1) return;                                          /* keep the default base layer */
    s->layer_count = face_type;

    if (d->layers[0].src[PROFILE_OUT_COLOR].kind == PROFILE_SRC_TEXTURE && d->layers[0].src[PROFILE_OUT_COLOR].tex_slot == 0) {
        snprintf(s->map_file, sizeof(s->map_file), "%s", p->tex_input_name[0]);
    }

    for (int i = 0; i < face_type; i++) {
        const skin_layer* o = &d->layers[i];
        texshop_layer* l = &s->layers[i];
        const uint32_t channels = o->channel_mask & ((1u << texshop_out_count) - 1u);
        reset_texshop_layer(l, o->name, channels);

        int first = 0;                                              /* the profile has a blend per output, the editor one per layer */
        for (int k = 0; k < texshop_out_count; k++)
            if (channels & texshop_bit(k)) { first = k; break; }
        l->blend     = (o->blend[first] < texshop_blend_count) ? (enum_texshop_blend)o->blend[first] : texshop_blend_lerp;
        l->triplanar = o->src[PROFILE_OUT_COLOR].triplanar;
        for (int k = 0; k < 4; k++) l->color[k] = o->color_tint[k];
        l->opacity = o->opacity;

        for (int mesh = 0; mesh < o->mask_count && mesh < PROFILE_MAX_MASKS && l->mask_count < texshop_max_masks; mesh++) {
            for (int t = 0; t < texshop_mask_count; t++)
                if (k_mask_to_profile[t] == o->masks[mesh].type) { l->masks[l->mask_count++] = (enum_texshop_mask)t; break; }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* start: load every skin file                                               */
/* ------------------------------------------------------------------------- */
static texshop_skin* find_texshop_skin_by_id(uint32_t id)
{
    if (id == 0) return NULL;
    for (int i = 0; i < g_texshop.skin_count; i++)
        if (g_texshop.skins[i].id == id) return &g_texshop.skins[i];
    return NULL;
}

static void load_texshop_skins_from_disk(void)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(TEXSHOP_SKIN_FOLDER "\\*" TEXSHOP_SKIN_EXT, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    uint32_t max_id = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (g_texshop.skin_count >= texshop_max_skins) break;

        char path[260];
        if (snprintf(path, sizeof(path), TEXSHOP_SKIN_FOLDER "\\%s", fd.cFileName) >= (int)sizeof(path)) continue;
        if (!shader_profile_load(path, &s_profile) || s_profile.kind != PROFILE_KIND_SKIN) continue;

        texshop_skin* s = &g_texshop.skins[g_texshop.skin_count];
        profile_to_skin(&s_profile, s);
        if (s->id == 0 || find_texshop_skin_by_id(s->id)) s->id = g_texshop.next_skin_id++;
        if (skin_name_taken(s->name)) make_unique_skin_name(s->name, s->name);
        if (s->id > max_id) max_id = s->id;
        s->saved_hash = hash_texshop_skin(s);                       /* loaded = clean */
        g_texshop.skin_count++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    if (max_id >= g_texshop.next_skin_id) g_texshop.next_skin_id = max_id + 1;
}

static void start_texshop_state(void)
{
    memset(&g_texshop, 0, sizeof(g_texshop));
    g_texshop.ready = true;                                         /* before loading: nothing below may call check_texshop_ready twice */
    g_texshop.next_skin_number = 1;
    /* ids come from the clock so a stale skin_ref saved in the object database before skins had files never lands on a new skin.
       Loaded files keep their own id, and new ids always start above the highest loaded one. */
    g_texshop.next_skin_id = (uint32_t)time(NULL);
    if (g_texshop.next_skin_id == 0) g_texshop.next_skin_id = 1;
    load_texshop_skins_from_disk();

    if (g_texshop.skin_count > 0) {
        char text[96];
        snprintf(text, sizeof(text), "Loaded %d skin(s) from " TEXSHOP_SKIN_FOLDER ".", g_texshop.skin_count);
        set_texshop_status(text);
    } else {
        set_texshop_status("Press Create to make a skin.");
    }
}

void check_texshop_ready(void)
{
    if (!g_texshop.ready) start_texshop_state();
}

/* ------------------------------------------------------------------------- */
/* skin editing                                                              */
/* ------------------------------------------------------------------------- */
texshop_skin* get_texshop_skin(void)
{
    if (g_texshop.skin_count <= 0) return NULL;
    if (g_texshop.selected_skin < 0) g_texshop.selected_skin = 0;
    if (g_texshop.selected_skin >= g_texshop.skin_count) g_texshop.selected_skin = g_texshop.skin_count - 1;
    return &g_texshop.skins[g_texshop.selected_skin];
}

texshop_layer* get_texshop_layer(texshop_skin* s)
{
    if (s->selected_layer < 0) s->selected_layer = 0;
    if (s->selected_layer >= s->layer_count) s->selected_layer = s->layer_count - 1;
    return &s->layers[s->selected_layer];
}

/* public: the context menu and the Skin Maker call these */
void create_texshop_skin(void)
{
    check_texshop_ready();
    if (g_texshop.skin_count >= texshop_max_skins) { set_texshop_status("Skin limit is 32."); return; }
    char name[texshop_name_len];
    do { snprintf(name, sizeof(name), "skin %d", g_texshop.next_skin_number++); } while (skin_name_taken(name));
    texshop_skin* s = &g_texshop.skins[g_texshop.skin_count];
    fill_texshop_skin(s, name);
    s->id = g_texshop.next_skin_id++;
    g_texshop.selected_skin = g_texshop.skin_count++;
    set_texshop_status("New skin. Edit it, then Export.");
}

/* a skin whose base color layer is white and reads a PNG from assets/textures. written to assets/skins so it shows in the File Archive. */
bool create_texshop_skin_from_map(const char* base_name, const char* map_file)
{
    check_texshop_ready();
    if (g_texshop.skin_count >= texshop_max_skins) {
        set_texshop_status("Skin limit is 32.");
        return false;
    }
    char name[texshop_name_len];
    make_unique_skin_name(name, base_name);

    texshop_skin* s = &g_texshop.skins[g_texshop.skin_count];
    fill_texshop_skin(s, name);
    s->id = g_texshop.next_skin_id++;
    snprintf(s->map_file, sizeof(s->map_file), "%s", map_file);
    s->layers[0].color[0] = 1.0f;
    s->layers[0].color[1] = 1.0f;
    s->layers[0].color[2] = 1.0f;
    s->layers[0].color[3] = 1.0f;
    s->layers[0].opacity = 1.0f;
    g_texshop.selected_skin = g_texshop.skin_count;
    g_texshop.skin_count++;
    export_texshop_skin();
    return true;
}

void duplicate_texshop_skin(void)
{
    check_texshop_ready();
    texshop_skin* src = get_texshop_skin();
    if (!src) { set_texshop_status("No skin to duplicate."); return; }
    if (g_texshop.skin_count >= texshop_max_skins) { set_texshop_status("Skin limit is 32."); return; }
    texshop_skin* dst = &g_texshop.skins[g_texshop.skin_count];
    char base[texshop_name_len];
    snprintf(base, sizeof(base), "%.18s copy", src->name);
    *dst = *src;
    dst->id = g_texshop.next_skin_id++;
    dst->saved_hash = 0;                                            /* a copy has no file yet */
    make_unique_skin_name(dst->name, base);
    g_texshop.selected_skin = g_texshop.skin_count++;
    set_texshop_status("Skin duplicated.");
}

void delete_texshop_skin(void)
{
    check_texshop_ready();
    texshop_skin* doomed = get_texshop_skin();
    if (!doomed) { set_texshop_status("No skin to delete."); return; }
    skin_gpu_remove(doomed->id);                        /* buffer + set are freed 3 frames later */
    recycle_skin_file(doomed->name);                    /* its .sprof goes to the recycle bin, or it would come back at the next start */
    for (int i = g_texshop.selected_skin; i < g_texshop.skin_count - 1; i++) g_texshop.skins[i] = g_texshop.skins[i + 1];
    g_texshop.skin_count--;
    if (g_texshop.selected_skin >= g_texshop.skin_count) g_texshop.selected_skin = g_texshop.skin_count - 1;
    file_archive_refresh();
    set_texshop_status("Skin deleted.");
}

void step_texshop_skin(int direction)
{
    check_texshop_ready();
    if (g_texshop.skin_count <= 0) return;
    g_texshop.selected_skin = (g_texshop.selected_skin + direction + g_texshop.skin_count) % g_texshop.skin_count;
}

void export_texshop_skin(void)
{
    check_texshop_ready();
    texshop_skin* s = get_texshop_skin();
    if (!s) { set_texshop_status("Create a skin first."); return; }

    CreateDirectoryA(FILE_ARCHIVE_ROOT_FOLDER, NULL);               /* already there is fine */
    CreateDirectoryA(TEXSHOP_SKIN_FOLDER, NULL);

    char path[260];
    skin_file_path(s->name, path, sizeof(path));
    skin_to_profile(s, &s_profile);
    if (!shader_profile_save(path, &s_profile)) { set_texshop_status("Export failed. See the log."); return; }

    s->saved_hash = hash_texshop_skin(s);
    file_archive_refresh();                                         /* shows up in the Files window */
    char text[96];
    snprintf(text, sizeof(text), "Exported %.30s" TEXSHOP_SKIN_EXT " to " TEXSHOP_SKIN_FOLDER, s->name);
    set_texshop_status(text);
}

void add_texshop_mask(texshop_layer* l, enum_texshop_mask mask)
{
    if (l->mask_count < texshop_max_masks) l->masks[l->mask_count++] = mask;
}

void remove_texshop_mask(texshop_layer* l, int index)
{
    if (index < 0 || index >= l->mask_count) return;
    for (int i = index; i < l->mask_count - 1; i++) l->masks[i] = l->masks[i + 1];
    l->mask_count--;
}

void add_texshop_layer(texshop_skin* s)
{
    if (s->layer_count >= texshop_max_layers) { set_texshop_status("Layer limit is 16."); return; }
    char name[texshop_name_len];
    snprintf(name, sizeof(name), "layer %d", s->layer_count);
    reset_texshop_layer(&s->layers[s->layer_count], name, texshop_bit(texshop_out_color));
    s->selected_layer = s->layer_count++;
}

void remove_texshop_layer(texshop_skin* s)
{
    if (s->layer_count <= 1) { set_texshop_status("A skin needs at least one layer."); return; }
    for (int i = s->selected_layer; i < s->layer_count - 1; i++) s->layers[i] = s->layers[i + 1];
    s->layer_count--;
    if (s->selected_layer >= s->layer_count) s->selected_layer = s->layer_count - 1;
}

void move_texshop_layer(texshop_skin* s, int direction)
{
    const int a = s->selected_layer, b = a + direction;
    if (b < 0 || b >= s->layer_count) return;
    texshop_layer t = s->layers[a];
    s->layers[a] = s->layers[b];
    s->layers[b] = t;
    s->selected_layer = b;
}

/* ------------------------------------------------------------------------- */
/* assign: object -> skin id (see system_textureshop_manager.h)              */
/* ------------------------------------------------------------------------- */
/* the object obj_vert_selected in the Hierarchy. NULL for nothing, or for the World Space Root (objects[0], never drawn) */
static system_scene_object* get_texshop_target_object(void)
{
    system_scene_object* o = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (!o || o == &g_scene_registry.objects[0]) return NULL;
    return o;
}

uint32_t get_object_skin_id(uint32_t object_id)
{
    check_texshop_ready();
    uint32_t size = 0;
    const skin_ref* r = (const skin_ref*)object_db_get(object_id, object_data_skin_ref, &size);
    if (!r || size != sizeof(*r)) return 0;
    return find_texshop_skin_by_id(r->skin_id) ? r->skin_id : 0;     /* a deleted skin counts as no skin */
}

const char* get_object_skin_name(uint32_t object_id)
{
    const texshop_skin* s = find_texshop_skin_by_id(get_object_skin_id(object_id));
    return s ? s->name : NULL;
}

void texshop_describe_target(char* out, size_t cap)
{
    const system_scene_object* o = get_texshop_target_object();
    if (!o) { snprintf(out, cap, "No object obj_vert_selected."); return; }
    const char* name = get_object_skin_name(o->id);
    snprintf(out, cap, "%.12s: %.16s", o->name, name ? name : "no skin");
}

void assign_texshop_skin_to_selected(void)
{
    check_texshop_ready();
    texshop_skin* s = get_texshop_skin();
    if (!s) { set_texshop_status("Create a skin first."); return; }

    system_scene_object* o = get_texshop_target_object();
    if (!o) { set_texshop_status("Select an object in the Hierarchy first."); return; }
    if (o->type == scene_object_camera || o->type == scene_object_light) { set_texshop_status("Cameras and lights have no surface."); return; }

    const skin_ref ref = { s->id };
    if (!object_db_set(o->id, object_data_skin_ref, &ref, (uint32_t)sizeof(ref))) {
        set_texshop_status("That object already holds 8 data blocks.");
        return;
    }
    texshop_build_skin_gpu(s->id);                      /* make sure the GPU copy is current before the next frame draws it */
    char text[96];
    snprintf(text, sizeof(text), "Assigned '%.20s' to '%.20s'. Scene tab: Textured shows it.", s->name, o->name);
    set_texshop_status(text);
}

/* the skin file picked in the File Archive -> the object picked in the scene. No Textureshop window needed. */
bool assign_archive_skin_to_selected(void)
{
    check_texshop_ready();

    const int node_index = file_archive_selected();
    const file_archive_node* node = file_archive_node_at(node_index);
    if (!node || node->kind != file_node_file) {
        set_statusbar_message("%s", "Select a skin file in the File Archive first.");
        return false;
    }

    char skin_name[file_archive_name_max];
    snprintf(skin_name, sizeof(skin_name), "%s", node->name);
    char* dot = strrchr(skin_name, '.');
    if (!dot || _stricmp(dot, TEXSHOP_SKIN_EXT) != 0) {
        set_statusbar_message("%s", "The selected archive file is not a skin (.sprof).");
        return false;
    }
    *dot = '\0';

    for (int i = 0; i < g_texshop.skin_count; i++) {
        if (_stricmp(g_texshop.skins[i].name, skin_name) == 0) {
            g_texshop.selected_skin = i;
            assign_texshop_skin_to_selected();
            set_statusbar_message("%s", g_texshop.status);
            return true;
        }
    }

    set_statusbar_message("%s", "That skin file is not loaded. Open Textureshop once or refresh the archive.");
    return false;
}

void clear_texshop_skin_on_selected(void)
{
    check_texshop_ready();
    system_scene_object* o = get_texshop_target_object();
    if (!o) { set_texshop_status("Select an object in the Hierarchy first."); return; }
    uint32_t size = 0;
    if (!object_db_get(o->id, object_data_skin_ref, &size)) { set_texshop_status("That object has no skin."); return; }
    object_db_remove(o->id, object_data_skin_ref);
    set_texshop_status("Skin cleared. The object is grey again.");
}

/* ------------------------------------------------------------------------- */
/* skin -> GPU (system_skin_gpu.c). Built on assign or edit and cached per skin, never per frame. */
/* ------------------------------------------------------------------------- */
static void fill_skin_gpu_data(const texshop_skin* s, skin_gpu_data* out)
{
    memset(out, 0, sizeof(*out));
    int face_type = s->layer_count;
    if (face_type > SKIN_GPU_MAX_LAYERS) face_type = SKIN_GPU_MAX_LAYERS;
    out->layer_count = (uint32_t)face_type;
    for (int i = 0; i < face_type; i++) {
        const texshop_layer* l = &s->layers[i];
        for (int k = 0; k < 4; k++) out->layers[i].color[k] = l->color[k];
        out->layers[i].opacity      = l->opacity;
        out->layers[i].channel_mask = l->channel_mask;
        out->layers[i].blend        = (uint32_t)l->blend;
        out->layers[i].triplanar    = l->triplanar ? 1u : 0u;
    }
}

/* public: the renderer calls this the first time it meets a skin that has no GPU data yet. */
extern load_image_asset_ptr   editor_load_image;           /* system_import_export.c: the loader plugin */
extern remove_image_asset_ptr editor_remove_image;
bool check_loader_ready(void);

void texshop_build_skin_gpu(uint32_t skin_id)
{
    texshop_skin* s = find_texshop_skin_by_id(skin_id);
    if (!s) return;
    skin_gpu_data data;
    fill_skin_gpu_data(s, &data);

    /* the base color map: decoded here, uploaded by skin_gpu_set, freed right after */
    loader_image_data image;
    memset(&image, 0, sizeof(image));
    bool map_loaded = false;
    if (s->map_file[0] && check_loader_ready()) {
        char path[260];
        snprintf(path, sizeof(path), FILE_ARCHIVE_ROOT_FOLDER "\\textures\\%s", s->map_file);
        map_loaded = editor_load_image(path, &image);
        if (map_loaded) {
            data.map_pixels = image.pixels;
            data.map_width  = (uint32_t)image.width;
            data.map_height = (uint32_t)image.height;
        } else {
            phoenix_warn("SKIN", "skin '%s': map '%s' could not be loaded.", s->name, path);
        }
    }
    if (skin_gpu_set(s->id, &data)) s->gpu_hash = hash_texshop_skin(s);
    if (map_loaded) editor_remove_image(&image);
}

/* once per frame from the Skin Maker: the obj_vert_selected skin is the only one that can be edited. A changed hash rebuilds its
   buffer + set; the old pair is retired and freed 3 frames later. A skin that was never drawn is built lazily instead. */
void texshop_sync_skin_gpu(void)
{
    texshop_skin* s = get_texshop_skin();
    if (!s) return;
    const uint32_t h = hash_texshop_skin(s);
    if (h == s->gpu_hash) return;
    if (skin_gpu_has(s->id)) texshop_build_skin_gpu(s->id);
    else                     s->gpu_hash = h;
}