#include "../../include/tui/tui_render.h"
#include "../../include/tui/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void tui_render_tabs(TuiApp* app) {
    TermSize size = terminal_get_size();
    terminal_cursor_move(1, 1);
    
    write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
    
    for (int i = 0; i < app->tab_count; i++) {
        if (i == app->active_tab) {
            write(STDOUT_FILENO, COLOR_BRIGHT_CYAN, strlen(COLOR_BRIGHT_CYAN));
            write(STDOUT_FILENO, ATTR_REVERSE, strlen(ATTR_REVERSE));
        } else {
            write(STDOUT_FILENO, COLOR_BRIGHT_BLACK, strlen(COLOR_BRIGHT_BLACK));
        }
        
        char tab_label[64];
        snprintf(tab_label, sizeof(tab_label), " %d:%s ", i + 1, app->tabs[i]->title);
        write(STDOUT_FILENO, tab_label, strlen(tab_label));
        
        write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
        write(STDOUT_FILENO, " ", 1);
    }
    
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    terminal_clear_to_end_of_line();
}

void tui_render_table(TuiApp* app, TuiTab* tab) {
    if (!tab || !tab->table) {
        return;
    }
    
    TermSize size = terminal_get_size();
    int content_start_row = 3;
    int content_end_row = size.rows - 2;
    int visible_rows = content_end_row - content_start_row;
    
    CsvTable* table = tab->table;
    
    int col_widths[table->column_count];
    for (int i = 0; i < table->column_count; i++) {
        col_widths[i] = strlen(table->columns[i].name);
        for (int j = 0; j < table->row_count && j < 100; j++) {
            Value* val = csv_get_value(table, j, i);
            if (val) {
                char* str = value_to_string(val);
                if (str) {
                    int len = strlen(str);
                    if (len > col_widths[i]) {
                        col_widths[i] = len;
                    }
                    free(str);
                }
            }
        }
        if (col_widths[i] > 30) {
            col_widths[i] = 30;
        }
        if (col_widths[i] < 3) {
            col_widths[i] = 3;
        }
    }
    
    terminal_cursor_move(content_start_row, 1);
    write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
    write(STDOUT_FILENO, COLOR_BRIGHT_YELLOW, strlen(COLOR_BRIGHT_YELLOW));
    
    int col_offset = 0;
    for (int i = tab->scroll_col; i < table->column_count; i++) {
        if (col_offset + col_widths[i] + 3 > size.cols) {
            break;
        }
        
        char header[128];
        snprintf(header, sizeof(header), " %-*.*s ", 
                col_widths[i], col_widths[i], table->columns[i].name);
        write(STDOUT_FILENO, header, strlen(header));
        write(STDOUT_FILENO, "|", 1);
        
        col_offset += strlen(header) + 1;
    }
    
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    terminal_clear_to_end_of_line();
    
    for (int row = 0; row < visible_rows && (tab->scroll_row + row) < table->row_count; row++) {
        int actual_row = tab->scroll_row + row;
        terminal_cursor_move(content_start_row + 1 + row, 1);
        
        if (actual_row == tab->cursor_row) {
            write(STDOUT_FILENO, ATTR_REVERSE, strlen(ATTR_REVERSE));
        }
        
        col_offset = 0;
        for (int col = tab->scroll_col; col < table->column_count; col++) {
            if (col_offset + col_widths[col] + 3 > size.cols) {
                break;
            }
            
            Value* val = csv_get_value(table, actual_row, col);
            char cell_content[128];
            
            if (val) {
                char* str = value_to_string(val);
                if (str) {
                    snprintf(cell_content, sizeof(cell_content), " %-*.*s ", 
                            col_widths[col], col_widths[col], str);
                    free(str);
                } else {
                    snprintf(cell_content, sizeof(cell_content), " %-*s ", 
                            col_widths[col], "NULL");
                }
            } else {
                snprintf(cell_content, sizeof(cell_content), " %-*s ", 
                        col_widths[col], "NULL");
            }
            
            write(STDOUT_FILENO, cell_content, strlen(cell_content));
            write(STDOUT_FILENO, "|", 1);
            
            col_offset += strlen(cell_content) + 1;
        }
        
        if (actual_row == tab->cursor_row) {
            write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
        }
        
        terminal_clear_to_end_of_line();
    }
}

