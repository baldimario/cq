#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "evaluator.h"
#include "parser.h"
#include "csv_reader.h"
#include "string_utils.h"
#include "evaluator/evaluator_aggregates.h"

/* forward declarations for functions defined in other evaluator modules */
extern Value evaluate_expression(QueryContext* ctx, ASTNode* expr, Row* current_row, int table_index);
extern void value_deep_copy(Value* dst, const Value* src);
extern char* extract_column_alias(const char* col_spec);
extern Value evaluate_column_expression(const char* col_spec, QueryContext* ctx, Row* current_row, int* column_indices, int col_index);
extern void trim_trailing_spaces(char* str);

/* helper to get column index using csv_get_column_index with fallback to strip table prefix */
int find_column_index_with_fallback(CsvTable* table, const char* col_name) {
    if (!table || !col_name) return -1;
    
    // try exact match first for joined tables with prefixed columns
    int col_idx = csv_get_column_index(table, col_name);
    
    // if not found, try stripping table prefix
    if (col_idx < 0) {
        const char* dot = strchr(col_name, '.');
        if (dot) {
            const char* lookup_name = dot + 1;
            col_idx = csv_get_column_index(table, lookup_name);
        }
    }
    
    return col_idx;
}

/* alias for find_column_index_with_fallback for backward compatibility */
int find_column_index(CsvTable* table, const char* col_name) {
    return find_column_index_with_fallback(table, col_name);
}

bool is_aggregate_function(const char* func_name) {
    return strcasecmp(func_name, "COUNT") == 0 ||
           strcasecmp(func_name, "SUM") == 0 ||
           strcasecmp(func_name, "AVG") == 0 ||
           strcasecmp(func_name, "MIN") == 0 ||
           strcasecmp(func_name, "MAX") == 0 ||
           strcasecmp(func_name, "STDDEV") == 0 ||
           strcasecmp(func_name, "STDDEV_POP") == 0 ||
           strcasecmp(func_name, "MEDIAN") == 0;
}

/* helper to check if a SELECT clause contains any aggregate functions */
bool has_aggregate_functions(ASTNode* select_node) {
    if (!select_node || select_node->type != NODE_TYPE_SELECT) return false;
    
    // if we have column nodes (parsed AST), check those for window functions
    if (select_node->select.column_nodes) {
        for (int i = 0; i < select_node->select.column_count; i++) {
            ASTNode* col_node = select_node->select.column_nodes[i];
            if (!col_node) continue;
            
            // skip window functions (they're not regular aggregates)
            if (col_node->type == NODE_TYPE_WINDOW_FUNCTION) {
                continue;
            }
            
            // check if it's a regular aggregate function
            if (col_node->type == NODE_TYPE_FUNCTION) {
                const char* func_name = col_node->function.name;
                if (strcasecmp(func_name, "COUNT") == 0 || strcasecmp(func_name, "SUM") == 0 ||
                    strcasecmp(func_name, "AVG") == 0 || strcasecmp(func_name, "MIN") == 0 ||
                    strcasecmp(func_name, "MAX") == 0 || strcasecmp(func_name, "STDDEV") == 0 ||
                    strcasecmp(func_name, "MEDIAN") == 0) {
                    return true;
                }
            }
        }
        return false;
    }
    
    // fallback to string checking if nodes not available
    for (int i = 0; i < select_node->select.column_count; i++) {
        const char* col_spec = select_node->select.columns[i];
        
        // check if column spec contains an aggregate function (but not a window function)
        bool has_aggregate = false;
        if (strstr(col_spec, "COUNT(") || strstr(col_spec, "SUM(") ||
            strstr(col_spec, "AVG(") || strstr(col_spec, "MIN(") || strstr(col_spec, "MAX(") ||
            strstr(col_spec, "STDDEV(") || strstr(col_spec, "MEDIAN(")) {
            has_aggregate = true;
        }
        
        // but exclude window functions (those with OVER clause)
        if (has_aggregate && cq_strcasestr(col_spec, "OVER")) {
            has_aggregate = false; // this is a window function, not a regular aggregate
        }
        
        if (has_aggregate) {
            return true;
        }
    }
    
    return false;
}

