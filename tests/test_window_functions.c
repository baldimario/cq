#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

void test_row_number() {
    printf("Test: ROW_NUMBER() window function...\n");
    
    const char* query = "SELECT name, age, ROW_NUMBER() OVER (ORDER BY age) AS row_num FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // check that row numbers are sequential 1, 2, 3, ... (since we ORDER BY age)
    for (int i = 0; i < result->row_count; i++) {
        assert(result->rows[i].values[2].type == VALUE_TYPE_INTEGER);
        assert(result->rows[i].values[2].int_value == i + 1);
    }
    
    printf("  PASS (tested %d rows)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

void test_row_number_partition() {
    printf("Test: ROW_NUMBER() with PARTITION BY...\n");
    
    const char* query = "SELECT name, role, ROW_NUMBER() OVER (PARTITION BY role ORDER BY age) AS row_num FROM 'data/users.csv'";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // check that each partition has independent row numbers
    // will vary based on data but should have multiple 1s (one per partition)
    int ones_count = 0;
    for (int i = 0; i < result->row_count; i++) {
        if (result->rows[i].values[2].int_value == 1) {
            ones_count++;
        }
    }
    assert(ones_count >= 2); // at least 2 partitions
    
    printf("  PASS (found %d partitions)\n", ones_count);
    csv_free(result);
    releaseNode(ast);
}

void test_rank() {
    printf("Test: RANK() window function...\n");
    
    const char* query = "SELECT name, age, RANK() OVER (ORDER BY age) AS rnk FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // ranks should be monotonically increasing (since ordered by age)
    for (int i = 1; i < result->row_count; i++) {
        assert(result->rows[i].values[2].int_value >= result->rows[i-1].values[2].int_value);
    }
    
    printf("  PASS (%d rows)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

void test_dense_rank() {
    printf("Test: DENSE_RANK() window function...\n");
    
    const char* query = "SELECT name, age, DENSE_RANK() OVER (ORDER BY age) AS drnk FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // dense ranks should not have gaps
    int max_rank = 0;
    for (int i = 0; i < result->row_count; i++) {
        int rank = (int)result->rows[i].values[2].int_value;
        if (rank > max_rank) {
            assert(rank == max_rank + 1); // no gaps
            max_rank = rank;
        }
    }
    
    printf("  PASS (max rank: %d)\n", max_rank);
    csv_free(result);
    releaseNode(ast);
}

void test_lag() {
    printf("Test: LAG() window function with ORDER BY...\n");
    
    const char* query = "SELECT name, age, LAG(age) OVER (ORDER BY age) AS prev_age FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // Verify ORDER BY is working: first row (youngest) should have NULL
    assert(result->rows[0].values[2].type == VALUE_TYPE_NULL);
    
    // Subsequent rows should have LAG values that are <= current age
    for (int i = 1; i < result->row_count; i++) {
        if (result->rows[i].values[2].type != VALUE_TYPE_NULL) {
            long long prev_age = result->rows[i].values[2].int_value;
            long long curr_age = result->rows[i].values[1].int_value;
            assert(prev_age <= curr_age); // LAG should return previous age which is <= current
        }
    }
    
    printf("  PASS (%d rows, ORDER BY working correctly)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

void test_lead() {
    printf("Test: LEAD() window function with ORDER BY...\n");
    
    const char* query = "SELECT name, age, LEAD(age) OVER (ORDER BY age) AS next_age FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 0);
    assert(result->column_count == 3);
    
    // Verify ORDER BY is working: last row (oldest) should have NULL
    assert(result->rows[result->row_count - 1].values[2].type == VALUE_TYPE_NULL);
    
    // Other rows should have LEAD values that are >= current age
    for (int i = 0; i < result->row_count - 1; i++) {
        if (result->rows[i].values[2].type != VALUE_TYPE_NULL) {
            long long next_age = result->rows[i].values[2].int_value;
            long long curr_age = result->rows[i].values[1].int_value;
            assert(next_age >= curr_age); // LEAD should return next age which is >= current
        }
    }
    
    printf("  PASS (%d rows, ORDER BY working correctly)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

void test_sum_over() {
    printf("Test: SUM() OVER window function (running sum)...\n");
    
    const char* query = "SELECT name, age, SUM(age) OVER (ORDER BY age) AS running_sum FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 5); // should have multiple rows
    assert(result->column_count == 3);
    
    // running sum should be monotonically increasing (since ordered by age)
    for (int i = 1; i < result->row_count; i++) {
        if (result->rows[i].values[2].type == VALUE_TYPE_INTEGER) {
            assert(result->rows[i].values[2].int_value >= result->rows[i-1].values[2].int_value);
        } else if (result->rows[i].values[2].type == VALUE_TYPE_DOUBLE) {
            assert(result->rows[i].values[2].double_value >= result->rows[i-1].values[2].double_value);
        }
    }
    
    printf("  PASS (%d rows, progressive running sum)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

void test_count_over() {
    printf("Test: COUNT() OVER window function (running count)...\n");
    
    const char* query = "SELECT name, age, COUNT(*) OVER (ORDER BY age) AS running_count FROM 'data/users.csv' ORDER BY age";
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count > 5); // should have multiple rows
    assert(result->column_count == 3);
    
    // running count should be monotonically increasing and match position
    for (int i = 0; i < result->row_count; i++) {
        assert(result->rows[i].values[2].int_value == i + 1);
    }
    
    printf("  PASS (%d rows, progressive running count)\n", result->row_count);
    csv_free(result);
    releaseNode(ast);
}

int main() {
    printf("=== Window Functions Test Suite ===\n\n");
    
    test_row_number();
    test_row_number_partition();
    test_rank();
    test_dense_rank();
    test_lag();
    test_lead();
    test_sum_over();
    test_count_over();
    
    printf("\n=== All window function tests passed! ===\n");
    return 0;
}
