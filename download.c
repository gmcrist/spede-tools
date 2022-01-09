/*   flash/source/download.c  */
/* pseudo s-record downloader for spede/flames */
/* downloads a.out and abm format files  */
/* written by riyadth alkazily   */
/* Tue Sep 19 16:27:30 PST 1989  */
/* Modified to support FLASH system by Sudhir C */
/* Added POSIX terminal support by Brian Witt, July 1997 */

/* $Header: /export/home/aleks/Projects/Intel-159/Core/00-Tools/flash/source/RCS/download.c,v 1.6 2001/08/31 05:49:32 aleks Exp $ */

/*  Version 2.3 includes M68010 (AT&T 3B1) support.  Version 3.1
 *  is entiredly i386 ELF files.  Sorry.
 */

/*  Handled by Makefile! */
/* #define  TARGET_i386 */
/* #undef   TARGET_3B1 */

/*  Should we even ask the target about Ethernet?
 */
#define  SW_QUERY_ETHERNET	1


#include "flash.h"
#include <sys/stat.h>
#include <setjmp.h>
#include <time.h>
#include <fcntl.h>

#ifdef HOST_POSIX
# include <termios.h>
# include <stdlib.h>
#else
# include <sys/ioctl.h>
# include <termio.h>
#endif

#if SW_QUERY_ETHERNET
# include <arpa/inet.h>
# undef PAGESIZE
#endif

/*  For FTP support. */
#include <pwd.h>

#include "elf.h"
#include "b.out.h"
#include "spede.h"

#ifndef HOST_POSIX
# define const
#endif


/*
 *  Print out extra debugging information.
 */
#define  SW_WATCH  0

/*
 *  For debugging target conversation, enable this.  Output stored
 *  in "/tmp/Trace" file.
 */
#define  SW_TRACE_TARGET  0

/*
 *  Insert micro pauses between each char sent to target.
 *  NOTE:  usleep() uses SIGALRM, do does alarm().  Therefore
 *	   you get no timeouts.
 */
#define  SW_SEND_SLOW  0


#define RECSIZ		128
#define BLOCKSIZ 	268
#define MAXTRIES 	10
#define ACK 		0x06
#define EOT		0x04
#define CTRLP 		('P'^0x40)
#define SREC_EOT   	'\004'



#ifndef TRUE
# define TRUE  1
#endif
#ifndef FALSE
# define FALSE 0
#endif

#if SW_SEND_SLOW
# define  SEND_SLOW_PAUSE  usleep(5000)
extern unsigned  usleep PARAMS((unsigned));
#else
# define  SEND_SLOW_PAUSE
#endif


/* ----------------------------------------------------------------------- */

#ifdef TARGET_i386
/*  Twisted into Intel little-endian byte order. */
# define  ELF_ID   0x464c457f    /* '\07fELF' */
#endif

#ifdef HOST_POSIX
struct termios  otty, ntty;
#else
struct sgttyb otty, ntty;
struct termio tbuf,o_tbuf;
#endif

struct stat term_stat;
int     fd_target;
FILE *	imagef;
int 	blk_cnt;		/* Counts how much downlaoded. */

time_t	start_time;		/* Used by ether and serial downloaders */

char *	start_addr = NULL;	/* Where the code starts. */

/*  Forward declares  */
#ifdef TARGET_i386
void	put386 PARAMS(( FILE *, char *, int ));
void	get386 PARAMS(( FILE *, char *, int ));
#endif
#ifdef TARGET_3B1
void	put68 PARAMS(( FILE *, char *, int ));
void	get68 PARAMS(( FILE *, char *, int ));
int	read_abm PARAMS(( FILE * ));
#endif
int	read_ab_out PARAMS(( struct bhdr * ));
int 	read_elf PARAMS(( FILE *, long size ));
int 	read_sbbb PARAMS(( FILE * ));
int	atten_target PARAMS(( void ));
int	writech(char ch, int fd);
int	readch(int fd);

void	send_abort PARAMS(( int fd ));
void	term PARAMS(( int fd ));
void	sends PARAMS(( int fd, long, unsigned char *, int ));
void	header PARAMS(( int fd ));
void	ResetAndCloseFiles PARAMS((void));
int	download PARAMS((const char *, const char *));

int	try_ethernet(int target_file_desc, const char * dli_fname);
int 	grok_object_file PARAMS((FILE *, struct bhdr *, long *dlsize));
char *	get_home_dir PARAMS((const char * username));
int 	query_ether_addr PARAMS((int fd, char *));
int	target_readline PARAMS((char *inbuf, int size_inbuff,
				int oper_timeout, int timeout,
				const char *resp1, const char *resp2,
				const char *resp3, const char *resp4));
void 	target_write(const char * outbuf, int size);

void	cleanup PARAMS((int));
void	Timeout PARAMS((int));

char	to_mesg[80];		/* alarm() timeout error message. */

#if SW_TRACE_TARGET
FILE *	tracef = NULL;
#endif


/* -------------------------------------------------------------------- */

char	host_little_endian;

/* -------------------------------------------------------------------- */


char * progname;

int 	verbose_flag = FALSE;
int 	force_serial_flag = FALSE;
int	go_flag = FALSE;


void
usage(rc)
     int  rc;
{
    printf("Usage : dl [-h] [-v] [-s] [-go] <filename> <device>\n");
    exit(rc);
}


int
main(argc,argv)
int argc;
char **argv;
{
    char * fname;
    char * dev;
    int    result;

    if( argc == 1 || strcmp(argv[1], "-h") == 0 ) {
	usage(1);
    }

	/*  Process cmdline options. */
	progname = argv[0];
	while( (++argv)[0][0] == '-' )
	  {
	    switch( argv[0][1] ) 
	      {
	      case 'v' :
		verbose_flag = TRUE;
		break;

	      case 's' :
		force_serial_flag = TRUE;
		break;

	      case 'g' :
		  if( argv[0][1] != 'o' ) {
		      printf("ERROR: -%s unrecognized\n", argv[0]);
		      exit(2);
		  }
		  go_flag = TRUE;
		  break;

	      default :
		usage(1);
	    }
	  }

	start_time = (time_t) 0;
        fname = argv[0];
	dev = argv[1];
#if SW_TRACE_TARGET
	tracef = fopen("/tmp/Trace", "w");
#endif
	result = download(fname, dev);

#if SW_TRACE_TARGET
	fclose(tracef);
#endif

	return( result );
}   /* end main() */



