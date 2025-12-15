#ifndef UTILS_H
#define UTILS_H

#include "csv_reader.h"
#include "string_utils.h"

/* forward declaration for ResultSet (defined in evaluator.h) */
#ifndef RESULTSET_TYPEDEF
#define RESULTSET_TYPEDEF
typedef CsvTable ResultSet;
#endif

char* skipWhitespaces(char* str);
void print_help(const char* program_name);
void write_csv_file(const char* filename, ResultSet* result, char delimiter);
char* read_query_from_file(const char* filename);
char* read_query_from_stdin(void);

#endif