#include "stats.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static double norm0(double x)
{
    if (x == 0.0) {
        return 0.0;
    }
    return x;
}

static size_t str_len(const char *s)
{
    return s == NULL ? 0 : strlen(s);
}

void fprint_stats(FILE *fp, const CsvSchema *schema)
{
    size_t c;
    fprintf(fp, "rows %zu\n", schema->nrows);
    fprintf(fp, "columns %zu\n", schema->ncols);

    for (c = 0; c < schema->ncols; c++) {
        const ColStats *s = &schema->stats[c];
        ColType t = schema->types[c];
        fprintf(fp, "\n");
        fprintf(fp, "column %zu\n", c);
        fprintf(fp, "name ");
        csv_fprint_quoted(fp, schema->names[c]);
        fprintf(fp, "\n");
        fprintf(fp, "type %s\n", csv_type_name(t));

        if (t == COL_NULL) {
            fprintf(fp, "rows %zu\n", schema->nrows);
            continue;
        }

        fprintf(fp, "nulls %zu\n", s->nulls);
        if (t == COL_INT) {
            fprintf(fp, "min %" PRId64 "\n", s->imin);
            fprintf(fp, "max %" PRId64 "\n", s->imax);
            fprintf(fp, "mean %.6f\n", norm0(s->imean));
            fprintf(fp, "positive %zu\n", s->positive);
            fprintf(fp, "negative %zu\n", s->negative);
            fprintf(fp, "even %zu\n", s->even);
        } 
        else if (t == COL_DOUBLE) {
            fprintf(fp, "min %.6f\n", norm0(s->dmin));
            fprintf(fp, "max %.6f\n", norm0(s->dmax));
            fprintf(fp, "mean %.6f\n", norm0(s->dmean));
            fprintf(fp, "positive %zu\n", s->d_positive);
            fprintf(fp, "negative %zu\n", s->d_negative);
            fprintf(fp, "integer %zu\n", s->d_integer);
        } 
        else if (t == COL_STRING) {
            fprintf(fp, "min_length %zu\n", s->min_len);
            fprintf(fp, "max_length %zu\n", s->max_len);
            fprintf(fp, "mean_length %.6f\n", norm0(s->mean_len));
            fprintf(fp, "empty %zu\n", s->empty);
        }
    }
}

void print_schema(const CsvSchema *schema)
{
    size_t c;
    printf("rows %zu\n", schema->nrows);
    printf("columns %zu\n", schema->ncols);
    for (c = 0; c < schema->ncols; c++) {
        printf("%zu ", c);
        csv_fprint_quoted(stdout, schema->names[c]);
        printf(" %s\n", csv_type_name(schema->types[c]));
    }
}

void print_stats(const CsvSchema *schema)
{
    fprint_stats(stdout, schema);
}

static size_t max_sz(size_t a, size_t b)
{
    return a > b ? a : b;
}

