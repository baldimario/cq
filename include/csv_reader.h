#ifndef CSV_READER_H
#define CSV_READER_H

#include <stddef.h>
#include <stdbool.h>

/* data types for CSV values */
typedef enum {
    VALUE_TYPE_NULL,
    VALUE_TYPE_INTEGER,
    VALUE_TYPE_DOUBLE,
    VALUE_TYPE_STRING,
} ValueType;

/* value structure */
typedef struct {
    ValueType type;
    union {
        long long int_value;
        double double_value;
        char* string_value;
    };
} Value;

/* column metadata */
typedef struct {
    char* name;
    ValueType inferred_type;
} Column;

/* row structure */
typedef struct {
    Value* values;
    int column_count;
} Row;

/* CSV table structure */
typedef struct {
    char* filename;
    char* data;          // mmap'd data
    size_t file_size;
    int fd;              // file descriptor
    
    Column* columns;
    int column_count;
    bool has_header;
    
    Row* rows;
    int row_count;
    int row_capacity;
    
    char delimiter;      // field delimiter (default: ',')
    char quote;          // quote character (default: '"')
} CsvTable;

/* configuration for CSV parsing */
typedef struct {
    char delimiter;
    char quote;
    bool has_header;
} CsvConfig;

/* create default CSV config used in tests */
CsvConfig csv_config_default(void);

/* load CSV file into memory using mmap */
CsvTable* csv_load(const char* filename, CsvConfig config);

/* save CSV table to file */
bool csv_save(const char* filename, CsvTable* table);

/* free CSV table */
void csv_free(CsvTable* table);

/* get value from table */
Value* csv_get_value(CsvTable* table, int row_index, int col_index);
Value* csv_get_value_by_name(CsvTable* table, int row_index, const char* col_name);

/* get column index by name */
int csv_get_column_index(CsvTable* table, const char* col_name);

/* print table for debugging */
void csv_print_table(CsvTable* table, int max_rows);
void csv_print_table_vertical(CsvTable* table, int max_rows);

/* value utilities */
void value_free(Value* value);
char* value_to_string(Value* value);
int value_compare(Value* a, Value* b);
Value parse_value(const char* str, size_t len);

#endif
