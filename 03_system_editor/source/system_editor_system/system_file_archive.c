#include "system_globals.h"

/* ========================================================================= */
/* two pools: the one on screen and the one a refresh builds. a refresh looks  */
/* at the old pool to keep which folders were open, then swaps.               */
/* ========================================================================= */
enum { scan_max_depth = 16 };

static file_archive_node s_pool[2][file_archive_max_nodes];
static int   s_count[2];
static int   s_cur      = 0;
static int   s_selected = -1;
static bool  s_started  = false;
static char  s_root_path[file_archive_path_max];
static int   s_wheel_pending = 0;

/* ------------------------------------------------------------------------- */
/* helpers                                                                    */
/* ------------------------------------------------------------------------- */
static int find_by_path(int pool, const char* path)
{
    for (int i = 0; i < s_count[pool]; i++)
        if (_stricmp(s_pool[pool][i].path, path) == 0) return i;
    return -1;
}

static bool path_is_inside_root(const char* path)
{
    const size_t n = strlen(s_root_path);
    return _strnicmp(path, s_root_path, n) == 0 && path[n] == '\\';
}

static int add_node(int pool, int parent, int prev_sibling, enum_file_node_kind kind, const char* name, const char* path)
{
    if (s_count[pool] >= file_archive_max_nodes) return -1;

    const int i = s_count[pool]++;
    file_archive_node* n = &s_pool[pool][i];
    memset(n, 0, sizeof(*n));
    n->parent       = parent;
    n->first_child  = -1;
    n->next_sibling = -1;
    n->kind         = kind;
    snprintf(n->name, sizeof(n->name), "%s", name);
    snprintf(n->path, sizeof(n->path), "%s", path);

    /* a refresh keeps folders open */
    if (kind == file_node_folder) {
        const int old = find_by_path(s_cur, path);
        if (old >= 0) n->is_open = s_pool[s_cur][old].is_open;
    }

    if (parent >= 0) {
        if (prev_sibling >= 0) s_pool[pool][prev_sibling].next_sibling = i;
        else                   s_pool[pool][parent].first_child        = i;
    }
    return i;
}

typedef struct { char name[file_archive_name_max]; bool is_dir; } scan_entry;

/* folders first, then files, each A-Z */
static int compare_entries(const void* a, const void* b)
{
    const scan_entry* ea = (const scan_entry*)a;
    const scan_entry* eb = (const scan_entry*)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return _stricmp(ea->name, eb->name);
}

static void scan_dir(int pool, int parent_idx, int depth)
{
    char pattern[file_archive_path_max + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", s_pool[pool][parent_idx].path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    scan_entry* list = NULL;
    int face_type = 0, cap = 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        if (strlen(fd.cFileName) >= file_archive_name_max) continue;

        if (face_type == cap) {
            const int new_cap = cap ? cap * 2 : 32;
            scan_entry* grown = (scan_entry*)realloc(list, (size_t)new_cap * sizeof(*list));
            if (!grown) break;
            list = grown;
            cap  = new_cap;
        }
        snprintf(list[face_type].name, sizeof(list[face_type].name), "%s", fd.cFileName);
        list[face_type].is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        face_type++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);

    if (face_type > 1) qsort(list, (size_t)face_type, sizeof(*list), compare_entries);

    int prev = -1;
    for (int k = 0; k < face_type; k++) {
        char path[file_archive_path_max];
        const int len = snprintf(path, sizeof(path), "%s\\%s", s_pool[pool][parent_idx].path, list[k].name);
        if (len < 0 || len >= (int)sizeof(path)) continue;

        const int i = add_node(pool, parent_idx, prev, list[k].is_dir ? file_node_folder : file_node_file, list[k].name, path);
        if (i < 0) break;                                   /* pool is full */
        prev = i;
        if (list[k].is_dir && depth < scan_max_depth) scan_dir(pool, i, depth + 1);
    }
    free(list);
}