GroupResult* create_groups(Row** rows, int row_count, CsvTable* table, const char* group_column) {
    GroupResult* result = calloc(1, sizeof(GroupResult));
    result->group_capacity = 16;
    result->groups = malloc(sizeof(GroupedRows) * result->group_capacity);
    result->group_count = 0;
    
    int group_col_idx = find_column_index_with_fallback(table, group_column);
    
    if (group_col_idx < 0) return result;
    
    for (int i = 0; i < row_count; i++) {
        Value* group_val = &rows[i]->values[group_col_idx];
        
        // convert value to string for grouping key
        char key_buf[256];
        switch (group_val->type) {
            case VALUE_TYPE_NULL:
                strcpy(key_buf, "NULL");
                break;
            case VALUE_TYPE_INTEGER:
                snprintf(key_buf, sizeof(key_buf), "%lld", group_val->int_value);
                break;
            case VALUE_TYPE_DOUBLE:
                snprintf(key_buf, sizeof(key_buf), "%.6f", group_val->double_value);
                break;
            case VALUE_TYPE_DATE:
                snprintf(key_buf, sizeof(key_buf), "%04d-%02d-%02d",
                         group_val->date_value.year, group_val->date_value.month, group_val->date_value.day);
                break;
            case VALUE_TYPE_STRING:
                strncpy(key_buf, group_val->string_value, sizeof(key_buf) - 1);
                key_buf[sizeof(key_buf) - 1] = '\0';
                break;
        }
        
        // find or create group
        int group_idx = -1;
        for (int j = 0; j < result->group_count; j++) {
            if (strcmp(result->groups[j].group_key, key_buf) == 0) {
                group_idx = j;
                break;
            }
        }
        
        if (group_idx < 0) {
            // create new group
            if (result->group_count >= result->group_capacity) {
                result->group_capacity *= 2;
                result->groups = realloc(result->groups, sizeof(GroupedRows) * result->group_capacity);
            }
            
            group_idx = result->group_count++;
            result->groups[group_idx].group_key = strdup(key_buf);
            result->groups[group_idx].row_capacity = 16;
            result->groups[group_idx].rows = malloc(sizeof(Row*) * result->groups[group_idx].row_capacity);
            result->groups[group_idx].row_count = 0;
        }
        
        // add row to group
        GroupedRows* group = &result->groups[group_idx];
        if (group->row_count >= group->row_capacity) {
            group->row_capacity *= 2;
            group->rows = realloc(group->rows, sizeof(Row*) * group->row_capacity);
        }
        group->rows[group->row_count++] = rows[i];
    }
    
    return result;
}

/* create groups by evaluating a SELECT expression for each row */
GroupResult* create_groups_by_expression(QueryContext* ctx, Row** rows, int row_count, 
                                                 ASTNode* group_expr) {
    GroupResult* result = calloc(1, sizeof(GroupResult));
    result->group_capacity = 16;
    result->groups = malloc(sizeof(GroupedRows) * result->group_capacity);
    result->group_count = 0;
    
    for (int i = 0; i < row_count; i++) {
        // evaluate the grouping expression for this row
        Value group_val = evaluate_expression(ctx, group_expr, rows[i], 0);
        
        // convert value to string for grouping key
        char key_buf[256];
        switch (group_val.type) {
            case VALUE_TYPE_NULL:
                strcpy(key_buf, "NULL");
                break;
            case VALUE_TYPE_INTEGER:
                snprintf(key_buf, sizeof(key_buf), "%lld", group_val.int_value);
                break;
            case VALUE_TYPE_DOUBLE:
                snprintf(key_buf, sizeof(key_buf), "%.6f", group_val.double_value);
                break;
            case VALUE_TYPE_DATE:
                snprintf(key_buf, sizeof(key_buf), "%04d-%02d-%02d",
                         group_val.date_value.year, group_val.date_value.month, group_val.date_value.day);
                break;
            case VALUE_TYPE_STRING:
                strncpy(key_buf, group_val.string_value, sizeof(key_buf) - 1);
                key_buf[sizeof(key_buf) - 1] = '\0';
                break;
        }
        
        // free string value if allocated
        if (group_val.type == VALUE_TYPE_STRING && group_val.string_value) {
            free((char*)group_val.string_value);
        }
        
        // find or create group
        int group_idx = -1;
        for (int j = 0; j < result->group_count; j++) {
            if (strcmp(result->groups[j].group_key, key_buf) == 0) {
                group_idx = j;
                break;
            }
        }
        
        if (group_idx < 0) {
            // create new group
            if (result->group_count >= result->group_capacity) {
                result->group_capacity *= 2;
                result->groups = realloc(result->groups, sizeof(GroupedRows) * result->group_capacity);
            }
            
            group_idx = result->group_count++;
            result->groups[group_idx].group_key = strdup(key_buf);
            result->groups[group_idx].row_capacity = 16;
            result->groups[group_idx].rows = malloc(sizeof(Row*) * result->groups[group_idx].row_capacity);
            result->groups[group_idx].row_count = 0;
        }
        
        // add row to group
        GroupedRows* group = &result->groups[group_idx];
        if (group->row_count >= group->row_capacity) {
            group->row_capacity *= 2;
            group->rows = realloc(group->rows, sizeof(Row*) * group->row_capacity);
        }
        group->rows[group->row_count++] = rows[i];
    }
    
    return result;
}

