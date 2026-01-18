#ifndef TUI_RENDER_H
#define TUI_RENDER_H

#include "tui_core.h"

/* render different UI components */
void tui_render_tabs(TuiApp* app);
void tui_render_table(TuiApp* app, TuiTab* tab);
void tui_render_status_bar(TuiApp* app);
void tui_render_query_input(TuiApp* app);
void tui_render_save_input(TuiApp* app);
void tui_render_help(TuiApp* app);
void tui_render_table_list(TuiApp* app);

/* helper rendering functions */
void tui_render_box(int row, int col, int width, int height, const char* title);
void tui_render_text_centered(int row, const char* text);

#endif