/*
    download(char *filename, char *destination)

    attempt to download the file 'filename' to the device 'destination'.
    if destination is NULL, use stdout for downloading
*/

int
download(file, dest)
const char *file;
const char *dest;
{
    struct bhdr  filhdr;
    long 	size;
    int  	c, count, type=ABM;
    int   	result = 0;

    /*  Make sure we got a filename! */
    if (file == NULL || file[0] == 0)
	return (-1);

    {
	int 	value = 0x12345678;
	if( *((char *) & value) == 0x78 ) {
	    host_little_endian = TRUE;
	} else {
	    host_little_endian = FALSE;
	}
#if SW_WATCH
	printf("Host little-endian? %s\n",
	       (host_little_endian ? "YES" : "NO") );
#endif
    }

    /* allow alternate destinations for downloading.. not just stdout */
    if (dest == NULL) {
	fd_target = STDIN;		/* STDIN/STDOUT */
    }
    else {
        if ((fd_target = open(dest, O_RDWR)) < 0) {
	    fprintf(stderr, "%s: couldn't open %s\n", progname, dest);
	    return 2;
        }
    }

    /* check output line to see if it is a tty device or not... */
    if (!isatty(fd_target)) {
	if (fd_target != STDIN) {
	    close(fd_target);
	}
	fprintf(stderr, "%s: %s is not a tty\n", progname, dest);
	return 2;
    }

    /* try to figure out what kind of file we are downloading */
    /* try to open file */
    if ((imagef = fopen(file, "r")) == NULL) {
	    fprintf(stderr, "%s: couldn't open %s\n", progname, file);
	    return 2;
    }

    type = grok_object_file( imagef, & filhdr, & size );
    if( type <= 0 ) {
	fclose(imagef);
	if (fd_target != STDIN) {
	    close(fd_target);
	}
	fprintf(stderr, "Invalid download file (format unknown)\n");
	return 2;
    }


#if SW_SEND_SLOW
    fprintf(stderr,"Will send bytes slowly; timeout disabled.\n" );
#endif
    printf("Total blocks to download:  0x%lx  (%d bytes each)\n\n", size, RECSIZ);
    blk_cnt = 0;

    /* get info on current terminal state */
#ifdef HOST_POSIX
    tcgetattr(fd_target, &otty);
#else
    ioctl(fd_target, TIOCGETP, &otty);
#endif
    fstat(fd_target, &term_stat);

    /* protect ourselves, allowing easy signal recovery */
    signal(SIGHUP, cleanup);
    signal(SIGINT, cleanup);
    signal(SIGQUIT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGSTOP, cleanup);
    signal(SIGTSTP, cleanup);
    signal(SIGALRM, Timeout);

    /* do not allow users to send to us while we are downloading... */
    fchmod(fd_target, term_stat.st_mode & 0700);	/* mesg n */

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
    ntty.c_cflag |= (CS8 | FLASH_BAUD) ;
        ntty.c_cc[VMIN] = 1;
        ntty.c_cc[VTIME] = 1;
    tcsetattr(fd_target, TCSANOW, &ntty);
#else
    ntty.sg_flags = (ntty.sg_flags & ~ECHO) | RAW;
    /* tell the device driver to use them */
    ioctl(fd_target, TIOCSETP, &ntty);

    ioctl(fd_target,TCGETA,(caddr_t)&tbuf);
    o_tbuf = tbuf;
        tbuf.c_iflag = 0;
        tbuf.c_oflag &= ~OPOST;
        tbuf.c_lflag &= ~(ISIG | ICANON | ECHO | XCASE);
    tbuf.c_cflag &= ((~CBAUD));
    tbuf.c_cflag |= FLASH_BAUD;
    tbuf.c_cflag &= ~(CSIZE | PARENB);
    tbuf.c_cflag |= (CS8 | FLASH_BAUD) ;
        tbuf.c_cc[VMIN] = 1;
        tbuf.c_cc[VTIME] = 1;
    ioctl(fd_target,TCSETAF,(caddr_t)&tbuf);
#endif

#if SW_QUERY_ETHERNET
    if(  force_serial_flag ) {
	result = try_ethernet(fd_target, file);
	printf("i am in ethernet");
	alarm(0);
	if( result == 0 ) {
	    goto DOWNLOAD_OK_CLEANUP;
	} else {
	    printf("%s\n", to_mesg);
	    if( result > 0 ) {
	    /*  Bad troubles... try_ethernet has already displayed a
	     *  diagnostic message.
	     */
		ResetAndCloseFiles();
		printf("Result is %d", result);
		return( result );
		
	    }
	}
	/*  result < 0, try the serial download approach.. */
    }
#endif
    result = 0;

    count = atten_target();
    if(count < 0) {
	fprintf(stderr,"ERROR: %s\n", to_mesg);
	return -1;
    }

    /* send pseudo s-record header */
    start_time = time(NULL);
    count = 999;
    alarm(30); 		/* Wait for 30 seconds */
    strcpy(to_mesg, "Comm Error on Header. ");
    do {
/*	printf("Sending HEader\n");*/
	header(fd_target);
	c = readch(fd_target);

    } while ((c != ACK) && (--count));
    if(count <= 0){
	printf("ERROR: %s\n", to_mesg);
	return -1;
   }
   alarm(0); /* cancel alarm */

   printf("Now have attention of monitor...\r");
   fflush(stdout);
    /* what do we do if we retry MAXTRY times? exit? */
    strcpy(to_mesg, "Comm Error in data section. ");

    switch (type) {
	case ADOTOUT :	/* a.out format file to download */
		read_ab_out( & filhdr );
	
		break;

	case BDOTOUT :	/* b.out format file to download */
		read_ab_out( & filhdr );
	
		break;

#ifdef TARET_3B1
	case ABM :	/* ABM format file to download */
			/* ABM format can have several chunks.. */
		read_abm(imagef);
		break;
#endif
	    case SBBB :
		read_sbbb(imagef);
		break;

	    case ELF :
		read_elf(imagef, size);
		break;

	    default :
		fprintf(stderr,"INTERNAL ERROR: no code to download.\n");
		ResetAndCloseFiles();
		return 3;

    }

    /* send termination block until an ACK is received */
    count = MAXTRIES;
    alarm(10);
    strcpy(to_mesg, "Comm Error on finish. ");
    do {
	term(fd_target);
	c = readch(fd_target);
    } while ((c != ACK) && (--count));
    if(count <= 0){
	fprintf(stderr, "ERROR: %s\n", to_mesg);
	return 2;
    }

  DOWNLOAD_OK_CLEANUP :

    /*  Cancel the alarm.  Change 'start_time' into delta_time.  Tell user
     *  where the code ended up.  Display bytes/per rate, rounded up.
     *  (Floating-point rate was just too weird.)
     */
    alarm(0);
    start_time = time(NULL) - start_time;
    fprintf(stderr, "Load Successful ; Code loaded at 0x%p (%d bytes/sec)\n",
	    start_addr, (int)((size*RECSIZ + RECSIZ/2) / start_time) );

    /*  Send the "go" command?  Do it while we have the serial port open.
     */
    if( go_flag ) {
	target_write("go\r", 3);
    }


    ResetAndCloseFiles();

    /* exit cleanly */
    return (result);
}


