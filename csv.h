#ifndef CSV_H
#define CSV_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    COL_NULL = 0,
    COL_INT,
    COL_DOUBLE,
    COL_STRING
} ColType;

typedef struct {
    char *text;
    int is_null;
} CsvField;

typedef struct {
    int64_t imin;
    int64_t imax;
    double imean;
    size_t positive;
    size_t negative;
    size_t even;
    size_t inonnull;
    int i_has;
    double dmin;
    double dmax;
    double dmean;
    size_t d_positive;
    size_t d_negative;
    size_t d_integer;
    size_t dnonnull;
    int d_has;
    size_t min_len;
    size_t max_len;
    double mean_len;
    size_t empty;
    size_t snonnull;
    int s_has;

    size_t nulls;
} ColStats;

typedef struct {
    size_t nrows;
    size_t ncols;
    char **names;
    ColType *types;
    ColStats *stats;
} CsvSchema;

void csv_schema_free(CsvSchema *schema);

int csv_analyze(const char *path, CsvSchema *out);

ColType csv_classify_text(const char *text);

const char *csv_type_name(ColType t);

void csv_fprint_quoted(FILE *fp, const char *s);

#endif 
