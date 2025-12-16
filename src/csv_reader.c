#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>



#include "csv_reader.h"
#include "string_utils.h"
#include "utils.h"
#include "date_utils.h"
#include "mmap.h"


/* CSV configuration used in tests */
CsvConfig csv_config_default(void) {
    CsvConfig config;
    config.delimiter = ',';
    config.quote = '"';
    config.has_header = true;
    return config;
}

/* helper: trim leading and trailing whitespace in-place */
static void trim_whitespace(char* str) {
    if (!str) return;
    
    // trim leading
    char* s = str;
    while (*s && isspace(*s)) s++;
    
    // trim trailing
    char* end = s + strlen(s) - 1;
    while (end > s && isspace(*end)) *end-- = '\0';
    
    // shift if needed
    if (s != str) {
        memmove(str, s, strlen(s) + 1);
    }
}

/* helper: ensure dynamic array capacity */
static void* ensure_field_capacity(void* array, int* capacity, int current_count, size_t element_size) {
    if (current_count >= *capacity) {
        *capacity *= 2;
        return realloc(array, element_size * (*capacity));
    }
    return array;
}

/* helper: convert value to numeric (double) */
static double value_to_numeric(Value* value) {
    if (!value) return 0.0;
    
    switch (value->type) {
        case VALUE_TYPE_INTEGER:
            return (double)value->int_value;
        case VALUE_TYPE_DOUBLE:
            return value->double_value;
        case VALUE_TYPE_STRING:
            return strtod(value->string_value, NULL);
        default:
            return 0.0;
    }
}

/* value utilities */
void value_free(Value* value) {
    if (value && value->type == VALUE_TYPE_STRING && value->string_value) {
        free(value->string_value);
        value->string_value = NULL;
    }
}

char* value_to_string(Value* value) {
    if (!value) return strdup("NULL");
    
    char buffer[256];
    switch (value->type) {
        case VALUE_TYPE_NULL:
            return strdup("NULL");
        case VALUE_TYPE_INTEGER:
            snprintf(buffer, sizeof(buffer), "%lld", value->int_value);
            return strdup(buffer);
        case VALUE_TYPE_DOUBLE:
            snprintf(buffer, sizeof(buffer), "%.2f", value->double_value);
            return strdup(buffer);
        case VALUE_TYPE_DATE:
            return format_date(value->date_value, DATE_FORMAT_ISO);
        case VALUE_TYPE_STRING:
            return strdup(value->string_value ? value->string_value : "");
    }
    return strdup("");
}

int value_compare(Value* a, Value* b) {
    if (!a || !b) return 0;
    
    // handle NULL comparisons
    if (a->type == VALUE_TYPE_NULL && b->type == VALUE_TYPE_NULL) return 0;
    if (a->type == VALUE_TYPE_NULL) return -1;
    if (b->type == VALUE_TYPE_NULL) return 1;
    
    // handle date comparisons
    if (a->type == VALUE_TYPE_DATE && b->type == VALUE_TYPE_DATE) {
        return compare_dates(a->date_value, b->date_value);
    }
    
    // handle numeric comparisons (int vs int, double vs double, int vs double)
    if ((a->type == VALUE_TYPE_INTEGER || a->type == VALUE_TYPE_DOUBLE) &&
        (b->type == VALUE_TYPE_INTEGER || b->type == VALUE_TYPE_DOUBLE)) {
        
        double a_val = value_to_numeric(a);
        double b_val = value_to_numeric(b);
        
        if (a_val < b_val) return -1;
        if (a_val > b_val) return 1;
        return 0;
    }
    
    // handle string comparisons
    if (a->type == VALUE_TYPE_STRING && b->type == VALUE_TYPE_STRING) {
        return strcmp(a->string_value, b->string_value);
    }
    
    // different types that aren't comparable
    return 0;
}

