#include "scanner.h"
#include <stdlib.h>

static
INT
IsTargetExtension(
    _In_ PCSTR FileName
    )
{
    UNREFERENCED_PARAMETER(FileName);
    return 0;
}

static
INT
GrowList(
    _Inout_ PFILE_LIST List
    )
{
    UNREFERENCED_PARAMETER(List);
    return -1;
}

_Check_return_
INT
ScannerInit(
    _Out_ PFILE_LIST List
    )
{
    if (List == NULL) return -1;
    List->Files = NULL;
    List->Count = 0;
    List->Capacity = 0;
    return -1;
}

VOID
ScannerFree(
    _Inout_ PFILE_LIST List
    )
{
    if (List == NULL) return;
    if (List->Files != NULL) {
        free(List->Files);
        List->Files = NULL;
    }
    List->Count = 0;
    List->Capacity = 0;
}

_Check_return_
INT
ScannerScan(
    _In_ PCSTR RootPath,
    _Inout_ PFILE_LIST List
    )
{
    UNREFERENCED_PARAMETER(RootPath);
    UNREFERENCED_PARAMETER(List);
    return -1;
}
