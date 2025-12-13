#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "parser.h"
#include "tokenizer.h"
#include "string_utils.h"


/* forward prototype declarations */
static ASTNode* parse_query_internal(Parser* parser);
static ASTNode* parse_additive_expr(Parser* parser);
static ASTNode* parse_logical_continuation(Parser* parser, ASTNode* left_condition);
static void* ensure_capacity(void* array, int* capacity, int current_count, size_t element_size);
static void parse_limit_offset(Parser* parser, int* limit, int* offset);

void retainNode(ASTNode* node) {
    if (node) {
        node->refcount++;
    }
}

void releaseNode(ASTNode* node) {
    if (!node) return;
    
    node->refcount--;
    if (node->refcount > 0) return;
    
    switch (node->type) {
        case NODE_TYPE_QUERY:
            releaseNode(node->query.select);
            releaseNode(node->query.from);
            if (node->query.joins) {
                for (int i = 0; i < node->query.join_count; i++) {
                    releaseNode(node->query.joins[i]);
                }
                free(node->query.joins);
            }
            releaseNode(node->query.where);
            releaseNode(node->query.group_by);
            releaseNode(node->query.having);
            releaseNode(node->query.order_by);
            break;
        case NODE_TYPE_SELECT:
            if (node->select.columns) {
                for (int i = 0; i < node->select.column_count; i++) {
                    free(node->select.columns[i]);
                }
                free(node->select.columns);
            }
            if (node->select.column_nodes) {
                for (int i = 0; i < node->select.column_count; i++) {
                    releaseNode(node->select.column_nodes[i]);
                }
                free(node->select.column_nodes);
            }
            break;
        case NODE_TYPE_CONDITION:
            releaseNode(node->condition.left);
            releaseNode(node->condition.right);
            free(node->condition.operator);
            break;
        case NODE_TYPE_FUNCTION:
            if (node->function.args) {
                for (int i = 0; i < node->function.arg_count; i++) {
                    releaseNode(node->function.args[i]);
                }
                free(node->function.args);
            }
            free(node->function.name);
            break;
        case NODE_TYPE_LIST:
            if (node->list.nodes) {
                for (int i = 0; i < node->list.node_count; i++) {
                    releaseNode(node->list.nodes[i]);
                }
                free(node->list.nodes);
            }
            break;
        case NODE_TYPE_ORDER_BY:
            free(node->order_by.column);
            break;
        case NODE_TYPE_FROM:
            free(node->from.table);
            releaseNode(node->from.subquery);
            free(node->from.alias);
            break;
        case NODE_TYPE_JOIN:
            free(node->join.table);
            free(node->join.alias);
            releaseNode(node->join.condition);
            break;
        case NODE_TYPE_SUBQUERY:
            releaseNode(node->subquery.query);
            break;
        case NODE_TYPE_BINARY_OP:
            releaseNode(node->binary_op.left);
            releaseNode(node->binary_op.right);
            free(node->binary_op.operator);
            break;
        case NODE_TYPE_SET_OP:
            releaseNode(node->set_op.left);
            releaseNode(node->set_op.right);
            break;
        case NODE_TYPE_INSERT:
            free(node->insert.table);
            if (node->insert.columns) {
                for (int i = 0; i < node->insert.column_count; i++) {
                    free(node->insert.columns[i]);
                }
                free(node->insert.columns);
            }
            if (node->insert.values) {
                for (int i = 0; i < node->insert.value_count; i++) {
                    releaseNode(node->insert.values[i]);
                }
                free(node->insert.values);
            }
            break;
        case NODE_TYPE_UPDATE:
            free(node->update.table);
            if (node->update.assignments) {
                for (int i = 0; i < node->update.assignment_count; i++) {
                    releaseNode(node->update.assignments[i]);
                }
                free(node->update.assignments);
            }
            releaseNode(node->update.where);
            break;
        case NODE_TYPE_DELETE:
            free(node->delete_stmt.table);
            releaseNode(node->delete_stmt.where);
            break;
        case NODE_TYPE_CREATE_TABLE:
            free(node->create_table.table);
            if (node->create_table.columns) {
                for (int i = 0; i < node->create_table.column_count; i++) {
                    free(node->create_table.columns[i]);
                }
                free(node->create_table.columns);
            }
            releaseNode(node->create_table.query);
            break;
        case NODE_TYPE_ALTER_TABLE:
            free(node->alter_table.table);
            free(node->alter_table.old_column_name);
            free(node->alter_table.new_column_name);
            break;
        case NODE_TYPE_ASSIGNMENT:
            free(node->assignment.column);
            releaseNode(node->assignment.value);
            break;
        case NODE_TYPE_LITERAL:
            free(node->literal);
            break;
        case NODE_TYPE_IDENTIFIER:
            free(node->identifier);
            break;
        case NODE_TYPE_ALIAS:
            free(node->alias);
            break;
        default:
            break;
    }
    free(node);
}


Parser* parser_init(Token* tokens, int token_count) {
    Parser* parser = malloc(sizeof(Parser));
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current_pos = 0;
    return parser;
}

void parser_free(Parser* parser) {
    free(parser);
}

Token* parser_current_token(Parser* parser) {
    if (parser->current_pos >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1]; // Return EOF
    }
    return &parser->tokens[parser->current_pos];
}

Token* parser_peek_token(Parser* parser, int offset) {
    int pos = parser->current_pos + offset;
    if (pos >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1]; // Return EOF
    }
    return &parser->tokens[pos];
}

void parser_advance(Parser* parser) {
    if (parser->current_pos < parser->token_count - 1) {
        parser->current_pos++;
    }
}

int parser_match(Parser* parser, TokenType type, const char* value) {
    Token* token = parser_current_token(parser);
    if (token->type != type) {
        return 0;
    }
    if (value && strcasecmp(token->value, value) != 0) {
        return 0;
    }
    return 1;
}

int parser_expect(Parser* parser, TokenType type, const char* value) {
    if (!parser_match(parser, type, value)) {
        fprintf(stderr, "Parse error: expected %s but got %s\n", 
                value ? value : "token", parser_current_token(parser)->value);
        return 0;
    }
    parser_advance(parser);
    return 1;
}

/* node creation helpers */
static ASTNode* create_node(ASTNodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    node->type = type;
    node->refcount = 1;
    return node;
}

static ASTNode* create_identifier_node(const char* name) {
    ASTNode* node = create_node(NODE_TYPE_IDENTIFIER);
    node->identifier = strdup(name);
    return node;
}

static ASTNode* create_literal_node(const char* value) {
    ASTNode* node = create_node(NODE_TYPE_LITERAL);
    node->literal = strdup(value);
    return node;
}

static ASTNode* create_condition_node(ASTNode* left, const char* op, ASTNode* right) {
    ASTNode* node = create_node(NODE_TYPE_CONDITION);
    node->condition.left = left;
    node->condition.operator = strdup(op);
    node->condition.right = right;
    return node;
}

static ASTNode* create_binary_op_node(ASTNode* left, const char* op, ASTNode* right) {
    ASTNode* node = create_node(NODE_TYPE_BINARY_OP);
    node->binary_op.left = left;
    node->binary_op.operator = strdup(op);
    node->binary_op.right = right;
    return node;
}

/* helper: parse qualified identifier (e.g., table.column) */
static char* parse_qualified_identifier(Parser* parser) {
    Token* token = parser_current_token(parser);
    if (token->type != TOKEN_TYPE_IDENTIFIER) {
        return NULL;
    }
    
    char buffer[256];
    int buf_pos = snprintf(buffer, sizeof(buffer), "%s", token->value);
    parser_advance(parser);
    
    // check for dot notation
    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ".")) {
        parser_advance(parser); // skip '.'
        Token* col_token = parser_current_token(parser);
        if (col_token->type == TOKEN_TYPE_IDENTIFIER) {
            buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ".%s", col_token->value);
            parser_advance(parser);
        }
    }
    
    return strdup(buffer);
}

