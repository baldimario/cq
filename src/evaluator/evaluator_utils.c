/* evaluator_utils.c - utility functions for query evaluation */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "evaluator.h"
#include "parser.h"
#include "csv_reader.h"
#include "string_utils.h"
#include "evaluator/evaluator_utils.h"
#include "evaluator/evaluator_aggregates.h"
#include "evaluator/evaluator_window.h"
#include "evaluator/evaluator_core.h"
#include "evaluator/evaluator_functions.h"
#include "evaluator/evaluator_internal.h"

/* forward declarations */
static int parse_function_arguments(const char* args_str, QueryContext* ctx, 
                                    Row* current_row, Value* out_args, int max_args);

/* deep copy a value to handle string duplication */
void value_deep_copy(Value* dst, const Value* src) {
    if (!dst || !src) return;
    
    dst->type = src->type;
    
    switch (src->type) {
        case VALUE_TYPE_NULL:
            dst->int_value = 0;
            break;
        case VALUE_TYPE_INTEGER:
            dst->int_value = src->int_value;
            break;
        case VALUE_TYPE_DOUBLE:
            dst->double_value = src->double_value;
            break;
        case VALUE_TYPE_DATE:
            dst->date_value = src->date_value;  // struct copy
            break;
        case VALUE_TYPE_STRING:
            dst->string_value = src->string_value ? strdup(src->string_value) : NULL;
            break;
    }
}

void trim_trailing_spaces(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && isspace(str[len - 1])) {
        str[--len] = '\0';
    }
}

/* helper to extract alias from column spec (part after AS keyword) */
char* extract_column_alias(const char* col_spec) {
    const char* as_pos = cq_strcasestr(col_spec, " AS ");
    if (as_pos) {
        return strdup(as_pos + 4);
    }
    return NULL;
}

/* helper to create and initialize ResultSet with schema from another ResultSet */
static ResultSet* create_result_set_schema(const char* filename, ResultSet* template) {
    ResultSet* result = calloc(1, sizeof(ResultSet));
    result->filename = strdup(filename);
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    result->column_count = template->column_count;
    
    // copy column definitions
    result->columns = malloc(sizeof(Column) * result->column_count);
    for (int i = 0; i < result->column_count; i++) {
        result->columns[i].name = strdup(template->columns[i].name);
        result->columns[i].inferred_type = template->columns[i].inferred_type;
    }
    
    return result;
}

