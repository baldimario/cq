#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../include/libcq.h"
#include "../include/parser.h"
#include "../include/evaluator.h"
#include "../include/csv_reader.h"
#include "../include/utils.h"

// internal structures

struct cq_context {
    int errno;
    char* error_msg;
};

struct cq_result {
    ResultSet* result;
};

struct cq_builder {
    char* select_clause;
    char* from_clause;
    char* where_clause;
    char* having_clause;
    char* order_by_clause;
    char* limit_clause;
    char* offset_clause;
    char* join_clause;
    char* group_by_clause;
    // for simplicity, use strings; in production, might use AST
};

/* helper functions */

void set_error(cq_context* ctx, int errno, const char* msg) {
    ctx->errno = errno;
    free(ctx->error_msg);
    ctx->error_msg = strdup(msg);
}

char* append_string(char* dest, const char* src) {
    if (!dest) {
        return strdup(src);
    }
    size_t len = strlen(dest) + strlen(src) + 2;
    dest = realloc(dest, len);
    strcat(dest, src);
    return dest;
}

/* initialization */

cq_context* cq_init(void) {
    cq_context* ctx = calloc(1, sizeof(cq_context));
    if (!ctx) return NULL;
    ctx->errno = 0;
    ctx->error_msg = NULL;
    return ctx;
}

void cq_free(cq_context* ctx) {
    if (!ctx) return;
    free(ctx->error_msg);
    free(ctx);
}

/* execute queries */

cq_result* cq_execute(cq_context* ctx, const char* sql) {
    if (!ctx || !sql) {
        set_error(ctx, 1, "Invalid arguments");
        return NULL;
    }

    // parse SQL
    ASTNode* ast = parse(sql);
    if (!ast) {
        set_error(ctx, 2, "Parsing failed");
        return NULL;
    }

    // evaluate
    ResultSet* result = evaluate_query(ast);
    if (!result) {
        set_error(ctx, 3, "Query evaluation failed");
        releaseNode(ast);
        return NULL;
    }

    // wrap in cq_result
    cq_result* cq_res = malloc(sizeof(cq_result));
    if (!cq_res) {
        set_error(ctx, 4, "Memory allocation failed");
        csv_free(result);
        releaseNode(ast);
        return NULL;
    }
    cq_res->result = result;

    // cleanup
    releaseNode(ast);

    return cq_res;
}

/* result access */

int cq_result_rows(cq_result* res) {
    return res ? res->result->row_count : 0;
}

int cq_result_cols(cq_result* res) {
    return res ? res->result->column_count : 0;
}

const char* cq_result_column_name(cq_result* res, int col) {
    if (!res || col < 0 || col >= res->result->column_count) return NULL;
    return res->result->columns[col].name;
}

const char* cq_result_get(cq_result* res, int row, int col) {
    if (!res || row < 0 || row >= res->result->row_count || col < 0 || col >= res->result->column_count) return NULL;
    Value* val = &res->result->rows[row].values[col];
    if (val->type == VALUE_TYPE_NULL) return NULL;
    // for simplicity return string representation, in production, cache or something
    return value_to_string(val);
}

double cq_result_get_double(cq_result* res, int row, int col) {
    if (!res || row < 0 || row >= res->result->row_count || col < 0 || col >= res->result->column_count) return 0.0;
    Value* val = &res->result->rows[row].values[col];
    if (val->type == VALUE_TYPE_DOUBLE) return val->double_value;
    if (val->type == VALUE_TYPE_INTEGER) return (double)val->int_value;
    return 0.0;
}

long cq_result_get_int(cq_result* res, int row, int col) {
    if (!res || row < 0 || row >= res->result->row_count || col < 0 || col >= res->result->column_count) return 0;
    Value* val = &res->result->rows[row].values[col];
    if (val->type == VALUE_TYPE_INTEGER) return val->int_value;
    if (val->type == VALUE_TYPE_DOUBLE) return (long)val->double_value;
    return 0;
}

