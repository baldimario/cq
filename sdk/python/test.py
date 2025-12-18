#!/usr/bin/env python3
import os
from libcq.libcq import CQContext, CQBuilder

# test the API
ctx = CQContext()

data_path = os.path.join(os.path.dirname(__file__), '../../data/test_data.csv')

# execute a simple query
result = ctx.execute(f"SELECT name FROM '{data_path}' LIMIT 5")

if result:
    print(f"Rows: {result.rows}, Cols: {result.cols}")
    for i in range(min(5, result.rows)):
        print(f"Row {i}: {result.get(i, 0)}")
else:
    print(f"Error: {ctx.error}")

# test builder with new features
builder = CQBuilder()
builder.select('name')
builder.from_(data_path)
builder.where_like('name', 'A%')
builder.order_by('age', desc=True)
builder.limit(3)

sql = builder.to_sql()
print(f"Generated SQL: {sql}")

result2 = builder.execute(ctx)
if result2:
    print(f"Builder result: Rows {result2.rows}")
else:
    print(f"Builder error: {ctx.error}")

# test aggregates
builder2 = CQBuilder()
builder2.count('name')
builder2.sum('age')
builder2.from_(data_path)

sql2 = builder2.to_sql()
print(f"Aggregate SQL: {sql2}")

result3 = builder2.execute(ctx)
if result3:
    print(f"Aggregate result: Cols {result3.cols}")
    for j in range(result3.cols):
        print(f"  {result3.column_name(j)}: {result3.get(0, j)}")
else:
    print(f"Aggregate error: {ctx.error}")

# test alias and window
builder3 = CQBuilder()
builder3.select_as('name', 'full_name')
builder3.window_row_number(None, 'name')
builder3.from_(data_path)
builder3.limit(3)

sql3 = builder3.to_sql()
print(f"Alias/Window SQL: {sql3}")

result4 = builder3.execute(ctx)
if result4:
    print(f"Window result: Rows {result4.rows}")
    for i in range(result4.rows):
        print(f"  {result4.get(i, 0)}: {result4.get(i, 1)}")
else:
    print(f"Window error: {ctx.error}")

# test CASE
builder4 = CQBuilder()
builder4.case("age > 30", "'Senior'", "'Junior'")
builder4.select_as('name', 'person')
builder4.from_(data_path)
builder4.limit(2)

sql4 = builder4.to_sql()
print(f"CASE SQL: {sql4}")

result5 = builder4.execute(ctx)
if result5:
    print(f"CASE result: Rows {result5.rows}")
    for i in range(result5.rows):
        print(f"  {result5.get(i, 0)}: {result5.get(i, 1)}")
else:
    print(f"CASE error: {ctx.error}")

# test subquery
sub_builder = CQBuilder()
sub_builder.select('name')
sub_builder.from_(data_path)
sub_builder.where_like('name', 'A%')

builder5 = CQBuilder()
builder5.select('name')
builder5.from_(data_path)
builder5.where_in_subquery('name', sub_builder)

sql5 = builder5.to_sql()
print(f"Subquery SQL: {sql5}")

result6 = builder5.execute(ctx)
if result6:
    print(f"Subquery result: Rows {result6.rows}")
else:
    print(f"Subquery error: {ctx.error}")

# test window rank and date function
builder6 = CQBuilder()
builder6.select('name')
builder6.window_rank(None, 'name')
builder6.from_(data_path)
builder6.limit(3)

sql6 = builder6.to_sql()
print(f"Rank SQL: {sql6}")

result7 = builder6.execute(ctx)
if result7:
    print(f"Rank result: Rows {result7.rows}")
    for i in range(result7.rows):
        print(f"  {result7.get(i, 0)}: {result7.get(i, 1)}")
else:
    print(f"Rank error: {ctx.error}")

# test function
builder7 = CQBuilder()
builder7.function('UPPER', 'name')
builder7.from_(data_path)
builder7.limit(2)

sql7 = builder7.to_sql()
print(f"Function SQL: {sql7}")

result8 = builder7.execute(ctx)
if result8:
    print(f"Function result: Rows {result8.rows}")
    for i in range(result8.rows):
        print(f"  {result8.get(i, 0)}")
else:
    print(f"Function error: {ctx.error}")

# test arithmetic expressions
builder9 = CQBuilder()
builder9.select_expr('age * 2')
builder9.select_expr('age + 10 AS adjusted_age')
builder9.from_(data_path)
builder9.where_expr('age / 2 > 15')
builder9.limit(3)

sql9 = builder9.to_sql()
print(f"Arithmetic SQL: {sql9}")

result9 = builder9.execute(ctx)
if result9:
    print(f"Arithmetic result: Rows {result9.rows}, Cols {result9.cols}")
    for i in range(result9.rows):
        print(f"  Row {i}: {result9.get(i, 0)}, {result9.get(i, 1)}")
else:
    print(f"Arithmetic error: {ctx.error}")