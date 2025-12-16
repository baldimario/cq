#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "formats.h"
#include "utils.h"

/* CSV print table */
void csv_print_table(CsvTable* table, int max_rows) {
    if (!table) return;
    if (table->column_count <= 0) return;

    const int MAX_COL_WIDTH = 40;
    int inspect_rows = (max_rows > 0 && max_rows < table->row_count) ? max_rows : table->row_count;
    int* widths = calloc(table->column_count, sizeof(int));
    if (!widths) return;

    for (int c = 0; c < table->column_count; c++) {
        widths[c] = (int)strlen(table->columns[c].name);
        if (widths[c] > MAX_COL_WIDTH) widths[c] = MAX_COL_WIDTH;
    }

    for (int r = 0; r < inspect_rows; r++) {
        if (r >= table->row_count) break;
        for (int c = 0; c < table->column_count; c++) {
            if (c >= table->rows[r].column_count) continue;
            char* s = value_to_string(&table->rows[r].values[c]);
            int len = (int)strlen(s);
            if (len > MAX_COL_WIDTH) len = MAX_COL_WIDTH;
            if (len > widths[c]) widths[c] = len;
            free(s);
        }
    }

    for (int c = 0; c < table->column_count; c++) if (widths[c] < 3) widths[c] = 3;

    for (int c = 0; c < table->column_count; c++) {
        printf("%-*s", widths[c] + 1, table->columns[c].name);
        if (c < table->column_count - 1) printf(" | ");
    }
    printf("\n");

    for (int c = 0; c < table->column_count; c++) {
        for (int k = 0; k < widths[c] + 1; k++) putchar('-');
        if (c < table->column_count - 1) printf("-+-");
    }
    printf("\n");

    int rows_to_print = inspect_rows;
    for (int r = 0; r < rows_to_print; r++) {
        for (int c = 0; c < table->column_count; c++) {
            char* s = NULL;
            if (r < table->row_count && c < table->rows[r].column_count) s = value_to_string(&table->rows[r].values[c]);
            else s = strdup("");

            int len = (int)strlen(s);
            if (len <= widths[c]) printf("%-*s", widths[c] + 1, s);
            else {
                if (widths[c] > 3) {
                    int keep = widths[c] - 3;
                    fwrite(s, 1, keep, stdout);
                    printf("...");
                    putchar(' ');
                } else {
                    for (int k = 0; k < widths[c] && k < len; k++) putchar(s[k]);
                    putchar(' ');
                }
            }
            free(s);
            if (c < table->column_count - 1) printf(" | ");
        }
        printf("\n");
    }

    if (max_rows > 0 && table->row_count > max_rows) printf("... (%d more rows)\n", table->row_count - max_rows);

    free(widths);
}

/* JSON printer */
void print_json(ResultSet* res) {
    if (!res) return;
    printf("[");
    for (int r = 0; r < res->row_count; r++) {
        if (r) printf(",\n");
        printf("  {");
        for (int c = 0; c < res->column_count; c++) {
            if (c) printf(", ");
            char* key = res->columns[c].name;
            char* val = NULL;
            if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
            printf("\"%s\": ", key);
            if (!val || strcmp(val, "NULL") == 0) {
                printf("null");
            } else {
                ValueType t = res->rows[r].values[c].type;
                if (t == VALUE_TYPE_INTEGER || t == VALUE_TYPE_DOUBLE) printf("%s", val);
                else {
                    putchar('"');
                    for (char* p = val; *p; p++) {
                        if (*p == '\\' || *p == '"') { putchar('\\'); putchar(*p); }
                        else if (*p == '\n') printf("\\n");
                        else putchar(*p);
                    }
                    putchar('"');
                }
            }
            if (val) free(val);
        }
        printf("}");
    }
    printf("\n]\n");
}

/* Markdown printer */
void print_markdown(ResultSet* res) {
    if (!res) return;
    for (int c = 0; c < res->column_count; c++) {
        if (c) printf(" |");
        printf(" %s", res->columns[c].name);
    }
    printf("\n");
    for (int c = 0; c < res->column_count; c++) {
        if (c) printf(" |");
        printf(" ---");
    }
    printf("\n");
    for (int r = 0; r < res->row_count; r++) {
        for (int c = 0; c < res->column_count; c++) {
            if (c) printf(" |");
            char* val = NULL;
            if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
            printf(" %s", val ? val : "");
            if (val) free(val);
        }
        printf("\n");
    }
}