int cq_result_is_null(cq_result* res, int row, int col) {
    if (!res || row < 0 || row >= res->result->row_count || col < 0 || col >= res->result->column_count) return 1;
    Value* val = &res->result->rows[row].values[col];
    return val->type == VALUE_TYPE_NULL;
}

void cq_result_free(cq_result* res) {
    if (!res) return;
    if (res->result) csv_free(res->result);
    free(res);
}

int cq_execute_file(cq_context* ctx, const char* csv_path, const char* sql, const char* output_path) {
    if (!ctx || !csv_path || !sql || !output_path) {
        set_error(ctx, 1, "Invalid arguments");
        return 0;
    }

    // for file execution, we need to set the global config or something, but for simplicity, assume default.
    
    // parse SQL
    ASTNode* ast = parse(sql);
    if (!ast) {
        set_error(ctx, 2, "Parsing failed");
        return 0;
    }

    // evaluate
    ResultSet* result = evaluate_query(ast);
    if (!result) {
        set_error(ctx, 3, "Query evaluation failed");
        releaseNode(ast);
        return 0;
    }

    // save to file
    if (!csv_save(output_path, result)) {
        set_error(ctx, 4, "Failed to save output file");
        csv_free(result);
        releaseNode(ast);
        return 0;
    }

    // cleanup
    csv_free(result);
    releaseNode(ast);
    return 1;
}

/* error handling */

int cq_errno(cq_context* ctx) {
    return ctx ? ctx->errno : 0;
}

const char* cq_error(cq_context* ctx) {
    return ctx ? ctx->error_msg : NULL;
}

/* query builder */

cq_builder* cq_builder_new(void) {
    cq_builder* b = calloc(1, sizeof(cq_builder));
    return b;
}

void cq_builder_free(cq_builder* b) {
    if (!b) return;
    free(b->select_clause);
    free(b->from_clause);
    free(b->where_clause);
    free(b->having_clause);
    free(b->order_by_clause);
    free(b->limit_clause);
    free(b->offset_clause);
    free(b->join_clause);
    free(b->group_by_clause);
    free(b);
}

void cq_select(cq_builder* b, const char* col) {
    cq_select_as(b, col, NULL);
}

void cq_select_as(cq_builder* b, const char* col, const char* alias) {
    cq_select_expr(b, col);
    if (alias) {
        b->select_clause = append_string(b->select_clause, " AS ");
        b->select_clause = append_string(b->select_clause, alias);
    }
}

void cq_select_expr(cq_builder* b, const char* expr) {
    if (!b || !expr) return;
    if (!b->select_clause) {
        b->select_clause = strdup("SELECT ");
    } else {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, expr);
}

void cq_select_all(cq_builder* b) {
    if (!b) return;
    b->select_clause = strdup("SELECT *");
}

void cq_distinct(cq_builder* b) {
    if (!b) return;
    if (!b->select_clause) {
        b->select_clause = strdup("SELECT DISTINCT ");
    } else {
        // Replace SELECT with SELECT DISTINCT
        char* temp = b->select_clause;
        b->select_clause = malloc(strlen(temp) + 10);
        sprintf(b->select_clause, "SELECT DISTINCT %s", temp + 7); // Skip "SELECT "
        free(temp);
    }
}

void cq_from(cq_builder* b, const char* table) {
    if (!b || !table) return;
    b->from_clause = append_string(b->from_clause, " FROM '");
    b->from_clause = append_string(b->from_clause, table);
    b->from_clause = append_string(b->from_clause, "'");
}

void cq_where_eq(cq_builder* b, const char* col, const char* value) {
    if (!b || !col || !value) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " = '");
    b->where_clause = append_string(b->where_clause, value);
    b->where_clause = append_string(b->where_clause, "'");
}

