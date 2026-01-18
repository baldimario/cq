#include "../../include/tui/tui_core.h"
#include "../../include/tui/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

void tui_scan_workspace(TuiApp* app) {
    if (!app || !app->workspace_dir) {
        return;
    }
    
    if (app->table_files) {
        for (int i = 0; i < app->table_count; i++) {
            free(app->table_files[i]);
        }
        free(app->table_files);
        app->table_files = NULL;
        app->table_count = 0;
    }
    
    DIR* dir = opendir(app->workspace_dir);
    if (!dir) {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Error: Cannot open directory %s", app->workspace_dir);
        return;
    }
    
    int capacity = 16;
    app->table_files = malloc(capacity * sizeof(char*));
    if (!app->table_files) {
        closedir(dir);
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".csv") == 0) {
            if (app->table_count >= capacity) {
                capacity *= 2;
                char** new_files = realloc(app->table_files, capacity * sizeof(char*));
                if (!new_files) {
                    break;
                }
                app->table_files = new_files;
            }
            
            size_t path_len = strlen(app->workspace_dir) + len + 2;
            app->table_files[app->table_count] = malloc(path_len);
            if (app->table_files[app->table_count]) {
                if (app->workspace_dir[strlen(app->workspace_dir) - 1] == '/') {
                    snprintf(app->table_files[app->table_count], path_len, 
                            "%s%s", app->workspace_dir, entry->d_name);
                } else {
                    snprintf(app->table_files[app->table_count], path_len, 
                            "%s/%s", app->workspace_dir, entry->d_name);
                }
                app->table_count++;
            }
        }
    }
    
    closedir(dir);
    
    snprintf(app->status_message, sizeof(app->status_message), 
            "Found %d CSV file(s) in %s", app->table_count, app->workspace_dir);
}
