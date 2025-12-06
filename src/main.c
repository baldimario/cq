#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "csv.h"
#include "csvparser.h"
#include "utils.h"
#include "argparser.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    exit(1);
  }

  FILE *fp = fopen(argv[1], "r");

  if (fp == NULL) {
    perror("Error opening CSV file");
    exit(1);
  }
  
  fseek(fp, 0, SEEK_END);
  size_t file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char* file_buffer = malloc(sizeof(*file_buffer)*(file_size+1));
  size_t bytes_read = fread(file_buffer, 1, file_size, fp);
  
  if (bytes_read == 0 | bytes_read != file_size) {
    fprintf(stderr, "Error reading the file, read %zu bytes, expected %zu\n", bytes_read, file_size);
    exit(1);
  }

  file_buffer[file_size] = '\0';

  fclose(fp);

  CsvFile* csv_file = parseFile(file_buffer);
  
  if (checkArgFlags(argc, argv, 'p')) {
    printCsvFile(csv_file);
  }

  free(file_buffer);
  return 0;
}
