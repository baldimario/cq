-- Window Functions Examples
-- Demonstrates ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD, and aggregate window functions

-- Example 1: Basic ROW_NUMBER
-- Add sequential row numbers ordered by age
SELECT name, age, 
       ROW_NUMBER() OVER (ORDER BY age) AS row_num
FROM 'data/users.csv'
LIMIT 5;

-- Example 2: RANK and DENSE_RANK
-- Rank users by age (RANK leaves gaps, DENSE_RANK doesn't)
SELECT name, age,
       RANK() OVER (ORDER BY age DESC) AS rank,
       DENSE_RANK() OVER (ORDER BY age DESC) AS dense_rank
FROM 'data/users.csv'
ORDER BY age DESC
LIMIT 10;

-- Example 3: PARTITION BY
-- Row number within each role group
SELECT role, name, age,
       ROW_NUMBER() OVER (PARTITION BY role ORDER BY age DESC) AS role_rank
FROM 'data/users.csv'
ORDER BY role, role_rank;

-- Example 4: LAG - Compare with previous row
-- Show each person's age and the previous person's age
SELECT name, age,
       LAG(age) OVER (ORDER BY age) AS prev_age,
       age - LAG(age) OVER (ORDER BY age) AS age_diff
FROM 'data/users.csv'
ORDER BY age;

-- Example 5: LEAD - Compare with next row
-- Show each person's age and the next person's age
SELECT name, age,
       LEAD(age) OVER (ORDER BY age) AS next_age,
       LEAD(age) OVER (ORDER BY age) - age AS age_gap
FROM 'data/users.csv'
ORDER BY age;

-- Example 6: Running Sum
-- Calculate running sum of ages ordered by name
SELECT name, age,
       SUM(age) OVER (ORDER BY name) AS running_sum
FROM 'data/users.csv'
ORDER BY name;

-- Example 7: Running Average
-- Calculate running average of ages
SELECT name, age,
       AVG(age) OVER (ORDER BY age) AS running_avg
FROM 'data/users.csv'
ORDER BY age;

-- Example 8: Running Count
-- Count how many rows processed so far
SELECT name, age,
       COUNT(*) OVER (ORDER BY age) AS running_count
FROM 'data/users.csv'
ORDER BY age;

-- Example 9: Multiple Window Functions
-- Combine multiple window functions in one query
SELECT name, age, role,
       ROW_NUMBER() OVER (ORDER BY age) AS overall_rank,
       ROW_NUMBER() OVER (PARTITION BY role ORDER BY age) AS role_rank,
       LAG(age) OVER (ORDER BY age) AS prev_age,
       LEAD(age) OVER (ORDER BY age) AS next_age
FROM 'data/users.csv'
ORDER BY age;
