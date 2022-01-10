/*  flash.h  Central include for FLAMES-SHELL.        suhdir May 1993 */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/flash.h,v 1.5
 * 2001/02/04 01:02:35 aleks Exp $ */

/*  "flash.h" is the central include file for FLASH, a user shell to
 *  the FLAMES ROM monitor.  This file collects common #include's and
 *  some system datatypes.
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

/*  Allow disk commands in FLASH.  If they aren't implemented, don't
 *  expand the abbreviation and don't print in help message.
 *  The early version of X86-mini-mon doesn't do disk I/O, so disable
 *  the save and load commands.
 */
#define FLASH_DISKIO 0

/*  How fast to talk with target?  Use B9600, B19200 or B38400.
 */
#ifdef TTYBAUD
#define _FLASH_CAT(x, y) x##y
#define _FLASH_XCAT(a, b) _FLASH_CAT(a, b)
#define FLASH_BAUD _FLASH_XCAT(B, TTYBAUD)
#else
#define FLASH_BAUD B38400
#endif

/*  Is the host computer a POSIX thing?  This controls OLD <termio.h> vs.
 *  <termions.h>, which is newer, POSIX like.  Also see flash.c for <utmp.h>
 *  vs. <utmpx.h> for detecting if user is logged in locally or remotely.
 *
 *  You can override this on the command line if needed.  Left undefined if
 *  not true.
 */

#ifdef _POSIX_VERSION
#ifndef HOST_POSIX
#define HOST_POSIX 1
#endif
#endif

/*  Check for cheaters!  This can happen when the CPP does neat things
 *  but the include directory isn't up to snuff...  ULTRIX4.4 is old.
 */
#ifdef vax
#ifdef bsd4_2
#undef HOST_POSIX
#endif
#endif

#ifdef HOST_POSIX
#include <stdlib.h>
#else
#include <sys/stat.h>
extern char *getenv();
extern char *malloc();
extern int   fchmod();
extern int   ioctl();
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/*-------------------------------------------------------------------*/

/*  Change a pointer value into an integer value, type "diff_t" */
#define PTR2INT(p) ((char *)(p) - (char *)0)

/*  Return byte offset of "element" inside structure "type". */
#ifndef OFFSETOF
#define OFFSETOF(type, element) (PTR2INT(&((type *)0)->element))
#endif

#undef PARAMS
#ifdef __STDC__
#define PARAMS(x) x
#else
#define PARAMS(x) ()
#endif

typedef int (*FPTR)();

typedef struct fnc_table {
    char command[20];
    FPTR func;
} FNC_TABLE;

/*  Control char to exit the FLINT sub-shell. */
#define FLINT_EXIT_CH ('X' - 64)

/*  When you relocate FLASH, just change the FLASH_HOME define.
 *  We use ANSI-C string concatenation to ``glue'' the prefix to
 *  the specific path.
 */
#ifndef FLASH_HOME
#error "FLASH_HOME undefined!  Try something like :"
#error "define  FLASH_HOME	\"/gaia/home/project/spede/sparc/tools\" "
#endif
#ifndef GDB_HOME
#define GDB_HOME "/gaia/home/project/spede2/Target-i386/i686/gnu"
#endif

/* GDB Configuration default files/excutables */
#if defined(TARGET_i386)
#define GDBPATH GDB_HOME "/bin/i386-unknown-gnu-gdb"
#define MODEL_GDBINIT_FILE FLASH_HOME "/etc/GDB159.RC"
#elif defined(TARGET_3b1)
#define GDBPATH GDB_HOME "/bin/68-gdb"
#define MODEL_GDBINIT_FILE GDB_HOME "/GDB159.RC"
#endif

/*  Located in user's project directory. */
#define USER_GDBINIT_FILE "./GDB159.RC"

/* FLASH Configuration default files/excutables */
#define DLPATH FLASH_HOME "/bin/dl"
#define FLPATH FLASH_HOME "/bin/fl"
#define FLASHSUPPATH FLASH_HOME "/bin/flash-sup"
#define CONFIG_FILE FLASH_HOME "/bin/Flash.cfg"
#define NETBOOT_PATH FLASH_HOME "/bin/netboot"

/*  Must define TTYPORT on command line (e.g. in Makefile).
 *  Each system is different, so don't provide defaults.
 *	Solaris:	/dev/term/b
 *	HU-UX 11:	/dev/tty0p0
 *	FreeBSD:	/dev/cua1
 */
#ifndef TTYPORT
#error "You must define TTYPORT!!!"
#endif

#define DBG_TYPE 1
#define DL_TYPE 2

#define PROMPT printf("\nFLASH %% ")

/*-------------------------------------------------------------------*/

extern char IDENT_STRING[];
extern char cmd_words[10][512];

extern int generate_config_file_flag;
extern int verbose_flag;
extern int debugger_only_flag;
extern int emacs_debugger_flag;

enum SupportCmd {
    SC_NONE = 0,
    SC_HARD_SAVE,
    SC_HARD_LOAD,
    SC_FLOPPY_SAVE,
    SC_FLOPPY_LOAD,
    SC_START_GDB_STUB,
    _SC_LAST
};

int          getch();
void         cmdinp();
void support PARAMS((enum SupportCmd));

typedef struct flash {
    char port[30];
    char dlpath[256];
    char flpath[256];
    char gdbpath[256];
    char pregdbpath[256];
    char download_file[256];
} CONFIG;

/* eof flash.h */