void free_groups(GroupResult* groups) {
    if (!groups) return;
    
    for (int i = 0; i < groups->group_count; i++) {
        free(groups->groups[i].group_key);
        free(groups->groups[i].rows);
    }
    free(groups->groups);
    free(groups);
}

Value evaluate_aggregate(const char* func_name, Row** rows, int row_count, CsvTable* table, const char* column_name) {
    Value result;
    result.type = VALUE_TYPE_NULL;
    
    // special case for COUNT(*), in this case no column needed
    if (strcasecmp(func_name, "COUNT") == 0 && strcmp(column_name, "*") == 0) {
        result.type = VALUE_TYPE_INTEGER;
        result.int_value = row_count;
        return result;
    }
    
    int col_idx = find_column_index_with_fallback(table, column_name);
    
    if (col_idx < 0) {
        return result;
    }
    
    if (strcasecmp(func_name, "COUNT") == 0) {
        result.type = VALUE_TYPE_INTEGER;
        result.int_value = row_count;
        return result;
    }
    
    if (strcasecmp(func_name, "AVG") == 0 || strcasecmp(func_name, "SUM") == 0) {
        double sum = 0;
        int count = 0;
        
        for (int i = 0; i < row_count; i++) {
            Value* val = &rows[i]->values[col_idx];
            if (val->type == VALUE_TYPE_INTEGER) {
                sum += val->int_value;
                count++;
            } else if (val->type == VALUE_TYPE_DOUBLE) {
                sum += val->double_value;
                count++;
            }
        }
        
        if (strcasecmp(func_name, "SUM") == 0) {
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = sum;
        } else {
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = count > 0 ? sum / count : 0;
        }
        return result;
    }
    
    if (strcasecmp(func_name, "MIN") == 0 || strcasecmp(func_name, "MAX") == 0) {
        Value* extreme = NULL;
        
        for (int i = 0; i < row_count; i++) {
            Value* val = &rows[i]->values[col_idx];
            if (val->type != VALUE_TYPE_NULL) {
                if (!extreme || 
                    (strcasecmp(func_name, "MIN") == 0 && value_compare(val, extreme) < 0) ||
                    (strcasecmp(func_name, "MAX") == 0 && value_compare(val, extreme) > 0)) {
                    extreme = val;
                }
            }
        }
        
        if (extreme) return *extreme;
    }
    
    if (strcasecmp(func_name, "STDDEV") == 0 || strcasecmp(func_name, "STDDEV_POP") == 0) {
        // calculate population standard deviation
        double sum = 0;
        int count = 0;
        
        // first pass: calculate mean
        for (int i = 0; i < row_count; i++) {
            Value* val = &rows[i]->values[col_idx];
            if (val->type == VALUE_TYPE_INTEGER) {
                sum += val->int_value;
                count++;
            } else if (val->type == VALUE_TYPE_DOUBLE) {
                sum += val->double_value;
                count++;
            }
        }
        
        if (count == 0) return result;
        
        double mean = sum / count;
        
        // second pass: calculate variance
        double variance_sum = 0;
        for (int i = 0; i < row_count; i++) {
            Value* val = &rows[i]->values[col_idx];
            double value = 0;
            if (val->type == VALUE_TYPE_INTEGER) {
                value = val->int_value;
            } else if (val->type == VALUE_TYPE_DOUBLE) {
                value = val->double_value;
            } else {
                continue;
            }
            double diff = value - mean;
            variance_sum += diff * diff;
        }
        
        double variance = variance_sum / count;
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = sqrt(variance);
        return result;
    }
    
    if (strcasecmp(func_name, "MEDIAN") == 0) {
        // collect numeric values
        double* values = malloc(sizeof(double) * row_count);
        int count = 0;
        
        for (int i = 0; i < row_count; i++) {
            Value* val = &rows[i]->values[col_idx];
            if (val->type == VALUE_TYPE_INTEGER) {
                values[count++] = val->int_value;
            } else if (val->type == VALUE_TYPE_DOUBLE) {
                values[count++] = val->double_value;
            }
        }
        
        if (count == 0) {
            free(values);
            return result;
        }
        
        // sort values
        for (int i = 0; i < count - 1; i++) {
            for (int j = i + 1; j < count; j++) {
                if (values[i] > values[j]) {
                    double temp = values[i];
                    values[i] = values[j];
                    values[j] = temp;
                }
            }
        }
        
        // calculate median
        result.type = VALUE_TYPE_DOUBLE;
        if (count % 2 == 1) {
            result.double_value = values[count / 2];
        } else {
            result.double_value = (values[count / 2 - 1] + values[count / 2]) / 2.0;
        }
        
        free(values);
        return result;
    }
    
    return result;
}

