#include "system_globals.h"


enum { settings_max_entries = 256, settings_key_len = 48, settings_value_len = 160, settings_max_floats = 8 };

typedef struct { char key[settings_key_len]; char value[settings_value_len]; } settings_entry;

static settings_entry s_entries[settings_max_entries];
static int  s_count        = 0;
static bool s_started      = false;
static bool s_file_existed = false;
static char s_dir[MAX_PATH]  = "";
static char s_path[MAX_PATH] = "";

/* ------------------------------------------------------------------------- */
/* table                                                                      */
/* ------------------------------------------------------------------------- */
static settings_entry* find_entry(const char* key)
{
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_entries[i].key, key) == 0) return &s_entries[i];
    return NULL;
}

static bool put_value(const char* key, const char* value)
{
    if (!key || !key[0] || !value) return false;
    if (strlen(key) >= settings_key_len || strlen(value) >= settings_value_len) return false;
    if (strchr(key, '=') || strchr(key, '\n') || strchr(value, '\n')) return false;

    settings_entry* e = find_entry(key);
    if (!e) {
        if (s_count >= settings_max_entries) return false;
        e = &s_entries[s_count++];
        memcpy(e->key, key, strlen(key) + 1);
    }
    memcpy(e->value, value, strlen(value) + 1);
    return true;
}

/* ------------------------------------------------------------------------- */
/* location + file                                                            */
/* ------------------------------------------------------------------------- */
static void locate_exe_folder(void)
{
    DWORD n = GetModuleFileNameA(NULL, s_dir, (DWORD)sizeof(s_dir));
    if (n == 0 || n >= (DWORD)sizeof(s_dir)) {
        s_dir[0] = '\0';                               /* fall back to the working directory */
    } else {
        char* back = strrchr(s_dir, '\\');
        char* fwd  = strrchr(s_dir, '/');
        char* cut  = (fwd > back) ? fwd : back;
        if (cut) cut[1] = '\0'; else s_dir[0] = '\0';
    }
    snprintf(s_path, sizeof(s_path), "%s%s", s_dir, SETTINGS_DB_FILE_NAME);
}

static void load_file(void)
{
    FILE* f = fopen(s_path, "r");
    if (!f) return;
    s_file_existed = true;

    char line[settings_key_len + settings_value_len + 8];
    while (fgets(line, (int)sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#' || strncmp(line, "PHOENIX_DB", 10) == 0) continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        put_value(line, eq + 1);                       /* unknown keys are kept, so they survive the next save */
    }
    fclose(f);
}

void settings_db_start(void)
{
    if (s_started) return;
    s_started = true;
    locate_exe_folder();
    load_file();
}

void settings_db_reload(void)
{
    settings_db_start();   /* makes sure the path is known */
    load_file();           /* put_value overwrites existing keys, so this refreshes them */
}

bool settings_db_save(void)
{
    settings_db_start();

    char tmp[MAX_PATH + 8];
    snprintf(tmp, sizeof(tmp), "%s.tmp", s_path);

    FILE* f = fopen(tmp, "w");
    if (!f) return false;

    fprintf(f, "PHOENIX_DB 1\n");
    for (int i = 0; i < s_count; i++) fprintf(f, "%s=%s\n", s_entries[i].key, s_entries[i].value);

    if (fclose(f) != 0) { remove(tmp); return false; }
    if (!MoveFileExA(tmp, s_path, MOVEFILE_REPLACE_EXISTING)) { remove(tmp); return false; }

    s_file_existed = true;
    return true;
}

bool        settings_db_file_existed(void) { settings_db_start(); return s_file_existed; }
const char* settings_db_directory(void)    { settings_db_start(); return s_dir; }
const char* settings_db_file_path(void)    { settings_db_start(); return s_path; }

bool settings_db_make_path(const char* file_name, char* out, size_t out_size)
{
    settings_db_start();
    if (!file_name || !out || out_size == 0) return false;
    int n = snprintf(out, out_size, "%s%s", s_dir, file_name);
    return n > 0 && (size_t)n < out_size;
}

/* ------------------------------------------------------------------------- */
/* get                                                                        */
/* ------------------------------------------------------------------------- */
bool settings_db_get_string(const char* key, char* out, size_t out_size)
{
    settings_db_start();
    settings_entry* e = key ? find_entry(key) : NULL;
    if (!e || !out || out_size == 0) return false;
    size_t len = strlen(e->value);
    if (len >= out_size) return false;
    memcpy(out, e->value, len + 1);
    return true;
}

bool settings_db_get_int(const char* key, int* out)
{
    settings_db_start();
    settings_entry* e = key ? find_entry(key) : NULL;
    if (!e || !out) return false;
    char* end = NULL;
    long vert_to_edge_array = strtol(e->value, &end, 10);
    if (end == e->value) return false;
    *out = (int)vert_to_edge_array;
    return true;
}

bool settings_db_get_float(const char* key, float* out)
{
    return settings_db_get_floats(key, out, 1);
}

bool settings_db_get_floats(const char* key, float* out, int face_type)
{
    settings_db_start();
    settings_entry* e = key ? find_entry(key) : NULL;
    if (!e || !out || face_type < 1 || face_type > settings_max_floats) return false;

    float parsed[settings_max_floats];
    const char* p = e->value;
    for (int i = 0; i < face_type; i++) {
        char* end = NULL;
        parsed[i] = (float)strtod(p, &end);
        if (end == p) return false;
        p = end;
        if (i < face_type - 1) {
            if (*p != ',') return false;
            p++;
        }
    }
    for (int i = 0; i < face_type; i++) out[i] = parsed[i];
    return true;
}

/* ------------------------------------------------------------------------- */
/* set                                                                        */
/* ------------------------------------------------------------------------- */
bool settings_db_set_string(const char* key, const char* value)
{
    settings_db_start();
    return put_value(key, value);
}

bool settings_db_set_int(const char* key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return settings_db_set_string(key, buf);
}

bool settings_db_set_float(const char* key, float value)
{
    return settings_db_set_floats(key, &value, 1);
}

bool settings_db_set_floats(const char* key, const float* values, int face_type)
{
    if (!values || face_type < 1 || face_type > settings_max_floats) return false;
    char buf[settings_value_len];
    size_t used = 0;
    buf[0] = '\0';
    for (int i = 0; i < face_type; i++) {
        int n = snprintf(buf + used, sizeof(buf) - used, "%s%.5f", i ? "," : "", (double)values[i]);
        if (n < 0 || (size_t)n >= sizeof(buf) - used) return false;
        used += (size_t)n;
    }
    return settings_db_set_string(key, buf);
}