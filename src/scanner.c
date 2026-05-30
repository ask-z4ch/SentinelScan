#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define STACK_INITIAL 64

typedef struct {
    WCHAR **Entries;
    INT Count;
    INT Capacity;
} DIRSTACK;

static BOOL is_target_ext(const WCHAR *Name)
{
    const WCHAR *Dot = wcsrchr(Name, L'.');
    if (Dot == NULL) return FALSE;
    if (_wcsicmp(Dot, L".exe") == 0) return TRUE;
    if (_wcsicmp(Dot, L".dll") == 0) return TRUE;
    if (_wcsicmp(Dot, L".msi") == 0) return TRUE;
    return FALSE;
}

static BOOL stack_push(DIRSTACK *S, const WCHAR *Path)
{
    if (S->Count >= S->Capacity) {
        INT NewCap = S->Capacity == 0 ? STACK_INITIAL : S->Capacity * 2;
        WCHAR **Tmp = (WCHAR**)realloc(S->Entries,
            (size_t)NewCap * sizeof(WCHAR*));
        if (Tmp == NULL) return FALSE;
        S->Entries = Tmp;
        S->Capacity = NewCap;
    }
    size_t Len = wcslen(Path) + 1;
    S->Entries[S->Count] = (WCHAR*)malloc(Len * sizeof(WCHAR));
    if (S->Entries[S->Count] == NULL) return FALSE;
    wcscpy(S->Entries[S->Count], Path);
    S->Count++;
    return TRUE;
}

static BOOL stack_pop(DIRSTACK *S, WCHAR *Buffer, INT BufSize)
{
    if (S->Count == 0) return FALSE;
    S->Count--;
    wcsncpy(Buffer, S->Entries[S->Count], BufSize - 1);
    Buffer[BufSize - 1] = L'\0';
    free(S->Entries[S->Count]);
    S->Entries[S->Count] = NULL;
    return TRUE;
}

static VOID stack_free(DIRSTACK *S)
{
    for (INT i = 0; i < S->Count; i++) {
        if (S->Entries[i]) free(S->Entries[i]);
    }
    if (S->Entries) free(S->Entries);
    S->Entries = NULL;
    S->Count = 0;
    S->Capacity = 0;
}

static BOOL grow_file_list(PFILE_LIST List)
{
    INT NewCap = List->Capacity == 0
        ? INITIAL_CAPACITY : List->Capacity * 2;
    PFILE_INFO Tmp = (PFILE_INFO)realloc(List->Files,
        (size_t)NewCap * sizeof(FILE_INFO));
    if (Tmp == NULL) return FALSE;
    List->Files = Tmp;
    List->Capacity = NewCap;
    return TRUE;
}

static BOOL collect_file(
    PFILE_LIST List,
    const WCHAR *DirPath,
    const WIN32_FIND_DATAW *Fd)
{
    if (List->Count >= List->Capacity) {
        if (!grow_file_list(List)) return FALSE;
    }
    PFILE_INFO Fi = &List->Files[List->Count];
    CHAR NameA[MAX_PATH];
    CHAR DirA[MAX_PATH];
    INT Rc;
    Rc = WideCharToMultiByte(CP_UTF8, 0,
        Fd->cFileName, -1, NameA, MAX_PATH, NULL, NULL);
    if (Rc == 0) return FALSE;
    Rc = WideCharToMultiByte(CP_UTF8, 0,
        DirPath, -1, DirA, MAX_PATH, NULL, NULL);
    if (Rc == 0) return FALSE;
    strncpy(Fi->FileName, NameA, MAX_PATH - 1);
    Fi->FileName[MAX_PATH - 1] = '\0';
    snprintf(Fi->FilePath, MAX_PATH, "%s\\%s", DirA, NameA);
    const WCHAR *Dot = wcsrchr(Fd->cFileName, L'.');
    if (Dot != NULL) {
        CHAR ExtA[MAX_EXTENSION];
        Rc = WideCharToMultiByte(CP_UTF8, 0,
            Dot, -1, ExtA, MAX_EXTENSION, NULL, NULL);
        if (Rc != 0) {
            strncpy(Fi->Extension, ExtA, MAX_EXTENSION - 1);
            Fi->Extension[MAX_EXTENSION - 1] = '\0';
        } else {
            Fi->Extension[0] = '\0';
        }
    } else {
        Fi->Extension[0] = '\0';
    }
    Fi->Size = Fd->nFileSizeLow;
    List->Count++;
    return TRUE;
}

INT ScannerInit(PFILE_LIST List)
{
    if (List == NULL) return -1;
    List->Files = (PFILE_INFO)malloc(
        (size_t)INITIAL_CAPACITY * sizeof(FILE_INFO));
    if (List->Files == NULL) return -1;
    List->Count = 0;
    List->Capacity = INITIAL_CAPACITY;
    return 0;
}

VOID ScannerFree(PFILE_LIST List)
{
    if (List == NULL) return;
    if (List->Files != NULL) {
        free(List->Files);
        List->Files = NULL;
    }
    List->Count = 0;
    List->Capacity = 0;
}

INT ScannerScan(PCSTR RootPath, PFILE_LIST List)
{
    WCHAR WideRoot[MAX_PATH];
    DIRSTACK Stack;
    WCHAR Current[MAX_PATH];
    INT Rc;

    if (List == NULL) return -1;
    if (RootPath == NULL) return -1;
    Rc = MultiByteToWideChar(CP_UTF8, 0,
        RootPath, -1, WideRoot, MAX_PATH);
    if (Rc == 0) return -1;

    Stack.Entries = NULL;
    Stack.Count = 0;
    Stack.Capacity = 0;
    if (!stack_push(&Stack, WideRoot)) {
        return -1;
    }
    while (stack_pop(&Stack, Current, MAX_PATH)) {
        WCHAR Search[MAX_PATH];
        WIN32_FIND_DATAW Fd;
        HANDLE H;

        swprintf(Search, MAX_PATH, L"%s\\*", Current);
        H = FindFirstFileExW(Search,
            FindExInfoBasic,
            &Fd,
            FindExSearchNameMatch,
            NULL,
            FIND_FIRST_EX_LARGE_FETCH);
        if (H == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            if (wcscmp(Fd.cFileName, L".") == 0 ||
                wcscmp(Fd.cFileName, L"..") == 0) {
                continue;
            }
            if (Fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                WCHAR Sub[MAX_PATH];
                swprintf(Sub, MAX_PATH,
                    L"%s\\%s", Current, Fd.cFileName);
                stack_push(&Stack, Sub);
            } else {
                if (!is_target_ext(Fd.cFileName)) continue;
                collect_file(List, Current, &Fd);
            }
        } while (FindNextFileW(H, &Fd) != 0);
        FindClose(H);
    }
    stack_free(&Stack);
    return 0;
}
