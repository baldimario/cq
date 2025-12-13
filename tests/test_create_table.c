#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

// Test 1: CREATE TABLE with empty schema (col1, col2, col3)
void test_create_empty_table() {
    TEST_START("CREATE TABLE with empty schema");
    
    const char* test_file = "data/test_empty_table.csv";
    unlink(test_file);
    
    const char* sql = "CREATE TABLE 'data/test_empty_table.csv' (id, name, age)";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    ASSERT_TRUE(ast->type == NODE_TYPE_CREATE_TABLE);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    ASSERT_EQUAL(1, result->row_count);
    
    // verify file was created
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->column_count);
    ASSERT_EQUAL(0, table->row_count);
    ASSERT_TRUE(strcmp(table->columns[0].name, "id") == 0);
    ASSERT_TRUE(strcmp(table->columns[1].name, "name") == 0);
    ASSERT_TRUE(strcmp(table->columns[2].name, "age") == 0);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(test_file);
    
    TEST_PASS();
}

// Test 2: CREATE TABLE AS SELECT (save query results)
void test_create_table_as_select() {
    TEST_START("CREATE TABLE AS SELECT");
    
    const char* source_file = "data/test_create_source.csv";
    const char* dest_file = "data/test_create_dest.csv";
    
    // create source file
    FILE* f = fopen(source_file, "w");
    fprintf(f, "id,name,age\n");
    fprintf(f, "1,Alice,25\n");
    fprintf(f, "2,Bob,30\n");
    fprintf(f, "3,Charlie,35\n");
    fclose(f);
    
    unlink(dest_file);
    
    const char* sql = "CREATE TABLE 'data/test_create_dest.csv' AS "
                      "SELECT * FROM 'data/test_create_source.csv' WHERE age > 25";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    ASSERT_TRUE(ast->type == NODE_TYPE_CREATE_TABLE);
    ASSERT_NOT_NULL(ast->create_table.query);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // Verify dest file was created with filtered data
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(dest_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->column_count);
    ASSERT_EQUAL(2, table->row_count);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(source_file);
    unlink(dest_file);
    
    TEST_PASS();
}

// Test 3: CREATE TABLE AS SELECT with aggregation
void test_create_table_with_aggregation() {
    TEST_START("CREATE TABLE with aggregation");
    
    const char* source_file = "data/test_agg_source.csv";
    const char* dest_file = "data/test_agg_result.csv";
    
    // create source file
    FILE* f = fopen(source_file, "w");
    fprintf(f, "role,age,salary\n");
    fprintf(f, "admin,25,50000\n");
    fprintf(f, "user,30,40000\n");
    fprintf(f, "admin,35,60000\n");
    fprintf(f, "user,28,45000\n");
    fclose(f);
    
    unlink(dest_file);
    
    const char* sql = "CREATE TABLE 'data/test_agg_result.csv' AS "
                      "SELECT role, COUNT(*) as cnt, AVG(age) as avg_age "
                      "FROM 'data/test_agg_source.csv' GROUP BY role";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify result file
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(dest_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->column_count);
    ASSERT_EQUAL(2, table->row_count);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(source_file);
    unlink(dest_file);
    
    TEST_PASS();
}

