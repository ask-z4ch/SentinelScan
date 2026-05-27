#include <stdio.h>
#include "scanner.h"
#include "report.h"

int main(int argc, char *argv[])
{
    const char *root = ".";
    if (argc > 1) root = argv[1];

    FileList list;
    if (scanner_init(&list) != 0) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        return 1;
    }

    printf("[INFO] Scanning directory: %s\n", root);
    if (scanner_scan(root, &list) != 0) {
        fprintf(stderr, "[ERROR] Failed to scan directory\n");
        scanner_free(&list);
        return 1;
    }

    report_console(&list);

    if (report_csv(&list, "report.csv") != 0) {
        fprintf(stderr, "[ERROR] Could not write report.csv\n");
        scanner_free(&list);
        return 1;
    }

    printf("[INFO] Report saved to report.csv\n");
    scanner_free(&list);
    return 0;
}
