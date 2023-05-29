#include <tty.h>

int tty_make_raw(int fd, tty_state_t *state) {
    if (tcgetattr(fd, state) < 0) {
        return -1;
    }

    struct termios raw = *state;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) {
        return -1;
    }

    return 0;
}

int tty_restore(int fd, tty_state_t state) {
    return tcsetattr(fd, TCSAFLUSH, &state);
}