void cq_where_gt(cq_builder* b, const char* col, const char* value) {
    if (!b || !col || !value) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " > '");
    b->where_clause = append_string(b->where_clause, value);
    b->where_clause = append_string(b->where_clause, "'");
}

void cq_where_lt(cq_builder* b, const char* col, const char* value) {
    if (!b || !col || !value) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " < '");
    b->where_clause = append_string(b->where_clause, value);
    b->where_clause = append_string(b->where_clause, "'");
}

void cq_where_like(cq_builder* b, const char* col, const char* pattern) {
    if (!b || !col || !pattern) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " LIKE '");
    b->where_clause = append_string(b->where_clause, pattern);
    b->where_clause = append_string(b->where_clause, "'");
}

void cq_where_between(cq_builder* b, const char* col, const char* low, const char* high) {
    if (!b || !col || !low || !high) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " BETWEEN '");
    b->where_clause = append_string(b->where_clause, low);
    b->where_clause = append_string(b->where_clause, "' AND '");
    b->where_clause = append_string(b->where_clause, high);
    b->where_clause = append_string(b->where_clause, "'");
}

void cq_where_in(cq_builder* b, const char* col, const char** values, int count) {
    if (!b || !col || !values || count <= 0) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " IN (");
    for (int i = 0; i < count; i++) {
        if (i > 0) b->where_clause = append_string(b->where_clause, ", ");
        b->where_clause = append_string(b->where_clause, "'");
        b->where_clause = append_string(b->where_clause, values[i]);
        b->where_clause = append_string(b->where_clause, "'");
    }
    b->where_clause = append_string(b->where_clause, ")");
}

void cq_where_expr(cq_builder* b, const char* expr) {
    if (!b || !expr) return;
    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, "(");
    b->where_clause = append_string(b->where_clause, expr);
    b->where_clause = append_string(b->where_clause, ")");
}

void cq_and(cq_builder* b) {
    if (!b || !b->where_clause) return;
    b->where_clause = append_string(b->where_clause, " AND ");
}

void cq_or(cq_builder* b) {
    if (!b || !b->where_clause) return;
    b->where_clause = append_string(b->where_clause, " OR ");
}

void cq_order_by(cq_builder* b, const char* col, int desc) {
    if (!b || !col) return;
    if (!b->order_by_clause) {
        b->order_by_clause = strdup(" ORDER BY ");
    } else {
        b->order_by_clause = append_string(b->order_by_clause, ", ");
    }
    b->order_by_clause = append_string(b->order_by_clause, col);
    if (desc) {
        b->order_by_clause = append_string(b->order_by_clause, " DESC");
    }
}

void cq_limit(cq_builder* b, int n) {
    if (!b) return;
    char buf[32];
    sprintf(buf, " LIMIT %d", n);
    b->limit_clause = append_string(b->limit_clause, buf);
}

void cq_offset(cq_builder* b, int n) {
    if (!b) return;
    char buf[32];
    sprintf(buf, " OFFSET %d", n);
    b->offset_clause = append_string(b->offset_clause, buf);
}

// joins
void cq_inner_join(cq_builder* b, const char* table, const char* on_left, const char* on_right) {
    if (!b || !table || !on_left || !on_right) return;
    b->join_clause = append_string(b->join_clause, " INNER JOIN '");
    b->join_clause = append_string(b->join_clause, table);
    b->join_clause = append_string(b->join_clause, "' ON ");
    b->join_clause = append_string(b->join_clause, on_left);
    b->join_clause = append_string(b->join_clause, " = ");
    b->join_clause = append_string(b->join_clause, on_right);
}

void cq_left_join(cq_builder* b, const char* table, const char* on_left, const char* on_right) {
    if (!b || !table || !on_left || !on_right) return;
    b->join_clause = append_string(b->join_clause, " LEFT JOIN '");
    b->join_clause = append_string(b->join_clause, table);
    b->join_clause = append_string(b->join_clause, "' ON ");
    b->join_clause = append_string(b->join_clause, on_left);
    b->join_clause = append_string(b->join_clause, " = ");
    b->join_clause = append_string(b->join_clause, on_right);
}

