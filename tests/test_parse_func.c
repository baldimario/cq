#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokenizer.h"
#include "parser.h"

int main() {
    printf("=== Testing parse_function_call directly ===\n\n");
    
    const char* sql = "ROW_NUMBER() OVER (ORDER BY age) FROM users.csv";
    int token_count = 0;
    Token* tokens = tokenize(sql, &token_count);
    
    printf("Tokens:\n");
    for (int i = 0; i < token_count; i++) {
        printf("  %d: type=%d, value='%s'\n", i, tokens[i].type, tokens[i].value);
    }
    
    Parser* parser = parser_init(tokens, token_count);
    
    printf("\nCalling parse_expression...\n");
    ASTNode* expr = parse_expression(parser);
    
    if (expr) {
        printf("✓ parse_expression returned a node\n");
        printf("  Node type: %d\n", expr->type);
        
        if (expr->type == NODE_TYPE_WINDOW_FUNCTION) {
            printf("  ✓ It's a WINDOW_FUNCTION!\n");
            printf("  Function name: %s\n", expr->window_function.name);
            if (expr->window_function.order_by_column) {
                printf("  ORDER BY: %s\n", expr->window_function.order_by_column);
            }
        } else if (expr->type == NODE_TYPE_FUNCTION) {
            printf("  ✗ It's a regular FUNCTION (not window)\n");
        } else {
            printf("  ✗ It's some other type\n");
        }
        
        printf("\nCurrent token after parsing: '%s' (pos=%d)\n", 
               parser_current_token(parser)->value, parser->current_pos);
        
        releaseNode(expr);
    } else {
        printf("✗ parse_expression returned NULL\n");
    }
    
    parser_free(parser);
    freeTokens(tokens, token_count);
    
    return 0;
}
