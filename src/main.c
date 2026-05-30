#include <stdio.h>
#include "scanner.h"

int main(int argc, char *argv[])
{
    PCSTR Root = argc > 1 ? argv[1] : ".";
    FILE_LIST List;

    if (ScannerInit(&List) != 0) {
        fprintf(stderr, "ScannerInit failed\n");
        return 1;
    }
    if (ScannerScan(Root, &List) != 0) {
        fprintf(stderr, "ScannerScan failed\n");
        ScannerFree(&List);
        return 1;
    }
    for (INT i = 0; i < List.Count; i++) {
        printf("%-30s %lu\n",
            List.Files[i].FileName, List.Files[i].Size);
    }
    printf("Files found: %d\n", List.Count);
    ScannerFree(&List);
    return 0;
}