void
ResetAndCloseFiles()
{
    /* set terminal modes to original state */
#ifdef HOST_POSIX
    tcsetattr(fd_target, TCSANOW, & otty );
#else

    ioctl(fd_target, TIOCSETP, &otty);
    ioctl(fd_target,TCSETAF,(caddr_t)&o_tbuf);
#endif

    /* set the terminal perm back to normal, allowing messages if desired */
    fchmod(fd_target, term_stat.st_mode);	/* mesg reset */

    /* we're finished with the files, close them */
    fclose(imagef);
    if( fd_target != STDIN ) {
	close(fd_target);
    }

    /* return signals to normal/default */
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGSTOP, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
}   /* end ResetAndCloseFiles() */


void
Timeout(int unused)
{
    unused = unused;
    ResetAndCloseFiles();

	printf("Communication Error. %s Unable to download .\n", to_mesg);
	printf("Please reset the target computer.\n");

    exit(2);
}


/*
    cleanup()

    signal handler to abort nicely on receipt of a signal.
    Start with termination packet.  Then shut down files.

    NOTE: Sending a ABORT packet rarely works since the target might be
	  in the middle of receiving a data packet.
*/
void
cleanup(int signo)
{
    signal(SIGALRM, Timeout);
    alarm(6);			/* Give us 6 seconds to cleanup. */
    strcat(to_mesg, "(Cleanup)");

    fprintf(stderr, " [BREAK] ");
    if( start_time != (time_t) 0 ) {
	fprintf(stderr, "(sending ABORT) ");
	fflush(stderr);
	start_time = (time_t) 0;
	send_abort(fd_target);
    }

    ResetAndCloseFiles();

    fprintf(stderr, "\nDownload exiting on signal(%d)\n", signo);
    exit(2);
}   /* end cleanup() */


/* -------------------------------------------------------------------- */

/*
 *   header(int fd)
 *
 *   send an S record header block to the target, starting download
 */

void
header(fd)
     int  fd;
{
    register char *ptr;
    register int i;
    char outstr[1024];

    ptr = outstr;
    *ptr++ = 'S';
    *ptr++ = '0';
    for (i = BLOCKSIZ - 2 ; i > 0 ; i--)
	*ptr++ = '0';

    *ptr = (char)0;
    alarm(6);
    target_write( outstr, strlen(outstr) );

}   /* header */


/*
 *    sends(int fd, long address, char *ptr, int count)
 *
 *   send count bytes of data, located at ptr, to the target machine
 *   using pseudo S record format.  send bytes to the indicated address
 *   in the target
 */

void
sends(fd,addr,buf,cnt)
int  fd;
long addr;
register unsigned char *buf;
int cnt;
{
    register char *ptr;
    char outstr[1024];
    char withchk[1024];
    int chksum, pad;

    pad = RECSIZ - cnt;
    chksum = 0;
    ptr = withchk;

    sprintf(ptr, "S2%02X", cnt - 1);	/* only load memory with cnt bytes */
    ptr += 4;
/*  chksum += ((4+cnt) & 0xff);	*/
    sprintf(ptr, "%06lX", addr & 0x00ffffff);
    ptr += 6;
/*  chksum += (addr & 0xff) + ((addr >> 8) & 0xff) + ((addr >> 16) & 0xff); */

    while (cnt--) {
	sprintf(ptr, "%02X", *buf);
	ptr += 2;
	chksum += *buf++;
    }

    while (pad-- > 0) {		/* fill the block so the rom will get it */
	*ptr++ = '0';
	*ptr++ = '0';
    }
    *ptr = '\0';

    alarm(6);
    sprintf(outstr, "%s%02X", withchk, chksum & 0xff);
    target_write( outstr, strlen(outstr) );

}   /* sends */


/*
 *   term(FILE *fp)
 *
 *   send normal termination S record to the opened file, ending download
 */

void
term(fd)
     int  fd;
{
    register char *ptr;
    register int i;
    char outstr[1024];

    ptr = outstr;
    *ptr++ = 'S';
    *ptr++ = '9';
    for (i = BLOCKSIZ - 2 ; i > 0 ; i--) {
	*ptr++ = '0';
    }
    *ptr = '\n';
    *ptr = 0;

    alarm(3);
    target_write(outstr, strlen(outstr) );
}   /* term() */


/*
    send_abort(int fd)

    send abort S record to the opened file to abort the download
*/
void
send_abort(fd)
     int	fd;
{
    register char *ptr;
    register int i;
    char outstr[1024];

    /*  Terminate the current packet. */
    for( i=10 ; --i > 0 ; ) {
	writech(fd, '.');
    }
    writech(fd, '\r');
    writech(fd, '\r');

    ptr = outstr;
    *ptr++ = 'S';
    *ptr++ = '8';
    for (i = BLOCKSIZ - 2 ; i > 0 ; i--)
	*ptr++ = '0';

    for(i = 0; i < strlen(outstr);i++){
	while(write(fd, & outstr[i], 1) == EOF);
	SEND_SLOW_PAUSE;
    }

}   /* send_abort() */


