#include "scanner.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int has_target_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) return 0;
    if (strcmp(dot, ".exe") == 0) return 1;
    if (strcmp(dot, ".dll") == 0) return 1;
    if (strcmp(dot, ".msi") == 0) return 1;
    return 0;
}

int scanner_init(FileList *list)
{
    list->files = (FileInfo*)malloc(INITIAL_CAPACITY * sizeof(FileInfo));
    if (list->files == NULL) return -1;
    list->count = 0;
    list->capacity = INITIAL_CAPACITY;
    return 0;
}

void scanner_free(FileList *list)
{
    if (list->files) {
        free(list->files);
        list->files = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

static int grow_list(FileList *list)
{
    int newcap = list->capacity * 2;
    FileInfo *tmp = (FileInfo*)realloc(list->files,
        newcap * sizeof(FileInfo));
    if (tmp == NULL) return -1;
    list->files = tmp;
    list->capacity = newcap;
    return 0;
}

int scanner_scan(const char *root, FileList *list)
{
    WIN32_FIND_DATAA fdata;
    char search_path[MAX_PATH];
    char subdir[MAX_PATH];
    HANDLE hFind;

    snprintf(search_path, MAX_PATH, "%s\\*", root);
    hFind = FindFirstFileA(search_path, &fdata);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (strcmp(fdata.cFileName, ".") == 0 ||
            strcmp(fdata.cFileName, "..") == 0) continue;

        if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            snprintf(subdir, MAX_PATH, "%s\\%s",
                root, fdata.cFileName);
            scanner_scan(subdir, list);
        } else {
            if (!has_target_extension(fdata.cFileName)) continue;

            if (list->count >= list->capacity) {
                if (grow_list(list) != 0) continue;
            }

            FileInfo *fi = &list->files[list->count];
            strncpy(fi->filename, fdata.cFileName,
                MAX_PATH - 1);
            fi->filename[MAX_PATH - 1] = '\0';

            snprintf(fi->filepath, MAX_PATH, "%s\\%s",
                root, fdata.cFileName);

            const char *dot = strrchr(fdata.cFileName, '.');
            if (dot) {
                strncpy(fi->extension, dot, MAX_EXTENSION - 1);
                fi->extension[MAX_EXTENSION - 1] = '\0';
            } else {
                fi->extension[0] = '\0';
            }

            fi->size = fdata.nFileSizeLow;
            list->count++;
        }
    } while (FindNextFileA(hFind, &fdata) != 0);

    FindClose(hFind);
    return 0;
}