/* helper: check if current token is a logical operator (AND/OR) and parse right side */
static ASTNode* parse_logical_continuation(Parser* parser, ASTNode* left_condition) {
    Token* token = parser_current_token(parser);
    if (token->type == TOKEN_TYPE_KEYWORD && 
        (strcasecmp(token->value, "AND") == 0 || strcasecmp(token->value, "OR") == 0)) {
        char* logical_op = token->value;
        parser_advance(parser);
        ASTNode* right = parse_condition(parser);
        return create_condition_node(left_condition, logical_op, right);
    }
    return left_condition;
}

/* helper: ensure dynamic array has enough capacity, realloc if needed */
static void* ensure_capacity(void* array, int* capacity, int current_count, size_t element_size) {
    if (current_count >= *capacity) {
        *capacity *= 2;
        return realloc(array, element_size * (*capacity));
    }
    return array;
}

/* helper: parse table name or filename (handles quoted paths and .csv extensions) */
static char* parse_table_name(Parser* parser) {
    Token* token = parser_current_token(parser);
    
    if (token->type == TOKEN_TYPE_LITERAL) {
        // quoted filename
        char* table = strdup(token->value);
        parser_advance(parser);
        return table;
    } else if (token->type == TOKEN_TYPE_IDENTIFIER) {
        // unquoted identifier with optional extension
        return parse_qualified_identifier(parser);
    }
    
    return NULL;
}

/* helper: parse optional alias (with or without AS keyword) */
static char* parse_optional_alias(Parser* parser, const char** excluded_keywords, int excluded_count) {
    Token* token = parser_current_token(parser);
    
    // check for AS keyword
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "AS")) {
        parser_advance(parser);
        token = parser_current_token(parser);
        if (token->type == TOKEN_TYPE_IDENTIFIER) {
            char* alias = strdup(token->value);
            parser_advance(parser);
            return alias;
        }
        return NULL;
    }
    
    // check for alias without AS (but not if it's a SQL keyword)
    if (token->type == TOKEN_TYPE_IDENTIFIER) {
        for (int i = 0; i < excluded_count; i++) {
            if (strcasecmp(token->value, excluded_keywords[i]) == 0) {
                return NULL; // it's a SQL keyword, not an alias
            }
        }
        char* alias = strdup(token->value);
        parser_advance(parser);
        return alias;
    }
    
    return NULL;
}

/* helper to parse JOIN type keyword (LEFT/RIGHT/FULL/INNER with optional OUTER) */
static JoinType parse_join_type(Parser* parser) {
    Token* token = parser_current_token(parser);
    
    if (token->type != TOKEN_TYPE_KEYWORD) {
        return JOIN_TYPE_INNER;
    }
    
    JoinType join_type = JOIN_TYPE_INNER;
    
    if (strcasecmp(token->value, "LEFT") == 0) {
        join_type = JOIN_TYPE_LEFT;
        parser_advance(parser);
    } else if (strcasecmp(token->value, "RIGHT") == 0) {
        join_type = JOIN_TYPE_RIGHT;
        parser_advance(parser);
    } else if (strcasecmp(token->value, "FULL") == 0) {
        join_type = JOIN_TYPE_FULL;
        parser_advance(parser);
    } else if (strcasecmp(token->value, "INNER") == 0) {
        join_type = JOIN_TYPE_INNER;
        parser_advance(parser);
    } else {
        return JOIN_TYPE_INNER; // default if no type specified
    }
    
    // check for optional OUTER keyword
    token = parser_current_token(parser);
    if (token->type == TOKEN_TYPE_KEYWORD && strcasecmp(token->value, "OUTER") == 0) {
        parser_advance(parser);
    }
    
    return join_type;
}

/* helper to build function call string for ORDER BY, legacy string-based approach */
static char* build_function_string(Parser* parser) {
    Token* token = parser_current_token(parser);
    if (token->type != TOKEN_TYPE_IDENTIFIER) {
        return NULL;
    }
    
    Token* next = parser_peek_token(parser, 1);
    if (next->type != TOKEN_TYPE_PUNCTUATION || strcmp(next->value, "(") != 0) {
        return NULL;
    }
    
    char buffer[256];
    int buf_pos = snprintf(buffer, sizeof(buffer), "%s(", token->value);
    parser_advance(parser); // function name
    parser_advance(parser); // '('
    
    // capture function arguments
    bool first_arg = true;
    while (!parser_match(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
        Token* arg_token = parser_current_token(parser);
        
        if (strcmp(arg_token->value, ",") == 0) {
            buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ", ");
            parser_advance(parser);
        } else if (arg_token->type == TOKEN_TYPE_IDENTIFIER) {
            if (!first_arg) {
                buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ", ");
            }
            
            // build qualified identifier if needed
            buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, "%s", arg_token->value);
            parser_advance(parser);
            
            // check for dot notation
            if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ".")) {
                buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ".");
                parser_advance(parser);
                Token* col_token = parser_current_token(parser);
                if (col_token->type == TOKEN_TYPE_IDENTIFIER) {
                    buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, "%s", col_token->value);
                    parser_advance(parser);
                }
            }
            
            first_arg = false;
        } else {
            // other tokens (literals, etc.)
            if (!first_arg) {
                buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ", ");
            }
            buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, "%s", arg_token->value);
            parser_advance(parser);
            first_arg = false;
        }
    }
    buf_pos += snprintf(buffer + buf_pos, sizeof(buffer) - buf_pos, ")");
    parser_advance(parser); // ')'
    
    return strdup(buffer);
}

/* helper: parse function call handling arguments and nested expressions */
static ASTNode* parse_function_call(Parser* parser, bool use_full_parser) {
    Token* token = parser_current_token(parser);
    if (token->type != TOKEN_TYPE_IDENTIFIER) {
        return NULL;
    }
    
    Token* next = parser_peek_token(parser, 1);
    if (next->type != TOKEN_TYPE_PUNCTUATION || strcmp(next->value, "(") != 0) {
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_FUNCTION);
    node->function.name = strdup(token->value);
    parser_advance(parser); // skip function name
    parser_advance(parser); // skip '('
    
    // parse arguments
    int capacity = 4;
    node->function.args = malloc(sizeof(ASTNode*) * capacity);
    node->function.arg_count = 0;
    
    while (!parser_match(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
        node->function.args = ensure_capacity(node->function.args, &capacity, 
                                             node->function.arg_count, sizeof(ASTNode*));
        
        // special handling for * in aggregate functions like COUNT(*)
        Token* current = parser_current_token(parser);
        if (current->type == TOKEN_TYPE_OPERATOR && strcmp(current->value, "*") == 0) {
            ASTNode* star_node = create_node(NODE_TYPE_IDENTIFIER);
            star_node->identifier = strdup("*");
            node->function.args[node->function.arg_count++] = star_node;
            parser_advance(parser);
        } else {
            // use appropriate parser based on context
            if (use_full_parser) {
                node->function.args[node->function.arg_count++] = parse_expression(parser);
            } else {
                node->function.args[node->function.arg_count++] = parse_additive_expr(parser);
            }
        }
        
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
            parser_advance(parser);
        }
    }
    parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
    return node;
}

