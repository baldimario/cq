#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenizer.h"
#include "parser.h"

int main() {
    printf("=== Testing Window Function Parsing ===\n\n");
    
    // Test 1: tokenizer recognizes OVER
    printf("Test 1: Tokenizer recognizes OVER keyword\n");
    const char* sql1 = "SELECT ROW_NUMBER() OVER (ORDER BY age)";
    int token_count = 0;
    Token* tokens = tokenize(sql1, &token_count);
    
    for (int i = 0; i < token_count; i++) {
        printf("  Token %d: type=%d, value='%s'\n", 
               i, tokens[i].type, tokens[i].value);
    }
    
    bool found_over = false;
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_TYPE_KEYWORD && 
            strcmp(tokens[i].value, "OVER") == 0) {
            found_over = true;
            break;
        }
    }
    
    if (found_over) {
        printf("  ✓ OVER recognized as keyword\n\n");
    } else {
        printf("  ✗ OVER NOT recognized as keyword\n\n");
    }
    
    freeTokens(tokens, token_count);
    
    // Test 2: parser handles window function
    printf("Test 2: Parser handles window function\n");
    const char* sql2 = "SELECT ROW_NUMBER() OVER (ORDER BY age) FROM users.csv";
    ASTNode* ast = parse(sql2);
    
    if (ast) {
        printf("  ✓ Query parsed successfully\n");
        
        if (ast->type == NODE_TYPE_QUERY) {
            printf("  ✓ Root node is QUERY\n");
            
            if (ast->query.select && ast->query.select->type == NODE_TYPE_SELECT) {
                printf("  ✓ Has SELECT node\n");
                printf("  Column count: %d\n", ast->query.select->select.column_count);
                
                if (ast->query.select->select.column_count > 0) {
                    printf("  First column string: '%s'\n", ast->query.select->select.columns[0]);
                    
                    if (ast->query.select->select.column_nodes && 
                        ast->query.select->select.column_nodes[0]) {
                        ASTNode* col = ast->query.select->select.column_nodes[0];
                        printf("  Column type: %d (WINDOW_FUNCTION=%d)\n", col->type, NODE_TYPE_WINDOW_FUNCTION);
                        
                        if (col->type == NODE_TYPE_WINDOW_FUNCTION) {
                            printf("  ✓ Column is WINDOW_FUNCTION\n");
                            printf("  Function name: %s\n", col->window_function.name);
                            
                            if (col->window_function.order_by_column) {
                                printf("  ✓ Has ORDER BY: %s\n", col->window_function.order_by_column);
                            } else {
                                printf("  ✗ Missing ORDER BY\n");
                            }
                        } else if (col->type == NODE_TYPE_FUNCTION) {
                            printf("  ✗ Column is FUNCTION (not WINDOW_FUNCTION)\n");
                            printf("  Function name: %s\n", col->function.name);
                        } else {
                            printf("  ✗ Column is NOT WINDOW_FUNCTION (type=%d)\n", col->type);
                        }
                    } else {
                        printf("  ✗ Column node is NULL\n");
                    }
                } else {
                    printf("  ✗ No columns in SELECT\n");
                }
            } else {
                printf("  ✗ No SELECT node or wrong type\n");
            }
        } else {
            printf("  ✗ Root node is not QUERY (type=%d)\n", ast->type);
        }
        
        releaseNode(ast);
    } else {
        printf("  ✗ Failed to parse query\n");
    }
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
