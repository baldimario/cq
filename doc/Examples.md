## Examples

The `assets/` directory contains ready-to-run SQL examples:

- **[example_between.sql](assets/example_between.sql)** - BETWEEN operator with SQL comments
- **[example_aggregation.sql](assets/example_aggregation.sql)** - Complex aggregations with GROUP BY and HAVING
- **[example_dml.sql](assets/example_dml.sql)** - INSERT, UPDATE, DELETE operations
- **[example_functions.sql](assets/example_functions.sql)** - String and math functions
- **[example_joins.sql](assets/example_joins.sql)** - JOIN operations between CSV files
- **[example_case.sql](assets/example_case.sql)** - CASE expressions (simple and searched forms)
- **[example_window_functions.sql](assets/example_window_functions.sql)** - Window functions (ROW_NUMBER, RANK, LAG, LEAD, running aggregates)

Run any example:
```bash
cq -f assets/example_between.sql -p
```

## Data Manipulation (INSERT, UPDATE, DELETE)

**Modify CSV files directly:**

```bash
# INSERT - Add new rows
cq -q "INSERT INTO 'data/users.csv' (id, name, age) VALUES (100, 'Mario', 30)"
cq -q "INSERT INTO 'data/users.csv' VALUES (101, 'Luigi', 28, 'user', 175, 1)"

# UPDATE - Modify existing rows
cq -q "UPDATE 'data/users.csv' SET age = 31 WHERE name = 'Mario'"
cq -q "UPDATE 'data/users.csv' SET role = 'admin', active = 1 WHERE age > 25"

# DELETE - Remove rows (WHERE clause required for safety)
cq -q "DELETE FROM 'data/users.csv' WHERE active = 0"
cq -q "DELETE FROM 'data/users.csv' WHERE age < 18"

# DELETE all rows (requires --force flag)
cq -q "DELETE FROM 'data/users.csv'" --force
```

**Notes:**
- All DML operations modify the CSV file in-place
- DELETE requires WHERE clause (safety measure to prevent accidental data loss)
- Use --force flag to allow DELETE without WHERE clause (deletes all rows)
- Use quotes around file paths with special characters: `'data/file.csv'`
- Column names in INSERT are optional if providing all values in order

## CREATE TABLE (Save Query Results)

**Create new CSV files from query results or with specified schema:**

```bash
# CREATE TABLE - Save query results to new file
cq -q "CREATE TABLE 'output.csv' AS SELECT * FROM 'data.csv' WHERE age > 25"

# CREATE TABLE - With filtering and aggregation
cq -q "CREATE TABLE 'summary.csv' AS SELECT role, COUNT(*) as cnt, AVG(age) as avg_age FROM 'users.csv' GROUP BY role"

# CREATE TABLE - Combine multiple files
cq -q "CREATE TABLE 'all_data.csv' AS SELECT * FROM 'data1.csv' UNION ALL SELECT * FROM 'data2.csv'"

# CREATE TABLE - With JOIN
cq -q "CREATE TABLE 'report.csv' AS SELECT u.name, r.role_name FROM 'users.csv' AS u JOIN 'roles.csv' AS r ON u.role_id = r.id"

# CREATE TABLE - Create empty file with schema
cq -q "CREATE TABLE 'new_table.csv' (id, name, age, role)"

# CREATE TABLE - Define schema for file without header
cq -q "CREATE TABLE 'data_no_header.csv' AS (col1, col2, col3)"
```

**Notes:**
- CREATE TABLE AS SELECT materializes query results into a new CSV file
- If file exists, it will be replaced
- Useful for creating derived tables, reports, or intermediate results
- Empty schema creation useful for defining structure before data insertion
- Schema mapping (AS (col1, col2)) creates empty file with specified column names

## ALTER TABLE (Modify CSV Headers)

**Modify the structure of existing CSV files:**

```bash
# ALTER TABLE - Rename a column
cq -q "ALTER TABLE 'users.csv' RENAME COLUMN old_name TO new_name"

# ALTER TABLE - Add a new column (filled with empty values)
cq -q "ALTER TABLE 'users.csv' ADD COLUMN email"

# ALTER TABLE - Drop a column (removes all data in that column)
cq -q "ALTER TABLE 'users.csv' DROP COLUMN middle_name"

# Multiple operations can be chained by running commands sequentially
cq -q "ALTER TABLE 'data.csv' RENAME COLUMN id TO user_id"
cq -q "ALTER TABLE 'data.csv' ADD COLUMN created_at"
cq -q "ALTER TABLE 'data.csv' DROP COLUMN deprecated_field"
```

**Supported Operations:**
- **RENAME COLUMN**: Changes the column name in the header
- **ADD COLUMN**: Adds a new column at the end (existing rows get empty values)
- **DROP COLUMN**: Removes a column and all its data (cannot drop the last column)

**Notes:**
- All operations modify the CSV file in place
- Column name matching is case-insensitive
- Cannot drop the last remaining column
- Cannot add a column that already exists
- ADD COLUMN fills existing rows with empty values
- Original file is overwritten (make backups if needed)

### SQL Comments

Both single-line and multi-line comments are supported:

```sql
-- This is a single-line comment
SELECT name, age FROM 'users.csv' -- inline comment
WHERE age > 20

/* This is a
   multi-line comment */
SELECT * FROM 'data.csv'
WHERE /* inline block comment */ status = 'active'
```

### CASE Expressions

CASE expressions provide conditional logic in SQL queries. Two forms are supported:

**Simple CASE** - Compares an expression against multiple values:
```sql
CASE expression
    WHEN value1 THEN result1
    WHEN value2 THEN result2
    ...
    ELSE default_result
END
```

**Searched CASE** - Evaluates conditions sequentially:
```sql
CASE
    WHEN condition1 THEN result1
    WHEN condition2 THEN result2
    ...
    ELSE default_result
END
```

