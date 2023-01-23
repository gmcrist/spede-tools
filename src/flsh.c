/*   flsh.c    Flint Shell char router                    May 1993, Sudhir */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/flsh.c,v 1.2
 * 2000/06/21 17:22:19 aleks Exp $ */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "spede.h"

#define MAXHISTORY 10
#define MAX_CMD_LINE 50
#define LAST 0
#define INDX 1
#define MATCH 2

typedef struct h {
    char cmd[MAX_CMD_LINE];
    int  hcount;
} HTABLE;

HTABLE htab[MAXHISTORY];
int    hndxhd, hndxtl, cur_cnt;

int  upd_history();
int  weed_ctrlchs();
void disp_history();
int  getcmd();

int flsh(int whdl, char *XmitBuf) {
    char  cmd[MAX_CMD_LINE];
    char  res[MAX_CMD_LINE];
    char *cmdptr;
    char *resptr;
    int   NumFlg;

    resptr = res;
    cmdptr = cmd;

    while (1) {
        resptr = res;
        cmd[0] = (char)0;
        fgets(cmd, sizeof(cmd), stdin);

        if (!weed_ctrlchs(cmd)) {
            return 0;
        }

        if (cmd[0] == '!') {
            /* History related stuff */
            cmdptr = &cmd[1];

            while (*cmdptr) {
                if (*cmdptr == ' ') {
                    break;
                } else {
                    *resptr++ = *cmdptr++;
                }
            }

            *resptr = (char)0;

            if (strlen(res) == 0) {
                disp_history();
                cmd[0] = (char)0;
                strcpy(XmitBuf, cmd);
                /*continue;*/
            } else {
                if (res[0] == '!') { /* !! command */
                    if (strlen(res) > 1) {
                        printf("Syntax Error\n");
                        return 0;
                    } else {
                        if (!getcmd(XmitBuf, LAST, NULL)) {
                            printf("Command not present\n");
                            return 0;
                        } else {
                            strcat(XmitBuf, cmdptr);
                            /*printf("%s",XmitBuf);*/
                        }
                    }
                } else {
                    NumFlg = true;
                    resptr = &res[0];

                    if (isdigit(res[0])) {
                        while (*resptr) {
                            if (!isdigit(*resptr)) {
                                NumFlg = false;
                                break;
                            }
                            resptr++;
                        }

                        if (NumFlg == false) {
                            printf("Syntax Error\n");
                            return 0;
                        } else {
                            if (!getcmd(XmitBuf, INDX, res)) {
                                printf("Command not present\n");
                                return 0;
                            } else {
                                strcat(XmitBuf, cmdptr);
                            }
                        }
                    } else {
                        if (!getcmd(XmitBuf, MATCH, res)) {
                            printf("Command not present\n");
                            return 0;
                        } else {
                            strcat(XmitBuf, cmdptr);
                        }
                    }
                }
            }
        } else
            strcpy(XmitBuf, cmd);

        upd_history(XmitBuf);
        return 1;
    }
}

int upd_history(char *xbuf) {
    if (strlen(xbuf) == 0)
        return 1;

    if (hndxtl == MAXHISTORY) {
        for (int i = 0; i < hndxtl; i++) {
            memcpy(&htab[i], &htab[i + 1], sizeof(HTABLE));
        }

        hndxtl = MAXHISTORY - 1;
        hndxhd = htab[0].hcount;
    }

    strcpy(htab[hndxtl].cmd, xbuf);
    htab[hndxtl].hcount = cur_cnt;
    hndxtl++;
    cur_cnt++;

    return 0;
}

int getcmd(char *dst, int type, char *cat) {
    int val;

    switch (type) {
        case LAST:
            if (hndxtl != 0) {
                strcpy(dst, htab[hndxtl - 1].cmd);
                return 1;
            }

            break;

        case INDX:
            val = atoi(cat);

            if ((val < cur_cnt) && (val >= hndxhd)) {
                strcpy(dst, htab[val - hndxhd].cmd);
                return 1;
            }

            break;

        case MATCH:
            for (int i = hndxtl - 1; i >= 0; i--) {
                if (!strncmp(htab[i].cmd, cat, strlen(cat))) {
                    strcpy(dst, htab[i].cmd);
                    return 1;
                }
            }

            break;
    }

    return 0;
}

void disp_history(void) {
    for (int i = 0; i < hndxtl; i++) {
        printf("[%3d] %s\n", htab[i].hcount, htab[i].cmd);
    }
}

int weed_ctrlchs(char *cmd) {
    while (*cmd) {
        if ((int)*cmd == FLINT_EXIT_CH) {
            break;
        }
        if (!isprint(*cmd))
            return 0;
        else
            cmd++;
    }
    return 1;
}