/* YAML printer */
void print_yaml(ResultSet* res) {
    if (!res) return;
    for (int r = 0; r < res->row_count; r++) {
        printf("-\n");
        for (int c = 0; c < res->column_count; c++) {
            char* key = res->columns[c].name;
            char* val = NULL;
            if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
            printf("  %s: %s\n", key, val ? val : "null");
            if (val) free(val);
        }
    }
}

/* Vertical table printer (one column per line) */
void csv_print_table_vertical(CsvTable* table, int max_rows) {
    if (!table) return;
    int max_name_len = 0;
    for (int i = 0; i < table->column_count; i++) {
        int len = (int)strlen(table->columns[i].name);
        if (len > max_name_len) max_name_len = len;
    }

    int rows_to_print = (max_rows > 0 && max_rows < table->row_count) ? max_rows : table->row_count;
    for (int i = 0; i < rows_to_print; i++) {
        printf("*************************** %d. row ***************************\n", i + 1);
        for (int j = 0; j < table->column_count && j < table->rows[i].column_count; j++) {
            char* str = value_to_string(&table->rows[i].values[j]);
            printf("%*s: %s\n", max_name_len, table->columns[j].name, str);
            free(str);
        }
    }

    if (max_rows > 0 && table->row_count > max_rows) {
        printf("... (%d more rows)\n", table->row_count - max_rows);
    }
}

bool write_output_file(const char* filename, ResultSet* res, OutputFormat fmt, char delimiter) {
    if (!filename || !res) return false;
    if (fmt == FMT_AUTO) fmt = FMT_CSV;

    if (fmt == FMT_JSON) {
        FILE* f = fopen(filename, "w");
        if (!f) return false;
        fprintf(f, "[");
        for (int r = 0; r < res->row_count; r++) {
            if (r) fprintf(f, ",\n");
            fprintf(f, "  {");
            for (int c = 0; c < res->column_count; c++) {
                if (c) fprintf(f, ", ");
                char* key = res->columns[c].name;
                char* val = NULL;
                if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
                fprintf(f, "\"%s\": ", key);
                if (!val || strcmp(val, "NULL") == 0) {
                    fprintf(f, "null");
                } else {
                    ValueType t = res->rows[r].values[c].type;
                    if (t == VALUE_TYPE_INTEGER || t == VALUE_TYPE_DOUBLE) fprintf(f, "%s", val);
                    else {
                        fprintf(f, "\"");
                        for (char* p = val; *p; p++) {
                            if (*p == '\\' || *p == '"') { fprintf(f, "\\%c", *p); }
                            else if (*p == '\n') fprintf(f, "\\n");
                            else fputc(*p, f);
                        }
                        fprintf(f, "\"");
                    }
                }
                if (val) free(val);
            }
            fprintf(f, "}");
        }
        fprintf(f, "\n]\n");
        fclose(f);
        return true;
    }

    if (fmt == FMT_MARKDOWN) {
        FILE* f = fopen(filename, "w");
        if (!f) return false;
        for (int c = 0; c < res->column_count; c++) {
            if (c) fprintf(f, " |");
            fprintf(f, " %s", res->columns[c].name);
        }
        fprintf(f, "\n");
        for (int c = 0; c < res->column_count; c++) {
            if (c) fprintf(f, " |");
            fprintf(f, " ---");
        }
        fprintf(f, "\n");
        for (int r = 0; r < res->row_count; r++) {
            for (int c = 0; c < res->column_count; c++) {
                if (c) fprintf(f, " |");
                char* val = NULL;
                if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
                fprintf(f, " %s", val ? val : "");
                if (val) free(val);
            }
            fprintf(f, "\n");
        }
        fclose(f);
        return true;
    }

    if (fmt == FMT_YAML) {
        FILE* f = fopen(filename, "w");
        if (!f) return false;
        for (int r = 0; r < res->row_count; r++) {
            fprintf(f, "-\n");
            for (int c = 0; c < res->column_count; c++) {
                char* key = res->columns[c].name;
                char* val = NULL;
                if (c < res->rows[r].column_count) val = value_to_string(&res->rows[r].values[c]);
                fprintf(f, "  %s: %s\n", key, val ? val : "null");
                if (val) free(val);
            }
        }
        fclose(f);
        return true;
    }

    // default: CSV
    write_csv_file(filename, res, delimiter);
    return true;
}
