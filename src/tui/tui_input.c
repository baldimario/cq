#include "../../include/tui/tui_core.h"
#include "../../include/tui/tui_render.h"
#include "../../include/tui/terminal.h"
#include "../../include/evaluator.h"
#include "../../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void tui_handle_key(TuiApp* app, int key) {
    if (!app) {
        return;
    }
    
    if (app->mode == MODE_HELP) {
        app->mode = MODE_NORMAL;
        app->needs_redraw = true;
        return;
    }
    
    if (app->mode == MODE_SAVE) {
        if (key == KEY_ESCAPE) {
            app->mode = MODE_NORMAL;
            app->needs_redraw = true;
            terminal_cursor_hide();
        } else if (key == KEY_ENTER) {
            if (app->save_buffer && strlen(app->save_buffer) > 0) {
                tui_export_table(app, app->save_buffer);
                free(app->save_buffer);
                app->save_buffer = NULL;
                app->save_cursor_pos = 0;
                app->save_buffer_size = 0;
            }
            app->mode = MODE_NORMAL;
            app->needs_redraw = true;
            terminal_cursor_hide();
        } else if (key == KEY_BACKSPACE) {
            if (app->save_cursor_pos > 0 && app->save_buffer) {
                memmove(&app->save_buffer[app->save_cursor_pos - 1],
                       &app->save_buffer[app->save_cursor_pos],
                       strlen(app->save_buffer) - app->save_cursor_pos + 1);
                app->save_cursor_pos--;
                app->needs_redraw = true;
            }
        } else if (key == KEY_DELETE) {
            if (app->save_buffer && app->save_cursor_pos < (int)strlen(app->save_buffer)) {
                memmove(&app->save_buffer[app->save_cursor_pos],
                       &app->save_buffer[app->save_cursor_pos + 1],
                       strlen(app->save_buffer) - app->save_cursor_pos);
                app->needs_redraw = true;
            }
        } else if (key == KEY_ARROW_LEFT) {
            if (app->save_cursor_pos > 0) {
                app->save_cursor_pos--;
                app->needs_redraw = true;
            }
        } else if (key == KEY_ARROW_RIGHT) {
            if (app->save_buffer && app->save_cursor_pos < (int)strlen(app->save_buffer)) {
                app->save_cursor_pos++;
                app->needs_redraw = true;
            }
        } else if (key == KEY_HOME) {
            app->save_cursor_pos = 0;
            app->needs_redraw = true;
        } else if (key == KEY_END) {
            if (app->save_buffer) {
                app->save_cursor_pos = strlen(app->save_buffer);
            }
            app->needs_redraw = true;
        } else if (key == KEY_CTRL_N) {
            tui_app_next_tab(app);
        } else if (key == KEY_CTRL_P) {
            tui_app_prev_tab(app);
        } else if (key >= 32 && key < 127) {
            if (!app->save_buffer) {
                app->save_buffer_size = 256;
                app->save_buffer = calloc(1, app->save_buffer_size);
            }
            
            int len = app->save_buffer ? strlen(app->save_buffer) : 0;
            if (len + 1 >= app->save_buffer_size) {
                app->save_buffer_size *= 2;
                app->save_buffer = realloc(app->save_buffer, app->save_buffer_size);
            }
            
            if (app->save_buffer) {
                memmove(&app->save_buffer[app->save_cursor_pos + 1],
                       &app->save_buffer[app->save_cursor_pos],
                       len - app->save_cursor_pos + 1);
                app->save_buffer[app->save_cursor_pos] = (char)key;
                app->save_cursor_pos++;
                app->needs_redraw = true;
            }
        }
        return;
    }
    
    if (app->mode == MODE_QUERY) {
        if (key == KEY_ESCAPE) {
            app->mode = MODE_NORMAL;
            app->needs_redraw = true;
            terminal_cursor_hide();
        } else if (key == KEY_ENTER) {
            if (app->query_buffer && strlen(app->query_buffer) > 0) {
                tui_execute_query(app, app->query_buffer);
                free(app->query_buffer);
                app->query_buffer = NULL;
                app->query_cursor_pos = 0;
                app->query_buffer_size = 0;
            }
            app->mode = MODE_NORMAL;
            app->needs_redraw = true;
            terminal_cursor_hide();
        } else if (key == KEY_BACKSPACE) {
            if (app->query_cursor_pos > 0 && app->query_buffer) {
                memmove(&app->query_buffer[app->query_cursor_pos - 1],
                       &app->query_buffer[app->query_cursor_pos],
                       strlen(app->query_buffer) - app->query_cursor_pos + 1);
                app->query_cursor_pos--;
                app->needs_redraw = true;
            }
        } else if (key == KEY_DELETE) {
            if (app->query_buffer && app->query_cursor_pos < (int)strlen(app->query_buffer)) {
                memmove(&app->query_buffer[app->query_cursor_pos],
                       &app->query_buffer[app->query_cursor_pos + 1],
                       strlen(app->query_buffer) - app->query_cursor_pos);
                app->needs_redraw = true;
            }
        } else if (key == KEY_ARROW_LEFT) {
            if (app->query_cursor_pos > 0) {
                app->query_cursor_pos--;
                app->needs_redraw = true;
            }
        } else if (key == KEY_ARROW_RIGHT) {
            if (app->query_buffer && app->query_cursor_pos < (int)strlen(app->query_buffer)) {
                app->query_cursor_pos++;
                app->needs_redraw = true;
            }
        } else if (key == KEY_HOME) {
            app->query_cursor_pos = 0;
            app->needs_redraw = true;
        } else if (key == KEY_END) {
            if (app->query_buffer) {
                app->query_cursor_pos = strlen(app->query_buffer);
            }
            app->needs_redraw = true;
        } else if (key == KEY_CTRL_N) {
            tui_app_next_tab(app);
        } else if (key == KEY_CTRL_P) {
            tui_app_prev_tab(app);
        } else if (key >= 32 && key < 127) {
            if (!app->query_buffer) {
                app->query_buffer_size = 256;
                app->query_buffer = calloc(1, app->query_buffer_size);
            }
            
            int len = app->query_buffer ? strlen(app->query_buffer) : 0;
            if (len + 1 >= app->query_buffer_size) {
                app->query_buffer_size *= 2;
                app->query_buffer = realloc(app->query_buffer, app->query_buffer_size);
            }
            
            if (app->query_buffer) {
                memmove(&app->query_buffer[app->query_cursor_pos + 1],
                       &app->query_buffer[app->query_cursor_pos],
                       len - app->query_cursor_pos + 1);
                app->query_buffer[app->query_cursor_pos] = (char)key;
                app->query_cursor_pos++;
                app->needs_redraw = true;
            }
        }
        return;
    }
    
    if (app->mode == MODE_TABLE_LIST) {
        if (key == KEY_ESCAPE || key == 't') {
            app->mode = MODE_NORMAL;
            app->needs_redraw = true;
        } else if (key == KEY_ARROW_UP && app->table_selected > 0) {
            app->table_selected--;
            app->needs_redraw = true;
        } else if (key == KEY_ARROW_DOWN && app->table_selected < app->table_count - 1) {
            app->table_selected++;
            app->needs_redraw = true;
        } else if (key == KEY_ENTER) {
            if (app->table_selected >= 0 && app->table_selected < app->table_count) {
                tui_load_table(app, app->table_files[app->table_selected]);
                app->mode = MODE_NORMAL;
                app->needs_redraw = true;
            }
        }
        return;
    }
    
    if (app->mode == MODE_NORMAL) {
        if (key == KEY_CTRL_Q || key == 'q') {
            app->running = false;
        } else if (key == 'h') {
            app->mode = MODE_HELP;
            app->needs_redraw = true;
        } else if (key == ':') {
            app->mode = MODE_QUERY;
            app->needs_redraw = true;
        } else if (key == 't') {
            tui_scan_workspace(app);
            app->mode = MODE_TABLE_LIST;
            app->needs_redraw = true;
        } else if (key == 'w') {
            app->mode = MODE_SAVE;
            app->needs_redraw = true;
        } else if (key == KEY_CTRL_N) {
            tui_app_next_tab(app);
        } else if (key == KEY_CTRL_P) {
            tui_app_prev_tab(app);
        } else if (key == KEY_CTRL_W) {
            if (app->active_tab >= 0) {
                tui_app_close_tab(app, app->active_tab);
            }
        } else if (app->active_tab >= 0 && app->active_tab < app->tab_count) {
            TuiTab* tab = app->tabs[app->active_tab];
            if (tab && tab->table) {
                TermSize size = terminal_get_size();
                int visible_rows = size.rows - 6;
                
                if (key == KEY_ARROW_UP && tab->cursor_row > 0) {
                    tab->cursor_row--;
                    if (tab->cursor_row < tab->scroll_row) {
                        tab->scroll_row = tab->cursor_row;
                    }
                    app->needs_redraw = true;
                } else if (key == KEY_ARROW_DOWN && tab->cursor_row < tab->table->row_count - 1) {
                    tab->cursor_row++;
                    if (tab->cursor_row >= tab->scroll_row + visible_rows) {
                        tab->scroll_row = tab->cursor_row - visible_rows + 1;
                    }
                    app->needs_redraw = true;
                } else if (key == KEY_PAGE_UP) {
                    tab->cursor_row -= visible_rows;
                    if (tab->cursor_row < 0) {
                        tab->cursor_row = 0;
                    }
                    tab->scroll_row = tab->cursor_row;
                    app->needs_redraw = true;
                } else if (key == KEY_PAGE_DOWN) {
                    tab->cursor_row += visible_rows;
                    if (tab->cursor_row >= tab->table->row_count) {
                        tab->cursor_row = tab->table->row_count - 1;
                    }
                    if (tab->cursor_row >= tab->scroll_row + visible_rows) {
                        tab->scroll_row = tab->cursor_row - visible_rows + 1;
                    }
                    app->needs_redraw = true;
                } else if (key == KEY_HOME) {
                    tab->cursor_row = 0;
                    tab->scroll_row = 0;
                    app->needs_redraw = true;
                } else if (key == KEY_END) {
                    tab->cursor_row = tab->table->row_count - 1;
                    if (tab->cursor_row >= tab->scroll_row + visible_rows) {
                        tab->scroll_row = tab->cursor_row - visible_rows + 1;
                    }
                    app->needs_redraw = true;
                } else if (key == KEY_ARROW_LEFT && tab->scroll_col > 0) {
                    tab->scroll_col--;
                    app->needs_redraw = true;
                } else if (key == KEY_ARROW_RIGHT && tab->scroll_col < tab->table->column_count - 1) {
                    tab->scroll_col++;
                    app->needs_redraw = true;
                }
            }
        }
    }
}

