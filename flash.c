/*  flash.c   Main for flash - FLAmes SHell. 		Sudhir Feb 1995 */
/* $Header: /export/home/aleks/Projects/Intel-159/Samples/00-Tools/flash/source/RCS/flash.c,v 1.3 2001/04/23 02:41:13 aleks Exp $ */

/*  This program is the main driver for the flames shell.  Depending on
 *  command line options, it either directly invokes a sub-program to
 *  get some work done, or calls a read-parse-execute loop accepting
 *  user commands.
 *
 *  The -v option may be added to all other options (or used just by
 *  itself) to print more details (verbose).  The -fullname option is
 *  meant for emacs' GUD (GNU Universal Debugger) mode.  If the user
 *  uses "flash" for the debugger, Emacs will insert -fullname as the
 *  first argument ahead of all other args (like the filename to debug).
 *
 *  The -c option will cause flash to print out a configuration file to
 *  stdout with the default (compiled in) settings.  
 *
 *  This program was created as part of a master's project of
 *  Sudhir Charuats, Sacramento, California, February 1995.
 */

/*
 *  These modifcations were made by Brian Witt, Fall 1997.
 *
 *  There are two ways to runs the debugger.  The first is for direct
 *  user interaction with -b option.  The user must first download the
 *  image to the target.  The other is when using Emacs, which passes
 *  us argv[1] == "-fullname".  For Emacs, no other options are needed,
 *  and all others ignored except for the last argument which must be
 *  the image name.  Invoke "gdb", set "(like this):" to be "flash os.dli".
 */

#include "flash.h"

/*  Most POSIX systems have <utmpx.h>.  However, HPUX9 on hp800 does not
 *  have this include file... sigh
 */
#ifdef HOST_POSIX
# define  USE_UTMPX
#endif
#ifdef hpux
# undef USE_UTMPX
#endif

#ifdef USE_UTMPX
# include <utmpx.h>
#else
# include <utmp.h>
#endif


/* char IDENT_STRING[] = { "Flames Shell (FlaSh)  Ver 1.0a (2/22/95/SC)" }; */
/***  IDENT_STRING[] = { "Flames Shell (FlaSh)  Ver %I% (%G%,SC/BJW)"} ***/
char  IDENT_STRING[] = { "Flames Shell (FlaSh)  $Revision: 1.3 $ (SC/BJW)" };


/*  Forward declarations. */
void init_cfg();
void config_defaults();
int parse_n_upd();
int check_cfg();
void PRINT_CFG();
void OK_to_be_here();
int 	proc_download();
int 	proc_debug();

int	generate_config_file_flag;
int 	verbose_flag;
int 	download_only_flag;
int 	debugger_only_flag;
int 	emacs_debugger_flag;
int	allow_nonconsole_user_flag = TRUE;	/* XXX - not an issue */

char    help_text[] = {
	"usage: flash { [-c] | [-v] [-d] [DLI_fname] }\n"
	"       flash { -g | -fullname } DLI_fname\n"
	"    -allow = don't check user owns console.\n"
	"    -c  = outputs config file.\n"
	"    -d  = only do a download.\n"
	"    -g  = run debugger on DLI.\n"
	"    -fullname = run from within Emacs.\n"
	"    -v  = prints commands before executing them.\n" };


/*-------------------------------------------------------------------*/


void
usage(rc)
    int  rc;
{
    printf( help_text );
    exit(rc);
    /*NOTREACHED*/
}



