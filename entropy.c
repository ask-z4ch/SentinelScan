#include "entropy.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define CHUNK_SIZE 65536

double entropy_calculate(const char *filepath)
{
    unsigned long long freq[256];
    unsigned long long total = 0;
    unsigned char buffer[CHUNK_SIZE];
    size_t bytes_read;
    double entropy = 0.0;
    FILE *fp;

    memset(freq, 0, sizeof(freq));

    fp = fopen(filepath, "rb");
    if (fp == NULL) return -1.0;

    while ((bytes_read = fread(buffer, 1, CHUNK_SIZE, fp)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            freq[buffer[i]]++;
        }
        total += bytes_read;
    }

    fclose(fp);
    if (total == 0) return 0.0;

    for (int i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        double p = (double)freq[i] / (double)total;
        entropy -= p * log2(p);
    }

    return entropy;
}
