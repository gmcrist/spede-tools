######################################################
#
# makefile.in (prototype) for FLASH programs
# $Header: /export/home/spede/Source/Samples/00-Tools/flash/source/RCS/Makefile.in,v 1.4 2001/02/04 01:01:57 aleks Exp $ 
#

## Install warning flag to compiler.
##	For Solaris 2.5 "cc", use "-vc"
##	For GNU2, use "-Wall"
##
WARNINGS = -Wall
CC = gcc

#DL_LIBS = -lsocket -lnsl

TARGET = -DTARGET_i386
MODEL = -v -g
OPT = -O1

DEFINES = -DFLASH_HOME=\"/gaia/home/project/spede2/Target-i386/i686/tools\" -DTTYPORT=\"/dev/ttyS0\" -DTTYBAUD=38400

CFLAGS = $(OPT) $(MODEL) $(WARNINGS) $(DEFINES) $(TARGET)
LDFLAGS = $(MODEL)

BINDIR = /gaia/home/project/spede2/Target-i386/i686/tools/bin
ETCDIR = /gaia/home/project/spede2/Target-i386/i686/tools/etc

HDR = flash.h
FLASH_OBJS = flash.o cmdinp.o getch.o
FLINT_OBJS = flint.o flsh.o
DL_OBJS = download.o
SUP_OBJS = flash-sup.o

ALL_SOURCE = b.out.h cmdinp.c download.c flash-sup.c flash.c \
	flash.h flint.c flsh.c getch.c spede.h
ALL_PROGRAMS = flash fl dl flash-sup

#
# ---------------------------
#
all:	$(ALL_PROGRAMS)

scrub :	clean
	sccs clean

clean:
	-rm -f *.o core *~ $(ALL_PROGRAMS)

#
#  Set a rwx--x--x mode upon the program.  Delete them first in case
#  another user installed them.
#
install:	$(ALL_PROGRAMS)
	(cd $(BINDIR) ; rm -f $(ALL_PROGRAMS) );
	cp $(ALL_PROGRAMS)  $(BINDIR)
	(cd $(BINDIR) ; chmod 755 $(ALL_PROGRAMS) );
	( cd .. ; cp GDB159.RC $(ETCDIR) );
	chmod 664 $(ETCDIR)/GDB159.RC

source :
	for i in $(ALL_SOURCE) ; do sccs get $$i ; done;

#
# ---------------------------
#
flash:	$(FLASH_OBJS) $(HDR)
	$(CC) $(LDFLAGS) -o flash $(FLASH_OBJS) $(LIBS)

fl: 	$(FLINT_OBJS)
	$(CC) $(LDFLAGS) -o fl $(FLINT_OBJS) $(LIBS)

dl:	$(DL_OBJS)
	$(CC) $(LDFLAGS) -o dl $(DL_LIBS) $(DL_OBJS) $(LIBS)

flash-sup:	$(SUP_OBJS)
	$(CC) $(LDFLAGS) -o flash-sup $(SUP_OBJS) $(LIBS)


flash.o :	$(HDR) flash.c

cmdinp.o:	$(HDR) cmdinp.c
	$(CC) $(CFLAGS) -c cmdinp.c

getch.o:	$(HDR) getch.c
	$(CC) $(CFLAGS) -c getch.c

flint.o:	$(HDR) flint.c
	$(CC) $(CFLAGS) -c flint.c

flsh.o:		$(HDR) flsh.c
	$(CC) $(CFLAGS) -c flsh.c

download.o: download.c b.out.h spede.h $(HDR)
	$(CC) $(CFLAGS) -c download.c

flash-sup.o: flash-sup.c
	$(CC) $(CFLAGS) -c flash-sup.c