int
main( int argc, char *argv[] )
{
    char line[512];
    char *string;
    FILE *fp;
    CONFIG	cfg;
    int 	j;
    FILE	*diag = stdout;

    init_cfg(&cfg);

    /**  Parse cmd line arguments  **/
    for( j=1 ; j < argc ; ++j )
    {
	if( argv[j][0] == '-' )
	{
	    switch(argv[j][1]){
		/*  -allow is a silent option. Don't complain
		 *  if not used correctly.
		 */
	    case 'a' :
	      if( strcmp( argv[j], "-allow" ) )
	      {
		  ;
	      } else {
		  allow_nonconsole_user_flag = 1;
	      }
	      break;

	    case 'g' :
	      debugger_only_flag = 1;
	      break;

	    case 'f' :
	      if( strcmp( argv[j], "-fullname" ) )
	      {
		  usage(1);
	      }
	      emacs_debugger_flag = 1;
	      strncpy( cfg.download_file, argv[argc-1],
		       sizeof(cfg.download_file)-2 );
	      goto DONE_PARSING;

	    case 'c' :
	      generate_config_file_flag = 1;
	      diag = stderr;
	      break;

	    case 'd' :
	      download_only_flag = 1;
	      break;

	    case 'v' :
	      verbose_flag = 1;
	      fprintf(stderr, "Flash: %s\n", IDENT_STRING);
	      break;

	    case 'h' :
	      usage(1);
	      break;

	    default :
	      printf("ERROR: unknown option \"%s\", try -h for help.\n",
		     argv[j]);
	      exit(2);
 
	    }
	} else
	{
	    if( cfg.download_file[0] != 0 )
	    {
		fprintf(diag,"ERROR: Too many `DLI_fname's specified\n");
		exit(1);
	    }
	    strncpy( cfg.download_file, argv[j], sizeof(cfg.download_file)-2 );
	}
    }

  DONE_PARSING :
    if( verbose_flag )
    {
#ifdef HOST_POSIX
	printf("Compiled " __DATE__ ". Host is POSIX\n");
#else
	printf("Compiled " __DATE__ ". Host NOT POSIX\n");
#endif
    }
    /*  When generating the config file, don't allow other options: */
    if( generate_config_file_flag &&
	(download_only_flag || debugger_only_flag || emacs_debugger_flag) )
    {
	fprintf(diag, "ERROR: conflicting options used with -c\n");
	exit(2);
    }

    /*  Allows config file to be generated on any pty  */
    if( ! generate_config_file_flag ) 
    {
	/*  If user isn't forcing themselves, then must check.. */
	if( ! allow_nonconsole_user_flag ) {
	    OK_to_be_here(diag);
	}
    }

    string = cfg.download_file;
    if( !generate_config_file_flag && string[0] != 0 ){
		/* Check if the file is present, there could be a typO
			u sea */
		if(access(string,F_OK) < 0){
			fprintf(diag,"\n%s :: File not present \n",string);
			exit(1);
		}
    }

    fp = (FILE *) fopen(CONFIG_FILE,"r");
    if(fp == (FILE *) NULL){
		fprintf(diag,"unable to open config file, using defaults....\n");
		config_defaults(&cfg);	
    }else {
		while(fgets(line,sizeof(line)-2,fp) != NULL){
			if(strlen(line) <= 1)	/* ignore NL only lines. */
				continue;
			/* Fgets puts a newline at the end zap it ..*/
			line[strlen(line)-1] = (char )0;
			if(!parse_n_upd(line,&cfg)){
				fclose(fp);
				exit(1);
			}
		}
		fclose(fp);
		/* Check if any of the options are omitted */
		if(!check_cfg(&cfg))
			exit(1);
    }

    if( generate_config_file_flag )
    {
	/*  After we've obtained our configuration params, allow the user
	 *  to see them.  If there was a config file, it prints those.
	 */
        PRINT_CFG(&cfg);
    } else
    if( debugger_only_flag || emacs_debugger_flag )
    {
        if( cfg.download_file[0] == 0 )
	{
	    fprintf(diag,"ERROR: -b requires DLI fname.\n");
	    exit(2);
	}

	strcpy( cmd_words[1], cfg.download_file );
	if( emacs_debugger_flag && proc_download(&cfg) != 1 )
	{
	    fprintf(diag, "ERROR: Couldn't download image.\n");
	    exit(2);
	}
        proc_debug(&cfg);
    } else
    if( download_only_flag )
    {
	/*  Just wants to download.  If the DLI filename is missing, they'll
	 *  get an error message.
	 */
	strcpy( cmd_words[1], cfg.download_file );
        proc_download(&cfg);
    } else {
        printf("\n>>>>>Welcome to the %s<<<<<<\n", IDENT_STRING);
        printf("Type ? for help\n");

	cmdinp(&cfg);
    }

    return 0;
}	/* main() */

 
/*-------------------------------------------------------------------*/
 


void init_cfg(cfg)
CONFIG *cfg;
{

	cfg->port[0] = (char )0;
	cfg->dlpath[0] = (char )0;
	cfg->flpath[0] = (char )0;
	cfg->gdbpath[0] = (char )0;
	cfg->pregdbpath[0] = (char )0;
	cfg->download_file[0] = (char )0;
}

void config_defaults(cfg)
CONFIG *cfg;
{
	sprintf(cfg->port,"%s",TTYPORT);
	sprintf(cfg->dlpath,"%s",DLPATH);	
	sprintf(cfg->flpath,"%s",FLPATH);
	sprintf(cfg->gdbpath,"%s",GDBPATH);
	sprintf(cfg->pregdbpath,"%s",FLASHSUPPATH);
}


/*   parse_n_upd()
 *	Read a line from the config file.  If there are problems, return 0.
 *	Otherwise try and parse, save into `cfg' and return 1.  Comments
 *	return 1.
 */
