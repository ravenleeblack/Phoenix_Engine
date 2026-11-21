#include "system_globals.h"
#include "system_textureshop_skins.h"
#include <commdlg.h>

/* ========================================================================= */
/* the loader plugin (phoenix_loader.dll, 06_system_loader)                   */
/* the editor resolves the functions itself, so nothing extra is linked.      */
/* ========================================================================= */
HMODULE                  editor_loader_module     = NULL;
load_3d_mesh_asset_ptr   editor_load_mesh         = NULL;
remove_3d_mesh_asset_ptr editor_remove_mesh       = NULL;
save_3d_mesh_asset_ptr   editor_save_mesh         = NULL;
load_3d_mesh_asset_scaled_ptr editor_load_mesh_scaled = NULL;
save_3d_mesh_asset_scaled_ptr editor_save_mesh_scaled = NULL;
load_image_asset_ptr     editor_load_image        = NULL;
remove_image_asset_ptr   editor_remove_image      = NULL;
save_image_asset_ptr     editor_save_image        = NULL;

bool check_loader_ready(void)
{
    if (editor_loader_module && editor_load_mesh && editor_save_mesh && editor_load_mesh_scaled && editor_save_mesh_scaled && editor_load_image && editor_save_image) {
        return true;
    }

    editor_loader_module = GetModuleHandleA("phoenix_loader.dll");
    if (!editor_loader_module) {
        editor_loader_module = LoadLibraryA("phoenix_loader.dll");
    }
    if (!editor_loader_module) {
        set_statusbar_message("%s", "phoenix_loader.dll was not found next to the editor.");
        return false;
    }

    editor_load_mesh    = (load_3d_mesh_asset_ptr)(void*)GetProcAddress(editor_loader_module, "load_3d_mesh_asset");
    editor_remove_mesh  = (remove_3d_mesh_asset_ptr)(void*)GetProcAddress(editor_loader_module, "remove_3d_mesh_asset");
    editor_save_mesh    = (save_3d_mesh_asset_ptr)(void*)GetProcAddress(editor_loader_module, "save_3d_mesh_asset");
    editor_load_mesh_scaled = (load_3d_mesh_asset_scaled_ptr)(void*)GetProcAddress(editor_loader_module, "load_3d_mesh_asset_scaled");
    editor_save_mesh_scaled = (save_3d_mesh_asset_scaled_ptr)(void*)GetProcAddress(editor_loader_module, "save_3d_mesh_asset_scaled");
    editor_load_image   = (load_image_asset_ptr)(void*)GetProcAddress(editor_loader_module, "load_image_asset");
    editor_remove_image = (remove_image_asset_ptr)(void*)GetProcAddress(editor_loader_module, "remove_image_asset");
    editor_save_image   = (save_image_asset_ptr)(void*)GetProcAddress(editor_loader_module, "save_image_asset");

    if (!editor_load_mesh || !editor_remove_mesh || !editor_save_mesh || !editor_load_mesh_scaled || !editor_save_mesh_scaled || !editor_load_image || !editor_remove_image || !editor_save_image) {
        set_statusbar_message("%s", "phoenix_loader.dll is out of date. Run the build (wsl make all) and start again.");
        return false;
    }
    return true;
}

/* ========================================================================= */
/* windows file dialogs                                                       */
/* ========================================================================= */
typedef BOOL (WINAPI *file_dialog_ptr)(LPOPENFILENAMEA);

