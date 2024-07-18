#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "config.h"

/* macros */
#define ABUF_INIT { NULL, 0 }

/* enums */
enum { mode_normal, mode_insert, mode_command };

/* structs */
typedef struct {
    char *b;
    int len;
} ABuf; /* append buffer */

typedef struct {
    int size;
    int render_size;
    char *chars;
    char *render;
} Row;

typedef struct {
    int cur_x, cur_y;
    int row_offset;
    int col_offset;
    int screen_rows;
    int screen_cols;
    int n_rows;
    Row *row;
    char *filename;
    struct termios orig_termios;
} Editor;

/* function declarations */
static void ab_append(ABuf *ab, const char *s, int len);
static void ab_free(ABuf *ab);
static void append_row(char *s, size_t len);
static void delete_row(int at);
static void die(const char *s);
static void disable_raw_mode(void);
static void draw_rows(ABuf *ab);
static void draw_bar(ABuf *ab);
static void enable_raw_mode(void);
static void file_open(char *filename);
static void file_save(void);
static void free_row(Row *row);
static int get_cursor_position(int *rows, int *cols);
static int get_window_size(int *rows, int *cols);
static void init(void);
static void insert_char(char c);
static void process_key(void);
static void quit(void);
static char read_key(void);
static void refresh_screen(void);
static void row_delete_char(Row *row, int at);
static void row_insert_char(Row *row, int at, char c);
static char *rows_to_str(int buflen);
static void scroll(void);
static void update_row(Row *row);

/* variables */
static Editor editor;
static int mode;

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

void append_row(char *s, size_t len) {
    editor.row = realloc(editor.row, sizeof(Row) * (editor.n_rows + 1));

    int at = editor.n_rows;
    editor.row[at].size = len;
    editor.row[at].chars = malloc(len + 1);
    memcpy(editor.row[at].chars, s, len);
    editor.row[at].chars[len] = '\0';

    editor.row[at].render_size = 0;
    editor.row[at].render = NULL;
    update_row(&editor.row[at]);

    editor.n_rows++;
}

static void delete_row(int at) {
    Row *row;

    if (at >= editor.n_rows)
        return;
    row = editor.row + at;
    free_row(row);
    memmove(editor.row + at, editor.row + at + 1, sizeof(editor.row[0]) * (editor.n_rows - at - 1));
    editor.n_rows--;
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
        int file_row = y + editor.row_offset;

        if (file_row < editor.n_rows) {
            int len = editor.row[file_row].render_size - editor.col_offset;
            if (len < 0)
                len = 0;
            if (len > editor.screen_cols)
                len = editor.screen_cols;
            ab_append(ab, &editor.row[file_row].render[editor.col_offset], len);
        }

        ab_append(ab, "\x1b[K", 3);
        ab_append(ab, "\r\n", 2);
    }
}

void draw_bar(ABuf *ab) {
    ab_append(ab, "\x1b[7m", 4);

    char status[64], r_status[64];
    int len = snprintf(status, sizeof(status), "%s", modes[mode]);
    int r_len = snprintf(r_status, sizeof(r_status), "%.20s - %d/%d", editor.filename ? editor.filename : "[NO NAME]", editor.cur_y + 1, editor.n_rows);
    
    if (len > editor.screen_cols)
        len = editor.screen_cols;

    ab_append(ab, status, len);
    
    while (len < editor.screen_cols) {
        if (editor.screen_cols - len == r_len) {
            ab_append(ab, r_status, r_len);
            break;
        } else {
            ab_append(ab, " ", 1);
            len++;
        }
    }

    ab_append(ab, "\x1b[m", 3);
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

void file_open(char *filename) {
    free(editor.filename);
    editor.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp)
        die("fopen");

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        while (line_len > 0 
        && (line[line_len - 1] == '\n'
        || line[line_len - 1] == '\r'))
            line_len--;
        append_row(line, line_len);
    }
    free(line);
    fclose(fp);
}

void file_save(void) {
    int len;
    char *buf = rows_to_str(len);
    FILE *file = fopen(editor.filename, "w");
    if (file == NULL)
        goto writeerr;
    fputs(buf, file);
    fclose(file);
    free(buf);
    return;

writeerr:
    free(buf);
    return;
}