// generate a column name from an AST node for display
static void generate_column_name(ASTNode* node, char* buf, size_t buf_size) {
    if (!node || !buf || buf_size == 0) return;
    
    switch (node->type) {
        case NODE_TYPE_IDENTIFIER:
            if (node->identifier) {
                snprintf(buf, buf_size, "%s", node->identifier);
            } else {
                snprintf(buf, buf_size, "*");
            }
            break;
            
        case NODE_TYPE_LITERAL:
            snprintf(buf, buf_size, "%s", node->literal);
            break;
            
        case NODE_TYPE_FUNCTION: {
            // generate function call string FUNC(args...)
            char args_str[256] = "";
            for (int i = 0; i < node->function.arg_count; i++) {
                if (node->function.args[i] == NULL) {
                    // handle NULL argument, it shouldn't happen, but let's be defensive
                    if (i > 0) cq_strlcat(args_str, ", ", sizeof(args_str));
                    cq_strlcat(args_str, "NULL", sizeof(args_str));
                    continue;
                }
                char arg_buf[128];
                generate_column_name(node->function.args[i], arg_buf, sizeof(arg_buf));
                if (i > 0) cq_strlcat(args_str, ", ", sizeof(args_str));
                cq_strlcat(args_str, arg_buf, sizeof(args_str));
            }
            snprintf(buf, buf_size, "%s(%s)", node->function.name, args_str);
            break;
        }
            
        case NODE_TYPE_BINARY_OP: {
            // generate arithmetic expression: left op right (or op right for unary)
            char left_str[256] = "", right_str[256] = "";
            
            // handle unary operator (left is NULL)
            if (!node->binary_op.left) {
                generate_column_name(node->binary_op.right, right_str, sizeof(right_str));
                bool right_complex = (node->binary_op.right && node->binary_op.right->type == NODE_TYPE_BINARY_OP);
                if (right_complex) {
                    snprintf(buf, buf_size, "%s(%s)", node->binary_op.operator, right_str);
                } else {
                    snprintf(buf, buf_size, "%s%s", node->binary_op.operator, right_str);
                }
                break;
            }
            
            generate_column_name(node->binary_op.left, left_str, sizeof(left_str));
            generate_column_name(node->binary_op.right, right_str, sizeof(right_str));
            
            // add parentheses for complex sub-expressions
            bool left_complex = (node->binary_op.left->type == NODE_TYPE_BINARY_OP);
            bool right_complex = (node->binary_op.right->type == NODE_TYPE_BINARY_OP);
            
            if (left_complex && right_complex) {
                snprintf(buf, buf_size, "(%s) %s (%s)", left_str, node->binary_op.operator, right_str);
            } else if (left_complex) {
                snprintf(buf, buf_size, "(%s) %s %s", left_str, node->binary_op.operator, right_str);
            } else if (right_complex) {
                snprintf(buf, buf_size, "%s %s (%s)", left_str, node->binary_op.operator, right_str);
            } else {
                snprintf(buf, buf_size, "%s %s %s", left_str, node->binary_op.operator, right_str);
            }
            break;
        }
            
        case NODE_TYPE_SUBQUERY:
            snprintf(buf, buf_size, "(subquery)");
            break;
            
        default:
            snprintf(buf, buf_size, "expr");
            break;
    }
}

/* function for parsing the primary expression */
ASTNode* parse_primary(Parser* parser) {
    Token* token = parser_current_token(parser);
    
    // handle parentheses, it could be a subquery or expression
    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
        parser_advance(parser);
        
        // check if this is a scalar subquery, it starts with SELECT
        Token* next = parser_current_token(parser);
        if (next->type == TOKEN_TYPE_KEYWORD && strcasecmp(next->value, "SELECT") == 0) {
            // parse as scalar subquery
            ASTNode* subquery_node = create_node(NODE_TYPE_SUBQUERY);
            subquery_node->subquery.query = parse_query_internal(parser);
            parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
            return subquery_node;
        }
        
        // otherwise, parse as regular expression/condition
        ASTNode* expr = parse_condition(parser);
        parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
        return expr;
    }
    
    // handle function calls
    ASTNode* func = parse_function_call(parser, true);
    if (func) return func;
    
    // handle identifiers including qualified identifiers like table.column
    if (token->type == TOKEN_TYPE_IDENTIFIER) {
        char* identifier = parse_qualified_identifier(parser);
        if (identifier) {
            return create_identifier_node(identifier);
        }
    }
    
    // handle literals
    if (token->type == TOKEN_TYPE_LITERAL) {
        parser_advance(parser);
        return create_literal_node(token->value);
    }
    
    // handle * for COUNT(*)
    if (token->type == TOKEN_TYPE_OPERATOR && strcmp(token->value, "*") == 0) {
        parser_advance(parser);
        return create_literal_node("*");
    }
    
    return NULL;
}

/* ===== Arithmetic Expression Parsing (with operator precedence) ===== */

// forward declarations for recursive descent parser
static ASTNode* parse_bitwise_expr(Parser* parser);
static ASTNode* parse_additive_expr(Parser* parser);
static ASTNode* parse_multiplicative_expr(Parser* parser);
static ASTNode* parse_arithmetic_primary(Parser* parser);

/* parse term handling *, /, % */
static ASTNode* parse_multiplicative_expr(Parser* parser) {
    ASTNode* left = parse_arithmetic_primary(parser);
    
    while (1) {
        Token* token = parser_current_token(parser);
        if (token->type == TOKEN_TYPE_OPERATOR && 
            (strcmp(token->value, "*") == 0 || strcmp(token->value, "/") == 0 || strcmp(token->value, "%") == 0)) {
            char* op = token->value;
            parser_advance(parser);
            ASTNode* right = parse_arithmetic_primary(parser);
            left = create_binary_op_node(left, op, right);
        } else {
            break;
        }
    }
    
    return left;
}

/* parse expression handling +, - */
static ASTNode* parse_additive_expr(Parser* parser) {
    ASTNode* left = parse_multiplicative_expr(parser);
    
    while (1) {
        Token* token = parser_current_token(parser);
        if (token->type == TOKEN_TYPE_OPERATOR && 
            (strcmp(token->value, "+") == 0 || strcmp(token->value, "-") == 0)) {
            char* op = token->value;
            parser_advance(parser);
            ASTNode* right = parse_multiplicative_expr(parser);
            left = create_binary_op_node(left, op, right);
        } else {
            break;
        }
    }
    
    return left;
}

/* parse bitwise operations handling &, | and ^ with lowest precedence for arithmetic */
static ASTNode* parse_bitwise_expr(Parser* parser) {
    ASTNode* left = parse_additive_expr(parser);
    
    while (1) {
        Token* token = parser_current_token(parser);
        if (token->type == TOKEN_TYPE_OPERATOR && 
            (strcmp(token->value, "&") == 0 || strcmp(token->value, "|") == 0 || strcmp(token->value, "^") == 0)) {
            char* op = token->value;
            parser_advance(parser);
            ASTNode* right = parse_additive_expr(parser);
            left = create_binary_op_node(left, op, right);
        } else {
            break;
        }
    }
    
    return left;
}

/* parse primary arithmetic expression for number, identifier, function call, or parenthesized expression */
static ASTNode* parse_arithmetic_primary(Parser* parser) {
    Token* token = parser_current_token(parser);
    
    // handle unary operators (- and +)
    if (token->type == TOKEN_TYPE_OPERATOR && 
        (strcmp(token->value, "-") == 0 || strcmp(token->value, "+") == 0)) {
        char* op = token->value;
        parser_advance(parser);
        ASTNode* operand = parse_arithmetic_primary(parser);
        // create a binary_op node with NULL left to indicate unary operator
        return create_binary_op_node(NULL, op, operand);
    }
    
    // handle parentheses
    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
        parser_advance(parser);
        
        // check if this is a scalar subquery
        Token* next = parser_current_token(parser);
        if (next->type == TOKEN_TYPE_KEYWORD && strcasecmp(next->value, "SELECT") == 0) {
            ASTNode* subquery_node = create_node(NODE_TYPE_SUBQUERY);
            subquery_node->subquery.query = parse_query_internal(parser);
            parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
            return subquery_node;
        }
        
        // otherwise parse as arithmetic expression respecting all precedence levels
        ASTNode* expr = parse_bitwise_expr(parser);
        parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
        return expr;
    }
    
    // handle function calls
    ASTNode* func = parse_function_call(parser, false);
    if (func) return func;
    
    // handle identifiers including qualified identifiers like table.column
    if (token->type == TOKEN_TYPE_IDENTIFIER) {
        char* identifier = parse_qualified_identifier(parser);
        if (identifier) {
            return create_identifier_node(identifier);
        }
    }
    
    // handle literals
    if (token->type == TOKEN_TYPE_LITERAL) {
        parser_advance(parser);
        return create_literal_node(token->value);
    }
    
    return NULL;
}

/* parse arithmetic expression, entry point for expressions
 * start with bitwise to include all precedence levels */
ASTNode* parse_expression(Parser* parser) {
    return parse_bitwise_expr(parser);
}

