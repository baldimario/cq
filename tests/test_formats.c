#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "csv_reader.h"
#include "formats.h"

static char* read_file_snippet(const char* path, size_t max_len) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    char* buf = malloc(max_len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, max_len, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

void test_write_formats() {
    printf("Running test_write_formats...\n");

    CsvConfig config = csv_config_default();
    CsvTable* table = csv_load("data/test_data.csv", config);
    assert(table != NULL);

    const char* json_path = "/tmp/test_formats.json";
    const char* md_path = "/tmp/test_formats.md";
    const char* yaml_path = "/tmp/test_formats.yaml";

    // JSON
    assert(write_output_file(json_path, (ResultSet*)table, FMT_JSON, ','));
    char* j = read_file_snippet(json_path, 4096);
    assert(j && strchr(j, '[') && strchr(j, '{'));
    free(j);

    // Markdown
    assert(write_output_file(md_path, (ResultSet*)table, FMT_MARKDOWN, ','));
    char* m = read_file_snippet(md_path, 4096);
    assert(m && strstr(m, "|") != NULL);
    free(m);

    // YAML
    assert(write_output_file(yaml_path, (ResultSet*)table, FMT_YAML, ','));
    char* y = read_file_snippet(yaml_path, 4096);
    assert(y && strchr(y, '-') != NULL);
    free(y);

    // cleanup
    csv_free(table);
    remove(json_path);
    remove(md_path);
    remove(yaml_path);

    printf("✓ test_write_formats passed\n\n");
}

int main(void) {
    printf("=== Output Formats Test ===\n\n");
    test_write_formats();
    printf("=== Output Formats tests passed ===\n");
    return 0;
}
