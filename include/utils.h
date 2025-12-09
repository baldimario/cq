#ifndef UTILS_H
#define UTILS_H

#include "csv_reader.h"
#include "string_utils.h"

char* skipWhitespaces(char* str);
void print_help(const char* program_name);
void write_csv_file(const char* filename, ResultSet* result, char delimiter);

#endif