/* condition parsing for WHERE clause */
ASTNode* parse_condition(Parser* parser) {
    // handle NOT operator
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "NOT")) {
        parser_advance(parser);
        
        // check if followed by parentheses for grouped condition
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
            parser_advance(parser);
            ASTNode* condition = parse_condition(parser);
            parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
            return create_condition_node(condition, "NOT", NULL);
        }
        
        ASTNode* condition = parse_condition(parser);
        // create a NOT condition node using "NOT" as operator with NULL right side
        return create_condition_node(condition, "NOT", NULL);
    }
    
    // parse left side that can be arithmetic expression with parentheses
    ASTNode* left = parse_expression(parser);
    
    // check for comparison operator
    Token* token = parser_current_token(parser);
    
    // check for NOT IN pattern
    bool is_not_in = false;
    if (token->type == TOKEN_TYPE_KEYWORD && strcasecmp(token->value, "NOT") == 0) {
        Token* next = parser_peek_token(parser, 1);
        if (next->type == TOKEN_TYPE_KEYWORD && strcasecmp(next->value, "IN") == 0) {
            is_not_in = true;
            parser_advance(parser); // skip NOT
            token = parser_current_token(parser); // now on IN
        }
    }
    
    // check for BETWEEN operator
    if (token->type == TOKEN_TYPE_KEYWORD && strcasecmp(token->value, "BETWEEN") == 0) {
        parser_advance(parser);
        
        // parse lower bound
        ASTNode* lower = parse_expression(parser);
        
        // expect AND keyword
        if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "AND")) {
            releaseNode(left);
            releaseNode(lower);
            return NULL;
        }
        
        // parse upper bound
        ASTNode* upper = parse_expression(parser);
        
        // create condition: left >= lower AND left <= upper
        // we need to duplicate left for the second comparison
        ASTNode* left_copy = NULL;
        if (left->type == NODE_TYPE_IDENTIFIER) {
            left_copy = create_identifier_node(left->identifier);
        } else if (left->type == NODE_TYPE_LITERAL) {
            left_copy = create_literal_node(left->literal);
        } else if (left->type == NODE_TYPE_BINARY_OP) {
            // for complex expressions, just reuse with incremented refcount
            left_copy = left;
            retainNode(left_copy);
        } else if (left->type == NODE_TYPE_FUNCTION) {
            // for function calls, reuse with incremented refcount
            left_copy = left;
            retainNode(left_copy);
        } else {
            // fallback: reuse node with incremented refcount
            left_copy = left;
            retainNode(left_copy);
        }
        
        ASTNode* cond_lower = create_condition_node(left, ">=", lower);
        ASTNode* cond_upper = create_condition_node(left_copy, "<=", upper);
        ASTNode* condition = create_condition_node(cond_lower, "AND", cond_upper);
        
        return parse_logical_continuation(parser, condition);
    }
    
    if (token->type == TOKEN_TYPE_OPERATOR || 
        (token->type == TOKEN_TYPE_KEYWORD && 
         (strcasecmp(token->value, "IN") == 0 || 
          strcasecmp(token->value, "LIKE") == 0 || 
          strcasecmp(token->value, "ILIKE") == 0))) {
        char* op = is_not_in ? "NOT IN" : token->value;
        parser_advance(parser);
        
        // handle IN clause with list or subquery
        if ((strcasecmp(token->value, "IN") == 0) && parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
            parser_advance(parser);
            
            // check if it's a subquery, it starts with SELECT
            Token* peek = parser_current_token(parser);
            if (peek->type == TOKEN_TYPE_KEYWORD && strcasecmp(peek->value, "SELECT") == 0) {
                // it's a subquery
                ASTNode* subquery_query = parse_query_internal(parser);
                if (!subquery_query) {
                    releaseNode(left);
                    return NULL;
                }
                
                if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
                    releaseNode(subquery_query);
                    releaseNode(left);
                    return NULL;
                }
                
                // create subquery node
                ASTNode* subquery_node = create_node(NODE_TYPE_SUBQUERY);
                subquery_node->subquery.query = subquery_query;
                
                ASTNode* condition = create_condition_node(left, op, subquery_node);
                return parse_logical_continuation(parser, condition);
            } else {
                // it's a list of values
                ASTNode* list_node = create_node(NODE_TYPE_LIST);
                
                int capacity = 4;
                list_node->list.nodes = malloc(sizeof(ASTNode*) * capacity);
                list_node->list.node_count = 0;
                
                while (!parser_match(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
                    if (list_node->list.node_count >= capacity) {
                        capacity *= 2;
                        list_node->list.nodes = realloc(list_node->list.nodes, sizeof(ASTNode*) * capacity);
                    }
                    list_node->list.nodes[list_node->list.node_count++] = parse_expression(parser);
                    
                    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                        parser_advance(parser);
                    }
                }
                parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
                
                ASTNode* condition = create_condition_node(left, op, list_node);
                return parse_logical_continuation(parser, condition);
            }
        }
        
        // normal comparison (can be arithmetic expression)
        ASTNode* right = parse_expression(parser);
        ASTNode* condition = create_condition_node(left, op, right);
        return parse_logical_continuation(parser, condition);
    }
    
    return left;
}
    
/* select clause parsing */
ASTNode* parse_select(Parser* parser) {
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "SELECT")) {
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_SELECT);
    
    // check for DISTINCT keyword
    node->select.distinct = false;
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "DISTINCT")) {
        node->select.distinct = true;
        parser_advance(parser);
    }
    
    int capacity = 4;
    node->select.columns = malloc(sizeof(char*) * capacity);
    node->select.column_nodes = malloc(sizeof(ASTNode*) * capacity);
    node->select.column_count = 0;
    
    // parse column list
    while (1) {
        Token* token = parser_current_token(parser);
        
        // resize arrays if needed
        node->select.columns = ensure_capacity(node->select.columns, &capacity, 
                                               node->select.column_count, sizeof(char*));
        node->select.column_nodes = ensure_capacity(node->select.column_nodes, &capacity, 
                                                    node->select.column_count, sizeof(ASTNode*));
        
        // check for scalar subquery: SELECT ...
        if (token->type == TOKEN_TYPE_PUNCTUATION && strcmp(token->value, "(") == 0) {
            Token* next = parser_peek_token(parser, 1);
            if (next->type == TOKEN_TYPE_KEYWORD && strcasecmp(next->value, "SELECT") == 0) {
                // scalar subquery in SELECT list
                parser_advance(parser); // skip '('
                
                ASTNode* subquery_node = create_node(NODE_TYPE_SUBQUERY);
                subquery_node->subquery.query = parse_query_internal(parser);
                parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")");
                
                // check for AS alias
                char alias_buf[256] = "";
                if (parser_match(parser, TOKEN_TYPE_KEYWORD, "AS")) {
                    parser_advance(parser);
                    Token* alias_token = parser_current_token(parser);
                    if (alias_token->type == TOKEN_TYPE_IDENTIFIER) {
                        snprintf(alias_buf, sizeof(alias_buf), " AS %s", alias_token->value);
                        parser_advance(parser);
                    }
                }
                
                // store the subquery node and a placeholder string
                node->select.column_nodes[node->select.column_count] = subquery_node;
                char col_str[512];
                snprintf(col_str, sizeof(col_str), "(subquery)%s", alias_buf);
                node->select.columns[node->select.column_count] = strdup(col_str);
                node->select.column_count++;
                
                if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                    parser_advance(parser);
                } else {
                    break;
                }
                continue;
            }
        }
        
        // check for SELECT *
        if (token->type == TOKEN_TYPE_OPERATOR && strcmp(token->value, "*") == 0) {
            node->select.columns[node->select.column_count] = strdup("*");
            node->select.column_nodes[node->select.column_count] = NULL;
            node->select.column_count++;
            parser_advance(parser);
            
            if (!parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                break;
            }
            parser_advance(parser); // skip comma
            continue;
        }
        
        // parse general expression handling identifiers, functions, arithmetic, etc.
        ASTNode* expr_node = parse_expression(parser);
        if (!expr_node) break;
        
        // generate column name from expression
        char col_str[512];
        generate_column_name(expr_node, col_str, sizeof(col_str));
        
        // check for AS alias
        if (parser_match(parser, TOKEN_TYPE_KEYWORD, "AS")) {
            parser_advance(parser);
            Token* alias_token = parser_current_token(parser);
            if (alias_token->type == TOKEN_TYPE_IDENTIFIER) {
                // append alias in format " AS alias_name" for evaluator compatibility
                char temp[512];
                snprintf(temp, sizeof(temp), "%s AS %s", col_str, alias_token->value);
                strncpy(col_str, temp, sizeof(col_str) - 1);
                col_str[sizeof(col_str) - 1] = '\0';
                parser_advance(parser);
            }
        }
        
        node->select.columns[node->select.column_count] = strdup(col_str);
        node->select.column_nodes[node->select.column_count] = expr_node;
        node->select.column_count++;
        
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
            parser_advance(parser);
        } else {
            break;
        }
    }
    
    return node;
}

