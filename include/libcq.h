/* high level api */

/* query api */
typedef struct cq_context cq_context;
typedef struct cq_result cq_result;

// initialization
cq_context* cq_init(void);
void cq_free(cq_context* ctx);

// execute queries
cq_result* cq_execute(cq_context* ctx, const char* sql);
int cq_execute_file(cq_context* ctx, const char* csv_path,
const char* sql, const char* output_path);

// result access
int cq_result_rows(cq_result* res);
int cq_result_cols(cq_result* res);
const char* cq_result_column_name(cq_result* res, int col);
const char* cq_result_get(cq_result* res, int row, int col);
double cq_result_get_double(cq_result* res, int row, int col);
long cq_result_get_int(cq_result* res, int row, int col);
int cq_result_is_null(cq_result* res, int row, int col);
void cq_result_free(cq_result* res);
  
// error handling
int cq_errno(cq_context* ctx);
const char* cq_error(cq_context* ctx);

/* query builder */

typedef struct cq_builder cq_builder;

// builder creation
cq_builder* cq_builder_new(void);
void cq_builder_free(cq_builder* b);

// build query
void cq_select(cq_builder* b, const char* col);
void cq_select_all(cq_builder* b);
void cq_distinct(cq_builder* b);
void cq_from(cq_builder* b, const char* table);
void cq_where_eq(cq_builder* b, const char* col, const char* value);
void cq_where_gt(cq_builder* b, const char* col, const char* value);
void cq_where_lt(cq_builder* b, const char* col, const char* value);
void cq_where_like(cq_builder* b, const char* col, const char* pattern);
void cq_where_between(cq_builder* b, const char* col, const char* low, const char* high);
void cq_where_in(cq_builder* b, const char* col, const char** values, int count);
void cq_where_expr(cq_builder* b, const char* expr);
void cq_and(cq_builder* b);
void cq_or(cq_builder* b);
void cq_order_by(cq_builder* b, const char* col, int desc);
void cq_limit(cq_builder* b, int n);
void cq_offset(cq_builder* b, int n);

// joins
void cq_inner_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_left_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_right_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_full_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);

// group by and having
void cq_group_by(cq_builder* b, const char* col);
void cq_having_eq(cq_builder* b, const char* col, const char* value);
void cq_having_gt(cq_builder* b, const char* col, const char* value);

// aggregates
void cq_count(cq_builder* b, const char* col);
void cq_sum(cq_builder* b, const char* col);
void cq_avg(cq_builder* b, const char* col);
void cq_min(cq_builder* b, const char* col);
void cq_max(cq_builder* b, const char* col);

// aliases and expressions
void cq_select_as(cq_builder* b, const char* col, const char* alias);
void cq_select_expr(cq_builder* b, const char* expr);

// window functions
void cq_window_sum(cq_builder* b, const char* col, const char* partition_by, const char* order_by);
void cq_window_row_number(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_rank(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_dense_rank(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_lag(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by);
void cq_window_lead(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by);

// CASE expressions
void cq_case(cq_builder* b, const char* when_condition, const char* then_value, const char* else_value);

// subqueries
void cq_where_in_subquery(cq_builder* b, const char* col, cq_builder* sub_b);

// date functions
void cq_date_year(cq_builder* b, const char* date_col);
void cq_date_month(cq_builder* b, const char* date_col);
void cq_date_day(cq_builder* b, const char* date_col);
void cq_current_date(cq_builder* b);

// scalar functions
void cq_function(cq_builder* b, const char* func_name, const char* arg1, const char* arg2);

// set operations
void cq_union(cq_builder* b, cq_builder* other_b);
void cq_union_all(cq_builder* b, cq_builder* other_b);
void cq_intersect(cq_builder* b, cq_builder* other_b);
void cq_except(cq_builder* b, cq_builder* other_b);

// NOT for conditions
void cq_not(cq_builder* b);

// execute
cq_result* cq_builder_execute(cq_context* ctx, cq_builder* b);
char* cq_builder_to_sql(cq_builder* b); // Debug: get SQL string