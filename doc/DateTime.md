# Date and Time

## Date Formats
- ISO: 2024-12-15
- US: 12/15/2024
- EU: 15/12/2024
- COMPACT: 20241215

## Date Auto-Detection
CSV columns with dates in YYYY-MM-DD format are detected as DATE.

## Date Functions
- DATE(year, month, day)
- DATE(string)
- DATE_FORMAT(date, format)
- CURRENT_DATE
- YEAR(date), MONTH(date), DAY(date), DAYOFWEEK(date), DAYOFYEAR(date)
- DATE_ADD(date, days), DATE_SUB(date, days), DATE_DIFF(date1, date2)

## Date Examples
```sql
SELECT * FROM events.csv WHERE event_date > '2024-07-01'
```
