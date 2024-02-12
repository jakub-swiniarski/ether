#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "config.h"

/* macros */
#define CTRL_KEY(k) ((k) & 0x1f)

/* structs */
typedef struct {
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
} Config;

/* function declarations */
static void die(const char *s);
static void disable_raw_mode(void);
static void draw_rows(void);
static void enable_raw_mode(void);
static int get_window_size(int *rows, int *cols);
static void init(void);
static void process_key(void);
static char read_key(void);
static void refresh_screen(void);

/* variables */
static Config config;

/* function implementations  */
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); /* clear the screen */
    write(STDOUT_FILENO, "\x1b[H", 3); /* move the cursor to the top-left corner */

    perror(s);
    exit(1);
}

void disable_raw_mode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.orig_termios) == -1)
        die("tcsetattr");
}

void draw_rows(void) {
    int y;
    for (y = 0; y < config.screen_rows; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &config.orig_termios) == -1)
        die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = config.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0)
        return -1;
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void init(void) {
    if (get_window_size(&config.screen_rows, &config.screen_cols) == -1)
        die("get_window_size");
}

void process_key(void) {
    char c = read_key();

    switch (c) {
        case CTRL_KEY(KEY_QUIT):
            write(STDOUT_FILENO, "\x1b[2J", 4); /* clear the screen */
            write(STDOUT_FILENO, "\x1b[H", 3); /* move the cursor to the top-left corner */

            exit(0);
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
    write(STDOUT_FILENO, "\x1b[2J", 4); /* clear the screen */
    write(STDOUT_FILENO, "\x1b[H", 3); /* move the cursor to the top-left corner */

    draw_rows();

    write(STDOUT_FILENO, "\x1b[H", 3);
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
