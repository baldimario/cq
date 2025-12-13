-- Example: Complex query with multiple features
-- Demonstrates JOINs, aggregations, and filtering

/*
 * This query combines multiple SQL features:
 * - JOIN operations
 * - Aggregate functions
 * - WHERE clause with comparisons
 * - GROUP BY and HAVING
 * - ORDER BY with LIMIT
 */

SELECT 
    u.role,
    COUNT(*) AS user_count,
    AVG(u.age) AS avg_age,
    STDDEV(u.age) AS age_stddev,
    MEDIAN(u.age) AS median_age,
    MIN(u.height) AS min_height,
    MAX(u.height) AS max_height
FROM './data/users.csv' AS u
WHERE u.active = 1  -- Only active users
  AND u.age BETWEEN 20 AND 50  -- Working age range
GROUP BY u.role
HAVING COUNT(*) >= 2  -- Roles with at least 2 users
ORDER BY user_count DESC, avg_age ASC
LIMIT 5;

-- Alternative: Statistical summary for all users
-- SELECT 
--     AVG(age) AS avg_age,
--     STDDEV(age) AS age_stddev,
--     MEDIAN(age) AS median_age,
--     MIN(age) AS min_age,
--     MAX(age) AS max_age
-- FROM './data/users.csv';

-- Alternative: Simple aggregation by role
-- SELECT role, COUNT(*) as total
-- FROM './data/users.csv'
-- GROUP BY role
-- ORDER BY total DESC;