/* parse function arguments from string like "arg1, 'literal', arg3", supports nested functions */
static int parse_function_arguments(const char* args_str, QueryContext* ctx, 
                                    Row* current_row, Value* out_args, int max_args) {
    if (!args_str || !out_args) return 0;
    
    int arg_count = 0;
    char* ptr = (char*)args_str;
    
    while (*ptr && arg_count < max_args) {
        // skip leading whitespace
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == '\0') break;
        
        char arg_buffer[512];
        int arg_len = 0;
        
        // check if it's a quoted string literal
        if (*ptr == '\'') {
            ptr++; // skip opening quote
            while (*ptr && *ptr != '\'' && arg_len < 511) {
                arg_buffer[arg_len++] = *ptr++;
            }
            if (*ptr == '\'') ptr++; // skip closing quote
            arg_buffer[arg_len] = '\0';
            
            out_args[arg_count].type = VALUE_TYPE_STRING;
            out_args[arg_count].string_value = strdup(arg_buffer);
            arg_count++;
        } else {
            // read until comma but skip commas inside parentheses for nested functions
            int paren_depth = 0;
            while (*ptr && (paren_depth > 0 || *ptr != ',') && arg_len < 511) {
                if (*ptr == '(') paren_depth++;
                else if (*ptr == ')') paren_depth--;
                arg_buffer[arg_len++] = *ptr++;
            }
            arg_buffer[arg_len] = '\0';
            
            // trim trailing whitespace
            while (arg_len > 0 && (arg_buffer[arg_len-1] == ' ' || arg_buffer[arg_len-1] == '\t')) {
                arg_buffer[--arg_len] = '\0';
            }
            
            if (arg_len == 0) {
                // empty argument, skip
                if (*ptr == ',') ptr++;
                continue;
            }
            
            // check if it's a nested function call, it should contains parentheses
            if (strchr(arg_buffer, '(')) {
                // parse nested function: FUNC_NAME(args...)
                char* nested_paren = strchr(arg_buffer, '(');
                *nested_paren = '\0';
                char nested_func_name[64];
                strncpy(nested_func_name, arg_buffer, sizeof(nested_func_name) - 1);
                nested_func_name[sizeof(nested_func_name) - 1] = '\0';
                
                // trim function name
                int name_len = strlen(nested_func_name);
                while (name_len > 0 && (nested_func_name[name_len-1] == ' ' || nested_func_name[name_len-1] == '\t')) {
                    nested_func_name[--name_len] = '\0';
                }
                
                // extract nested function arguments
                char* nested_args_start = nested_paren + 1;
                char* nested_paren_close = strrchr(nested_args_start, ')');
                if (nested_paren_close) *nested_paren_close = '\0';
                
                // recursively parse nested function arguments
                Value nested_args[10];
                int nested_arg_count = parse_function_arguments(nested_args_start, ctx, current_row, nested_args, 10);
                
                // evaluate nested function
                out_args[arg_count] = evaluate_scalar_function(nested_func_name, nested_args, nested_arg_count);
                
                // free temporary string arguments from nested call
                for (int i = 0; i < nested_arg_count; i++) {
                    if (nested_args[i].type == VALUE_TYPE_STRING && nested_args[i].string_value) {
                        free((char*)nested_args[i].string_value);
                    }
                }
                arg_count++;
            }
            // determine if it's a number or column name
            else if (isdigit(arg_buffer[0]) || (arg_buffer[0] == '-' && arg_buffer[1] && isdigit(arg_buffer[1]))) {
                // numeric literal
                out_args[arg_count] = parse_value(arg_buffer, strlen(arg_buffer));
                arg_count++;
            } else {
                // column name lookup
                int col_idx = find_column_index(ctx->tables[0].table, arg_buffer);
                
                if (col_idx >= 0 && current_row) {
                    value_deep_copy(&out_args[arg_count], &current_row->values[col_idx]);
                } else {
                    out_args[arg_count].type = VALUE_TYPE_NULL;
                }
                arg_count++;
            }
        }
        
        // skip comma
        if (*ptr == ',') ptr++;
    }
    
    return arg_count;
}

/* evaluate a column expression that can be a function or simple column reference */
Value evaluate_column_expression(const char* col_spec, QueryContext* ctx, 
                                        Row* current_row, int* column_indices, int col_index) {
    Value result;
    result.type = VALUE_TYPE_NULL;
    
    if (!col_spec || !ctx) return result;
    
    char col_spec_clean[256];
    strncpy(col_spec_clean, col_spec, sizeof(col_spec_clean) - 1);
    col_spec_clean[sizeof(col_spec_clean) - 1] = '\0';
    
    // remove " AS alias" part
    char* as_pos = (char*)cq_strcasestr(col_spec_clean, " AS ");
    if (as_pos) *as_pos = '\0';
    
    // check if it's a function call
    char* paren = strchr(col_spec_clean, '(');
    if (paren) {
        // parse function name
        *paren = '\0';
        char func_name[64];
        strncpy(func_name, col_spec_clean, sizeof(func_name) - 1);
        func_name[sizeof(func_name) - 1] = '\0';
        
        // extract arguments
        char* args_start = paren + 1;
        char* paren_close = strrchr(args_start, ')');
        if (paren_close) *paren_close = '\0';
        
        // parse and evaluate function arguments
        Value func_args[10];
        int arg_count = parse_function_arguments(args_start, ctx, current_row, func_args, 10);
        
        // evaluate scalar function
        result = evaluate_scalar_function(func_name, func_args, arg_count);
        
        // free temporary string arguments
        for (int i = 0; i < arg_count; i++) {
            if (func_args[i].type == VALUE_TYPE_STRING && func_args[i].string_value) {
                free((char*)func_args[i].string_value);
            }
        }
    } else {
        // regular column, get value from row
        int src_col_idx = column_indices ? column_indices[col_index] : -1;
        
        if (src_col_idx >= 0 && current_row && src_col_idx < current_row->column_count) {
            value_deep_copy(&result, &current_row->values[src_col_idx]);
        }
    }
    
    return result;
}

