# Architecture

Overview of CQ's architecture and how it processes queries.

Directories:
- include/ header files
- src/ source files
- tests/ test suite
- data/ test data
- Makefile for build configuration

## Query Execution Pipeline

```
SQL String
    ↓
Tokenizer → [Tokens]
    ↓
Parser → [Abstract Syntax Tree]
    ↓
Evaluator → [ResultSet]
    ↓
Output Formatter (Table/CSV/Count)
```