/* from clause parsing */
ASTNode* parse_from(Parser* parser) {
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "FROM")) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = create_node(NODE_TYPE_FROM);
    node->from.table = NULL;
    node->from.subquery = NULL;
    node->from.alias = NULL;
    
    Token* token = parser_current_token(parser);
    
    // check for subquery (parenthesized SELECT)
    if (token->type == TOKEN_TYPE_PUNCTUATION && strcmp(token->value, "(") == 0) {
        parser_advance(parser); // skip '('
        
        // parse the subquery as a full query
        ASTNode* subquery_query = parse_query_internal(parser);
        
        if (!subquery_query) {
            releaseNode(node);
            return NULL;
        }
        
        // expect closing parenthesis
        if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
            releaseNode(subquery_query);
            releaseNode(node);
            return NULL;
        }
        
        // create subquery node and attach the query
        ASTNode* subquery_node = create_node(NODE_TYPE_SUBQUERY);
        subquery_node->subquery.query = subquery_query;
        node->from.subquery = subquery_node;
        
        // subqueries in FROM must have an alias
        token = parser_current_token(parser);
        if (parser_match(parser, TOKEN_TYPE_KEYWORD, "AS")) {
            parser_advance(parser);
            token = parser_current_token(parser);
        }
        
        if (token->type == TOKEN_TYPE_IDENTIFIER) {
            node->from.alias = strdup(token->value);
            parser_advance(parser);
        } else {
            // subquery requires an alias
            fprintf(stderr, "Error: Subquery in FROM clause requires an alias\n");
            releaseNode(node);
            return NULL;
        }
        
        return node;
    }
    
    // expect a string literal filename or identifier table name
    node->from.table = parse_table_name(parser);
    if (!node->from.table) {
        releaseNode(node);
        return NULL;
    }
    
    // parse optional alias
    const char* excluded[] = {"WHERE", "GROUP", "ORDER", "LIMIT", "UNION", "INTERSECT", "EXCEPT"};
    node->from.alias = parse_optional_alias(parser, excluded, 7);
    
    return node;
}

/* where clause parsing */
ASTNode* parse_where(Parser* parser) {
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "WHERE")) {
        return NULL;
    }
    parser_advance(parser);
    
    return parse_condition(parser);
}

/* group by clause parsing */
ASTNode* parse_group_by(Parser* parser) {
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "GROUP")) {
        return NULL;
    }
    parser_advance(parser);
    
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "BY")) {
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_GROUP_BY);
    
    // parse qualified identifier
    node->identifier = parse_qualified_identifier(parser);
    
    return node;
}

/* order by clause parsing */
ASTNode* parse_order_by(Parser* parser) {
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "ORDER")) {
        return NULL;
    }
    parser_advance(parser);
    
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "BY")) {
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_ORDER_BY);
    node->order_by.descending = false;
    
    // try to parse as function call first
    char* func_str = build_function_string(parser);
    if (func_str) {
        node->order_by.column = func_str;
    } else {
        // parse as qualified identifier
        node->order_by.column = parse_qualified_identifier(parser);
    }
    
    // check for ASC/DESC
    Token* token = parser_current_token(parser);
    if (token->type == TOKEN_TYPE_KEYWORD) {
        if (strcasecmp(token->value, "DESC") == 0) {
            node->order_by.descending = true;
            parser_advance(parser);
        } else if (strcasecmp(token->value, "ASC") == 0) {
            node->order_by.descending = false;
            parser_advance(parser);
        }
    }
    
    return node;
}

/* join clause parsing */
ASTNode* parse_join(Parser* parser) {
    // parse JOIN type (LEFT/RIGHT/FULL/INNER with optional OUTER)
    JoinType join_type = parse_join_type(parser);
    
    // expect JOIN keyword
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "JOIN")) {
        return NULL;
    }
    parser_advance(parser);
    
    ASTNode* node = create_node(NODE_TYPE_JOIN);
    node->join.join_type = join_type;
    
    // parse table name
    node->join.table = parse_table_name(parser);
    if (!node->join.table) {
        releaseNode(node);
        return NULL;
    }
    
    // parse optional alias
    const char* excluded[] = {"ON", "WHERE", "GROUP", "ORDER", "LIMIT"};
    node->join.alias = parse_optional_alias(parser, excluded, 5);
    
    // parse ON condition
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "ON")) {
        parser_advance(parser);
        node->join.condition = parse_condition(parser);
    }
    
    return node;
}

/* helper: parse LIMIT and OFFSET clauses */
static void parse_limit_offset(Parser* parser, int* limit, int* offset) {
    if (!parser_match(parser, TOKEN_TYPE_KEYWORD, "LIMIT")) {
        return;
    }
    
    parser_advance(parser);
    Token* limit_token = parser_current_token(parser);
    if (limit_token->type != TOKEN_TYPE_LITERAL) {
        return;
    }
    
    *limit = atoi(limit_token->value);
    parser_advance(parser);
    
    // check for OFFSET or comma syntax
    Token* next = parser_current_token(parser);
    if (next->type == TOKEN_TYPE_PUNCTUATION && strcmp(next->value, ",") == 0) {
        // LIMIT offset, count (MySQL style)
        parser_advance(parser);
        Token* count_token = parser_current_token(parser);
        if (count_token->type == TOKEN_TYPE_LITERAL) {
            *offset = *limit;
            *limit = atoi(count_token->value);
            parser_advance(parser);
        }
    } else if (next->type == TOKEN_TYPE_KEYWORD && strcasecmp(next->value, "OFFSET") == 0) {
        // LIMIT count OFFSET offset (standard SQL)
        parser_advance(parser);
        Token* offset_token = parser_current_token(parser);
        if (offset_token->type == TOKEN_TYPE_LITERAL) {
            *offset = atoi(offset_token->value);
            parser_advance(parser);
        }
    }
}

/* main parse function
 * Internal function to parse a full query 
 * Handles SELECT, FROM, JOINs, WHERE, GROUP BY, HAVING, ORDER BY, LIMIT/OFFSET
 */