**Examples:**
```sql
-- Simple CASE: categorize by exact age match
SELECT name, age,
    CASE age
        WHEN 25 THEN 'Quarter century'
        WHEN 30 THEN 'Thirty'
        WHEN 40 THEN 'Forty'
        ELSE 'Other age'
    END AS age_label
FROM users.csv

-- Searched CASE: categorize by age ranges
SELECT name, age,
    CASE
        WHEN age < 18 THEN 'Minor'
        WHEN age >= 18 AND age < 65 THEN 'Adult'
        WHEN age >= 65 THEN 'Senior'
        ELSE 'Unknown'
    END AS age_group
FROM users.csv

-- CASE with numeric results
SELECT name,
    CASE
        WHEN age < 30 THEN 1
        WHEN age < 40 THEN 2
        ELSE 3
    END AS tier
FROM users.csv

-- Nested CASE expressions
SELECT name,
    CASE
        WHEN age < 30 THEN
            CASE
                WHEN age < 20 THEN 'Very young'
                ELSE 'Young'
            END
        ELSE 'Older'
    END AS category
FROM users.csv

-- CASE in WHERE clause
SELECT name, age FROM users.csv
WHERE CASE
    WHEN role = 'admin' THEN 1
    WHEN age > 50 THEN 1
    ELSE 0
END = 1

-- CASE in ORDER BY
SELECT name, age FROM users.csv
ORDER BY CASE
    WHEN age < 30 THEN 3
    WHEN age < 50 THEN 2
    ELSE 1
END, name

-- CASE without ELSE (returns NULL if no match)
SELECT name,
    CASE
        WHEN age > 100 THEN 'Centenarian'
    END AS special_status
FROM users.csv
```

**Notes:**
- If no WHEN condition matches and there's no ELSE clause, the result is NULL
- CASE expressions can be nested
- CASE works in SELECT, WHERE, ORDER BY, and GROUP BY clauses
- Both string and numeric results are supported

**Using CASE Aliases in GROUP BY, ORDER BY, and WHERE:**

CQ supports using SELECT column aliases in GROUP BY, ORDER BY, and WHERE clauses, making queries more readable and avoiding repetition of complex expressions:

```sql
-- GROUP BY with CASE alias
SELECT CASE WHEN age < 30 THEN 'young' ELSE 'old' END AS category,
       COUNT(*) AS cnt
FROM users.csv
GROUP BY category

-- GROUP BY with multiple columns (including aliases)
SELECT role,
       CASE WHEN age < 30 THEN 'junior' ELSE 'senior' END AS level,
       COUNT(*) AS total,
       AVG(age) AS avg_age
FROM users.csv
GROUP BY role, level
ORDER BY total DESC

-- ORDER BY with CASE alias
SELECT name,
       CASE WHEN LENGTH(name) > 5 THEN 'long' ELSE 'short' END AS name_len
FROM users.csv
ORDER BY name_len, name

-- WHERE with alias (extension - non-standard SQL)
SELECT age,
       CASE WHEN age % 2 = 0 THEN 'even' ELSE 'odd' END AS parity,
       name
FROM users.csv
WHERE parity = 'even'

-- Combine all three
SELECT name, age,
       CASE WHEN age < 30 THEN 'junior'
            WHEN age < 40 THEN 'mid'
            ELSE 'senior' END AS level
FROM users.csv
WHERE level != 'junior'
ORDER BY level, age
```

*Note: Using aliases in WHERE is a CQ extension. Standard SQL evaluates WHERE before SELECT, so aliases aren't normally available. However, this feature is convenient for computed columns like CASE expressions.*

### Aggregate Functions
```sql
COUNT(*)              -- Count all rows
COUNT(column)         -- Count non-null values
SUM(column)          -- Sum numeric values
AVG(column)          -- Average of numeric values
MIN(column)          -- Minimum value
MAX(column)          -- Maximum value
STDDEV(column)       -- Population standard deviation
MEDIAN(column)       -- Median value (50th percentile)
```

### Scalar Functions

#### String Functions
```sql
CONCAT(str1, str2, ...)      -- Concatenate strings
UPPER(str)                    -- Convert to uppercase
LOWER(str)                    -- Convert to lowercase
LENGTH(str)                   -- String length
SUBSTRING(str, start, len)    -- Extract substring (1-indexed)
REPLACE(str, from, to)        -- Replace all occurrences
COALESCE(val1, val2, ...)    -- First non-null value
```

#### Mathematical Functions
```sql
POWER(base, exponent)         -- Raise base to exponent power
SQRT(number)                  -- Square root (NULL for negative numbers)
CEIL(number)                  -- Round up to nearest integer
CEILING(number)               -- Alias for CEIL
FLOOR(number)                 -- Round down to nearest integer
ROUND(number, [decimals])     -- Round to specified decimal places (default 0)
ABS(number)                   -- Absolute value
EXP(number)                   -- e raised to the power of number
LN(number)                    -- Natural logarithm (NULL for numbers ≤ 0)
LOG(number)                   -- Alias for LN
MOD(dividend, divisor)        -- Modulo (remainder of division, NULL if divisor is 0)
```

#### Date Functions
```sql
-- Construction and Formatting
DATE(year, month, day)        -- Create date from components
DATE(string)                  -- Parse date from string
DATE_FORMAT(date, format)     -- Format date ('ISO', 'US', 'EU', 'COMPACT')
CURRENT_DATE                  -- Current system date (no parentheses)

-- Extraction
YEAR(date)                    -- Extract year
MONTH(date)                   -- Extract month (1-12)
DAY(date)                     -- Extract day (1-31)
DAYOFWEEK(date)               -- Day of week (0=Sunday, 6=Saturday)
DAYOFYEAR(date)               -- Day of year (1-366)

-- Arithmetic
DATE_ADD(date, days)          -- Add days to date
DATE_SUB(date, days)          -- Subtract days from date
DATE_DIFF(date1, date2)       -- Days between dates (date1 - date2)
```

