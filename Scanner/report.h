#ifndef REPORT_H
#define REPORT_H

#include "scanner.h"

void report_console(const FileList *list);
int report_csv(const FileList *list, const char *output_path);

#endif