bool pick_file_path(bool save, const char* title, const char* filter, const char* default_extension,
                    const char* initial_name, char* out_path, size_t out_size)
{
    HMODULE module = LoadLibraryA("comdlg32.dll");
    if (!module) {
        return false;
    }

    file_dialog_ptr dialog = NULL;
    if (save) {
        dialog = (file_dialog_ptr)(void*)GetProcAddress(module, "GetSaveFileNameA");
    } else {
        dialog = (file_dialog_ptr)(void*)GetProcAddress(module, "GetOpenFileNameA");
    }
    if (!dialog) {
        FreeLibrary(module);
        return false;
    }

    out_path[0] = '\0';
    if (initial_name) {
        snprintf(out_path, out_size, "%s", initial_name);
    }

    OPENFILENAMEA info;
    memset(&info, 0, sizeof(info));
    info.lStructSize = sizeof(info);
    info.hwndOwner = hwnd_window_00;
    info.lpstrFilter = filter;
    info.nFilterIndex = 1;
    info.lpstrFile = out_path;
    info.nMaxFile = (DWORD)out_size;
    info.lpstrTitle = title;
    info.lpstrDefExt = default_extension;
    info.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (save) {
        info.Flags |= OFN_OVERWRITEPROMPT;
    } else {
        info.Flags |= OFN_FILEMUSTEXIST;
    }

    BOOL picked = dialog(&info);
    FreeLibrary(module);

    /* the dialog ate the mouse up of the click that opened it: read the real button state again, or the next click has no press edge.
       and the editor takes the keyboard back, or keys would go to whatever window the dialog returned focus to. */
    element_context.input.mouse_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    element_context.input.mouse_pressed = false;
    if (hwnd_window_00) {
        SetForegroundWindow(hwnd_window_00);
        SetFocus(hwnd_window_00);
    }
    return picked != 0;
}

bool pick_open_file_path(const char* title, const char* filter, char* out_path, size_t out_size)
{
    return pick_file_path(false, title, filter, NULL, NULL, out_path, out_size);
}

bool pick_save_file_path(const char* title, const char* filter, const char* default_extension,
                         const char* initial_name, char* out_path, size_t out_size)
{
    return pick_file_path(true, title, filter, default_extension, initial_name, out_path, out_size);
}

/* ========================================================================= */
/* path helpers                                                               */
/* ========================================================================= */
void get_path_base_name(const char* path, char* out, size_t out_size)
{
    const char* name = path;
    for (const char* p = path; *p; p++) {
        if (*p == '\\' || *p == '/') {
            name = p + 1;
        }
    }
    snprintf(out, out_size, "%s", name);
    char* dot = strrchr(out, '.');
    if (dot) {
        *dot = '\0';
    }
}

bool check_path_extension(const char* path, const char* extension)
{
    const size_t path_length = strlen(path);
    const size_t extension_length = strlen(extension);
    if (path_length < extension_length) {
        return false;
    }
    return _stricmp(path + (path_length - extension_length), extension) == 0;
}

/* assets\models\name.fbx, or "name 2.fbx" when that is taken */
void make_unique_asset_path(const char* folder, const char* base, const char* extension, char* out, size_t out_size)
{
    snprintf(out, out_size, "%s\\%s%s", folder, base, extension);
    for (int n = 2; GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES && n < 1000; n++) {
        snprintf(out, out_size, "%s\\%s %d%s", folder, base, n, extension);
    }
}

/* the imported file becomes an asset: it is copied into the archive, the tree is refreshed and the file is selected. */
bool copy_file_into_archive(const char* source_path, const char* subfolder, char* out_path, size_t out_size)
{
    char folder[260];
    CreateDirectoryA(FILE_ARCHIVE_ROOT_FOLDER, NULL);
    snprintf(folder, sizeof(folder), FILE_ARCHIVE_ROOT_FOLDER "\\%s", subfolder);
    CreateDirectoryA(folder, NULL);

    /* a file that already lives in that folder is not copied onto itself */
    char source_full[260];
    char folder_full[260];
    if (GetFullPathNameA(source_path, sizeof(source_full), source_full, NULL) > 0 &&
        GetFullPathNameA(folder, sizeof(folder_full), folder_full, NULL) > 0) {
        const size_t folder_length = strlen(folder_full);
        if (_strnicmp(source_full, folder_full, folder_length) == 0 && source_full[folder_length] == '\\') {
            snprintf(out_path, out_size, "%s", source_full);
            return true;
        }
    }

    char extension[16];
    const char* dot = strrchr(source_path, '.');
    snprintf(extension, sizeof(extension), "%s", dot);
    for (char* p = extension; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = (char)(*p - 'A' + 'a');
        }
    }

    char base[128];
    get_path_base_name(source_path, base, sizeof(base));
    if (strcmp(subfolder, "textures") == 0 && strlen(base) > 22) {
        base[22] = '\0';                                    /* the skin file stores this name in 31 characters */
    }
    make_unique_asset_path(folder, base, extension, out_path, out_size);
    return CopyFileA(source_path, out_path, TRUE) != 0;
}