/* --------------------------------------------------------------- */

/**
 *	Drive the ethernet downloader.
 *
 *	RETURN: =0 means we downloaded without errors.
 *		>0 means really bad troubles.
 *		<0 please fallback to serial download.
 */
#if SW_QUERY_ETHERNET
int
try_ethernet(int targetFD, const char * dli_fname)
{
    char	ether_addr[32];   	/* MAC string, if ethering. */
    char	temp[256];
    char * 	p;
    int 	c;
    struct in_addr	targetIP;

    /*  First try ethernet download via FTP (for RMON) */
    if( query_ether_addr(targetFD, ether_addr) )
    {
# if TARGET_i386
	/* -------------------------------------------------------- */
	fprintf(stderr, "Ethernet download...");
	fflush(stderr);

	alarm(30);
	strcpy(to_mesg, "Waiting for NET RARP response");

	strcpy( temp, "net rarp\r");
	target_write( temp, strlen(temp) );
	c = target_readline(temp, sizeof(temp), 30, 8,
			    "IP =", "ERROR", "timed out", NULL);
	switch( c ) {
	  case 1 :	/* Normal "IP = a.b.c.d" message */
	    break;

	  case 2 :
	  case 3 :
	    printf("\nERROR: Target can't RARP itself.\n");
	    return 2;

	  case 0 :
	  case -1 :
	    printf("\nTimeout: %s\n", to_mesg);
	    return -1;
	  }
	fprintf(stderr, "(RARP'ed) ");
	fflush(stderr);
	p = strstr(temp, "IP =");
	assert( NULL != p );
	p += 4;   			/* Point just beyond '=' */
	while( !isdigit((int)*p) ) {
	    ++p;
	}
	targetIP.s_addr = inet_addr(p);
	sprintf(temp, NETBOOT_PATH " %s %s &", dli_fname, inet_ntoa(targetIP));
	if( verbose_flag ) {
	    printf( "%% %s\n", temp );
	}
	c = system( temp );
	if( c != 0 ) {
	    fprintf(stderr, "ERROR: Can't start host NETBOOT!\n");
	    return -1;
	}

	start_time = time(NULL);
	alarm(30);
	sprintf( temp, "net load %04lx\r", (unsigned long) start_addr );
	target_write( temp, strlen(temp) );

	/*  Wait for FTP download to finish.... 20 second.  Use "15" for
	 *  operation timeout since most output will arrive at end.
	 */
	strcpy(to_mesg, "NETBOOT timeout; please reset target.");

	c = target_readline(temp, sizeof(temp), 20, 15,
			    "LOAD finish", "ERROR", "Warning", "ABORTED");
	switch( c ) {
	  case 1 :	/* Normal "reloc success" message */
	    break;

	  case 2 :
	  case 3 :
	  case 4 :
	    printf("\nERROR: NETBOOT troubles on target.\nERROR: %s", temp);
	    return 2;

	  case 0 :
	  case -1 :
	    printf("\nTimeout: %s\n", to_mesg);
	    return 2;
	  }

	to_mesg[0] = '\0';
	fputc('\n', stderr);
	return 0;

# elif TARGET_3B1
	/* -------------------------------------------------------- */
	/*  Monitor replied to request for "eth1".  Copy file to FTP area
	 *  (dir ~ftp/spede/) then issue download command.
	 */
	char *  ftp_homedir = get_home_dir("ftp");

	if( ftp_homedir == NULL ) {
	    printf("Warning: Can't find FTP homedir, using serial download.\n");
	    return -1;
	}

	fprintf(stderr, "Ethernet download...");
	fflush(stderr);
	start_time = time(NULL);

	sprintf( temp, "rm -f %s/spede/%s", ftp_homedir, ether_addr );
	if( verbose_flag ) {
	    printf( "%% %s\n", temp );
	}
	system( temp ); 	/* Diagnostics go to stdout! */

	sprintf( temp, "cp -p %s %s/spede/%s",
		 dli_fname, ftp_homedir, ether_addr );
	if( verbose_flag ) {
	    printf( "%% %s\n", temp );
	}
	c = system( temp ); 	/* Diagnostics go to stdout! */
	if( c != 0 ) {
	    fprintf(stderr, "ERROR: Can't copy image to FTP area!\n");
	    return -1;
	}

	sprintf( temp, "le %04lx\r", (unsigned long) start_addr );
	alarm(10);
	target_write( temp, strlen(temp) );

	/*  Wait for FTP download to finish.... 120 second. */
	strcpy(to_mesg, "FTP download timeout; please reset target.");
	
	c = target_readline(temp, sizeof(temp), 120, 15,
			    "reloc success", "ERROR", "Warning", NULL);
	switch( c ) {
	  case 1 :	/* Normal "reloc success" message */
	    break;

	  case 2 :
	  case 3 :
	    printf("\nERROR: Target had problems with FTP download.\n");
	    return 2;

	  case 0 :
	  case -1 :
	    printf("\nTimeout: %s\n", to_mesg);
	    return 2;
	  }

	fputc('\n', stderr);
	return 0;
# endif
    }

    return -1;		/* Try serial download.. */
}   /* try_ethernet() */
#endif


/* --------------------------------------------------------------- */


/**
 *	Inspect object file, return the total 128-byte blocks to download.
 *	Information in 'filehdr' is really an opaque datum to the appropriate
 *	downloader routine.  It can be filled in with any info.
 *
 *	Return value is type of file, whether ADOTOUT,
 *	BDOTOUT, ABM, ELF or SBBB.  Return <= 0 if error.
 *
 *	If ELF, we read "start_addr" and "codesize".  Lead 'imagef' at
 *	start of file so header is downloaded.  Helps monitor identify
 *	what we're feeding it.
 *
 *	Leaves fileptr in 'imagef' at start of stuff to download.  Since
 *	ABM files have no header, it rewind()'s after probing.
 *
 *	GLOBALS:	start_addr (write)
 */