static ASTNode* parse_query_internal(Parser* parser) {
    // Check for INSERT, UPDATE, DELETE, CREATE, ALTER first
    Token* first = parser_current_token(parser);
    if (first->type == TOKEN_TYPE_KEYWORD) {
        if (strcasecmp(first->value, "INSERT") == 0) {
            return parse_insert(parser);
        } else if (strcasecmp(first->value, "UPDATE") == 0) {
            return parse_update(parser);
        } else if (strcasecmp(first->value, "DELETE") == 0) {
            return parse_delete(parser);
        } else if (strcasecmp(first->value, "CREATE") == 0) {
            return parse_create_table(parser);
        } else if (strcasecmp(first->value, "ALTER") == 0) {
            return parse_alter_table(parser);
        }
    }
    
    // create root query node to hold all clauses (SELECT)
    ASTNode* root = create_node(NODE_TYPE_QUERY);
    root->query.joins = NULL;
    root->query.join_count = 0;
    root->query.limit = -1;   // no limit by default
    root->query.offset = -1;  // no offset by default
    
    // SELECT
    root->query.select = parse_select(parser);
    if (!root->query.select) {
        releaseNode(root);
        return NULL;
    }
    
    // FROM
    root->query.from = parse_from(parser);
    
    // JOINs
    int join_capacity = 4;
    root->query.joins = malloc(sizeof(ASTNode*) * join_capacity);
    
    while (1) {
        Token* token = parser_current_token(parser);
        if (token->type != TOKEN_TYPE_KEYWORD) break;
        
        if (strcasecmp(token->value, "JOIN") == 0 ||
            strcasecmp(token->value, "LEFT") == 0 ||
            strcasecmp(token->value, "RIGHT") == 0 ||
            strcasecmp(token->value, "FULL") == 0 ||
            strcasecmp(token->value, "INNER") == 0) {
            
            root->query.joins = ensure_capacity(root->query.joins, &join_capacity, 
                                               root->query.join_count, sizeof(ASTNode*));
            
            ASTNode* join_node = parse_join(parser);
            if (join_node) {
                root->query.joins[root->query.join_count++] = join_node;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    // WHERE
    root->query.where = parse_where(parser);
    
    // GROUP BY
    root->query.group_by = parse_group_by(parser);
    
    // HAVING (after GROUP BY)
    root->query.having = NULL;
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "HAVING")) {
        parser_advance(parser);
        root->query.having = parse_condition(parser);
    }
    
    //ORDER BY
    root->query.order_by = parse_order_by(parser);
    
    // LIMIT/OFFSET
    parse_limit_offset(parser, &root->query.limit, &root->query.offset);
    
    return root;
}

ASTNode* parse(const char* sql) {
    int token_count = 0;
    Token* tokens = tokenize(sql, &token_count);
    
    if (!tokens) {
        return NULL;
    }
    
    Parser* parser = parser_init(tokens, token_count);
    
    // parse first query
    ASTNode* left = parse_query_internal(parser);
    if (!left) {
        parser_free(parser);
        freeTokens(tokens, token_count);
        return NULL;
    }
    
    // check for set operations (UNION, INTERSECT, EXCEPT)
    while (1) {
        Token* token = parser_current_token(parser);
        if (token->type != TOKEN_TYPE_KEYWORD) break;
        
        SetOpType op_type;
        bool found_op = false;
        
        if (strcasecmp(token->value, "UNION") == 0) {
            parser_advance(parser);
            // check for UNION ALL
            if (parser_match(parser, TOKEN_TYPE_KEYWORD, "ALL")) {
                parser_advance(parser);
                op_type = SET_OP_UNION_ALL;
            } else {
                op_type = SET_OP_UNION;
            }
            found_op = true;
        } else if (strcasecmp(token->value, "INTERSECT") == 0) {
            parser_advance(parser);
            op_type = SET_OP_INTERSECT;
            found_op = true;
        } else if (strcasecmp(token->value, "EXCEPT") == 0) {
            parser_advance(parser);
            op_type = SET_OP_EXCEPT;
            found_op = true;
        }
        
        if (!found_op) break;
        
        // parse right query
        ASTNode* right = parse_query_internal(parser);
        if (!right) {
            releaseNode(left);
            parser_free(parser);
            freeTokens(tokens, token_count);
            return NULL;
        }
        
        // create set operation node
        ASTNode* set_op = create_node(NODE_TYPE_SET_OP);
        set_op->set_op.op_type = op_type;
        set_op->set_op.left = left;
        set_op->set_op.right = right;
        
        left = set_op;  // for chaining multiple operations
    }
    
    parser_free(parser);
    freeTokens(tokens, token_count);
    
    return left;
}

/* helper to print indentation for AST visualization */
static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

void printAst(ASTNode* node, int depth) {
    if (!node) return;
    
    print_indent(depth);
    
    switch (node->type) {
        case NODE_TYPE_QUERY:
            printf("QUERY:\n");
            if (node->query.select) {
                print_indent(depth + 1);
                printf("SELECT:\n");
                printAst(node->query.select, depth + 1);
            }
            if (node->query.from) {
                print_indent(depth + 1);
                printf("FROM:\n");
                printAst(node->query.from, depth + 2);
            }
            if (node->query.where) {
                print_indent(depth + 1);
                printf("WHERE:\n");
                printAst(node->query.where, depth + 2);
            }
            if (node->query.group_by) {
                print_indent(depth + 1);
                printf("GROUP BY:\n");
                printAst(node->query.group_by, depth + 2);
            }
            if (node->query.order_by) {
                print_indent(depth + 1);
                printf("ORDER BY:\n");
                printAst(node->query.order_by, depth + 2);
            }
            break;
        case NODE_TYPE_SELECT:
            for (int i = 0; i < node->select.column_count; i++) {
                print_indent(depth);
                printf("- %s\n", node->select.columns[i]);
            }
            break;
        case NODE_TYPE_GROUP_BY:
            printf("%s\n", node->identifier);
            break;
        case NODE_TYPE_ORDER_BY:
            printf("%s %s\n", node->order_by.column, node->order_by.descending ? "DESC" : "ASC");
            break;
        case NODE_TYPE_FROM:
            printf("Table: %s", node->from.table);
            if (node->from.alias) {
                printf(" AS %s", node->from.alias);
            }
            printf("\n");
            break;
        case NODE_TYPE_IDENTIFIER:
            printf("IDENTIFIER: %s\n", node->identifier);
            break;
        case NODE_TYPE_LITERAL:
            printf("LITERAL: %s\n", node->literal);
            break;
        case NODE_TYPE_CONDITION:
            printf("CONDITION: %s\n", node->condition.operator);
            printAst(node->condition.left, depth + 1);
            printAst(node->condition.right, depth + 1);
            break;
        case NODE_TYPE_FUNCTION:
            printf("FUNCTION: %s\n", node->function.name);
            for (int i = 0; i < node->function.arg_count; i++) {
                printAst(node->function.args[i], depth + 1);
            }
            break;
        case NODE_TYPE_LIST:
            printf("LIST:\n");
            for (int i = 0; i < node->list.node_count; i++) {
                printAst(node->list.nodes[i], depth + 1);
            }
            break;
        case NODE_TYPE_INSERT:
            printf("INSERT INTO: %s\n", node->insert.table);
            if (node->insert.columns) {
                print_indent(depth + 1);
                printf("COLUMNS: ");
                for (int i = 0; i < node->insert.column_count; i++) {
                    printf("%s%s", i > 0 ? ", " : "", node->insert.columns[i]);
                }
                printf("\n");
            }
            print_indent(depth + 1);
            printf("VALUES:\n");
            for (int i = 0; i < node->insert.value_count; i++) {
                printAst(node->insert.values[i], depth + 2);
            }
            break;
        case NODE_TYPE_UPDATE:
            printf("UPDATE: %s\n", node->update.table);
            print_indent(depth + 1);
            printf("SET:\n");
            for (int i = 0; i < node->update.assignment_count; i++) {
                printAst(node->update.assignments[i], depth + 2);
            }
            if (node->update.where) {
                print_indent(depth + 1);
                printf("WHERE:\n");
                printAst(node->update.where, depth + 2);
            }
            break;
        case NODE_TYPE_DELETE:
            printf("DELETE FROM: %s\n", node->delete_stmt.table);
            if (node->delete_stmt.where) {
                print_indent(depth + 1);
                printf("WHERE:\n");
                printAst(node->delete_stmt.where, depth + 2);
            }
            break;
        case NODE_TYPE_CREATE_TABLE:
            printf("CREATE TABLE: %s\n", node->create_table.table);
            if (node->create_table.is_schema_only) {
                print_indent(depth + 1);
                printf("COLUMNS:\n");
                for (int i = 0; i < node->create_table.column_count; i++) {
                    print_indent(depth + 2);
                    printf("%s\n", node->create_table.columns[i]);
                }
            }
            if (node->create_table.query) {
                print_indent(depth + 1);
                printf("AS:\n");
                printAst(node->create_table.query, depth + 2);
            }
            break;
        case NODE_TYPE_ALTER_TABLE:
            printf("ALTER TABLE: %s\n", node->alter_table.table);
            print_indent(depth + 1);
            switch (node->alter_table.operation) {
                case ALTER_RENAME_COLUMN:
                    printf("RENAME COLUMN: %s TO %s\n", 
                           node->alter_table.old_column_name, 
                           node->alter_table.new_column_name);
                    break;
                case ALTER_ADD_COLUMN:
                    printf("ADD COLUMN: %s\n", node->alter_table.new_column_name);
                    break;
                case ALTER_DROP_COLUMN:
                    printf("DROP COLUMN: %s\n", node->alter_table.old_column_name);
                    break;
            }
            break;
        case NODE_TYPE_ASSIGNMENT:
            printf("ASSIGN: %s = ", node->assignment.column);
            if (node->assignment.value) {
                printAst(node->assignment.value, depth);
            }
            break;
        default:
            printf("UNKNOWN NODE (type=%d)\n", node->type);
            break;
    }
}

/* parse INSERT INTO table (columns) VALUES (values) */
ASTNode* parse_insert(Parser* parser) {
    // INSERT keyword already verified by caller, just advance
    parser_advance(parser);
    
    // INTO (parser_expect already advances past it)
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "INTO")) {
        fprintf(stderr, "Error: Expected INTO after INSERT\n");
        return NULL;
    }
    
    // table name can be quoted path like './data/test.csv'
    Token* table_token = parser_current_token(parser);
    if (table_token->type != TOKEN_TYPE_IDENTIFIER && table_token->type != TOKEN_TYPE_LITERAL) {
        fprintf(stderr, "Error: Expected table name after INTO\n");
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_INSERT);
    node->insert.table = strdup(table_token->value);
    node->insert.columns = NULL;
    node->insert.column_count = 0;
    parser_advance(parser);
    
    // optional column list: (col1, col2, col3)
    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
        parser_advance(parser);
        
        int capacity = 4;
        node->insert.columns = malloc(sizeof(char*) * capacity);
        
        while (1) {
            Token* col = parser_current_token(parser);
            if (col->type != TOKEN_TYPE_IDENTIFIER) {
                fprintf(stderr, "Error: Expected column name in INSERT column list\n");
                releaseNode(node);
                return NULL;
            }
            
            if (node->insert.column_count >= capacity) {
                capacity *= 2;
                node->insert.columns = realloc(node->insert.columns, sizeof(char*) * capacity);
            }
            
            node->insert.columns[node->insert.column_count++] = strdup(col->value);
            parser_advance(parser);
            
            if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                parser_advance(parser);
            } else {
                break;
            }
        }
        
        if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
            fprintf(stderr, "Error: Expected ')' after column list\n");
            releaseNode(node);
            return NULL;
        }
    }
    
    // VALUES (parser_expect already advances)
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "VALUES")) {
        fprintf(stderr, "Error: Expected VALUES in INSERT statement\n");
        releaseNode(node);
        return NULL;
    }
    
    // (value1, value2, value3) (parser_expect already advances)
    if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
        fprintf(stderr, "Error: Expected '(' after VALUES\n");
        releaseNode(node);
        return NULL;
    }
    
    int capacity = 4;
    node->insert.values = malloc(sizeof(ASTNode*) * capacity);
    node->insert.value_count = 0;
    
    while (1) {
        ASTNode* value = parse_expression(parser);
        if (!value) {
            fprintf(stderr, "Error: Expected value in VALUES list\n");
            releaseNode(node);
            return NULL;
        }
        
        if (node->insert.value_count >= capacity) {
            capacity *= 2;
            node->insert.values = realloc(node->insert.values, sizeof(ASTNode*) * capacity);
        }
        
        node->insert.values[node->insert.value_count++] = value;
        
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
            parser_advance(parser);
        } else {
            break;
        }
    }
    
    if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
        fprintf(stderr, "Error: Expected ')' after VALUES list\n");
        releaseNode(node);
        return NULL;
    }
    
    return node;
}