### Arithmetic Operators
```sql
+    -- Addition
-    -- Subtraction
*    -- Multiplication
/    -- Division
%    -- Modulo
&    -- Bitwise AND
|    -- Bitwise OR
^    -- Bitwise XOR
```

**Operator Precedence** (highest to lowest):
1. Parentheses `()`
2. Multiplication, Division, Modulo `*`, `/`, `%`
3. Addition, Subtraction `+`, `-`
4. Bitwise `&`, `|`, `^`

## Data Types

The engine automatically infers types from CSV data:

| Type | Description | Examples |
|------|-------------|----------|
| `INTEGER` | Whole numbers | `42`, `-17`, `0` |
| `DOUBLE` | Floating point | `3.14`, `-0.5`, `1.23e-4` |
| `STRING` | Text values | `"Alice"`, `"admin"` |
| `DATE` | Date values | `2024-12-15`, `12/15/2024`, `15/12/2024` |
| `NULL` | Missing values | Empty CSV cells |

### Date Type Support

CQ provides comprehensive support for DATE values with automatic type inference, multiple format parsing, and a rich set of date functions.

#### Date Formats

Dates can be parsed in multiple formats:

| Format | Example | Description |
|--------|---------|-------------|
| ISO (YYYY-MM-DD) | `2024-12-15` | International standard format |
| US (MM/DD/YYYY) | `12/15/2024` | American format |
| EU (DD/MM/YYYY) | `15/12/2024` | European format |
| Compact (YYYYMMDD) | `20241215` | Compact numeric format |

```sql
-- All these are valid DATE values:
SELECT * FROM events.csv WHERE event_date = '2024-12-15'
SELECT * FROM events.csv WHERE event_date = '12/15/2024'
SELECT * FROM events.csv WHERE event_date = '15/12/2024'
SELECT * FROM events.csv WHERE event_date = '20241215'
```

#### Date Auto-Detection

CSV columns containing date values in YYYY-MM-DD format are automatically detected and treated as DATE type:

```csv
event_id,event_name,event_date
1,Conference,2024-06-15
2,Workshop,2024-07-20
3,Meetup,2024-08-10
```

```sql
-- Dates are automatically parsed from CSV
SELECT event_name FROM events.csv WHERE event_date > '2024-07-01'
```

#### Date Functions

**Construction and Formatting:**
```sql
DATE(year, month, day)           -- Create a date from components
DATE('2024-12-15')               -- Parse a date string
DATE_FORMAT(date, format)        -- Format date ('ISO', 'US', 'EU', 'COMPACT')
CURRENT_DATE                     -- Current system date (no parentheses)
```

**Extraction Functions:**
```sql
YEAR(date)                       -- Extract year (e.g., 2024)
MONTH(date)                      -- Extract month (1-12)
DAY(date)                        -- Extract day of month (1-31)
DAYOFWEEK(date)                  -- Day of week (0=Sunday, 6=Saturday)
DAYOFYEAR(date)                  -- Day of year (1-366)
```

**Date Arithmetic:**
```sql
DATE_ADD(date, days)             -- Add days to date
DATE_SUB(date, days)             -- Subtract days from date
DATE_DIFF(date1, date2)          -- Days between dates (date1 - date2)
```

**Date Comparisons:**
```sql
-- All standard comparison operators work with dates
date1 = date2
date1 != date2
date1 < date2
date1 <= date2
date1 > date2
date1 >= date2

-- BETWEEN works with dates (inclusive)
event_date BETWEEN '2024-01-01' AND '2024-12-31'
```

#### Date Examples

**Basic Date Queries:**
```sql
-- Filter by date
SELECT * FROM events.csv WHERE event_date = '2024-06-15'

-- Date range queries
SELECT * FROM orders.csv 
WHERE order_date >= '2024-01-01' 
  AND order_date < '2024-02-01'

-- BETWEEN with dates
SELECT * FROM sales.csv 
WHERE sale_date BETWEEN '2024-Q1-01' AND '2024-03-31'

-- Compare dates
SELECT event_name, event_date 
FROM events.csv 
WHERE event_date > CURRENT_DATE
```

**Date Extraction:**
```sql
-- Extract date components
SELECT 
    event_name,
    event_date,
    YEAR(event_date) AS year,
    MONTH(event_date) AS month,
    DAY(event_date) AS day,
    DAYOFWEEK(event_date) AS weekday,
    DAYOFYEAR(event_date) AS day_of_year
FROM events.csv

-- Group by month
SELECT 
    YEAR(sale_date) AS year,
    MONTH(sale_date) AS month,
    COUNT(*) AS total_sales,
    SUM(amount) AS revenue
FROM sales.csv
GROUP BY YEAR(sale_date), MONTH(sale_date)
ORDER BY year, month

-- Filter weekends (Saturday=6, Sunday=0)
SELECT * FROM events.csv 
WHERE DAYOFWEEK(event_date) IN (0, 6)
```

**Date Arithmetic:**
```sql
-- Add/subtract days
SELECT 
    event_name,
    event_date,
    DATE_ADD(event_date, 7) AS one_week_later,
    DATE_SUB(event_date, 30) AS thirty_days_ago
FROM events.csv

-- Calculate age of records
SELECT 
    event_name,
    DATE_DIFF(CURRENT_DATE, event_date) AS days_ago
FROM events.csv

-- Find events in next 30 days
SELECT * FROM events.csv
WHERE DATE_DIFF(event_date, CURRENT_DATE) BETWEEN 0 AND 30

-- Days between two dates
SELECT 
    order_date,
    ship_date,
    DATE_DIFF(ship_date, order_date) AS processing_days
FROM orders.csv
```

