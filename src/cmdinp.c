/*  flash/source/cmdinp.c   Parse and do user commands     sudhir, May 1993 */
/* $Header: /export/home/aleks/Projects/Intel-159/Core/00-Tools/flash/source/RCS/cmdinp.c,v 1.11
 * 2001/08/31 05:50:23 aleks Exp $ */

/*
 *	The command read-process-output loop for FLASH.
 *
 *	Commands can be entered in upper- or lower-case, doens't matter.
 */

#include "flash.h"
#include <setjmp.h>
#include <unistd.h>    /* For getcwd() */
#include <sys/param.h> /* For MAXPATHLEN */
#include <sys/stat.h>  /* For fstat() */

/*  These to restore terminal mode on ^C hit */
#ifdef HOST_POSIX
#include <termios.h>
#else
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termio.h>
#endif

/*  If non-zero, then repeat anmy signal we're sent down to child.
 *	According to POSIX as long as us and child process are in the
 *	same process group (which is inherited during fork()), we'll
 *	all get the same signal.
 */
#define SW_RESIGNAL_CHILD 0

/*  I have much trouble getting signal handling to work correctly for GDB
 *  and flash-sup.  GDB catches ^C gracefully.  Normally, leave this 0
 *  for end-user usage.  I got it working by having proc_debug() always
 *  kill the child GDB process.  (bwitt, Jan1999).
 *
 *	If user hits ^C, then we're signalled.  If awaiting a child to
 *	die (like GDB to finish up), our wait() returns with -1 and
 *	errno == EINTR.  We must re-call wait().  See long_wait() below.
 *		(bwitt, Feb 2000)
 */

#define SW_DEBUG_SIGNALLING 0

/*  Forward declarations. */
int proc_download(CONFIG *);
int proc_debug(CONFIG *);
int proc_flint(CONFIG *);
int proc_save(CONFIG *);
int proc_load(CONFIG *);
int proc_help(CONFIG *);
int proc_quit(CONFIG *);
int proc_make(CONFIG *);
int proc_pwd(CONFIG *);
int proc_cd(CONFIG *);
int proc_uptracebuffer(CONFIG *);

int  expand_cmd(char *, int *);
void search_exec(char *, CONFIG *);
int  spwords();

/* These chars have been exported to getch.c where it is filled appropriately
 */
char erase_char;
char kill_char;

#define CH_EOF ('D' - 64)

/*  These are used for <ESCAPE> expansion: */
static char *commands[] = {"download", "debug", "gdb",   "save", "load", "make",
                           "pwd",      "cd",    "flint", "help", "quit"};
#define MAX_CMDS (sizeof(commands) / sizeof(commands[0]))

FNC_TABLE cmd_table[] = {
    /* If user types just "d", the first match is taken: download. */
    {"do", proc_download},
    {"dow", proc_download},
    {"down", proc_download},
    {"downl", proc_download},
    {"downlo", proc_download},
    {"downloa", proc_download},
    {"download", proc_download},
    {"de", proc_debug},
    {"deb", proc_debug},
    {"debu", proc_debug},
    {"debug", proc_debug},
    {"gdb", proc_debug}, /* Allow "gdb" cuz GDB is run! */
    {"fl", proc_flint},
    {"fli", proc_flint},
    {"flin", proc_flint},
    {"flint", proc_flint},
#if FLASH_DISKIO
    {"sa", proc_save},
    {"sav", proc_save},
    {"save", proc_save},
    {"lo", proc_load},
    {"loa", proc_load},
    {"load", proc_load},
#endif
    {"m", proc_make},
    {"ma", proc_make},
    {"mak", proc_make},
    {"make", proc_make},
    {"pw", proc_pwd},
    {"pwd", proc_pwd},
    {"cd", proc_cd},
    {"tr", proc_uptracebuffer},
    {"tra", proc_uptracebuffer},
    {"trac", proc_uptracebuffer},
    {"trace", proc_uptracebuffer},

    {"?", proc_help},
    {"he", proc_help},
    {"hel", proc_help},
    {"help", proc_help},
    {"qu", proc_quit},
    {"qui", proc_quit},
    {"quit", proc_quit}};
#define NUMBER_ALL_CMDS (sizeof(cmd_table) / sizeof(cmd_table[0]))

/*  Tokens from user's input.  cmdwords[0] is the command. */
char cmd_words[10][512];
int  arg_count;

