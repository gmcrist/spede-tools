/**
 *
 * Original implementation
 *   Sudhir, May 1993
 */

#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include "spede.h"

struct termios otty, ntty;

static pid_t child_id;
static int   status; /* Status from wait() after fork() */

int        rhdl, whdl;
char       std_buf[BUFSIZ];
int        ctrlc_hit;
extern int hndxhd, hndxtl, cur_cnt; /* History index and current ptr */

extern int flsh(/* int, char * */);
/* Commnad line is "flint port" */
char *dev;

/* LOCAL forwards: */
void cleanup();
void ctrlc_handler();
void child_cleanup();

/* ----------------------------------------------------------------------- */

/**
 *	Create two processes upont this port.  The child listens to
 *	the user, and the parent talks with the target computer.
 */

int    main(argc, argv)
int    argc;
char **argv;
{
    char buffer[256];
    char c;
    int  i, n;

    /* Open the TTY device */
    if (argc != 2) {
        printf("\nUsage : fl <dev>\n");
        exit(1);
    }
    dev = argv[1];

    rhdl = open(dev, O_RDONLY);
    if (rhdl < 0) {
        printf("\n");
        perror("Error opening device");
        exit(1);
    }

    printf("\n You are in the Flames command shell \n");
    printf("Please refer to the Manual for the list of commands\n");

    child_id = fork();
    if (child_id == 0) {
        /***************************/
        /****   CHILD  PROCESS  ****/
        /***************************/
        usleep(200000);
        whdl = open(dev, O_WRONLY);
        if (whdl <= 0) {
            printf("Error in opening device \n");
            exit(1);
        }
        tcgetattr(whdl, &ntty);

        signal(SIGHUP, child_cleanup);
        signal(SIGINT, ctrlc_handler);
        signal(SIGQUIT, child_cleanup);
        signal(SIGTERM, child_cleanup);
        signal(SIGSTOP, child_cleanup);
        signal(SIGTSTP, child_cleanup);

        otty = ntty;
        ntty.c_lflag &= ~(ECHO | ICANON);
        ntty.c_lflag |= ISIG;
        ntty.c_iflag = 0;
        ntty.c_cflag &= ~(CBAUD | (CSIZE | PARENB)); /* Punch hole. */
        ntty.c_cflag |= (CS8 | FLASH_BAUD);
        ntty.c_cc[VMIN]  = 1; /* Wait forever for at least one char. */
        ntty.c_cc[VTIME] = 0;
        ntty.c_cc[VKILL] = _POSIX_VDISABLE; /* no kill line */
        tcsetattr(whdl, TCSANOW, &ntty);

        hndxhd    = 0;
        hndxtl    = 0;
        cur_cnt   = 0;
        ctrlc_hit = 0;
        c         = 0xd;
        write(whdl, &c, 1);

        /*
         *  Read from the user and send to target.  flsh() gets
         *  a line of text.  We send out the RETURN afterwards.
         *  FLINT_EXIT_CH must occur right after RETURN (first char).
         */
        while (1) {
            buffer[0] = (char)0;
            if ((!flsh(whdl, buffer)) || (ctrlc_hit)) {
                printf("Invalid Command\n");
                c = 0xd;
                write(whdl, &c, 1);
                continue;
            }

            if (buffer[0] == FLINT_EXIT_CH) {
                child_cleanup();
                /*NOTREACHED*/
            }

            for (i = 0; i < strlen(buffer); i++) {
                if (write(whdl, &buffer[i], 1) <= 0)
                    printf("Write Error\n");
            }
            c = 0xd;
            write(whdl, &c, 1);
        }
        /*NOTREACHED*/
    } else {
        /***************************/
        /****  PARENT  PROCESS  ****/
        /***************************/

        if (tcgetattr(rhdl, &ntty) != 0) {
            perror("flint:IOCTL");
            exit(1);
        }

        signal(SIGHUP, cleanup);
        signal(SIGINT, ctrlc_handler);
        signal(SIGQUIT, cleanup);
        signal(SIGTERM, cleanup);
        signal(SIGSTOP, cleanup);
        signal(SIGTSTP, cleanup);
        signal(SIGCHLD, cleanup); /* <-- normal exit signal */

        otty = ntty; /* 'otty' used during cleanup() */

        ntty.c_lflag &= ~(ECHO | ICANON); /* was flag "CBREAK" */
        ntty.c_lflag |= ISIG;
        ntty.c_iflag = 0;
        ntty.c_cflag &= ~(CBAUD | (CSIZE | PARENB)); /* Punch hole. */
        ntty.c_cflag |= (CS8 | FLASH_BAUD);
        ntty.c_cc[VMIN]  = 1; /* Wait forever for at least one char. */
        ntty.c_cc[VTIME] = 0;
        ntty.c_cc[VKILL] = _POSIX_VDISABLE; /* no kill line */

        /*
         *  Read from the target and send to the user.
         */
        while (1) {
            do n = read(rhdl, buffer, sizeof(buffer));
            while (n == 0);

            if ((n < 0) && (errno != EWOULDBLOCK)) {
                perror("flint: Read Error");
                cleanup();
            }
            if (n < 0)
                continue;
            for (i = n; --i >= 0;) {
                buffer[i] &= 0x7F;
            }
            write(2, buffer, n); /* whosh! */
        }                        /* while hell not frozen over.. */
    }

    /*NOTREACHED*/
} /* main() */

void ctrlc_handler() {
    ctrlc_hit = 1;

    if (child_id == 0)
        child_cleanup();
    else
        cleanup();
    /*NOTREACHED*/
}

void cleanup() {
    tcsetattr(rhdl, TCSANOW, &otty);
    close(rhdl);
    kill(child_id, SIGINT); /* SIGQUIT will cause core dump! */
    wait(&status);
    exit(1);
}

void child_cleanup() {
    tcsetattr(whdl, TCSANOW, &otty);
    close(whdl);
    kill(getppid(), SIGINT); /* SIGQUIT will cause core dump! */
    exit(0);
}