**Date Formatting:**
```sql
-- Format dates for output
SELECT 
    event_name,
    DATE_FORMAT(event_date, 'ISO') AS iso_format,      -- 2024-12-15
    DATE_FORMAT(event_date, 'US') AS us_format,        -- 12/15/2024
    DATE_FORMAT(event_date, 'EU') AS eu_format,        -- 15/12/2024
    DATE_FORMAT(event_date, 'COMPACT') AS compact      -- 20241215
FROM events.csv
```

**Creating Dates:**
```sql
-- Construct dates from components
SELECT 
    event_name,
    DATE(2024, 12, 15) AS fixed_date,
    DATE(YEAR(CURRENT_DATE), 1, 1) AS year_start,
    DATE(YEAR(event_date), MONTH(event_date), 1) AS month_start
FROM events.csv

-- Parse date strings
SELECT DATE('2024-12-15') AS parsed_date

-- Using current date
SELECT 
    event_name,
    DATE_DIFF(event_date, CURRENT_DATE) AS days_until
FROM events.csv
WHERE event_date > CURRENT_DATE
ORDER BY days_until
```

**Complex Date Operations:**
```sql
-- Find quarterly reports
SELECT 
    YEAR(report_date) AS year,
    CASE
        WHEN MONTH(report_date) <= 3 THEN 'Q1'
        WHEN MONTH(report_date) <= 6 THEN 'Q2'
        WHEN MONTH(report_date) <= 9 THEN 'Q3'
        ELSE 'Q4'
    END AS quarter,
    COUNT(*) AS report_count
FROM reports.csv
GROUP BY year, quarter
ORDER BY year, quarter

-- Business days calculation (approximate, excludes weekends)
SELECT 
    task_name,
    start_date,
    end_date,
    DATE_DIFF(end_date, start_date) AS total_days,
    DATE_DIFF(end_date, start_date) - 
        (DATE_DIFF(end_date, start_date) / 7) * 2 AS approx_business_days
FROM tasks.csv

-- Age in years (approximate)
SELECT 
    name,
    birth_date,
    DATE_DIFF(CURRENT_DATE, birth_date) / 365 AS age_years
FROM people.csv
```

**Date Validation:**
```sql
-- Invalid dates are treated as NULL
SELECT DATE('2024-02-30')  -- Returns NULL (invalid date)
SELECT DATE('2024-13-01')  -- Returns NULL (invalid month)
SELECT DATE('2024-00-15')  -- Returns NULL (invalid day)

-- Check for valid dates
SELECT * FROM events.csv 
WHERE event_date IS NOT NULL  -- Only valid dates
```

**Notes:**
- DATE values are stored internally as year/month/day (12 bytes)
- Dates are compared chronologically (earlier dates are "less than" later dates)
- Invalid dates (e.g., Feb 30, Month 13) return NULL
- DATE functions return NULL for invalid inputs
- CURRENT_DATE has no parentheses (constant, not a function)
- Date arithmetic uses days as the unit
- Year range: -9999 to 9999 (practical range: 1-9999)
- Leap years are handled correctly

## CSV Format

### Supported Features
- Header row (automatically detected)
- Custom delimiters (`,`, `\t`, `;`, etc.)
- Quoted fields (`"value with, comma"`)
- Escaped quotes (`"quote ""inside"" field"`)
- Large files (memory-mapped I/O)

### Example CSV
```csv
id,name,age,role,height,active
1,Alice,25,admin,165.50,1
2,Bob,30,user,178.20,1
3,Charlie,35,moderator,172.00,0
```

### Advanced Features

#### DISTINCT - Remove Duplicate Rows
```sql
SELECT DISTINCT gender FROM users.csv
SELECT DISTINCT city, country FROM addresses.csv
```

#### LIKE / ILIKE - Pattern Matching
```sql
-- LIKE: case-sensitive pattern matching
SELECT name FROM users.csv WHERE name LIKE 'A%'        -- Starts with 'A'
SELECT email FROM users.csv WHERE email LIKE '%@gmail.com'  -- Ends with
SELECT code FROM products.csv WHERE code LIKE 'USB-___'     -- Exactly 3 chars after USB-
SELECT name FROM users.csv WHERE name LIKE '%john%'         -- Contains 'john'

-- ILIKE: case-insensitive pattern matching
SELECT name FROM users.csv WHERE name ILIKE 'alice'    -- Matches Alice, ALICE, alice
SELECT email FROM users.csv WHERE email ILIKE '%@EXAMPLE.COM'

-- Wildcards:
--   %  matches any sequence of characters (including empty)
--   _  matches exactly one character
```

#### Set Operations - Combine Query Results
```sql
-- UNION: Combine results, remove duplicates
SELECT name FROM customers_2023.csv
UNION
SELECT name FROM customers_2024.csv

-- UNION ALL: Combine results, keep all rows (including duplicates)
SELECT product FROM sales_q1.csv
UNION ALL
SELECT product FROM sales_q2.csv

-- INTERSECT: Return only rows that appear in both queries
SELECT email FROM newsletter.csv
INTERSECT
SELECT email FROM customers.csv

-- EXCEPT: Return rows from first query that are NOT in second query
SELECT email FROM all_users.csv
EXCEPT
SELECT email FROM inactive_users.csv

-- Chaining multiple set operations
SELECT id FROM table_a.csv
UNION
SELECT id FROM table_b.csv
INTERSECT
SELECT id FROM table_c.csv

-- Set operations with WHERE clauses
SELECT name FROM users.csv WHERE age < 30
UNION
SELECT name FROM users.csv WHERE role = 'admin'
```

**Set Operation Rules:**
- Both queries must have the same number of columns
- Column names come from the first query
- UNION removes duplicates, UNION ALL keeps all rows
- INTERSECT returns common rows (duplicates removed)
- EXCEPT returns rows in first but not second (duplicates removed)
- Operations can be chained: (A UNION B) INTERSECT C