/* ------------------------------------------------------------------------- */
/* start / refresh                                                            */
/* ------------------------------------------------------------------------- */
void file_archive_start(void)
{
    if (s_started) return;
    s_started = true;

    if (!GetFullPathNameA(FILE_ARCHIVE_ROOT_FOLDER, (DWORD)sizeof(s_root_path), s_root_path, NULL)) {
        snprintf(s_root_path, sizeof(s_root_path), "%s", FILE_ARCHIVE_ROOT_FOLDER);
    }
    CreateDirectoryA(s_root_path, NULL);                    /* already there is fine */
    phoenix_info("FileArchive", "root folder: %s", s_root_path);

    s_count[0] = s_count[1] = 0;
    s_cur = 0;
    file_archive_refresh();
}

void file_archive_refresh(void)
{
    if (!s_started) { file_archive_start(); return; }

    char selected_path[file_archive_path_max] = "";
    if (s_selected >= 0 && s_selected < s_count[s_cur])
        snprintf(selected_path, sizeof(selected_path), "%s", s_pool[s_cur][s_selected].path);

    const bool root_was_open = (s_count[s_cur] > 0) ? s_pool[s_cur][0].is_open : true;

    const int next = 1 - s_cur;
    s_count[next] = 0;

    const int root = add_node(next, -1, -1, file_node_folder, FILE_ARCHIVE_ROOT_FOLDER, s_root_path);
    if (root >= 0) {
        s_pool[next][root].is_open = root_was_open;
        scan_dir(next, root, 0);
    }

    s_cur = next;
    s_selected = selected_path[0] ? find_by_path(s_cur, selected_path) : -1;
}

/* ------------------------------------------------------------------------- */
/* read + select                                                              */
/* ------------------------------------------------------------------------- */
int file_archive_node_count(void) { return s_count[s_cur]; }

const file_archive_node* file_archive_node_at(int index)
{
    return (index >= 0 && index < s_count[s_cur]) ? &s_pool[s_cur][index] : NULL;
}

int  file_archive_selected(void) { return (s_selected >= 0 && s_selected < s_count[s_cur]) ? s_selected : -1; }

void file_archive_select(int index)
{
    s_selected = (index >= 0 && index < s_count[s_cur]) ? index : -1;
}

void file_archive_toggle_open(int index)
{
    if (index < 0 || index >= s_count[s_cur]) return;
    file_archive_node* n = &s_pool[s_cur][index];
    if (n->kind == file_node_folder) n->is_open = !n->is_open;
}

void file_archive_set_all_open(bool open)
{
    for (int i = 1; i < s_count[s_cur]; i++)               /* 0 = root, stays open */
        if (s_pool[s_cur][i].kind == file_node_folder) s_pool[s_cur][i].is_open = open;
}

/* ------------------------------------------------------------------------- */
/* create                                                                     */
/* ------------------------------------------------------------------------- */
/* where new things go: the obj_vert_selected folder, else the folder of the obj_vert_selected file, else the root */
static int target_folder_index(void)
{
    const int sel = file_archive_selected();
    if (sel < 0) return 0;
    const file_archive_node* n = &s_pool[s_cur][sel];
    if (n->kind == file_node_folder) return sel;
    return n->parent >= 0 ? n->parent : 0;
}

static bool make_unique_path(const char* dir, const char* base, const char* ext, char* out, size_t cap)
{
    for (int n = 1; n < 1000; n++) {
        int len;
        if (n == 1) len = snprintf(out, cap, "%s\\%s%s",    dir, base, ext);
        else        len = snprintf(out, cap, "%s\\%s %d%s", dir, base, n, ext);
        if (len < 0 || len >= (int)cap) return false;
        if (GetFileAttributesA(out) == INVALID_FILE_ATTRIBUTES) return true;
    }
    return false;
}

