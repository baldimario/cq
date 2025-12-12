#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "tokenizer.h"


typedef enum {
    NODE_TYPE_QUERY,
    NODE_TYPE_SELECT,
    NODE_TYPE_FROM,
    NODE_TYPE_JOIN,
    NODE_TYPE_WHERE,
    NODE_TYPE_GROUP_BY,
    NODE_TYPE_ORDER_BY,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_CONDITION,
    NODE_TYPE_LITERAL,
    NODE_TYPE_IDENTIFIER,
    NODE_TYPE_ALIAS,
    NODE_TYPE_LIST,
    NODE_TYPE_SUBQUERY,
    NODE_TYPE_BINARY_OP,
    NODE_TYPE_SET_OP,
    NODE_TYPE_INSERT,
    NODE_TYPE_UPDATE,
    NODE_TYPE_DELETE,
    NODE_TYPE_ASSIGNMENT,
} ASTNodeType;

typedef enum {
    JOIN_TYPE_INNER,
    JOIN_TYPE_LEFT,
    JOIN_TYPE_RIGHT,
    JOIN_TYPE_FULL,
} JoinType;

typedef enum {
    SET_OP_UNION,
    SET_OP_UNION_ALL,
    SET_OP_INTERSECT,
    SET_OP_EXCEPT,
} SetOpType;

/* forward declaration */
typedef struct ASTNode ASTNode;

struct ASTNode {
    int refcount;
    ASTNodeType type;
    union {
        struct {
            ASTNode* select;
            ASTNode* from;
            ASTNode** joins;      // array of JOIN nodes
            int join_count;
            ASTNode* where;
            ASTNode* group_by;
            ASTNode* having;
            ASTNode* order_by;
            int limit;           // -1 means no limit
            int offset;          // -1 means no offset
        } query;

        struct {
            char** columns;           // string representation for backward compatibility
            ASTNode** column_nodes;   // AST representation for subqueries and complex expressions
            int column_count;
            bool distinct;            // true if SELECT DISTINCT
        } select;

        struct {
            ASTNode* left;
            ASTNode* right;
            char* operator;
        } condition;

        struct {
            char* name;
            ASTNode** args;
            int arg_count;
        } function;

        struct {
            ASTNode** nodes;
            int node_count;
        } list;
        
        struct {
            char* column;
            bool descending;
        } order_by;
        
        struct {
            char* table;  // filename or table name (NULL if subquery)
            ASTNode* subquery;  // subquery node (NULL if table)
            char* alias;  // optional alias
        } from;
        
        struct {
            JoinType join_type;
            char* table;       // right table filename
            char* alias;       // optional alias for right table
            ASTNode* condition; // ON condition
        } join;
        
        struct {
            ASTNode* query;  // nested query
        } subquery;

        struct {
            ASTNode* left;
            ASTNode* right;
            char* operator;  // +, -, *, /
        } binary_op;

        struct {
            SetOpType op_type;
            ASTNode* left;   // left query
            ASTNode* right;  // right query
        } set_op;

        struct {
            char* table;           // target CSV file
            char** columns;        // column names (NULL if not specified)
            int column_count;
            ASTNode** values;      // values to insert
            int value_count;
        } insert;

        struct {
            char* table;           // target CSV file
            ASTNode** assignments; // array of ASSIGNMENT nodes
            int assignment_count;
            ASTNode* where;        // WHERE condition (optional)
        } update;

        struct {
            char* table;           // target CSV file
            ASTNode* where;        // WHERE condition (required for safety)
        } delete_stmt;

        struct {
            char* column;          // column name
            ASTNode* value;        // value to assign
        } assignment;

        char* literal;
        char* identifier;
        char* alias;
    };
};

/* as we will use reference counting for memory management, we need utility functions to handle it */
void retainNode(ASTNode* node);
void releaseNode(ASTNode* node);


/* parser state structure */
typedef struct {
    Token* tokens;
    int token_count;
    int current_pos;
} Parser;

/* main parsing function */
ASTNode* parse(const char* sql);

/* parser initialization and cleanup */
Parser* parser_init(Token* tokens, int token_count);
void parser_free(Parser* parser);

/* helper functions */
Token* parser_current_token(Parser* parser);
Token* parser_peek_token(Parser* parser, int offset);
void parser_advance(Parser* parser);
int parser_match(Parser* parser, TokenType type, const char* value);
int parser_expect(Parser* parser, TokenType type, const char* value);

/* parsing functions for different SQL components */
ASTNode* parse_select(Parser* parser);
ASTNode* parse_where(Parser* parser);
ASTNode* parse_group_by(Parser* parser);
ASTNode* parse_order_by(Parser* parser);
ASTNode* parse_expression(Parser* parser);
ASTNode* parse_condition(Parser* parser);
ASTNode* parse_primary(Parser* parser);
ASTNode* parse_insert(Parser* parser);
ASTNode* parse_update(Parser* parser);
ASTNode* parse_delete(Parser* parser);
void printAst(ASTNode* node, int depth);

#endif