#### Joins - Combine Multiple Tables
```sql
-- INNER JOIN
SELECT t1.name, t2.email 
FROM users.csv AS t1 
INNER JOIN emails.csv AS t2 ON t1.id = t2.user_id

-- LEFT JOIN
SELECT t1.name, t2.phone 
FROM users.csv AS t1 
LEFT JOIN contacts.csv AS t2 ON t1.id = t2.id

-- FULL JOIN
SELECT * FROM table1.csv FULL JOIN table2.csv ON table1.id = table2.id
```

#### Subqueries
```sql
-- Subquery in FROM (derived table)
SELECT * FROM (SELECT name, age FROM users.csv WHERE age > 25) AS sub

-- Subquery in WHERE with IN
SELECT name FROM users.csv WHERE role IN (SELECT role FROM admins.csv)

-- Scalar subquery in SELECT
SELECT name, (SELECT MAX(age) FROM users.csv) AS max_age FROM users.csv

-- Correlated subquery
SELECT name, age FROM users.csv AS t1 
WHERE age > (SELECT AVG(age) FROM users.csv WHERE role = t1.role)
```

#### GROUP BY with HAVING
```sql
SELECT role, COUNT(*) AS cnt, AVG(age) AS avg_age
FROM users.csv
GROUP BY role
HAVING COUNT(*) > 5 AND AVG(age) > 30
ORDER BY cnt DESC
```

#### Window Functions

Window functions perform calculations across a set of rows that are related to the current row. Unlike aggregate functions with GROUP BY, window functions retain all rows in the result.

**Supported Window Functions:**
```sql
ROW_NUMBER()      -- Sequential number for each row within partition
RANK()            -- Rank with gaps for tied values
DENSE_RANK()      -- Rank without gaps for tied values
LAG(column)       -- Access previous row's value
LEAD(column)      -- Access next row's value
SUM(column)       -- Running/cumulative sum
AVG(column)       -- Running/cumulative average
COUNT(*)          -- Running/cumulative count
```

**Basic Window Function:**
```sql
-- Add row numbers to all rows
SELECT name, age, 
       ROW_NUMBER() OVER (ORDER BY age) AS row_num
FROM users.csv

-- Rank employees by salary
SELECT name, salary,
       RANK() OVER (ORDER BY salary DESC) AS rank,
       DENSE_RANK() OVER (ORDER BY salary DESC) AS dense_rank
FROM employees.csv
```

**PARTITION BY - Separate Window per Group:**
```sql
-- Row number within each department
SELECT department, name, salary,
       ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees.csv

-- Running total per category
SELECT category, product, price,
       SUM(price) OVER (PARTITION BY category ORDER BY price) AS running_total
FROM products.csv
```

**LAG and LEAD - Access Adjacent Rows:**
```sql
-- Compare with previous row
SELECT name, age,
       LAG(age) OVER (ORDER BY age) AS prev_age,
       age - LAG(age) OVER (ORDER BY age) AS age_diff
FROM users.csv

-- Compare with next row
SELECT date, value,
       LEAD(value) OVER (ORDER BY date) AS next_value,
       LEAD(value) OVER (ORDER BY date) - value AS change
FROM timeseries.csv
```

**Running Aggregates:**
```sql
-- Running sum and average
SELECT date, amount,
       SUM(amount) OVER (ORDER BY date) AS running_sum,
       AVG(amount) OVER (ORDER BY date) AS running_avg
FROM sales.csv

-- Running count
SELECT product, sale_date,
       COUNT(*) OVER (ORDER BY sale_date) AS total_sales_so_far
FROM orders.csv
```

**Notes:**
- All window functions require an OVER clause
- ORDER BY within OVER determines row ordering for the calculation
- PARTITION BY divides rows into groups (optional)
- Window functions are evaluated after WHERE, GROUP BY, and HAVING
- Multiple window functions can be used in the same query
- LAG/LEAD respect ORDER BY within partitions
- Running aggregates (SUM, AVG, COUNT) are cumulative and progressive

**Examples:**
```bash
# Basic window function
./build/cq -q "SELECT name, age, ROW_NUMBER() OVER (ORDER BY age) AS row_num FROM 'data/users.csv' ORDER BY age" -p

# Running sum
./build/cq -q "SELECT name, age, SUM(age) OVER (ORDER BY age) AS running_sum FROM 'data/users.csv' ORDER BY age" -p

# PARTITION BY with LAG
./build/cq -q "SELECT role, name, age, LAG(age) OVER (PARTITION BY role ORDER BY age) AS prev_age FROM 'data/users.csv'" -p
```

#### Nested Functions
```sql
SELECT UPPER(CONCAT(SUBSTRING(name, 1, 1), '. ', surname)) AS formatted
FROM users.csv

SELECT LENGTH(REPLACE(UPPER(name), 'A', '')) AS modified_len
FROM users.csv
```

## Usage Examples

### Basic Queries

```sql
-- Simple SELECT
SELECT name, age FROM data.csv

-- SELECT with WHERE
SELECT name, age FROM data.csv WHERE age > 30 AND role = 'admin'

-- SELECT with ORDER BY
SELECT name, age FROM data.csv ORDER BY age DESC

-- SELECT with LIMIT
SELECT name, age FROM data.csv LIMIT 10

-- SELECT with LIMIT and OFFSET (pagination)
SELECT name, age FROM data.csv ORDER BY age LIMIT 10 OFFSET 20
```

### Arithmetic Expressions

```sql
-- Basic arithmetic in SELECT
SELECT name, age, age * 2 AS double_age, age + 10 AS age_plus_10
FROM data.csv

-- Arithmetic with proper precedence
SELECT price + tax, quantity * (price + tax) AS total
FROM orders.csv

-- Modulo for finding patterns
SELECT name, age FROM data.csv WHERE age % 10 = 0

-- Bitwise operations
SELECT id, flags & 15 AS lower_bits FROM data.csv WHERE (flags & 16) > 0
```

