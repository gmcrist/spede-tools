/*   flash-sup.c  Send simple commands to target; check response.  suhidr May 1993 */
/* $Header:
 * /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/flash-sup.c,v 1.3
 * 2001/02/04 01:03:31 aleks Exp $ */

/* Syntax : flash-sup [-v] [dev_name] [type]
 *	-v  = verbose
 *	type = 	e for execute in case of debugger
 *		s save to hard drive
 *		l load to hard drive
 *		f save to floppy drive
 *		o load from floppy drive
 *		n boot from ethernet/ftp
 *
 *  Environment:
 *	FLASH_DISK_ARGS    "3000:15000 700"
 */

#include "flash.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HOST_POSIX
#include <termios.h>
#else
#include <sys/ioctl.h>
#include <termio.h>
extern char *getenv();
#endif

#if defined(TARGET_i386)
/* String to send before exec'ing GDB */
#define EXEC_GDB_STRING "gdb"

#elif defined(TARGET_3b1)
/* String to send before exec'ing GDB */
#define EXEC_GDB_STRING "g 3006"

/* Default disk load/save args, and env var to override it with: */
#define DEF_DISK_ARGS "3000:15000 700"
#define ENV_DISK_ARGS "FLASH_DISK_ARGS"
#endif

/* ----------------------------------------------------------------- */

#ifdef HOST_POSIX
struct termios otty, ntty;
#else
struct sgttyb otty, ntty;
struct termio tbuf, o_tbuf;
#endif

int  verbose_flag = FALSE;
int  target_fd;
char std_buf[80];
char out_msg[80];

#if FLASH_DISKIO
/*  Whatever user wants for hard disk args: */
char *hdisk_args = DEF_DISK_ARGS;
#endif

int timeout_count = 0;

char *program_name;

/* --------------------------------------------------------------------- */

void sig_alarm(signo) int signo;
{
    (void)signo;
    write(2, "..timeout..\n", 12);
    ++timeout_count;
}

int   main(argc, argv) int argc;
char *argv[];
{
    int   j;
    char  eoln = 0x0d;
    char *dev;
    char *type;
    char *good_resp = NULL; /* Used for strstr() call */
    char *bad_resp1 = NULL;
    char *bad_resp2 = NULL;
    char  response[256];

    program_name = argv[0];
    if (argc > 2 && strcmp(argv[1], "-v") == 0) {
        verbose_flag = TRUE;
        argv[1]      = argv[0];
        argv++;
        argc--;
        printf(
            "("__FILE__
            ", %s, " __DATE__ ")\n",
            "$Revision: 1.3 $");
    }

    if (argc != 3) {
        printf("Usage Error: %s [-v] device cmd\n", program_name);
        exit(1);
    }

#if FLASH_DISKIO
    /*  See if user wants to override default memory range for disk
     *  load and stores...
     */
    if ((dev = getenv(ENV_DISK_ARGS)) != NULL) {
        hdisk_args = dev;
    }
#endif

    dev  = argv[1];
    type = argv[2];
    /*	printf("Pre-GDB : Device : %s \t Type :  %s\n",argv[1],argv[2]);
     */
    target_fd = open(dev, O_RDWR);
    if (target_fd <= 0) {
        printf("\nError in opening device \n");
        exit(1);
    }

#ifdef HOST_POSIX
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
#else
    ioctl(target_fd, TIOCGETP, &ntty);
    otty = ntty;
    ntty.sg_flags &= ~ECHO;
    ntty.sg_flags |= CBREAK;
    ioctl(target_fd, TIOCSETP, &ntty);
    /***************************************************************/
    ioctl(target_fd, TCGETA, (caddr_t)&tbuf);
    o_tbuf       = tbuf;
    tbuf.c_iflag = 0;
    tbuf.c_cflag &= ((~CBAUD));
    tbuf.c_cflag |= FLASH_BAUD;
    tbuf.c_cflag &= ~(CSIZE | PARENB);
    tbuf.c_cflag |= (CS8 | FLASH_BAUD);
    ioctl(target_fd, TCSETAF, (caddr_t)&tbuf);
#endif
    /****************************************************************/

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

    switch (*type) {
        case 'e':
            sprintf(std_buf, "%s", EXEC_GDB_STRING);
            out_msg[0] = (char)0;
            break;
#if FLASH_DISKIO
        case 's':
            sprintf(std_buf, "sh %s", hdisk_args);
            sprintf(out_msg, "Saved to the hard disk  !");
            good_resp = "save success";
            bad_resp1 = "ERROR";
            break;
        case 'l':
            sprintf(std_buf, "lh %s", hdisk_args);
            sprintf(out_msg, "Load from hard disk successful !");
            good_resp = "load success";
            bad_resp1 = "ERROR";
            break;
        case 'f':
            sprintf(std_buf, "sf 3000:15000 30");
            sprintf(out_msg, "Saved to the floppy disk !");
            bad_resp1 = "ERROR";
            break;
        case 'o':
            sprintf(std_buf, "lf 3000:15000 30");
            sprintf(out_msg, "Load from floppy disk successful !");
            bad_resp1 = "ERROR";
            break;
#endif
        case 'n':
            sprintf(std_buf, "le 3000");
            sprintf(out_msg, "Load from Ethernet/FTP successful !");
            good_resp = "load success";
            bad_resp1 = "ERROR";
            bad_resp2 = "Illegal"; /* Found FLAMES, not RMON */
            break;

        default:
            printf("Oops ... Error ...\n");
            exit(1);
    }

#if 0
	/*  XXX - need to read non-blocking, then reset it back to
	 *  blocking mode. (bwitt, June 1998)
	 */
	/*  Drain any text before sending command. */
    while( read( target_fd, response, sizeof(response)-1 ) > 0 )
	;
#else
    read(target_fd, response, sizeof(response) - 1);
#endif

    if (verbose_flag) {
        printf("write(%s, \"%s\")\n", dev, std_buf);
    }
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
        int   remaining, size, en;
        char *p = response;
        char  found_nl;

        memset(response, 0, sizeof(response));
        remaining = sizeof(response) - 1;
        en        = errno;

        /* Read response until we get a newline or buffer fills. */
        for (found_nl = FALSE; remaining > 0 && !found_nl;) {
            size = read(target_fd, p, remaining);
            for (j = size; --j >= 0; ++p) {
                *p &= 0x7F;
                if (p > response + 3 && (*p == '\r' || *p == '\n'))
                    found_nl = TRUE;
            }
            p += size;
            remaining -= size;
        }
        *p = '\0';

        if (verbose_flag) {
            FILE *fp = fopen("/tmp/FF", "w");
            if (fp == NULL) {
                fp = stderr;
            }
            fprintf(fp, "errno=%d, cnt=%d, \"%s\"\n", en, (int)(p - response), response);
            fclose(fp);
        }
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
#ifndef HOST_POSIX
    else {
        /*  Let serial port drain a bit before restoring terminal settings.
         *  POSIX host can do this explicitly with TCSADRAIN.
         */
        sleep(1);
    }
#endif

    /*  Now restore the terminal, but only after what we've written has
     *  been sent out (drained).  Especially for "start debug" which
     *  doesn't expect a response.  If we don't wait then some chars
     *  could be written at 300 baud when we restore previous settings!
     */
#ifdef HOST_POSIX
    tcsetattr(target_fd, TCSADRAIN, &otty);
#else
    ioctl(target_fd, TIOCSETP, &otty);
    ioctl(target_fd, TCSETAF, (caddr_t)&o_tbuf);
#endif
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
