#include <stdio.h>
#include "scanner.h"

int main(int argc, char *argv[])
{
    FILE_LIST list;

    if (argc > 1) {
        printf("[INFO] Target directory: %s\n", argv[1]);
    }

    if (ScannerInit(&list) != 0) {
        fprintf(stderr, "[ERROR] ScannerInit failed\n");
        return 1;
    }

    ScannerFree(&list);
    return 0;
}
