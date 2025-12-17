# CSV Format

## Supported Features
- Header row (automatically detected)
- Custom delimiters (`,`, `\t`, `;`, etc.)
- Quoted fields ("value with, comma")
- Escaped quotes ("quote "inside" field")
- Memory-mapped I/O for large files

## Example CSV

```csv
id,name,age
1,Alice,25
2,Bob,30
3,Charlie,35
```

## Advanced Features
- DISTINCT - Remove Duplicate Rows
- LIKE / ILIKE - Pattern Matching
- Set Operations (UNION, UNION ALL, INTERSECT, EXCEPT)