// helper to evaluate expression in HAVING context on aggregated result rows
static Value evaluate_having_expression(ASTNode* expr, ResultSet* result, int row_idx, ASTNode* select_node) {
    Value val;
    val.type = VALUE_TYPE_NULL;
    
    if (!expr || !result || row_idx < 0 || row_idx >= result->row_count) return val;
    
    if (expr->type == NODE_TYPE_LITERAL) {
        return parse_value(expr->literal, strlen(expr->literal));
    }
    
    if (expr->type == NODE_TYPE_FUNCTION) {
        // for aggregate functions in HAVING, we need to match them to result columns
        // e.g., COUNT(*) should match the column named "COUNT(*)"
        char func_str[256];
        
        // reconstruct function string
        snprintf(func_str, sizeof(func_str), "%s(", expr->function.name);
        for (int i = 0; i < expr->function.arg_count; i++) {
            if (i > 0) strcat(func_str, ", ");
            if (expr->function.args[i]->type == NODE_TYPE_IDENTIFIER) {
                strcat(func_str, expr->function.args[i]->identifier);
            } else if (expr->function.args[i]->type == NODE_TYPE_LITERAL) {
                strcat(func_str, expr->function.args[i]->literal);
            }
        }
        strcat(func_str, ")");
        
        // find matching column in result
        for (int col = 0; col < result->column_count; col++) {
            // try exact match or match with the SELECT column spec
            if (strcasecmp(result->columns[col].name, func_str) == 0 ||
                (select_node && col < select_node->select.column_count &&
                 strncasecmp(select_node->select.columns[col], func_str, strlen(func_str)) == 0)) {
                return result->rows[row_idx].values[col];
            }
        }
    }
    
    if (expr->type == NODE_TYPE_IDENTIFIER) {
        // lookup column by name in result
        for (int col = 0; col < result->column_count; col++) {
            if (strcasecmp(result->columns[col].name, expr->identifier) == 0) {
                return result->rows[row_idx].values[col];
            }
        }
    }
    
    return val;
}