/* build result for non-aggregated queries */
ResultSet* build_result(QueryContext* ctx, Row** filtered_rows, int row_count) {
    if (!ctx || !ctx->query) return NULL;
    
    // create result table
    ResultSet* result = calloc(1, sizeof(ResultSet));
    result->filename = strdup("query_result");
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    // get selected columns from SELECT clause
    ASTNode* select_node = ctx->query->query.select;
    if (!select_node) return result;
    
    // check if we have * in the column list - need to expand it
    bool has_star = false;
    for (int i = 0; i < select_node->select.column_count; i++) {
        if (strcmp(select_node->select.columns[i], "*") == 0) {
            has_star = true;
            break;
        }
    }
    
    if (has_star) {
        // expand * to all columns
        int total_columns = (select_node->select.column_count - 1) + ctx->tables[0].table->column_count;
        result->column_count = total_columns;
        result->columns = malloc(sizeof(Column) * total_columns);
        
        // build column specs array with * expanded
        char** expanded_specs = malloc(sizeof(char*) * total_columns);
        int* column_indices = malloc(sizeof(int) * total_columns);
        int* original_indices = malloc(sizeof(int) * total_columns); // track original column index
        int col_idx = 0;
        
        for (int i = 0; i < select_node->select.column_count; i++) {
            if (strcmp(select_node->select.columns[i], "*") == 0) {
                // expand * to all table columns
                for (int j = 0; j < ctx->tables[0].table->column_count; j++) {
                    expanded_specs[col_idx] = strdup(ctx->tables[0].table->columns[j].name);
                    result->columns[col_idx].name = strdup(ctx->tables[0].table->columns[j].name);
                    result->columns[col_idx].inferred_type = VALUE_TYPE_STRING;
                    column_indices[col_idx] = j;
                    original_indices[col_idx] = -1; // star columns don't have AST nodes
                    col_idx++;
                }
            } else {
                // regular column or function
                expanded_specs[col_idx] = strdup(select_node->select.columns[i]);
                original_indices[col_idx] = i; // track which original column this is
                
                // parse column name for display
                const char* col_spec = select_node->select.columns[i];
                char col_name[256];
                const char* as_pos = cq_strcasestr(col_spec, " AS ");
                if (as_pos) {
                    const char* alias_start = as_pos + 4;
                    result->columns[col_idx].name = strdup(alias_start);
                    
                    int col_len = as_pos - col_spec;
                    strncpy(col_name, col_spec, col_len);
                    col_name[col_len] = '\0';
                } else {
                    strcpy(col_name, col_spec);
                    if (strchr(col_name, '(')) {
                        result->columns[col_idx].name = strdup(col_name);
                    } else {
                        const char* dot = strchr(col_name, '.');
                        if (dot) {
                            result->columns[col_idx].name = strdup(dot + 1);
                        } else {
                            result->columns[col_idx].name = strdup(col_name);
                        }
                    }
                }
                
                result->columns[col_idx].inferred_type = VALUE_TYPE_STRING;
                
                // find column index (if not a function)
                column_indices[col_idx] = -1;
                if (!strchr(col_name, '(')) {
                    column_indices[col_idx] = find_column_index(ctx->tables[0].table, col_name);
                }
                
                col_idx++;
            }
        }
        
        // build rows with expanded columns
        result->row_count = row_count;
        result->row_capacity = row_count;
        result->rows = malloc(sizeof(Row) * row_count);
        
        for (int i = 0; i < row_count; i++) {
            result->rows[i].column_count = result->column_count;
            result->rows[i].values = malloc(sizeof(Value) * result->column_count);
            
            for (int j = 0; j < result->column_count; j++) {
                // check if this column has an AST node (expression, subquery, etc.)
                int orig_idx = original_indices[j];
                if (orig_idx >= 0 && select_node->select.column_nodes && 
                    select_node->select.column_nodes[orig_idx]) {
                    ASTNode* col_node = select_node->select.column_nodes[orig_idx];
                    
                    if (col_node->type == NODE_TYPE_SUBQUERY) {
                        // evaluate scalar subquery that may be correlated
                        ResultSet* subquery_result = evaluate_query_internal(col_node->subquery.query, 
                            filtered_rows[i], ctx->tables[0].table);
                        
                        if (!subquery_result) {
                            result->rows[i].values[j].type = VALUE_TYPE_NULL;
                        } else if (subquery_result->row_count != 1 || subquery_result->column_count != 1) {
                            fprintf(stderr, "error: scalar subquery must return exactly one row and one column (got %d rows, %d columns)\n",
                                    subquery_result->row_count, subquery_result->column_count);
                            csv_free(subquery_result);
                            result->rows[i].values[j].type = VALUE_TYPE_NULL;
                        } else {
                            value_deep_copy(&result->rows[i].values[j], &subquery_result->rows[0].values[0]);
                            csv_free(subquery_result);
                        }
                    } else if (col_node->type == NODE_TYPE_WINDOW_FUNCTION) {
                        // window functions evaluated separately
                        result->rows[i].values[j].type = VALUE_TYPE_NULL;
                    } else {
                        // evaluate any expression like identifier, binary_op, function, etc.
                        Value tmp = evaluate_expression(ctx, col_node, filtered_rows[i], 0);
                        result->rows[i].values[j] = value_copy(&tmp);
                    }
                } else {
                    // regular column from table or string-based expression
                    Value tmp = evaluate_column_expression(
                        expanded_specs[j], ctx, filtered_rows[i], column_indices, j
                    );
                    result->rows[i].values[j] = value_copy(&tmp);
                }
            }
        }
        
        // evaluate window functions (after all rows are created)
        if (select_node->select.column_nodes) {
            for (int j = 0; j < result->column_count; j++) {
                int orig_idx = original_indices[j];
                if (orig_idx >= 0) {
                    ASTNode* col_node = select_node->select.column_nodes[orig_idx];
                    if (col_node && col_node->type == NODE_TYPE_WINDOW_FUNCTION) {
                        Value* win_results = evaluate_window_function(col_node, ctx, filtered_rows, row_count);
                        if (win_results) {
                            for (int i = 0; i < row_count; i++) {
                                value_deep_copy(&result->rows[i].values[j], &win_results[i]);
                                if (win_results[i].type == VALUE_TYPE_STRING && win_results[i].string_value) {
                                    free((char*)win_results[i].string_value);
                                }
                            }
                            free(win_results);
                        }
                    }
                }
            }
        }
        
        for (int i = 0; i < result->column_count; i++) {
            free(expanded_specs[i]);
        }
        free(expanded_specs);
        free(column_indices);
        free(original_indices);
        
        return result;
    }
    
    // normal SELECT without * 
    result->column_count = select_node->select.column_count;
    result->columns = malloc(sizeof(Column) * result->column_count);
    
    // build column index mapping and extract aliases
    int* column_indices = malloc(sizeof(int) * result->column_count);
    char** column_specs = malloc(sizeof(char*) * result->column_count);  // store full specs for functions
    
    for (int i = 0; i < result->column_count; i++) {
        const char* col_spec = select_node->select.columns[i];
        column_specs[i] = strdup(col_spec);
        char col_name[256];
        
        // parse "column" or "column AS alias"
        char* alias = extract_column_alias(col_spec);
        if (alias) {
            // extract column name
            const char* as_pos = cq_strcasestr(col_spec, " AS ");
            int col_len = as_pos - col_spec;
            strncpy(col_name, col_spec, col_len);
            col_name[col_len] = '\0';
            
            result->columns[i].name = alias;
        } else {
            strcpy(col_name, col_spec);
            
            // check if it's a function - if so, use function call as display name
            if (strchr(col_name, '(')) {
                result->columns[i].name = strdup(col_name);
            } else {
                // for regular columns, use only column name without table prefix
                const char* dot = strchr(col_name, '.');
                if (dot) {
                    result->columns[i].name = strdup(dot + 1);
                } else {
                    result->columns[i].name = strdup(col_name);
                }
            }
        }
        
        result->columns[i].inferred_type = VALUE_TYPE_STRING;
        column_indices[i] = -1;
        
        // skip column lookup if it's a function (will be evaluated later)
        if (strchr(col_name, '(')) {
            continue;
        }
        
        // find column in source table
        column_indices[i] = find_column_index(ctx->tables[0].table, col_name);
    }
    
    // build rows
    result->row_count = row_count;
    result->row_capacity = row_count;
    result->rows = malloc(sizeof(Row) * row_count);
    
    for (int i = 0; i < row_count; i++) {
        result->rows[i].column_count = result->column_count;
        result->rows[i].values = malloc(sizeof(Value) * result->column_count);
        
        // copy values from filtered row based on column mapping
        for (int j = 0; j < result->column_count; j++) {
            // check if this column has an AST node (expression, subquery, etc.)
            if (select_node->select.column_nodes && select_node->select.column_nodes[j]) {
                ASTNode* col_node = select_node->select.column_nodes[j];
                
                if (col_node->type == NODE_TYPE_SUBQUERY) {
                    // evaluate scalar subquery that may be correlated
                    ResultSet* subquery_result = evaluate_query_internal(col_node->subquery.query, 
                        filtered_rows[i], ctx->tables[0].table);
                    
                    // validation, it must return exactly 1 row and 1 column
                    if (!subquery_result) {
                        result->rows[i].values[j].type = VALUE_TYPE_NULL;
                    } else if (subquery_result->row_count != 1 || subquery_result->column_count != 1) {
                        fprintf(stderr, "error: scalar subquery must return exactly one row and one column (got %d rows, %d columns)\n",
                                subquery_result->row_count, subquery_result->column_count);
                        csv_free(subquery_result);
                        result->rows[i].values[j].type = VALUE_TYPE_NULL;
                    } else {
                        // copy the single value from subquery result
                        value_deep_copy(&result->rows[i].values[j], &subquery_result->rows[0].values[0]);
                        csv_free(subquery_result);
                    }
                } else if (col_node->type == NODE_TYPE_WINDOW_FUNCTION) {
                    // window functions are evaluated separately for all rows at once
                    // skip here, will be handled after all rows are created
                    result->rows[i].values[j].type = VALUE_TYPE_NULL;
                } else {
                    // evaluate any expression like identifier, binary_op, function, etc.
                    Value tmp = evaluate_expression(ctx, col_node, filtered_rows[i], 0);
                    result->rows[i].values[j] = value_copy(&tmp);
                }
            } else {
                Value tmp = evaluate_column_expression(
                    column_specs[j], ctx, filtered_rows[i], column_indices, j
                );
                result->rows[i].values[j] = value_copy(&tmp);
            }
        }
    }
    
    // free temporary arrays
    for (int i = 0; i < result->column_count; i++) {
        free(column_specs[i]);
    }
    free(column_specs);
    free(column_indices);
    
    // evaluate window functions (after all rows are created)
    if (select_node->select.column_nodes) {
        for (int j = 0; j < result->column_count; j++) {
            ASTNode* col_node = select_node->select.column_nodes[j];
            if (col_node && col_node->type == NODE_TYPE_WINDOW_FUNCTION) {
                Value* win_results = evaluate_window_function(col_node, ctx, filtered_rows, row_count);
                if (win_results) {
                    for (int i = 0; i < row_count; i++) {
                        value_deep_copy(&result->rows[i].values[j], &win_results[i]);
                        if (win_results[i].type == VALUE_TYPE_STRING && win_results[i].string_value) {
                            free((char*)win_results[i].string_value);
                        }
                    }
                    free(win_results);
                }
            }
        }
    }
    
    return result;
}

