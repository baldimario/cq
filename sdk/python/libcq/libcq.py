import os
from ctypes import CDLL, c_int, c_char_p, c_double, c_long, c_void_p, Structure, POINTER

# load the shared library
_lib_path = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'build', 'libcq.so')
_lib = CDLL(_lib_path)

# opaque structures
cq_context = c_void_p
cq_result = c_void_p
cq_builder = c_void_p

# function signatures

# initialization
_lib.cq_init.restype = cq_context
_lib.cq_init.argtypes = []

_lib.cq_free.argtypes = [cq_context]

# execute queries
_lib.cq_execute.restype = cq_result
_lib.cq_execute.argtypes = [cq_context, c_char_p]

_lib.cq_execute_file.restype = c_int
_lib.cq_execute_file.argtypes = [cq_context, c_char_p, c_char_p, c_char_p]

# result access
_lib.cq_result_rows.restype = c_int
_lib.cq_result_rows.argtypes = [cq_result]

_lib.cq_result_cols.restype = c_int
_lib.cq_result_cols.argtypes = [cq_result]

_lib.cq_result_column_name.restype = c_char_p
_lib.cq_result_column_name.argtypes = [cq_result, c_int]

_lib.cq_result_get.restype = c_char_p
_lib.cq_result_get.argtypes = [cq_result, c_int, c_int]

_lib.cq_result_get_double.restype = c_double
_lib.cq_result_get_double.argtypes = [cq_result, c_int, c_int]

_lib.cq_result_get_int.restype = c_long
_lib.cq_result_get_int.argtypes = [cq_result, c_int, c_int]

_lib.cq_result_is_null.restype = c_int
_lib.cq_result_is_null.argtypes = [cq_result, c_int, c_int]

_lib.cq_result_free.argtypes = [cq_result]

# error handling
_lib.cq_errno.restype = c_int
_lib.cq_errno.argtypes = [cq_context]

_lib.cq_error.restype = c_char_p
_lib.cq_error.argtypes = [cq_context]

# query builder
_lib.cq_builder_new.restype = cq_builder
_lib.cq_builder_new.argtypes = []

_lib.cq_builder_free.argtypes = [cq_builder]

_lib.cq_select.argtypes = [cq_builder, c_char_p]
_lib.cq_select_all.argtypes = [cq_builder]
_lib.cq_distinct.argtypes = [cq_builder]
_lib.cq_from.argtypes = [cq_builder, c_char_p]
_lib.cq_where_eq.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_where_gt.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_where_lt.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_where_like.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_where_between.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_where_in.argtypes = [cq_builder, c_char_p, POINTER(c_char_p), c_int]
_lib.cq_where_expr.argtypes = [cq_builder, c_char_p]
_lib.cq_and.argtypes = [cq_builder]
_lib.cq_or.argtypes = [cq_builder]
_lib.cq_order_by.argtypes = [cq_builder, c_char_p, c_int]
_lib.cq_limit.argtypes = [cq_builder, c_int]
_lib.cq_offset.argtypes = [cq_builder, c_int]
_lib.cq_inner_join.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_left_join.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_right_join.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_full_join.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_group_by.argtypes = [cq_builder, c_char_p]
_lib.cq_having_eq.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_having_gt.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_count.argtypes = [cq_builder, c_char_p]
_lib.cq_sum.argtypes = [cq_builder, c_char_p]
_lib.cq_avg.argtypes = [cq_builder, c_char_p]
_lib.cq_min.argtypes = [cq_builder, c_char_p]
_lib.cq_max.argtypes = [cq_builder, c_char_p]
_lib.cq_select_as.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_select_expr.argtypes = [cq_builder, c_char_p]
_lib.cq_window_sum.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_window_row_number.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_window_rank.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_window_dense_rank.argtypes = [cq_builder, c_char_p, c_char_p]
_lib.cq_window_lag.argtypes = [cq_builder, c_char_p, c_int, c_char_p, c_char_p]
_lib.cq_window_lead.argtypes = [cq_builder, c_char_p, c_int, c_char_p, c_char_p]
_lib.cq_case.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_where_in_subquery.argtypes = [cq_builder, c_char_p, cq_builder]
_lib.cq_date_year.argtypes = [cq_builder, c_char_p]
_lib.cq_date_month.argtypes = [cq_builder, c_char_p]
_lib.cq_date_day.argtypes = [cq_builder, c_char_p]
_lib.cq_current_date.argtypes = [cq_builder]
_lib.cq_function.argtypes = [cq_builder, c_char_p, c_char_p, c_char_p]
_lib.cq_union.argtypes = [cq_builder, cq_builder]
_lib.cq_union_all.argtypes = [cq_builder, cq_builder]
_lib.cq_intersect.argtypes = [cq_builder, cq_builder]
_lib.cq_except.argtypes = [cq_builder, cq_builder]
_lib.cq_not.argtypes = [cq_builder]