// evaluate HAVING condition on a result row
static bool evaluate_having_condition(ASTNode* condition, ResultSet* result, int row_idx, ASTNode* select_node) {
    if (!condition) return true;
    if (condition->type != NODE_TYPE_CONDITION) return false;
    
    const char* op = condition->condition.operator;
    
    // handle logical operators
    if (strcasecmp(op, "AND") == 0) {
        return evaluate_having_condition(condition->condition.left, result, row_idx, select_node) &&
               evaluate_having_condition(condition->condition.right, result, row_idx, select_node);
    }
    
    if (strcasecmp(op, "OR") == 0) {
        return evaluate_having_condition(condition->condition.left, result, row_idx, select_node) ||
               evaluate_having_condition(condition->condition.right, result, row_idx, select_node);
    }
    
    // handle comparison operators
    Value left = evaluate_having_expression(condition->condition.left, result, row_idx, select_node);
    Value right = evaluate_having_expression(condition->condition.right, result, row_idx, select_node);
    
    int cmp = value_compare(&left, &right);
    
    if (strcmp(op, "=") == 0) return cmp == 0;
    if (strcmp(op, "!=") == 0) return cmp != 0;
    if (strcmp(op, "<>") == 0) return cmp != 0;
    if (strcmp(op, ">") == 0) return cmp > 0;
    if (strcmp(op, "<") == 0) return cmp < 0;
    if (strcmp(op, ">=") == 0) return cmp >= 0;
    if (strcmp(op, "<=") == 0) return cmp <= 0;
    
    return false;
}

/* filter result rows based on HAVING clause */
void apply_having_filter(ResultSet* result, ASTNode* having, ASTNode* select_node) {
    if (!result || !having || result->row_count == 0) return;
    
    Row* filtered_rows = malloc(sizeof(Row) * result->row_count);
    int filtered_count = 0;
    
    for (int i = 0; i < result->row_count; i++) {
        if (evaluate_having_condition(having, result, i, select_node)) {
            // keep this row
            filtered_rows[filtered_count++] = result->rows[i];
        } else {
            // free this row's values
            for (int j = 0; j < result->rows[i].column_count; j++) {
                if (result->rows[i].values[j].type == VALUE_TYPE_STRING &&
                    result->rows[i].values[j].string_value) {
                    free((char*)result->rows[i].values[j].string_value);
                }
            }
            free(result->rows[i].values);
        }
    }
    
    /* replace rows with filtered ones */
    free(result->rows);
    result->rows = filtered_rows;
    result->row_count = filtered_count;
    result->row_capacity = result->row_count;
}