static char cmdd[1024]; /* Line user typed in. */

static pid_t parentproc; /*  So subprocess can signal trouble. */
static pid_t childproc;  /*  So we can signal it when we get signaled. */
static int   status;     /* Status from wait() after fork() */

jmp_buf jbCmdLoop;

/*******************************************************************/

/*  XXX - may have to re-wait() for child proc if SIGSUSP arrives.. */

void mysighandler(int signo) {
#if SW_DEBUG_SIGNALLING
    char buff[80];
#endif

    /*  If we get SIGTERM, SIGINTR, SIGSUSP, SIGKILL, pass it on... */
    /*  GDB recieves the char with "stty -isig" not catching the signal.
     *  So our attemp to pass-it-on, only aborts GDB... (sigh).
     */
    if (childproc != (pid_t)0) {
#if SW_RESIGNAL_CHILD
#if SW_DEBUG_SIGNALLING
        write(1, "--pass to child-- ", 19);
#endif
        kill(childproc, signo);
#else
#if SW_DEBUG_SIGNALLING
        write(1, "--NOT passing to child-- ", 25);
#endif
#endif
    } else if (signo == SIGINT) {
        /*  Input loop received ^C... longjump back into command loop. */
        fflush(stdout);
        write(1, "<BREAK>\n", 8);
        longjmp(jbCmdLoop, signo);
        /*NOTREACHED*/
    }

    /*  Now it's our turn.  Resignalling should cause default behavior. */
#if SW_DEBUG_SIGNALLING
    sprintf(buff, "--sig%d in proc %lu --\n", signo, (unsigned long)getpid());
    write(1, buff, strlen(buff));
#endif

#if SW_DEBUG_SIGNALLING
    write(1, "_done_", 6);
#endif

} /* end mysighandler() */

void cmdinp(cfg) CONFIG *cfg;
{
    unsigned int s;
    int          i;
    int          ignoreESC;

    int            std_in = fileno(stdin);
    struct termios pre_tty;

    /* Save mode so we can recovery on ^C */
    tcgetattr(std_in, &pre_tty);

    parentproc = getpid();
    if ((i = setjmp(jbCmdLoop)) != 0) {
        /* Back from interrrupt when getting input.... */
        /* However, -1 means exit command input loop. */
        if (i == -1) {
            tcsetattr(std_in, TCSANOW, &pre_tty);
            return;
        }
    }

    while (1) {
        childproc = (pid_t)0;
        signal(SIGINT, mysighandler);
        signal(SIGHUP, mysighandler);
        signal(SIGQUIT, mysighandler);
        signal(SIGTERM, mysighandler);
        signal(SIGTSTP, mysighandler);

        for (i = 0; i < 10; i++) cmd_words[i][0] = (char)0;
        ignoreESC = 0;
        PROMPT;
        /*		cmdd[0] = (char)0; */
        i = 0;
        while (1) {
            fflush(stdout);
            s = getch();
            /*			printf("/%d/",s); */
            if (s == '\n') {
                putchar(s);
                cmdd[i] = (char)0;
                break;
            } else if (s == CH_EOF || s == -1) {
                return;
            } else if ((s == 0x1b) && (!ignoreESC)) {
                cmdd[i]   = (char)0;
                ignoreESC = expand_cmd(cmdd, &i);
            } else if (s == erase_char) {
                if (i == 0)
                    continue;
                printf("\b \b");
                i--;
            } else {
                if (!isprint(s))
                    continue;
                cmdd[i] = (char)s;
                i++;
                putchar(s);
                if (s == ' ')
                    ignoreESC = 1;
            }
        }
        if (i > 0) {
            if ((arg_count = spwords(cmdd)))
                search_exec(cmdd, cfg);
        }
    }
}

/* Expand the command */
int   expand_cmd(cmdd, i)
char *cmdd;
int  *i;
{
    int   j;
    int   valid_index = 0;
    int   count;
    char *ptr    = NULL;
    int   retval = 0;

    count  = 0;
    retval = 0;

    /*  Skip leading whitespace, then lower-case the command. */
    while ((*cmdd == ' ') || (*cmdd == '\t')) cmdd++;
    for (j = strlen(cmdd); --j >= 0;) {
        cmdd[j] = tolower(cmdd[j]);
    }

    for (j = 0; j < MAX_CMDS; j++) {
        if (strncmp(cmdd, commands[j], strlen(cmdd)) == 0) {
            if (count)
                return 0;
            count++;
            valid_index = j;
            ptr         = commands[j];
            retval      = 1;
        }
    }
    if (retval) {
        ptr += strlen(cmdd);
        printf("%s", ptr);
        sprintf(cmdd, "%s", commands[valid_index]);
        *i = strlen(commands[valid_index]);
        return retval;
    }

    return 0;
}

