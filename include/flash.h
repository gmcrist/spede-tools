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
#include <stdlib.h>

/*  How fast to talk with target?  Use B9600, B19200 or B38400.
 */
#ifdef TTYBAUD
#define _FLASH_CAT(x, y) x##y
#define _FLASH_XCAT(a, b) _FLASH_CAT(a, b)
#define FLASH_BAUD _FLASH_XCAT(B, TTYBAUD)
#else
#define FLASH_BAUD B38400
#endif

/*  Change a pointer value into an integer value, type "diff_t" */
#define PTR2INT(p) ((char *)(p) - (char *)0)

/*  Return byte offset of "element" inside structure "type". */
#ifndef OFFSETOF
#define OFFSETOF(type, element) (PTR2INT(&((type *)0)->element))
#endif

typedef int (*FPTR)();

typedef struct fnc_table {
    char command[20];
    FPTR func;
} FNC_TABLE;

/*  Control char to exit the FLINT sub-shell. */
#define FLINT_EXIT_CH ('X' - 64)

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
void support (enum SupportCmd);

typedef struct flash {
    char port[30];
    char dlpath[256];
    char flpath[256];
    char gdbpath[256];
    char pregdbpath[256];
    char download_file[256];
} CONFIG;