void select_archive_file(const char* path)
{
    file_archive_refresh();

    char full[260];
    if (GetFullPathNameA(path, sizeof(full), full, NULL) == 0) {
        return;
    }
    const int count = file_archive_node_count();
    for (int i = 0; i < count; i++) {
        const file_archive_node* node = file_archive_node_at(i);
        if (!node || node->kind != file_node_file) {
            continue;
        }
        char node_full[260];
        if (GetFullPathNameA(node->path, sizeof(node_full), node_full, NULL) == 0) {
            continue;
        }
        if (_stricmp(node_full, full) == 0) {
            int parent = node->parent;
            while (parent >= 0) {
                const file_archive_node* folder = file_archive_node_at(parent);
                if (!folder) {
                    break;
                }
                if (!folder->is_open) {
                    file_archive_toggle_open(parent);
                }
                parent = folder->parent;
            }
            file_archive_select(i);
            return;
        }
    }
}

/* ========================================================================= */
/* import                                                                     */
/* ========================================================================= */
/* a loaded mesh -> a scene object, the same way the cube does it */
bool create_system_imported_mesh(const char* name, const loader_mesh_data* mesh)
{
    if (!mesh || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3) {
        return false;
    }
    if (g_scene_registry.object_count >= max_scene_objects) {
        set_statusbar_message("%s", "Database full. Cannot allocate the imported object.");
        return false;
    }

    editor_vertex* vertices = (editor_vertex*)calloc(mesh->vertex_count, sizeof(editor_vertex));
    if (!vertices) {
        return false;
    }
    const float grey = skin_empty_grey_linear();            /* the same grey as every untextured object */
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        const loader_vertex* source = &mesh->vertices[i];
        editor_vertex* target = &vertices[i];
        target->x = source->pos[0];
        target->y = source->pos[1];
        target->z = source->pos[2];
        target->r = grey;
        target->g = grey;
        target->b = grey;
        target->a = 1.0f;
        target->u = source->uv[0];
        target->vert_to_edge_array = 1.0f - source->uv[1];     /* FBX v points up, image rows start at the top */
        target->nx = source->normal[0];
        target->ny = source->normal[1];
        target->nz = source->normal[2];
    }

    system_scene_object* obj = spawn_new_scene_object(name, (editor_vec3){ 0.0f, 0.0f, 0.0f });
    if (!obj) {
        free(vertices);
        return false;
    }
    obj->type = scene_object_mesh;
    obj->scale = (editor_vec3){ 1.0f, 1.0f, 1.0f };

    /* the database copy and edit mode first, so the object stays editable even if the GPU upload fails */
    set_object_mesh_data(obj->id, vertices, (uint32_t)sizeof(editor_vertex), mesh->vertex_count, mesh->indices, mesh->index_count);
    em_registry_adopt_triangles(obj->id, vertices, mesh->vertex_count, mesh->indices, mesh->index_count);

    bool uploaded = upload_scene_object_mesh(obj, render_context.logical_device, render_context.physical_device,
                                             vertices, mesh->vertex_count, mesh->indices, mesh->index_count);
    free(vertices);
    if (!uploaded) {
        set_statusbar_message("Import: '%s' GPU upload failed, the object is in the hierarchy only.", name);
        return false;
    }

    g_scene_registry.selected_object_id = obj->id;
    focus_system_grid_on_selected_object();
    return true;
}

