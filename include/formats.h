#ifndef FORMATS_H
#define FORMATS_H

#include "utils.h"

typedef enum { FMT_AUTO = 0, FMT_CSV, FMT_TABLE, FMT_MARKDOWN, FMT_YAML, FMT_JSON } OutputFormat;

/* Printers for various output formats */
void print_json(ResultSet* res);
void print_markdown(ResultSet* res);
void print_yaml(ResultSet* res);

/* Write result set to file in given format. Returns true on success. */
bool write_output_file(const char* filename, ResultSet* res, OutputFormat fmt, char delimiter);

#endif