// Test 4: CREATE TABLE AS with ORDER BY and LIMIT
void test_create_table_with_order_limit() {
    TEST_START("CREATE TABLE with ORDER BY and LIMIT");
    
    const char* source_file = "data/test_order_source.csv";
    const char* dest_file = "data/test_order_result.csv";
    
    // create source file
    FILE* f = fopen(source_file, "w");
    fprintf(f, "id,value\n");
    fprintf(f, "3,300\n");
    fprintf(f, "1,100\n");
    fprintf(f, "4,400\n");
    fprintf(f, "2,200\n");
    fclose(f);
    
    unlink(dest_file);
    
    const char* sql = "CREATE TABLE 'data/test_order_result.csv' AS "
                      "SELECT * FROM 'data/test_order_source.csv' ORDER BY id LIMIT 2";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify result file has top 2 ordered rows
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(dest_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(source_file);
    unlink(dest_file);
    
    TEST_PASS();
}

// Test 5: CREATE TABLE AS (SELECT ...) with parentheses
void test_create_table_as_subquery() {
    TEST_START("CREATE TABLE AS (SELECT)");
    
    const char* source_file = "data/test_sub_source.csv";
    const char* dest_file = "data/test_sub_dest.csv";
    
    // create source file
    FILE* f = fopen(source_file, "w");
    fprintf(f, "id,value\n");
    fprintf(f, "1,100\n");
    fprintf(f, "2,200\n");
    fprintf(f, "3,300\n");
    fclose(f);
    
    unlink(dest_file);
    
    const char* sql = "CREATE TABLE 'data/test_sub_dest.csv' AS "
                      "(SELECT * FROM 'data/test_sub_source.csv' WHERE value > 100)";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify result
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(dest_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(source_file);
    unlink(dest_file);
    
    TEST_PASS();
}

// Test 6: CREATE TABLE replacing existing file
void test_create_table_replace_existing() {
    TEST_START("CREATE TABLE replacing existing file");
    
    const char* file = "data/test_replace.csv";
    
    // create initial file
    FILE* f = fopen(file, "w");
    fprintf(f, "old_col\n");
    fprintf(f, "old_data\n");
    fclose(f);
    
    // create new table with different schema
    const char* sql = "CREATE TABLE 'data/test_replace.csv' (new_col1, new_col2)";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify file was replaced
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->column_count);
    ASSERT_EQUAL(0, table->row_count);
    ASSERT_TRUE(strcmp(table->columns[0].name, "new_col1") == 0);
    ASSERT_TRUE(strcmp(table->columns[1].name, "new_col2") == 0);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(file);
    
    TEST_PASS();
}

// Test 7: CREATE TABLE with complex SELECT (JOIN)
void test_create_table_with_join() {
    TEST_START("CREATE TABLE with JOIN");
    
    const char* users_file = "data/test_users_join.csv";
    const char* roles_file = "data/test_roles_join.csv";
    const char* dest_file = "data/test_join_result.csv";
    
    // create source files
    FILE* f = fopen(users_file, "w");
    fprintf(f, "id,name,role_id\n");
    fprintf(f, "1,Alice,2\n");
    fprintf(f, "2,Bob,1\n");
    fclose(f);
    
    f = fopen(roles_file, "w");
    fprintf(f, "id,role_name\n");
    fprintf(f, "1,user\n");
    fprintf(f, "2,admin\n");
    fclose(f);
    
    unlink(dest_file);
    
    const char* sql = "CREATE TABLE 'data/test_join_result.csv' AS "
                      "SELECT u.name, r.role_name "
                      "FROM 'data/test_users_join.csv' AS u "
                      "INNER JOIN 'data/test_roles_join.csv' AS r ON u.role_id = r.id";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify result
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(dest_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    ASSERT_EQUAL(2, table->column_count);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(users_file);
    unlink(roles_file);
    unlink(dest_file);
    
    TEST_PASS();
}

// Test 8: CREATE TABLE AS schema mapping (col1, col2, col3)
void test_create_table_schema_mapping() {
    TEST_START("CREATE TABLE AS (columns)");
    
    const char* file = "data/test_schema_map.csv";
    unlink(file);
    
    // CREATE TABLE AS (col1, col2) for schema definition
    const char* sql = "CREATE TABLE 'data/test_schema_map.csv' AS (col_a, col_b, col_c)";
    ASTNode* ast = parse(sql);
    ASSERT_NOT_NULL(ast);
    ASSERT_TRUE(ast->create_table.is_schema_only);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify file created with schema
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->column_count);
    ASSERT_EQUAL(0, table->row_count);
    ASSERT_TRUE(strcmp(table->columns[0].name, "col_a") == 0);
    ASSERT_TRUE(strcmp(table->columns[1].name, "col_b") == 0);
    ASSERT_TRUE(strcmp(table->columns[2].name, "col_c") == 0);
    
    csv_free(table);
    csv_free(result);
    releaseNode(ast);
    unlink(file);
    
    TEST_PASS();
}

int main() {
    test_create_empty_table();
    test_create_table_as_select();
    test_create_table_with_aggregation();
    test_create_table_with_order_limit();
    test_create_table_as_subquery();
    test_create_table_replace_existing();
    test_create_table_with_join();
    test_create_table_schema_mapping();
    
    print_test_summary();
    return (tests_failed == 0) ? 0 : 1;
}