### String Functions

```sql
-- Concatenation
SELECT CONCAT(name, ' - ', role) AS info FROM data.csv

-- Case conversion
SELECT UPPER(name) AS upper_name, LOWER(role) AS lower_role FROM data.csv

-- String manipulation
SELECT SUBSTRING(name, 1, 3) AS initials, 
       LENGTH(name) AS name_len,
       REPLACE(role, 'admin', 'administrator') AS new_role
FROM data.csv

-- Functions in WHERE
SELECT name FROM data.csv WHERE LENGTH(name) > 5 AND UPPER(role) = 'ADMIN'
```

### Mathematical Functions

```sql
-- Power and roots
SELECT POWER(2, 10) AS power_result FROM data.csv  -- 1024
SELECT SQRT(age) AS sqrt_age FROM data.csv WHERE age >= 0

-- Rounding functions
SELECT CEIL(height) AS rounded_up FROM data.csv
SELECT FLOOR(height) AS rounded_down FROM data.csv
SELECT ROUND(height, 1) AS rounded_1dp FROM data.csv
SELECT ROUND(height) AS rounded_int FROM data.csv

-- Absolute value
SELECT ABS(age - 30) AS age_diff FROM data.csv

-- Exponential and logarithm
SELECT EXP(1) AS e FROM data.csv                    -- 2.718...
SELECT LN(age) AS log_age FROM data.csv WHERE age > 0

-- Modulo
SELECT age, MOD(age, 10) AS last_digit FROM data.csv

-- Combined math operations
SELECT ROUND(SQRT(POWER(x, 2) + POWER(y, 2)), 2) AS distance FROM coordinates.csv
SELECT FLOOR(LN(population)) AS log_pop FROM cities.csv

-- Math in WHERE clause
SELECT name FROM data.csv WHERE SQRT(age) > 5
SELECT name FROM data.csv WHERE MOD(age, 5) = 0
SELECT name FROM data.csv WHERE POWER(age, 2) > 1000
```

### Pattern Matching

```sql
-- LIKE: case-sensitive
SELECT * FROM products.csv WHERE name LIKE 'iPhone%'
SELECT * FROM users.csv WHERE email LIKE '%@company.com'
SELECT * FROM codes.csv WHERE code LIKE 'A__B'  -- A, 2 chars, B

-- ILIKE: case-insensitive
SELECT * FROM users.csv WHERE name ILIKE 'john%'
SELECT * FROM emails.csv WHERE email ILIKE '%@GMAIL.COM'

-- Combined with other conditions
SELECT name, email FROM users.csv 
WHERE name LIKE 'A%' AND email ILIKE '%@example.com'

-- Pattern in subquery
SELECT * FROM orders.csv 
WHERE customer_id IN (
    SELECT id FROM customers.csv WHERE name LIKE 'Smith%'
)
```

### Aggregation

```sql
-- Basic aggregation
SELECT COUNT(*) FROM data.csv WHERE active = 1

-- GROUP BY with aggregates
SELECT role, COUNT(*) AS count, AVG(age) AS avg_age, MAX(height) AS max_height
FROM data.csv
GROUP BY role

-- Statistical aggregates
SELECT 
    AVG(age) AS mean_age,
    STDDEV(age) AS stddev_age,
    MEDIAN(age) AS median_age,
    MIN(age) AS min_age,
    MAX(age) AS max_age
FROM data.csv

-- Statistical aggregates with GROUP BY
SELECT role, COUNT(*) AS cnt, AVG(age) AS avg, STDDEV(age) AS stdev, MEDIAN(age) AS median
FROM data.csv
GROUP BY role

-- Aggregates without GROUP BY (entire table as one group)
SELECT COUNT(*) AS total, AVG(age) AS avg_age, MIN(age) AS min_age
FROM data.csv

-- HAVING clause
SELECT role, COUNT(*) AS cnt FROM data.csv 
GROUP BY role 
HAVING COUNT(*) > 2
ORDER BY cnt DESC
```

### Set Operations

```sql
-- UNION: Combine and remove duplicates
SELECT name, email FROM customers_2023.csv
UNION
SELECT name, email FROM customers_2024.csv

-- UNION ALL: Keep all rows including duplicates
SELECT product_id FROM orders_jan.csv
UNION ALL
SELECT product_id FROM orders_feb.csv

-- INTERSECT: Find common rows
SELECT user_id FROM premium_users.csv
INTERSECT
SELECT user_id FROM active_users.csv

-- EXCEPT: Find differences
SELECT email FROM all_subscribers.csv
EXCEPT
SELECT email FROM bounced_emails.csv

-- Multiple operations chained
SELECT id FROM group_a.csv
UNION SELECT id FROM group_b.csv
INTERSECT SELECT id FROM active.csv

-- With complex queries
SELECT name FROM users.csv WHERE age > 30 AND city = 'NYC'
UNION
SELECT name FROM admins.csv WHERE role = 'super_admin'
EXCEPT
SELECT name FROM banned.csv
```

### Complex Queries

```sql
-- Join with aggregation
SELECT t.role, COUNT(*) AS cnt, MAX(t.height) AS max_h
FROM 'data/users.csv' AS t
LEFT JOIN 'data/emails.csv' AS e ON t.id = e.user_id
WHERE t.age > 25
GROUP BY t.role
HAVING COUNT(*) > 1
ORDER BY max_h DESC
LIMIT 10

-- Subquery in FROM with filtering
SELECT * FROM (
    SELECT name, age, role FROM data.csv WHERE age > 18
) AS adults
WHERE role = 'user'

-- Multiple subqueries
SELECT name, age,
       (SELECT AVG(age) FROM data.csv) AS avg_age,
       (SELECT MAX(age) FROM data.csv WHERE role = t1.role) AS role_max
FROM data.csv AS t1
WHERE age > (SELECT AVG(age) FROM data.csv)
```