void cq_right_join(cq_builder* b, const char* table, const char* on_left, const char* on_right) {
    if (!b || !table || !on_left || !on_right) return;
    b->join_clause = append_string(b->join_clause, " RIGHT JOIN '");
    b->join_clause = append_string(b->join_clause, table);
    b->join_clause = append_string(b->join_clause, "' ON ");
    b->join_clause = append_string(b->join_clause, on_left);
    b->join_clause = append_string(b->join_clause, " = ");
    b->join_clause = append_string(b->join_clause, on_right);
}

void cq_full_join(cq_builder* b, const char* table, const char* on_left, const char* on_right) {
    if (!b || !table || !on_left || !on_right) return;
    b->join_clause = append_string(b->join_clause, " FULL JOIN '");
    b->join_clause = append_string(b->join_clause, table);
    b->join_clause = append_string(b->join_clause, "' ON ");
    b->join_clause = append_string(b->join_clause, on_left);
    b->join_clause = append_string(b->join_clause, " = ");
    b->join_clause = append_string(b->join_clause, on_right);
}

// advanced: direct AST building
void cq_add_join(cq_builder* b, const char* table, const char* on_left, const char* on_right) {
    // simple INNER JOIN
    if (!b || !table || !on_left || !on_right) return;
    b->join_clause = append_string(b->join_clause, " JOIN ");
    b->join_clause = append_string(b->join_clause, table);
    b->join_clause = append_string(b->join_clause, " ON ");
    b->join_clause = append_string(b->join_clause, on_left);
    b->join_clause = append_string(b->join_clause, " = ");
    b->join_clause = append_string(b->join_clause, on_right);
}

void cq_group_by(cq_builder* b, const char* col) {
    if (!b || !col) return;
    if (!b->group_by_clause) {
        b->group_by_clause = strdup(" GROUP BY ");
    } else {
        b->group_by_clause = append_string(b->group_by_clause, ", ");
    }
    b->group_by_clause = append_string(b->group_by_clause, col);
}

void cq_having_eq(cq_builder* b, const char* col, const char* value) {
    if (!b || !col || !value) return;
    if (!b->having_clause) {
        b->having_clause = strdup(" HAVING ");
    } else {
        b->having_clause = append_string(b->having_clause, " AND ");
    }
    b->having_clause = append_string(b->having_clause, col);
    b->having_clause = append_string(b->having_clause, " = '");
    b->having_clause = append_string(b->having_clause, value);
    b->having_clause = append_string(b->having_clause, "'");
}

void cq_having_gt(cq_builder* b, const char* col, const char* value) {
    if (!b || !col || !value) return;
    if (!b->having_clause) {
        b->having_clause = strdup(" HAVING ");
    } else {
        b->having_clause = append_string(b->having_clause, " AND ");
    }
    b->having_clause = append_string(b->having_clause, col);
    b->having_clause = append_string(b->having_clause, " > '");
    b->having_clause = append_string(b->having_clause, value);
    b->having_clause = append_string(b->having_clause, "'");
}

void cq_add_aggregate(cq_builder* b, const char* func, const char* col) {
    cq_count(b, col); // Placeholder, implement properly
}

// CASE expressions
void cq_case(cq_builder* b, const char* when_condition, const char* then_value, const char* else_value) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "CASE WHEN ");
    b->select_clause = append_string(b->select_clause, when_condition);
    b->select_clause = append_string(b->select_clause, " THEN ");
    b->select_clause = append_string(b->select_clause, then_value);
    if (else_value) {
        b->select_clause = append_string(b->select_clause, " ELSE ");
        b->select_clause = append_string(b->select_clause, else_value);
    }
    b->select_clause = append_string(b->select_clause, " END");
}

