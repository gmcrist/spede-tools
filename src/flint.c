/*  flint.c    Flint shell (main)                      Sudhir, may 1993 */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/flint.c,v 1.3 2000/02/19 21:27:07 aleks Exp $ */

/* ------------------------------------------
 *   Why can't use just use `tip' ??
 * ------------------------------------------
 */

#include "flash.h"

#include <fcntl.h>

#ifdef HOST_POSIX
# include <termios.h>
#else
# include <sys/ioctl.h>
# include <sys/stat.h>
# include <termio.h>
#endif


#define  SW_WATCH  0


extern int   usleep(unsigned int);


#ifdef HOST_POSIX
struct termios  otty, ntty;
#else
struct sgttyb otty,ntty;
struct termio tbuf,o_tbuf;
#endif

static pid_t    child_id;
static int	status;			/* Status from wait() after fork() */

int rhdl,whdl;
char std_buf[BUFSIZ];
int  ctrlc_hit;
extern int hndxhd,hndxtl,cur_cnt;	/* History index and current ptr */

extern int flsh( /* int, char * */ );
/* Commnad line is "flint port" */
char *dev;

/* LOCAL forwards: */
void cleanup();
void ctrlc_handler();
void child_cleanup();


#define signal(pid,sig)


/* ----------------------------------------------------------------------- */

/**
 *	Create two processes upont this port.  The child listens to
 *	the user, and the parent talks with the target computer.
 */