/* function to build aggregated result */
ResultSet* build_aggregated_result(QueryContext* ctx, GroupResult* groups, ASTNode* select_node) {
    ResultSet* result = calloc(1, sizeof(ResultSet));
    result->filename = strdup("query_result");
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    if (!select_node) return result;
    
    result->column_count = select_node->select.column_count;
    result->columns = malloc(sizeof(Column) * result->column_count);
    
    /* parse column specifications for aliases and functions */
    for (int i = 0; i < result->column_count; i++) {
        const char* col_spec = select_node->select.columns[i];
        
        /* extract alias if present */
        char* alias = extract_column_alias(col_spec);
        if (alias) {
            result->columns[i].name = alias;
        } else {
            /* check if it's a function call checking if contains parentheses */
            const char* paren = strchr(col_spec, '(');
            if (paren) {
                /* for functions like AVG(t.height), keep the whole function as the column name
                   but strip the table prefix from the argument if present */
                char func_buf[256];
                const char* paren_close = strchr(paren, ')');
                
                /* extract function name */
                int func_len = paren - col_spec;
                strncpy(func_buf, col_spec, func_len);
                func_buf[func_len] = '\0';
                
                /* extract argument */
                const char* arg_start = paren + 1;
                int arg_len = paren_close - arg_start;
                char arg_buf[128];
                strncpy(arg_buf, arg_start, arg_len);
                arg_buf[arg_len] = '\0';
                
                /* strip table prefix from argument if present */
                const char* dot = strchr(arg_buf, '.');
                const char* arg_name = dot ? dot + 1 : arg_buf;
                
                /* build display name: FUNC(column) */
                char display_name[256];
                snprintf(display_name, sizeof(display_name), "%s(%s)", func_buf, arg_name);
                result->columns[i].name = strdup(display_name);
            } else {
                /* for display name, use only column name without table prefix */
                const char* dot = strchr(col_spec, '.');
                if (dot) {
                    result->columns[i].name = strdup(dot + 1);
                } else {
                    result->columns[i].name = strdup(col_spec);
                }
            }
        }
        result->columns[i].inferred_type = VALUE_TYPE_STRING;
    }
    
    /* building rows, one row per group */
    result->row_count = groups->group_count;
    result->row_capacity = groups->group_count;
    result->rows = malloc(sizeof(Row) * result->row_count);
    
    for (int g = 0; g < groups->group_count; g++) {
        GroupedRows* group = &groups->groups[g];
        result->rows[g].column_count = result->column_count;
        result->rows[g].values = malloc(sizeof(Value) * result->column_count);
        
        for (int col = 0; col < result->column_count; col++) {
            const char* col_spec = select_node->select.columns[col];
            char col_name[256];
            char func_name[64] = "";
            
            /* parse column specification */
            char* alias = extract_column_alias(col_spec);
            if (alias) {
                // find " AS " in the original string to extract the expression part
                const char* as_pos = cq_strcasestr(col_spec, " AS ");
                if (as_pos) {
                    int col_len = as_pos - col_spec;
                    strncpy(col_name, col_spec, col_len);
                    col_name[col_len] = '\0';
                }
                free(alias);
            } else {
                strcpy(col_name, col_spec);
            }
            
            /* trim trailing spaces */
            trim_trailing_spaces(col_name);
            
            /* check if it's a function call */
            char* paren = strchr(col_name, '(');
            if (paren) {
                /* extract function name */
                int func_len = paren - col_name;
                strncpy(func_name, col_name, func_len);
                func_name[func_len] = '\0';
                
                /* check if it's an aggregate function */
                if (is_aggregate_function(func_name)) {
                    /* extract column argument */
                    char* arg_start = paren + 1;
                    char* paren_close = strchr(arg_start, ')');
                    if (paren_close) {
                        int arg_len = paren_close - arg_start;
                        char temp_buf[256];
                        strncpy(temp_buf, arg_start, arg_len);
                        temp_buf[arg_len] = '\0';
                        strcpy(col_name, temp_buf);
                    }
                    
                    /* handle qualified identifiers in function argument
                       pass the full column name to evaluate_aggregate
                       it will try exact match first, then strip prefix if needed */
                    
                    /* evaluate aggregate function */
                    Value tmp = evaluate_aggregate(func_name, group->rows, group->row_count, 
                                                   ctx->tables[0].table, col_name);
                    result->rows[g].values[col] = value_copy(&tmp);
                } else {
                    /* scalar function - evaluate on first row of the group */
                    if (group->row_count > 0) {
                        Value tmp = evaluate_column_expression(col_spec, ctx, group->rows[0], NULL, col);
                        result->rows[g].values[col] = value_copy(&tmp);
                    } else {
                        result->rows[g].values[col].type = VALUE_TYPE_NULL;
                    }
                }
            } else {
                /* regular column - check if we have a column_node (expression) for it */
                ASTNode* col_node = select_node->select.column_nodes ? select_node->select.column_nodes[col] : NULL;
                
                if (col_node && col_node->type != NODE_TYPE_IDENTIFIER) {
                    /* this is an expression (CASE, function call, etc.) - evaluate it on first row */
                    if (group->row_count > 0) {
                        Value tmp = evaluate_expression(ctx, col_node, group->rows[0], 0);
                        result->rows[g].values[col] = value_copy(&tmp);
                    } else {
                        result->rows[g].values[col].type = VALUE_TYPE_NULL;
                    }
                } else {
                    /* regular column reference - use first row's value from the group */
                    int col_idx = find_column_index_with_fallback(ctx->tables[0].table, col_name);
                    
                    if (col_idx >= 0 && group->row_count > 0) {
                        Value* src = &group->rows[0]->values[col_idx];
                        Value* dst = &result->rows[g].values[col];
                        
                        value_deep_copy(dst, src);
                    } else {
                        result->rows[g].values[col].type = VALUE_TYPE_NULL;
                    }
                }
            }
        }
    }
    
    return result;
}
