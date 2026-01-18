#ifndef TUI_CORE_H
#define TUI_CORE_H

#include <stdbool.h>
#include "../csv_reader.h"

/* forward declarations */
typedef struct TuiApp TuiApp;
typedef struct TuiTab TuiTab;

/* TUI modes */
typedef enum {
    MODE_NORMAL,      /* browsing tables */
    MODE_QUERY,       /* entering SQL query */
    MODE_SAVE,        /* entering save filename */
    MODE_HELP,        /* help screen */
    MODE_TABLE_LIST,  /* table selection sidebar */
} TuiMode;

/* tab content types */
typedef enum {
    TAB_TYPE_TABLE,   /* displaying a CSV table */
    TAB_TYPE_QUERY,   /* displaying query results */
} TabType;

/* tab structure */
struct TuiTab {
    char* title;          /* tab display name */
    TabType type;
    
    /* content */
    CsvTable* table;      /* loaded table or query result */
    
    /* pagination */
    int scroll_row;       /* first visible row */
    int scroll_col;       /* first visible column */
    int cursor_row;       /* cursor position in table */
    int cursor_col;
    
    /* metadata */
    char* source_file;    /* original CSV file path (for TAB_TYPE_TABLE) */
    char* query;          /* SQL query that generated this (for TAB_TYPE_QUERY) */
};

/* application state */
struct TuiApp {
    /* tabs */
    TuiTab** tabs;
    int tab_count;
    int tab_capacity;
    int active_tab;
    
    /* mode */
    TuiMode mode;
    
    /* workspace */
    char* workspace_dir;  /* directory containing CSV files */
    
    /* query input buffer */
    char* query_buffer;
    int query_buffer_size;
    int query_cursor_pos;
    
    /* save input buffer */
    char* save_buffer;
    int save_buffer_size;
    int save_cursor_pos;
    
    /* table list */
    char** table_files;   /* available CSV files */
    int table_count;
    int table_selected;
    
    /* status message */
    char status_message[256];
    
    /* flags */
    bool running;
    bool needs_redraw;
};

/* application lifecycle */
TuiApp* tui_app_create(const char* workspace_dir);
void tui_app_destroy(TuiApp* app);
int tui_app_run(TuiApp* app);

/* tab management */
TuiTab* tui_tab_create(const char* title, TabType type);
void tui_tab_destroy(TuiTab* tab);
bool tui_app_add_tab(TuiApp* app, TuiTab* tab);
void tui_app_close_tab(TuiApp* app, int index);
void tui_app_switch_tab(TuiApp* app, int index);
void tui_app_next_tab(TuiApp* app);
void tui_app_prev_tab(TuiApp* app);

/* rendering */
void tui_render(TuiApp* app);

/* event handling */
void tui_handle_key(TuiApp* app, int key);

/* query execution */
bool tui_execute_query(TuiApp* app, const char* query);

/* table loading */
bool tui_load_table(TuiApp* app, const char* filename);

/* workspace scanning */
void tui_scan_workspace(TuiApp* app);

/* export */
bool tui_export_table(TuiApp* app, const char* filename);

#endif
