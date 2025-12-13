#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "tokenizer.h"

void retainToken(Token* token) {
    if (token) {
        token->refcount++;
    }
}

void releaseToken(Token* token) {
    free(((Token*)token)->value);
    free(token);
}

/* helper function to check if a string is a SQL keyword */
static bool is_keyword(const char* str) {
    const char* keywords[] = {
        "SELECT", "DISTINCT", "FROM", "WHERE", "GROUP", "BY", "ORDER", "AND", "OR", 
        "NOT", "IN", "AS", "ASC", "DESC", "HAVING", "JOIN", "LEFT", 
        "RIGHT", "INNER", "OUTER", "FULL", "ON", "LIMIT", "OFFSET", "LIKE", "ILIKE",
        "UNION", "INTERSECT", "EXCEPT", "ALL", 
        "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE", "CREATE", "TABLE", NULL
    };
    
    for (int i = 0; keywords[i] != NULL; i++) {
        if (strcasecmp(str, keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* helper function to duplicate a substring */
static char* xstrndup(const char* str, size_t n) {
    char* result = malloc(n + 1);
    if (result) {
        memcpy(result, str, n);
        result[n] = '\0';
    }
    return result;
}

/* helper function to create a token */
static Token create_token(TokenType type, const char* value) {
    Token token;
    token.refcount = 1;
    token.type = type;
    token.value = strdup(value);
    return token;
}

/* helper function to add a token to the list */
static void add_token(Token** tokens, int* count, int* capacity, Token token) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        *tokens = realloc(*tokens, sizeof(Token) * (*capacity));
    }
    (*tokens)[*count] = token;
    (*count)++;
}

/* helper function to add a single-character token */
static void add_single_char_token(Token** tokens, int* count, int* capacity, TokenType type, char c) {
    char value[2] = {c, '\0'};
    add_token(tokens, count, capacity, create_token(type, value));
}

/* helper function to add a two-character token */
static void add_two_char_token(Token** tokens, int* count, int* capacity, TokenType type, char c1, char c2) {
    char value[3] = {c1, c2, '\0'};
    add_token(tokens, count, capacity, create_token(type, value));
}

/* check if two characters form a two-character operator */
static bool is_two_char_operator(char c1, char c2) {
    return (c1 == '>' && c2 == '=') ||
           (c1 == '<' && c2 == '=') ||
           (c1 == '!' && c2 == '=') ||
           (c1 == '<' && c2 == '>');
}

/* check if character is a single-character operator */
static bool is_operator_char(char c) {
    return c == '=' || c == '>' || c == '<' || c == '+' ||
           c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '&' || c == '|' || c == '^';
}

/* check if character is punctuation */
static bool is_punctuation_char(char c) {
    return c == '(' || c == ')' || c == ',' || c == ';' || c == '.';
}

/* parse string literal (quoted string) */
static const char* parse_string_literal(const char* ptr, Token** tokens, int* count, int* capacity) {
    char quote = *ptr;
    ptr++;
    const char* start = ptr;
    
    while (*ptr != '\0' && *ptr != quote) {
        ptr++;
    }
    
    if (*ptr == quote) {
        char* value = xstrndup(start, ptr - start);
        add_token(tokens, count, capacity, create_token(TOKEN_TYPE_LITERAL, value));
        free(value);
        ptr++;
    }
    
    return ptr;
}

/* parse numeric literal */
static const char* parse_number_literal(const char* ptr, Token** tokens, int* count, int* capacity) {
    const char* start = ptr;
    
    while (isdigit(*ptr) || *ptr == '.') {
        ptr++;
    }
    
    char* value = xstrndup(start, ptr - start);
    add_token(tokens, count, capacity, create_token(TOKEN_TYPE_LITERAL, value));
    free(value);
    
    return ptr;
}

/* parse identifier or keyword */
static const char* parse_identifier(const char* ptr, Token** tokens, int* count, int* capacity) {
    const char* start = ptr;
    
    while (isalnum(*ptr) || *ptr == '_') {
        ptr++;
    }
    
    char* value = xstrndup(start, ptr - start);
    TokenType type = is_keyword(value) ? TOKEN_TYPE_KEYWORD : TOKEN_TYPE_IDENTIFIER;
    add_token(tokens, count, capacity, create_token(type, value));
    free(value);
    
    return ptr;
}

/* this is the tokenize function
 * it takes an input SQL string and produces a list of tokens.
 */
Token* tokenize(const char* sql, int* token_count) {
    if (!sql || !token_count) {
        if (token_count) *token_count = 0;
        return NULL;
    }

    Token* tokens = NULL;
    int count = 0;
    int capacity = 0;
    
    const char* ptr = sql;
    
    while (*ptr != '\0') {
        // skip whitespace
        if (isspace(*ptr)) {
            ptr++;
            continue;
        }
        
        // handle string literals
        if (*ptr == '\'' || *ptr == '"') {
            ptr = parse_string_literal(ptr, &tokens, &count, &capacity);
            continue;
        }
        
        // handle numbers
        if (isdigit(*ptr)) {
            ptr = parse_number_literal(ptr, &tokens, &count, &capacity);
            continue;
        }
        
        // handle identifiers and keywords
        if (isalpha(*ptr) || *ptr == '_') {
            ptr = parse_identifier(ptr, &tokens, &count, &capacity);
            continue;
        }
        
        // handle two-character operators
        if (ptr[0] && ptr[1] && is_two_char_operator(ptr[0], ptr[1])) {
            add_two_char_token(&tokens, &count, &capacity, TOKEN_TYPE_OPERATOR, ptr[0], ptr[1]);
            ptr += 2;
            continue;
        }
        
        // handle single-character operators
        if (is_operator_char(*ptr)) {
            add_single_char_token(&tokens, &count, &capacity, TOKEN_TYPE_OPERATOR, *ptr);
            ptr++;
            continue;
        }
        
        // handle punctuation
        if (is_punctuation_char(*ptr)) {
            add_single_char_token(&tokens, &count, &capacity, TOKEN_TYPE_PUNCTUATION, *ptr);
            ptr++;
            continue;
        }
        
        // unknown character, skip it
        ptr++;
    }
    
    // add EOF token
    add_token(&tokens, &count, &capacity, create_token(TOKEN_TYPE_EOF, ""));
    
    *token_count = count;
    return tokens;
}

/* utility function to print tokens for debugging */
void printTokens(Token* tokens, int token_count) {
    const char* type_names[] = {
        "KEYWORD", "IDENTIFIER", "LITERAL", "OPERATOR", "PUNCTUATION", "EOF"
    };
    
    printf("Tokens (%d):\n", token_count);
    for (int i = 0; i < token_count; i++) {
        printf("  [%d] %s: '%s'\n", i, type_names[tokens[i].type], tokens[i].value);
    }
}

void freeTokens(Token* tokens, int token_count) {
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}