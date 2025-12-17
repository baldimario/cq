# Data Types

The engine automatically infers types from CSV data:

| Type | Description | Examples |
|------|-------------|----------|
| `INTEGER` | Whole numbers | 42, -17, 0 |
| `DOUBLE`  | Floating point | 3.14, -0.5 |
| `STRING`  | Text values | "Alice" |
| `DATE`    | Date values | 2024-12-15 |
| `NULL`    | Missing values | Empty cells |

## Date Formats
Dates can be parsed in multiple formats (ISO, US, EU, Compact).

## Date Auto-Detection
CSV columns with dates in YYYY-MM-DD are detected as DATE.

## Date Functions
- DATE(year, month, day)
- DATE(string)
- DATE_FORMAT(date, format)
- CURRENT_DATE
- YEAR(date), MONTH(date), DAY(date), DAYOFWEEK(date), DAYOFYEAR(date)
- DATE_ADD(date, days), DATE_SUB(date, days), DATE_DIFF(date1, date2)
- DATE_FORMAT(date, 'ISO'|'US'|'EU'|'COMPACT')

## Date Examples
```
SELECT event_name FROM events.csv WHERE event_date > '2024-07-01'
```
