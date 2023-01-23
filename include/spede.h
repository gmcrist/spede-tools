#ifndef SPEDE_H
#define SPEDE_H

#ifdef TTYBAUD
#define _FLASH_CAT(x, y) x##y
#define _FLASH_XCAT(a, b) _FLASH_CAT(a, b)
#define FLASH_BAUD _FLASH_XCAT(B, TTYBAUD)
#else
#define FLASH_BAUD B38400
#endif

/*  Control char to exit the FLINT sub-shell. */
#define FLINT_EXIT_CH ('X' - 64)

/* misc defines for spedePlus */
#define STDIN 0
#define STDOUT 1

/* define the file types that can be used */
#define BDOTOUT 2
#define ABM 3
#define ADOTOUT 4
#define ELF 5
#define SBBB 6 /* See http://www.acm.uiuc.edu/sigops/ */

#ifndef CBAUD
#define CBAUD 0010017
#endif

#ifndef XCASE
#define XCASE 0000004
#endif

#endif
