#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "parser.h"
#include "evaluator.h"
#include "csv_reader.h"

void test_line_comment() {
    printf("Testing line comments (--)...\n");
    
    // create test CSV
    FILE* f = fopen("test_line_comment.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,20\n");
    fprintf(f, "Bob,25\n");
    fclose(f);
    
    const char* query = 
        "-- This is a comment\n"
        "SELECT name FROM 'test_line_comment.csv' -- inline comment\n"
        "WHERE age > 20 -- another comment";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1); // Only Bob (age > 20)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_line_comment.csv");
    printf("✓ Line comments passed\n");
}

void test_block_comment() {
    printf("Testing block comments (/* */)...\n");
    
    // create test CSV
    FILE* f = fopen("test_block_comment.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,20\n");
    fprintf(f, "Bob,25\n");
    fclose(f);
    
    const char* query = 
        "/* This is a\n"
        "   multi-line comment */\n"
        "SELECT name /* inline block */ FROM 'test_block_comment.csv'\n"
        "WHERE age > /* comment in condition */ 20";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1); // only Bob (age > 20)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_block_comment.csv");
    printf("✓ Block comments passed\n");
}

void test_mixed_comments() {
    printf("Testing mixed comments...\n");
    
    // create test CSV
    FILE* f = fopen("test_mixed_comment.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,20\n");
    fprintf(f, "Bob,25\n");
    fclose(f);
    
    const char* query = 
        "/* Block comment */\n"
        "-- Line comment\n"
        "SELECT name FROM 'test_mixed_comment.csv' -- select users\n"
        "/* WHERE condition */ WHERE age > 20";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1); // only Bob (age > 20)
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_mixed_comment.csv");
    printf("✓ Mixed comments passed\n");
}

void test_comment_at_end_of_file() {
    printf("Testing comment at end of file...\n");
    
    // create test CSV
    FILE* f = fopen("test_comment_eof.csv", "w");
    fprintf(f, "name\n");
    fprintf(f, "Alice\n");
    fclose(f);
    
    const char* query = 
        "SELECT name FROM 'test_comment_eof.csv' -- comment at end";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_comment_eof.csv");
    printf("✓ Comment at end of file passed\n");
}

void test_multiline_block_comment() {
    printf("Testing multi-line block comment...\n");
    
    // create test CSV
    FILE* f = fopen("test_multiline_comment.csv", "w");
    fprintf(f, "name,age\n");
    fprintf(f, "Alice,25\n");
    fclose(f);
    
    const char* query = 
        "/* This is a very long\n"
        "   multi-line comment that\n"
        "   spans several lines\n"
        "   and contains useful information */\n"
        "SELECT name FROM 'test_multiline_comment.csv'";
    
    ASTNode* ast = parse(query);
    assert(ast != NULL);
    
    ResultSet* result = evaluate_query(ast);
    assert(result != NULL);
    assert(result->row_count == 1);
    
    csv_free(result);
    releaseNode(ast);
    
    remove("test_multiline_comment.csv");
    printf("✓ Multi-line block comment passed\n");
}

int main() {
    printf("\n=== SQL Comments Tests ===\n\n");
    
    test_line_comment();
    test_block_comment();
    test_mixed_comments();
    test_comment_at_end_of_file();
    test_multiline_block_comment();
    
    printf("\n✓ All SQL comment tests passed!\n");
    return 0;
}
