#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "csv_reader.h"
#include "evaluator.h"
#include "string_utils.h"
#include "utils.h"


/* Portable string functions for cross-platform compatibility */

/* 
 * cq_strndup - duplicate a string with length limit
 * Not available on Windows and some older systems
 */
char* cq_strndup(const char* s, size_t n) {
    if (!s) return NULL;
    
    size_t len = 0;
    while (len < n && s[len] != '\0') {
        len++;
    }
    
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

/*
 * cq_strlcat - size-bounded string concatenation
 * BSD-specific, not available on Linux/Windows
 * Returns the total length of the string it tried to create
 */
size_t cq_strlcat(char* dst, const char* src, size_t size) {
    if (!dst || !src || size == 0) {
        return src ? strlen(src) : 0;
    }
    
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);
    
    if (dst_len >= size) {
        return size + src_len;
    }
    
    size_t copy_len = size - dst_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }
    
    memcpy(dst + dst_len, src, copy_len);
    dst[dst_len + copy_len] = '\0';
    
    return dst_len + src_len;
}

/*
 * cq_strcasestr - case-insensitive substring search
 * GNU extension, not available on Windows or BSD by default
 */
char* cq_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;
    
    size_t needle_len = strlen(needle);
    
    for (; *haystack != '\0'; haystack++) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i;
            for (i = 1; i < needle_len; i++) {
                if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i])) {
                    break;
                }
            }
            if (i == needle_len) {
                return (char*)haystack;
            }
        }
    }
    
    return NULL;
}


/* function to skip whitespace characters in the input string
 * it returns a pointer to the next first non-whitespace character
 */
char* skipWhitespaces(char* str) {
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    return str;
}

void print_help(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h           Show this help message\n");
    printf("  -q <query>   SQL query to execute (required)\n");
    printf("  -o <file>    Write result as CSV to output file\n");
    printf("  -c           Print count of rows that match the query\n");
    printf("  -p           Print result as formatted table to stdout\n");
    printf("  -v           Print result in vertical format (one column per line)\n");
    printf("  -s <char>    Field separator for input CSV (default: ',')\n");
    printf("  -d <char>    Output delimiter for -o option (default: ',')\n");
    printf("\nExample:\n");
    printf("  %s -q \"SELECT name, age WHERE age > 30\" -p\n", program_name);
    printf("  %s -q \"SELECT * WHERE active = 1\" -o output.csv -c\n", program_name);
    printf("  %s -q \"SELECT * FROM data.tsv\" -s '\\t' -p\n", program_name);
    printf("  %s -q \"SELECT * FROM data.csv LIMIT 5\" -v\n", program_name);
}

/* Write ResultSet to CSV file */
void write_csv_file(const char* filename, ResultSet* result, char delimiter) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot open output file '%s'\n", filename);
        return;
    }
    
    // header
    for (int i = 0; i < result->column_count; i++) {
        if (i > 0) fprintf(f, "%c", delimiter);
        fprintf(f, "%s", result->columns[i].name);
    }
    fprintf(f, "\n");
    
    // rows
    for (int i = 0; i < result->row_count; i++) {
        Row* row = &result->rows[i];
        for (int j = 0; j < row->column_count; j++) {
            if (j > 0) fprintf(f, "%c", delimiter);
            
            Value* val = &row->values[j];
            switch (val->type) {
                case VALUE_TYPE_NULL:
                    break;
                case VALUE_TYPE_INTEGER:
                    fprintf(f, "%lld", val->int_value);
                    break;
                case VALUE_TYPE_DOUBLE:
                    fprintf(f, "%.2f", val->double_value);
                    break;
                case VALUE_TYPE_STRING: {
                    // check if string contains delimiter, newline, or quote char
                    bool needs_quoting = false;
                    if (val->string_value) {
                        for (const char* p = val->string_value; *p; p++) {
                            if (*p == delimiter || *p == '"' || *p == '\n' || *p == '\r') {
                                needs_quoting = true;
                                break;
                            }
                        }
                    }
                    
                    if (needs_quoting) {
                        fprintf(f, "\"");
                        // escape quotes by doubling them
                        for (const char* p = val->string_value; *p; p++) {
                            if (*p == '"') {
                                fprintf(f, "\"\"");
                            } else {
                                fprintf(f, "%c", *p);
                            }
                        }
                        fprintf(f, "\"");
                    } else {
                        fprintf(f, "%s", val->string_value ? val->string_value : "");
                    }
                    break;
                }
            }
        }
        fprintf(f, "\n");
    }
    
    fclose(f);
    printf("Result written to '%s'\n", filename);
}
