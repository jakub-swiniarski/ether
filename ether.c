#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "config.h"

/* macros */
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

/* structs */
typedef struct {
    char *b;
    int len;
} ABuf; /* append buffer */

typedef struct {
    int size;
    char *chars;
} Row; /* TODO: this is not in alphabetical order */

typedef struct {
    int cur_x, cur_y;
    int screen_rows;
    int screen_cols;
    int n_rows;
    Row row;
    struct termios orig_termios;
} Editor;

/* function declarations */
static void ab_append(ABuf *ab, const char *s, int len);
static void ab_free(ABuf *ab);
static void die(const char *s);
static void disable_raw_mode(void);
static void draw_rows(ABuf *ab);
static void enable_raw_mode(void);
static int get_cursor_position(int *rows, int *cols);
static int get_window_size(int *rows, int *cols);
static void init(void);
static void process_key(void);
static char read_key(void);
static void refresh_screen(void);

/* variables */
static Editor editor;

/* function implementations  */
void ab_append(ABuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL)
        return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void ab_free(ABuf *ab) {
    free(ab->b);
}

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disable_raw_mode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.orig_termios) == -1)
        die("tcsetattr");
}

void draw_rows(ABuf *ab) {
    int y;

    for (y = 0; y < editor.screen_rows; y++) {
        ab_append(ab, "~", 1);

        ab_append(ab, "\x1b[K", 3);
        if (y < editor.screen_rows - 1)
            ab_append(ab, "\r\n", 2);
    }
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &editor.orig_termios) == -1)
        die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = editor.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return -1;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return get_cursor_position(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void init(void) {
    editor.cur_x=0;
    editor.cur_y=0;
    editor.n_rows=0;
        
    if (get_window_size(&editor.screen_rows, &editor.screen_cols) == -1)
        die("get_window_size");
}

void process_key(void) {
    char c = read_key();

    switch (c) {
        /* quit */
        case CTRL_KEY(KEY_QUIT):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
            break;

        /* move the cursor */
        case KEY_LEFT:
            if (editor.cur_x != 0)
                editor.cur_x--;
            break;
        case KEY_DOWN:
            if (editor.cur_y != editor.screen_rows - 1)
                editor.cur_y++;
            break;
        case KEY_UP:
            if (editor.cur_y != 0)
                editor.cur_y--;
            break;
        case KEY_RIGHT:
            if (editor.cur_x != editor.screen_cols - 1)
                editor.cur_x++;
            break;
    }
}

char read_key(void) {
    int n_read;
    char c;

    while ((n_read = read(STDIN_FILENO, &c, 1)) != 1) {
        if (n_read == -1 && errno != EAGAIN)
            die("read");
    }

    return c;
}

void refresh_screen(void) {
    ABuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    draw_rows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editor.cur_y + 1, editor.cur_x + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

int main(void) {
    enable_raw_mode();
    init();

    while (1) {
        refresh_screen();
        process_key();
    }

    return 0;
}