int parse_n_upd(line,cfg)
char *line;
CONFIG *cfg;
{
char *ptr;
char *from_here;
char *cpy;

    ptr = line;
    if( verbose_flag )
	fprintf(stderr,"#Current Line \"%s\"\n",line);

    if(*line == '#')
    {
	    /* This is a comment so ignore it */
	    return 1;
    }

    while((*ptr == ' ') || (*ptr == '\t'))
	    ptr++;
    from_here = ptr;
    while((*ptr != ' ') && (*ptr != '\t')){
	    if(*ptr)
		    ptr++;
	    else {
		    if(strlen(from_here) > 0){
			    printf("Config Error : [Line] %s\n",from_here);
			    return 0;
		    }
		    else 
			    /*skip line;*/
			    return 1;
	    }
    }
    if(strlen(from_here) == 0)
	    return 1;			/* Empty line */

    *ptr = (char )0;
    if(!strcmp(from_here,"TTYPORT"))
	    cpy = cfg->port;
    else if(!strcmp(from_here,"DLPATH"))
	    cpy = cfg->dlpath;
    else if(!strcmp(from_here,"FLPATH"))
	    cpy = cfg->flpath;
    else if(!strcmp(from_here,"GDBPATH"))
	    cpy = cfg->gdbpath;
    else if(!strcmp(from_here,"FLASHSUPPATH"))
	    cpy = cfg->pregdbpath;
    else {
	    printf("Config Error : Unknown variable : %s\n",from_here);
	    return 0;
     }
    /* Skip the just inserted NULL char */
    ++ptr;
    while((*ptr != '\"')){ 
	    if(*ptr)
		    ptr++;
	    else {
		    printf("Config Error : Ill-formed value : %s\n",line);
		    return 0;
	    }
    }
    /* skip over the "  */
    ++ptr;
    from_here = ptr;		
    while((*ptr != '\"')){
	    if(*ptr)
		    ptr++;
	    else{
		    printf("Config Error : [Line] %s\n",line);
		    return 0;
	    }
    }

    *ptr = (char )0; /* Now the terminating " is eliminated */
    strcpy(cpy,from_here);
    return 1;
}   /* parse_n_upd() */


int check_cfg(cfg)
CONFIG *cfg;
{

	if(strlen(cfg->port) == 0){
		printf("TTYPORT not defined in Flash.cfg\n");
		return 0;
	}
	if(strlen( cfg->dlpath) == 0){
		printf("DLPATH not defined in Flash.cfg\n");
		return 0;
	}
	if(strlen(cfg->flpath) == 0){
		printf("FLPATH not defined in Flash.cfg\n");
		return 0;
	}
	if(strlen(cfg->gdbpath ) == 0){
		printf("GDBPATH path not defined in Flash.cfg\n");
		return 0;
	}
	if(strlen(cfg->pregdbpath)== 0){
		printf("FLASHSUPPATH not defined in Flash.cfg\n");
		return 0;
	}
	return 1;
}
		

void PRINT_CFG(cfg)
CONFIG *cfg;
{
    printf("## ConfigFile: \"%s\"\n", CONFIG_FILE);
    printf("TTYPORT    \"%s\" \n",cfg->port);
    printf("DLPATH    \"%s\" \n",cfg->dlpath);	
    printf("FLPATH   \"%s\" \n",cfg->flpath);
    printf("GDBPATH   \"%s\"\n",cfg->gdbpath);
    printf("FLASHSUPPATH  \"%s\"\n",cfg->pregdbpath);
    printf("# Download file : %s\n",cfg->download_file);
    printf("# model GDB init: %s\n", MODEL_GDBINIT_FILE);
}


		
/*-------------------------------------------------------------------*/


/*  Check if user logged into console.  If logged through, then tell them
 *	off, print a message and then exit(1).  If OK, then just returns.
 *	Overridden by `allow_nonconsole_user_flag'.
 */
#define DEC_SPL_STRING	":0.0"

void
OK_to_be_here(diag)
FILE  *diag;
{		
#ifdef USE_UTMPX
	struct utmpx   *pxusr;
	pid_t   me = getpid();

	setutxent();
	while( (pxusr = getutxent()) != NULL ) {
	    if( pxusr->ut_pid == me &&
		strcmp(pxusr->ut_host, DEC_SPL_STRING) ) {
	     // fprintf(diag, "\nYou are logged in from %.*s\n",pxusr->ut_syslen, pxusr->ut_host);
	      fprintf(diag, "You have to be at the console to use FLASH !\n");
	      exit(1);
	    }
	}
	endutxent();
#else
	struct utmp usr;
	FILE *ufp;
	char  *p, *t;

	/* If argument present treat it as the name of the executable */
	/* Initialize and read the config file */	
	if (!(ufp = fopen(UTMP_FILE, "r"))){
		perror("\nFlash : error reading " UTMP_FILE);
		exit(1);
	}
	if (NULL != (p = ttyname(0))) {
            /* strip any directory component */
	    if (NULL != (t = strrchr(p, '/')))
		p = t + 1;
	    while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
		if (usr.ut_name && !strcmp(usr.ut_line, p)){
		    if(strcmp(usr.ut_host,DEC_SPL_STRING)){
		        fprintf(diag, "\nYou are logged in from %.*s\n",
					16,usr.ut_host);
			fprintf(diag, "You have to be at the console to use FLASH !\n");
			exit(1);
		    }
		}
	}
#endif
			
}   /* OK_to_be_here() */