## Advanced Topics

### NULL Handling
```sql
-- NULL in comparisons evaluates to false
SELECT * FROM data.csv WHERE email = NULL  -- Returns no rows

-- Use scalar functions for NULL handling
SELECT COALESCE(email, 'N/A') AS email FROM data.csv

-- NULL in aggregates are ignored
SELECT AVG(age) FROM data.csv  -- Ignores NULL ages
```

### Type Coercion
```sql
-- Strings compared lexicographically
WHERE name > 'M'

-- Numbers automatically promoted to doubles when needed
SELECT age / 3.0 FROM data.csv  -- Result is always double

-- Integer division returns integer if no remainder
SELECT 10 / 2  -- Returns 5 (integer)
SELECT 10 / 3  -- Returns 3.333... (double)
```

### Qualified Identifiers
```sql
-- Use table aliases to avoid ambiguity in joins
SELECT t1.name, t2.email
FROM users.csv AS t1
JOIN contacts.csv AS t2 ON t1.id = t2.id

-- Column without qualifier tries all tables
SELECT name FROM users.csv  -- Works if name exists
```

### Example Queries

```sql
-- Basic query
SELECT name, age FROM data.csv WHERE age > 30

-- With functions
SELECT UPPER(name), LENGTH(name) AS len FROM data.csv

-- Complex join with aggregates
SELECT t.role, MAX(t.height) AS max_h, LOWER(t2.email) AS email
FROM './data.csv' AS t 
LEFT JOIN './emails.csv' AS t2 ON t.id = t2.id
WHERE t.age > 25
GROUP BY t.role
ORDER BY MAX(t.height) DESC
LIMIT 5

-- CREATE TABLE - Save query results
CREATE TABLE 'active_users.csv' AS
SELECT * FROM 'users.csv' WHERE active = 1

-- CREATE TABLE - Aggregation report
CREATE TABLE 'summary.csv' AS
SELECT role, COUNT(*) as cnt, AVG(age) as avg_age
FROM 'users.csv'
GROUP BY role

-- CREATE TABLE - Empty table with schema
CREATE TABLE 'new_table.csv' (id, name, email, created_at)

-- String manipulation
SELECT 
  name,
  CONCAT(name, ' - ', role) AS info,
  SUBSTRING(name, 1, 3) AS short,
  REPLACE(role, 'admin', 'ADMIN') AS new_role
FROM data.csv

-- Pagination
SELECT name, age 
FROM data.csv 
ORDER BY age DESC 
LIMIT 10 OFFSET 20  -- Page 3 (20 records skipped)

-- SELECT * variations
SELECT * FROM data.csv LIMIT 5
SELECT *, UPPER(name) AS name_upper FROM data.csv
SELECT id, *, age FROM data.csv WHERE age > 30

-- Arithmetic expressions
SELECT name, age * 2 AS double_age, height / 100 AS height_m FROM data.csv
SELECT price + tax AS total, quantity * price AS subtotal FROM orders.csv

-- Nested functions
SELECT UPPER(CONCAT(name, ' - ', role)) AS full_info FROM data.csv
SELECT REPLACE(UPPER(CONCAT(name, ' ', role)), 'ADMIN', 'ADMINISTRATOR') FROM data.csv
SELECT LENGTH(CONCAT(name, role)) AS total_len FROM data.csv
SELECT SUBSTRING(UPPER(name), 1, 3) AS short FROM data.csv

-- Functions in WHERE clause
SELECT name FROM data.csv WHERE LENGTH(name) > 5
SELECT name FROM data.csv WHERE UPPER(role) = 'ADMIN'
SELECT name FROM data.csv WHERE LENGTH(CONCAT(name, role)) > 10
SELECT name FROM data.csv WHERE SUBSTRING(name, 1, 1) = 'A'

-- BETWEEN operator (inclusive on both ends)
SELECT name, age FROM 'users.csv' WHERE age BETWEEN 25 AND 35
SELECT * FROM 'products.csv' WHERE price BETWEEN 10.0 AND 50.0
SELECT name FROM 'users.csv' WHERE name BETWEEN 'A' AND 'M'
SELECT * FROM 'sales.csv' WHERE date BETWEEN '2024-01-01' AND '2024-12-31'
SELECT name, age FROM 'users.csv' WHERE age * 2 BETWEEN 50 AND 70

-- CASE expressions
SELECT name, age,
  CASE age
    WHEN 25 THEN 'Quarter century'
    WHEN 30 THEN 'Thirty'
    ELSE 'Other'
  END AS age_label
FROM data.csv

SELECT name,
  CASE
    WHEN age < 18 THEN 'Minor'
    WHEN age < 65 THEN 'Adult'
    ELSE 'Senior'
  END AS age_group
FROM data.csv

SELECT name, age,
  CASE
    WHEN age < 30 THEN 1
    WHEN age < 50 THEN 2
    ELSE 3
  END AS tier
FROM data.csv

SELECT * FROM data.csv
WHERE CASE WHEN age < 30 THEN 1 ELSE 0 END = 1

SELECT name FROM data.csv
ORDER BY CASE WHEN age < 30 THEN 2 WHEN age < 50 THEN 1 ELSE 0 END

-- Scalar functions with GROUP BY
SELECT role, UPPER(role) AS upper_role FROM data.csv GROUP BY role
SELECT role, LENGTH(role) AS len, COUNT(*) FROM data.csv GROUP BY role
SELECT role, UPPER(CONCAT('Role: ', role)) AS formatted FROM data.csv GROUP BY role
SELECT role, LENGTH(UPPER(role)) AS len, SUM(age) FROM data.csv GROUP BY role

-- HAVING clause
SELECT role, COUNT(*) FROM data.csv GROUP BY role HAVING COUNT(*) > 2
SELECT role, AVG(age) AS avg_age FROM data.csv GROUP BY role HAVING AVG(age) > 30
SELECT role, COUNT(*) AS cnt FROM data.csv GROUP BY role HAVING cnt >= 2 ORDER BY cnt DESC
SELECT role, COUNT(*), AVG(age) FROM data.csv GROUP BY role HAVING COUNT(*) >= 2 AND AVG(age) > 30

-- Subqueries in FROM clause
SELECT * FROM (SELECT name, age FROM data.csv) AS sub WHERE age > 30
SELECT name FROM (SELECT * FROM data.csv WHERE role = 'admin') AS sub
SELECT role, COUNT(*) FROM (SELECT * FROM data.csv WHERE age > 25) AS sub GROUP BY role
SELECT * FROM (SELECT role, AVG(age) AS avg_age FROM data.csv GROUP BY role HAVING AVG(age) > 30) AS sub

-- Subqueries in WHERE clause (IN operator)
SELECT name FROM data.csv WHERE role IN (SELECT role FROM data.csv WHERE age > 30)
SELECT name FROM data.csv WHERE name IN (SELECT name FROM data.csv WHERE role = 'admin')
SELECT name, age FROM data.csv WHERE age IN (SELECT age FROM data.csv WHERE age > 40)
SELECT name FROM data.csv WHERE role IN (SELECT role FROM data.csv WHERE age > 30) AND age < 40

-- Scalar subqueries in SELECT
SELECT name, (SELECT MAX(age) FROM data.csv) AS max_age FROM data.csv LIMIT 1
SELECT name, (SELECT AVG(age) FROM data.csv) AS avg_age FROM data.csv WHERE age > 25
SELECT name, (SELECT COUNT(*) FROM data.csv WHERE role = 'admin') AS admin_count FROM data.csv
SELECT name, (SELECT MIN(age) FROM data.csv) AS min, (SELECT MAX(age) FROM data.csv) AS max FROM data.csv

-- Scalar subqueries in WHERE
SELECT name FROM data.csv WHERE age = (SELECT MAX(age) FROM data.csv)
SELECT name FROM data.csv WHERE age > (SELECT AVG(age) FROM data.csv)
SELECT name FROM data.csv WHERE age < (SELECT AVG(age) FROM data.csv WHERE role = 'admin')

-- Correlated subqueries
SELECT name, age, role FROM data.csv AS t1 
WHERE age > (SELECT AVG(age) FROM data.csv WHERE role = t1.role)

SELECT name, role, (SELECT COUNT(*) FROM data.csv WHERE role = t1.role) AS role_count 
FROM data.csv AS t1

SELECT name, age FROM data.csv AS t1 
WHERE age = (SELECT MAX(age) FROM data.csv WHERE role = t1.role)
```