// subqueries
void cq_where_in_subquery(cq_builder* b, const char* col, cq_builder* sub_b) {
    if (!b || !col || !sub_b) return;
    char* sub_sql = cq_builder_to_sql(sub_b);
    if (!sub_sql) return;

    if (!b->where_clause) {
        b->where_clause = strdup(" WHERE ");
    } else {
        b->where_clause = append_string(b->where_clause, " AND ");
    }
    b->where_clause = append_string(b->where_clause, col);
    b->where_clause = append_string(b->where_clause, " IN (");
    b->where_clause = append_string(b->where_clause, sub_sql);
    b->where_clause = append_string(b->where_clause, ")");

    free(sub_sql);
}

// date functions
void cq_date_year(cq_builder* b, const char* date_col) {
    cq_function(b, "YEAR", date_col, NULL);
}

void cq_date_month(cq_builder* b, const char* date_col) {
    cq_function(b, "MONTH", date_col, NULL);
}

void cq_date_day(cq_builder* b, const char* date_col) {
    cq_function(b, "DAY", date_col, NULL);
}

void cq_current_date(cq_builder* b) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "CURRENT_DATE");
}

// scalar functions
void cq_function(cq_builder* b, const char* func_name, const char* arg1, const char* arg2) {
    if (!b || !func_name) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, func_name);
    b->select_clause = append_string(b->select_clause, "(");
    if (arg1) {
        b->select_clause = append_string(b->select_clause, arg1);
        if (arg2) {
            b->select_clause = append_string(b->select_clause, ", ");
            b->select_clause = append_string(b->select_clause, arg2);
        }
    }
    b->select_clause = append_string(b->select_clause, ")");
}

// set operations (append to existing SQL)
void cq_union(cq_builder* b, cq_builder* other_b) {
    if (!b || !other_b) return;
    char* other_sql = cq_builder_to_sql(other_b);
    if (!other_sql) return;
    b->select_clause = append_string(b->select_clause, " UNION ");
    b->select_clause = append_string(b->select_clause, other_sql);
    free(other_sql);
}

void cq_union_all(cq_builder* b, cq_builder* other_b) {
    if (!b || !other_b) return;
    char* other_sql = cq_builder_to_sql(other_b);
    if (!other_sql) return;
    b->select_clause = append_string(b->select_clause, " UNION ALL ");
    b->select_clause = append_string(b->select_clause, other_sql);
    free(other_sql);
}

void cq_intersect(cq_builder* b, cq_builder* other_b) {
    if (!b || !other_b) return;
    char* other_sql = cq_builder_to_sql(other_b);
    if (!other_sql) return;
    b->select_clause = append_string(b->select_clause, " INTERSECT ");
    b->select_clause = append_string(b->select_clause, other_sql);
    free(other_sql);
}

void cq_except(cq_builder* b, cq_builder* other_b) {
    if (!b || !other_b) return;
    char* other_sql = cq_builder_to_sql(other_b);
    if (!other_sql) return;
    b->select_clause = append_string(b->select_clause, " EXCEPT ");
    b->select_clause = append_string(b->select_clause, other_sql);
    free(other_sql);
}

// NOT for conditions
void cq_not(cq_builder* b) {
    if (!b || !b->where_clause) return;
    // insert NOT before the last condition
    // simple: prepend "NOT (" and append ")"
    char* temp = b->where_clause;
    b->where_clause = malloc(strlen(temp) + 8);
    sprintf(b->where_clause, " WHERE NOT (%s)", temp + 7); // Skip " WHERE "
    free(temp);
}

// aggregates
void cq_count(cq_builder* b, const char* col) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "COUNT(");
    if (col) b->select_clause = append_string(b->select_clause, col);
    else b->select_clause = append_string(b->select_clause, "*");
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_sum(cq_builder* b, const char* col) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "SUM(");
    b->select_clause = append_string(b->select_clause, col);
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_avg(cq_builder* b, const char* col) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "AVG(");
    b->select_clause = append_string(b->select_clause, col);
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_min(cq_builder* b, const char* col) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "MIN(");
    b->select_clause = append_string(b->select_clause, col);
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_max(cq_builder* b, const char* col) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "MAX(");
    b->select_clause = append_string(b->select_clause, col);
    b->select_clause = append_string(b->select_clause, ")");
}

