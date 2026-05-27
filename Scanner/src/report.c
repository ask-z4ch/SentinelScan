#include "report.h"
#include "entropy.h"
#include <stdio.h>

static const char* classify(double entropy)
{
    if (entropy < 0.0) return "Error";
    if (entropy < 6.5) return "Normal";
    if (entropy <= 7.2) return "Moderate";
    return "Suspicious";
}

void report_console(const FileList *list)
{
    int warn_count = 0;

    printf("\n=== SentinelScan Report ===\n");
    printf("%-30s %-10s %-10s %s\n",
        "Filename", "Size", "Entropy", "Status");
    printf("-------------------------------------"
        "-----------------------\n");

    for (int i = 0; i < list->count; i++) {
        FileInfo *fi = &list->files[i];
        double ent = entropy_calculate(fi->filepath);
        const char *status = classify(ent);

        printf("%-30s %-10lu %-10.2f %s\n",
            fi->filename, fi->size, ent, status);
        if (ent >= 6.5) warn_count++;
    }

    printf("-------------------------------------"
        "-----------------------\n");
    printf("[INFO] Total files found: %d\n",
        list->count);
    printf("[INFO] Warnings: %d\n\n", warn_count);

    for (int i = 0; i < list->count; i++) {
        FileInfo *fi = &list->files[i];
        double ent = entropy_calculate(fi->filepath);
        const char *status = classify(ent);

        if (ent >= 6.5) {
            printf("[WARN] %s\n", fi->filename);
        } else {
            printf("[INFO] %s\n", fi->filename);
        }
        printf("       Path: %s\n", fi->filepath);
        printf("       Size: %lu bytes\n", fi->size);
        printf("       Entropy: %.2f (%s)\n",
            ent, status);
    }
}

int report_csv(const FileList *list, const char *output_path)
{
    FILE *fp = fopen(output_path, "w");
    if (fp == NULL) return -1;

    fprintf(fp, "Filename,Path,Extension,Size,Entropy,Status\n");

    for (int i = 0; i < list->count; i++) {
        FileInfo *fi = &list->files[i];
        double ent = entropy_calculate(fi->filepath);
        const char *status = classify(ent);
        fprintf(fp, "%s,%s,%s,%lu,%.2f,%s\n",
            fi->filename,
            fi->filepath,
            fi->extension,
            fi->size,
            ent,
            status);
    }

    fclose(fp);
    return 0;
}