/* ===== type inference ===== */
static ValueType infer_type(const char* str, size_t len) {
    // try to parse as date first (YYYY-MM-DD format)
    if (len >= 8 && len <= 10) {
        char date_str[32];
        if (len < sizeof(date_str)) {
            memcpy(date_str, str, len);
            date_str[len] = '\0';
            
            // trim whitespace
            char* trimmed = date_str;
            while (*trimmed && isspace(*trimmed)) trimmed++;
            size_t trimmed_len = strlen(trimmed);
            while (trimmed_len > 0 && isspace(trimmed[trimmed_len - 1])) {
                trimmed[--trimmed_len] = '\0';
            }
            
            DateValue date;
            if (parse_date(trimmed, &date)) {
                return VALUE_TYPE_DATE;
            }
        }
    }
    
    // check if it's a number
    bool has_dot = false;
    bool is_number = true;
    size_t i = 0;
    
    // skip leading whitespace
    while (i < len && isspace(str[i])) i++;
    
    // check for sign
    if (i < len && (str[i] == '+' || str[i] == '-')) i++;
    
    if (i >= len) return VALUE_TYPE_STRING;
    
    // check digits
    bool has_digit = false;
    while (i < len && !isspace(str[i])) {
        if (isdigit(str[i])) {
            has_digit = true;
        } else if (str[i] == '.' && !has_dot) {
            has_dot = true;
        } else {
            is_number = false;
            break;
        }
        i++;
    }
    
    // skip trailing whitespace
    while (i < len && isspace(str[i])) i++;
    
    if (is_number && has_digit && i == len) {
        return has_dot ? VALUE_TYPE_DOUBLE : VALUE_TYPE_INTEGER;
    }
    
    return VALUE_TYPE_STRING;
}

Value parse_value(const char* str, size_t len) {
    Value value;
    value.type = VALUE_TYPE_NULL;  // initialize
    value.int_value = 0;  // initialize union
    
    ValueType type = infer_type(str, len);
    value.type = type;
    
    switch (type) {
        case VALUE_TYPE_NULL:
            break;
        case VALUE_TYPE_INTEGER:
            value.int_value = strtoll(str, NULL, 10);
            break;
        case VALUE_TYPE_DOUBLE:
            value.double_value = strtod(str, NULL);
            break;
        case VALUE_TYPE_DATE: {
            char date_str[32];
            if (len < sizeof(date_str)) {
                memcpy(date_str, str, len);
                date_str[len] = '\0';
                
                // trim whitespace
                char* trimmed = date_str;
                while (*trimmed && isspace(*trimmed)) trimmed++;
                size_t trimmed_len = strlen(trimmed);
                while (trimmed_len > 0 && isspace(trimmed[trimmed_len - 1])) {
                    trimmed[--trimmed_len] = '\0';
                }
                
                if (!parse_date(trimmed, &value.date_value)) {
                    // fallback to null if parse fails
                    value.type = VALUE_TYPE_NULL;
                }
            }
            break;
        }
        case VALUE_TYPE_STRING:
            value.string_value = cq_strndup(str, len);
            trim_whitespace(value.string_value);
            break;
    }
    
    return value;
}

/* deep copy a value */
Value value_copy(const Value* src) {
    Value dst;
    dst.type = src->type;
    
    switch (src->type) {
        case VALUE_TYPE_NULL:
            dst.int_value = 0;
            break;
        case VALUE_TYPE_INTEGER:
            dst.int_value = src->int_value;
            break;
        case VALUE_TYPE_DOUBLE:
            dst.double_value = src->double_value;
            break;
        case VALUE_TYPE_DATE:
            dst.date_value = src->date_value;  // struct copy
            break;
        case VALUE_TYPE_STRING:
            dst.string_value = src->string_value ? strdup(src->string_value) : NULL;
            break;
    }
    
    return dst;
}

/* csv parsing functions */

static void add_row(CsvTable* table, Row row) {
    if (table->row_count >= table->row_capacity) {
        table->row_capacity = (table->row_capacity == 0) ? 64 : (table->row_capacity * 2);
        table->rows = realloc(table->rows, sizeof(Row) * table->row_capacity);
    }
    table->rows[table->row_count++] = row;
}