typedef struct {
    ResultSet* result;
    int column_index;
    bool descending;
} ResultSortContext;

/* global context for result sorting */
static ResultSortContext* g_result_sort_ctx = NULL;

/* compare function for result sorting */
static int compare_result_rows(const void* a, const void* b) {
    ResultSortContext* ctx = g_result_sort_ctx;
    Row* row_a = (Row*)a;
    Row* row_b = (Row*)b;
    
    if (!ctx) return 0;
    
    if (ctx->column_index < 0 || ctx->column_index >= row_a->column_count) {
        return 0;
    }
    
    Value* val_a = &row_a->values[ctx->column_index];
    Value* val_b = &row_b->values[ctx->column_index];
    
    int cmp = value_compare(val_a, val_b);
    return ctx->descending ? -cmp : cmp;
}

void sort_result(ResultSet* result, ASTNode* select_node, const char* column_spec, bool descending) {
    if (!result || result->row_count == 0) return;
    
    // parse column specification that might be a function like AVG(t.height) or simple column like t.age
    char lookup_name[256];
    
    // check if it's a function call first
    char* paren = strchr(column_spec, '(');
    if (paren) {
        // for functions like AVG(t.height), extract function name and argument
        char func_name[64];
        int func_len = paren - column_spec;
        strncpy(func_name, column_spec, func_len);
        func_name[func_len] = '\0';
        
        // extract column argument
        char* arg_start = paren + 1;
        char* paren_close = strchr(arg_start, ')');
        if (paren_close) {
            int arg_len = paren_close - arg_start;
            char arg_buf[128];
            strncpy(arg_buf, arg_start, arg_len);
            arg_buf[arg_len] = '\0';
            
            // strip table prefix from argument e.g., t.height -> height
            const char* arg_dot = strchr(arg_buf, '.');
            const char* arg_name = arg_dot ? arg_dot + 1 : arg_buf;
            
            // build the column name to search: FUNC(column)    
            snprintf(lookup_name, sizeof(lookup_name), "%s(%s)", func_name, arg_name);
        }
    } else {
        // simple column, strip table prefix if present
        const char* dot = strchr(column_spec, '.');
        if (dot) {
            snprintf(lookup_name, sizeof(lookup_name), "%s", dot + 1);
        } else {
            snprintf(lookup_name, sizeof(lookup_name), "%s", column_spec);
        }
    }
    
    // find column index in result, try matching by alias first, then by expression
    int col_idx = -1;
    for (int i = 0; i < result->column_count; i++) {
        if (strcasecmp(result->columns[i].name, lookup_name) == 0) {
            col_idx = i;
            break;
        }
    }
    
    // if not found by display name, try matching against SELECT expressions
    if (col_idx < 0 && select_node) {
        for (int i = 0; i < select_node->select.column_count; i++) {
            const char* col_spec = select_node->select.columns[i];
            
            // extract the expression part before AS if present
            char expr_buf[256];
            char* alias = extract_column_alias(col_spec);
            if (alias) {
                const char* as_pos = cq_strcasestr(col_spec, " AS ");
                int len = as_pos - col_spec;
                strncpy(expr_buf, col_spec, len);
                expr_buf[len] = '\0';
                free(alias);
            } else {
                strncpy(expr_buf, col_spec, sizeof(expr_buf) - 1);
                expr_buf[sizeof(expr_buf) - 1] = '\0';
            }
            
            // trim spaces
            trim_trailing_spaces(expr_buf);
            
            // normalize both expressions for comparison stripping table prefixes
            char normalized_expr[256];
            char* expr_paren = strchr(expr_buf, '(');
            if (expr_paren) {
                // for functions extract func name and arg
                char expr_func[64];
                int expr_func_len = expr_paren - expr_buf;
                strncpy(expr_func, expr_buf, expr_func_len);
                expr_func[expr_func_len] = '\0';
                
                char* expr_arg_start = expr_paren + 1;
                char* expr_paren_close = strchr(expr_arg_start, ')');
                if (expr_paren_close) {
                    int expr_arg_len = expr_paren_close - expr_arg_start;
                    char expr_arg_buf[128];
                    strncpy(expr_arg_buf, expr_arg_start, expr_arg_len);
                    expr_arg_buf[expr_arg_len] = '\0';
                    
                    const char* expr_arg_dot = strchr(expr_arg_buf, '.');
                    const char* expr_arg_name = expr_arg_dot ? expr_arg_dot + 1 : expr_arg_buf;
                    
                    snprintf(normalized_expr, sizeof(normalized_expr), "%s(%s)", expr_func, expr_arg_name);
                }
            } else {
                const char* expr_dot = strchr(expr_buf, '.');
                snprintf(normalized_expr, sizeof(normalized_expr), "%s", expr_dot ? expr_dot + 1 : expr_buf);
            }
            
            // compare normalized expressions
            if (strcasecmp(normalized_expr, lookup_name) == 0) {
                col_idx = i;
                break;
            }
        }
    }
    
    if (col_idx < 0) {
        fprintf(stderr, "warning: cannot sort by unknown column '%s' (looked for '%s')\n", column_spec, lookup_name);
        return;
    }
    
    ResultSortContext sort_ctx;
    sort_ctx.result = result;
    sort_ctx.column_index = col_idx;
    sort_ctx.descending = descending;
    
    g_result_sort_ctx = &sort_ctx;
    qsort(result->rows, result->row_count, sizeof(Row), compare_result_rows);
    g_result_sort_ctx = NULL;
}

