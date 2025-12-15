#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "test_framework.h"
#include "test_helpers.h"
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"
#include "date_utils.h"

/* test date parsing and formatting */
void test_date_parsing(void) {
    TEST_START("Date parsing - ISO format");
    DateValue date;
    
    // test ISO format (YYYY-MM-DD)
    ASSERT_TRUE(parse_date("2025-12-15", &date) == 1);
    ASSERT_EQUAL(2025, date.year);
    ASSERT_EQUAL(12, date.month);
    ASSERT_EQUAL(15, date.day);
    TEST_PASS();
}

void test_date_formats(void) {
    TEST_START("Date parsing - multiple formats");
    DateValue date;
    
    // test US format (MM/DD/YYYY)
    ASSERT_TRUE(parse_date("12/15/2025", &date) == 1);
    ASSERT_EQUAL(2025, date.year);
    
    // test EU format (DD/MM/YYYY)
    ASSERT_TRUE(parse_date("15/12/2025", &date) == 1);
    ASSERT_EQUAL(2025, date.year);
    TEST_PASS();
}

void test_date_validation(void) {
    TEST_START("Date validation");
    DateValue date;
    
    // test invalid dates
    ASSERT_TRUE(parse_date("2025-13-01", &date) == 0);
    ASSERT_TRUE(parse_date("2025-02-30", &date) == 0);
    
    // test leap year
    ASSERT_TRUE(parse_date("2024-02-29", &date) == 1);
    ASSERT_TRUE(parse_date("2025-02-29", &date) == 0);
    TEST_PASS();
}

/* test date comparisons */
void test_date_comparisons(void) {
    TEST_START("Date comparisons");
    
    DateValue d1 = {2025, 12, 15};
    DateValue d2 = {2025, 12, 15};
    DateValue d3 = {2025, 12, 20};
    DateValue d4 = {2024, 12, 15};
    
    ASSERT_TRUE(compare_dates(d1, d2) == 0);
    ASSERT_TRUE(compare_dates(d1, d3) < 0);
    ASSERT_TRUE(compare_dates(d3, d1) > 0);
    ASSERT_TRUE(compare_dates(d1, d4) > 0);
    TEST_PASS();
}

/* test date arithmetic */
void test_date_arithmetic(void) {
    TEST_START("Date arithmetic - add days");
    
    DateValue date = {2025, 1, 15};
    DateValue result;
    
    // add days
    result = date_add_days(date, 10);
    ASSERT_TRUE(result.year == 2025 && result.month == 1 && result.day == 25);
    
    result = date_add_days(date, 20);
    ASSERT_TRUE(result.year == 2025 && result.month == 2 && result.day == 4);
    TEST_PASS();
}

void test_date_arithmetic_months(void) {
    TEST_START("Date arithmetic - add months");
    
    DateValue date = {2025, 1, 15};
    DateValue result;
    
    // add months
    result = date_add_months(date, 2);
    ASSERT_TRUE(result.year == 2025 && result.month == 3 && result.day == 15);
    
    result = date_add_months(date, 13);
    ASSERT_TRUE(result.year == 2026 && result.month == 2 && result.day == 15);
    TEST_PASS();
}

void test_date_arithmetic_years(void) {
    TEST_START("Date arithmetic - add years");
    
    DateValue date = {2025, 1, 15};
    DateValue result;
    
    // add years
    result = date_add_years(date, 5);
    ASSERT_TRUE(result.year == 2030 && result.month == 1 && result.day == 15);
    TEST_PASS();
}

void test_date_differences(void) {
    TEST_START("Date differences");
    
    // date difference
    DateValue d1 = {2025, 1, 1};
    DateValue d2 = {2025, 1, 11};
    ASSERT_EQUAL(10, date_diff_days(d2, d1));
    
    DateValue d3 = {2025, 6, 1};
    ASSERT_EQUAL(5, date_diff_months(d3, d1));
    
    DateValue d4 = {2030, 1, 1};
    ASSERT_EQUAL(5, date_diff_years(d4, d1));
    TEST_PASS();
}