// window functions
void cq_window_sum(cq_builder* b, const char* col, const char* partition_by, const char* order_by) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "SUM(");
    b->select_clause = append_string(b->select_clause, col);
    b->select_clause = append_string(b->select_clause, ") OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_window_row_number(cq_builder* b, const char* partition_by, const char* order_by) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "ROW_NUMBER() OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_window_rank(cq_builder* b, const char* partition_by, const char* order_by) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "RANK() OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_window_dense_rank(cq_builder* b, const char* partition_by, const char* order_by) {
    if (!b) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "DENSE_RANK() OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_window_lag(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "LAG(");
    b->select_clause = append_string(b->select_clause, col);
    if (offset != 1) {
        char buf[32];
        sprintf(buf, ", %d", offset);
        b->select_clause = append_string(b->select_clause, buf);
    }
    b->select_clause = append_string(b->select_clause, ") OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

void cq_window_lead(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by) {
    if (!b || !col) return;
    if (!b->select_clause || strcmp(b->select_clause, "SELECT *") == 0) {
        b->select_clause = strdup("SELECT ");
    }
    if (strlen(b->select_clause) > 7) {
        b->select_clause = append_string(b->select_clause, ", ");
    }
    b->select_clause = append_string(b->select_clause, "LEAD(");
    b->select_clause = append_string(b->select_clause, col);
    if (offset != 1) {
        char buf[32];
        sprintf(buf, ", %d", offset);
        b->select_clause = append_string(b->select_clause, buf);
    }
    b->select_clause = append_string(b->select_clause, ") OVER (");
    if (partition_by) {
        b->select_clause = append_string(b->select_clause, "PARTITION BY ");
        b->select_clause = append_string(b->select_clause, partition_by);
        if (order_by) b->select_clause = append_string(b->select_clause, " ");
    }
    if (order_by) {
        b->select_clause = append_string(b->select_clause, "ORDER BY ");
        b->select_clause = append_string(b->select_clause, order_by);
    }
    b->select_clause = append_string(b->select_clause, ")");
}

/* execute */

cq_result* cq_builder_execute(cq_context* ctx, cq_builder* b) {
    char* sql = cq_builder_to_sql(b);
    if (!sql) return NULL;
    cq_result* res = cq_execute(ctx, sql);
    free(sql);
    return res;
}

char* cq_builder_to_sql(cq_builder* b) {
    if (!b || !b->select_clause || !b->from_clause) return NULL;

    size_t len = strlen(b->select_clause) + strlen(b->from_clause) + 1;
    if (b->where_clause) len += strlen(b->where_clause);
    if (b->order_by_clause) len += strlen(b->order_by_clause);
    if (b->limit_clause) len += strlen(b->limit_clause);
    if (b->offset_clause) len += strlen(b->offset_clause);
    if (b->join_clause) len += strlen(b->join_clause);
    if (b->group_by_clause) len += strlen(b->group_by_clause);

    char* sql = malloc(len + 1);
    if (!sql) return NULL;

    strcpy(sql, b->select_clause);
    strcat(sql, b->from_clause);
    if (b->join_clause) strcat(sql, b->join_clause);
    if (b->where_clause) strcat(sql, b->where_clause);
    if (b->group_by_clause) strcat(sql, b->group_by_clause);
    if (b->having_clause) strcat(sql, b->having_clause);
    if (b->order_by_clause) strcat(sql, b->order_by_clause);
    if (b->limit_clause) strcat(sql, b->limit_clause);
    if (b->offset_clause) strcat(sql, b->offset_clause);

    return sql;
}