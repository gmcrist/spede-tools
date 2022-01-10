/*  flash/source/getch.c */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/getch.c,v 1.2
 * 2000/02/19 21:27:07 aleks Exp $ */

/*
    The following definitions are from VAX/Ultrix man pag for tty(4).

        CBREAK = This mode eliminates the chacater, wor and line editing
        input facilities, making the input chacater available to the user
        program as it is typed.  Flow control, literal text, and interrupt
        processing are still done in this mode.  Output processing is done.

        CBREAK is a sort of half-cooked mode.  Programs can read each
        character as soon as typed, instead of waiting for a full line;
        all processing is done except the input editing: character and word
        erase and line kill, input reprint, and the special treatment of
        \ and EOT as disabled.

        ECHO = Echo.  Full duplex.

        CRMOD = Map CR to LF; echo LF or CR as CR-LF.

*/

#include "flash.h"
#include <sys/stat.h>

#ifdef HOST_POSIX
#include <termios.h>
#else
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termio.h>
#endif

extern char erase_char;
extern char kill_char;

#ifdef HOST_POSIX
/**  IEEE version to get one character  **/
int getch() {
    int            std_in = fileno(stdin);
    auto char      c;
    int            x = 0;
    struct termios otty, ntty;

    tcgetattr(std_in, &otty);
    erase_char = otty.c_cc[VERASE];
    kill_char  = otty.c_cc[VKILL];

    ntty = otty;
    ntty.c_lflag &= ~(ECHO | ICANON); /* was flag "CBREAK" */
    ntty.c_lflag |= ISIG;
    ntty.c_iflag = ICRNL | IGNPAR;
    ntty.c_oflag |= OPOST | ONLCR; /* was flag "CRMOD" */
    ntty.c_cc[VMIN]  = 1;          /* Wait forever on a single char. */
    ntty.c_cc[VTIME] = 0;
    ntty.c_cc[VKILL] = _POSIX_VDISABLE; /* no kill line */
    /* tell the device driver to use them */
    tcsetattr(std_in, TCSANOW, &ntty);
    if (read(std_in, &c, 1) <= 0)
        x = -1;
    tcsetattr(std_in, TCSANOW, &otty);

    return (x == 0) ? c : -1;
}

#else
/**  Old System V version to get one character  **/
int getch() {
    int           c;
    struct sgttyb otty, ntty;
    ioctl(fileno(stdin), TIOCGETP, &otty);
    erase_char = otty.sg_erase;
    kill_char  = otty.sg_kill;
    ntty       = otty;
    /*ntty.sg_flags &= (~ECHO | CRMOD);*/
    ntty.sg_flags &= ~ECHO;
    ntty.sg_flags |= CRMOD;
    ntty.sg_flags |= CBREAK;
    /* tell the device driver to use them */
    ioctl(fileno(stdin), TIOCSETP, &ntty);
    c = getc(stdin);
    ioctl(fileno(stdin), TIOCSETP, &otty);
    return c;
}
#endif
