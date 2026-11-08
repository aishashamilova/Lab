#include "csv.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} DynBuf;

static int db_init(DynBuf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    return 0;
}

static void db_free(DynBuf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static int db_reserve(DynBuf *b, size_t need)
{
    char *n;
    size_t ncap;

    if (need <= b->cap) {
        return 0;
    }
    ncap = b->cap == 0 ? 64 : b->cap;
    while (ncap < need) {
        if (ncap > (SIZE_MAX / 2)) {
            return 1;
        }
        ncap *= 2;
    }
    n = (char *)realloc(b->data, ncap);
    if (n == NULL) {
        return 1;
    }
    b->data = n;
    b->cap = ncap;
    return 0;
}

static int db_append_char(DynBuf *b, char c)
{
    if (db_reserve(b, b->len + 2) != 0) {
        return 1;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return 0;
}

typedef struct {
    CsvField *items;
    size_t n;
    size_t cap;
} FieldList;

static void field_clear(CsvField *f)
{
    free(f->text);
    f->text = NULL;
    f->is_null = 0;
}

static void fields_free(FieldList *fl)
{
    size_t i;

    if (fl->items != NULL) {
        for (i = 0; i < fl->n; i++) {
            field_clear(&fl->items[i]);
        }
        free(fl->items);
    }
    fl->items = NULL;
    fl->n = 0;
    fl->cap = 0;
}

static int fields_push(FieldList *fl, char *text, int is_null)
{
    CsvField *ni;

    if (fl->n >= fl->cap) {
        size_t ncap = fl->cap == 0 ? 8 : fl->cap * 2;
        ni = (CsvField *)realloc(fl->items, ncap * sizeof(CsvField));
        if (ni == NULL) {
            free(text);
            return 1;
        }
        fl->items = ni;
        fl->cap = ncap;
    }
    fl->items[fl->n].text = text;
    fl->items[fl->n].is_null = is_null;
    fl->n++;
    return 0;
}

static int is_space_tab(char c)
{
    return c == ' ' || c == '\t';
}

static int parse_record(const char *line, FieldList *out)
{
    size_t i = 0;
    size_t n;

    fields_free(out);
    out->items = NULL;
    out->n = 0;
    out->cap = 0;

    if (line == NULL) {
        return 1;
    }
    n = strlen(line);

    if (n == 0) {
        char *empty = (char *)malloc(1);
        if (empty == NULL) {
            return 1;
        }
        empty[0] = '\0';
        return fields_push(out, empty, 1);
    }

    while (1) {
        while (i < n && is_space_tab(line[i])) {
            i++;
        }

        if (i < n && line[i] == '"') {
            DynBuf buf;
            char *owned;
            i++;
            db_init(&buf);
            while (1) {
                if (i >= n) {
                    db_free(&buf);
                    return 1;
                }
                if (line[i] == '"') {
                    if (i + 1 < n && line[i + 1] == '"') {
                        if (db_append_char(&buf, '"') != 0) {
                            db_free(&buf);
                            return 1;
                        }
                        i += 2;
                    } 
                    else {
                        i++;
                        break;
                    }
                } 
                else {
                    if (db_append_char(&buf, line[i]) != 0) {
                        db_free(&buf);
                        return 1;
                    }
                    i++;
                }
            }
            while (i < n && is_space_tab(line[i])) {
                i++;
            }
            if (i < n && line[i] != ',') {
                db_free(&buf);
                return 1;
            }
            if (buf.data == NULL) {
                owned = (char *)malloc(1);
                if (owned == NULL) {
                    return 1;
                }
                owned[0] = '\0';
            } 
            else {
                owned = buf.data;
                buf.data = NULL;
            }
            db_free(&buf);
            if (fields_push(out, owned, 0) != 0) {
                return 1;
            }
        } 
        else {
            size_t start = i;
            size_t end;
            size_t j;
            char *owned;

            while (i < n && line[i] != ',') {
                i++;
            }
            end = i;
            while (end > start && is_space_tab(line[end - 1])) {
                end--;
            }
            for (j = start; j < end; j++) {
                if (line[j] == '"') {
                    return 1;
                }
            }
            if (end == start) {
                owned = (char *)malloc(1);
                if (owned == NULL) {
                    return 1;
                }
                owned[0] = '\0';
                if (fields_push(out, owned, 1) != 0) {
                    return 1;
                }
            }
             else {
                size_t len = end - start;
                owned = (char *)malloc(len + 1);
                if (owned == NULL) {
                    return 1;
                }
                memcpy(owned, line + start, len);
                owned[len] = '\0';
                if (fields_push(out, owned, 0) != 0) {
                    return 1;
                }
            }
        }

        if (i >= n) {
            break;
        }
        if (line[i] != ',') {
            return 1;
        }
        i++;
    }

    return 0;
}

static void strip_lf(char *line)
{
    size_t len;

    if (line == NULL) {
        return;
    }
    len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static ColType classify_intish(const char *text)
{
    const char *p = text;
    char *end = NULL;
    long long v;

    if (text == NULL || text[0] == '\0') {
        return COL_STRING;
    }
    if (*p == '+' || *p == '-') {
        p++;
    }
    if (!is_digit(*p)) {
        return COL_STRING;
    }
    while (is_digit(*p)) {
        p++;
    }
    if (*p != '\0') {
        return COL_STRING;
    }

    errno = 0;
    v = strtoll(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') {
        return COL_STRING;
    }
    (void)v;
    return COL_INT;
}

static ColType classify_doubleish(const char *text)
{
    const char *p = text;
    char *end = NULL;
    double v;

    if (text == NULL || text[0] == '\0') {
        return COL_STRING;
    }
    if (*p == '+' || *p == '-') {
        p++;
    }
    if (!is_digit(*p)) {
        return COL_STRING;
    }
    while (is_digit(*p)) {
        p++;
    }
    if (*p != '.') {
        return COL_STRING;
    }
    p++;
    if (!is_digit(*p)) {
        return COL_STRING;
    }
    while (is_digit(*p)) {
        p++;
    }
    if (*p != '\0') {
        return COL_STRING;
    }

    errno = 0;
    v = strtod(text, &end);
    if (end == text || *end != '\0') {
        return COL_STRING;
    }
    if (!isfinite(v)) {
        return COL_STRING;
    }
    return COL_DOUBLE;
}

ColType csv_classify_text(const char *text)
{
    ColType t;

    t = classify_intish(text);
    if (t == COL_INT) {
        return COL_INT;
    }
    t = classify_doubleish(text);
    if (t == COL_DOUBLE) {
        return COL_DOUBLE;
    }
    return COL_STRING;
}

static ColType widen_type(ColType cur, ColType v)
{
    if (v == COL_NULL) {
        return cur;
    }
    if (cur == COL_NULL) {
        return v;
    }
    if (v > cur) {
        return v;
    }
    return cur;
}

const char *csv_type_name(ColType t)
{
    switch (t) {
    case COL_NULL:
        return "NULL";
    case COL_INT:
        return "INT";
    case COL_DOUBLE:
        return "DOUBLE";
    case COL_STRING:
        return "STRING";
    default:
        return "?";
    }
}

void csv_fprint_quoted(FILE *fp, const char *s)
{
    const char *p;

    fputc('"', fp);
    if (s != NULL) {
        for (p = s; *p != '\0'; p++) {
            if (*p == '"') {
                fputc('"', fp);
            }
            fputc(*p, fp);
        }
    }
    fputc('"', fp);
}

void csv_schema_free(CsvSchema *schema)
{
    size_t i;

    if (schema == NULL) {
        return;
    }
    if (schema->names != NULL) {
        for (i = 0; i < schema->ncols; i++) {
            free(schema->names[i]);
        }
        free(schema->names);
    }
    free(schema->types);
    free(schema->stats);
    schema->names = NULL;
    schema->types = NULL;
    schema->stats = NULL;
    schema->nrows = 0;
    schema->ncols = 0;
}

static void stats_init(ColStats *s)
{
    memset(s, 0, sizeof(*s));
}

static int parse_i64(const char *text, int64_t *out)
{
    char *end = NULL;
    long long v;

    errno = 0;
    v = strtoll(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0') {
        return 1;
    }
    *out = (int64_t)v;
    return 0;
}

static int parse_f64(const char *text, double *out)
{
    char *end = NULL;
    double v;

    errno = 0;
    v = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(v)) {
        return 1;
    }
    *out = v;
    return 0;
}

static void update_int_stats(ColStats *s, int64_t v)
{
    size_t n;

    if (!s->i_has) {
        s->imin = v;
        s->imax = v;
        s->imean = 0.0;
        s->i_has = 1;
    } else {
        if (v < s->imin) {
            s->imin = v;
        }
        if (v > s->imax) {
            s->imax = v;
        }
    }
    s->inonnull++;
    n = s->inonnull;
    s->imean += ((double)v - s->imean) / (double)n;
    if (v > 0) {
        s->positive++;
    } else if (v < 0) {
        s->negative++;
    }
    if (v % 2 == 0) {
        s->even++;
    }
}

static void update_double_stats(ColStats *s, double v)
{
    size_t n;

    if (!s->d_has) {
        s->dmin = v;
        s->dmax = v;
        s->dmean = 0.0;
        s->d_has = 1;
    } else {
        if (v < s->dmin) {
            s->dmin = v;
        }
        if (v > s->dmax) {
            s->dmax = v;
        }
    }
    s->dnonnull++;
    n = s->dnonnull;
    s->dmean += (v - s->dmean) / (double)n;
    if (v > 0.0) {
        s->d_positive++;
    } else if (v < 0.0) {
        s->d_negative++;
    }
    if (v == trunc(v)) {
        s->d_integer++;
    }
}

static void update_string_stats(ColStats *s, const char *text)
{
    size_t len = strlen(text);
    size_t n;

    if (!s->s_has) {
        s->min_len = len;
        s->max_len = len;
        s->mean_len = 0.0;
        s->s_has = 1;
    } else {
        if (len < s->min_len) {
            s->min_len = len;
        }
        if (len > s->max_len) {
            s->max_len = len;
        }
    }
    s->snonnull++;
    n = s->snonnull;
    s->mean_len += ((double)len - s->mean_len) / (double)n;
    if (len == 0) {
        s->empty++;
    }
}

static int names_unique(char **names, size_t ncols)
{
    size_t i, j;

    for (i = 0; i < ncols; i++) {
        for (j = i + 1; j < ncols; j++) {
            if (strcmp(names[i], names[j]) == 0) {
                return 0;
            }
        }
    }
    return 1;
}

typedef enum {
    PASS_TYPES = 0,
    PASS_STATS = 1
} PassKind;

typedef struct {
    PassKind pass;
    CsvSchema *schema;
    int saw_header;
    size_t data_rows;
    size_t pending_empty;
} ScanState;

static int process_data_fields(ScanState *st, FieldList *fl)
{
    size_t c;
    CsvSchema *sch = st->schema;

    if (fl->n != sch->ncols) {
        return 1;
    }

    if (st->pass == PASS_TYPES) {
        for (c = 0; c < sch->ncols; c++) {
            ColType vt;
            if (fl->items[c].is_null) {
                vt = COL_NULL;
            } else {
                vt = csv_classify_text(fl->items[c].text);
            }
            sch->types[c] = widen_type(sch->types[c], vt);
        }
    }
     else {
        for (c = 0; c < sch->ncols; c++) {
            ColStats *cs = &sch->stats[c];
            ColType t = sch->types[c];

            if (fl->items[c].is_null) {
                cs->nulls++;
                continue;
            }
            if (t == COL_NULL) {
                return 1;
            } 
            else if (t == COL_INT) {
                int64_t v;
                if (parse_i64(fl->items[c].text, &v) != 0) {
                    return 1;
                }
                update_int_stats(cs, v);
            } 
            else if (t == COL_DOUBLE) {
                ColType ct = csv_classify_text(fl->items[c].text);
                if (ct == COL_INT) {
                    int64_t iv;
                    if (parse_i64(fl->items[c].text, &iv) != 0) {
                        return 1;
                    }
                    update_double_stats(cs, (double)iv);
                } 
                else {
                    double dv;
                    if (parse_f64(fl->items[c].text, &dv) != 0) {
                        return 1;
                    }
                    update_double_stats(cs, dv);
                }
            }
            else if (t == COL_STRING) {
                update_string_stats(cs, fl->items[c].text);
            }
        }
    }

    st->data_rows++;
    return 0;
}

static int flush_pending_empty(ScanState *st, FieldList *tmp)
{
    while (st->pending_empty > 0) {
        char *empty;
        fields_free(tmp);
        empty = (char *)malloc(1);
        if (empty == NULL) {
            return 1;
        }
        empty[0] = '\0';
        if (fields_push(tmp, empty, 1) != 0) {
            return 1;
        }
        if (process_data_fields(st, tmp) != 0) {
            return 1;
        }
        st->pending_empty--;
    }
    return 0;
}

static int handle_record_line(ScanState *st, const char *line, FieldList *fl)
{
    if (parse_record(line, fl) != 0) {
        return 1;
    }

    if (!st->saw_header) {
        size_t c;
        CsvSchema *sch = st->schema;

        if (fl->n == 0) {
            return 1;
        }
        sch->ncols = fl->n;
        sch->names = (char **)calloc(sch->ncols, sizeof(char *));
        sch->types = (ColType *)calloc(sch->ncols, sizeof(ColType));
        sch->stats = (ColStats *)calloc(sch->ncols, sizeof(ColStats));
        if (sch->names == NULL || sch->types == NULL || sch->stats == NULL) {
            return 1;
        }
        for (c = 0; c < sch->ncols; c++) {
            stats_init(&sch->stats[c]);
            sch->types[c] = COL_NULL;
            if (fl->items[c].is_null || fl->items[c].text[0] == '\0') {
                return 1;
            }
            sch->names[c] = fl->items[c].text;
            fl->items[c].text = NULL;
        }
        if (!names_unique(sch->names, sch->ncols)) {
            return 1;
        }
        st->saw_header = 1;
        return 0;
    }
    if (flush_pending_empty(st, fl) != 0) {
        return 1;
    }
    if (parse_record(line, fl) != 0) {
        return 1;
    }
    return process_data_fields(st, fl);
}

static int scan_file(const char *path, ScanState *st)
{
    FILE *fp;
    char *line = NULL;
    size_t cap = 0;
    ssize_t nread;
    FieldList fl;
    int rc = 1;

    fl.items = NULL;
    fl.n = 0;
    fl.cap = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }

    st->saw_header = 0;
    st->data_rows = 0;
    st->pending_empty = 0;

    while ((nread = getline(&line, &cap, fp)) != -1) {
        strip_lf(line);

        if (!st->saw_header) {
            if (line[0] == '\0') {
            }
            if (handle_record_line(st, line, &fl) != 0) {
                goto done;
            }
            continue;
        }

        if (line[0] == '\0') {
            st->pending_empty++;
            continue;
        }

        if (handle_record_line(st, line, &fl) != 0) {
            goto done;
        }
    }

    if (!feof(fp)) {
        goto done;
    }
    if (!st->saw_header) {
        goto done;
    }
    st->schema->nrows = st->data_rows;
    rc = 0;

done:
    free(line);
    fields_free(&fl);
    fclose(fp);
    return rc;
}

static int flush_empty_rows(CsvSchema *schema, size_t *rows, size_t count,
                            FieldList *scratch)
{
    while (count > 0) {
        ScanState tmpst;
        char *empty;

        fields_free(scratch);
        empty = (char *)malloc(1);
        if (empty == NULL) {
            return 1;
        }
        empty[0] = '\0';
        if (fields_push(scratch, empty, 1) != 0) {
            return 1;
        }
        tmpst.pass = PASS_STATS;
        tmpst.schema = schema;
        tmpst.saw_header = 1;
        tmpst.data_rows = *rows;
        tmpst.pending_empty = 0;
        if (process_data_fields(&tmpst, scratch) != 0) {
            return 1;
        }
        *rows = tmpst.data_rows;
        count--;
    }
    return 0;
}

static int scan_stats_pass(const char *path, CsvSchema *schema)
{
    FILE *fp;
    char *line = NULL;
    size_t cap = 0;
    ssize_t nread;
    FieldList fl;
    int header_done = 0;
    size_t pending_empty = 0;
    size_t rows = 0;
    size_t c;
    int rc = 1;

    for (c = 0; c < schema->ncols; c++) {
        stats_init(&schema->stats[c]);
    }

    fl.items = NULL;
    fl.n = 0;
    fl.cap = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }

    while ((nread = getline(&line, &cap, fp)) != -1) {
        strip_lf(line);

        if (!header_done) {
            if (parse_record(line, &fl) != 0 || fl.n != schema->ncols) {
                goto done;
            }
            header_done = 1;
            continue;
        }

        if (line[0] == '\0') {
            pending_empty++;
            continue;
        }

        if (flush_empty_rows(schema, &rows, pending_empty, &fl) != 0) {
            goto done;
        }
        pending_empty = 0;

        if (parse_record(line, &fl) != 0) {
            goto done;
        }
        {
            ScanState tmpst;
            tmpst.pass = PASS_STATS;
            tmpst.schema = schema;
            tmpst.saw_header = 1;
            tmpst.data_rows = rows;
            tmpst.pending_empty = 0;
            if (process_data_fields(&tmpst, &fl) != 0) {
                goto done;
            }
            rows = tmpst.data_rows;
        }
    }

    if (!feof(fp)) {
        goto done;
    }
    if (rows != schema->nrows) {
        goto done;
    }
    rc = 0;

done:
    free(line);
    fields_free(&fl);
    fclose(fp);
    return rc;
}

int csv_analyze(const char *path, CsvSchema *out)
{
    ScanState st;
    CsvSchema schema;

    if (path == NULL || out == NULL) {
        return 1;
    }

    memset(&schema, 0, sizeof(schema));

    st.pass = PASS_TYPES;
    st.schema = &schema;
    if (scan_file(path, &st) != 0) {
        csv_schema_free(&schema);
        return 1;
    }

    if (scan_stats_pass(path, &schema) != 0) {
        csv_schema_free(&schema);
        return 1;
    }

    *out = schema;
    return 0;
}
