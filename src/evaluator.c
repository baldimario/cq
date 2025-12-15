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
#include "evaluator/evaluator_core.h"
#include "evaluator/evaluator_expressions.h"
#include "evaluator/evaluator_conditions.h"
#include "evaluator/evaluator_functions.h"
#include "evaluator/evaluator_aggregates.h"
#include "evaluator/evaluator_window.h"
#include "evaluator/evaluator_joins.h"
#include "evaluator/evaluator_statements.h"
#include "evaluator/evaluator_utils.h"

/* global csv configuration to can be set before calling evaluate_query */
CsvConfig global_csv_config = {.delimiter = ',', .quote = '"', .has_header = true};

/* main internal query evaluation logic */
ResultSet* evaluate_query_internal(ASTNode* query_ast, Row* outer_row, CsvTable* outer_table) {
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
    
    if (group_by && group_by->type == NODE_TYPE_GROUP_BY && group_by->group_by.columns && group_by->group_by.column_count > 0) {
        // group rows by columns
        GroupResult* groups = NULL;
        ASTNode* select_node = query_ast->query.select;
        
        // build array of column names/expressions for grouping
        char** group_columns = malloc(sizeof(char*) * group_by->group_by.column_count);
        ASTNode** group_exprs = malloc(sizeof(ASTNode*) * group_by->group_by.column_count);
        
        for (int g = 0; g < group_by->group_by.column_count; g++) {
            const char* group_column = group_by->group_by.columns[g];
            group_columns[g] = (char*)group_column;
            group_exprs[g] = NULL;
            
            // check if group_column is an alias in the SELECT clause
            if (select_node && select_node->type == NODE_TYPE_SELECT && select_node->select.column_nodes) {
                for (int i = 0; i < select_node->select.column_count; i++) {
                    const char* col_str = select_node->select.columns[i];
                    if (!col_str) continue;
                    
                    // check for " AS alias" pattern
                    const char* as_pos = cq_strcasestr(col_str, " AS ");
                    if (as_pos) {
                        const char* alias_start = as_pos + 4;
                        while (*alias_start && isspace(*alias_start)) alias_start++;
                        
                        if (strcasecmp(alias_start, group_column) == 0) {
                            // found alias! use the expression
                            group_exprs[g] = select_node->select.column_nodes[i];
                            break;
                        }
                    }
                }
            }
        }
        
        // create groups with composite keys
        if (group_by->group_by.column_count == 1 && !group_exprs[0]) {
            // single column no expression - use optimized single-column grouping
            groups = create_groups(filtered_rows, filtered_count, ctx->tables[0].table, group_columns[0]);
        } else if (group_by->group_by.column_count == 1 && group_exprs[0]) {
            // single expression
            groups = create_groups_by_expression(ctx, filtered_rows, filtered_count, group_exprs[0]);
        } else {
            // multiple columns - create composite grouping
            groups = calloc(1, sizeof(GroupResult));
            groups->group_capacity = 16;
            groups->groups = malloc(sizeof(GroupedRows) * groups->group_capacity);
            groups->group_count = 0;
            
            for (int i = 0; i < filtered_count; i++) {
                // build composite key from all group columns
                char composite_key[1024] = "";
                
                for (int g = 0; g < group_by->group_by.column_count; g++) {
                    if (g > 0) strcat(composite_key, "\t");  // use tab as separator
                    
                    char key_part[256];
                    if (group_exprs[g]) {
                        // evaluate expression
                        Value val = evaluate_expression(ctx, group_exprs[g], filtered_rows[i], 0);
                        switch (val.type) {
                            case VALUE_TYPE_NULL:
                                strcpy(key_part, "NULL");
                                break;
                            case VALUE_TYPE_INTEGER:
                                snprintf(key_part, sizeof(key_part), "%lld", val.int_value);
                                break;
                            case VALUE_TYPE_DOUBLE:
                                snprintf(key_part, sizeof(key_part), "%.6f", val.double_value);
                                break;
                            case VALUE_TYPE_DATE:
                                snprintf(key_part, sizeof(key_part), "%04d-%02d-%02d",
                                         val.date_value.year, val.date_value.month, val.date_value.day);
                                break;
                            case VALUE_TYPE_STRING:
                                strncpy(key_part, val.string_value, sizeof(key_part) - 1);
                                key_part[sizeof(key_part) - 1] = '\0';
                                free((char*)val.string_value);
                                break;
                        }
                    } else {
                        // get column value
                        int col_idx = csv_get_column_index(ctx->tables[0].table, group_columns[g]);
                        if (col_idx >= 0) {
                            Value* val = &filtered_rows[i]->values[col_idx];
                            switch (val->type) {
                                case VALUE_TYPE_NULL:
                                    strcpy(key_part, "NULL");
                                    break;
                                case VALUE_TYPE_INTEGER:
                                    snprintf(key_part, sizeof(key_part), "%lld", val->int_value);
                                    break;
                                case VALUE_TYPE_DOUBLE:
                                    snprintf(key_part, sizeof(key_part), "%.6f", val->double_value);
                                    break;
                                case VALUE_TYPE_DATE:
                                    snprintf(key_part, sizeof(key_part), "%04d-%02d-%02d",
                                             val->date_value.year, val->date_value.month, val->date_value.day);
                                    break;
                                case VALUE_TYPE_STRING:
                                    strncpy(key_part, val->string_value, sizeof(key_part) - 1);
                                    key_part[sizeof(key_part) - 1] = '\0';
                                    break;
                            }
                        } else {
                            strcpy(key_part, "NULL");
                        }
                    }
                    strcat(composite_key, key_part);
                }
                
                // find or create group
                int group_idx = -1;
                for (int j = 0; j < groups->group_count; j++) {
                    if (strcmp(groups->groups[j].group_key, composite_key) == 0) {
                        group_idx = j;
                        break;
                    }
                }
                
                if (group_idx < 0) {
                    // create new group
                    if (groups->group_count >= groups->group_capacity) {
                        groups->group_capacity *= 2;
                        groups->groups = realloc(groups->groups, sizeof(GroupedRows) * groups->group_capacity);
                    }
                    
                    group_idx = groups->group_count++;
                    groups->groups[group_idx].group_key = strdup(composite_key);
                    groups->groups[group_idx].row_capacity = 16;
                    groups->groups[group_idx].rows = malloc(sizeof(Row*) * groups->groups[group_idx].row_capacity);
                    groups->groups[group_idx].row_count = 0;
                }
                
                // add row to group
                GroupedRows* group = &groups->groups[group_idx];
                if (group->row_count >= group->row_capacity) {
                    group->row_capacity *= 2;
                    group->rows = realloc(group->rows, sizeof(Row*) * group->row_capacity);
                }
                group->rows[group->row_count++] = filtered_rows[i];
            }
        }
        
        free(group_columns);
        free(group_exprs);
        
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
        // aggregate functions without GROUP BY - entire result is a single group
        GroupResult* groups = malloc(sizeof(GroupResult));
        groups->group_count = 1;
        groups->groups = malloc(sizeof(GroupedRows));
        groups->groups[0].group_key = strdup("_all_");
        groups->groups[0].row_count = filtered_count;
        groups->groups[0].rows = filtered_rows;
        
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
        // build result first so ORDER BY can use aliases
        result = build_result(ctx, filtered_rows, filtered_count);
        
        // apply ORDER BY for non-aggregated results
        ASTNode* order_by = query_ast->query.order_by;
        if (order_by && order_by->type == NODE_TYPE_ORDER_BY) {
            const char* col_name = order_by->order_by.column;
            bool descending = order_by->order_by.descending;
            
            if (col_name) {
                sort_result(result, query_ast->query.select, col_name, descending);
            }
        }
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

/* api wrapper to evaluates query without outer context */
ResultSet* evaluate_query(ASTNode* query_ast) {
    if (!query_ast) return NULL;
    
    // handle dml statements
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
    
    // handle set operations
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
