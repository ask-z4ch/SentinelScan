#pragma once

#include <Windows.h>

#define MAX_EXTENSION   8
#define INITIAL_CAPACITY 1024

typedef struct _FILE_INFO {
    CHAR  FileName[MAX_PATH];
    CHAR  FilePath[MAX_PATH];
    CHAR  Extension[MAX_EXTENSION];
    DWORD Size;
} FILE_INFO, *PFILE_INFO;

typedef struct _FILE_LIST {
    PFILE_INFO Files;
    INT        Count;
    INT        Capacity;
} FILE_LIST, *PFILE_LIST;

_Check_return_
INT
ScannerInit(
    _Out_ PFILE_LIST List
    );

VOID
ScannerFree(
    _Inout_ PFILE_LIST List
    );

_Check_return_
INT
ScannerScan(
    _In_ PCSTR RootPath,
    _Inout_ PFILE_LIST List
    );