static void parse_line(CsvTable* table, const char* line_start, const char* line_end, bool is_header) {
    const char* ptr = line_start;
    int field_count = 0;
    int field_capacity = 16;
    int field_lengths_capacity = 16;
    char** fields = malloc(sizeof(char*) * field_capacity);
    size_t* field_lengths = malloc(sizeof(size_t) * field_lengths_capacity);
    
    while (ptr < line_end) {
        // skip leading whitespace
        while (ptr < line_end && isspace(*ptr) && *ptr != '\n' && *ptr != '\r') ptr++;
        
        if (ptr >= line_end) break;
        
        const char* field_start = ptr;
        size_t field_len = 0;
        
        // handle quoted fields
        if (*ptr == table->quote) {
            ptr++;
            field_start = ptr;
            
            while (ptr < line_end) {
                if (*ptr == table->quote) {
                    // check for escaped quote
                    if (ptr + 1 < line_end && *(ptr + 1) == table->quote) {
                        ptr += 2;
                        field_len += 2;
                    } else {
                        // end of quoted field
                        field_len = ptr - field_start;
                        ptr++;
                        break;
                    }
                } else {
                    ptr++;
                }
            }
            
            // skip to delimiter or end of line
            while (ptr < line_end && *ptr != table->delimiter && *ptr != '\n' && *ptr != '\r') ptr++;
        } else {
            // unquoted field
            while (ptr < line_end && *ptr != table->delimiter && *ptr != '\n' && *ptr != '\r') {
                ptr++;
            }
            field_len = ptr - field_start;
        }
        
        // store field
        fields = ensure_field_capacity(fields, &field_capacity, field_count, sizeof(char*));
        field_lengths = ensure_field_capacity(field_lengths, &field_lengths_capacity, field_count, sizeof(size_t));
        
        fields[field_count] = (char*)field_start;
        field_lengths[field_count] = field_len;
        field_count++;
        
        // skip delimiter
        if (ptr < line_end && *ptr == table->delimiter) {
            ptr++;
        }
    }
    
    // process fields
    if (is_header) {
        // store column names
        table->column_count = field_count;
        table->columns = malloc(sizeof(Column) * field_count);
        
        for (int i = 0; i < field_count; i++) {
            if (table->has_header && field_lengths[i] > 0) {
                table->columns[i].name = cq_strndup(fields[i], field_lengths[i]);
                trim_whitespace(table->columns[i].name);
            } else {
                // generate column name $0, $1, $2, etc.
                char col_name[16];
                snprintf(col_name, sizeof(col_name), "$%d", i);
                table->columns[i].name = strdup(col_name);
            }
            table->columns[i].inferred_type = VALUE_TYPE_STRING;
        }
    } else {
        // store data row
        int col_count = table->column_count > 0 ? table->column_count : field_count;
        Row row;
        row.column_count = col_count;
        row.values = malloc(sizeof(Value) * col_count);
        for (int i = 0; i < col_count; i++) {
            if (i < field_count) {
                row.values[i] = parse_value(fields[i], field_lengths[i]);
            } else {
                // pad missing columns with NULL
                row.values[i].type = VALUE_TYPE_NULL;
                row.values[i].int_value = 0;
            }
        }
        add_row(table, row);
    }
    
    free(fields);
    free(field_lengths);
}

CsvTable* csv_load(const char* filename, CsvConfig config) {
    size_t file_size;
    int fd;
    
    // Use portable mmap wrapper
    char* data = portable_mmap(filename, &file_size, &fd);
    if (!data) {
        perror("Error loading file");
        return NULL;
    }
    
    // create table structure
    CsvTable* table = calloc(1, sizeof(CsvTable));
    table->filename = strdup(filename);
    table->data = data;
    table->file_size = file_size;
    table->fd = fd;
    table->delimiter = config.delimiter;
    table->quote = config.quote;
    table->has_header = config.has_header;
    table->rows = NULL;
    table->row_count = 0;
    table->row_capacity = 0;
    
    // parse CSV
    const char* ptr = data;
    const char* end = data + file_size;
    bool first_line = true;
    
    while (ptr < end) {
        // find end of line
        const char* line_start = ptr;
        while (ptr < end && *ptr != '\n' && *ptr != '\r') ptr++;
        const char* line_end = ptr;
        
        // skip empty lines
        if (line_end > line_start) {
            if (first_line) {
                parse_line(table, line_start, line_end, true);
                first_line = false;
                
                // if no header, also parse as data
                if (!config.has_header) {
                    parse_line(table, line_start, line_end, false);
                }
            } else {
                parse_line(table, line_start, line_end, false);
            }
        }
        
        // skip line terminators
        while (ptr < end && (*ptr == '\n' || *ptr == '\r')) ptr++;
    }
    
    // infer column types from data
    if (table->row_count > 0 && table->column_count > 0) {
        int sample_size = table->row_count < 20 ? table->row_count : 20;
        
        for (int col = 0; col < table->column_count; col++) {
            // count occurrences of each type
            int type_counts[5] = {0}; // NULL, INTEGER, DOUBLE, STRING, DATE
            
            for (int row = 0; row < sample_size; row++) {
                if (row < table->row_count && col < table->rows[row].column_count) {
                    ValueType type = table->rows[row].values[col].type;
                    if (type >= 0 && type < 5) {
                        type_counts[type]++;
                    }
                }
            }
            
            // determine predominant type (prefer DATE > DOUBLE > INTEGER > STRING > NULL)
            // ignore NULL values in type inference
            ValueType inferred = VALUE_TYPE_STRING;
            
            if (type_counts[VALUE_TYPE_DATE] > 0) {
                inferred = VALUE_TYPE_DATE;
            } else if (type_counts[VALUE_TYPE_DOUBLE] > 0) {
                inferred = VALUE_TYPE_DOUBLE;
            } else if (type_counts[VALUE_TYPE_INTEGER] > 0) {
                inferred = VALUE_TYPE_INTEGER;
            } else if (type_counts[VALUE_TYPE_STRING] > 0) {
                inferred = VALUE_TYPE_STRING;
            }
            
            table->columns[col].inferred_type = inferred;
        }
    }
    
    return table;
}

