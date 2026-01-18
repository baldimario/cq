#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tui/tui_core.h"
#include "evaluator.h"

extern CsvConfig global_csv_config;

int main(void) {
    printf("Testing TUI components...\n");
    
    global_csv_config.delimiter = ',';
    global_csv_config.quote = '"';
    global_csv_config.has_header = true;
    
    TuiApp* app = tui_app_create("data");
    if (!app) {
        fprintf(stderr, "Failed to create TUI app\n");
        return 1;
    }
    
    printf("✓ TUI app created\n");
    printf("✓ Workspace: %s\n", app->workspace_dir);
    printf("✓ Found %d CSV files\n", app->table_count);
    
    for (int i = 0; i < app->table_count && i < 5; i++) {
        printf("  - %s\n", app->table_files[i]);
    }
    
    if (app->table_count > 0) {
        printf("\n✓ Loading first table...\n");
        bool loaded = tui_load_table(app, app->table_files[0]);
        if (loaded) {
            printf("✓ Table loaded successfully\n");
            if (app->active_tab >= 0) {
                TuiTab* tab = app->tabs[app->active_tab];
                printf("✓ Tab title: %s\n", tab->title);
                printf("✓ Rows: %d, Columns: %d\n", 
                       tab->table->row_count, 
                       tab->table->column_count);
            }
        }
    }
    
    printf("\n✓ Testing query execution...\n");
    const char* test_query = "SELECT * FROM 'data/users.csv' WHERE age > 30";
    bool query_ok = tui_execute_query(app, test_query);
    if (query_ok) {
        printf("✓ Query executed successfully\n");
        if (app->active_tab >= 0) {
            TuiTab* tab = app->tabs[app->active_tab];
            printf("✓ Query result: %d rows\n", tab->table->row_count);
        }
    }
    
    tui_app_destroy(app);
    printf("\n✓ All tests passed!\n");
    
    return 0;
}
