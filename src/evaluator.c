#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#include "evaluator.h"
#include "parser.h"
#include "csv_reader.h"
#include "string_utils.h"

/* global CSV configuration to can be set before calling evaluate_query */
CsvConfig global_csv_config = {.delimiter = ',', .quote = '"', .has_header = true};

/* forward declarations */
static Value evaluate_scalar_function(const char* func_name, Value* args, int arg_count);
static void value_deep_copy(Value* dst, const Value* src);
static ResultSet* evaluate_query_internal(ASTNode* query_ast, Row* outer_row, CsvTable* outer_table);
static bool match_pattern(const char* str, const char* pattern, bool case_sensitive);
static ResultSet* set_union(ResultSet* left, ResultSet* right, bool include_duplicates);
static ResultSet* set_intersect(ResultSet* left, ResultSet* right);
static ResultSet* set_except(ResultSet* left, ResultSet* right);
static ResultSet* evaluate_insert(ASTNode* insert_node);
static ResultSet* evaluate_update(ASTNode* update_node);
static ResultSet* evaluate_delete(ASTNode* delete_node);
static ResultSet* evaluate_create_table(ASTNode* create_node);
static ResultSet* evaluate_alter_table(ASTNode* alter_node);

/* pattern matching for LIKE and ILIKE operators
 * supports % (any sequence) and _ (single character)
 */
static bool match_pattern(const char* str, const char* pattern, bool case_sensitive) {
    if (!str || !pattern) return false;
    
    const char* s = str;
    const char* p = pattern;
    const char* star = NULL;
    const char* ss = NULL;
    
    while (*s) {
        if (*p == '%') {
            // remember position for backtracking
            star = p++;
            ss = s;
        } else if (*p == '_') {
            // single character wildcard
            s++;
            p++;
        } else {
            // check character match
            bool match;
            if (case_sensitive) {
                match = (*s == *p);
            } else {
                match = (tolower((unsigned char)*s) == tolower((unsigned char)*p));
            }
            
            if (match) {
                s++;
                p++;
            } else if (star) {
                // backtrack to last %
                p = star + 1;
                s = ++ss;
            } else {
                return false;
            }
        }
    }
    
    // consume remaining % at end of pattern
    while (*p == '%') p++;
    
    return *p == '\0';
}

/* context management */
QueryContext* context_create(ASTNode* query_ast) {
    QueryContext* ctx = calloc(1, sizeof(QueryContext));
    ctx->query = query_ast;
    ctx->tables = NULL;
    ctx->table_count = 0;
    ctx->outer_row = NULL;
    ctx->outer_table = NULL;
    return ctx;
}

void context_free(QueryContext* ctx) {
    if (!ctx) return;
    
    // free loaded tables
    for (int i = 0; i < ctx->table_count; i++) {
        free(ctx->tables[i].alias);
        csv_free(ctx->tables[i].table);
    }
    free(ctx->tables);
    free(ctx);
}

/* table loading */
CsvTable* load_table_from_string(const char* filename) {
    // remove quotes if present
    const char* start = filename;
    const char* end = filename + strlen(filename);
    
    if (*start == '"' || *start == '\'') start++;
    if (end > start && (*(end-1) == '"' || *(end-1) == '\'')) end--;
    
    char* clean_filename = cq_strndup(start, end - start);
    
    CsvConfig config = global_csv_config;
    CsvTable* table = csv_load(clean_filename, config);
    
    free(clean_filename);
    return table;
}

TableRef* context_get_table(QueryContext* ctx, const char* alias) {
    if (!ctx || !alias) return NULL;
    
    for (int i = 0; i < ctx->table_count; i++) {
        if (strcasecmp(ctx->tables[i].alias, alias) == 0) {
            return &ctx->tables[i];
        }
    }
    return NULL;
}

/* function to resolve column by name */
Value* resolve_column(QueryContext* ctx, const char* column_name, Row* current_row, int table_index) {
    if (!ctx || !column_name || !current_row) return NULL;
    
    if (table_index < 0 || table_index >= ctx->table_count) return NULL;
    CsvTable* table = ctx->tables[table_index].table;
    
    // check if it's a qualified name like table.column
    const char* dot = strchr(column_name, '.');
    
    if (dot) {
        // if qualified try exact match first for joined tables
        int col_index = csv_get_column_index(table, column_name);
        if (col_index >= 0) {
            return &current_row->values[col_index];
        }
        
        // if not found, try traditional resolution for table alias lookup
        size_t alias_len = dot - column_name;
        char* table_alias = cq_strndup(column_name, alias_len);
        const char* col_name = dot + 1;
        
        // find the table
        TableRef* table_ref = context_get_table(ctx, table_alias);
        free(table_alias);
        
        if (!table_ref) {
            // if not found in current query check if it's referencing outer table in correlated subquery
            if (ctx->outer_row && ctx->outer_table) {
                col_index = csv_get_column_index(ctx->outer_table, col_name);
                if (col_index >= 0) {
                    return &ctx->outer_row->values[col_index];
                }
            }
            return NULL;
        }
        
        // get column index
        col_index = csv_get_column_index(table_ref->table, col_name);
        if (col_index < 0) {
            // if not found in current query check outer context for correlated subquery
            if (ctx->outer_row && ctx->outer_table) {
                col_index = csv_get_column_index(ctx->outer_table, col_name);
                if (col_index >= 0) {
                    return &ctx->outer_row->values[col_index];
                }
            }
            return NULL;
        }
        
        return &current_row->values[col_index];
    } else {
        // if unqualified look in current table
        int col_index = csv_get_column_index(table, column_name);
        if (col_index < 0) {
            // if not found in current query check outer context for correlated subquery
            if (ctx->outer_row && ctx->outer_table) {
                col_index = csv_get_column_index(ctx->outer_table, column_name);
                if (col_index >= 0) {
                    return &ctx->outer_row->values[col_index];
                }
            }
            return NULL;
        }
        
        return &current_row->values[col_index];
    }
}