int
grok_object_file(imagef, filehdr, pBlockCnt )
     FILE *		imagef;
     struct bhdr *	filehdr;
     long *		pBlockCnt;	/* Block size */
{
    int   type = -1;
    long    addr;
    long    size = 0;
    unsigned long	magicnum;
    Elf32_Off		offset;
     long  		codesize;
     long  		datasize;
     char		buffer[32];
     int 		rc;
     struct stat	sb;
     auto unsigned	temp32;

     /*  First check for SBBB bundle file. */
     rc = fread(buffer, 1, sizeof(buffer), imagef);
     if( rc < 16 ) {
	 return -1;
     }
     if( strcmp(buffer, "SBBB/Directory") == 0 ) {
	 type = SBBB;
	 if( fstat(fileno(imagef), & sb) < 0 ) {
	     return -1;
	 }
	 
	 printf("File type is 'SBBB' (UIUC SIGOPS)\n");
	 start_addr = (char *) 0x100000;
	 *pBlockCnt = (sb.st_size + 0x7F) >> 7;
	 rewind(imagef);
	 return( type );
     }

     /*  Now make a check for ELF file, Intel '386 arch... */
     if( strcmp( buffer, "\177ELF\001\001\001" ) == 0 &&
	 (buffer[16] == 0x02 && buffer[17] == 0x00) ) {

	 /*  The header is written in target byte order, not host byte
	  *  order.  Read the header first in host order and extract
	  *  code and data size.  Then rewind the file so the downloader
	  *  will start at the header and work from there.
	  */
	 fseek( imagef, OFFSETOF(Elf32_Ehdr, e_entry), SEEK_SET);
	 get386(imagef, (char *) &temp32, 4);
	 filehdr->entry = temp32;

	 /*  Read "e_phoff" (program header file offset). */
	 get386(imagef, (char *) &offset, 4);
	 if( offset == 0 ) {
	     /* ERROR: Program header offset is 0. */
	     return -1;
	 }

	 /*  The program header "Elf32_Phdr" is 32 bytes. */
	 fseek( imagef, offset, SEEK_SET);
	 get386(imagef, (char *) &magicnum, 4);
	 if( magicnum != PT_LOAD ) {
	     /* ERROR: Not a load file. */
	     return -1;
	 }

	 /*  Good ELF headers have load file offset == 0.  We want to load
	  * 	the ELF header and everything into memory (but not the 
	  *	symbol tables).
	  */
	 get386(imagef, (char *) &magicnum, 4);
	 if( magicnum != 0 ) {
	     return -1;
	 }

	 /*  Smells good.  Read "p_filesz" which includes code and data
	  *  (we're non-paged).  Round up to S-Record block size and
	  *  return.
	  */
	 type = ELF;
	 printf("File type is 'ELF'\n");
	 get386(imagef, (char *) &magicnum, sizeof(magicnum));
	 start_addr = (char *) magicnum;

	 fseek( imagef, offset + OFFSETOF(Elf32_Phdr, p_filesz), SEEK_SET);
	 get386(imagef, (char *) &temp32, 4);	/* File Size */
/*  printf(" ELF load size = 0x%x\n", temp32); */
	 size = (temp32 + 0x7f) >> 7;

	 rewind(imagef);
	 /* return download size and object type */
	 *pBlockCnt = size;
	 return( type );
     }	/* If maybe ELF.. */


     rewind(imagef);
    get386(imagef, (char *) &magicnum, sizeof(magicnum));
    start_addr = NULL;

    switch(magicnum) {
	case 0x00020107 :		/* AOUT Magic Number */
	case 0x01020107 :		/* AOUT Magic Number */
			type = ADOTOUT;
			printf("File type is 'a.out' (GNU)\n");
			/* get size info for block calculation */
			get386(imagef, (char *) & codesize, 4);
			get386(imagef, (char *) & datasize, 4);
			size = ((codesize + 0x7f) >> 7) +
			       ((datasize + 0x7f) >> 7);

			/* read in the a.out header */
			rewind(imagef);		/* return to beginning of file */

		get386(imagef, (char *) &filehdr->fmagic, sizeof(filehdr->fmagic));
		get386(imagef, (char *) &filehdr->tsize, sizeof(filehdr->tsize));
		get386(imagef, (char *) &filehdr->dsize, sizeof(filehdr->dsize));
		get386(imagef, (char *) &filehdr->bsize, sizeof(filehdr->bsize));
		get386(imagef, (char *) &filehdr->ssize, sizeof(filehdr->ssize));
		get386(imagef, (char *) &filehdr->entry, sizeof(filehdr->entry));
		get386(imagef, (char *) &filehdr->rtsize, sizeof(filehdr->rtsize));
		get386(imagef, (char *) &filehdr->rdsize, sizeof(filehdr->rdsize));
			start_addr = (char *) filehdr->entry;
			break ;
	case OMAGIC :
	case FMAGIC :
	case NMAGIC :
	case IMAGIC :	type = BDOTOUT;		/* b.out format */
			printf("File type is 'b.out' (C)\n");
			/* get size info for block calculation */
			get386(imagef, (char *) & codesize, 4);
			get386(imagef, (char *) & datasize, 4);
			size = ((codesize + 0x7f) >> 7) +
			       ((datasize + 0x7f) >> 7);
			/* read in the b.out header */
			rewind(imagef);		/* return to beginning of file */
		get386(imagef, (char *) &filehdr->fmagic, sizeof(filehdr->fmagic));
		get386(imagef, (char *) &filehdr->tsize, sizeof(filehdr->tsize));
		get386(imagef, (char *) &filehdr->dsize, sizeof(filehdr->dsize));
		get386(imagef, (char *) &filehdr->bsize, sizeof(filehdr->bsize));
		get386(imagef, (char *) &filehdr->ssize, sizeof(filehdr->ssize));
		get386(imagef, (char *) &filehdr->rtsize, sizeof(filehdr->rtsize));
		get386(imagef, (char *) &filehdr->rdsize, sizeof(filehdr->rdsize));
		get386(imagef, (char *) &filehdr->entry, sizeof(filehdr->entry));
			start_addr = (char *) filehdr->entry;
			break;

	default	:
		if (magicnum & 0xff000000) {
			/* high 8 bits of address contains data - text? INVALID! */
			return -1;
		}

		/* if nothing else, then must be abm... */
		type = ABM;
		printf("File type is ABM/OBJ (Modula/assembly)\n");
		/* skip through file to calculate download size */
		size = 0;
		while (feof(imagef) == 0) { /* not yet at end of file */
			get386(imagef, (char *) & codesize, 4);
			if( start_addr == NULL ) {
			    start_addr = (char *) magicnum;
			}
			if (codesize < 0)
				break;
			size += ((codesize + 0x7f) >> 7);
			/* note: fseek does not set eof flag */
			if (fseek(imagef, codesize, 1) < 0)
				break;
			/* we must now read to see if we are at eof */
			get386(imagef, (char *) &addr, 4);
		}
		rewind(imagef);
    }

    /* return download size and object type */
    *pBlockCnt = size;
    return( type );
}   /* grok_object_file() */


