#ifndef SPEDE_H
#define SPEDE_H

/* misc defines for spedePlus */
#define STDIN 0
#define STDOUT 1

/* define the file types that can be used */
#define BDOTOUT 2
#define ABM 3
#define ADOTOUT 4
#define ELF 5
#define SBBB 6 /* See http://www.acm.uiuc.edu/sigops/ */

/* define default entry point and stack */
#define DEFENTRY 0x3000
#define DEFSTACK 0x80000

/* bits used for various flags */
#define LISTING 0x4000
#define MAP 0x2000
#define XREF 0x1000
#define OVERFLOW 0x0800
#define RANGE 0x0400
#define SMALL 0x0200
#define WASTE 0x0080
#define ROM 0x0040
#define DEBUG 0x0020
#define VERBOSE 0x0008

#endif
