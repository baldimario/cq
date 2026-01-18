#include "../../include/tui/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

static struct termios orig_termios;
static bool terminal_initialized = false;

bool terminal_init(void) {
    if (terminal_initialized) {
        return true;
    }
    
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        perror("tcgetattr");
        return false;
    }
    
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        perror("tcsetattr");
        return false;
    }
    
    terminal_initialized = true;
    terminal_clear_screen();
    terminal_cursor_hide();
    
    return true;
}

void terminal_cleanup(void) {
    if (!terminal_initialized) {
        return;
    }
    
    terminal_cursor_show();
    terminal_clear_screen();
    terminal_cursor_move(1, 1);
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    terminal_initialized = false;
}

TermSize terminal_get_size(void) {
    struct winsize ws;
    TermSize size = {24, 80};
    
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return size;
    }
    
    size.rows = ws.ws_row;
    size.cols = ws.ws_col;
    return size;
}

void terminal_cursor_move(int row, int col) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void terminal_cursor_hide(void) {
    write(STDOUT_FILENO, "\x1b[?25l", 6);
}

void terminal_cursor_show(void) {
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

void terminal_clear_screen(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

void terminal_clear_line(void) {
    write(STDOUT_FILENO, "\x1b[2K", 4);
}

void terminal_clear_to_end_of_line(void) {
    write(STDOUT_FILENO, "\x1b[K", 3);
}

int terminal_read_key(void) {
    char c;
    int nread;
    
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            return -1;
        }
    }
    
    if (c == '\x1b') {
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return c;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return c;
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return c;
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return KEY_HOME;
                        case '3': return KEY_DELETE;
                        case '4': return KEY_END;
                        case '5': return KEY_PAGE_UP;
                        case '6': return KEY_PAGE_DOWN;
                        case '7': return KEY_HOME;
                        case '8': return KEY_END;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return KEY_ARROW_UP;
                    case 'B': return KEY_ARROW_DOWN;
                    case 'C': return KEY_ARROW_RIGHT;
                    case 'D': return KEY_ARROW_LEFT;
                    case 'H': return KEY_HOME;
                    case 'F': return KEY_END;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return KEY_HOME;
                case 'F': return KEY_END;
                case 'P': return KEY_F1;
                case 'Q': return KEY_F2;
                case 'R': return KEY_F3;
                case 'S': return KEY_F4;
            }
        }
        
        return c;
    }
    
    return c;
}