/* test date SQL functions */
void test_date_sql_functions(void) {
    TEST_START("Date SQL functions - YEAR, MONTH, DAY");
    
    const char* query = "SELECT YEAR(event_date), MONTH(event_date), DAY(event_date) FROM 'data/events.csv' LIMIT 1";
    ASTNode* ast = parse(query);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->row_count > 0);
    ASSERT_EQUAL(VALUE_TYPE_INTEGER, result->rows[0].values[0].type);
    ASSERT_EQUAL(VALUE_TYPE_INTEGER, result->rows[0].values[1].type);
    ASSERT_EQUAL(VALUE_TYPE_INTEGER, result->rows[0].values[2].type);
    
    csv_free(result);
    releaseNode(ast);
    TEST_PASS();
}

void test_date_comparison_where(void) {
    TEST_START("Date comparison in WHERE");
    
    const char* query = "SELECT name FROM 'data/events.csv' WHERE event_date > '2025-03-01'";
    ASTNode* ast = parse(query);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->row_count >= 1); // at least one event after March 1
    
    csv_free(result);
    releaseNode(ast);
    TEST_PASS();
}

/* test date arithmetic functions */
void test_date_add_function(void) {
    TEST_START("DATE_ADD function");
    
    const char* query = "SELECT DATE_ADD(event_date, 30, 'DAYS') FROM 'data/events.csv' LIMIT 1";
    ASTNode* ast = parse(query);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->row_count > 0);
    ASSERT_EQUAL(VALUE_TYPE_DATE, result->rows[0].values[0].type);
    
    csv_free(result);
    releaseNode(ast);
    TEST_PASS();
}

void test_date_diff_function(void) {
    TEST_START("DATE_DIFF function");
    
    const char* query = "SELECT name, DATE_DIFF(event_date, created_at, 'DAYS') as days_until FROM 'data/events.csv' LIMIT 1";
    ASTNode* ast = parse(query);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->row_count > 0);
    ASSERT_EQUAL(VALUE_TYPE_INTEGER, result->rows[0].values[1].type);
    
    csv_free(result);
    releaseNode(ast);
    TEST_PASS();
}

/* test BETWEEN with dates */
void test_date_between(void) {
    TEST_START("BETWEEN with dates");
    
    const char* query = "SELECT name FROM 'data/events.csv' WHERE event_date BETWEEN '2025-02-01' AND '2025-06-30'";
    ASTNode* ast = parse(query);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_TRUE(result->row_count >= 1);
    
    csv_free(result);
    releaseNode(ast);
    TEST_PASS();
}

/* test CSV auto-detection of dates */
void test_csv_date_autodetect(void) {
    TEST_START("CSV date auto-detection");
    
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load("data/events.csv", config);
    ASSERT_NOT_NULL(table);
    
    // check that date columns were detected
    int event_date_col = csv_get_column_index(table, "event_date");
    int created_at_col = csv_get_column_index(table, "created_at");
    
    ASSERT_TRUE(event_date_col >= 0);
    ASSERT_TRUE(created_at_col >= 0);
    
    ASSERT_EQUAL(VALUE_TYPE_DATE, table->columns[event_date_col].inferred_type);
    ASSERT_EQUAL(VALUE_TYPE_DATE, table->columns[created_at_col].inferred_type);
    
    // check first row has date values
    Value* val = csv_get_value(table, 0, event_date_col);
    ASSERT_NOT_NULL(val);
    
    // Debug output
    if (val->type != VALUE_TYPE_DATE) {
        printf("Expected type DATE (%d), got %d\n", VALUE_TYPE_DATE, val->type);
    }
    
    ASSERT_EQUAL(VALUE_TYPE_DATE, val->type);
    ASSERT_EQUAL(2025, val->date_value.year);
    ASSERT_EQUAL(3, val->date_value.month);
    ASSERT_EQUAL(15, val->date_value.day);
    
    csv_free(table);
    TEST_PASS();
}

int main(void) {
    printf("\n=== Date Functions Tests ===\n\n");
    
    test_date_parsing();
    test_date_formats();
    test_date_validation();
    test_date_comparisons();
    test_date_arithmetic();
    test_date_arithmetic_months();
    test_date_arithmetic_years();
    test_date_differences();
    test_date_sql_functions();
    test_date_comparison_where();
    test_date_add_function();
    test_date_diff_function();
    test_date_between();
    test_csv_date_autodetect();
    
    print_test_summary();
    
    return tests_failed > 0 ? 1 : 0;
}
