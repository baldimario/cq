# Command Line Interface

cq [OPTIONS]

Options:
- -h, --help   Show this help message
- -q <query>   SQL query to execute (use '-' to read from stdin)
- -f <file>    Read SQL query from file
- -o <file>    Write result as CSV to output file
- -c           Print count of rows that match the query
- -p           Print result as formatted table to stdout
- -v           Print result in vertical format (one column per line)
- -s <char>    Field separator for input CSV (default: ',')
- -d <char>    Output delimiter for -o option (default: ',')
- -F, --force  Allow DELETE without WHERE clause (dangerous!)

Examples:

```bash
# Print formatted table
cq -q "SELECT name, age WHERE age > 30" -p
```

```bash
# Read query from file
cq -f query.sql -p
```

```bash
# Read query from stdin (piping)
echo "SELECT * FROM data.csv" | cq -q - -p
```