_lib.cq_builder_execute.restype = cq_result
_lib.cq_builder_execute.argtypes = [cq_context, cq_builder]

_lib.cq_builder_to_sql.restype = c_char_p
_lib.cq_builder_to_sql.argtypes = [cq_builder]

# python wrapper classes

class CQContext:
    def __init__(self):
        self.ctx = _lib.cq_init()

    def __del__(self):
        if self.ctx:
            _lib.cq_free(self.ctx)

    def execute(self, sql):
        return CQResult(_lib.cq_execute(self.ctx, sql.encode('utf-8')))

    def execute_file(self, csv_path, sql, output_path):
        return _lib.cq_execute_file(self.ctx, csv_path.encode('utf-8'), sql.encode('utf-8'), output_path.encode('utf-8')) == 1

    @property
    def errno(self):
        return _lib.cq_errno(self.ctx)

    @property
    def error(self):
        err = _lib.cq_error(self.ctx)
        return err.decode('utf-8') if err else None

class CQResult:
    def __init__(self, res):
        self.res = res

    def __del__(self):
        if self.res:
            _lib.cq_result_free(self.res)

    @property
    def rows(self):
        return _lib.cq_result_rows(self.res)

    @property
    def cols(self):
        return _lib.cq_result_cols(self.res)

    def column_name(self, col):
        name = _lib.cq_result_column_name(self.res, col)
        return name.decode('utf-8') if name else None

    def get(self, row, col):
        val = _lib.cq_result_get(self.res, row, col)
        return val.decode('utf-8') if val else None

    def get_double(self, row, col):
        return _lib.cq_result_get_double(self.res, row, col)

    def get_int(self, row, col):
        return _lib.cq_result_get_int(self.res, row, col)

    def is_null(self, row, col):
        return _lib.cq_result_is_null(self.res, row, col) != 0