/* function to evaluate expression */
Value evaluate_expression(QueryContext* ctx, ASTNode* expr, Row* current_row, int table_index) {
    Value result;
    result.type = VALUE_TYPE_NULL;
    
    if (!expr) return result;
    
    switch (expr->type) {
        case NODE_TYPE_LITERAL:
            return parse_value(expr->literal, strlen(expr->literal));
            
        case NODE_TYPE_IDENTIFIER: {
            // resolve column value
            Value* val = resolve_column(ctx, expr->identifier, current_row, table_index);
            if (val) {
                // do a deep copy to avoid freeing shared string pointers
                value_deep_copy(&result, val);
                return result;
            }
            break;
        }
        
        case NODE_TYPE_SUBQUERY: {
            // evaluate scalar subquery, that may be correlated
            // Note: This is for scalar contexts (SELECT, comparison ops)
            // IN operator handles multi-row subqueries separately in evaluate_condition
            if (!expr->subquery.query) break;
            
            // pass the outer context for correlated subqueries
            ResultSet* subquery_result = evaluate_query_internal(expr->subquery.query, 
                current_row, ctx->tables[table_index].table);
            
            if (!subquery_result) break;
            
            // validation only in scalar context (not for IN operator)
            // IN operator is handled separately and allows multi-row results
            if (subquery_result->row_count != 1 || subquery_result->column_count != 1) {
                // only report error if it's truly being used as scalar
                // don't report for IN operator which is handled elsewhere
                csv_free(subquery_result);
                break;
            }
            
            // copy the single value from subquery result
            value_deep_copy(&result, &subquery_result->rows[0].values[0]);
            csv_free(subquery_result);
            return result;
        }
        
        case NODE_TYPE_FUNCTION: {
            // evaluate function call in WHERE clause
            Value func_args[10];
            int func_arg_count = 0;
            
            // evaluate each argument recursively
            for (int i = 0; i < expr->function.arg_count && i < 10; i++) {
                func_args[func_arg_count++] = evaluate_expression(ctx, expr->function.args[i], current_row, table_index);
            }
            
            // call the function
            result = evaluate_scalar_function(expr->function.name, func_args, func_arg_count);
            
            // free temporary string values allocated during argument evaluation
            for (int i = 0; i < func_arg_count; i++) {
                if (func_args[i].type == VALUE_TYPE_STRING && func_args[i].string_value) {
                    free((char*)func_args[i].string_value);
                }
            }
            
            return result;
        }
        
        case NODE_TYPE_BINARY_OP: {
            // check for unary operator (left is NULL, only right exists)
            if (!expr->binary_op.left) {
                if (!expr->binary_op.right) {
                    result.type = VALUE_TYPE_NULL;
                    return result;
                }
                
                const char* op = expr->binary_op.operator;
                Value operand = evaluate_expression(ctx, expr->binary_op.right, current_row, table_index);
                
                if (strcmp(op, "-") == 0) {
                    // unary minus
                    if (operand.type == VALUE_TYPE_INTEGER) {
                        result.type = VALUE_TYPE_INTEGER;
                        result.int_value = -operand.int_value;
                        return result;
                    } else if (operand.type == VALUE_TYPE_DOUBLE) {
                        result.type = VALUE_TYPE_DOUBLE;
                        result.double_value = -operand.double_value;
                        return result;
                    }
                } else if (strcmp(op, "+") == 0) {
                    // unary plus (no-op)
                    return operand;
                }
                result.type = VALUE_TYPE_NULL;
                return result;
            }
            
            // evaluate binary arithmetic operation
            Value left = evaluate_expression(ctx, expr->binary_op.left, current_row, table_index);
            
            // check for unary operator (right is NULL) - shouldn't happen with current parser
            if (!expr->binary_op.right) {
                const char* op = expr->binary_op.operator;
                if (strcmp(op, "-") == 0) {
                    // unary minus
                    if (left.type == VALUE_TYPE_INTEGER) {
                        result.type = VALUE_TYPE_INTEGER;
                        result.int_value = -left.int_value;
                        return result;
                    } else if (left.type == VALUE_TYPE_DOUBLE) {
                        result.type = VALUE_TYPE_DOUBLE;
                        result.double_value = -left.double_value;
                        return result;
                    }
                } else if (strcmp(op, "+") == 0) {
                    // unary plus (no-op)
                    return left;
                }
                result.type = VALUE_TYPE_NULL;
                return result;
            }
            
            Value right = evaluate_expression(ctx, expr->binary_op.right, current_row, table_index);
            const char* op = expr->binary_op.operator;
            
            // convert to numeric values
            double left_val = 0, right_val = 0;
            long long left_int = 0, right_int = 0;
            bool left_is_int = false, right_is_int = false;
            
            if (left.type == VALUE_TYPE_INTEGER) {
                left_val = (double)left.int_value;
                left_int = left.int_value;
                left_is_int = true;
            } else if (left.type == VALUE_TYPE_DOUBLE) {
                left_val = left.double_value;
            } else {
                result.type = VALUE_TYPE_NULL;
                return result;
            }
            
            if (right.type == VALUE_TYPE_INTEGER) {
                right_val = (double)right.int_value;
                right_int = right.int_value;
                right_is_int = true;
            } else if (right.type == VALUE_TYPE_DOUBLE) {
                right_val = right.double_value;
            } else {
                result.type = VALUE_TYPE_NULL;
                return result;
            }
            
            // perform the operation
            double result_val = 0;
            long long result_int = 0;
            bool result_is_int = false;
            
            if (strcmp(op, "+") == 0) {
                result_val = left_val + right_val;
            } else if (strcmp(op, "-") == 0) {
                result_val = left_val - right_val;
            } else if (strcmp(op, "*") == 0) {
                result_val = left_val * right_val;
            } else if (strcmp(op, "/") == 0) {
                if (right_val == 0) {
                    result.type = VALUE_TYPE_NULL;
                    return result;
                }
                result_val = left_val / right_val;
            } else if (strcmp(op, "%") == 0) {
                // % requires integers
                if (left_is_int && right_is_int) {
                    if (right_int == 0) {
                        result.type = VALUE_TYPE_NULL;
                        return result;
                    }
                    result_int = left_int % right_int;
                    result_is_int = true;
                } else {
                    // doubles use fmod
                    if (right_val == 0) {
                        result.type = VALUE_TYPE_NULL;
                        return result;
                    }
                    result_val = fmod(left_val, right_val);
                }
            } else if (strcmp(op, "&") == 0) {
                // bitwise AND requires integers
                if (left_is_int && right_is_int) {
                    result_int = left_int & right_int;
                    result_is_int = true;
                } else {
                    result.type = VALUE_TYPE_NULL;
                    return result;
                }
            } else if (strcmp(op, "|") == 0) {
                // bitwise OR requires integers
                if (left_is_int && right_is_int) {
                    result_int = left_int | right_int;
                    result_is_int = true;
                } else {
                    result.type = VALUE_TYPE_NULL;
                    return result;
                }
            } else if (strcmp(op, "^") == 0) {
                // bitwise XOR requires integers
                if (left_is_int && right_is_int) {
                    result_int = left_int ^ right_int;
                    result_is_int = true;
                } else {
                    result.type = VALUE_TYPE_NULL;
                    return result;
                }
            }
            
            // return result
            if (result_is_int) {
                result.type = VALUE_TYPE_INTEGER;
                result.int_value = result_int;
            } else if (left.type == VALUE_TYPE_INTEGER && right.type == VALUE_TYPE_INTEGER && 
                       result_val == (long long)result_val) {
                result.type = VALUE_TYPE_INTEGER;
                result.int_value = (long long)result_val;
            } else {
                result.type = VALUE_TYPE_DOUBLE;
                result.double_value = result_val;
            }
            
            return result;
        }
            
        default:
            break;
    }
    
    return result;
}

/* function to evaluate condition */
bool evaluate_condition(QueryContext* ctx, ASTNode* condition, Row* current_row, int table_index) {
    if (!condition) return true;
    
    if (condition->type != NODE_TYPE_CONDITION) return false;
    
    const char* op = condition->condition.operator;
    
    // handle unary NOT operator
    if (strcasecmp(op, "NOT") == 0) {
        bool result = evaluate_condition(ctx, condition->condition.left, current_row, table_index);
        return !result;
    }
    
    // handle logical operators
    if (strcasecmp(op, "AND") == 0) {
        bool left = evaluate_condition(ctx, condition->condition.left, current_row, table_index);
        bool right = evaluate_condition(ctx, condition->condition.right, current_row, table_index);
        return left && right;
    }
    
    if (strcasecmp(op, "OR") == 0) {
        bool left = evaluate_condition(ctx, condition->condition.left, current_row, table_index);
        bool right = evaluate_condition(ctx, condition->condition.right, current_row, table_index);
        return left || right;
    }
    
    // handle comparison operators
    Value left = evaluate_expression(ctx, condition->condition.left, current_row, table_index);
    Value right = evaluate_expression(ctx, condition->condition.right, current_row, table_index);
    
    int cmp = value_compare(&left, &right);
    
    if (strcmp(op, "=") == 0) return cmp == 0;
    if (strcmp(op, "!=") == 0) return cmp != 0;
    if (strcmp(op, "<>") == 0) return cmp != 0;
    if (strcmp(op, ">") == 0) return cmp > 0;
    if (strcmp(op, "<") == 0) return cmp < 0;
    if (strcmp(op, ">=") == 0) return cmp >= 0;
    if (strcmp(op, "<=") == 0) return cmp <= 0;
    
    // handle IN and NOT IN operators
    if (strcasecmp(op, "IN") == 0 || strcasecmp(op, "NOT IN") == 0) {
        bool is_not_in = (strcasecmp(op, "NOT IN") == 0);
        ASTNode* right_node = condition->condition.right;
        
        // check if it's a subquery or a list
        if (right_node->type == NODE_TYPE_SUBQUERY) {
            // evaluate the subquery
            if (!right_node->subquery.query) return is_not_in; // NOT IN empty = true
            
            ResultSet* subquery_result = evaluate_query(right_node->subquery.query);
            if (!subquery_result) return is_not_in;
            
            // the subquery should return a single column
            if (subquery_result->column_count != 1) {
                fprintf(stderr, "Error: IN subquery must return exactly one column\n");
                csv_free(subquery_result);
                return false;
            }
            
            // check if left value matches any value in the subquery result
            bool found = false;
            for (int i = 0; i < subquery_result->row_count; i++) {
                Value* subquery_val = &subquery_result->rows[i].values[0];
                if (value_compare(&left, subquery_val) == 0) {
                    found = true;
                    break;
                }
            }
            
            csv_free(subquery_result);
            return is_not_in ? !found : found;
        } else if (right_node->type == NODE_TYPE_LIST) {
            // list-based IN operator
            ASTNode* list = right_node;
            for (int i = 0; i < list->list.node_count; i++) {
                Value list_val = evaluate_expression(ctx, list->list.nodes[i], current_row, table_index);
                if (value_compare(&left, &list_val) == 0) {
                    value_free(&list_val);
                    return is_not_in ? false : true;
                }
                value_free(&list_val);
            }
            return is_not_in ? true : false;
        }
        
        return is_not_in; // empty list: NOT IN = true, IN = false
    }
    
    // handle LIKE and ILIKE operators
    if (strcasecmp(op, "LIKE") == 0 || strcasecmp(op, "ILIKE") == 0) {
        bool case_sensitive = (strcasecmp(op, "LIKE") == 0);
        
        // both operands must be strings
        if (left.type != VALUE_TYPE_STRING || right.type != VALUE_TYPE_STRING) {
            return false;
        }
        
        return match_pattern(left.string_value, right.string_value, case_sensitive);
    }
    
    return false;
}

typedef struct {
    char* group_key;
    Row** rows;
    int row_count;
    int row_capacity;
} GroupedRows;

typedef struct {
    GroupedRows* groups;
    int group_count;
    int group_capacity;
} GroupResult;

