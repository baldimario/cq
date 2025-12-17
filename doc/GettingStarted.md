# Getting Started

This document provides a lightweight path to get CQ up and running.

Prerequisites: See Installation doc.

## Build

- Run `make` to build the executable.

## Run a Query

```bash
./build/cq -q "SELECT name, age FROM data.csv WHERE age > 25" -p
```

## Examples

The repository includes example SQL under assets/ (example_between.sql, example_aggregation.sql, etc.). See assets/ for details.