/* --------------------------------------------------------------- */


int
read_sbbb(imagef)
     FILE * imagef;
{
    long  addr;
    int   c, count;
    char  bufr[1024];
    int   bufcnt;

    addr = (long) start_addr;
    while (feof(imagef) == 0) { /* not yet at end of file */
	bufcnt = fread(bufr, 1, RECSIZ, imagef);
	if (bufcnt <= 0)
	    break;

	count = MAXTRIES;
	do {
		sends(fd_target, addr, bufr, bufcnt);
		c = readch(fd_target);
	      
	} while ((c != ACK) && (--count));
	if(count <= 0){
		printf("Comm Error\n");	
		return -1;
	}
	addr += bufcnt;

	blk_cnt++;
	printf("No of Blocks Downloaded : 0x%x \r",blk_cnt);
	fflush(stdout);
    }

    return 0;
}   /* end read_sbbb() */


int
read_elf(imagef, size)
    FILE * imagef;
    long size;
{
    long  addr;
    int   c, count;
    char  bufr[1024];
    int   bufcnt;
    int   remaining_bufcnt = size;

    /*  Send the ELF header down to target. */
    addr = (long) start_addr;
    bufcnt = fread(bufr, 1, 128, imagef);
    assert( bufcnt <= RECSIZ );
    count = MAXTRIES;
    do {
	sends(fd_target, addr, bufr, bufcnt);
	c = readch(fd_target);
	      
    } while ((c != ACK) && (--count));
    if(count <= 0){
	printf("Comm Error\n");	
	return -1;
    }
    ++blk_cnt;

    /*  Send the code/data block.  ELF makes them one piece. */
    addr += 128;
    while (feof(imagef) == 0 && --remaining_bufcnt >= 0 ) {
	bufcnt = fread(bufr, 1, RECSIZ, imagef);
	if (bufcnt <= 0)
	    break;

	count = MAXTRIES;
	do {
		sends(fd_target, addr, bufr, bufcnt);
		c = readch(fd_target);
	      
	} while ((c != ACK) && (--count));
	if(count <= 0){
		printf("Comm Error\n");	
		return -1;
	}
	addr += bufcnt;

	blk_cnt++;
	printf("No of Blocks Downloaded : 0x%x \r",blk_cnt);
	fflush(stdout);
    }

    return 0;
}   /* end read_elf() */


int
read_abm(imagef)
FILE   *imagef;
{
    long  addr, code_size;
    int   c, count;
    char  bufr[1024];
    int   bufcnt;

    while (feof(imagef) == 0) { /* not yet at end of file */
	/* get current address and size */
	get386(imagef, (char *) &addr, 4);
	get386(imagef, (char *) &code_size, 4);
	if( start_addr == NULL ) {
	    start_addr = (char *) addr;
	}

	/* send current module */
	while (code_size > 0) {
	    bufcnt = fread(bufr, sizeof(char),
		code_size >= RECSIZ ? RECSIZ : code_size,
		    imagef);
	    if (bufcnt <= 0)
		break;

	    count = MAXTRIES;
	    do {
		sends(fd_target, addr, bufr, bufcnt);
		c = readch(fd_target);
	      
	    } while ((c != ACK) && (--count));
	    if(count <= 0){
		    printf("Comm Error\n");	
		    return -1;
	    }
	    addr += bufcnt;
	    code_size -= bufcnt;
	    blk_cnt++;
	    printf("No of Blocks Downloaded : 0x%x \r",blk_cnt);
	    fflush(stdout);
	}
    }

    return 0;
}   /* read_abm() */


int
read_ab_out( filhdr )
    struct bhdr *filhdr;
{
    char bufr[1024];
    int bufcnt, c, count;
    long addr, code_size, data_size;


    /* get entry point and segment sizes */
    addr = filhdr->entry;
    code_size = filhdr->tsize;
    data_size = filhdr->dsize;
    start_addr = (char *) addr;

#if SW_WATCH
    printf("Code Size : 0x%lx\n",code_size);
    printf("Data Size : 0x%lx\n",data_size);
#endif
    /* send code segment */
    while (code_size > 0) {
	bufcnt = fread(bufr, sizeof(char),
	    (code_size >= RECSIZ ? RECSIZ : code_size), imagef);
	count = MAXTRIES;	
	do {
	    sends(fd_target, addr, (unsigned char *)bufr, bufcnt);
	    c = readch(fd_target);
	} while ((c != ACK) && (--count));
	if(count <= 0){
	    printf("Comm Error\n");
	    return -1;
	}
	blk_cnt++;
	printf("No of Blocks Downloaded : 0x%x \r",blk_cnt);
	fflush(stdout);
	addr += bufcnt;
	code_size -= bufcnt;
    }

    /* send data segment */
    while (data_size > 0) {
	bufcnt = fread(bufr, sizeof(char),
	    (data_size >= RECSIZ ? RECSIZ : data_size), imagef);
	count = MAXTRIES;
	do {
	    sends(fd_target, addr, (unsigned char *) bufr, bufcnt);
	    c = readch(fd_target);
	} while ((c != ACK) && (--count));
	if(count <= 0){
	    printf("Comm Error\n");	
	    return -1;
	}
	blk_cnt++;
	printf("No of Blocks Downloaded : 0x%x \r",blk_cnt);
	    fflush(stdout);
	addr += bufcnt;
	data_size -= bufcnt;
    }