/* find column index in table, supporting qualified identifiers like table.column */
static int find_column_index(CsvTable* table, const char* col_name) {
    if (!table || !col_name) return -1;
    
    // try exact match first for joined tables with prefixed columns like "t.role"
    for (int i = 0; i < table->column_count; i++) {
        if (strcasecmp(col_name, table->columns[i].name) == 0) {
            return i;
        }
    }
    
    // try without table prefix for qualified identifiers like "t.column"
    const char* dot = strchr(col_name, '.');
    if (dot) {
        const char* lookup_name = dot + 1;
        for (int i = 0; i < table->column_count; i++) {
            if (strcasecmp(lookup_name, table->columns[i].name) == 0) {
                return i;
            }
        }
    }
    
    return -1;
}

/* helper to get column index using csv_get_column_index with fallback to strip table prefix */
static int find_column_index_with_fallback(CsvTable* table, const char* col_name) {
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

/* deep copy a value to handle string duplication */
static void value_deep_copy(Value* dst, const Value* src) {
    if (!dst || !src) return;
    
    dst->type = src->type;
    if (src->type == VALUE_TYPE_STRING && src->string_value) {
        dst->string_value = strdup(src->string_value);
    } else {
        dst->int_value = src->int_value;  // Covers the whole union
    }
}

/* helper to trim trailing spaces from a string in place */
static void trim_trailing_spaces(char* str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && isspace(str[len - 1])) {
        str[--len] = '\0';
    }
}

/* helper to extract alias from column spec (part after AS keyword) */
static char* extract_column_alias(const char* col_spec) {
    const char* as_pos = cq_strcasestr(col_spec, " AS ");
    if (as_pos) {
        return strdup(as_pos + 4);
    }
    return NULL;
}

/* helper to transform string case (UPPER or LOWER) */
static char* transform_string_case(const char* str, bool to_upper) {
    if (!str) return NULL;
    
    char* result = strdup(str);
    for (int i = 0; result[i]; i++) {
        result[i] = to_upper ? toupper(result[i]) : tolower(result[i]);
    }
    return result;
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
static Value evaluate_column_expression(const char* col_spec, QueryContext* ctx, 
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

/* helper to evaluate scalar functions like UPPER(name), LENGTH(role), etc. */
static Value evaluate_scalar_function(const char* func_name, Value* args, int arg_count) {
    Value result;
    result.type = VALUE_TYPE_NULL;
    
    if (arg_count < 1) return result;
    
    // CONCAT
    if (strcasecmp(func_name, "CONCAT") == 0) {
        char buffer[1024] = "";
        for (int i = 0; i < arg_count; i++) {
            if (args[i].type == VALUE_TYPE_STRING && args[i].string_value) {
                strcat(buffer, args[i].string_value);
            } else if (args[i].type == VALUE_TYPE_INTEGER) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%lld", args[i].int_value);
                strcat(buffer, temp);
            } else if (args[i].type == VALUE_TYPE_DOUBLE) {
                char temp[64];
                snprintf(temp, sizeof(temp), "%.2f", args[i].double_value);
                strcat(buffer, temp);
            }
        }
        result.type = VALUE_TYPE_STRING;
        result.string_value = strdup(buffer);
        return result;
    }
    
    // LOWER
    if (strcasecmp(func_name, "LOWER") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_STRING;
            result.string_value = transform_string_case(args[0].string_value, false);
        }
        return result;
    }
    
    // UPPER
    if (strcasecmp(func_name, "UPPER") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_STRING;
            result.string_value = transform_string_case(args[0].string_value, true);
        }
        return result;
    }
    
    // LENGTH
    if (strcasecmp(func_name, "LENGTH") == 0) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = strlen(args[0].string_value);
        }
        return result;
    }
    
    // SUBSTRING(str, start, length)
    if (strcasecmp(func_name, "SUBSTRING") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value &&
            args[1].type == VALUE_TYPE_INTEGER && args[2].type == VALUE_TYPE_INTEGER) {
            
            int start = args[1].int_value - 1; // Convert to 0-indexed
            int length = args[2].int_value;
            const char* str = args[0].string_value;
            int str_len = strlen(str);
            
            if (start < 0) start = 0;
            if (start >= str_len) {
                result.type = VALUE_TYPE_STRING;
                result.string_value = strdup("");
                return result;
            }
            
            if (start + length > str_len) {
                length = str_len - start;
            }
            
            char* substr = malloc(length + 1);
            strncpy(substr, str + start, length);
            substr[length] = '\0';
            
            result.type = VALUE_TYPE_STRING;
            result.string_value = substr;
        }
        return result;
    }
    
    // REPLACE(str, from, to)
    if (strcasecmp(func_name, "REPLACE") == 0 && arg_count >= 3) {
        if (args[0].type == VALUE_TYPE_STRING && args[0].string_value &&
            args[1].type == VALUE_TYPE_STRING && args[1].string_value &&
            args[2].type == VALUE_TYPE_STRING && args[2].string_value) {
            
            const char* str = args[0].string_value;
            const char* from = args[1].string_value;
            const char* to = args[2].string_value;
            
            int from_len = strlen(from);
            int to_len = strlen(to);
            
            if (from_len == 0) {
                result.type = VALUE_TYPE_STRING;
                result.string_value = strdup(str);
                return result;
            }
            
            // count occurrences
            int count = 0;
            const char* pos = str;
            while ((pos = strstr(pos, from)) != NULL) {
                count++;
                pos += from_len;
            }
            
            // allocate result buffer
            int result_len = strlen(str) + count * (to_len - from_len);
            char* new_str = malloc(result_len + 1);
            char* dest = new_str;
            
            pos = str;
            while (*pos) {
                if (strncmp(pos, from, from_len) == 0) {
                    strcpy(dest, to);
                    dest += to_len;
                    pos += from_len;
                } else {
                    *dest++ = *pos++;
                }
            }
            *dest = '\0';
            
            result.type = VALUE_TYPE_STRING;
            result.string_value = new_str;
        }
        return result;
    }
    
    // COALESCE
    if (strcasecmp(func_name, "COALESCE") == 0) {
        for (int i = 0; i < arg_count; i++) {
            if (args[i].type != VALUE_TYPE_NULL) {
                // deep copy the value to avoid freeing shared pointers
                result.type = args[i].type;
                if (args[i].type == VALUE_TYPE_STRING && args[i].string_value) {
                    result.string_value = strdup(args[i].string_value);
                } else {
                    result.int_value = args[i].int_value;
                }
                return result;
            }
        }
        return result;
    }
    
    // POWER(base, exponent)
    if (strcasecmp(func_name, "POWER") == 0 && arg_count >= 2) {
        double base = 0, exponent = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            base = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            base = args[0].double_value;
        } else {
            return result;
        }
        
        if (args[1].type == VALUE_TYPE_INTEGER) {
            exponent = (double)args[1].int_value;
        } else if (args[1].type == VALUE_TYPE_DOUBLE) {
            exponent = args[1].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = pow(base, exponent);
        return result;
    }
    
    // SQRT(number)
    if (strcasecmp(func_name, "SQRT") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        if (val < 0) {
            return result; // NULL for negative numbers
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = sqrt(val);
        return result;
    }
    
    // CEIL(number)
    if (strcasecmp(func_name, "CEIL") == 0 || strcasecmp(func_name, "CEILING") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value;
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = ceil(val);
        return result;
    }
    
    // FLOOR(number)
    if (strcasecmp(func_name, "FLOOR") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value;
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = floor(val);
        return result;
    }
    
    // ROUND(number, [decimals])
    if (strcasecmp(func_name, "ROUND") == 0) {
        double val = 0;
        int decimals = 0;
        
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        // optional second argument for decimal places
        if (arg_count >= 2) {
            if (args[1].type == VALUE_TYPE_INTEGER) {
                decimals = (int)args[1].int_value;
            } else if (args[1].type == VALUE_TYPE_DOUBLE) {
                decimals = (int)args[1].double_value;
            }
        }
        
        // round to specified decimal places
        double multiplier = pow(10.0, decimals);
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = round(val * multiplier) / multiplier;
        
        // if no decimals specified and result is whole number, return as integer
        if (decimals == 0 && result.double_value == floor(result.double_value)) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = (long long)result.double_value;
        }
        
        return result;
    }
    
    // ABS(number)
    if (strcasecmp(func_name, "ABS") == 0) {
        if (args[0].type == VALUE_TYPE_INTEGER) {
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = llabs(args[0].int_value);
            return result;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = fabs(args[0].double_value);
            return result;
        }
        return result;
    }
    
    // EXP(number) - e^x
    if (strcasecmp(func_name, "EXP") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = exp(val);
        return result;
    }
    
    // LN(number) - natural logarithm
    if (strcasecmp(func_name, "LN") == 0 || strcasecmp(func_name, "LOG") == 0) {
        double val = 0;
        if (args[0].type == VALUE_TYPE_INTEGER) {
            val = (double)args[0].int_value;
        } else if (args[0].type == VALUE_TYPE_DOUBLE) {
            val = args[0].double_value;
        } else {
            return result;
        }
        
        if (val <= 0) {
            return result; // NULL for non-positive numbers
        }
        
        result.type = VALUE_TYPE_DOUBLE;
        result.double_value = log(val);
        return result;
    }
    
    // MOD(dividend, divisor)
    if (strcasecmp(func_name, "MOD") == 0 && arg_count >= 2) {
        if (args[0].type == VALUE_TYPE_INTEGER && args[1].type == VALUE_TYPE_INTEGER) {
            if (args[1].int_value == 0) {
                return result; // NULL for division by zero
            }
            result.type = VALUE_TYPE_INTEGER;
            result.int_value = args[0].int_value % args[1].int_value;
            return result;
        } else {
            double dividend = 0, divisor = 0;
            if (args[0].type == VALUE_TYPE_INTEGER) {
                dividend = (double)args[0].int_value;
            } else if (args[0].type == VALUE_TYPE_DOUBLE) {
                dividend = args[0].double_value;
            } else {
                return result;
            }
            
            if (args[1].type == VALUE_TYPE_INTEGER) {
                divisor = (double)args[1].int_value;
            } else if (args[1].type == VALUE_TYPE_DOUBLE) {
                divisor = args[1].double_value;
            } else {
                return result;
            }
            
            if (divisor == 0) {
                return result; // NULL for division by zero
            }
            
            result.type = VALUE_TYPE_DOUBLE;
            result.double_value = fmod(dividend, divisor);
            return result;
        }
    }
    
    return result;
}

