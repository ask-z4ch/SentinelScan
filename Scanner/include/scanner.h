#ifndef SCANNER_H
#define SCANNER_H

#include <windows.h>

#define MAX_EXTENSION 8
#define INITIAL_CAPACITY 1024

typedef struct {
    char filename[MAX_PATH];
    char filepath[MAX_PATH];
    char extension[MAX_EXTENSION];
    DWORD size;
} FileInfo;

typedef struct {
    FileInfo *files;
    int count;
    int capacity;
} FileList;

int scanner_init(FileList *list);
void scanner_free(FileList *list);
int scanner_scan(const char *root, FileList *list);

#endif