    return 0;
}   /* read_ab_out */


/* --------------------------------------------------------- */

int	atten_count;

static void
startup_to(int signo)
{
#if SW_WATCH
    write(2, "()", 2);
#endif
    atten_count = 9999;

    signal(SIGALRM, startup_to);		/* And again! */
    alarm(5);
}   /* end startup_to() */

int
atten_target()
{
    void (*old_alarm)(int);
    char	eh = '?';
    int 	retries;
    char	c = ~ ACK;

    old_alarm = signal(SIGALRM, startup_to);
    writech('\r', fd_target);	/* Clear any previous (ether attempt) cruft */

    /*  signal 7300 that we are ready to download, await ack.  Use timer
     *  (alarm) to timeout us waiting for any response from target.  If
     *  timeout, then send ^P again.  Total time to wait is:
     *		retries * alarm(5)
     *  The timeout will cause readch() to return -1/EINTR.
     *	By keeping the for() loop inside the while, we can read many
     *	chars each "timeout" cycle.
     */
    retries = 4;
    alarm(5);
    strcpy(to_mesg, "Comm Error on Startup. ");

    /* Always send control-P if we don't get ACK from target. */
    do {
	write(1, & eh, 1);
	while(writech(CTRLP,fd_target) == EOF) ;
	SEND_SLOW_PAUSE;

	/*  This will read the prompt.. "90" must be longer than prompt. 
	 *  A timeout will set this way high so we can exit the for() loop
	 *  and resend the ^P atten char.
	 */
	for(atten_count = 0; atten_count < 90; atten_count++){
		c = readch(fd_target);
#if SW_WATCH
 printf(" (%c)", (isprint((int)c) ? c : '.')); fflush(stdout);		
#endif
		if(c == ACK)
			break;
	}
    } while ((c != ACK) && --retries >= 0 );

    alarm(0);
    signal(SIGALRM, old_alarm);

    return( retries );
}   /* end atten_target() */


int
writech(char ch, int fd)
{
    return write( fd, & ch, 1 );
}   /* end writech() */

int
readch(int fd)
{
    unsigned char   ch;
    int    result;

    result = read(fd, (char *) & ch, 1);
    if( result > 0 ) {
	result = ch;
    }
    return( result );
}   /* end readch() */


char *
get_home_dir(username)
     const char * username;
{
    struct passwd *   pwp;

    pwp = getpwnam(username);
    if( pwp == NULL ) {
	return NULL;
    }

    return strdup( pwp->pw_dir );
}   /* end get_home_dir() */


/*   query_ether_addr
 *	Ask the target for information about "eth1" device.  Success gives us
 *	the MAC address.  Send ^D (SREC_EOT) first to clear any previous
 *	download attempts.
 *	RETURN:  0 = not there, 1 = yes, target has ethernet.
 */
int
query_ether_addr(target, mac_string)
    int  target;
    char * mac_string;
{
    char  	input[1024];
    char *	p;
    char *	q;
    int   	resp;

#if TARGET_i386
    const char	ask_ether[] = { "\025dev eth1\r" };
    const char	Addr[] = { "MAC = " };
    const int	LineTimeOut = 3;

    mac_string[0] = '\0';
    alarm(10);
    strcpy(to_mesg, "Comm error on inquire");
    target_write( ask_ether, strlen(ask_ether) );

    resp = target_readline( input, sizeof(input), 12, LineTimeOut,
			    Addr, "Error", "Unknown command", NULL );
    if( resp != 1 ) {
# if SW_TRACE_TARGET
	fprintf(tracef, "<<< query_ether_addr() NOT THERE\n%s", input);
# endif
	return 0;		/* Ethernet not there... */
    }

#elif TARGET_3B1
    const char	ask_ether[] = { "\025i eth1\r" };    /* <KILL> i eth1 <RETURN> */
    const char	Addr[] = { "Addr=" };
    const int	LineTimeOut = 3;

# if SW_TRACE_TARGET
    fprintf(tracef, ">>> query_ether_addr()\n");
# endif

    mac_string[0] = '\0';
    strcpy(to_mesg, "Comm error on inquire");
    alarm(18);
    target_write( ask_ether, strlen(ask_ether) );

    /*  We've sent our request, now see what comes back.  Addr[] is a good
     *  thing.  "Error" and "Unknown command" are bad things.
     */
    resp = target_readline( input, sizeof(input), 8, LineTimeOut,
			    Addr, "Error", "Unknown command", NULL );
    if( resp != 1 ) {
# if SW_TRACE_TARGET
	fprintf(tracef, "<<< query_ether_addr() NOT THERE\n%s", input);
# endif
	return 0;		/* Ethernet not there... */
    }
#endif

    q = strstr(input, Addr);
    assert( q != NULL );

    for( p = mac_string, q += strlen(Addr) ; isalnum((int)*q) || *q == ':' ; ) {
	*p++ = *q++;
    }
    *p = (char) 0;
#if SW_TRACE_TARGET
	fprintf(tracef, "<<< query_ether_addr() THERE MAC=%s\n", mac_string);
#endif
    return 1;
}   /* end query_ether_addr() */


jmp_buf	jb_done_tr;

void
alarm_target_readstring(int signo)
{
    longjmp(jb_done_tr, signo);
}


/*   target_readline
 *	Read each line, with timeout, until one of four substrings is found.
 *	Returns substring index (1,2,3) or 0 if none found.  On return, the
 *	most recent line read is available in `inbuffer'.
 */

