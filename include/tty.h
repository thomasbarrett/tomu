#ifndef TTY_H
#define TTY_H

#include <termios.h>

typedef struct termios tty_state_t;

/**
 * Enable raw mode on the given tty `fd` and store the previous tty state
 * in `state`.
 * 
 * \param fd the tty.
 * \param state the previous tty state.
 * \return On success, 0 is returned. On error, -1 is returned and errno is
 *         set to indicate the error.
 */
int tty_make_raw(int fd, tty_state_t *state);

/**
 * Restore the tty `fd` to the given `state`.
 * 
 * \param fd the tty.
 * \param state the tty state.
 * \return On success, 0 is returned. On error, -1 is returned and errno is
 *         set to indicate the error.
 */
int tty_restore(int fd, tty_state_t state);

#endif
