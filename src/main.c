#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "tokenizer.h"
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"
#include "formats.h"
#include "utils.h"




int main(int argc, char* argv[]) {
    char* query = NULL;
    char* query_file = NULL;
    char* output_file = NULL;
    bool print_count = false;
    bool print_table = false;
    bool vertical_output = false;
    bool query_allocated = false;  // track if we need to free query
    char input_separator = ',';
    char output_delimiter = ',';
    
    OutputFormat print_format = FMT_AUTO;
    OutputFormat file_format = FMT_AUTO;
    // long options for --force
    static struct option long_options[] = {
        {"force", no_argument, 0, 'F'},
        {"help", no_argument, 0, 'h'},
        {"format", required_argument, 0, 'O'},
        {0, 0, 0, 0}
    };
    
    // parse args
    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hq:f:o:O:cp::s:d:vF", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_help(argv[0]);
                return 0;
            case 'q':
                query = optarg;
                break;
            case 'f':
                query_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'c':
                print_count = true;
                break;
            case 'p':
                // optional argument: if provided, set printed format; otherwise default to table
                print_table = true;
                if (optarg) {
                    if (strcasecmp(optarg, "csv") == 0) print_format = FMT_CSV;
                    else if (strcasecmp(optarg, "table") == 0) print_format = FMT_TABLE;
                    else if (strcasecmp(optarg, "markdown") == 0 || strcasecmp(optarg, "md") == 0) print_format = FMT_MARKDOWN;
                    else if (strcasecmp(optarg, "yaml") == 0 || strcasecmp(optarg, "yml") == 0) print_format = FMT_YAML;
                    else if (strcasecmp(optarg, "json") == 0) print_format = FMT_JSON;
                } else {
                    print_format = FMT_TABLE;
                }
                break;
            case 'O':
                if (optarg) {
                    if (strcasecmp(optarg, "csv") == 0) file_format = FMT_CSV;
                    else if (strcasecmp(optarg, "table") == 0) file_format = FMT_TABLE;
                    else if (strcasecmp(optarg, "markdown") == 0 || strcasecmp(optarg, "md") == 0) file_format = FMT_MARKDOWN;
                    else if (strcasecmp(optarg, "yaml") == 0 || strcasecmp(optarg, "yml") == 0) file_format = FMT_YAML;
                    else if (strcasecmp(optarg, "json") == 0) file_format = FMT_JSON;
                }
                break;
            case 's':
                input_separator = optarg[0];
                break;
            case 'd':
                output_delimiter = optarg[0];
                break;
            case 'v':
                vertical_output = true;
                print_table = true;  // implicit
                break;
            case 'F':
                force_delete = true;
                break;
            default:
                print_help(argv[0]);
                return 1;
        }
    }
    
    // determine query source (priority: -f, -q, stdin)
    if (query_file) {
        // read from file
        query = read_query_from_file(query_file);
        if (!query) {
            return 1;
        }
        query_allocated = true;
    } else if (query) {
        // check if query is "-" which means read from stdin
        if (strcmp(query, "-") == 0) {
            query = read_query_from_stdin();
            if (!query) {
                return 1;
            }
            query_allocated = true;
        }
        // else use query as-is from command line
    } else {
        // no -q or -f specified
        fprintf(stderr, "Error: Query is required (use -q or -f)\n\n");
        print_help(argv[0]);
        return 1;
    }
    
    // global CSV configuration
    global_csv_config.delimiter = input_separator;
    global_csv_config.quote = '"';
    global_csv_config.has_header = true;
    
    // parse SQL query
    ASTNode* ast = parse(query);
    if (!ast) {
        fprintf(stderr, "Error: Parsing failed\n");
        return 1;
    }
    
    // evaluate query
    ResultSet* result = evaluate_query(ast);
    if (!result) {
        fprintf(stderr, "Error: Query evaluation failed\n");
        releaseNode(ast);
        return 1;
    }
    
    // output results based on flags
    if (print_count) {
        printf("Records: %d\n", result->row_count);
        printf("Columns: %d\n", result->column_count);
    }

    
    if (print_table) {
        OutputFormat use = print_format;
        if (use == FMT_AUTO) use = FMT_TABLE;
        if (use == FMT_JSON) print_json(result);
        else if (use == FMT_MARKDOWN) print_markdown(result);
        else if (use == FMT_YAML) print_yaml(result);
        else {
            if (vertical_output) csv_print_table_vertical(result, result->row_count);
            else csv_print_table(result, result->row_count);
        }
    }

    if (output_file) {
        OutputFormat fout = file_format;
        if (fout == FMT_AUTO) fout = FMT_CSV;
        if (!write_output_file(output_file, result, fout, output_delimiter)) {
            perror("write_output_file");
        }
    }

    // if no output options specified, default to count
    if (!print_count && !print_table && !output_file) {
        printf("Count: %d\n", result->row_count);
    }
    
    // cleanup
    csv_free(result);
    releaseNode(ast);
    if (query_allocated) {
        free(query);
    }
    
    return 0;
}