class CQBuilder:
    def __init__(self):
        self.builder = _lib.cq_builder_new()

    def __del__(self):
        if self.builder:
            _lib.cq_builder_free(self.builder)

    def select(self, col, alias=None):
        if alias:
            _lib.cq_select_as(self.builder, col.encode('utf-8'), alias.encode('utf-8'))
        else:
            _lib.cq_select(self.builder, col.encode('utf-8'))

    def select_all(self):
        _lib.cq_select_all(self.builder)

    def from_(self, table):
        _lib.cq_from(self.builder, table.encode('utf-8'))

    def where_eq(self, col, value):
        _lib.cq_where_eq(self.builder, col.encode('utf-8'), value.encode('utf-8'))

    def where_gt(self, col, value):
        _lib.cq_where_gt(self.builder, col.encode('utf-8'), value.encode('utf-8'))

    def where_lt(self, col, value):
        _lib.cq_where_lt(self.builder, col.encode('utf-8'), value.encode('utf-8'))

    def and_(self):
        _lib.cq_and(self.builder)

    def or_(self):
        _lib.cq_or(self.builder)

    def order_by(self, col, desc=False):
        _lib.cq_order_by(self.builder, col.encode('utf-8'), 1 if desc else 0)

    def limit(self, n):
        _lib.cq_limit(self.builder, n)

    def offset(self, n):
        _lib.cq_offset(self.builder, n)

    def add_join(self, table, on_left, on_right):
        _lib.cq_add_join(self.builder, table.encode('utf-8'), on_left.encode('utf-8'), on_right.encode('utf-8'))

    def distinct(self):
        _lib.cq_distinct(self.builder)

    def where_like(self, col, pattern):
        _lib.cq_where_like(self.builder, col.encode('utf-8'), pattern.encode('utf-8'))

    def where_between(self, col, low, high):
        _lib.cq_where_between(self.builder, col.encode('utf-8'), low.encode('utf-8'), high.encode('utf-8'))

    def where_in(self, col, values):
        arr = (c_char_p * len(values))(*[v.encode('utf-8') for v in values])
        _lib.cq_where_in(self.builder, col.encode('utf-8'), arr, len(values))

    def where_expr(self, expr):
        _lib.cq_where_expr(self.builder, expr.encode('utf-8'))

    def inner_join(self, table, on_left, on_right):
        _lib.cq_inner_join(self.builder, table.encode('utf-8'), on_left.encode('utf-8'), on_right.encode('utf-8'))

    def left_join(self, table, on_left, on_right):
        _lib.cq_left_join(self.builder, table.encode('utf-8'), on_left.encode('utf-8'), on_right.encode('utf-8'))

    def right_join(self, table, on_left, on_right):
        _lib.cq_right_join(self.builder, table.encode('utf-8'), on_left.encode('utf-8'), on_right.encode('utf-8'))

    def full_join(self, table, on_left, on_right):
        _lib.cq_full_join(self.builder, table.encode('utf-8'), on_left.encode('utf-8'), on_right.encode('utf-8'))

    def group_by(self, col):
        _lib.cq_group_by(self.builder, col.encode('utf-8'))

    def having_eq(self, col, value):
        _lib.cq_having_eq(self.builder, col.encode('utf-8'), value.encode('utf-8'))

    def having_gt(self, col, value):
        _lib.cq_having_gt(self.builder, col.encode('utf-8'), value.encode('utf-8'))

    def count(self, col=None):
        _lib.cq_count(self.builder, col.encode('utf-8') if col else None)

    def sum(self, col):
        _lib.cq_sum(self.builder, col.encode('utf-8'))

    def avg(self, col):
        _lib.cq_avg(self.builder, col.encode('utf-8'))

    def min(self, col):
        _lib.cq_min(self.builder, col.encode('utf-8'))

    def max(self, col):
        _lib.cq_max(self.builder, col.encode('utf-8'))

    def select_as(self, col, alias):
        _lib.cq_select_as(self.builder, col.encode('utf-8'), alias.encode('utf-8'))

    def select_expr(self, expr):
        _lib.cq_select_expr(self.builder, expr.encode('utf-8'))

    def window_sum(self, col, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_sum(self.builder, col.encode('utf-8'), p, o)

    def window_row_number(self, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_row_number(self.builder, p, o)

    def window_rank(self, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_rank(self.builder, p, o)

    def window_dense_rank(self, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_dense_rank(self.builder, p, o)

    def window_lag(self, col, offset=1, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_lag(self.builder, col.encode('utf-8'), offset, p, o)

    def window_lead(self, col, offset=1, partition_by=None, order_by=None):
        p = partition_by.encode('utf-8') if partition_by else None
        o = order_by.encode('utf-8') if order_by else None
        _lib.cq_window_lead(self.builder, col.encode('utf-8'), offset, p, o)

    def case(self, when_condition, then_value, else_value=None):
        e = else_value.encode('utf-8') if else_value else None
        _lib.cq_case(self.builder, when_condition.encode('utf-8'), then_value.encode('utf-8'), e)

    def where_in_subquery(self, col, sub_builder):
        _lib.cq_where_in_subquery(self.builder, col.encode('utf-8'), sub_builder.builder)

    def date_year(self, date_col):
        _lib.cq_date_year(self.builder, date_col.encode('utf-8'))

    def date_month(self, date_col):
        _lib.cq_date_month(self.builder, date_col.encode('utf-8'))

    def date_day(self, date_col):
        _lib.cq_date_day(self.builder, date_col.encode('utf-8'))

    def current_date(self):
        _lib.cq_current_date(self.builder)

    def function(self, func_name, arg1, arg2=None):
        a2 = arg2.encode('utf-8') if arg2 else None
        _lib.cq_function(self.builder, func_name.encode('utf-8'), arg1.encode('utf-8'), a2)

    def union(self, other_builder):
        _lib.cq_union(self.builder, other_builder.builder)

    def union_all(self, other_builder):
        _lib.cq_union_all(self.builder, other_builder.builder)

    def intersect(self, other_builder):
        _lib.cq_intersect(self.builder, other_builder.builder)

    def except_(self, other_builder):
        _lib.cq_except(self.builder, other_builder.builder)

    def not_(self):
        _lib.cq_not(self.builder)

    def add_aggregate(self, func, col):
        _lib.cq_add_aggregate(self.builder, func.encode('utf-8'), col.encode('utf-8'))

    def execute(self, ctx):
        return CQResult(_lib.cq_builder_execute(ctx.ctx, self.builder))

    def to_sql(self):
        sql = _lib.cq_builder_to_sql(self.builder)
        return sql.decode('utf-8') if sql else None