static GroupResult* create_groups(Row** rows, int row_count, CsvTable* table, const char* group_column) {
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

static void free_groups(GroupResult* groups) {
    if (!groups) return;
    
    for (int i = 0; i < groups->group_count; i++) {
        free(groups->groups[i].group_key);
        free(groups->groups[i].rows);
    }
    free(groups->groups);
    free(groups);
}

static bool is_aggregate_function(const char* func_name) {
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
static bool has_aggregate_functions(ASTNode* select_node) {
    if (!select_node || select_node->type != NODE_TYPE_SELECT) return false;
    
    for (int i = 0; i < select_node->select.column_count; i++) {
        const char* col_spec = select_node->select.columns[i];
        
        // check if column spec contains an aggregate function
        if (strstr(col_spec, "COUNT(") || strstr(col_spec, "SUM(") ||
            strstr(col_spec, "AVG(") || strstr(col_spec, "MIN(") || strstr(col_spec, "MAX(") ||
            strstr(col_spec, "STDDEV(") || strstr(col_spec, "MEDIAN(")) {
            return true;
        }
    }
    
    return false;
}

static Value evaluate_aggregate(const char* func_name, Row** rows, int row_count, CsvTable* table, const char* column_name) {
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
static void apply_having_filter(ResultSet* result, ASTNode* having, ASTNode* select_node) {
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
static ResultSet* build_aggregated_result(QueryContext* ctx, GroupResult* groups, ASTNode* select_node) {
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
                    result->rows[g].values[col] = evaluate_aggregate(func_name, group->rows, group->row_count, 
                                                                     ctx->tables[0].table, col_name);
                } else {
                    /* scalar function - evaluate on first row of the group */
                    if (group->row_count > 0) {
                        result->rows[g].values[col] = evaluate_column_expression(col_spec, ctx, group->rows[0], NULL, col);
                    } else {
                        result->rows[g].values[col].type = VALUE_TYPE_NULL;
                    }
                }
            } else {
                /* regular column - use first row's value from the group */
                int col_idx = find_column_index_with_fallback(ctx->tables[0].table, col_name);
                
                if (col_idx >= 0 && group->row_count > 0) {
                    Value* src = &group->rows[0]->values[col_idx];
                    Value* dst = &result->rows[g].values[col];
                    
                    dst->type = src->type;
                    if (src->type == VALUE_TYPE_STRING && src->string_value) {
                        dst->string_value = strdup(src->string_value);
                    } else {
                        dst->int_value = src->int_value;
                    }
                } else {
                    result->rows[g].values[col].type = VALUE_TYPE_NULL;
                }
            }
        }
    }
    
    return result;
}

/* function to build result from filtered rows */
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
                            fprintf(stderr, "Error: Scalar subquery must return exactly one row and one column (got %d rows, %d columns)\n",
                                    subquery_result->row_count, subquery_result->column_count);
                            csv_free(subquery_result);
                            result->rows[i].values[j].type = VALUE_TYPE_NULL;
                        } else {
                            value_deep_copy(&result->rows[i].values[j], &subquery_result->rows[0].values[0]);
                            csv_free(subquery_result);
                        }
                    } else {
                        // evaluate any expression like identifier, binary_op, function, etc.
                        result->rows[i].values[j] = evaluate_expression(ctx, col_node, filtered_rows[i], 0);
                    }
                } else {
                    // regular column from table or string-based expression
                    result->rows[i].values[j] = evaluate_column_expression(
                        expanded_specs[j], ctx, filtered_rows[i], column_indices, j
                    );
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
    char** column_specs = malloc(sizeof(char*) * result->column_count);  // Store full specs for functions
    
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
                        fprintf(stderr, "Error: Scalar subquery must return exactly one row and one column (got %d rows, %d columns)\n",
                                subquery_result->row_count, subquery_result->column_count);
                        csv_free(subquery_result);
                        result->rows[i].values[j].type = VALUE_TYPE_NULL;
                    } else {
                        // copy the single value from subquery result
                        value_deep_copy(&result->rows[i].values[j], &subquery_result->rows[0].values[0]);
                        csv_free(subquery_result);
                    }
                } else {
                    // evaluate any expression like identifier, binary_op, function, etc.
                    result->rows[i].values[j] = evaluate_expression(ctx, col_node, filtered_rows[i], 0);
                }
            } else {
                result->rows[i].values[j] = evaluate_column_expression(
                    column_specs[j], ctx, filtered_rows[i], column_indices, j
                );
            }
        }
    }
    
    // free temporary arrays
    for (int i = 0; i < result->column_count; i++) {
        free(column_specs[i]);
    }
    free(column_specs);
    free(column_indices);
    
    return result;
}


typedef struct {
    CsvTable* table;
    int column_index;
    bool descending;
} SortContext;

/* Global context for generic sorting */
static SortContext* g_sort_ctx = NULL;

/* Compare function for row sorting */
static int compare_rows(const void* a, const void* b) {
    Row* row_a = *(Row**)a;
    Row* row_b = *(Row**)b;
    SortContext* sort_ctx = g_sort_ctx;
    
    if (!sort_ctx) return 0;
    
    int col_idx = sort_ctx->column_index;
    if (col_idx < 0 || col_idx >= row_a->column_count) return 0;
    
    Value* val_a = &row_a->values[col_idx];
    Value* val_b = &row_b->values[col_idx];
    
    int cmp = value_compare(val_a, val_b);
    
    return sort_ctx->descending ? -cmp : cmp;
}

static void sort_rows(Row** rows, int row_count, CsvTable* table, const char* column_name, bool descending) {
    int col_idx = find_column_index_with_fallback(table, column_name);
    
    if (col_idx < 0) {
        fprintf(stderr, "Warning: Cannot sort by unknown column '%s'\n", column_name);
        return;
    }
    
    SortContext sort_ctx;
    sort_ctx.table = table;
    sort_ctx.column_index = col_idx;
    sort_ctx.descending = descending;
    
    g_sort_ctx = &sort_ctx;
    qsort(rows, row_count, sizeof(Row*), compare_rows);
    g_sort_ctx = NULL;
}


typedef struct {
    ResultSet* result;
    int column_index;
    bool descending;
} ResultSortContext;

/* Global context for result sorting */
static ResultSortContext* g_result_sort_ctx = NULL;

/* Compare function for result sorting */
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