void    search_exec(cmdd, cfg) char *cmdd;
CONFIG *cfg;
{
    FNC_TABLE *cmdtbl;
    int        i;
    int        done;
    char      *p;

    cmdtbl = &cmd_table[0];
    done   = 0;

    /*  First downcase the user's command before comparing to table. */
    for (p = cmd_words[0]; *p != '\0'; ++p) {
        *p = tolower(*p);
    }

    for (i = 0; i < NUMBER_ALL_CMDS; i++, cmdtbl++) {
        if ((strncmp(cmdtbl->command, cmd_words[0], strlen(cmd_words[0]))) == 0) {
            (cmdtbl->func)(cfg);
            done = 1;
            break;
        }
    }
    if (!done) {
        printf("\nInvalid Command : [ %s ], Type help for list of commands\n", cmdd);
    }
} /* end search_exec() */

/* --------------------------------------------------------------------- */

char args[10][512];

void print_args(int cnt, char args[10][512]) {
    if (verbose_flag || cnt < 0) {
        if (cnt < 0)
            cnt = -cnt;

        ++cnt; /* DEBUG */
        fprintf(stderr, "%%");
        while (--cnt >= 0) {
            fprintf(stderr, " %s", *args++);
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}

/**
 *	If user hits ^C, then wait() returns EINTR.  However, the
 *	child seems to also receive this signal (Solaris 2.7)
 *	 Must continue waiting until child cleans up itself.
 *	After signal() is tripped, program must set it again.
 */
int long_wait() {
    while (wait(&status) == -1 && errno == EINTR) {
        signal(SIGINT, mysighandler);
        signal(SIGHUP, mysighandler);
        signal(SIGQUIT, mysighandler);
        signal(SIGTERM, mysighandler);
        signal(SIGTSTP, mysighandler);
#if SW_DEBUG_SIGNALLING
        fprintf(stderr, " // still waiting // ");
#endif
        errno = 0;
    }

    signal(SIGINT, mysighandler);
    signal(SIGHUP, mysighandler);
    signal(SIGQUIT, mysighandler);
    signal(SIGTERM, mysighandler);
    signal(SIGTSTP, mysighandler);

    return status;
} /* end long_wait() */

/*************************************************************************
 *			proc_download
 *************************************************************************
 *  PURPOSE:	Download imazge to target.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:	Updates cfg->download_file when fname is explicitly set.
 *  OUTLINE:	error if no download file set.
 *		Error if file does not exist.
 *		Ask flash-support to load-from-ether first.
 *		If OK, then retun OK.
 *		exec(fg->dlpath) to download current image.
 *
 *************************************************************************
 */

int     proc_download(cfg)
CONFIG *cfg;
{
    char filename[512];

    /* The first word should contain the filename (when explicit) */
    strcpy(filename, cmd_words[1]);
    if (0 == strlen(filename)) {
        if (strlen(cfg->download_file)) {
            strcpy(filename, cfg->download_file);
        } else {
            printf("\nUsage : download <filename>.dli or .abm\n");
            return 0;
        }
    } else {
        /*  Each time file specified explicitly, it provides a
         *  default value from this time forward.
         */
        strcpy(cfg->download_file, filename);
    }

    if (access(filename, R_OK) < 0) {
        printf("ERROR: file <%s> not found.\n", filename);
        return 0;
    }

#if defined(TARGET_3b1)
#if 0
	/*  CAN'T DO DOWNLOAD YET, SINCE IMAGE ISN'T COPIED TO FTP AREA.
	 *  THE downloader WILL TRY ETHERNET FIRST, THEN FALLBACK TO
	 *  SERIAL DOWNLOAD.  USE "-s" TO FORCE SERIAL ALWAYS.
	 */
	/*  Try ethernet download first. */
	if( (childproc=fork()) == 0 ) {
		int  j = 1;
		sprintf(args[0],"flash-sup");
		if( verbose_flag ) {
		    strcpy(args[j++], "-v" );
		}
		sprintf(args[j++],cfg->port);
		sprintf(args[j++],"n"); 	/* send "le 3000" to target. */
		print_args(j, args);
		execlp(cfg->pregdbpath,args[0],args[1],args[2],
		       ((j==4) ? args[3] : (char *)0), (char *)0);
		perror("flash-sup Exec Failed");
		exit(1);
	}
	long_wait();
	childproc = 0;
	if( status == 0 ) {
		return 1;
	}
#endif
#endif

    /*  Try serial port download. */
    if ((childproc = fork()) == 0) {
        sprintf(args[0], "dl");
        sprintf(args[1], filename);
        sprintf(args[2], cfg->port);
        print_args(3, args);
        execlp(cfg->dlpath, args[0], args[1], args[2], (char *)0);
        perror("Exec Failed");
        exit(1);
    }
    long_wait();
    childproc = 0;

    return (status == 0);
}

/*************************************************************************
 *			proc_debug
 *************************************************************************
 *  PURPOSE:	Invoke GDB on image.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:	Assumes image already on target.
 *		If we're inside Emacs, invokes "GDB -fullname".
 *  OUTLINE:	error if no download file set.
 *		exec(fg->dlpath) to download current image.
 *
 *************************************************************************
 */

int     proc_debug(cfg)
CONFIG *cfg;
{
    char gdb_init_fname[128];
    char filename[1024];
    int  j;
    printf("I m in gdb");
    /* The first word should contain the filename */
    strcpy(filename, cmd_words[1]);
    if (!strlen(filename)) {
        if (strlen(cfg->download_file)) {
            strcpy(filename, cfg->download_file);
            printf("%s debug file", filename);
        }
        if (strlen(filename) == 0) {
            printf("\nUsage : debug <filename>.dli or .abm\n");
            return 0;
        }
    } else {
        /*  If file named explicitly, and no default, then set it. */
        if (0 == strlen(cfg->download_file)) {
            strcpy(cfg->download_file, filename);
        }
    }

    /* Check if GDBINIT file is present in the current directory */

    strcpy(gdb_init_fname, USER_GDBINIT_FILE);
    if (access(gdb_init_fname, F_OK) < 0) {
        char temp_line[1024];

        printf("\nNote: File \"" USER_GDBINIT_FILE
               "\" isn't present, now copying to current directory.\n");

        /*  Don't "cp -p" cuz it makes "spede" user the owner. */
        sprintf(temp_line, "cp " MODEL_GDBINIT_FILE " %s", gdb_init_fname);
        if (verbose_flag) {
            printf("%% %s\n", temp_line);
        }
        (void)system(temp_line);

        if (access(gdb_init_fname, F_OK) < 0) {
            printf("ERROR: Copy failed, command aborted.\n");
            return 0;
        }
    }

    if ((childproc = fork()) == 0) {
        sprintf(args[0], "flash-sup");
        j = 1;
        if (verbose_flag) {
            strcpy(args[j++], "-v");
        }

        sprintf(args[j++], cfg->port);
        sprintf(args[j++], "e"); /* send "g 3006" to target. */
        print_args(j, args);
        printf("I m in gdb");
        printf("%s %s %s %s", cfg->pregdbpath, args[0], args[1], args[2]);
        execlp(cfg->pregdbpath, args[0], args[1], args[2], ((j == 4) ? args[3] : (char *)0),
               (char *)0);
        perror("flash-sup Exec Failed");
        exit(1);
    }
    long_wait();
#if SW_DEBUG_SIGNALLING
    printf("run status=%04x\n", status);
    status = 0;
#endif
    childproc = 0;
    if (status == 0) {
        if ((childproc = fork()) == 0) {
            sprintf(args[0], strrchr(cfg->gdbpath, '/') + 1);
            sprintf(args[1], "-command=%s", gdb_init_fname);

            sprintf(args[2], "-readnow"); /* Preload symbols */
            sprintf(args[3], "-nx");      /* Ignore all .gdbinit files */
            j = 4;
            if (emacs_debugger_flag) {
                sprintf(args[j++], "-fullname"); /* Emacs subprocess */
            }
            strcpy(args[j], filename);
            print_args(j + 1, args);
            printf("j is %d", j);
            printf("path n %s %s %s %s %s %s %s", cfg->gdbpath, args[0], args[1], args[2], args[3],
                   args[4]);

            execlp(cfg->gdbpath, args[0], args[1], args[2], args[3], args[4],
                   ((j == 5) ? args[5] : (char *)0), (char *)0);
            perror("GDB Exec Failed");
            exit(1);
        }
        long_wait();
#if SW_DEBUG_SIGNALLING
        printf("**** proc_debug() done waiting *****\n");
#endif
        assert(childproc != (pid_t)0);
        kill(childproc, SIGKILL);
    }

    childproc = 0;

    return 1;
}

/*************************************************************************
 *			proc_flint
 *************************************************************************
 *  PURPOSE:	Enter flint monitor command mode (a terminal program).
 *  RETURN:	(int) 0 = error, 1 = OK;
 *  NOTES:
 *  OUTLINE:	exec(cfg->flpath).
 *
 *************************************************************************
 */

int     proc_flint(cfg)
CONFIG *cfg;
{
    /* FLames shell here */
    if (arg_count != 1)
        printf("Usage : flint \n");
    else {
        if ((childproc = fork()) == 0) {
            sprintf(args[0], "fl");
            sprintf(args[1], cfg->port);
            print_args(1, args);
            execlp(cfg->flpath, args[0], args[1], (char *)0);
        }
        long_wait();
        childproc = 0;
    }
    return 1;
}

/*************************************************************************
 *			proc_save
 *************************************************************************
 *  PURPOSE:	Save image on target to harddisk or floppy.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	By default, save to hard disk. $FLASH_DISK_ARGS can override
 *		range/sector for save/loads.
 *		IF second parameter, "f" means use floppy.
 *
 *************************************************************************
 */

int     proc_save(cfg)
CONFIG *cfg;
{
    int j;

    if (arg_count == 1) {
        /* No argument so save to hard drive */
        if ((childproc = fork()) == 0) {
            sprintf(args[0], "flash-sup");
            j = 1;
            if (verbose_flag) {
                strcpy(args[j++], "-v");
            }
            sprintf(args[j++], cfg->port);
            sprintf(args[j++], "s");
            print_args(j, args);
            execlp(cfg->pregdbpath, args[0], args[1], args[2], ((j == 4) ? args[3] : (char *)0),
                   (char *)0);

            perror("Exec Failed");
            exit(1);
        }
        long_wait();
        childproc = 0;
    } else if (arg_count == 2) {
        /* There is another argument check it, see if it is 'f' */
        if (cmd_words[1][0] == 'f') {
            if ((childproc = fork()) == 0) {
                sprintf(args[0], "flash-sup");
                sprintf(args[1], cfg->port);
                sprintf(args[2], "f");
                print_args(2, args);
                execlp(cfg->pregdbpath, args[0], args[1], args[2], (char *)0);
                perror("Exec Failed");
                exit(1);
            }
            long_wait();
            childproc = 0;
        }
    }
    return 1;
}

/*************************************************************************
 *			proc_load
 *************************************************************************
 *  PURPOSE:	Load from hard disk or floppy.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	By default, save to hard disk. $FLASH_DISK_ARGS can override
 *		range/sector for save/loads.
 *		IF second parameter, "f" means use floppy.
 *
 *************************************************************************
 */

int     proc_load(cfg)
CONFIG *cfg;
{
    int   j;
    char *cmd = NULL;

    if (arg_count == 1) {
        /* No argument so load from hard drive */
        cmd = "l";
    } else if (arg_count == 2 && cmd_words[1][0] == 'f') {
        /* There is another argument check it, see if it is 'f' */
        cmd = "o";
    } else {
        printf("\nInvalid argument to the load command\n");
        return 0;
    }

    if ((childproc = fork()) == 0) {
        strcpy(args[0], "flash-sup");
        j = 1;
        if (verbose_flag) {
            strcpy(args[j++], "-v");
        }
        strcpy(args[j++], cfg->port);
        strcpy(args[j++], cmd);
        print_args(j, args);
        execlp(cfg->pregdbpath, args[0], args[1], args[2], ((j == 4) ? args[3] : (char *)0),
               (char *)0);

        perror("Exec Failed");
        exit(1);
    }
    long_wait();
    childproc = 0;

    return 1;
} /* proc_load() */

/*************************************************************************
 *			proc_make
 *************************************************************************
 *  PURPOSE:	Shell out to run "make"
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	Allows user to not leave FLASH just to run make.
 *		If make return isn't 0, then print it.
 *
 *************************************************************************
 */

int     proc_make(cfg)
CONFIG *cfg;
{
    char  buffer[512 + 8];
    char *p;
    int   rc;

    putchar('\n');
    strcpy(buffer, "make");
    /*  Skip first word user type (could be shorthand for "make").  If
     *  user types "   make", we must skip leading spaces first!
     */
    for (p = cmdd; isspace((int)*p) && *p != '\0'; ++p)
        ;
    for (; !isspace((int)*p) && *p != '\0'; ++p)
        ;
    strcat(buffer, p);
    if (verbose_flag) {
        printf("%% system(%s)\n", buffer);
    }
    rc = system(buffer);
    if (rc != 0) {
        printf("make return code = %d\n", rc);
    }

    return 1;
} /* proc_make() */

/*************************************************************************
 *			proc_cd
 *************************************************************************
 *  PURPOSE:	Change the current working directory.  Shell variables and
 *		"~" can used in directory string.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	Ask OS to change current working directory.
 *		Since we're running an external command, abort if trying
 *		to alter PATH or IFS, or invoking more than one command.
 *		If popen() fails, try to change-dir using dirname
 *		directly.
 *
 *************************************************************************
 */
int     proc_cd(cfg)
CONFIG *cfg;
{
    char  buffer[1024];
    FILE *pread;
    int   result;

    if (cmd_words[1][0] == (char)0 || cmd_words[2][0] != (char)0) {
        printf("Usage: cd <directory>\n");
        return 0;
    }
    /*  Ensure string has no security holes..
     *  	;  - maybe two commands.
     *   	IFS - change inter-field separator.
     *   	PATH = trying to change PATH.
     */
    if (strchr(cmd_words[1], ';') || strstr(cmd_words[1], "IFS") || strstr(cmd_words[1], "PATH")) {
        printf("ERROR: bad directory name string\n");
        return 0;
    }

    sprintf(buffer, "$SHELL -c '/usr/bin/echo \"%s\"'", cmd_words[1]);
    pread = popen(buffer, "r");
    if (pread != NULL) {
        /*  Execute echo with user's shell.  This will expand variables
         *  and leading "~" for CSH.  If "~/.cshrc" has no output cmds,
         *  then the single of output will be translated value.
         *  Read this single line and close the pipe.  However, we
         *  install protection in case there is no output.  In
         *  that case, CD into current working directory, "." .
         */
        strcpy(buffer, ".\n");
        buffer[sizeof(buffer)] = '\0';
        fgets(buffer, sizeof(buffer) - 1, pread);
        pclose(pread);

        if (strlen(buffer) <= 1 || strstr(buffer, "Undefined variable")) {
            /*  CSH said failure... */
            printf("ERROR: troubles during expansion.\n");
            return 0;
        }

        /*  Mash NEWLINE at end of string, then try CD. */
        if (strlen(buffer) > 0) {
            buffer[strlen(buffer) - 1] = '\0';
        }

        result = chdir(buffer);
    } else {
        result = chdir(cmd_words[1]);
    }

    if (result < 0) {
        perror("ERROR: Can't change current working directory");
        return 0;
    }

    return 1;
} /* end proc_cd() */

/*************************************************************************
 *			proc_pwd
 *************************************************************************
 *  PURPOSE:	Print current working directory
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	Ask OS for current working directory,then print it.
 *
 *************************************************************************
 */
int     proc_pwd(cfg)
CONFIG *cfg;
{
    char buffer[512 + 8];

    if (NULL == getcwd(buffer, sizeof(buffer) - 1)) {
        perror("ERROR: Can't get current working directory");
        return 0;
    }

    printf("%s\n", buffer);

    return 1;
} /* end proc_pwd() */

/*************************************************************************
 *			proc_uptracebuffer
 *************************************************************************
 *  PURPOSE:	Upload the trace buffer to host.
 *  RETURN:	(int) 0 = error, 1 = OK.
 *  NOTES:
 *  OUTLINE:	Assumes user will call a routine on target to output
 *		the trace buffer's contents.
 *		Expect "S0xxxx" timeout 20 secs as first line.
 *			"xxxx" is hex of target bytes per line.
 *		Expect "S2aaaaaa:xxyyzz..IIII" for each line, "aaaaaa" is
 *			offset in trace buffer, each byte is hex printed.
 *			IIII is Internet checksum over whole buffer.
 *		Expect "S900" as closing.
 *
 *************************************************************************
 */
static jmp_buf timeout_jb;

static void uptrace_timeout(int s) {
    printf("ERROR: TIMEOUT!\n");
    longjmp(timeout_jb, 1);
} /* end uptrace_timeout() */

int target_readline(int targ, char *buffer, int size_buffer) {
    static char holder[16];
    static int  next = 0;
    static int  last = 0;

    char *inc = holder;
    int   cnt = 0;
    char  byt;

    while (cnt < size_buffer) {
        /*  If our local stream buffer is empty, re-fill from system.
         *  Try to reduce overhead by having a tiny intermediate buffer.
         */
        if (next == last) {
            last = read(targ, holder, sizeof(holder));
            inc  = holder;
        }
        ++cnt;
        ++next;
        *buffer++ = byt = *inc++;
        if (iscntrl((int)byt)) {
            return (cnt);
        }
    }

    return (cnt);
} /* end target_readline() */

int     proc_uptracebuffer(cfg)
CONFIG *cfg;
{
    struct stat term_stat;
#ifdef HOST_POSIX
    struct termios otty, ntty;
#else
    struct sgttyb otty, ntty;
    struct termio tbuf, o_tbuf;
#endif
    const char spinner[] = {'-', '\\', '|', '/'};
    static int si        = 0;

    char  fname[MAXPATHLEN];
    char  buffer[4096 + 50];
    char *p;
    FILE *ifp;
    int   fd_target;
    int   ofd;

    /*  First open the connection to the target.  */
    if ((ifp = fopen(TTYPORT, "rw")) == NULL) {
        printf("ERROR: Cannot open target port <" TTYPORT ">\n");
        return 0;
    }

    /* get info on current terminal state */
    fd_target = fileno(ifp);
#ifdef HOST_POSIX
    tcgetattr(fd_target, &otty);
#else
    ioctl(fd_target, TIOCGETP, &otty);
#endif
    fstat(fd_target, &term_stat);

    /* copy original terminal state info to a new structure */
    ntty = otty;

    /* set the new flags that we want.. */
#ifdef HOST_POSIX
    ntty.c_iflag = 0;
    ntty.c_oflag &= ~OPOST;
    ntty.c_lflag &= ~(ISIG | ICANON | ECHO | XCASE);
    ntty.c_cflag &= ((~CBAUD));
    ntty.c_cflag |= FLASH_BAUD;
    ntty.c_cflag &= ~(CSIZE | PARENB);
    ntty.c_cflag |= (CS8 | FLASH_BAUD);
    ntty.c_cc[VMIN]  = 1;
    ntty.c_cc[VTIME] = 1;
    tcsetattr(fd_target, TCSANOW, &ntty);
#else
    ntty.sg_flags = (ntty.sg_flags & ~ECHO) | RAW;
    /* tell the device driver to use them */
    ioctl(fd_target, TIOCSETP, &ntty);

    ioctl(fd_target, TCGETA, (caddr_t)&tbuf);
    o_tbuf = tbuf;
    tbuf.c_iflag = 0;
    tbuf.c_oflag &= ~OPOST;
    tbuf.c_lflag &= ~(ISIG | ICANON | ECHO | XCASE);
    tbuf.c_cflag &= ((~CBAUD));
    tbuf.c_cflag |= FLASH_BAUD;
    tbuf.c_cflag &= ~(CSIZE | PARENB);
    tbuf.c_cflag |= (CS8 | FLASH_BAUD);
    tbuf.c_cc[VMIN] = 1;
    tbuf.c_cc[VTIME] = 1;
    ioctl(fd_target, TCSETAF, (caddr_t)&tbuf);
#endif

    /*  Then open a file to store the goods. */
    strcpy(fname, "TraceXXXXXX");
    ofd = mkstemp(fname);
    if (ofd < 0) {
        printf("ERROR: Cannot create file <%s> for trace upload.\n", fname);
        return 0;
    }

    if (setjmp(timeout_jb)) {
        printf("ERROR: Timeout in upload, aborting.\n");
    FULL_ERROR_CLEANUP:
        fclose(ifp);
        close(ofd);
        unlink(fname);
        return 0;
    }

    /*  Get that all important first line. */
    signal(SIGALRM, uptrace_timeout);
    buffer[sizeof(buffer) - 1] = 0;
    alarm(25);
    do {
        fgets(buffer, sizeof(buffer) - 1, ifp);
        if (verbose_flag) {
            printf("(trying to sync) from target: %s\n", buffer);
        }
        for (p = buffer; *p != '\0' && *p != 'S'; ++p) {
        }
    } while (0 != strncmp(p, "SL", 2) && 0 != strncmp(p, "S9", 2));
    if (p[1] != 'L') {
        printf("ERROR: Trace buffer didn't synchronize.\n");
        goto FULL_ERROR_CLEANUP;
    }
    printf("[ ");
    fflush(stdout);

    /*  Read a block of data then immediately write it out.  Might be faster
     *  to mmap() the output file.  There is no packet-received-OK-ACK back
     *	to the target, so be quick here...
     *
     *  Because the file produced in "cooked" mode, each line ends with
     *  A CR-LF pair.  We store whatever we read, post-processing software
     *  can make things pretty...
     */
    for (;;) {
        alarm(12);
        fgets(buffer, sizeof(buffer) - 1, ifp);
        for (p = buffer; *p != '\0' && *p != 'S'; ++p) {
        }
        if (ferror(ifp) || 0 == strncmp(p, "S9", 2)) {
            break;
        }
        printf("\b%c", spinner[si++]);
        fflush(stdout);
        if (si >= sizeof(spinner)) {
            si = 0;
        }

        write(ofd, buffer, strlen(buffer));
    }

    alarm(0);
    printf("]\n");

    /*  See how we ended.  If no troubles, then write trailing S9 packet.
     *  Otherwise print a diagnostic and return to command loop.
     */
    if (ferror(ifp)) {
        printf("ERROR: Troubles while reading from target\n");
        goto FULL_ERROR_CLEANUP;
    }
    write(ofd, buffer, strlen(buffer));

    printf("TRACE: trace buffer successfully uploaded into <%s>.\n", fname);
    fclose(ifp);
    close(ofd);
    return 1;
} /* end proc_uptracebuffer() */

/* ------------------------------------------------------------------------ */

int     proc_help(cfg)
CONFIG *cfg;
{
    char border[] = {"***********************************************************************\n"};

    printf("%s\n --------- %s ------------\n\n", border, IDENT_STRING);
    printf("help   - This is how you got here\n");
    printf("quit   - Quit \n");
    printf("flint  - You can run flames commands from here\n");
    printf("pwd    - Print current directory\n");
    printf("cd     - Change current directory (shell vars OK)\n");
    printf(
        "make   - Usage : make [option..]\n"
        "        Invoke `make' with any options.\n");
    printf("trace  - Capture trace buffer from target.\n");
    printf(
        "download - Usage : download < filename >.{abm,dli}\n"
        "	   Download the specified file. Sets default for debug\n");
    printf("gdb    -\n");
    printf(
        "debug  - Usage : debug <filename>.dli\n"
        "         Debug a C program using the GNU debugger\n");
#if FLASH_DISKIO
    printf(
        "save   - Usage : save [f] \n"
        "	 save the download image to hard drive (default)\n"
        "	 f option saves the download image to the\n"
        "	 floppy drive\n");
    printf(
        "load   - Usage : load [f] \n"
        "	 load the download image from the hard drive(default)\n"
        "	 \"f\" option loads the download image from the\n"
        "	 floppy drive\n");
#endif
    printf("\n%s", border);

    return 1;
}

/*************************************************************************
 *			proc_quit
 *************************************************************************
 *  PURPOSE:	Quit this program.
 *  RETURN:	(int) 0. BUt never really returns.
 *  NOTES:
 *  OUTLINE:	Use jumpbuf to exit main loop.
 *
 *************************************************************************
 */

int     proc_quit(cfg)
CONFIG *cfg;
{
    printf("\n");
    longjmp(jbCmdLoop, -1);
    /*NOTREACHED*/
    return 0;
}

int   spwords(str)
char *str;
{
    char *ptr, *src;
    int   count;

    count = 0;
    src   = str;
    /* Skip all the blanks */
    while (1) {
        ptr = &cmd_words[count][0];
        while ((*src) && (isspace((int)*src))) src++;
        if (!(*src))
            return count;
        else {
            while ((*src) && ((*src != ' ') && (*src != '\t')))
                if (isprint((int)*src))
                    *ptr++ = *src++;
            *ptr = (char)0;
            count++;
            if (!(*src)) {
                return count;
            }
        }
    }
}
