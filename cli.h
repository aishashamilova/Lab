#ifndef CLI_H
#define CLI_H

typedef enum {
    CMD_HELP,
    CMD_SCHEMA,
    CMD_STATS,
    CMD_REPORT
} Command;

typedef struct {
    Command command;
    const char *csv_path;
    const char *report_path;
} CliOptions;

int cli_parse(int argc, char *argv[], CliOptions *out);
void cli_print_help(void);
#endif 