/* helper to apply LIMIT and OFFSET to result */
void apply_limit_offset(ResultSet* result, int limit, int offset) {
    if (limit < 0 && offset < 0) return;
    
    int actual_offset = offset >= 0 ? offset : 0;
    int actual_limit = limit >= 0 ? limit : result->row_count;
    
    if (actual_offset >= result->row_count) {
        free_row_range(result->rows, 0, result->row_count);
        result->row_count = 0;
        return;
    }
    
    int start = actual_offset;
    int count = actual_limit;
    if (start + count > result->row_count) {
        count = result->row_count - start;
    }
    
    // free rows before offset
    free_row_range(result->rows, 0, start);
    
    // free rows after limit
    free_row_range(result->rows, start + count, result->row_count);
    
    // shift remaining rows to start of array if needed
    if (start > 0 && count > 0) {
        memmove(result->rows, result->rows + start, sizeof(Row) * count);
    }
    
    result->row_count = count;
}

/* helper function to check if two rows are equal (all values match) */
static bool rows_equal(Row* row1, Row* row2, int column_count) {
    if (!row1 || !row2) return false;
    
    for (int i = 0; i < column_count; i++) {
        if (value_compare(&row1->values[i], &row2->values[i]) != 0) {
            return false;
        }
    }
    return true;
}