/* .fbx and .obj: the model is sized for the import profile (Preferences > Import / Export) */
bool import_model_file(const char* source_path, const char* kind_name)
{
    const int profile = g_unit_prefs.import_profile;
    char asset_path[260];
    if (!copy_file_into_archive(source_path, "models", asset_path, sizeof(asset_path))) {
        set_statusbar_message("Import failed: could not copy '%s' into the File Archive.", source_path);
        return false;
    }

    loader_mesh_data mesh;
    memset(&mesh, 0, sizeof(mesh));
    if (!editor_load_mesh_scaled(asset_path, &mesh, get_unit_profile_meters_per_unit(profile), get_unit_profile_trusts_fbx_units(profile))) {
        set_statusbar_message("Import failed: '%s' is not an %s with mesh geometry the loader understands.", asset_path, kind_name);
        select_archive_file(asset_path);
        return false;
    }

    char base[128];
    get_path_base_name(asset_path, base, sizeof(base));
    const uint32_t vertex_count = mesh.vertex_count;
    const uint32_t triangle_count = mesh.index_count / 3;
    const bool created = create_system_imported_mesh(base, &mesh);
    editor_remove_mesh(&mesh);

    select_archive_file(asset_path);
    if (created) {
        set_statusbar_message("Imported '%s' (%s units): %u vertices, %u triangles. Saved in the File Archive (models).", base, get_unit_profile_name(profile), vertex_count, triangle_count);
    }
    phoenix_info("Import", "%s '%s' (%s units): %u vertices, %u triangles, created=%d", kind_name, asset_path, get_unit_profile_name(profile), vertex_count, triangle_count, (int)created);
    return created;
}

bool import_png_file(const char* source_path)
{
    char asset_path[260];
    if (!copy_file_into_archive(source_path, "textures", asset_path, sizeof(asset_path))) {
        set_statusbar_message("Import failed: could not copy '%s' into the File Archive.", source_path);
        return false;
    }

    loader_image_data image;
    memset(&image, 0, sizeof(image));
    if (!editor_load_image(asset_path, &image)) {
        set_statusbar_message("Import failed: '%s' is not a PNG the loader understands (interlaced PNGs are not supported).", asset_path);
        select_archive_file(asset_path);
        return false;
    }

    const int width = image.width;
    const int height = image.height;
    editor_remove_image(&image);                           /* only checked here: the skin loads the file itself */

    char base[128];
    get_path_base_name(asset_path, base, sizeof(base));
    const char* file_name = strrchr(asset_path, '\\');
    if (file_name) {
        file_name++;
    } else {
        file_name = asset_path;
    }
    const bool created = create_texshop_skin_from_map(base, file_name);
    select_archive_file(asset_path);

    /* a mesh is selected: it gets the new skin right away */
    bool assigned = false;
    const system_scene_object* target = find_scene_object_by_id(g_scene_registry.selected_object_id);
    if (created && target && target != &g_scene_registry.objects[0] && target->type == scene_object_mesh) {
        assign_texshop_skin_to_selected();
        assigned = true;
    }
    if (created) {
        if (assigned) {
            set_statusbar_message("Imported '%s' (%dx%d): skin '%s' reads it as its base color map and is assigned to '%s'.", base, width, height, g_texshop.skins[g_texshop.selected_skin].name, target->name);
        } else {
            set_statusbar_message("Imported '%s' (%dx%d): skin '%s' reads it as its base color map. Select an object and assign the skin.", base, width, height, g_texshop.skins[g_texshop.selected_skin].name);
        }
    }
    phoenix_info("Import", "png '%s': %dx%d, skin created=%d", asset_path, width, height, (int)created);
    return created;
}

bool import_asset_file(const char* source_path)
{
    if (!source_path || source_path[0] == '\0') {
        return false;
    }
    if (!check_loader_ready()) {
        return false;
    }
    if (check_path_extension(source_path, ".fbx")) {
        return import_model_file(source_path, "FBX");
    }
    if (check_path_extension(source_path, ".obj")) {
        return import_model_file(source_path, "OBJ");
    }
    if (check_path_extension(source_path, ".png")) {
        return import_png_file(source_path);
    }
    set_statusbar_message("%s", "Import supports .fbx and .obj models and .png images.");
    return false;
}

