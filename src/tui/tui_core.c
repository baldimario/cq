#include "../../include/tui/tui_core.h"
#include "../../include/tui/tui_render.h"
#include "../../include/tui/terminal.h"
#include "../../include/formats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern CsvConfig global_csv_config;

TuiApp* tui_app_create(const char* workspace_dir) {
    TuiApp* app = calloc(1, sizeof(TuiApp));
    if (!app) {
        return NULL;
    }
    
    if (workspace_dir) {
        app->workspace_dir = strdup(workspace_dir);
    } else {
        app->workspace_dir = strdup(".");
    }
    
    app->tabs = NULL;
    app->tab_count = 0;
    app->tab_capacity = 0;
    app->active_tab = -1;
    app->mode = MODE_NORMAL;
    app->running = true;
    app->needs_redraw = true;
    
    app->query_buffer = NULL;
    app->query_buffer_size = 0;
    app->query_cursor_pos = 0;
    
    app->save_buffer = NULL;
    app->save_buffer_size = 0;
    app->save_cursor_pos = 0;
    
    app->table_files = NULL;
    app->table_count = 0;
    app->table_selected = 0;
    
    snprintf(app->status_message, sizeof(app->status_message), 
            "Welcome to cq TUI. Press F1 or ? for help.");
    
    tui_scan_workspace(app);
    
    return app;
}

void tui_app_destroy(TuiApp* app) {
    if (!app) {
        return;
    }
    
    for (int i = 0; i < app->tab_count; i++) {
        tui_tab_destroy(app->tabs[i]);
    }
    free(app->tabs);
    
    free(app->workspace_dir);
    free(app->query_buffer);
    free(app->save_buffer);
    
    if (app->table_files) {
        for (int i = 0; i < app->table_count; i++) {
            free(app->table_files[i]);
        }
        free(app->table_files);
    }
    
    free(app);
}

void tui_render(TuiApp* app) {
    if (!app->needs_redraw) {
        return;
    }
    
    terminal_clear_screen();
    
    tui_render_tabs(app);
    
    if (app->mode == MODE_HELP) {
        tui_render_help(app);
    } else if (app->mode == MODE_TABLE_LIST) {
        if (app->active_tab >= 0 && app->active_tab < app->tab_count) {
            tui_render_table(app, app->tabs[app->active_tab]);
        }
        tui_render_table_list(app);
    } else {
        if (app->active_tab >= 0 && app->active_tab < app->tab_count) {
            tui_render_table(app, app->tabs[app->active_tab]);
        } else {
            TermSize size = terminal_get_size();
            terminal_cursor_move(size.rows / 2, 1);
            tui_render_text_centered(size.rows / 2, "No tables open. Press 't' to browse available tables.");
        }
    }
    
    if (app->mode == MODE_QUERY) {
        tui_render_query_input(app);
    } else if (app->mode == MODE_SAVE) {
        tui_render_save_input(app);
    } else {
        tui_render_status_bar(app);
    }
    
    app->needs_redraw = false;
}

int tui_app_run(TuiApp* app) {
    if (!app) {
        return 1;
    }
    
    if (!terminal_init()) {
        fprintf(stderr, "Failed to initialize terminal\n");
        return 1;
    }
    
    while (app->running) {
        tui_render(app);
        
        int key = terminal_read_key();
        if (key > 0) {
            tui_handle_key(app, key);
        }
    }
    
    terminal_cleanup();
    
    return 0;
}

bool tui_export_table(TuiApp* app, const char* filename) {
    if (!app || !filename || app->active_tab < 0 || app->active_tab >= app->tab_count) {
        return false;
    }
    
    TuiTab* tab = app->tabs[app->active_tab];
    if (!tab || !tab->table) {
        return false;
    }
    
    bool success = write_output_file(filename, tab->table, FMT_CSV, ',');
    
    if (success) {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Exported to %s", filename);
    } else {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Error: Failed to export to %s", filename);
    }
    
    app->needs_redraw = true;
    
    return success;
}