/* helper function to check if a row exists in a result set */
static bool row_exists_in_result(ResultSet* result, Row* row) {
    for (int i = 0; i < result->row_count; i++) {
        if (rows_equal(&result->rows[i], row, result->column_count)) {
            return true;
        }
    }
    return false;
}

/* helper function to copy a row */
static Row copy_row(Row* source, int column_count) {
    Row new_row;
    new_row.column_count = column_count;
    new_row.values = malloc(sizeof(Value) * column_count);
    
    for (int i = 0; i < column_count; i++) {
        value_deep_copy(&new_row.values[i], &source->values[i]);
    }
    
    return new_row;
}

/* UNION operation - combine two result sets (optionally removing duplicates) */
ResultSet* set_union(ResultSet* left, ResultSet* right, bool include_duplicates) {
    if (!left || !right) return NULL;
    
    // create result with same schema as left
    ResultSet* result = create_result_set_schema("union_result", left);
    
    // allocate rows
    result->row_capacity = left->row_count + right->row_count;
    result->rows = malloc(sizeof(Row) * result->row_capacity);
    result->row_count = 0;
    
    // add all rows from left
    for (int i = 0; i < left->row_count; i++) {
        result->rows[result->row_count++] = copy_row(&left->rows[i], left->column_count);
    }
    
    // add rows from right
    for (int i = 0; i < right->row_count; i++) {
        // if not UNION ALL, check for duplicates
        if (!include_duplicates && row_exists_in_result(result, &right->rows[i])) {
            continue;
        }
        result->rows[result->row_count++] = copy_row(&right->rows[i], right->column_count);
    }
    
    return result;
}

