/**
 * st-cmd SPEDE Target Command
 * Sends simple commands to the target/flames shell
 *
 * Original implementation (flash-sup.c; suhidr May 1993)
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "spede.h"

enum cmd_list { CMD_NONE, CMD_GDB, CMD_RUN };

#define CMD_EXEC_GDB "gdb"
#define CMD_EXEC_RUN "G"

/* ----------------------------------------------------------------- */

struct termios otty, ntty;

int  target_fd;
char std_buf[80];
char out_msg[80];

int timeout_count = 0;

char *program_name;

/* --------------------------------------------------------------------- */

void sig_alarm(int signo) {
    (void)signo;
    write(2, "..timeout..\n", 12);
    ++timeout_count;
}

void usage(char *self, int rc) {
    printf("usage: %s [-g|-r] [-h] <device>\n", self);
    printf("\n");
    exit(rc);
}

int main(int argc, char **argv) {
    int   j;
    char  eoln = 0x0d;
    char *dev;
    char *good_resp = NULL; /* Used for strstr() call */
    char *bad_resp1 = NULL;
    char *bad_resp2 = NULL;
    char  response[256];

    int cmd = CMD_NONE;

    program_name = argv[0];

    int opt;
    while ((opt = getopt(argc, argv, "hgr")) != -1) {
        switch (opt) {
            case 'g':
                cmd = CMD_GDB;
                break;

            case 'r':
                cmd = CMD_RUN;
                break;

            case 'h':
            default:
                usage(argv[0], 1);
                break;
        }
    }

    if (argc - optind < 1) {
        usage(argv[0], 1);
    }

    dev = argv[optind++];

    target_fd = open(dev, O_RDWR);
    if (target_fd <= 0) {
        printf("\nError in opening device \n");
        exit(1);
    }

    tcgetattr(target_fd, &ntty);
    otty = ntty;
    ntty.c_lflag &= ~(ECHO | ICANON); /* was flag "CBREAK" */
    ntty.c_lflag |= ISIG;
    ntty.c_iflag = 0;
    ntty.c_cflag &= ((~CBAUD));
    ntty.c_cflag |= FLASH_BAUD;
    ntty.c_cflag &= ~(CSIZE | PARENB);
    ntty.c_cflag |= (CS8 | FLASH_BAUD);
    tcsetattr(target_fd, TCSANOW, &ntty);

    /* Set timeout for 6 seconds. */
    signal(SIGALRM, sig_alarm);
    alarm(6);

    /*  Send a <RETURN> to clear out any schme in FLAMES' buffer.
     *  Then wait a second for the prompt to appear.  Since FLAMES
     *  serial input isn't interrupt driven, it won't start listening
     *  until its prompt is displayed, which could lose the first few
     *  chars of the actual command we send it.
     */

    write(target_fd, &eoln, 1);
    sleep(1);

    switch (cmd) {
        case CMD_GDB:
            sprintf(std_buf, "%s", CMD_EXEC_GDB);
            out_msg[0] = (char)0;
            break;

        case CMD_RUN:
            sprintf(std_buf, "%s", CMD_EXEC_RUN);
            out_msg[0] = (char)0;
            break;

        default:
            printf("error: unknown command\n");
            usage(argv[0], 1);
    }

    read(target_fd, response, sizeof(response) - 1);

    for (j = 0; j < strlen(std_buf); j++) {
        while (write(target_fd, &std_buf[j], 1) <= 0) {
            printf("Write Error\n");
            sleep(1);
        }
    }
    write(target_fd, &eoln, 1);
    errno = 0;
    alarm(0);

    j = 0; /* Return code to invoker */
    if (good_resp != NULL) {
        /*  Get one (first) line from target and decode messages.
         *  If neither a good response substring nor bad response
         *  substring was found, just proceed like it is OK...
         */
        int   remaining, size;
        char *p = response;
        char  found_nl;

        memset(response, 0, sizeof(response));
        remaining = sizeof(response) - 1;

        /* Read response until we get a newline or buffer fills. */
        for (found_nl = false; remaining > 0 && !found_nl;) {
            size = read(target_fd, p, remaining);
            for (j = size; --j >= 0; ++p) {
                *p &= 0x7F;
                if (p > response + 3 && (*p == '\r' || *p == '\n'))
                    found_nl = true;
            }
            p += size;
            remaining -= size;
        }
        *p = '\0';

        j = 0;
        if (strstr(response, good_resp)) {
            ; /* Message printed below.. */
        } else if (strstr(response, bad_resp1) || strstr(response, bad_resp2)) {
            /*  When the response is printed out, it can contain the monitor's
             *  prompt.  Thus, this program will exit with text on the current
             *  line.  Looks bad cuz the UNIX prompt will appear to the right
             *  of the target's prompt.
             */
            j          = 2;
            out_msg[0] = '\0';
            printf("ERROR...%s", response);
        }
    }

    /*  Now restore the terminal, but only after what we've written has
     *  been sent out (drained).  Especially for "start debug" which
     *  doesn't expect a response.  If we don't wait then some chars
     *  could be written at 300 baud when we restore previous settings!
     */
    tcsetattr(target_fd, TCSADRAIN, &otty);
    close(target_fd);
    if (out_msg[0] != '\0')
        printf("\n%s\n", out_msg);

    /*  If can't reach target, then _after_ terminal settings have been
     *  restored, signal an error to our invoker.
     */
    if (timeout_count > 0) {
        exit(2);
    }

    return (j);
}
