#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_framework.h"
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

/* external force flag defined in parser.c */
extern bool force_delete;

/* helper function to create a temporary test file */
static void create_test_file(const char* filename, const char* content) {
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}

/* test DELETE without WHERE fails without force flag */
void test_delete_without_where_fails() {
    TEST_START("DELETE without WHERE fails without force flag");
    
    force_delete = false;
    
    ASTNode* ast = parse("DELETE FROM 'data/test.csv'");
    ASSERT_NULL(ast);
    
    TEST_PASS();
}

/* test DELETE without WHERE succeeds with force flag */
void test_delete_without_where_with_force() {
    TEST_START("DELETE without WHERE succeeds with force flag");
    
    const char* test_file = "data/test_force_delete.csv";
    create_test_file(test_file, "id,name,age\n1,Alice,25\n2,Bob,30\n3,Charlie,35\n");
    
    force_delete = true;
    
    ASTNode* ast = parse("DELETE FROM 'data/test_force_delete.csv'");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    /* verify all rows were deleted */
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(0, table->row_count);
    
    csv_free(table);
    unlink(test_file);
    
    force_delete = false;  /* reset */
    
    TEST_PASS();
}

/* test DELETE with WHERE works regardless of force flag */
void test_delete_with_where_always_works() {
    TEST_START("DELETE with WHERE works regardless of force flag");
    
    const char* test_file = "data/test_delete_where.csv";
    create_test_file(test_file, "id,name,age\n1,Alice,25\n2,Bob,30\n3,Charlie,35\n");
    
    force_delete = false;
    
    ASTNode* ast = parse("DELETE FROM 'data/test_delete_where.csv' WHERE age > 30");
    ASSERT_NOT_NULL(ast);
    
    ResultSet* result = evaluate_query(ast);
    ASSERT_NOT_NULL(result);
    
    releaseNode(ast);
    csv_free(result);
    
    /* verify only one row was deleted */
    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load(test_file, config);
    ASSERT_NOT_NULL(table);
    ASSERT_EQUAL(2, table->row_count);
    
    csv_free(table);
    unlink(test_file);
    
    TEST_PASS();
}

int main() {
    printf("\n=== Running Force Flag Tests ===\n\n");
    
    /* DELETE tests */
    test_delete_without_where_fails();
    test_delete_without_where_with_force();
    test_delete_with_where_always_works();
    
    print_test_summary();
    
    return tests_failed > 0 ? 1 : 0;
}
