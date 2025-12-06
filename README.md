# CSV query CLI tool like jq, yq or xq

### Usage
```
sage: ./build/cq <file_name> <options>
Options:
        -h      Show this help message
        -p      Enable printing of the CSV file
        -e      Expression to evaluate
```

### Compile
```
make
```

### Run
```
    cq test_data.csv -p
    cq test_data.csv -p -e "..."
```