### LIMIT Syntax
```sql
-- Simple limit
LIMIT 10

-- With offset (standard SQL)
LIMIT 10 OFFSET 5

-- With offset (MySQL style)
LIMIT 5, 10  -- offset 5, limit 10
```

### Arithmetic Expressions
**Full Support** - Complex arithmetic with proper operator precedence and parentheses
```sql
SELECT age + 10, age * 2, (age + 5) * 2 FROM data.csv

SELECT name, age FROM data.csv WHERE age * 2 > 60

SELECT (age + 10) * 2 / 5 FROM data.csv WHERE (age + 5) > 30

-- Modulo operator
SELECT age % 10 FROM data.csv WHERE age % 2 = 0

-- Bitwise operators
SELECT age & 15, age | 1 FROM data.csv WHERE (age & 16) > 0
SELECT id ^ 255 AS xor_result FROM data.csv  -- XOR operation
```
**Operators:**
- Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo)
- Bitwise: `&` (AND), `|` (OR), `^` (XOR)
- Operator precedence: parentheses > `*`, `/`, `%` > `+`, `-` > `&`, `|`, `^`
- Parentheses for grouping: `(2 + 3) * 4` = 20
- Works in SELECT and WHERE clauses
- Mixed integer and floating-point arithmetic
- Type preservation: integer operations return integers when possible
- Bitwise operations require integer operands

### Logical Operators
**NOT Operator** - Logical negation of conditions
```sql
SELECT name FROM data.csv WHERE NOT age > 30

SELECT name FROM data.csv WHERE NOT (age > 20 AND age < 30)

SELECT name FROM data.csv WHERE age NOT IN (25, 30, 35)
```
**Features:**
- `NOT` prefix operator for negating conditions
- Works with parenthesized complex conditions
- `NOT IN` for list/subquery exclusion
- Can be combined with `AND`/`OR`

## Examples

### Operator Precedence
```sql
-- Without parentheses: 2 + (3 * 4) = 14
SELECT 2 + 3 * 4 FROM test_data.csv
-- Result: 14 for all rows

-- With parentheses: (2 + 3) * 4 = 20  
SELECT (2 + 3) * 4 FROM test_data.csv
-- Result: 20 for all rows
```

### Complex Expressions
```sql
-- Nested operations with columns
SELECT name, age, (age + 10) * 2 - 5 FROM test_data.csv
-- Result: For Alice (age 25): (25 + 10) * 2 - 5 = 65

-- WHERE with arithmetic
SELECT name, age FROM test_data.csv WHERE (age + 5) * 2 > 70
-- Result: Charlie (35), Eve (42), Grace (33)
```

### Type Handling
```sql
-- Integer arithmetic stays integer when possible
SELECT age / 2 FROM test_data.csv WHERE age = 30
-- Result: 15 (integer)

-- Mixed or non-whole results become double
SELECT age / 3 FROM test_data.csv WHERE age = 25
-- Result: 8.33 (double)
```
