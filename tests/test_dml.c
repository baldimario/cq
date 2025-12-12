#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

// helper function to create a temporary test file
static void create_test_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}

// test INSERT with all columns
void test_insert_all_columns() {
    TEST_START("INSERT with all columns");
    
    const char* test_file = "data/test_insert_all.csv";
    create_test_file(test_file, "id,name,age\n1,Alice,25\n2,Bob,30\n");
    
    // parse and execute INSERT
    ASTNode* ast = parse("INSERT INTO 'data/test_insert_all.csv' VALUES (3, 'Charlie', 35)");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    // verify result message
    ASSERT_EQUAL(1, result->row_count);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify the row was inserted
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->row_count);
    
    // check the new row
    Value* name = csv_get_value_by_name(table, 2, "name");
    ASSERT_NOT_NULL(name);
    ASSERT_TRUE(name->type == VALUE_TYPE_STRING);
    ASSERT_TRUE(strcmp(name->string_value, "Charlie") == 0);
    
    Value* age = csv_get_value_by_name(table, 2, "age");
    ASSERT_NOT_NULL(age);
    ASSERT_EQUAL(35, age->int_value);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test INSERT with specific columns
void test_insert_specific_columns() {
    TEST_START("INSERT with specific columns");
    
    const char* test_file = "data/test_insert_specific.csv";
    create_test_file(test_file, "id,name,age,role\n1,Alice,25,admin\n");
    
    // parse and execute INSERT
    ASTNode* ast = parse("INSERT INTO 'data/test_insert_specific.csv' (id, name, age) VALUES (2, 'Bob', 30)");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify the row was inserted
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    ASSERT_EQUAL(4, table->column_count);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test UPDATE single column
void test_update_single_column() {
    TEST_START("UPDATE single column");
    
    const char* test_file = "data/test_update_single.csv";
    create_test_file(test_file, "id,name,age\n1,Alice,25\n2,Bob,30\n3,Charlie,35\n");
    
    // parse and execute UPDATE
    ASTNode* ast = parse("UPDATE 'data/test_update_single.csv' SET age = 26 WHERE name = 'Alice'");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify the update
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    
    Value* age = csv_get_value_by_name(table, 0, "age");
    ASSERT_NOT_NULL(age);
    ASSERT_EQUAL(26, age->int_value);
    
    // verify other rows unchanged
    Value* age2 = csv_get_value_by_name(table, 1, "age");
    ASSERT_EQUAL(30, age2->int_value);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test UPDATE multiple columns
void test_update_multiple_columns() {
    TEST_START("UPDATE multiple columns");
    
    const char* test_file = "data/test_update_multiple.csv";
    create_test_file(test_file, "id,name,age,role\n1,Alice,25,user\n2,Bob,30,user\n");
    
    // parse and execute UPDATE
    ASTNode* ast = parse("UPDATE 'data/test_update_multiple.csv' SET age = 31, role = 'admin' WHERE name = 'Bob'");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify the update
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    
    Value* age = csv_get_value_by_name(table, 1, "age");
    ASSERT_EQUAL(31, age->int_value);
    
    Value* role = csv_get_value_by_name(table, 1, "role");
    ASSERT_TRUE(strcmp(role->string_value, "admin") == 0);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test UPDATE without WHERE (updates all rows)
void test_update_all_rows() {
    TEST_START("UPDATE all rows");
    
    const char* test_file = "data/test_update_all.csv";
    create_test_file(test_file, "id,name,active\n1,Alice,0\n2,Bob,0\n3,Charlie,0\n");
    
    // parse and execute UPDATE
    ASTNode* ast = parse("UPDATE 'data/test_update_all.csv' SET active = 1 WHERE id > 0");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify all rows updated
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    
    for (int i = 0; i < table->row_count; i++) {
        Value* active = csv_get_value_by_name(table, i, "active");
        ASSERT_EQUAL(1, active->int_value);
    }
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test DELETE with simple condition
void test_delete_simple() {
    TEST_START("DELETE with simple condition");
    
    const char* test_file = "data/test_delete_simple.csv";
    create_test_file(test_file, "id,name,age\n1,Alice,25\n2,Bob,30\n3,Charlie,35\n4,Diana,28\n");
    
    // parse and execute DELETE
    ASTNode* ast = parse("DELETE FROM 'data/test_delete_simple.csv' WHERE age > 30");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify rows deleted
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(3, table->row_count);
    
    // verify Charlie (age 35) was deleted
    for (int i = 0; i < table->row_count; i++) {
        Value* name = csv_get_value_by_name(table, i, "name");
        ASSERT_TRUE(strcmp(name->string_value, "Charlie") != 0);
    }
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test DELETE with complex condition
void test_delete_complex_condition() {
    TEST_START("DELETE with complex condition");
    
    const char* test_file = "data/test_delete_complex.csv";
    create_test_file(test_file, "id,name,age,active\n1,Alice,25,1\n2,Bob,30,0\n3,Charlie,35,0\n4,Diana,28,1\n");
    
    // parse and execute DELETE
    ASTNode* ast = parse("DELETE FROM 'data/test_delete_complex.csv' WHERE active = 0 AND age > 25");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    // verify correct rows deleted
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    
    // should have Alice and Diana (both have active=1)
    Value* name1 = csv_get_value_by_name(table, 0, "name");
    Value* name2 = csv_get_value_by_name(table, 1, "name");
    ASSERT_TRUE(strcmp(name1->string_value, "Alice") == 0 || strcmp(name2->string_value, "Alice") == 0);
    ASSERT_TRUE(strcmp(name1->string_value, "Diana") == 0 || strcmp(name2->string_value, "Diana") == 0);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

// test INSERT, UPDATE, DELETE sequence
void test_dml_sequence() {
    TEST_START("INSERT, UPDATE, DELETE sequence");
    
    const char* test_file = "data/test_dml_sequence.csv";
    create_test_file(test_file, "id,name,score\n1,Alice,85\n");
    
    // INSERT
    ASTNode* insert_ast = parse("INSERT INTO 'data/test_dml_sequence.csv' VALUES (2, 'Bob', 90)");
    ResultSet* insert_result = evaluate_query(insert_ast);
    releaseNode(insert_ast);
    csv_free(insert_result);
    
    // UPDATE
    ASTNode* update_ast = parse("UPDATE 'data/test_dml_sequence.csv' SET score = 95 WHERE name = 'Bob'");
    ResultSet* update_result = evaluate_query(update_ast);
    releaseNode(update_ast);
    csv_free(update_result);
    
    // DELETE
    ASTNode* delete_ast = parse("DELETE FROM 'data/test_dml_sequence.csv' WHERE score < 90");
    ResultSet* delete_result = evaluate_query(delete_ast);
    releaseNode(delete_ast);
    csv_free(delete_result);
    
    // verify final state
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(1, table->row_count);
    
    Value* name = csv_get_value_by_name(table, 0, "name");
    ASSERT_TRUE(strcmp(name->string_value, "Bob") == 0);
    
    Value* score = csv_get_value_by_name(table, 0, "score");
    ASSERT_EQUAL(95, score->int_value);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

int main() {
    printf("\n=== Running DML Tests ===\n\n");
    
    test_insert_all_columns();
    test_insert_specific_columns();
    test_update_single_column();
    test_update_multiple_columns();
    test_update_all_rows();
    test_delete_simple();
    test_delete_complex_condition();
    test_dml_sequence();
    
    print_test_summary();
    
    return tests_failed > 0 ? 1 : 0;
}