/* INTERSECT operation - return rows that exist in both result sets */
ResultSet* set_intersect(ResultSet* left, ResultSet* right) {
    if (!left || !right) return NULL;
    
    // create result with same schema as left
    ResultSet* result = create_result_set_schema("intersect_result", left);
    
    // allocate rows (at most left->row_count)
    result->row_capacity = left->row_count;
    result->rows = malloc(sizeof(Row) * result->row_capacity);
    result->row_count = 0;
    
    // add rows from left that also exist in right
    for (int i = 0; i < left->row_count; i++) {
        // check if this row exists in right
        bool found = false;
        for (int j = 0; j < right->row_count; j++) {
            if (rows_equal(&left->rows[i], &right->rows[j], left->column_count)) {
                found = true;
                break;
            }
        }
        
        if (found) {
            // avoid duplicates in result
            if (!row_exists_in_result(result, &left->rows[i])) {
                result->rows[result->row_count++] = copy_row(&left->rows[i], left->column_count);
            }
        }
    }
    
    return result;
}

/* EXCEPT operation - return rows from left that don't exist in right */
ResultSet* set_except(ResultSet* left, ResultSet* right) {
    if (!left || !right) return NULL;
    
    // create result with same schema as left
    ResultSet* result = create_result_set_schema("except_result", left);
    
    // allocate rows (at most left->row_count)
    result->row_capacity = left->row_count;
    result->rows = malloc(sizeof(Row) * result->row_capacity);
    result->row_count = 0;
    
    // add rows from left that don't exist in right
    for (int i = 0; i < left->row_count; i++) {
        // check if this row exists in right
        bool found = false;
        for (int j = 0; j < right->row_count; j++) {
            if (rows_equal(&left->rows[i], &right->rows[j], left->column_count)) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // avoid duplicates in result
            if (!row_exists_in_result(result, &left->rows[i])) {
                result->rows[result->row_count++] = copy_row(&left->rows[i], left->column_count);
            }
        }
    }
    
    return result;
}

