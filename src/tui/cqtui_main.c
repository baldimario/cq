#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "../../include/tui/tui_core.h"
#include "../../include/tui/terminal.h"
#include "../../include/evaluator.h"

extern CsvConfig global_csv_config;

static void print_tui_help(const char* program_name) {
    printf("Usage: %s [OPTIONS] [DIRECTORY]\n", program_name);
    printf("\n");
    printf("cq TUI - Terminal User Interface for querying CSV files\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -s CHAR             Set input CSV delimiter (default: ',')\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  DIRECTORY           Directory containing CSV files (default: current directory)\n");
    printf("\n");
    printf("Keyboard Shortcuts:\n");
    printf("  Arrow Keys          Navigate table data / Edit query\n");
    printf("  Page Up/Down        Scroll pages\n");
    printf("  Home/End            Jump to start/end\n");
    printf("  :                   Enter query mode\n");
    printf("  t                   Show table list\n");
    printf("  w                   Save/export current table\n");
    printf("  Ctrl+N              Next tab\n");
    printf("  Ctrl+P              Previous tab\n");
    printf("  Ctrl+W              Close current tab\n");
    printf("  h                   Show help\n");
    printf("  q or Ctrl+Q         Quit\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    char input_separator = ',';
    
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hs:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_tui_help(argv[0]);
                return 0;
            case 's':
                input_separator = optarg[0];
                break;
            default:
                print_tui_help(argv[0]);
                return 1;
        }
    }
    
    global_csv_config.delimiter = input_separator;
    global_csv_config.quote = '"';
    global_csv_config.has_header = true;
    
    const char* workspace_dir = ".";
    if (optind < argc) {
        workspace_dir = argv[optind];
    }
    
    TuiApp* app = tui_app_create(workspace_dir);
    if (!app) {
        fprintf(stderr, "Error: Failed to create TUI application\n");
        return 1;
    }
    
    int result = tui_app_run(app);
    
    tui_app_destroy(app);
    
    return result;
}