int write_report(const CsvSchema *schema, const char *path)
{
    FILE *fp;
    size_t c;
    size_t w_idx = strlen("Idx");
    size_t w_name = strlen("Name");
    size_t w_type = strlen("Type");
    size_t w_nulls = strlen("Nulls");
    size_t w_extra = strlen("Details");
    char details[256];

    if (schema == NULL || path == NULL) {
        return 1;
    }

    for (c = 0; c < schema->ncols; c++) {
        char idxbuf[32];
        size_t nlen;
        size_t dlen;

        snprintf(idxbuf, sizeof(idxbuf), "%zu", c);
        w_idx = max_sz(w_idx, strlen(idxbuf));
        nlen = str_len(schema->names[c]);
        w_name = max_sz(w_name, nlen);
        w_type = max_sz(w_type, strlen(csv_type_name(schema->types[c])));

        {
            char nbuf[64];
            snprintf(nbuf, sizeof(nbuf), "%zu", schema->stats[c].nulls);
            w_nulls = max_sz(w_nulls, strlen(nbuf));
        }

        details[0] = '\0';
        if (schema->types[c] == COL_INT) {
            snprintf(details, sizeof(details),
                     "min=%" PRId64 " max=%" PRId64 " mean=%.6f +=%zu -=%zu even=%zu",
                     schema->stats[c].imin, schema->stats[c].imax,
                     norm0(schema->stats[c].imean),
                     schema->stats[c].positive, schema->stats[c].negative,
                     schema->stats[c].even);
        } 
        else if (schema->types[c] == COL_DOUBLE) {
            snprintf(details, sizeof(details),
                     "min=%.6f max=%.6f mean=%.6f +=%zu -=%zu int=%zu",
                     norm0(schema->stats[c].dmin), norm0(schema->stats[c].dmax),
                     norm0(schema->stats[c].dmean),
                     schema->stats[c].d_positive, schema->stats[c].d_negative,
                     schema->stats[c].d_integer);
        } 
        else if (schema->types[c] == COL_STRING) {
            snprintf(details, sizeof(details),
                     "min_len=%zu max_len=%zu mean_len=%.6f empty=%zu",
                     schema->stats[c].min_len, schema->stats[c].max_len,
                     norm0(schema->stats[c].mean_len), schema->stats[c].empty);
        } 
        else {
            snprintf(details, sizeof(details), "rows=%zu (all NULL)", schema->nrows);
        }
        dlen = strlen(details);
        w_extra = max_sz(w_extra, dlen);
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return 1;
    }

    fprintf(fp, "csvstat report\n");
    fprintf(fp, "==============\n");
    fprintf(fp, "rows: %zu\n", schema->nrows);
    fprintf(fp, "columns: %zu\n\n", schema->ncols);

    fprintf(fp, "| %-*s | %-*s | %-*s | %-*s | %-*s |\n",
            (int)w_idx, "Idx",
            (int)w_name, "Name",
            (int)w_type, "Type",
            (int)w_nulls, "Nulls",
            (int)w_extra, "Details");

    fprintf(fp, "|-");
    for (c = 0; c < w_idx; c++) fputc('-', fp);
    fprintf(fp, "-+-");
    for (c = 0; c < w_name; c++) fputc('-', fp);
    fprintf(fp, "-+-");
    for (c = 0; c < w_type; c++) fputc('-', fp);
    fprintf(fp, "-+-");
    for (c = 0; c < w_nulls; c++) fputc('-', fp);
    fprintf(fp, "-+-");
    for (c = 0; c < w_extra; c++) fputc('-', fp);
    fprintf(fp, "-|\n");

    for (c = 0; c < schema->ncols; c++) {
        details[0] = '\0';
        if (schema->types[c] == COL_INT) {
            snprintf(details, sizeof(details),
                     "min=%" PRId64 " max=%" PRId64 " mean=%.6f +=%zu -=%zu even=%zu",
                     schema->stats[c].imin, schema->stats[c].imax,
                     norm0(schema->stats[c].imean),
                     schema->stats[c].positive, schema->stats[c].negative,
                     schema->stats[c].even);
        } 
        else if (schema->types[c] == COL_DOUBLE) {
            snprintf(details, sizeof(details),
                     "min=%.6f max=%.6f mean=%.6f +=%zu -=%zu int=%zu",
                     norm0(schema->stats[c].dmin), norm0(schema->stats[c].dmax),
                     norm0(schema->stats[c].dmean),
                     schema->stats[c].d_positive, schema->stats[c].d_negative,
                     schema->stats[c].d_integer);
        }
        else if (schema->types[c] == COL_STRING) {
            snprintf(details, sizeof(details),
                     "min_len=%zu max_len=%zu mean_len=%.6f empty=%zu",
                     schema->stats[c].min_len, schema->stats[c].max_len,
                     norm0(schema->stats[c].mean_len), schema->stats[c].empty);
        } 
        else {
            snprintf(details, sizeof(details), "rows=%zu (all NULL)", schema->nrows);
        }

        fprintf(fp, "| %-*zu | %-*s | %-*s | %-*zu | %-*s |\n",
                (int)w_idx, c,
                (int)w_name, schema->names[c],
                (int)w_type, csv_type_name(schema->types[c]),
                (int)w_nulls,
                (schema->types[c] == COL_NULL) ? schema->nrows : schema->stats[c].nulls,
                (int)w_extra, details);
    }

    fprintf(fp, "\nPer-column statistics (same as `csvstat stats`):\n\n");
    fprint_stats(fp, schema);

    if (ferror(fp)) {
        fclose(fp);
        remove(path);
        return 1;
    }
    if (fclose(fp) != 0) {
        remove(path);
        return 1;
    }
    return 0;
}
