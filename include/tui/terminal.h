#ifndef TUI_TERMINAL_H
#define TUI_TERMINAL_H

#include <stdbool.h>
#include <stddef.h>

/* terminal dimensions */
typedef struct {
    int rows;
    int cols;
} TermSize;

/* terminal initialization and cleanup */
bool terminal_init(void);
void terminal_cleanup(void);

/* terminal size */
TermSize terminal_get_size(void);

/* cursor control */
void terminal_cursor_move(int row, int col);
void terminal_cursor_hide(void);
void terminal_cursor_show(void);

/* screen control */
void terminal_clear_screen(void);
void terminal_clear_line(void);
void terminal_clear_to_end_of_line(void);

/* input handling */
int terminal_read_key(void);

/* special key codes */
#define KEY_ARROW_UP    1000
#define KEY_ARROW_DOWN  1001
#define KEY_ARROW_LEFT  1002
#define KEY_ARROW_RIGHT 1003
#define KEY_PAGE_UP     1004
#define KEY_PAGE_DOWN   1005
#define KEY_HOME        1006
#define KEY_END         1007
#define KEY_DELETE      1008
#define KEY_BACKSPACE   127
#define KEY_ESCAPE      27
#define KEY_TAB         9
#define KEY_ENTER       13
#define KEY_F1          1010
#define KEY_F2          1011
#define KEY_F3          1012
#define KEY_F4          1013
#define KEY_CTRL_Q      17
#define KEY_CTRL_S      19
#define KEY_CTRL_W      23
#define KEY_CTRL_N      14
#define KEY_CTRL_P      16

/* colors */
#define COLOR_RESET     "\x1b[0m"
#define COLOR_BLACK     "\x1b[30m"
#define COLOR_RED       "\x1b[31m"
#define COLOR_GREEN     "\x1b[32m"
#define COLOR_YELLOW    "\x1b[33m"
#define COLOR_BLUE      "\x1b[34m"
#define COLOR_MAGENTA   "\x1b[35m"
#define COLOR_CYAN      "\x1b[36m"
#define COLOR_WHITE     "\x1b[37m"
#define COLOR_BRIGHT_BLACK   "\x1b[90m"
#define COLOR_BRIGHT_RED     "\x1b[91m"
#define COLOR_BRIGHT_GREEN   "\x1b[92m"
#define COLOR_BRIGHT_YELLOW  "\x1b[93m"
#define COLOR_BRIGHT_BLUE    "\x1b[94m"
#define COLOR_BRIGHT_MAGENTA "\x1b[95m"
#define COLOR_BRIGHT_CYAN    "\x1b[96m"
#define COLOR_BRIGHT_WHITE   "\x1b[97m"

/* text attributes */
#define ATTR_BOLD       "\x1b[1m"
#define ATTR_DIM        "\x1b[2m"
#define ATTR_UNDERLINE  "\x1b[4m"
#define ATTR_REVERSE    "\x1b[7m"

#endif