bool tui_execute_query(TuiApp* app, const char* query) {
    if (!app || !query) {
        return false;
    }
    
    ASTNode* ast = parse(query);
    if (!ast) {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Error: Failed to parse query");
        return false;
    }
    
    ResultSet* result = evaluate_query(ast);
    releaseNode(ast);
    
    if (!result) {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Error: Failed to execute query");
        return false;
    }
    
    char title[128];
    snprintf(title, sizeof(title), "Query Result (%d rows)", result->row_count);
    
    TuiTab* tab = tui_tab_create(title, TAB_TYPE_QUERY);
    if (!tab) {
        csv_free(result);
        return false;
    }
    
    tab->table = result;
    tab->query = strdup(query);
    
    tui_app_add_tab(app, tab);
    
    snprintf(app->status_message, sizeof(app->status_message), 
            "Query executed: %d rows returned", result->row_count);
    
    return true;
}

bool tui_load_table(TuiApp* app, const char* filename) {
    if (!app || !filename) {
        return false;
    }
    
    CsvTable* table = csv_load(filename, global_csv_config);
    if (!table) {
        snprintf(app->status_message, sizeof(app->status_message), 
                "Error: Failed to load %s", filename);
        return false;
    }
    
    const char* base_name = strrchr(filename, '/');
    if (!base_name) {
        base_name = filename;
    } else {
        base_name++;
    }
    
    TuiTab* tab = tui_tab_create(base_name, TAB_TYPE_TABLE);
    if (!tab) {
        csv_free(table);
        return false;
    }
    
    tab->table = table;
    tab->source_file = strdup(filename);
    
    tui_app_add_tab(app, tab);
    
    snprintf(app->status_message, sizeof(app->status_message), 
            "Loaded %s (%d rows, %d cols)", base_name, table->row_count, table->column_count);
    
    return true;
}
