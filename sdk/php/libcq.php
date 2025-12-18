<?php

// define FFI signatures manually
$ffi = FFI::cdef("
typedef struct cq_context cq_context;
typedef struct cq_result cq_result;
typedef struct cq_builder cq_builder;

cq_context* cq_init(void);
void cq_free(cq_context* ctx);
cq_result* cq_execute(cq_context* ctx, const char* sql);
int cq_execute_file(cq_context* ctx, const char* csv_path, const char* sql, const char* output_path);
int cq_result_rows(cq_result* res);
int cq_result_cols(cq_result* res);
const char* cq_result_column_name(cq_result* res, int col);
const char* cq_result_get(cq_result* res, int row, int col);
double cq_result_get_double(cq_result* res, int row, int col);
long cq_result_get_int(cq_result* res, int row, int col);
int cq_result_is_null(cq_result* res, int row, int col);
void cq_result_free(cq_result* res);
int cq_errno(cq_context* ctx);
const char* cq_error(cq_context* ctx);
cq_builder* cq_builder_new(void);
void cq_builder_free(cq_builder* b);
void cq_select(cq_builder* b, const char* col);
void cq_select_all(cq_builder* b);
void cq_select_as(cq_builder* b, const char* col, const char* alias);
void cq_select_expr(cq_builder* b, const char* expr);
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
void cq_inner_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_left_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_right_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_full_join(cq_builder* b, const char* table, const char* on_left, const char* on_right);
void cq_group_by(cq_builder* b, const char* col);
void cq_having_eq(cq_builder* b, const char* col, const char* value);
void cq_having_gt(cq_builder* b, const char* col, const char* value);
void cq_count(cq_builder* b, const char* col);
void cq_sum(cq_builder* b, const char* col);
void cq_avg(cq_builder* b, const char* col);
void cq_min(cq_builder* b, const char* col);
void cq_max(cq_builder* b, const char* col);
void cq_window_sum(cq_builder* b, const char* col, const char* partition_by, const char* order_by);
void cq_window_row_number(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_rank(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_dense_rank(cq_builder* b, const char* partition_by, const char* order_by);
void cq_window_lag(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by);
void cq_window_lead(cq_builder* b, const char* col, int offset, const char* partition_by, const char* order_by);
void cq_case(cq_builder* b, const char* when_condition, const char* then_value, const char* else_value);
void cq_where_in_subquery(cq_builder* b, const char* col, cq_builder* sub_b);
void cq_date_year(cq_builder* b, const char* date_col);
void cq_date_month(cq_builder* b, const char* date_col);
void cq_date_day(cq_builder* b, const char* date_col);
void cq_current_date(cq_builder* b);
void cq_function(cq_builder* b, const char* func_name, const char* arg1, const char* arg2);
void cq_union(cq_builder* b, cq_builder* other_b);
void cq_union_all(cq_builder* b, cq_builder* other_b);
void cq_intersect(cq_builder* b, cq_builder* other_b);
void cq_except(cq_builder* b, cq_builder* other_b);
void cq_not(cq_builder* b);
cq_result* cq_builder_execute(cq_context* ctx, cq_builder* b);
const char* cq_builder_to_sql(cq_builder* b);
", __DIR__ . '/../../build/libcq.so');

// CQContext class
class CQContext {
    private $ctx;

    public function __construct() {
        global $ffi;
        $this->ctx = $ffi->cq_init();
    }

    public function __destruct() {
        if ($this->ctx) {
            $GLOBALS['ffi']->cq_free($this->ctx);
        }
    }

    public function execute($sql) {
        $res = $GLOBALS['ffi']->cq_execute($this->ctx, $sql);
        if ($res === null) {
            return null;
        }
        return new CQResult($res);
    }

    public function executeFile($csvPath, $sql, $outputPath) {
        return $GLOBALS['ffi']->cq_execute_file($this->ctx, $csvPath, $sql, $outputPath) === 1;
    }

    public function errno() {
        return $GLOBALS['ffi']->cq_errno($this->ctx);
    }

    public function error() {
        global $ffi;
        $err = $ffi->cq_error($this->ctx);
        return $err ?: null;
    }

    public function getCtx() {
        return $this->ctx;
    }
}

// CQResult class
class CQResult {
    private $res;

    public function __construct($res) {
        $this->res = $res;
    }

    public function __destruct() {
        if ($this->res) {
            $GLOBALS['ffi']->cq_result_free($this->res);
        }
    }

    public function rows() {
        return $GLOBALS['ffi']->cq_result_rows($this->res);
    }

    public function cols() {
        return $GLOBALS['ffi']->cq_result_cols($this->res);
    }

    public function columnName($col) {
        return $GLOBALS['ffi']->cq_result_column_name($this->res, $col);
    }

    public function get($row, $col) {
        return $GLOBALS['ffi']->cq_result_get($this->res, $row, $col);
    }

    public function getDouble($row, $col) {
        return $GLOBALS['ffi']->cq_result_get_double($this->res, $row, $col);
    }

    public function getInt($row, $col) {
        return $GLOBALS['ffi']->cq_result_get_int($this->res, $row, $col);
    }

    public function isNull($row, $col) {
        return $GLOBALS['ffi']->cq_result_is_null($this->res, $row, $col) !== 0;
    }
}

// CQBuilder class
class CQBuilder {
    private $builder;

    public function __construct() {
        $this->builder = $GLOBALS['ffi']->cq_builder_new();
    }

    public function __destruct() {
        if ($this->builder) {
            $GLOBALS['ffi']->cq_builder_free($this->builder);
        }
    }

    public function select($col, $alias = null) {
        if ($alias) {
            $GLOBALS['ffi']->cq_select_as($this->builder, $col, $alias);
        } else {
            $GLOBALS['ffi']->cq_select($this->builder, $col);
        }
    }

    public function selectAll() {
        $GLOBALS['ffi']->cq_select_all($this->builder);
    }

    public function selectExpr($expr) {
        $GLOBALS['ffi']->cq_select_expr($this->builder, $expr);
    }

    public function distinct() {
        $GLOBALS['ffi']->cq_distinct($this->builder);
    }

    public function from($table) {
        $GLOBALS['ffi']->cq_from($this->builder, $table);
    }

    public function whereEq($col, $value) {
        $GLOBALS['ffi']->cq_where_eq($this->builder, $col, $value);
    }

    public function whereGt($col, $value) {
        $GLOBALS['ffi']->cq_where_gt($this->builder, $col, $value);
    }

    public function whereLt($col, $value) {
        $GLOBALS['ffi']->cq_where_lt($this->builder, $col, $value);
    }

    public function whereLike($col, $pattern) {
        $GLOBALS['ffi']->cq_where_like($this->builder, $col, $pattern);
    }

    public function whereBetween($col, $low, $high) {
        $GLOBALS['ffi']->cq_where_between($this->builder, $col, $low, $high);
    }

    public function whereIn($col, $values) {
        // For simplicity, assume string array
        $arr = FFI::new('char*[' . count($values) . ']');
        foreach ($values as $i => $v) {
            $arr[$i] = FFI::new('char[' . (strlen($v) + 1) . ']', false);
            FFI::memcpy($arr[$i], $v, strlen($v));
        }
        $GLOBALS['ffi']->cq_where_in($this->builder, $col, $arr, count($values));
    }

    public function whereExpr($expr) {
        $GLOBALS['ffi']->cq_where_expr($this->builder, $expr);
    }

    public function and_() {
        $GLOBALS['ffi']->cq_and($this->builder);
    }

    public function or_() {
        $GLOBALS['ffi']->cq_or($this->builder);
    }

    public function orderBy($col, $desc = false) {
        $GLOBALS['ffi']->cq_order_by($this->builder, $col, $desc ? 1 : 0);
    }

    public function limit($n) {
        $GLOBALS['ffi']->cq_limit($this->builder, $n);
    }

    public function offset($n) {
        $GLOBALS['ffi']->cq_offset($this->builder, $n);
    }

    public function innerJoin($table, $onLeft, $onRight) {
        $GLOBALS['ffi']->cq_inner_join($this->builder, $table, $onLeft, $onRight);
    }

    public function leftJoin($table, $onLeft, $onRight) {
        $GLOBALS['ffi']->cq_left_join($this->builder, $table, $onLeft, $onRight);
    }

    public function rightJoin($table, $onLeft, $onRight) {
        $GLOBALS['ffi']->cq_right_join($this->builder, $table, $onLeft, $onRight);
    }

    public function fullJoin($table, $onLeft, $onRight) {
        $GLOBALS['ffi']->cq_full_join($this->builder, $table, $onLeft, $onRight);
    }

    public function groupBy($col) {
        $GLOBALS['ffi']->cq_group_by($this->builder, $col);
    }

    public function havingEq($col, $value) {
        $GLOBALS['ffi']->cq_having_eq($this->builder, $col, $value);
    }

    public function havingGt($col, $value) {
        $GLOBALS['ffi']->cq_having_gt($this->builder, $col, $value);
    }

    public function count($col = null) {
        $c = $col ?: null;
        $GLOBALS['ffi']->cq_count($this->builder, $c);
    }

    public function sum($col) {
        $GLOBALS['ffi']->cq_sum($this->builder, $col);
    }

    public function avg($col) {
        $GLOBALS['ffi']->cq_avg($this->builder, $col);
    }

    public function min($col) {
        $GLOBALS['ffi']->cq_min($this->builder, $col);
    }

    public function max($col) {
        $GLOBALS['ffi']->cq_max($this->builder, $col);
    }

    public function windowSum($col, $partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_sum($this->builder, $col, $p, $o);
    }

    public function windowRowNumber($partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_row_number($this->builder, $p, $o);
    }

    public function windowRank($partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_rank($this->builder, $p, $o);
    }

    public function windowDenseRank($partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_dense_rank($this->builder, $p, $o);
    }

    public function windowLag($col, $offset = 1, $partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_lag($this->builder, $col, $offset, $p, $o);
    }

    public function windowLead($col, $offset = 1, $partitionBy = null, $orderBy = null) {
        $p = $partitionBy ?: null;
        $o = $orderBy ?: null;
        $GLOBALS['ffi']->cq_window_lead($this->builder, $col, $offset, $p, $o);
    }

    public function case_($whenCondition, $thenValue, $elseValue = null) {
        $e = $elseValue ?: null;
        $GLOBALS['ffi']->cq_case($this->builder, $whenCondition, $thenValue, $e);
    }

    public function whereInSubquery($col, $subBuilder) {
        $GLOBALS['ffi']->cq_where_in_subquery($this->builder, $col, $subBuilder->builder);
    }

    public function dateYear($dateCol) {
        $GLOBALS['ffi']->cq_date_year($this->builder, $dateCol);
    }

    public function dateMonth($dateCol) {
        $GLOBALS['ffi']->cq_date_month($this->builder, $dateCol);
    }

    public function dateDay($dateCol) {
        $GLOBALS['ffi']->cq_date_day($this->builder, $dateCol);
    }

    public function currentDate() {
        $GLOBALS['ffi']->cq_current_date($this->builder);
    }

    public function function_($funcName, $arg1, $arg2 = null) {
        $a2 = $arg2 ?: null;
        $GLOBALS['ffi']->cq_function($this->builder, $funcName, $arg1, $a2);
    }

    public function union($otherBuilder) {
        $GLOBALS['ffi']->cq_union($this->builder, $otherBuilder->builder);
    }

    public function unionAll($otherBuilder) {
        $GLOBALS['ffi']->cq_union_all($this->builder, $otherBuilder->builder);
    }

    public function intersect($otherBuilder) {
        $GLOBALS['ffi']->cq_intersect($this->builder, $otherBuilder->builder);
    }

    public function except($otherBuilder) {
        $GLOBALS['ffi']->cq_except($this->builder, $otherBuilder->builder);
    }

    public function not_() {
        $GLOBALS['ffi']->cq_not($this->builder);
    }

    public function execute($ctx) {
        global $ffi;
        $res = $ffi->cq_builder_execute($ctx->getCtx(), $this->builder);
        if ($res === null) {
            return null;
        }
        return new CQResult($res);
    }

    public function toSql() {
        return $GLOBALS['ffi']->cq_builder_to_sql($this->builder);
    }
}

?>