int
main(argc,argv)
int argc;
char **argv;
{

  char	buffer[ 256 ];
  char c;
  int i,n;

  /* Open the TTY device */
	if(argc != 2){
		printf("\nUsage : fl <dev>\n");
		exit(1);
	}
	dev = argv[1];

#ifdef HOST_POSIX
	rhdl = open(dev,O_RDONLY);
#else
	rhdl = open(dev,O_RDONLY|O_NDELAY);
#endif
	if(rhdl < 0){
		printf("\n");
		perror("Error opening device");
		exit(1);
	}

	printf("\n You are in the Flames command shell \n");
	printf("Please refer to the Manual for the list of commands\n");
	
	child_id = fork();
	if(child_id == 0){
		/***************************/
		/****   CHILD  PROCESS  ****/
		/***************************/
		usleep(200000);
		whdl = open(dev,O_WRONLY);
		if(whdl <= 0){
			printf("Error in opening device \n");
			exit(1);
		}
#ifdef HOST_POSIX
		tcgetattr(whdl,&ntty);
#else
		ioctl(whdl,TIOCGETP,&ntty);
#endif

		signal(SIGHUP, child_cleanup);
    		signal(SIGINT, ctrlc_handler);
    		signal(SIGQUIT, child_cleanup);
    		signal(SIGTERM, child_cleanup);
    		signal(SIGSTOP, child_cleanup);
    		signal(SIGTSTP, child_cleanup);
#ifdef HOST_POSIX
		otty = ntty;
		ntty.c_lflag &= ~( ECHO | ICANON );
		ntty.c_lflag |= ISIG;
        	ntty.c_iflag = 0;
    		ntty.c_cflag &= ~(CBAUD | (CSIZE | PARENB));  /* Punch hole. */
    		ntty.c_cflag |= (CS8 | FLASH_BAUD) ;
		ntty.c_cc[VMIN] = 1;      /* Wait forever for at least one char. */
		ntty.c_cc[VTIME] = 0;
		ntty.c_cc[VKILL] = _POSIX_VDISABLE;	/* no kill line */
		tcsetattr( whdl, TCSANOW, & ntty );

#else
		otty = ntty; 
		ntty.sg_flags &= ~ECHO;
		ntty.sg_flags |= CBREAK;
		ioctl(whdl,TIOCSETP,&ntty);
/***************************************************************/
		ioctl(whdl,TCGETA,(caddr_t)&tbuf);
		o_tbuf = tbuf;
        	tbuf.c_iflag = 0;
    		tbuf.c_cflag &= ((~CBAUD));
    		tbuf.c_cflag |= FLASH_BAUD;
    		tbuf.c_cflag &= ~(CSIZE | PARENB);
    		tbuf.c_cflag |= (CS8 | FLASH_BAUD) ;
		ioctl(whdl,TCSETAF,(caddr_t)&tbuf);
/****************************************************************/
#endif
		hndxhd = 0;
		hndxtl = 0;
		cur_cnt = 0;
		ctrlc_hit = 0;
		c = 0xd;
		write(whdl,&c,1);

		/*
		 *  Read from the user and send to target.  flsh() gets
		 *  a line of text.  We send out the RETURN afterwards.
		 *  FLINT_EXIT_CH must occur right after RETURN (first char).
		 */
		while(1){
			buffer[0] = (char)0;
			if((!flsh(whdl,buffer)) || (ctrlc_hit)){
				printf("Invalid Command\n");
				c = 0xd;
				write(whdl,&c,1);
				continue;
			}

			if(buffer[0] == FLINT_EXIT_CH) {
				child_cleanup();
				/*NOTREACHED*/
			}

			for(i = 0; i < strlen(buffer); i++){
				if(write(whdl,&buffer[i],1) <= 0)
					printf("Write Error\n");
				
			}
			c = 0xd;
			write(whdl,&c,1);
		}
		/*NOTREACHED*/
	}
	else {

		/***************************/
		/****  PARENT  PROCESS  ****/
		/***************************/

#ifdef HOST_POSIX
	if(tcgetattr(rhdl,&ntty) != 0) {
		perror("flint:IOCTL");
		exit(1);
	}
#else		
	if(ioctl(rhdl,TIOCGETP,&ntty) != 0){
		perror("flint:IOCTL");
		exit(1);
	}
#endif

	signal(SIGHUP,  cleanup);
    	signal(SIGINT,  ctrlc_handler);
    	signal(SIGQUIT, cleanup);
    	signal(SIGTERM, cleanup);
    	signal(SIGSTOP, cleanup);
    	signal(SIGTSTP, cleanup);
	signal(SIGCHLD, cleanup); 	/* <-- normal exit signal */

	otty = ntty; 			/* 'otty' used during cleanup() */
#ifdef HOST_POSIX
	ntty.c_lflag &= ~(ECHO | ICANON);	/* was flag "CBREAK" */
	ntty.c_lflag |= ISIG;
        ntty.c_iflag = 0;
    	ntty.c_cflag &= ~(CBAUD | (CSIZE | PARENB));   /* Punch hole. */
    	ntty.c_cflag |= (CS8 | FLASH_BAUD) ;
	ntty.c_cc[VMIN] = 1;      /* Wait forever for at least one char. */
	ntty.c_cc[VTIME] = 0;
	ntty.c_cc[VKILL] = _POSIX_VDISABLE;		/* no kill line */
#else
	ntty.sg_flags = (ntty.sg_flags & ~ECHO) | CBREAK ;
	ioctl(rhdl,TIOCSETP,&ntty);
	ioctl(rhdl,TCGETA,(caddr_t)&tbuf);
	o_tbuf = tbuf;
#endif

	/*
	 *  Read from the target and send to the user.
	 */
	while(1){

		do
			n = read(rhdl,buffer, sizeof(buffer) );
		while( n == 0 );

		if((n < 0) && (errno != EWOULDBLOCK)){
			perror("flint: Read Error");
			cleanup();
		}
		if( n < 0 )
		    continue;
		for(i=n; --i >= 0 ; ) {
		    buffer[i] &= 0x7F;
		}
		write( 2, buffer, n );	/* whosh! */
	}	/* while hell not frozen over.. */
    }

	/*NOTREACHED*/
}   /* main() */


void ctrlc_handler()
{

	ctrlc_hit = 1;
#if SW_WATCH
	printf( "SigInt" );
#endif
	if( child_id == 0 )
	    child_cleanup();
	else
	    cleanup();
	/*NOTREACHED*/
}
		

#undef signal


void cleanup()
{
#if SW_WATCH
    printf ("parent cleanup\n");
#endif
#ifdef HOST_POSIX
    tcsetattr(rhdl, TCSANOW, &otty);
#else
    ioctl(rhdl,TCSETAF,(caddr_t)&o_tbuf);
    ioctl(rhdl, TIOCSETP, &otty);
#endif
    close(rhdl);
    kill( child_id, SIGINT );		/* SIGQUIT will cause core dump! */
    wait(&status);
    exit(1);
}


void child_cleanup()
{
#if SW_WATCH
    /* WHEN USER HITS ^X, THIS NORMALLY PRINTS. */
    printf ("child cleanup\n");
#endif
#ifdef HOST_POSIX
    tcsetattr(whdl, TCSANOW, &otty);
#else
    ioctl(whdl, TIOCSETP, &otty);
    ioctl(whdl,TCSETAF,(caddr_t)&o_tbuf);
#endif
    close(whdl);
    kill( getppid(), SIGINT );		/* SIGQUIT will cause core dump! */
    exit(0);
}

