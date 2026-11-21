#include "system_installer.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* see system_installer.h */

void make_installer_folders(const char* file_path)
{
    char path[installer_path_size];
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

bool copy_installer_file(const char* from_path, const char* to_path)
{
    make_installer_folders(to_path);
    return CopyFileA(from_path, to_path, FALSE) != 0;
}

bool write_installer_uninstall_script(const installer_job* job)
{
    char path[installer_path_size];
    snprintf(path, sizeof(path), "%s\\uninstall.bat", job->install_folder);
    FILE* file = fopen(path, "w");
    if (!file) {
        return false;
    }
    fprintf(file, "@echo off\n");
    fprintf(file, "echo Removing %s\n", job->install_folder);
    fprintf(file, "cd /d \"%%TEMP%%\"\n");
    fprintf(file, "rmdir /s /q \"%s\"\n", job->install_folder);
    fclose(file);
    return true;
}

bool run_installer_job(installer_job* job)
{
    char manifest_path[installer_path_size];
    snprintf(manifest_path, sizeof(manifest_path), "%s\\phoenix_manifest.txt", job->game_folder);
    FILE* manifest = fopen(manifest_path, "r");
    if (!manifest) {
        printf("installer: no phoenix_manifest.txt in %s. Run phoenix_build.exe first.\n", job->game_folder);
        return false;
    }

    char line[installer_path_size];
    while (fgets(line, (int)sizeof(line), manifest) != NULL) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (length == 0 || strncmp(line, "PHOENIX_BUILD", 13) == 0) {
            continue;
        }
        char from_path[installer_path_size];
        char to_path[installer_path_size];
        snprintf(from_path, sizeof(from_path), "%s\\%s", job->game_folder, line);
        snprintf(to_path, sizeof(to_path), "%s\\%s", job->install_folder, line);
        if (copy_installer_file(from_path, to_path)) {
            job->copied_count++;
        } else {
            job->failed_count++;
            printf("installer: could not copy %s\n", from_path);
        }
    }
    fclose(manifest);

    write_installer_uninstall_script(job);
    printf("installer: %d files installed, %d failed -> %s\n", job->copied_count, job->failed_count, job->install_folder);
    return job->failed_count == 0;
}

int main(int argument_count, char** arguments)
{
    installer_job job;
    memset(&job, 0, sizeof(job));

    if (argument_count >= 2) {
        snprintf(job.game_folder, sizeof(job.game_folder), "%s", arguments[1]);
    } else {
        DWORD length = GetModuleFileNameA(NULL, job.game_folder, (DWORD)sizeof(job.game_folder));
        if (length == 0 || length >= (DWORD)sizeof(job.game_folder)) {
            printf("installer: could not find the folder phoenix_installer.exe is in.\n");
            return 1;
        }
        char* cut = strrchr(job.game_folder, '\\');
        if (cut) {
            *cut = '\0';
        }
    }

    if (argument_count >= 3) {
        snprintf(job.install_folder, sizeof(job.install_folder), "%s", arguments[2]);
    } else {
        char local_app_data[installer_path_size];
        DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
        if (length == 0 || length >= (DWORD)sizeof(local_app_data)) {
            printf("installer: LOCALAPPDATA is not set, pass an install folder.\n");
            return 2;
        }
        snprintf(job.install_folder, sizeof(job.install_folder), "%s\\phoenix_game", local_app_data);
    }

    if (run_installer_job(&job)) {
        return 0;
    }
    return 1;
}