int
target_readline( inbuffer, size_inbuff, operation_timeout, line_timeout,
		 resp1, resp2, resp3, resp4)
     char *	inbuffer;
     int   	size_inbuff;
     int 	operation_timeout;
     int 	line_timeout;
     const char * resp1;
     const char * resp2;
     const char * resp3;
     const char * resp4;
{
    int 	remaining;
    int 	match;
    int    	got, j = 0;
    void 	(*old_alarm)(int);
    time_t	oper_done_time = time(NULL) + operation_timeout;

    assert( operation_timeout > line_timeout );
    alarm(0);
    old_alarm = ( void(*)(int) ) signal(SIGALRM, alarm_target_readstring);
    if( setjmp(jb_done_tr) != 0 ) {
        signal(SIGALRM, old_alarm);	/* Restore previous handler. */
#if SW_TRACE_TARGET
	fprintf(tracef, "readline: timeout\n");
	fflush(tracef);
#endif
	return -1;
    }

    remaining = MAXTRIES;
    inbuffer[0] = (char) 0;
    for( match = 0 ; match == 0 ; ) {
	if( time(NULL) > oper_done_time ) {
	    longjmp( jb_done_tr, -1 );
	    /*NOTREACHED*/
	}

	/*  Read one line from target. */
	alarm(line_timeout);
	for( j = 0 ; j < size_inbuff-2 ; j += got )
	{
	    got = read( fd_target, & inbuffer[j], 1 );
	    if( got == 0 ) {
		continue;
	    }
	    if( got < 0 ) {
		printf("Comm error..");
		fflush(stdout);
		if( --remaining <= 0 ) {
		    alarm(0);
		    return -1;
		}
		continue;
	    }

	    /*  For FLAMES, strip parity bit.  On end-of-line, go check for
	     *  matches.  Since the target can send CR-LF, we can get zero
	     *  length lines -- that's OK!
	     */
	    inbuffer[j] &= 0x7F;
#if SW_TRACE_TARGET
	    fputc( inbuffer[j], tracef );
	    fflush(tracef);
#endif
	    if( inbuffer[j] == '\r' || inbuffer[j] == '\n' )
	        break;
	}
	inbuffer[j] = (char) 0;
	if( 0 == j ) {
	    continue;
	}

#if SW_TRACE_TARGET
	fprintf(tracef, "(%d) [%d] ??? \"%s\"\n", j, strlen(inbuffer), inbuffer);
#endif
	/*  See if line contains any magic strings. */
	if( resp1 && NULL != strstr(inbuffer, resp1) )	match = 1;
	if( resp2 && NULL != strstr(inbuffer, resp2) )	match = 2;
	if( resp3 && NULL != strstr(inbuffer, resp3) )	match = 3;
	if( resp4 && NULL != strstr(inbuffer, resp4) )	match = 4;

    }

#if SW_TRACE_TARGET
    fflush(tracef);
#endif
    alarm(0);
    return( match );
}   /* end target_readline() */



/*   target_write
 *	Sends buffer to target.  Error conditions are by alarm.  When done,
 *	resets `errno' and clears alarm.
 */
void
target_write( const char * outbuff, int size )
{
    int   outfd = fd_target;
    int   j, remaining = MAXTRIES;

    for( j = 0; remaining >= 0 && j < size ; j++ ) {
   	while( write(outfd, & outbuff[j], 1) <= 0 ) {
		time_t   next_sec = time(NULL) + 1;
       		fprintf(stderr,"\nWrite Error... (%d) ", errno);
		errno = 0;
		/*
		 *  Use a delay loop for 1 second instead of sleep() so
		 *  we don't interfere with alarm() usage.
		 */
		while( time(NULL) < next_sec ) {
		}
		--remaining;
	}
	SEND_SLOW_PAUSE;
    }

#if SW_TRACE_TARGET
    fprintf(tracef, "WROTE %d chars: %s\n", j, outbuff);
#endif
    errno = 0;
    alarm(0);

}   /* end target_write() */


/* --------------------------------------------------------- */

#ifdef TARGET_i386

/*
    get386(FILE *fp, char *ptr, int size)

    reads size bytes (either 2 or 4) to address ptr from the
    file fp in little endian byte order (least significant
    byte first, followed by next, etc.)
*/

void
get386(file, p, c)
register FILE	*file;
register char	*p;
int c;
{
    if(c == 2) {
	*(short *)p = getc(file);
	*(short *)p = *(short *)p | (getc(file) << 8);
    }
    else {
	*(long *)p = getc(file);
	*(long *)p = *(long *)p | (getc(file) << 8);
	*(long *)p = *(long *)p | (getc(file) << 16);
	*(long *)p = *(long *)p | (getc(file) << 24);
    }
}

/*
    put386(FILE *fp, char *ptr, int size)

    writes size bytes (either 2 or 4) at address ptr to the
    file fp in little endian byte order (least significant
    byte first, followed by next, etc.)
*/

void
put386(file, p, c)
register FILE	*file;
register char	*p;
int c;
{
    if(c == 2) {
	putc(*(short *)p, file);
	putc(*(short *)p >> 8, file);
    }
    else {
	putc(*(long *)p, file);
	putc(*(long *)p >> 8, file);
	putc(*(long *)p >> 16, file);
	putc(*(long *)p >> 24, file);
    }
}
#endif


/* --------------------------------------------------------- */

#ifdef TARGET_3B1

/*
    get68(FILE *fp, char *ptr, int size)

    reads size bytes (either 2 or 4) to address ptr from the
    file fp in motorola standard byte order (most significant
    byte first, followed by next, etc.)
*/

void
get68(file, p, c)
register FILE	*file;
register char	*p;
int c;
{
    if(c == 2) {
	*(short *)p = getc(file);
	*(short *)p = *(short *)p << 8 | getc(file);
    }
    else {
	*(long *)p = getc(file);
	*(long *)p = *(long *)p << 8 | getc(file);
	*(long *)p = *(long *)p << 8 | getc(file);
	*(long *)p = *(long *)p << 8 | getc(file);
    }
}

/*
    put68(FILE *fp, char *ptr, int size)

    writes size bytes (either 2 or 4) at address ptr to the
    file fp in motorola standard byte order (most significant
    byte first, followed by next, etc.)
*/

void
put68(file, p, c)
register FILE	*file;
register char	*p;
int c;
{
    if(c == 2) {
	putc(*(short *)p >> 8, file);
	putc(*(short *)p, file);
    }
    else {
	putc(*(long *)p >> 24, file);
	putc(*(long *)p >> 16, file);
	putc(*(long *)p >> 8, file);
	putc(*(long *)p, file);
    }
}

#endif