void tui_render_status_bar(TuiApp* app) {
    TermSize size = terminal_get_size();
    terminal_cursor_move(size.rows - 1, 1);
    
    write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
    write(STDOUT_FILENO, COLOR_BLACK, strlen(COLOR_BLACK));
    write(STDOUT_FILENO, "\x1b[47m", 5);
    
    char mode_str[32];
    switch (app->mode) {
        case MODE_NORMAL:
            snprintf(mode_str, sizeof(mode_str), " NORMAL ");
            break;
        case MODE_QUERY:
            snprintf(mode_str, sizeof(mode_str), " QUERY ");
            break;
        case MODE_SAVE:
            snprintf(mode_str, sizeof(mode_str), " SAVE ");
            break;
        case MODE_HELP:
            snprintf(mode_str, sizeof(mode_str), " HELP ");
            break;
        case MODE_TABLE_LIST:
            snprintf(mode_str, sizeof(mode_str), " TABLES ");
            break;
        default:
            snprintf(mode_str, sizeof(mode_str), " ");
            break;
    }
    
    write(STDOUT_FILENO, mode_str, strlen(mode_str));
    
    if (app->active_tab >= 0 && app->active_tab < app->tab_count) {
        TuiTab* tab = app->tabs[app->active_tab];
        if (tab->table) {
            char info[128];
            snprintf(info, sizeof(info), "| Rows: %d | Cols: %d | Row: %d ", 
                    tab->table->row_count, 
                    tab->table->column_count,
                    tab->cursor_row + 1);
            write(STDOUT_FILENO, info, strlen(info));
        }
    }
    
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    terminal_clear_to_end_of_line();
    
    terminal_cursor_move(size.rows, 1);
    write(STDOUT_FILENO, COLOR_BRIGHT_BLACK, strlen(COLOR_BRIGHT_BLACK));
    write(STDOUT_FILENO, app->status_message, strlen(app->status_message));
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    terminal_clear_to_end_of_line();
}

void tui_render_query_input(TuiApp* app) {
    TermSize size = terminal_get_size();
    int input_row = size.rows - 3;
    
    terminal_cursor_move(input_row, 1);
    write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
    write(STDOUT_FILENO, COLOR_BRIGHT_GREEN, strlen(COLOR_BRIGHT_GREEN));
    write(STDOUT_FILENO, "Query: ", 7);
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    
    if (app->query_buffer) {
        write(STDOUT_FILENO, app->query_buffer, strlen(app->query_buffer));
    }
    
    terminal_clear_to_end_of_line();
    terminal_cursor_show();
    terminal_cursor_move(input_row, 8 + app->query_cursor_pos);
}

void tui_render_save_input(TuiApp* app) {
    TermSize size = terminal_get_size();
    int input_row = size.rows - 3;
    
    terminal_cursor_move(input_row, 1);
    write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
    write(STDOUT_FILENO, COLOR_BRIGHT_GREEN, strlen(COLOR_BRIGHT_GREEN));
    write(STDOUT_FILENO, "Save as: ", 9);
    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    
    if (app->save_buffer) {
        write(STDOUT_FILENO, app->save_buffer, strlen(app->save_buffer));
    }
    
    terminal_clear_to_end_of_line();
    terminal_cursor_show();
    terminal_cursor_move(input_row, 10 + app->save_cursor_pos);
}

