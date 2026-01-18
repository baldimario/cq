#include "../../include/tui/tui_core.h"
#include "../../include/tui/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TuiTab* tui_tab_create(const char* title, TabType type) {
    TuiTab* tab = calloc(1, sizeof(TuiTab));
    if (!tab) {
        return NULL;
    }
    
    tab->title = strdup(title);
    if (!tab->title) {
        free(tab);
        return NULL;
    }
    
    tab->type = type;
    tab->table = NULL;
    tab->scroll_row = 0;
    tab->scroll_col = 0;
    tab->cursor_row = 0;
    tab->cursor_col = 0;
    tab->source_file = NULL;
    tab->query = NULL;
    
    return tab;
}

void tui_tab_destroy(TuiTab* tab) {
    if (!tab) {
        return;
    }
    
    free(tab->title);
    if (tab->table) {
        csv_free(tab->table);
    }
    free(tab->source_file);
    free(tab->query);
    free(tab);
}

bool tui_app_add_tab(TuiApp* app, TuiTab* tab) {
    if (!app || !tab) {
        return false;
    }
    
    if (app->tab_count >= app->tab_capacity) {
        int new_capacity = app->tab_capacity == 0 ? 8 : app->tab_capacity * 2;
        TuiTab** new_tabs = realloc(app->tabs, new_capacity * sizeof(TuiTab*));
        if (!new_tabs) {
            return false;
        }
        app->tabs = new_tabs;
        app->tab_capacity = new_capacity;
    }
    
    app->tabs[app->tab_count] = tab;
    app->active_tab = app->tab_count;
    app->tab_count++;
    app->needs_redraw = true;
    
    return true;
}

void tui_app_close_tab(TuiApp* app, int index) {
    if (!app || index < 0 || index >= app->tab_count) {
        return;
    }
    
    tui_tab_destroy(app->tabs[index]);
    
    for (int i = index; i < app->tab_count - 1; i++) {
        app->tabs[i] = app->tabs[i + 1];
    }
    
    app->tab_count--;
    
    if (app->tab_count == 0) {
        app->active_tab = -1;
    } else if (app->active_tab >= app->tab_count) {
        app->active_tab = app->tab_count - 1;
    } else if (app->active_tab > index) {
        app->active_tab--;
    }
    
    app->needs_redraw = true;
}

void tui_app_switch_tab(TuiApp* app, int index) {
    if (!app || index < 0 || index >= app->tab_count) {
        return;
    }
    
    app->active_tab = index;
    app->needs_redraw = true;
}

void tui_app_next_tab(TuiApp* app) {
    if (!app || app->tab_count == 0) {
        return;
    }
    
    app->active_tab = (app->active_tab + 1) % app->tab_count;
    app->needs_redraw = true;
}

void tui_app_prev_tab(TuiApp* app) {
    if (!app || app->tab_count == 0) {
        return;
    }
    
    app->active_tab = (app->active_tab - 1 + app->tab_count) % app->tab_count;
    app->needs_redraw = true;
}
