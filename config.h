#ifndef CONFIG_H
#define CONFIG_H

/* keys */
#define KEY_QUIT 'q'
#define KEY_SAVE 'w'
#define KEY_DELETE_CHAR 'x'
#define KEY_DELETE_ROW 'd' 

#define KEY_LEFT 'h'
#define KEY_DOWN 'j'
#define KEY_UP 'k'
#define KEY_RIGHT 'l'

#define KEY_INSERT 'i'
#define KEY_COMMAND ':'

/* modes */
static const char *modes[] = { "NORMAL", "INSERT", "COMMAND" }; /* display names */

/* other */
#define TAB_STOP 4

#endif /* CONFIG_H */
