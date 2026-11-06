#include "engine_globals.h"


/* Shader profile files (.sprof). A profile is one flat blob (system_shader_profile.h), so a file is the size of the struct
   followed by the struct itself. The size is checked on load: if shader_profile changes layout, old files are refused
   instead of read as garbage. Bump PROFILE_FILE_VERSION and write a converter when that happens.
   NOT implemented yet: shader_profile_compile and shader_profile_bake_object (nothing calls them). */

void shader_profile_defaults(shader_profile* p, profile_kind kind)
{
    memset(p, 0, sizeof(*p));                       /* zeroed padding too, so equal profiles write equal files */
    p->magic   = PROFILE_FILE_MAGIC;
    p->version = PROFILE_FILE_VERSION;
    p->kind    = (uint8_t)kind;

    if (kind == PROFILE_KIND_SKIN) {
        snprintf(p->name, sizeof(p->name), "Default Lit");
        skin_profile_data* d = &p->u.skin;
        d->shading_model = PROFILE_SHADING_STANDARD;
        d->layer_count   = 1;

        skin_layer* l = &d->layers[0];
        snprintf(l->name, sizeof(l->name), "base");
        l->enabled      = true;
        l->channel_mask = PROFILE_OUT_BIT(PROFILE_OUT_COLOR);
        for (int k = 0; k < PROFILE_OUT_COUNT; k++) {
            l->src[k].kind      = PROFILE_SRC_CONSTANT;
            l->src[k].tex_slot  = PROFILE_TEX_NONE;
            l->src[k].channel   = PROFILE_CH_RGBA;
            l->src[k].tiling[0] = l->src[k].tiling[1] = 1.0f;
        }
        l->value[PROFILE_OUT_ROUGHNESS] = 0.5f;
        l->color_tint[0] = l->color_tint[1] = l->color_tint[2] = 0.55f;   /* the empty-skin grey */
        l->color_tint[3] = 1.0f;
        l->opacity       = 1.0f;
        l->rough_mul     = 1.0f;
        l->height_scale  = 1.0f;
    } else {
        snprintf(p->name, sizeof(p->name), "Thin Wire");
        wire_profile_data* w = &p->u.wire;
        w->pick_rule       = WIRE_PICK_ALL;
        w->color_mode      = WIRE_COLOR_SOLID;
        for (int k = 0; k < 4; k++) { w->tint[k] = 1.0f; w->highlight_tint[k] = 1.0f; }
        w->width           = 1.0f;
        w->width_in_pixels = true;
        w->opacity         = 1.0f;
        w->edge_softness   = 0.5f;
        w->over_skin       = true;
    }
}

bool shader_profile_save(const char* path, const shader_profile* p)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    const uint32_t size = (uint32_t)sizeof(*p);
    bool ok = (fwrite(&size, sizeof(size), 1, f) == 1) && (fwrite(p, sizeof(*p), 1, f) == 1);
    if (fclose(f) != 0) ok = false;
    return ok;
}

bool shader_profile_load(const char* path, shader_profile* out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    uint32_t size = 0;
    bool ok = (fread(&size, sizeof(size), 1, f) == 1) && size == (uint32_t)sizeof(*out) && (fread(out, sizeof(*out), 1, f) == 1);
    fclose(f);

    if (ok) ok = (out->magic == PROFILE_FILE_MAGIC) && (out->version == PROFILE_FILE_VERSION)
              && (out->kind == PROFILE_KIND_SKIN || out->kind == PROFILE_KIND_WIRE);
    if (!ok) memset(out, 0, sizeof(*out));
    return ok;
}

void material_defaults(material* mesh)
{
    memset(mesh, 0, sizeof(*mesh));                       /* EMPTY: no profiles, no textures. Objects without a skin draw neutral grey. */
    for (int i = 0; i < PROFILE_MAX_TEX_INPUTS; i++) { mesh->skin.tex_bindings[i] = PROFILE_TEX_NONE; mesh->wire.tex_bindings[i] = PROFILE_TEX_NONE; }
}