void import_asset_from_dialog(void)
{
    char path[260];
    const char filter[] = "Models and images (*.fbx;*.obj;*.png)\0*.fbx;*.obj;*.png\0FBX models (*.fbx)\0*.fbx\0OBJ models (*.obj)\0*.obj\0PNG images (*.png)\0*.png\0\0";
    if (!pick_open_file_path("Import", filter, path, sizeof(path))) {
        return;
    }
    import_asset_file(path);
}

/* ========================================================================= */
/* unit profiles (Preferences > Import / Export)                               */
/* ========================================================================= */
unit_prefs g_unit_prefs = { unit_profile_phoenix, unit_profile_phoenix };

const char* get_unit_profile_name(int profile)
{
    switch (profile) {
        case unit_profile_maya:   return "Maya";
        case unit_profile_unity:  return "Unity";
        case unit_profile_zbrush: return "ZBrush";
        default:                  return "Phoenix";
    }
}

float get_unit_profile_meters_per_unit(int profile)
{
    if (profile == unit_profile_maya || profile == unit_profile_zbrush) {
        return 0.01f;
    }
    return 1.0f;
}

bool get_unit_profile_trusts_fbx_units(int profile)
{
    return profile != unit_profile_zbrush;
}

float get_unit_profile_units_per_meter(int profile)
{
    if (profile == unit_profile_maya || profile == unit_profile_zbrush) {
        return 100.0f;
    }
    return 1.0f;
}

/* ========================================================================= */
/* export                                                                     */
/* ========================================================================= */
bool export_window_open = false;
int  export_format      = export_format_fbx_binary;
int  export_source      = export_source_scene_object;

bool is_export_window_open(void)
{
    return export_window_open;
}

void open_export_window(void)
{
    preferences_window_open = false;            /* two modal windows at once would fight over the mouse */
    export_window_open = true;
    set_statusbar_message("%s", "Export: pick what to export and the format, then press Export.");
}

/* a scene object -> loader mesh (object space, in meters). the vertices are copied, free them with free_export_mesh */
bool build_export_mesh(const system_scene_object* obj, loader_mesh_data* out)
{
    const void* vertices = NULL;
    uint32_t vertex_size = 0;
    uint32_t vertex_count = 0;
    const uint32_t* indices = NULL;
    uint32_t index_count = 0;
    if (!get_object_mesh_data(obj->id, &vertices, &vertex_size, &vertex_count, &indices, &index_count)) {
        return false;
    }
    if (vertex_size != sizeof(editor_vertex) || vertex_count == 0 || index_count < 3) {
        return false;
    }

    loader_vertex* copy = (loader_vertex*)calloc(vertex_count, sizeof(loader_vertex));
    uint32_t* copy_indices = (uint32_t*)malloc(sizeof(uint32_t) * index_count);
    if (!copy || !copy_indices) {
        free(copy);
        free(copy_indices);
        return false;
    }
    const editor_vertex* source = (const editor_vertex*)vertices;
    for (uint32_t i = 0; i < vertex_count; i++) {
        copy[i].pos[0] = source[i].x;
        copy[i].pos[1] = source[i].y;
        copy[i].pos[2] = source[i].z;
        copy[i].color[0] = source[i].r;
        copy[i].color[1] = source[i].g;
        copy[i].color[2] = source[i].b;
        copy[i].color[3] = source[i].a;
        copy[i].uv[0] = source[i].u;
        copy[i].uv[1] = 1.0f - source[i].vert_to_edge_array;
        copy[i].normal[0] = source[i].nx;
        copy[i].normal[1] = source[i].ny;
        copy[i].normal[2] = source[i].nz;
    }
    memcpy(copy_indices, indices, sizeof(uint32_t) * index_count);

    out->vertices = copy;
    out->indices = copy_indices;
    out->vertex_count = vertex_count;
    out->index_count = index_count;
    return true;
}

void free_export_mesh(loader_mesh_data* mesh)
{
    free(mesh->vertices);
    free(mesh->indices);
    memset(mesh, 0, sizeof(*mesh));
}

