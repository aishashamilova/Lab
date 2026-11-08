#include "cli.h"
#include <stdio.h>
#include <string.h>

void cli_print_help(void)
{
    printf(
        "usage:\n"
        "  csvstat --help\n"
        "  csvstat schema <csv-file>\n"
        "  csvstat stats <csv-file>\n"
        "  csvstat report <csv-file> <report-file>\n"
        "\n"
        "exit codes: 0 ok, 1 runtime error, 2 bad args\n"
    );
}

int cli_parse(int argc, char *argv[], CliOptions *out)
{
    if (out == NULL) {
        return 2;
    }

    out->command = CMD_HELP;
    out->csv_path = NULL;
    out->report_path = NULL;

    if (argc < 2) {
        return 2;
    }

    if (strcmp(argv[1], "--help") == 0) {
        if (argc != 2) {
            return 2;
        }
        out->command = CMD_HELP;
        return 0;
    }

    if (strcmp(argv[1], "schema") == 0) {
        if (argc != 3) {
            return 2;
        }
        out->command = CMD_SCHEMA;
        out->csv_path = argv[2];
        return 0;
    }

    if (strcmp(argv[1], "stats") == 0) {
        if (argc != 3) {
            return 2;
        }
        out->command = CMD_STATS;
        out->csv_path = argv[2];
        return 0;
    }

    if (strcmp(argv[1], "report") == 0) {
        if (argc != 4) {
            return 2;
        }
        out->command = CMD_REPORT;
        out->csv_path = argv[2];
        out->report_path = argv[3];
        return 0;
    }

    return 2;
}