void csv_free(CsvTable* table) {
    if (!table) return;
    
    // free rows
    for (int i = 0; i < table->row_count; i++) {
        for (int j = 0; j < table->rows[i].column_count; j++) {
            value_free(&table->rows[i].values[j]);
        }
        free(table->rows[i].values);
    }
    free(table->rows);
    
    // free columns
    for (int i = 0; i < table->column_count; i++) {
        free(table->columns[i].name);
    }
    free(table->columns);
    
    // unmap/free file data using portable wrapper
    portable_munmap(table->data, table->file_size, table->fd);
    
    free(table->filename);
    free(table);
}

/* ===== Access Functions ===== */
Value* csv_get_value(CsvTable* table, int row_index, int col_index) {
    if (!table || row_index < 0 || row_index >= table->row_count) return NULL;
    if (col_index < 0 || col_index >= table->rows[row_index].column_count) return NULL;
    
    return &table->rows[row_index].values[col_index];
}

int csv_get_column_index(CsvTable* table, const char* col_name) {
    if (!table || !col_name) return -1;
    
    for (int i = 0; i < table->column_count; i++) {
        if (strcasecmp(table->columns[i].name, col_name) == 0) {
            return i;
        }
    }
    return -1;
}

Value* csv_get_value_by_name(CsvTable* table, int row_index, const char* col_name) {
    int col_index = csv_get_column_index(table, col_name);
    if (col_index < 0) return NULL;
    return csv_get_value(table, row_index, col_index);
}


/* save CSV table to file */
bool csv_save(const char* filename, CsvTable* table) {
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("fopen");
        return false;
    }
    
    // write header
    if (table->has_header) {
        for (int i = 0; i < table->column_count; i++) {
            if (i > 0) fprintf(f, "%c", table->delimiter);
            
            // check if column name needs quoting, it contains delimiter, quote, or newline
            const char* col_name = table->columns[i].name;
            bool needs_quote = false;
            for (const char* p = col_name; *p; p++) {
                if (*p == table->delimiter || *p == table->quote || *p == '\n' || *p == '\r') {
                    needs_quote = true;
                    break;
                }
            }
            
            if (needs_quote) {
                fprintf(f, "%c", table->quote);
                for (const char* p = col_name; *p; p++) {
                    if (*p == table->quote) {
                        fprintf(f, "%c%c", table->quote, table->quote); // escape quote
                    } else {
                        fprintf(f, "%c", *p);
                    }
                }
                fprintf(f, "%c", table->quote);
            } else {
                fprintf(f, "%s", col_name);
            }
        }
        fprintf(f, "\n");
    }
    
    // write rows
    for (int row = 0; row < table->row_count; row++) {
        for (int col = 0; col < table->rows[row].column_count && col < table->column_count; col++) {
            if (col > 0) fprintf(f, "%c", table->delimiter);
            
            Value* val = &table->rows[row].values[col];
            
            switch (val->type) {
                case VALUE_TYPE_NULL:
                    // write nothing (empty field)
                    break;
                    
                case VALUE_TYPE_INTEGER:
                    fprintf(f, "%lld", val->int_value);
                    break;
                    
                case VALUE_TYPE_DOUBLE:
                    fprintf(f, "%.15g", val->double_value);
                    break;
                    
                case VALUE_TYPE_DATE:
                    fprintf(f, "%04d-%02d-%02d",
                            val->date_value.year,
                            val->date_value.month,
                            val->date_value.day);
                    break;
                    
                case VALUE_TYPE_STRING: {
                    // check if string needs quoting
                    bool needs_quote = false;
                    const char* str = val->string_value;
                    for (const char* p = str; *p; p++) {
                        if (*p == table->delimiter || *p == table->quote || *p == '\n' || *p == '\r') {
                            needs_quote = true;
                            break;
                        }
                    }
                    
                    if (needs_quote) {
                        fprintf(f, "%c", table->quote);
                        for (const char* p = str; *p; p++) {
                            if (*p == table->quote) {
                                fprintf(f, "%c%c", table->quote, table->quote);
                            } else {
                                fprintf(f, "%c", *p);
                            }
                        }
                        fprintf(f, "%c", table->quote);
                    } else {
                        fprintf(f, "%s", str);
                    }
                    break;
                }
            }
        }
        fprintf(f, "\n");
    }
    
    fclose(f);
    return true;
}