static void sort_result(ResultSet* result, ASTNode* select_node, const char* column_spec, bool descending) {
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
        fprintf(stderr, "Warning: Cannot sort by unknown column '%s' (looked for '%s')\n", column_spec, lookup_name);
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

/* helper to set NULL values for a range of columns in a row */
static void set_null_values(Value* values, int start, int count) {
    for (int i = start; i < start + count; i++) {
        values[i].type = VALUE_TYPE_NULL;
    }
}

/* helper to create a joined row with allocated values */
static Row* create_joined_row(CsvTable* result, int column_count) {
    Row* new_row = &result->rows[result->row_count++];
    new_row->column_count = column_count;
    new_row->values = malloc(sizeof(Value) * column_count);
    return new_row;
}

/* helper to copy table columns to result with alias prefix */
static void copy_columns_with_prefix(Column* dest, int dest_offset, CsvTable* table, const char* alias) {
    for (int i = 0; i < table->column_count; i++) {
        char prefixed_name[256];
        snprintf(prefixed_name, sizeof(prefixed_name), "%s.%s", alias, table->columns[i].name);
        dest[dest_offset + i].name = strdup(prefixed_name);
        dest[dest_offset + i].inferred_type = table->columns[i].inferred_type;
    }
}

/* helper to evaluate JOIN ON condition */
static bool evaluate_join_condition(QueryContext* ctx, ASTNode* on_condition,
                                     Row* left_row, Row* right_row) {
    if (!on_condition) return true;
    
    if (on_condition->type == NODE_TYPE_CONDITION &&
        strcmp(on_condition->condition.operator, "=") == 0 &&
        on_condition->condition.left->type == NODE_TYPE_IDENTIFIER &&
        on_condition->condition.right->type == NODE_TYPE_IDENTIFIER) {
        
        Value* left_val = resolve_column(ctx, on_condition->condition.left->identifier,
                                         left_row, 0);
        Value* right_val = resolve_column(ctx, on_condition->condition.right->identifier,
                                          right_row, 1);
        
        if (left_val && right_val) {
            return (value_compare(left_val, right_val) == 0);
        }
    }
    
    return false;
}


// simple JOIN implementation, it creates a temporary joined table
static CsvTable* perform_join(QueryContext* ctx, CsvTable* left_table, const char* left_alias,
                               CsvTable* right_table, const char* right_alias,
                               ASTNode* on_condition, JoinType join_type) {
    // Create result table with combined columns
    CsvTable* result = calloc(1, sizeof(CsvTable));
    result->filename = strdup("joined_result");
    result->has_header = true;
    result->delimiter = ',';
    
    // Combine column names with table prefixes
    result->column_count = left_table->column_count + right_table->column_count;
    result->columns = malloc(sizeof(Column) * result->column_count);
    
    copy_columns_with_prefix(result->columns, 0, left_table, left_alias);
    copy_columns_with_prefix(result->columns, left_table->column_count, right_table, right_alias);
    
    // allocate rows
    result->row_capacity = left_table->row_count * right_table->row_count;
    result->rows = malloc(sizeof(Row) * result->row_capacity);
    result->row_count = 0;
    
    // extend QueryContext to include both tables temporarily for condition evaluation
    int orig_table_count = ctx->table_count;
    TableRef* orig_tables = ctx->tables;
    
    ctx->table_count = 2;
    ctx->tables = malloc(sizeof(TableRef) * 2);
    ctx->tables[0].alias = strdup(left_alias);
    ctx->tables[0].table = left_table;
    ctx->tables[1].alias = strdup(right_alias);
    ctx->tables[1].table = right_table;
    
    // perform join
    for (int l = 0; l < left_table->row_count; l++) {
        bool found_match = false;
        
        for (int r = 0; r < right_table->row_count; r++) {
            // create temporary combined row for condition evaluation
            // for condition evaluation, we need to check if rows match
            
            bool matches = evaluate_join_condition(ctx, on_condition,
                                                    &left_table->rows[l], &right_table->rows[r]);
            
            if (matches || (join_type == JOIN_TYPE_INNER && on_condition == NULL)) {
                found_match = true;
                
                // create combined row
                Row* new_row = create_joined_row(result, result->column_count);
                
                // copy left table values
                for (int i = 0; i < left_table->column_count; i++) {
                    value_deep_copy(&new_row->values[i], &left_table->rows[l].values[i]);
                }
                
                // copy right table values
                for (int i = 0; i < right_table->column_count; i++) {
                    value_deep_copy(&new_row->values[left_table->column_count + i], &right_table->rows[r].values[i]);
                }
                
                if (result->row_count == 1) {
                    // debug
                }
            }
        }
        
        // LEFT/FULL JOIN, if no match found, add left row with NULLs for right
        if (!found_match && (join_type == JOIN_TYPE_LEFT || join_type == JOIN_TYPE_FULL)) {
            Row* new_row = create_joined_row(result, result->column_count);
            
            // copy left table values
            for (int i = 0; i < left_table->column_count; i++) {
                value_deep_copy(&new_row->values[i], &left_table->rows[l].values[i]);
            }
            
            // NULL values for right table
            set_null_values(new_row->values, left_table->column_count, right_table->column_count);
        }
    }
    
    // RIGHT/FULL JOIN: add unmatched rows from right table with NULLs for left
    if (join_type == JOIN_TYPE_RIGHT || join_type == JOIN_TYPE_FULL) {
        for (int r = 0; r < right_table->row_count; r++) {
            bool found_match = false;
            
            // check if this right row matched any left row
            for (int l = 0; l < left_table->row_count; l++) {
                bool matches = evaluate_join_condition(ctx, on_condition,
                                                        &left_table->rows[l], &right_table->rows[r]);
                
                if (matches) {
                    found_match = true;
                    break;
                }
            }
            
            // if no match, add right row with NULLs for left
            if (!found_match) {
                Row* new_row = create_joined_row(result, result->column_count);
                
                // NULL values for left table
                set_null_values(new_row->values, 0, left_table->column_count);
                
                // copy right table values
                for (int i = 0; i < right_table->column_count; i++) {
                    value_deep_copy(&new_row->values[left_table->column_count + i], &right_table->rows[r].values[i]);
                }
            }
        }
    }
    
    // restore original context
    free(ctx->tables[0].alias);
    free(ctx->tables[1].alias);
    free(ctx->tables);
    ctx->tables = orig_tables;
    ctx->table_count = orig_table_count;
    
    return result;
}


/* convert a ResultSet query result to a CsvTable for use in subqueries */
static CsvTable* result_to_csv_table(ResultSet* result) {
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
static void free_row_range(Row* rows, int start, int end) {
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

/* helper to load table from FROM clause */
static CsvTable* load_from_table(ASTNode* from_clause, const char** out_alias, QueryContext* ctx) {
    (void)ctx; // unused parameter, kept for future extensions
    
    if (!from_clause || from_clause->type != NODE_TYPE_FROM) {
        fprintf(stderr, "Error: FROM clause is required\n");
        return NULL;
    }
    
    CsvTable* source_table = NULL;
    const char* table_alias = NULL;
    
    if (from_clause->from.subquery) {
        ASTNode* subquery_node = from_clause->from.subquery;
        if (subquery_node->type != NODE_TYPE_SUBQUERY || !subquery_node->subquery.query) {
            fprintf(stderr, "Error: Invalid subquery in FROM clause\n");
            return NULL;
        }
        
        ResultSet* subquery_result = evaluate_query(subquery_node->subquery.query);
        if (!subquery_result) {
            fprintf(stderr, "Error: Subquery evaluation failed\n");
            return NULL;
        }
        
        source_table = result_to_csv_table(subquery_result);
        csv_free(subquery_result);
        
        if (!source_table) {
            fprintf(stderr, "Error: Failed to convert subquery result to table\n");
            return NULL;
        }
        
        table_alias = from_clause->from.alias ? from_clause->from.alias : "subquery";
    } else if (from_clause->from.table) {
        const char* filename = from_clause->from.table;
        source_table = csv_load(filename, global_csv_config);
        
        if (!source_table) {
            fprintf(stderr, "Failed to load table from '%s'\n", filename);
            return NULL;
        }
        
        table_alias = from_clause->from.alias ? from_clause->from.alias : "main";
    } else {
        fprintf(stderr, "Error: FROM clause must specify a table or subquery\n");
        return NULL;
    }
    
    *out_alias = table_alias;
    return source_table;
}

/* helper to process JOINs */
static CsvTable* process_joins(ASTNode* query_ast, QueryContext* ctx, CsvTable* base_table, const char* base_alias) {
    if (query_ast->query.join_count == 0) {
        return base_table;
    }
    
    CsvTable* working_table = base_table;
    const char* working_alias = base_alias;
    bool joined = false;
    
    for (int j = 0; j < query_ast->query.join_count; j++) {
        ASTNode* join_node = query_ast->query.joins[j];
        if (join_node->type != NODE_TYPE_JOIN) continue;
        
        CsvTable* right_table = csv_load(join_node->join.table, global_csv_config);
        if (!right_table) {
            fprintf(stderr, "Failed to load join table from '%s'\n", join_node->join.table);
            continue;
        }
        
        const char* right_alias = join_node->join.alias ? join_node->join.alias : "right";
        
        CsvTable* joined_table = perform_join(ctx, working_table, working_alias,
                                               right_table, right_alias,
                                               join_node->join.condition,
                                               join_node->join.join_type);
        
        if (joined) {
            csv_free(working_table);
        }
        csv_free(right_table);
        
        working_table = joined_table;
        working_alias = "joined";
        joined = true;
    }
    
    return joined ? working_table : base_table;
}

/* helper to apply WHERE filtering */
static Row** filter_rows(QueryContext* ctx, ASTNode* where_clause, int* out_filtered_count) {
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

/* helper to apply LIMIT and OFFSET to result */
static void apply_limit_offset(ResultSet* result, int limit, int offset) {
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
static ResultSet* set_union(ResultSet* left, ResultSet* right, bool include_duplicates) {
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
static ResultSet* set_intersect(ResultSet* left, ResultSet* right) {
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
static ResultSet* set_except(ResultSet* left, ResultSet* right) {
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
static void apply_distinct(ResultSet* result) {
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
            bool rows_equal = true;
            for (int col = 0; col < result->column_count; col++) {
                if (value_compare(&result->rows[i].values[col], 
                                 &result->rows[j].values[col]) != 0) {
                    rows_equal = false;
                    break;
                }
            }
            
            if (rows_equal) {
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

/* evaluation function that supports correlated subqueries */
static ResultSet* evaluate_query_internal(ASTNode* query_ast, Row* outer_row, CsvTable* outer_table) {
    if (!query_ast || query_ast->type != NODE_TYPE_QUERY) {
        fprintf(stderr, "Invalid query AST\n");
        return NULL;
    }
    
    // execution context
    QueryContext* ctx = context_create(query_ast);
    
    // set outer context for correlated subqueries
    ctx->outer_row = outer_row;
    ctx->outer_table = outer_table;
    
    // load table from FROM clause
    const char* table_alias = NULL;
    CsvTable* source_table = load_from_table(query_ast->query.from, &table_alias, ctx);
    if (!source_table) {
        context_free(ctx);
        return NULL;
    }
    
    ctx->table_count = 1;
    ctx->tables = malloc(sizeof(TableRef) * 1);
    ctx->tables[0].alias = strdup(table_alias);
    ctx->tables[0].table = source_table;
    
    // process JOINs if there are any
    CsvTable* working_table = process_joins(query_ast, ctx, ctx->tables[0].table, ctx->tables[0].alias);
    
    // replace context table with joined result if needed
    if (working_table != ctx->tables[0].table) {
        csv_free(ctx->tables[0].table);
        ctx->tables[0].table = working_table;
    }
    
    // apply WHERE filtering
    int filtered_count = 0;
    Row** filtered_rows = filter_rows(ctx, query_ast->query.where, &filtered_count);
    
    // check GROUP BY or aggregate functions
    ASTNode* group_by = query_ast->query.group_by;
    ResultSet* result;
    
    if (group_by && group_by->type == NODE_TYPE_GROUP_BY && group_by->identifier) {
        // group rows by column if specified
        const char* group_column = group_by->identifier;
        
        GroupResult* groups = create_groups(filtered_rows, filtered_count, ctx->tables[0].table, group_column);
        
        // compute aggregated result
        result = build_aggregated_result(ctx, groups, query_ast->query.select);
        
        free_groups(groups);
        
        // evaluate HAVING filter if present
        if (query_ast->query.having) {
            apply_having_filter(result, query_ast->query.having, query_ast->query.select);
        }
        
        // apply ORDER BY to the aggregated result
        ASTNode* order_by = query_ast->query.order_by;
        if (order_by && order_by->type == NODE_TYPE_ORDER_BY && order_by->order_by.column) {
            sort_result(result, query_ast->query.select, order_by->order_by.column, order_by->order_by.descending);
        }
    } else if (has_aggregate_functions(query_ast->query.select)) {
        // aggregate functions without GROUP BY, the entire result is a single group
        GroupResult* groups = malloc(sizeof(GroupResult));
        groups->group_count = 1;
        groups->groups = malloc(sizeof(GroupedRows));
        groups->groups[0].group_key = strdup("_all_");
        groups->groups[0].row_count = filtered_count;
        groups->groups[0].rows = filtered_rows; // Don't copy, just use the pointer
        
        // compute aggregated result
        result = build_aggregated_result(ctx, groups, query_ast->query.select);
        
        // clean up groups without freeing filtered_rows
        free(groups->groups[0].group_key);
        free(groups->groups);
        free(groups);
        
        // evaluate HAVING filter if present
        if (query_ast->query.having) {
            apply_having_filter(result, query_ast->query.having, query_ast->query.select);
        }
        
        // apply ORDER BY to aggregated result
        ASTNode* order_by = query_ast->query.order_by;
        if (order_by && order_by->type == NODE_TYPE_ORDER_BY && order_by->order_by.column) {
            sort_result(result, query_ast->query.select, order_by->order_by.column, order_by->order_by.descending);
        }
    } else {
        // apply ORDER BY for non-aggregated results
        ASTNode* order_by = query_ast->query.order_by;
        if (order_by && order_by->type == NODE_TYPE_ORDER_BY) {
            const char* col_name = order_by->order_by.column;
            bool descending = order_by->order_by.descending;
            
            if (col_name) {
                sort_rows(filtered_rows, filtered_count, ctx->tables[0].table, col_name, descending);
            }
        }
        
        // build result
        result = build_result(ctx, filtered_rows, filtered_count);
    }
    
    free(filtered_rows);
    context_free(ctx);
    
    // apply DISTINCT if specified
    if (query_ast->query.select && query_ast->query.select->select.distinct) {
        apply_distinct(result);
    }
    
    // apply LIMIT and OFFSET
    apply_limit_offset(result, query_ast->query.limit, query_ast->query.offset);
    
    return result;
}

/* API wrapper to evaluates query without outer context */
ResultSet* evaluate_query(ASTNode* query_ast) {
    if (!query_ast) return NULL;
    
    // handle DML statements (INSERT, UPDATE, DELETE, CREATE TABLE, ALTER TABLE)
    if (query_ast->type == NODE_TYPE_INSERT) {
        return evaluate_insert(query_ast);
    } else if (query_ast->type == NODE_TYPE_UPDATE) {
        return evaluate_update(query_ast);
    } else if (query_ast->type == NODE_TYPE_DELETE) {
        return evaluate_delete(query_ast);
    } else if (query_ast->type == NODE_TYPE_CREATE_TABLE) {
        return evaluate_create_table(query_ast);
    } else if (query_ast->type == NODE_TYPE_ALTER_TABLE) {
        return evaluate_alter_table(query_ast);
    }
    
    // handle set operations (UNION, INTERSECT, EXCEPT)
    if (query_ast->type == NODE_TYPE_SET_OP) {
        ResultSet* left = evaluate_query(query_ast->set_op.left);
        if (!left) return NULL;
        
        ResultSet* right = evaluate_query(query_ast->set_op.right);
        if (!right) {
            csv_free(left);
            return NULL;
        }
        
        // check column count compatibility
        if (left->column_count != right->column_count) {
            fprintf(stderr, "Error: SET operation queries must have the same number of columns\n");
            csv_free(left);
            csv_free(right);
            return NULL;
        }
        
        ResultSet* result = NULL;
        
        switch (query_ast->set_op.op_type) {
            case SET_OP_UNION:
                result = set_union(left, right, false);
                break;
            case SET_OP_UNION_ALL:
                result = set_union(left, right, true);
                break;
            case SET_OP_INTERSECT:
                result = set_intersect(left, right);
                break;
            case SET_OP_EXCEPT:
                result = set_except(left, right);
                break;
        }
        
        csv_free(left);
        csv_free(right);
        return result;
    }
    
    return evaluate_query_internal(query_ast, NULL, NULL);
}

/* evaluate INSERT statement */
static ResultSet* evaluate_insert(ASTNode* insert_node) {
    // load existing table
    CsvTable* table = load_table_from_string(insert_node->insert.table);
    if (!table) {
        fprintf(stderr, "Error: Could not load table '%s'\n", insert_node->insert.table);
        return NULL;
    }
    
    // validate column count
    int value_count = insert_node->insert.value_count;
    if (insert_node->insert.columns) {
        // column list specified
        if (insert_node->insert.column_count != value_count) {
            fprintf(stderr, "Error: Column count (%d) does not match value count (%d)\n",
                    insert_node->insert.column_count, value_count);
            csv_free(table);
            return NULL;
        }
    } else {
        // no column list, values must match all columns
        if (value_count != table->column_count) {
            fprintf(stderr, "Error: Value count (%d) does not match table column count (%d)\n",
                    value_count, table->column_count);
            csv_free(table);
            return NULL;
        }
    }
    
    // create new row
    Row new_row;
    new_row.column_count = table->column_count;
    new_row.values = calloc(table->column_count, sizeof(Value));
    
    // initialize all values as NULL
    for (int i = 0; i < table->column_count; i++) {
        new_row.values[i].type = VALUE_TYPE_NULL;
    }
    
    // fill in values
    for (int i = 0; i < value_count; i++) {
        int target_col = i;
        
        // if column list specified, find target column index
        if (insert_node->insert.columns) {
            const char* col_name = insert_node->insert.columns[i];
            target_col = csv_get_column_index(table, col_name);
            if (target_col < 0) {
                fprintf(stderr, "Error: Column '%s' not found in table\n", col_name);
                free(new_row.values);
                csv_free(table);
                return NULL;
            }
        }
        
        // evaluate value expression
        ASTNode* val_node = insert_node->insert.values[i];
        
        // for simple literals, convert directly
        if (val_node->type == NODE_TYPE_LITERAL) {
            const char* literal = val_node->literal;
            Value parsed = parse_value(literal, strlen(literal));
            new_row.values[target_col] = parsed;
        } else if (val_node->type == NODE_TYPE_BINARY_OP) {
            // evaluate arithmetic expression
            QueryContext temp_ctx = {0};
            Value result = evaluate_expression(&temp_ctx, val_node, NULL, 0);
            new_row.values[target_col] = result;
        } else {
            fprintf(stderr, "Error: Unsupported value expression in INSERT\n");
            free(new_row.values);
            csv_free(table);
            return NULL;
        }
    }
    
    // add row to table
    if (table->row_count >= table->row_capacity) {
        table->row_capacity = table->row_capacity ? table->row_capacity * 2 : 8;
        table->rows = realloc(table->rows, sizeof(Row) * table->row_capacity);
    }
    table->rows[table->row_count++] = new_row;
    
    // save table back to file
    if (!csv_save(insert_node->insert.table, table)) {
        fprintf(stderr, "Error: Could not save table '%s'\n", insert_node->insert.table);
        csv_free(table);
        return NULL;
    }
    
    // return result message
    ResultSet* result = malloc(sizeof(ResultSet));
    result->filename = strdup("INSERT result");
    result->data = NULL;
    result->file_size = 0;
    result->fd = -1;
    result->column_count = 1;
    result->columns = malloc(sizeof(Column));
    result->columns[0].name = strdup("message");
    result->columns[0].inferred_type = VALUE_TYPE_STRING;
    result->row_count = 1;
    result->row_capacity = 1;
    result->rows = malloc(sizeof(Row));
    result->rows[0].column_count = 1;
    result->rows[0].values = malloc(sizeof(Value));
    result->rows[0].values[0].type = VALUE_TYPE_STRING;
    result->rows[0].values[0].string_value = malloc(100);
    snprintf(result->rows[0].values[0].string_value, 100, "Inserted 1 row");
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    csv_free(table);
    return result;
}

/* evaluate UPDATE statement */
static ResultSet* evaluate_update(ASTNode* update_node) {
    // load existing table
    CsvTable* table = load_table_from_string(update_node->update.table);
    if (!table) {
        fprintf(stderr, "Error: Could not load table '%s'\n", update_node->update.table);
        return NULL;
    }
    
    // create context for condition evaluation
    QueryContext ctx;
    ctx.tables = malloc(sizeof(TableRef));
    ctx.tables[0].alias = strdup("__main__");
    ctx.tables[0].table = table;
    ctx.table_count = 1;
    ctx.query = NULL;
    ctx.outer_row = NULL;
    ctx.outer_table = NULL;
    
    int updated_count = 0;
    
    // process each row
    for (int row = 0; row < table->row_count; row++) {
        // check WHERE condition
        bool matches = true;
        if (update_node->update.where) {
            matches = evaluate_condition(&ctx, update_node->update.where, &table->rows[row], 0);
        }
        
        if (matches) {
            // update columns
            for (int i = 0; i < update_node->update.assignment_count; i++) {
                ASTNode* assignment = update_node->update.assignments[i];
                const char* col_name = assignment->assignment.column;
                
                int col_idx = csv_get_column_index(table, col_name);
                if (col_idx < 0) {
                    fprintf(stderr, "Error: Column '%s' not found\n", col_name);
                    context_free(&ctx);
                    csv_free(table);
                    return NULL;
                }
                
                // Note: We don't free old value because it may point to mmap'd data
                // The value will be overwritten and csv_save will write the new value
                
                // evaluate new value
                ASTNode* val_node = assignment->assignment.value;
                if (val_node->type == NODE_TYPE_LITERAL) {
                    const char* literal = val_node->literal;
                    Value parsed = parse_value(literal, strlen(literal));
                    table->rows[row].values[col_idx] = parsed;
                } else {
                    Value result = evaluate_expression(&ctx, val_node, &table->rows[row], 0);
                    table->rows[row].values[col_idx] = result;
                }
            }
            updated_count++;
        }
    }
    
    // save table back to file
    if (!csv_save(update_node->update.table, table)) {
        fprintf(stderr, "Error: Could not save table '%s'\n", update_node->update.table);
        free(ctx.tables[0].alias);
        free(ctx.tables);
        csv_free(table);
        return NULL;
    }
    
    // return result message
    ResultSet* result = malloc(sizeof(ResultSet));
    result->filename = strdup("UPDATE result");
    result->data = NULL;
    result->file_size = 0;
    result->fd = -1;
    result->column_count = 1;
    result->columns = malloc(sizeof(Column));
    result->columns[0].name = strdup("message");
    result->columns[0].inferred_type = VALUE_TYPE_STRING;
    result->row_count = 1;
    result->row_capacity = 1;
    result->rows = malloc(sizeof(Row));
    result->rows[0].column_count = 1;
    result->rows[0].values = malloc(sizeof(Value));
    result->rows[0].values[0].type = VALUE_TYPE_STRING;
    result->rows[0].values[0].string_value = malloc(100);
    snprintf(result->rows[0].values[0].string_value, 100, "Updated %d row(s)", updated_count);
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    // Free context manually (don't use context_free to avoid double-free of table)
    free(ctx.tables[0].alias);
    free(ctx.tables);
    csv_free(table);
    return result;
}

/* evaluate DELETE statement */
static ResultSet* evaluate_delete(ASTNode* delete_node) {
    // load existing table
    CsvTable* table = load_table_from_string(delete_node->delete_stmt.table);
    if (!table) {
        fprintf(stderr, "Error: Could not load table '%s'\n", delete_node->delete_stmt.table);
        return NULL;
    }
    
    // create context for condition evaluation
    QueryContext ctx;
    ctx.tables = malloc(sizeof(TableRef));
    ctx.tables[0].alias = strdup("__main__");
    ctx.tables[0].table = table;
    ctx.table_count = 1;
    ctx.query = NULL;
    ctx.outer_row = NULL;
    ctx.outer_table = NULL;
    
    // find rows to delete
    Row** rows_to_keep = malloc(sizeof(Row*) * table->row_count);
    int keep_count = 0;
    int deleted_count = 0;
    
    for (int row = 0; row < table->row_count; row++) {
        bool matches = evaluate_condition(&ctx, delete_node->delete_stmt.where, &table->rows[row], 0);
        
        if (!matches) {
            // keep this row
            rows_to_keep[keep_count++] = &table->rows[row];
        } else {
            // delete this row - free string values
            deleted_count++;
            for (int col = 0; col < table->rows[row].column_count; col++) {
                if (table->rows[row].values[col].type == VALUE_TYPE_STRING) {
                    free(table->rows[row].values[col].string_value);
                }
            }
            free(table->rows[row].values);
        }
    }
    
    // rebuild rows array
    Row* new_rows = malloc(sizeof(Row) * keep_count);
    for (int i = 0; i < keep_count; i++) {
        new_rows[i] = *rows_to_keep[i];
    }
    free(rows_to_keep);
    free(table->rows);
    table->rows = new_rows;
    table->row_count = keep_count;
    table->row_capacity = keep_count;
    
    // save table back to file
    if (!csv_save(delete_node->delete_stmt.table, table)) {
        fprintf(stderr, "Error: Could not save table '%s'\n", delete_node->delete_stmt.table);
        free(ctx.tables[0].alias);
        free(ctx.tables);
        csv_free(table);
        return NULL;
    }
    
    // return result message
    ResultSet* result = malloc(sizeof(ResultSet));
    result->filename = strdup("DELETE result");
    result->data = NULL;
    result->file_size = 0;
    result->fd = -1;
    result->column_count = 1;
    result->columns = malloc(sizeof(Column));
    result->columns[0].name = strdup("message");
    result->columns[0].inferred_type = VALUE_TYPE_STRING;
    result->row_count = 1;
    result->row_capacity = 1;
    result->rows = malloc(sizeof(Row));
    result->rows[0].column_count = 1;
    result->rows[0].values = malloc(sizeof(Value));
    result->rows[0].values[0].type = VALUE_TYPE_STRING;
    result->rows[0].values[0].string_value = malloc(100);
    snprintf(result->rows[0].values[0].string_value, 100, "Deleted %d row(s)", deleted_count);
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    // free context manually, don't use context_free to avoid double-free of table
    free(ctx.tables[0].alias);
    free(ctx.tables);
    csv_free(table);
    return result;
}

/* evaluate CREATE TABLE statement */
static ResultSet* evaluate_create_table(ASTNode* create_node) {
    const char* filepath = create_node->create_table.table;
    
    if (create_node->create_table.is_schema_only) {
        // CREATE TABLE 'file.csv' (col1, col2, col3) - create empty file with header
        if (create_node->create_table.column_count == 0) {
            fprintf(stderr, "Error: No columns specified for CREATE TABLE\n");
            return NULL;
        }
        
        // create empty CSV table with just header
        CsvTable* table = malloc(sizeof(CsvTable));
        table->filename = strdup(filepath);
        table->data = NULL;
        table->file_size = 0;
        table->fd = -1;
        table->column_count = create_node->create_table.column_count;
        table->columns = malloc(sizeof(Column) * table->column_count);
        
        for (int i = 0; i < table->column_count; i++) {
            table->columns[i].name = strdup(create_node->create_table.columns[i]);
            table->columns[i].inferred_type = VALUE_TYPE_STRING;  // default type
        }
        
        table->row_count = 0;
        table->row_capacity = 0;
        table->rows = NULL;
        table->has_header = true;
        table->delimiter = ',';
        table->quote = '"';
        
        // save to file
        if (!csv_save(filepath, table)) {
            fprintf(stderr, "Error: Could not create table '%s'\n", filepath);
            csv_free(table);
            return NULL;
        }
        
        csv_free(table);
        
        // return success message
        ResultSet* result = malloc(sizeof(ResultSet));
        result->filename = strdup("CREATE TABLE result");
        result->data = NULL;
        result->file_size = 0;
        result->fd = -1;
        result->column_count = 1;
        result->columns = malloc(sizeof(Column));
        result->columns[0].name = strdup("message");
        result->columns[0].inferred_type = VALUE_TYPE_STRING;
        result->row_count = 1;
        result->row_capacity = 1;
        result->rows = malloc(sizeof(Row));
        result->rows[0].column_count = 1;
        result->rows[0].values = malloc(sizeof(Value));
        result->rows[0].values[0].type = VALUE_TYPE_STRING;
        result->rows[0].values[0].string_value = malloc(100);
        snprintf(result->rows[0].values[0].string_value, 100, 
                "Created table '%s' with %d column(s)", 
                filepath, create_node->create_table.column_count);
        result->has_header = true;
        result->delimiter = ',';
        result->quote = '"';
        
        return result;
        
    } else if (create_node->create_table.query) {
        // CREATE TABLE 'file.csv' AS SELECT ... - save query results
        ResultSet* query_result = evaluate_query(create_node->create_table.query);
        if (!query_result) {
            fprintf(stderr, "Error: Failed to execute query in CREATE TABLE AS\n");
            return NULL;
        }
        
        // save result to file
        if (!csv_save(filepath, query_result)) {
            fprintf(stderr, "Error: Could not save table '%s'\n", filepath);
            csv_free(query_result);
            return NULL;
        }
        
        int saved_rows = query_result->row_count;
        csv_free(query_result);
        
        // return success message
        ResultSet* result = malloc(sizeof(ResultSet));
        result->filename = strdup("CREATE TABLE result");
        result->data = NULL;
        result->file_size = 0;
        result->fd = -1;
        result->column_count = 1;
        result->columns = malloc(sizeof(Column));
        result->columns[0].name = strdup("message");
        result->columns[0].inferred_type = VALUE_TYPE_STRING;
        result->row_count = 1;
        result->row_capacity = 1;
        result->rows = malloc(sizeof(Row));
        result->rows[0].column_count = 1;
        result->rows[0].values = malloc(sizeof(Value));
        result->rows[0].values[0].type = VALUE_TYPE_STRING;
        result->rows[0].values[0].string_value = malloc(100);
        snprintf(result->rows[0].values[0].string_value, 100, 
                "Created table '%s' with %d row(s)", 
                filepath, saved_rows);
        result->has_header = true;
        result->delimiter = ',';
        result->quote = '"';
        
        return result;
    } else {
        fprintf(stderr, "Error: Invalid CREATE TABLE statement\n");
        return NULL;
    }
}
/*
 * evaluate ALTER TABLE statement
 * supports:
 *   - RENAME COLUMN: changes column name in header
 *   - ADD COLUMN: adds new column to end of header (fills with empty values)
 *   - DROP COLUMN: removes column from header and all data
 */
static ResultSet* evaluate_alter_table(ASTNode* alter_node) {
    const char* filepath = alter_node->alter_table.table;
    
    // load the CSV file
    CsvConfig config = global_csv_config;
    CsvTable* table = csv_load(filepath, config);
    if (!table) {
        fprintf(stderr, "Error: Could not load table '%s'\n", filepath);
        return NULL;
    }
    
    char message[200];
    bool modified = false;
    
    switch (alter_node->alter_table.operation) {
        case ALTER_RENAME_COLUMN: {
            // find old column
            int col_idx = -1;
            for (int i = 0; i < table->column_count; i++) {
                if (strcasecmp(table->columns[i].name, alter_node->alter_table.old_column_name) == 0) {
                    col_idx = i;
                    break;
                }
            }
            
            if (col_idx == -1) {
                fprintf(stderr, "Error: Column '%s' not found in table\n", 
                       alter_node->alter_table.old_column_name);
                csv_free(table);
                return NULL;
            }
            
            // rename column
            free(table->columns[col_idx].name);
            table->columns[col_idx].name = strdup(alter_node->alter_table.new_column_name);
            
            snprintf(message, sizeof(message), 
                    "Renamed column '%s' to '%s' in table '%s'",
                    alter_node->alter_table.old_column_name,
                    alter_node->alter_table.new_column_name,
                    filepath);
            modified = true;
            break;
        }
        
        case ALTER_ADD_COLUMN: {
            // check if column already exists
            for (int i = 0; i < table->column_count; i++) {
                if (strcasecmp(table->columns[i].name, alter_node->alter_table.new_column_name) == 0) {
                    fprintf(stderr, "Error: Column '%s' already exists in table\n", 
                           alter_node->alter_table.new_column_name);
                    csv_free(table);
                    return NULL;
                }
            }
            
            // add new column to header
            table->columns = realloc(table->columns, sizeof(Column) * (table->column_count + 1));
            table->columns[table->column_count].name = strdup(alter_node->alter_table.new_column_name);
            table->columns[table->column_count].inferred_type = VALUE_TYPE_STRING;
            table->column_count++;
            
            // add empty value to all existing rows
            for (int i = 0; i < table->row_count; i++) {
                table->rows[i].values = realloc(table->rows[i].values, 
                                               sizeof(Value) * table->column_count);
                table->rows[i].values[table->column_count - 1].type = VALUE_TYPE_STRING;
                table->rows[i].values[table->column_count - 1].string_value = strdup("");
                table->rows[i].column_count = table->column_count;
            }
            
            snprintf(message, sizeof(message), 
                    "Added column '%s' to table '%s'",
                    alter_node->alter_table.new_column_name,
                    filepath);
            modified = true;
            break;
        }
        
        case ALTER_DROP_COLUMN: {
            // find column to drop
            int col_idx = -1;
            for (int i = 0; i < table->column_count; i++) {
                if (strcasecmp(table->columns[i].name, alter_node->alter_table.old_column_name) == 0) {
                    col_idx = i;
                    break;
                }
            }
            
            if (col_idx == -1) {
                fprintf(stderr, "Error: Column '%s' not found in table\n", 
                       alter_node->alter_table.old_column_name);
                csv_free(table);
                return NULL;
            }
            
            if (table->column_count == 1) {
                fprintf(stderr, "Error: Cannot drop the last column\n");
                csv_free(table);
                return NULL;
            }
            
            // remove column from header
            free(table->columns[col_idx].name);
            for (int i = col_idx; i < table->column_count - 1; i++) {
                table->columns[i] = table->columns[i + 1];
            }
            table->column_count--;
            table->columns = realloc(table->columns, sizeof(Column) * table->column_count);
            
            // remove column from all rows
            for (int i = 0; i < table->row_count; i++) {
                if (table->rows[i].values[col_idx].type == VALUE_TYPE_STRING) {
                    free(table->rows[i].values[col_idx].string_value);
                }
                
                for (int j = col_idx; j < table->rows[i].column_count - 1; j++) {
                    table->rows[i].values[j] = table->rows[i].values[j + 1];
                }
                
                table->rows[i].column_count--;
                table->rows[i].values = realloc(table->rows[i].values, 
                                               sizeof(Value) * table->rows[i].column_count);
            }
            
            snprintf(message, sizeof(message), 
                    "Dropped column '%s' from table '%s'",
                    alter_node->alter_table.old_column_name,
                    filepath);
            modified = true;
            break;
        }
        
        default:
            fprintf(stderr, "Error: Unknown ALTER TABLE operation\n");
            csv_free(table);
            return NULL;
    }
    
    // save modified table back to file
    if (modified) {
        if (!csv_save(filepath, table)) {
            fprintf(stderr, "Error: Could not save modified table '%s'\n", filepath);
            csv_free(table);
            return NULL;
        }
    }
    
    csv_free(table);
    
    // return success message
    ResultSet* result = malloc(sizeof(ResultSet));
    result->filename = strdup("ALTER TABLE result");
    result->data = NULL;
    result->file_size = 0;
    result->fd = -1;
    result->column_count = 1;
    result->columns = malloc(sizeof(Column));
    result->columns[0].name = strdup("message");
    result->columns[0].inferred_type = VALUE_TYPE_STRING;
    result->row_count = 1;
    result->row_capacity = 1;
    result->rows = malloc(sizeof(Row));
    result->rows[0].column_count = 1;
    result->rows[0].values = malloc(sizeof(Value));
    result->rows[0].values[0].type = VALUE_TYPE_STRING;
    result->rows[0].values[0].string_value = strdup(message);
    result->has_header = true;
    result->delimiter = ',';
    result->quote = '"';
    
    return result;
}