bool export_current_selection(void)
{
    if (!check_loader_ready()) {
        return false;
    }

    char default_name[128];
    char source_file[260];
    source_file[0] = '\0';
    const system_scene_object* obj = NULL;

    if (export_source == export_source_scene_object) {
        obj = find_scene_object_by_id(g_scene_registry.selected_object_id);
        if (!obj || obj == &g_scene_registry.objects[0] || obj->type != scene_object_mesh) {
            set_statusbar_message("%s", "Export: select a mesh object in the scene first.");
            return false;
        }
        if (export_format == export_format_png) {
            set_statusbar_message("%s", "Export: a scene object can only be written as FBX or OBJ. Pick a model format.");
            return false;
        }
        snprintf(default_name, sizeof(default_name), "%s", obj->name);
    } else {
        const file_archive_node* node = file_archive_node_at(file_archive_selected());
        if (!node || node->kind != file_node_file) {
            set_statusbar_message("%s", "Export: select an .fbx, .obj or .png file in the File Archive first.");
            return false;
        }
        snprintf(source_file, sizeof(source_file), "%s", node->path);
        const bool is_png = check_path_extension(source_file, ".png");
        const bool is_model = check_path_extension(source_file, ".fbx") || check_path_extension(source_file, ".obj");
        if (!is_png && !is_model) {
            set_statusbar_message("%s", "Export: the selected archive file is not an .fbx, .obj or .png.");
            return false;
        }
        if (is_png && export_format != export_format_png) {
            set_statusbar_message("%s", "Export: a PNG can only be written as PNG. Pick the PNG format.");
            return false;
        }
        if (is_model && export_format == export_format_png) {
            set_statusbar_message("%s", "Export: a model can only be written as FBX or OBJ. Pick a model format.");
            return false;
        }
        get_path_base_name(source_file, default_name, sizeof(default_name));
    }

    const char* extension = "fbx";
    const char* title = "Export FBX";
    const char fbx_filter[] = "FBX model (*.fbx)\0*.fbx\0\0";
    const char obj_filter[] = "OBJ model (*.obj)\0*.obj\0\0";
    const char png_filter[] = "PNG image (*.png)\0*.png\0\0";
    const char* filter = fbx_filter;
    if (export_format == export_format_png) {
        extension = "png";
        title = "Export PNG";
        filter = png_filter;
    } else if (export_format == export_format_obj) {
        extension = "obj";
        title = "Export OBJ";
        filter = obj_filter;
    }

    char destination[260];
    if (!pick_save_file_path(title, filter, extension, default_name, destination, sizeof(destination))) {
        return false;
    }

    bool ok = false;
    if (export_format == export_format_png) {
        loader_image_data image;
        memset(&image, 0, sizeof(image));
        if (editor_load_image(source_file, &image)) {
            ok = editor_save_image(destination, &image);
            editor_remove_image(&image);
        }
    } else {
        int mesh_format = loader_mesh_format_fbx_binary;
        if (export_format == export_format_fbx_ascii) {
            mesh_format = loader_mesh_format_fbx_ascii;
        } else if (export_format == export_format_obj) {
            mesh_format = loader_mesh_format_obj;
        }
        const float units_per_meter = get_unit_profile_units_per_meter(g_unit_prefs.export_profile);
        const int import_profile = g_unit_prefs.import_profile;

        loader_mesh_data mesh;
        memset(&mesh, 0, sizeof(mesh));
        if (obj) {
            if (build_export_mesh(obj, &mesh)) {
                ok = editor_save_mesh_scaled(destination, &mesh, mesh_format, units_per_meter);
                free_export_mesh(&mesh);
            }
        } else if (editor_load_mesh_scaled(source_file, &mesh, get_unit_profile_meters_per_unit(import_profile), get_unit_profile_trusts_fbx_units(import_profile))) {
            ok = editor_save_mesh_scaled(destination, &mesh, mesh_format, units_per_meter);
            editor_remove_mesh(&mesh);
        }
    }

    if (ok) {
        set_statusbar_message("Exported to %s (%s units).", destination, get_unit_profile_name(g_unit_prefs.export_profile));
        file_archive_refresh();
    } else {
        set_statusbar_message("Export failed: could not write %s", destination);
    }
    phoenix_info("Export", "%s ok=%d", destination, (int)ok);
    return ok;
}
