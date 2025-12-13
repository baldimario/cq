# cq - High-Performance SQL Query Engine for CSV Files

[![Build and Test](https://github.com/iworokonit/cq/workflows/Build%20and%20Test%20on%20Multiple%20Platforms/badge.svg)](https://github.com/iworokonit/cq/actions)

A lightweight, fast SQL query processor written in C that enables executing SQL queries directly on CSV files without requiring a database. Features include joins, subqueries, aggregations, arithmetic expressions, and more.

![cq in action](assets/cq.png)

## Quick Start

```bash
# Install (no Make required)
./install.sh   # Unix-like
install.bat    # Windows

# Run a query
./build/cq -q "SELECT name, age FROM data.csv WHERE age > 25" -p

# Save results to CSV
cq -q "SELECT * FROM data.csv WHERE role = 'admin'" -o output.csv

# Count matching rows
cq -q "SELECT * FROM data.csv WHERE age > 30" -c

# Run query from file
cq -f assets/example_between.sql -p

# Read query from stdin
cat query.sql | cq -q - -p
```

## Example SQL Files

The `assets/` directory contains ready-to-run SQL examples:

- **[example_between.sql](assets/example_between.sql)** - BETWEEN operator with SQL comments
- **[example_aggregation.sql](assets/example_aggregation.sql)** - Complex aggregations with GROUP BY and HAVING
- **[example_dml.sql](assets/example_dml.sql)** - INSERT, UPDATE, DELETE operations
- **[example_functions.sql](assets/example_functions.sql)** - String and math functions
- **[example_joins.sql](assets/example_joins.sql)** - JOIN operations between CSV files

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
```

**Notes:**
- All DML operations modify the CSV file in-place
- DELETE requires WHERE clause (safety measure to prevent accidental data loss)
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

## Installation

### Prerequisites

**Unix-like (Linux, macOS):**
- C compiler (GCC or Clang)
- Make
- Standard C library

**Linux ARM64 (Raspberry Pi):**
- GCC for ARM64 (`gcc-aarch64-linux-gnu` for cross-compilation)
- Or native GCC on Raspberry Pi OS 64-bit
- Make
- Standard C library

**Windows:**
- Visual Studio Build Tools (MSVC) or MinGW
- Standard C library

### Build Commands

**Quick Install (no Make required):**
```bash
# Unix-like (Linux, macOS, BSD)
./install.sh

# Windows (MSVC)
install.bat
```

**Using Make:**

**Unix-like (Linux, macOS):**
```bash
# Build the main executable
make

# Build and run all tests
make test

# Clean build artifacts
make clean
```

**Windows (MSVC):**
```cmd
# Build using the provided batch script
install.bat

# Or manually with MSVC
cl.exe /W3 /O2 /Iinclude src\*.c /Febuild\cq.exe
```

**Windows (MinGW/Git Bash):**
```bash
# Use standard Unix Makefile or install script
make
# or
./install.sh
```

**Raspberry Pi (ARM64):**
```bash
# Native build on Raspberry Pi
./install.sh
# or
make

# Cross-compile from x86_64 Linux
sudo apt-get install gcc-aarch64-linux-gnu
make CC=aarch64-linux-gnu-gcc
```

## Features

### SQL Keywords Supported

| Category | Keywords |
|----------|----------|
| **Query Structure** | `SELECT`, `DISTINCT`, `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, `OFFSET` |
| **Data Definition** | `CREATE`, `ALTER`, `TABLE`, `AS`, `RENAME`, `COLUMN`, `ADD`, `DROP`, `TO` |
| **Data Manipulation** | `INSERT`, `INTO`, `VALUES`, `UPDATE`, `SET`, `DELETE` |
| **Joins** | `JOIN`, `INNER JOIN`, `LEFT JOIN`, `RIGHT JOIN`, `FULL JOIN`, `ON` |
| **Set Operations** | `UNION`, `UNION ALL`, `INTERSECT`, `EXCEPT` |
| **Logical Operators** | `AND`, `OR`, `NOT`, `IN`, `NOT IN` |
| **Comparison** | `=`, `!=`, `<>`, `<`, `>`, `<=`, `>=`, `BETWEEN` |
| **Pattern Matching** | `LIKE`, `ILIKE` |
| **Sorting** | `ASC`, `DESC` |
| **Aliases** | `AS` |

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

## Command Line Interface

```bash
cq [OPTIONS]

Options:
  -h              Show help message
  -q <query>      SQL query to execute (use '-' to read from stdin)
  -f <file>       Read SQL query from file
  -o <file>       Write results as CSV to output file
  -c              Print count of rows
  -p              Print results as formatted table to stdout
  -v              Print results in vertical format (one column per line)
  -s <char>       Field separator for input CSV (default: ',')
  -d <char>       Output delimiter for -o option (default: ',')

Examples:
  # Print formatted table
  cq -q "SELECT name, age WHERE age > 30" -p

  # Read query from file
  cq -f query.sql -p

  # Read query from stdin (piping)
  echo "SELECT * WHERE active = 1" | cq -q - -p
  cat query.sql | cq -q - -o output.csv

  # Save to CSV file
  cq -q "SELECT * WHERE active = 1" -o output.csv

  # Count matching rows
  cq -q "SELECT * WHERE role = 'admin'" -c

  # Custom delimiter (TSV input)
  cq -q "SELECT * FROM data.tsv" -s '\t' -p
  
  # Vertical output (useful for wide tables)
  cq -q "SELECT * FROM data.csv LIMIT 5" -v

  # Combine options
  cq -q "SELECT name FROM data.csv WHERE age > 25" -o filtered.csv -c
```

## Data Types

The engine automatically infers types from CSV data:

| Type | Description | Examples |
|------|-------------|----------|
| `INTEGER` | Whole numbers | `42`, `-17`, `0` |
| `DOUBLE` | Floating point | `3.14`, `-0.5`, `1.23e-4` |
| `STRING` | Text values | `"Alice"`, `"admin"` |
| `NULL` | Missing values | Empty CSV cells |

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

## Testing

### Run All Tests
```bash
make test
```

### Test Suite
```
tests/
├── test_alter_table.c          # ALTER TABLE operations (8 tests)
├── test_arithmetic.c           # Arithmetic expressions (22 tests)
├── test_create_table.c         # CREATE TABLE operations (8 tests)
├── test_csv.c                  # CSV loading and parsing
├── test_distinct.c             # DISTINCT keyword (4 tests)
├── test_dml.c                  # INSERT/UPDATE/DELETE operations (8 tests)
├── test_evaluator.c            # Query evaluation
├── test_extended_operators.c   # Modulo, bitwise, NOT (23 tests)
├── test_like.c                 # LIKE and ILIKE operators (8 tests)
├── test_load_performance.c     # Performance benchmarking
├── test_parser.c               # SQL parsing
├── test_set_ops.c              # UNION, INTERSECT, EXCEPT (8 tests)
├── test_tokenizer.c            # Lexical analysis
└── test_where_functions.c      # Functions in WHERE (10 tests)
```

### Individual Test Execution
```bash
# Run specific test
./build/test_arithmetic
./build/test_create_table
./build/test_distinct
./build/test_dml
./build/test_evaluator
```

## Performance

### CSV Loading Performance
Tested with 1,000,000 rows:
- **Load time**: ~273ms
- **Throughput**: ~3.67M rows/second
- **Memory usage**: ~113 MB

### Memory Efficiency
- Uses memory-mapped I/O for large CSV files
- Efficient value storage (integers vs strings)
- Proper memory cleanup (no leaks)

## Architecture

### Components

```
cq/
├── include/           # Header files
│   ├── csv_reader.h   # CSV parsing and data structures
│   ├── evaluator.h    # Query execution engine
│   ├── parser.h       # SQL parser and AST
│   ├── tokenizer.h    # Lexical analyzer
│   └── utils.h        # Utility functions
├── src/               # Source files
│   ├── csv_reader.c   # CSV file loading with mmap
│   ├── evaluator.c    # Query evaluation and execution
│   ├── main.c         # CLI interface
│   ├── parser.c       # Recursive descent SQL parser
│   ├── tokenizer.c    # Tokenization and lexical analysis
│   └── utils.c        # Helper functions
├── tests/             # Test suite
├── data/              # Test data files
└── Makefile           # Build configuration
```

### Query Execution Pipeline

```
SQL String
    ↓
Tokenizer → [Tokens]
    ↓
Parser → [Abstract Syntax Tree]
    ↓
Evaluator → [ResultSet]
    ↓
Output (Table/CSV/Count)
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

## Troubleshooting

### Common Issues

**"Error: Query is required (use -q)"**
- Solution: Always provide the `-q` flag with your query

**"Error: Failed to load CSV file"**
- Check file path exists
- Verify file permissions
- Ensure CSV format is valid

**"Parse error" / "Syntax error"**
- Check SQL syntax matches supported features
- Verify all parentheses are balanced
- Check for typos in keywords

### Debugging
```bash
# Enable detailed logging (if implemented)
cq -q "SELECT * FROM data.csv" -p --verbose

# Check file with small query first
cq -q "SELECT * FROM data.csv LIMIT 5" -p
```

## Contributing

### Building with Debug Symbols
```bash
cc -g -Wall -W -Iinclude src/*.c -o build/cq_debug
```

### Running with Address Sanitizer
```bash
make address_sanitizer
```

### Code Style
- C99 standard
- 4-space indentation
- Clear variable names
- Comments for complex logic

## Technical Details

### Compiler Compatibility
- GCC 4.8+
- Clang 3.5+
- Any C99-compliant compiler

### Dependencies
- Standard C library only
- No external dependencies
- POSIX for memory-mapped I/O

## Roadmap

### Completed Features
- DISTINCT keyword (deduplication)
- LIKE/ILIKE operators (pattern matching)
- UNION/INTERSECT/EXCEPT (set operations)
- Subqueries (FROM, WHERE, scalar, correlated)
- Arithmetic expressions with precedence
- Scalar and aggregate functions
- Statistical aggregates (STDDEV, MEDIAN)
- All join types (INNER, LEFT, RIGHT, FULL)
- Data manipulation (INSERT, UPDATE, DELETE)
- CREATE TABLE (save query results, define schema)
- ALTER TABLE (rename/add/drop columns)
- Read queries from file (-f option)
- Read queries from stdin (piping support)
- SQL comments (-- and /* */)

### Planned Features
- [ ] Window functions (ROW_NUMBER, RANK)
- [ ] CASE expressions
- [ ] Index support for large files
- [ ] Query optimization

## 🔗 Additional Resources

- [SQL Standard Reference](https://www.iso.org/standard/63555.html)
- [CSV Format Specification](https://tools.ietf.org/html/rfc4180)

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

## Performance Test

```sh
$ ./build/test_load_performance
=== CSV Load Performance Test ===

Testing file: data/bigdata.csv

Results:
--------
Rows loaded:      1000000
Columns:          5
Total time:       238.79 ms
Time per row:     0.0002 ms
Rows per second:  4187796

Breakdown:
  File I/O + CSV parsing: 238.79 ms (100%)

Memory usage (approximate):
  Total: 113.27 MB
  Per row: 0.12 KB

=== Test completed successfully ===
```

---

**Version**: 1.0  
**Author**: Mario  
**Last Updated**: December 2025

## License

MIT License - see LICENSE file for details
