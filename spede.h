/*  flash/source/spede.h */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/spede.h,v 1.1 2000/02/01 21:24:34 aleks Exp $ */


/* misc defines for spedePlus */
#define STDIN	0
#define STDOUT	1

/* define the processors that can be used */
#define MC68000		0
#define MC68010		1
#define MC68020		2
#define MC68030		3

/* define the languages that can be used */
#define MODULA2	1
#define C	2

#define myatoi(x)	strtol((x), NULL, 0)

/* define the file types that can be used */
#define BDOTOUT		2
#define ABM		3
#define ADOTOUT         4
#define ELF		5
#define SBBB		6		/* See http://www.acm.uiuc.edu/sigops/ */

/* define default entry point and stack */
#define DEFENTRY	0x3000
#define DEFSTACK	0x80000

/* bits used for various flags */
#define LISTING		0x4000
#define MAP		0x2000
#define XREF		0x1000
#define OVERFLOW	0x0800
#define RANGE		0x0400
#define SMALL		0x0200
#define WASTE		0x0080
#define ROM		0x0040
#define DEBUG		0x0020
#define VERBOSE		0x0008

/* default spedecf file is in current directory */
/* should be overridden when made: -DSPEDECF="/home/student/spede/spede.cf" */
#ifndef SPEDECF
#define SPEDECF		"spede.cf"
#endif

/* name of local config file, in home dir or current dir */
#define LOCALCF		".spedecf"

/* names of commands to be called, and where to find them */
/* for Modula 2 */
#define SPHOME		"/usr/home/student/spede/usrbin"
#define MODCOMP		"mod68k"
#define MODLINK		"asl"
#define ASSEMBLE	"as68"
#define STRIPCODE	"StripCode"
#define UNC		"unc"
