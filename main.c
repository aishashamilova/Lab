#include "cli.h"
#include "csv.h"
#include "stats.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    CliOptions opt;
    CsvSchema schema;
    int rc;

    if (cli_parse(argc, argv, &opt) != 0) {
        fprintf(stderr, "csvstat: bad arguments\n");
        return 2;
    }

    if (opt.command == CMD_HELP) {
        cli_print_help();
        return 0;
    }

    schema.nrows = 0;
    schema.ncols = 0;
    schema.names = NULL;
    schema.types = NULL;
    schema.stats = NULL;

    if (csv_analyze(opt.csv_path, &schema) != 0) {
        fprintf(stderr, "csvstat: cannot process '%s'\n", opt.csv_path);
        return 1;
    }

    if (opt.command == CMD_SCHEMA) {
        print_schema(&schema);
        rc = 0;
    } 
    else if (opt.command == CMD_STATS) {
        print_stats(&schema);
        rc = 0;
    } 
    else if (opt.command == CMD_REPORT) {
        if (write_report(&schema, opt.report_path) != 0) {
            fprintf(stderr, "csvstat: cannot write '%s'\n", opt.report_path);
            rc = 1;
        } 
        else {
            rc = 0;
        }
    } 
    else {
        fprintf(stderr, "csvstat: unknown command\n");
        rc = 1;
    }

    csv_schema_free(&schema);
    return rc;
}
