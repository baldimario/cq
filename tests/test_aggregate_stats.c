#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

void test_stddev_basic() {
    printf("Testing STDDEV basic...\n");
    
    // create test CSV with known values
    FILE* f = fopen("test_stddev.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "10\n");
    fprintf(f, "20\n");
    fprintf(f, "30\n");
    fprintf(f, "40\n");
    fprintf(f, "50\n");
    fclose(f);
    
    const char* query = "SELECT STDDEV(value) AS stdev FROM 'test_stddev.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    // expected stddev for [10, 20, 30, 40, 50]
    // mean = 30, Variance = 200, StdDev = sqrt(200) ≈ 14.14
    double stdev = result->rows[0].values[0].double_value;
    assert(fabs(stdev - 14.142135) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_stddev.csv");
    printf("✓ STDDEV basic passed (%.2f)\n", stdev);
}

void test_stddev_with_group_by() {
    printf("Testing STDDEV with GROUP BY...\n");
    
    FILE* f = fopen("test_stddev_group.csv", "w");
    fprintf(f, "category,value\n");
    fprintf(f, "A,10\n");
    fprintf(f, "A,20\n");
    fprintf(f, "A,30\n");
    fprintf(f, "B,100\n");
    fprintf(f, "B,200\n");
    fclose(f);
    
    const char* query = "SELECT category, STDDEV(value) AS stdev FROM 'test_stddev_group.csv' GROUP BY category ORDER BY category";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 2);
    
    // category A: [10, 20, 30] -> mean=20, variance=66.67, stddev≈8.16
    // category B: [100, 200] -> mean=150, variance=2500, stddev=50
    double stdev_a = result->rows[0].values[1].double_value;
    double stdev_b = result->rows[1].values[1].double_value;
    
    assert(fabs(stdev_a - 8.164965) < 0.001);
    assert(fabs(stdev_b - 50.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_stddev_group.csv");
    printf("✓ STDDEV with GROUP BY passed (A=%.2f, B=%.2f)\n", stdev_a, stdev_b);
}

void test_median_odd_count() {
    printf("Testing MEDIAN with odd count...\n");
    
    FILE* f = fopen("test_median_odd.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "10\n");
    fprintf(f, "30\n");
    fprintf(f, "20\n");
    fprintf(f, "50\n");
    fprintf(f, "40\n");
    fclose(f);
    
    const char* query = "SELECT MEDIAN(value) AS median FROM 'test_median_odd.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    // sorted: [10, 20, 30, 40, 50] -> median = 30
    double median = result->rows[0].values[0].double_value;
    assert(fabs(median - 30.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_median_odd.csv");
    printf("✓ MEDIAN with odd count passed (%.1f)\n", median);
}

void test_median_even_count() {
    printf("Testing MEDIAN with even count...\n");
    
    FILE* f = fopen("test_median_even.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "10\n");
    fprintf(f, "40\n");
    fprintf(f, "20\n");
    fprintf(f, "30\n");
    fclose(f);
    
    const char* query = "SELECT MEDIAN(value) AS median FROM 'test_median_even.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    // sorted: [10, 20, 30, 40] -> median = (20 + 30) / 2 = 25
    double median = result->rows[0].values[0].double_value;
    assert(fabs(median - 25.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_median_even.csv");
    printf("✓ MEDIAN with even count passed (%.1f)\n", median);
}

void test_median_with_group_by() {
    printf("Testing MEDIAN with GROUP BY...\n");
    
    FILE* f = fopen("test_median_group.csv", "w");
    fprintf(f, "category,value\n");
    fprintf(f, "A,10\n");
    fprintf(f, "A,30\n");
    fprintf(f, "A,20\n");
    fprintf(f, "B,100\n");
    fprintf(f, "B,300\n");
    fprintf(f, "B,200\n");
    fprintf(f, "B,400\n");
    fclose(f);
    
    const char* query = "SELECT category, MEDIAN(value) AS median FROM 'test_median_group.csv' GROUP BY category ORDER BY category";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 2);
    
    // category A: [10, 20, 30] -> median = 20
    // category B: [100, 200, 300, 400] -> median = (200 + 300) / 2 = 250
    double median_a = result->rows[0].values[1].double_value;
    double median_b = result->rows[1].values[1].double_value;
    
    assert(fabs(median_a - 20.0) < 0.001);
    assert(fabs(median_b - 250.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_median_group.csv");
    printf("✓ MEDIAN with GROUP BY passed (A=%.1f, B=%.1f)\n", median_a, median_b);
}

void test_combined_aggregates() {
    printf("Testing combined aggregates (AVG, STDDEV, MEDIAN)...\n");
    
    FILE* f = fopen("test_combined_agg.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "10\n");
    fprintf(f, "20\n");
    fprintf(f, "30\n");
    fprintf(f, "40\n");
    fprintf(f, "50\n");
    fclose(f);
    
    const char* query = "SELECT AVG(value) AS avg, STDDEV(value) AS stdev, MEDIAN(value) AS median FROM 'test_combined_agg.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    double avg = result->rows[0].values[0].double_value;
    double stdev = result->rows[0].values[1].double_value;
    double median = result->rows[0].values[2].double_value;
    
    assert(fabs(avg - 30.0) < 0.001);
    assert(fabs(stdev - 14.142135) < 0.001);
    assert(fabs(median - 30.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_combined_agg.csv");
    printf("✓ Combined aggregates passed (avg=%.1f, stdev=%.2f, median=%.1f)\n", avg, stdev, median);
}

void test_stddev_single_value() {
    printf("Testing STDDEV with single value...\n");
    
    FILE* f = fopen("test_stddev_single.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "42\n");
    fclose(f);
    
    const char* query = "SELECT STDDEV(value) AS stdev FROM 'test_stddev_single.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    // single value should have stddev = 0
    double stdev = result->rows[0].values[0].double_value;
    assert(fabs(stdev - 0.0) < 0.001);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_stddev_single.csv");
    printf("✓ STDDEV with single value passed (%.1f)\n", stdev);
}

int main() {
    printf("\n=== Statistical Aggregate Functions Tests ===\n\n");
    
    test_stddev_basic();
    test_stddev_with_group_by();
    test_median_odd_count();
    test_median_even_count();
    test_median_with_group_by();
    test_combined_aggregates();
    test_stddev_single_value();
    
    printf("\n✓ All statistical aggregate tests passed!\n");
    return 0;
}
