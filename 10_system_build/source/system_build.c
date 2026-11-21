#include "system_build.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* see system_build.h: the editor's 01_bin -> a game folder */

bool is_build_file_excluded(const char* file_name)
{
    const char* excluded[] = {
        "phoenix_editor.dll",
        "phoenix_editor.pdb",
        "phoenix_editor.pdb.tmp",
        "phoenix_build.exe",
        "phoenix_installer.exe",
        "phoenix_manifest.txt"
    };
    const int excluded_count = (int)(sizeof(excluded) / sizeof(excluded[0]));
    for (int i = 0; i < excluded_count; i++) {
        if (_stricmp(file_name, excluded[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* makes every folder in the path of a file (a\b\c.txt makes a and a\b) */
void make_build_folders(const char* file_path)
{
    char path[build_path_size];
    snprintf(path, sizeof(path), "%s", file_path);
    for (char* p = path + 1; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            char saved = *p;
            *p = '\0';
            CreateDirectoryA(path, NULL);
            *p = saved;
        }
    }
}

bool copy_build_file(const char* from_path, const char* to_path)
{
    make_build_folders(to_path);
    return CopyFileA(from_path, to_path, FALSE) != 0;
}

bool copy_build_folder(build_job* job, const char* from_folder, const char* to_folder, const char* relative_folder, void* manifest_file)
{
    char pattern[build_path_size];
    snprintf(pattern, sizeof(pattern), "%s\\*", from_folder);

    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    FILE* manifest = (FILE*)manifest_file;

    do {
        if (strcmp(found.cFileName, ".") == 0 || strcmp(found.cFileName, "..") == 0) {
            continue;
        }
        char from_path[build_path_size];
        char to_path[build_path_size];
        char relative_path[build_path_size];
        snprintf(from_path, sizeof(from_path), "%s\\%s", from_folder, found.cFileName);
        snprintf(to_path, sizeof(to_path), "%s\\%s", to_folder, found.cFileName);
        if (relative_folder[0] == '\0') {
            snprintf(relative_path, sizeof(relative_path), "%s", found.cFileName);
        } else {
            snprintf(relative_path, sizeof(relative_path), "%s\\%s", relative_folder, found.cFileName);
        }

        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            copy_build_folder(job, from_path, to_path, relative_path, manifest_file);
        } else if (copy_build_file(from_path, to_path)) {
            job->copied_count++;
            if (manifest) {
                fprintf(manifest, "%s\n", relative_path);
            }
        } else {
            job->failed_count++;
            printf("build: could not copy %s\n", from_path);
        }
    } while (FindNextFileA(handle, &found) != 0);

    FindClose(handle);
    return true;
}

bool run_build_job(build_job* job)
{
    char manifest_path[build_path_size];
    snprintf(manifest_path, sizeof(manifest_path), "%s\\phoenix_manifest.txt", job->output_folder);
    make_build_folders(manifest_path);

    FILE* manifest = fopen(manifest_path, "w");
    if (!manifest) {
        printf("build: could not write %s\n", manifest_path);
        return false;
    }
    fprintf(manifest, "PHOENIX_BUILD 1\n");

    /* the runtime files sitting directly in the source folder */
    char pattern[build_path_size];
    snprintf(pattern, sizeof(pattern), "%s\\*", job->source_folder);
    WIN32_FIND_DATAA found;
    HANDLE handle = FindFirstFileA(pattern, &found);
    if (handle == INVALID_HANDLE_VALUE) {
        printf("build: source folder not found: %s\n", job->source_folder);
        fclose(manifest);
        return false;
    }
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (is_build_file_excluded(found.cFileName)) {
            job->skipped_count++;
            continue;
        }
        char from_path[build_path_size];
        char to_path[build_path_size];
        snprintf(from_path, sizeof(from_path), "%s\\%s", job->source_folder, found.cFileName);
        snprintf(to_path, sizeof(to_path), "%s\\%s", job->output_folder, found.cFileName);
        if (copy_build_file(from_path, to_path)) {
            job->copied_count++;
            fprintf(manifest, "%s\n", found.cFileName);
        } else {
            job->failed_count++;
            printf("build: could not copy %s\n", from_path);
        }
    } while (FindNextFileA(handle, &found) != 0);
    FindClose(handle);

    /* the project assets (File Archive: models, textures, skins) */
    char assets_from[build_path_size];
    char assets_to[build_path_size];
    snprintf(assets_from, sizeof(assets_from), "%s\\assets", job->source_folder);
    snprintf(assets_to, sizeof(assets_to), "%s\\assets", job->output_folder);
    if (GetFileAttributesA(assets_from) != INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(assets_to, NULL);
        copy_build_folder(job, assets_from, assets_to, "assets", manifest);
    }

    fclose(manifest);
    printf("build: %d files copied, %d failed, %d left out (editor and settings) -> %s\n",
           job->copied_count, job->failed_count, job->skipped_count, job->output_folder);
    return job->failed_count == 0;
}

int main(int argument_count, char** arguments)
{
    build_job job;
    memset(&job, 0, sizeof(job));

    if (argument_count < 2) {
        printf("usage: phoenix_build.exe <output_folder> [source_folder]\n");
        return 2;
    }
    snprintf(job.output_folder, sizeof(job.output_folder), "%s", arguments[1]);
    if (argument_count >= 3) {
        snprintf(job.source_folder, sizeof(job.source_folder), "%s", arguments[2]);
    } else {
        DWORD length = GetModuleFileNameA(NULL, job.source_folder, (DWORD)sizeof(job.source_folder));
        if (length == 0 || length >= (DWORD)sizeof(job.source_folder)) {
            printf("build: could not find the folder phoenix_build.exe is in.\n");
            return 1;
        }
        char* cut = strrchr(job.source_folder, '\\');
        if (cut) {
            *cut = '\0';
        }
    }

    if (run_build_job(&job)) {
        return 0;
    }
    return 1;
}