/* remove duplicate rows from result set for DISTINCT */
void apply_distinct(ResultSet* result) {
    if (!result || result->row_count <= 1) return;
    
    // hash-based deduplication using a simple approach:
    // we'll track which rows to keep and compact the array
    bool* keep = calloc(result->row_count, sizeof(bool));
    int unique_count = 0;
    
    // compare each row with previously kept rows
    for (int i = 0; i < result->row_count; i++) {
        bool is_duplicate = false;
        
        // check if this row is duplicate of any previous kept row
        for (int j = 0; j < i; j++) {
            if (!keep[j]) continue;
            
            // compare all values in the row
            bool rows_equal_flag = true;
            for (int col = 0; col < result->column_count; col++) {
                if (value_compare(&result->rows[i].values[col], 
                                 &result->rows[j].values[col]) != 0) {
                    rows_equal_flag = false;
                    break;
                }
            }
            
            if (rows_equal_flag) {
                is_duplicate = true;
                break;
            }
        }
        
        if (!is_duplicate) {
            keep[i] = true;
            unique_count++;
        }
    }
    
    // if all rows are unique, nothing to do
    if (unique_count == result->row_count) {
        free(keep);
        return;
    }
    
    // compact the rows array, keeping only unique rows
    int write_pos = 0;
    for (int i = 0; i < result->row_count; i++) {
        if (keep[i]) {
            if (write_pos != i) {
                // move row to write position
                result->rows[write_pos] = result->rows[i];
            }
            write_pos++;
        } else {
            // free duplicate row
            for (int col = 0; col < result->column_count; col++) {
                value_free(&result->rows[i].values[col]);
            }
            free(result->rows[i].values);
        }
    }
    
    result->row_count = unique_count;
    free(keep);
}

/* convert result set to csv table */
CsvTable* result_to_csv_table(ResultSet* result) {
    if (!result) return NULL;
    
    CsvTable* table = calloc(1, sizeof(CsvTable));
    table->filename = strdup(result->filename);
    table->has_header = result->has_header;
    table->delimiter = result->delimiter;
    table->quote = result->quote;
    
    // copy columns
    table->column_count = result->column_count;
    table->columns = malloc(sizeof(Column) * table->column_count);
    for (int i = 0; i < table->column_count; i++) {
        table->columns[i].name = strdup(result->columns[i].name);
        table->columns[i].inferred_type = result->columns[i].inferred_type;
    }
    
    // copy rows
    table->row_count = result->row_count;
    table->row_capacity = result->row_capacity;
    table->rows = malloc(sizeof(Row) * table->row_count);
    
    for (int i = 0; i < table->row_count; i++) {
        table->rows[i].column_count = result->rows[i].column_count;
        table->rows[i].values = malloc(sizeof(Value) * table->rows[i].column_count);
        
        for (int j = 0; j < table->rows[i].column_count; j++) {
            Value* src = &result->rows[i].values[j];
            Value* dst = &table->rows[i].values[j];
            
            value_deep_copy(dst, src);
        }
    }
    
    return table;
}

/* helper to free a range of rows */
void free_row_range(Row* rows, int start, int end) {
    for (int i = start; i < end; i++) {
        for (int j = 0; j < rows[i].column_count; j++) {
            if (rows[i].values[j].type == VALUE_TYPE_STRING && 
                rows[i].values[j].string_value) {
                free((char*)rows[i].values[j].string_value);
            }
        }
        free(rows[i].values);
    }
}

/* helper to apply WHERE filtering */
Row** filter_rows(QueryContext* ctx, ASTNode* where_clause, int* out_filtered_count) {
    int filtered_capacity = ctx->tables[0].table->row_count;
    Row** filtered_rows = malloc(sizeof(Row*) * filtered_capacity);
    int filtered_count = 0;
    
    for (int i = 0; i < ctx->tables[0].table->row_count; i++) {
        Row* row = &ctx->tables[0].table->rows[i];
        
        bool matches = true;
        if (where_clause) {
            matches = evaluate_condition(ctx, where_clause, row, 0);
        }
        
        if (matches) {
            filtered_rows[filtered_count++] = row;
        }
    }
    
    *out_filtered_count = filtered_count;
    return filtered_rows;
}