void tui_render_help(TuiApp* app) {
    int start_row = 4;
    
    const char* help_lines[] = {
        "cq TUI - Keyboard Shortcuts",
        "",
        "Navigation:",
        "  Arrow Keys      - Move cursor / Edit query (in : mode)",
        "  Page Up/Down    - Scroll pages",
        "  Home/End        - Jump to start/end / Query start/end",
        "",
        "Tabs:",
        "  Ctrl+N          - Next tab (works in query mode too)",
        "  Ctrl+P          - Previous tab (works in query mode too)",
        "  Ctrl+W          - Close current tab",
        "",
        "Actions:",
        "  :               - Enter query mode",
        "  t               - Show table list",
        "  w               - Enter save mode (save current table)",
        "  h               - Show this help",
        "  q or Ctrl+Q     - Quit",
        "",
        "Query Mode:",
        "  Enter           - Execute query",
        "  ESC             - Cancel query",
        "  Arrow Keys      - Move cursor in query",
        "  Ctrl+N/P        - Switch tabs while editing",
        "",
        "Save Mode:",
        "  Enter           - Save to entered filename",
        "  ESC             - Cancel save",
        "  Arrow Keys      - Move cursor in filename",
        "  Ctrl+N/P        - Switch tabs while editing",
        "",
        "Press any key to return...",
        NULL
    };
    
    terminal_clear_screen();
    tui_render_tabs(app);
    
    int row = start_row;
    for (int i = 0; help_lines[i] != NULL; i++) {
        terminal_cursor_move(row++, 3);
        
        if (i == 0) {
            write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
            write(STDOUT_FILENO, COLOR_BRIGHT_CYAN, strlen(COLOR_BRIGHT_CYAN));
        } else if (strstr(help_lines[i], ":") && strlen(help_lines[i]) > 2 && help_lines[i][0] != ' ') {
            write(STDOUT_FILENO, COLOR_BRIGHT_YELLOW, strlen(COLOR_BRIGHT_YELLOW));
        }
        
        write(STDOUT_FILENO, help_lines[i], strlen(help_lines[i]));
        write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    }
}

void tui_render_table_list(TuiApp* app) {
    TermSize size = terminal_get_size();
    int list_width = 40;
    int list_height = size.rows - 6;
    int start_row = 4;
    int start_col = (size.cols - list_width) / 2;
    
    tui_render_box(start_row - 1, start_col - 1, list_width + 2, list_height + 2, "Tables");
    
    for (int i = 0; i < list_height && i < app->table_count; i++) {
        terminal_cursor_move(start_row + i, start_col);
        
        if (i == app->table_selected) {
            write(STDOUT_FILENO, ATTR_REVERSE, strlen(ATTR_REVERSE));
            write(STDOUT_FILENO, COLOR_BRIGHT_GREEN, strlen(COLOR_BRIGHT_GREEN));
        }
        
        const char* filename = strrchr(app->table_files[i], '/');
        if (!filename) {
            filename = app->table_files[i];
        } else {
            filename++;
        }
        
        char line[64];
        snprintf(line, sizeof(line), " %-*.*s", list_width - 2, list_width - 2, filename);
        write(STDOUT_FILENO, line, strlen(line));
        
        if (i == app->table_selected) {
            write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
        }
    }
}

void tui_render_box(int row, int col, int width, int height, const char* title) {
    terminal_cursor_move(row, col);
    write(STDOUT_FILENO, "┌", strlen("┌"));
    
    if (title) {
        char title_str[128];
        snprintf(title_str, sizeof(title_str), " %s ", title);
        write(STDOUT_FILENO, ATTR_BOLD, strlen(ATTR_BOLD));
        write(STDOUT_FILENO, title_str, strlen(title_str));
        write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
        
        for (int i = strlen(title_str); i < width - 2; i++) {
            write(STDOUT_FILENO, "─", strlen("─"));
        }
    } else {
        for (int i = 0; i < width - 2; i++) {
            write(STDOUT_FILENO, "─", strlen("─"));
        }
    }
    write(STDOUT_FILENO, "┐", strlen("┐"));
    
    for (int i = 1; i < height - 1; i++) {
        terminal_cursor_move(row + i, col);
        write(STDOUT_FILENO, "│", strlen("│"));
        terminal_cursor_move(row + i, col + width - 1);
        write(STDOUT_FILENO, "│", strlen("│"));
    }
    
    terminal_cursor_move(row + height - 1, col);
    write(STDOUT_FILENO, "└", strlen("└"));
    for (int i = 0; i < width - 2; i++) {
        write(STDOUT_FILENO, "─", strlen("─"));
    }
    write(STDOUT_FILENO, "┘", strlen("┘"));
}

void tui_render_text_centered(int row, const char* text) {
    TermSize size = terminal_get_size();
    int col = (size.cols - strlen(text)) / 2;
    terminal_cursor_move(row, col);
    write(STDOUT_FILENO, text, strlen(text));
}