/* parse UPDATE table SET col1=val1, col2=val2 WHERE condition */
ASTNode* parse_update(Parser* parser) {
    // UPDATE keyword already verified by caller, just advance
    parser_advance(parser);
    
    // table name
    Token* table_token = parser_current_token(parser);
    if (table_token->type != TOKEN_TYPE_IDENTIFIER && table_token->type != TOKEN_TYPE_LITERAL) {
        fprintf(stderr, "Error: Expected table name after UPDATE\n");
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_UPDATE);
    node->update.table = strdup(table_token->value);
    node->update.assignments = NULL;
    node->update.assignment_count = 0;
    node->update.where = NULL;
    parser_advance(parser);
    
    // SET (parser_expect already advances)
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "SET")) {
        fprintf(stderr, "Error: Expected SET after table name in UPDATE\n");
        releaseNode(node);
        return NULL;
    }
    
    // parse assignments col1=val1, col2=val2
    int capacity = 4;
    node->update.assignments = malloc(sizeof(ASTNode*) * capacity);
    
    while (1) {
        Token* col = parser_current_token(parser);
        if (col->type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Error: Expected column name in SET clause\n");
            releaseNode(node);
            return NULL;
        }
        
        ASTNode* assignment = create_node(NODE_TYPE_ASSIGNMENT);
        assignment->assignment.column = strdup(col->value);
        parser_advance(parser);
        
        // = (parser_expect already advances)
        if (!parser_expect(parser, TOKEN_TYPE_OPERATOR, "=")) {
            fprintf(stderr, "Error: Expected '=' in assignment\n");
            releaseNode(assignment);
            releaseNode(node);
            return NULL;
        }
        
        // value
        assignment->assignment.value = parse_expression(parser);
        if (!assignment->assignment.value) {
            fprintf(stderr, "Error: Expected value in assignment\n");
            releaseNode(assignment);
            releaseNode(node);
            return NULL;
        }
        
        if (node->update.assignment_count >= capacity) {
            capacity *= 2;
            node->update.assignments = realloc(node->update.assignments, sizeof(ASTNode*) * capacity);
        }
        
        node->update.assignments[node->update.assignment_count++] = assignment;
        
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
            parser_advance(parser);
        } else {
            break;
        }
    }
    
    // optional WHERE
    node->update.where = parse_where(parser);
    
    return node;
}

/* parse DELETE FROM table WHERE condition */
ASTNode* parse_delete(Parser* parser) {
    // DELETE keyword already verified by caller, just advance
    parser_advance(parser);
    
    // FROM (parser_expect already advances)
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "FROM")) {
        fprintf(stderr, "Error: Expected FROM after DELETE\n");
        return NULL;
    }
    
    // table name
    Token* table_token = parser_current_token(parser);
    if (table_token->type != TOKEN_TYPE_IDENTIFIER && table_token->type != TOKEN_TYPE_LITERAL) {
        fprintf(stderr, "Error: Expected table name after FROM\n");
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_DELETE);
    node->delete_stmt.table = strdup(table_token->value);
    node->delete_stmt.where = NULL;
    parser_advance(parser);
    
    // WHERE, required for safety, we don't allow DELETE without WHERE
    node->delete_stmt.where = parse_where(parser);
    if (!node->delete_stmt.where) {
        fprintf(stderr, "Error: WHERE clause is required for DELETE (safety measure)\n");
        releaseNode(node);
        return NULL;
    }
    
    return node;
}

/* parse CREATE TABLE 'file.csv' AS SELECT ... 
 *    or CREATE TABLE 'file.csv' (col1, col2, col3)
 *    or CREATE TABLE 'file.csv' AS (col1, col2, col3)
 */
