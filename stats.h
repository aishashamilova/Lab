#ifndef STATS_H
#define STATS_H

#include "csv.h"
#include <stdio.h>

void print_schema(const CsvSchema *schema);
void print_stats(const CsvSchema *schema);
int write_report(const CsvSchema *schema, const char *path);
void fprint_stats(FILE *fp, const CsvSchema *schema);
#endif
