#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

void test_between_integers() {
    printf("Testing BETWEEN with integers...\n");
    
    // create test CSV
    FILE* f = fopen("test_between_int.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,20\n");
    fprintf(f, "Bob,25\n");
    fprintf(f, "Charlie,30\n");
    fprintf(f, "Diana,35\n");
    fprintf(f, "Eve,40\n");
    fclose(f);
    
    const char* query = "SELECT name, age FROM 'test_between_int.csv' WHERE age BETWEEN 25 AND 35";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 3); // Bob (25), Charlie (30), Diana (35)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_between_int.csv");
    printf("✓ BETWEEN with integers passed\n");
}

void test_between_strings() {
    printf("Testing BETWEEN with strings...\n");
    
    // create test CSV
    FILE* f = fopen("test_between_str.csv", "w");
    fprintf(f, "name\n");
    fprintf(f, "Alice\n");
    fprintf(f, "Bob\n");
    fprintf(f, "Charlie\n");
    fprintf(f, "Diana\n");
    fprintf(f, "Eve\n");
    fclose(f);
    
    const char* query = "SELECT name FROM 'test_between_str.csv' WHERE name BETWEEN 'Alice' AND 'Charlie'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 3); // Alice, Bob, Charlie
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_between_str.csv");
    printf("✓ BETWEEN with strings passed\n");
}

void test_between_with_expressions() {
    printf("Testing BETWEEN with arithmetic expressions...\n");
    
    // create test CSV
    FILE* f = fopen("test_between_expr.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,20\n");
    fprintf(f, "Bob,25\n");
    fprintf(f, "Charlie,30\n");
    fprintf(f, "Diana,35\n");
    fprintf(f, "Eve,40\n");
    fclose(f);
    
    const char* query = "SELECT name, age FROM 'test_between_expr.csv' WHERE age * 2 BETWEEN 50 AND 70";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 3); // Bob (50), Charlie (60), Diana (70)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_between_expr.csv");
    printf("✓ BETWEEN with expressions passed\n");
}

void test_between_boundary_inclusive() {
    printf("Testing BETWEEN boundary inclusiveness...\n");
    
    // create test CSV
    FILE* f = fopen("test_between_boundary.csv", "w");
    fprintf(f, "value\n");
    fprintf(f, "10\n");
    fprintf(f, "20\n");
    fprintf(f, "30\n");
    fprintf(f, "40\n");
    fprintf(f, "50\n");
    fclose(f);
    
    const char* query = "SELECT value FROM 'test_between_boundary.csv' WHERE value BETWEEN 20 AND 40";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 3); // 20, 30, 40 (inclusive on both ends)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_between_boundary.csv");
    printf("✓ BETWEEN boundary inclusiveness passed\n");
}

int main() {
    printf("\n=== BETWEEN Operator Tests ===\n\n");
    
    test_between_integers();
    test_between_strings();
    test_between_with_expressions();
    test_between_boundary_inclusive();
    
    printf("\n✓ All BETWEEN tests passed!\n");
    return 0;
}