static int create_entry(const char* base, const char* ext, bool folder)
{
    file_archive_start();

    const int dir = target_folder_index();
    char path[file_archive_path_max];
    if (!make_unique_path(s_pool[s_cur][dir].path, base, ext ? ext : "", path, sizeof(path))) {
        phoenix_info("FileArchive", "could not find a free name for '%s'", base);
        return -1;
    }

    bool ok;
    if (folder) {
        ok = CreateDirectoryA(path, NULL) != 0;
    } else {
        HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        ok = (h != INVALID_HANDLE_VALUE);
        if (ok) CloseHandle(h);
    }
    if (!ok) {
        phoenix_info("FileArchive", "create failed (error %lu): %s", (unsigned long)GetLastError(), path);
        return -1;
    }

    s_pool[s_cur][dir].is_open = true;                      /* so the new entry is visible */
    file_archive_refresh();

    const int idx = find_by_path(s_cur, path);
    s_selected = idx;
    phoenix_info("FileArchive", "created %s", path);
    return idx;
}

int file_archive_create_folder(void)                                 { return create_entry("New Folder", "", true); }
int file_archive_create_file(const char* base_name, const char* ext) { return create_entry(base_name ? base_name : "New File", ext, false); }

/* ------------------------------------------------------------------------- */
/* delete + reveal                                                            */
/* ------------------------------------------------------------------------- */
void file_archive_delete_selected(void)
{
    const int sel = file_archive_selected();
    if (sel <= 0) return;                                   /* nothing obj_vert_selected, or the root */

    char path[file_archive_path_max];
    char parent_path[file_archive_path_max] = "";
    snprintf(path, sizeof(path), "%s", s_pool[s_cur][sel].path);
    if (!path_is_inside_root(path)) { phoenix_info("FileArchive", "refused to delete outside the root: %s", path); return; }

    const int parent = s_pool[s_cur][sel].parent;
    if (parent >= 0) snprintf(parent_path, sizeof(parent_path), "%s", s_pool[s_cur][parent].path);

    /* SHFileOperation wants the path list double-null terminated. the buffer is zeroed and one char shorter than the path max. */
    char from[file_archive_path_max + 2];
    memset(from, 0, sizeof(from));
    snprintf(from, sizeof(from) - 1, "%s", path);

    SHFILEOPSTRUCTA op;
    memset(&op, 0, sizeof(op));
    op.wFunc  = FO_DELETE;
    op.pFrom  = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;   /* ALLOWUNDO = recycle bin */
    const int result = SHFileOperationA(&op);
    if (result != 0 || op.fAnyOperationsAborted) phoenix_info("FileArchive", "delete failed (code %d): %s", result, path);
    else                                         phoenix_info("FileArchive", "deleted %s", path);

    s_selected = -1;
    file_archive_refresh();
    if (parent_path[0]) s_selected = find_by_path(s_cur, parent_path);   /* land on the folder it was in */
}

void file_archive_reveal_selected(void)
{
    file_archive_start();
    const int sel = file_archive_selected();
    const file_archive_node* n = &s_pool[s_cur][sel >= 0 ? sel : 0];

    if (n->kind == file_node_file) {
        char args[file_archive_path_max + 16];
        snprintf(args, sizeof(args), "/select,\"%s\"", n->path);
        ShellExecuteA(NULL, "open", "explorer.exe", args, NULL, SW_SHOWNORMAL);
    } else {
        ShellExecuteA(NULL, "open", n->path, NULL, NULL, SW_SHOWNORMAL);
    }
}

/* ------------------------------------------------------------------------- */
/* wheel: the window reads and clears this in its draw                        */
/* ------------------------------------------------------------------------- */
void file_archive_notify_wheel(int wheel_delta) { s_wheel_pending += wheel_delta; }

/* the draw file asks for it through this (kept out of the header: only the window uses it) */
int file_archive_take_wheel(void);
int file_archive_take_wheel(void) { const int vert_to_edge_array = s_wheel_pending; s_wheel_pending = 0; return vert_to_edge_array; }