void free_row(Row *row) {
    free(row->render);
    free(row->chars);
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
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void init(void) {
    editor.cur_x = 0;
    editor.cur_y = 0;
    editor.row_offset = 0;
    editor.col_offset = 0;
    editor.n_rows = 0;
    editor.row = NULL;
    editor.filename = NULL;

    mode = mode_normal;
        
    if (get_window_size(&editor.screen_rows, &editor.screen_cols) == -1)
        die("get_window_size");
    editor.screen_rows -= 1;
}

void insert_char(char c) {
    if (editor.cur_y == editor.n_rows)
        append_row("", 0);
    row_insert_char(&editor.row[editor.cur_y], editor.cur_x, c);
    editor.cur_x++;
}

void process_key(void) {
    char c = read_key();
    Row *row = (editor.cur_y >= editor.n_rows) ? NULL : &editor.row[editor.cur_y];

    if (c == 27) /* escape key */
        mode = mode_normal;
    else if (mode == mode_normal) {
        switch (c) {
            /* move the cursor */
            case KEY_LEFT:
                if (editor.cur_x != 0)
                    editor.cur_x--;
                break;
            case KEY_DOWN:
                if (editor.cur_y < editor.n_rows)
                    editor.cur_y++;
                break;
            case KEY_UP:
                if (editor.cur_y != 0)
                    editor.cur_y--;
                break;
            case KEY_RIGHT:
                if (row && editor.cur_x < row->size)
                    editor.cur_x++;
                break;

            /* deleting */
            case KEY_DELETE_CHAR:
                row_delete_char(row, editor.cur_x);
                break;
            case KEY_DELETE_ROW:
                delete_row(editor.cur_y);
                break;

            /* modes */
            case KEY_INSERT:
                mode = mode_insert;
                break;
            case KEY_COMMAND:
                mode = mode_command;
                break;
        }
    } else if (mode == mode_insert)
        insert_char(c);
    else if (mode == mode_command) {
        switch (c) {
            case KEY_QUIT:
                quit();
                break;
            /* saving */
            case KEY_SAVE:
                file_save();
                mode = mode_normal;
                break;
        }
    }

    row = (editor.cur_y >= editor.n_rows) ? NULL : &editor.row[editor.cur_y];
    int row_len = row ? row->size : 0;
    if (editor.cur_x > row_len)
        editor.cur_x = row_len;
}

void quit(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    exit(0);
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
    scroll();

    ABuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    draw_rows(&ab);
    draw_bar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editor.cur_y - editor.row_offset + 1, editor.cur_x - editor.col_offset + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void row_delete_char(Row *row, int at) {
    if (row->size <= at)
        return;
    memmove(row->chars + at, row->chars + at + 1, row->size - at);
    update_row(row);
    row->size--;
}


void row_insert_char(Row *row, int at, char c) {
    if (at < 0 || at > row->size)
        at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    update_row(row);
}

char *rows_to_str(int buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int i;

    for (i = 0; i < editor.n_rows; i++)
        totlen += editor.row[i].size + 1; /* +1 is for "\n" at end of every row */
    buflen = totlen;
    totlen++; /* also make space for nulterm */

    p = buf = malloc(totlen);
    for (i = 0; i < editor.n_rows; i++) {
        memcpy(p, editor.row[i].chars, editor.row[i].size);
        p += editor.row[i].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

void scroll(void) {
    /* vertical */
    if (editor.cur_y < editor.row_offset)
        editor.row_offset = editor.cur_y;
    if (editor.cur_y >= editor.row_offset + editor.screen_rows)
        editor.row_offset = editor.cur_y - editor.screen_rows + 1;

    /* horizontal */
    if (editor.cur_x < editor.col_offset)
        editor.col_offset = editor.cur_x;
    if (editor.cur_x >= editor.col_offset + editor.screen_cols)
        editor.col_offset = editor.cur_x - editor.screen_cols + 1;
}

void update_row(Row *row) {
    int tabs = 0;
    int j;

    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t')
            tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_STOP != 0)
                row->render[idx++] = ' ';
        } else
            row->render[idx++] = row->chars[j];
    }

    row->render[idx] = '\0';
    row->render_size = idx;
}

int main(int argc, char *argv[]) {
    enable_raw_mode();
    init();
    if (argc >= 2)
        file_open(argv[1]);

    while (1) {
        refresh_screen();
        process_key();
    }

    return 0;
}
