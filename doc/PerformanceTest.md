## Performance Test

```sh
$ python ./utils/generate_big_dataset.py 1000000
$ make test
$ ./build/test_load_performance
=== CSV Load Performance Test ===

Testing file: data/bigdata.csv

Results:
--------
Rows loaded:      1000000
Columns:          5
Total time:       238.79 ms
Time per row:     0.0002 ms
Rows per second:  4187796

Breakdown:
  File I/O + CSV parsing: 238.79 ms (100%)

Memory usage (approximate):
  Total: 113.27 MB
  Per row: 0.12 KB

=== Test completed successfully ===
```