ASTNode* parse_create_table(Parser* parser) {
    // CREATE keyword already verified by caller, just advance
    parser_advance(parser);
    
    // TABLE keyword
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "TABLE")) {
        fprintf(stderr, "Error: Expected TABLE after CREATE\n");
        return NULL;
    }
    
    // table name (file path)
    Token* table_token = parser_current_token(parser);
    if (table_token->type != TOKEN_TYPE_IDENTIFIER && table_token->type != TOKEN_TYPE_LITERAL) {
        fprintf(stderr, "Error: Expected table name/path after CREATE TABLE\n");
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_CREATE_TABLE);
    node->create_table.table = strdup(table_token->value);
    node->create_table.columns = NULL;
    node->create_table.column_count = 0;
    node->create_table.query = NULL;
    node->create_table.is_schema_only = false;
    parser_advance(parser);
    
    // check next token: AS or (
    if (parser_match(parser, TOKEN_TYPE_KEYWORD, "AS")) {
        parser_advance(parser);
        
        if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
            // could be AS (col1, col2) or AS (SELECT ...)
            // peek to see if next token is SELECT
            Token* peek = parser_peek_token(parser, 1);
            if (peek && peek->type == TOKEN_TYPE_KEYWORD && 
                strcasecmp(peek->value, "SELECT") == 0) {
                // AS (SELECT ...) for a subquery
                parser_advance(parser); // consume (
                node->create_table.query = parse_query_internal(parser);
                if (!node->create_table.query) {
                    fprintf(stderr, "Error: Failed to parse SELECT query in CREATE TABLE AS\n");
                    releaseNode(node);
                    return NULL;
                }
                if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
                    fprintf(stderr, "Error: Expected ')' after SELECT query\n");
                    releaseNode(node);
                    return NULL;
                }
            } else {
                // AS (col1, col2, col3) for schema mapping
                parser_advance(parser); // consume (
                int capacity = 4;
                node->create_table.columns = malloc(sizeof(char*) * capacity);
                
                while (1) {
                    Token* col = parser_current_token(parser);
                    if (col->type != TOKEN_TYPE_IDENTIFIER) {
                        fprintf(stderr, "Error: Expected column name in schema definition\n");
                        releaseNode(node);
                        return NULL;
                    }
                    
                    if (node->create_table.column_count >= capacity) {
                        capacity *= 2;
                        node->create_table.columns = realloc(node->create_table.columns, 
                                                             sizeof(char*) * capacity);
                    }
                    
                    node->create_table.columns[node->create_table.column_count++] = strdup(col->value);
                    parser_advance(parser);
                    
                    if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                        parser_advance(parser);
                    } else {
                        break;
                    }
                }
                
                if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
                    fprintf(stderr, "Error: Expected ')' after column list\n");
                    releaseNode(node);
                    return NULL;
                }
                // this is schema mapping but we'll treat as schema only for now
                node->create_table.is_schema_only = true;
            }
        } else {
            // AS SELECT without parentheses
            node->create_table.query = parse_query_internal(parser);
            if (!node->create_table.query) {
                fprintf(stderr, "Error: Failed to parse SELECT query in CREATE TABLE AS\n");
                releaseNode(node);
                return NULL;
            }
        }
    } else if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, "(")) {
        // CREATE TABLE 'file' (col1, col2, col3) for empty table with schema
        parser_advance(parser);
        int capacity = 4;
        node->create_table.columns = malloc(sizeof(char*) * capacity);
        
        while (1) {
            Token* col = parser_current_token(parser);
            if (col->type != TOKEN_TYPE_IDENTIFIER) {
                fprintf(stderr, "Error: Expected column name in CREATE TABLE\n");
                releaseNode(node);
                return NULL;
            }
            
            if (node->create_table.column_count >= capacity) {
                capacity *= 2;
                node->create_table.columns = realloc(node->create_table.columns, 
                                                     sizeof(char*) * capacity);
            }
            
            node->create_table.columns[node->create_table.column_count++] = strdup(col->value);
            parser_advance(parser);
            
            if (parser_match(parser, TOKEN_TYPE_PUNCTUATION, ",")) {
                parser_advance(parser);
            } else {
                break;
            }
        }
        
        if (!parser_expect(parser, TOKEN_TYPE_PUNCTUATION, ")")) {
            fprintf(stderr, "Error: Expected ')' after column list\n");
            releaseNode(node);
            return NULL;
        }
        node->create_table.is_schema_only = true;
    } else {
        fprintf(stderr, "Error: Expected AS or '(' after table name in CREATE TABLE\n");
        releaseNode(node);
        return NULL;
    }
    
    return node;
}
/*
 * parse ALTER TABLE statement
 * supports:
 *   ALTER TABLE 'file.csv' RENAME COLUMN old_name TO new_name
 *   ALTER TABLE 'file.csv' ADD COLUMN new_name
 *   ALTER TABLE 'file.csv' DROP COLUMN col_name
 */
ASTNode* parse_alter_table(Parser* parser) {
    // ALTER keyword already verified by caller, just advance
    parser_advance(parser);
    
    // TABLE keyword
    if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "TABLE")) {
        fprintf(stderr, "Error: Expected TABLE after ALTER\n");
        return NULL;
    }
    
    // table name (file path)
    Token* table_token = parser_current_token(parser);
    if (table_token->type != TOKEN_TYPE_IDENTIFIER && table_token->type != TOKEN_TYPE_LITERAL) {
        fprintf(stderr, "Error: Expected table name/path after ALTER TABLE\n");
        return NULL;
    }
    
    ASTNode* node = create_node(NODE_TYPE_ALTER_TABLE);
    node->alter_table.table = strdup(table_token->value);
    node->alter_table.old_column_name = NULL;
    node->alter_table.new_column_name = NULL;
    parser_advance(parser);
    
    // check operation: RENAME, ADD, or DROP
    Token* op_token = parser_current_token(parser);
    if (!op_token || op_token->type != TOKEN_TYPE_KEYWORD) {
        fprintf(stderr, "Error: Expected RENAME, ADD, or DROP after table name\n");
        releaseNode(node);
        return NULL;
    }
    
    if (strcasecmp(op_token->value, "RENAME") == 0) {
        // RENAME COLUMN old_name TO new_name
        node->alter_table.operation = ALTER_RENAME_COLUMN;
        parser_advance(parser);
        
        if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "COLUMN")) {
            fprintf(stderr, "Error: Expected COLUMN after RENAME\n");
            releaseNode(node);
            return NULL;
        }
        
        Token* old_col = parser_current_token(parser);
        if (old_col->type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Error: Expected column name after RENAME COLUMN\n");
            releaseNode(node);
            return NULL;
        }
        node->alter_table.old_column_name = strdup(old_col->value);
        parser_advance(parser);
        
        if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "TO")) {
            fprintf(stderr, "Error: Expected TO after old column name\n");
            releaseNode(node);
            return NULL;
        }
        
        Token* new_col = parser_current_token(parser);
        if (new_col->type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Error: Expected new column name after TO\n");
            releaseNode(node);
            return NULL;
        }
        node->alter_table.new_column_name = strdup(new_col->value);
        parser_advance(parser);
        
    } else if (strcasecmp(op_token->value, "ADD") == 0) {
        // ADD COLUMN new_name
        node->alter_table.operation = ALTER_ADD_COLUMN;
        parser_advance(parser);
        
        if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "COLUMN")) {
            fprintf(stderr, "Error: Expected COLUMN after ADD\n");
            releaseNode(node);
            return NULL;
        }
        
        Token* new_col = parser_current_token(parser);
        if (new_col->type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Error: Expected column name after ADD COLUMN\n");
            releaseNode(node);
            return NULL;
        }
        node->alter_table.new_column_name = strdup(new_col->value);
        parser_advance(parser);
        
    } else if (strcasecmp(op_token->value, "DROP") == 0) {
        // DROP COLUMN col_name
        node->alter_table.operation = ALTER_DROP_COLUMN;
        parser_advance(parser);
        
        if (!parser_expect(parser, TOKEN_TYPE_KEYWORD, "COLUMN")) {
            fprintf(stderr, "Error: Expected COLUMN after DROP\n");
            releaseNode(node);
            return NULL;
        }
        
        Token* col = parser_current_token(parser);
        if (col->type != TOKEN_TYPE_IDENTIFIER) {
            fprintf(stderr, "Error: Expected column name after DROP COLUMN\n");
            releaseNode(node);
            return NULL;
        }
        node->alter_table.old_column_name = strdup(col->value);
        parser_advance(parser);
        
    } else {
        fprintf(stderr, "Error: Unsupported ALTER TABLE operation '%s'\n", op_token->value);
        releaseNode(node);
        return NULL;
    }
    
    return node;
}