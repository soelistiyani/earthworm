/************************************************************************
                                sendpage.c
        	This is the solaris version. Alex 1/19/96
 ************************************************************************/

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <sgtty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ttold.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PAGEIT "/dev/pagerport"		/* This is port ttya		*/
#define PAGEIT_BAUD B9600

static	void	beep();			/* Defined below		*/
	u_int	cksum();		/* libq (cksum.c)		*/
	char	*emalloc();		/* libq (emalloc.c)		*/
	void	error();		/* libq (error.c)		*/
static	char	*getregion();		/* Defined below		*/
static	int	popen2();		/* Defined below		*/
	int	read_to();		/* libq (read_to.c)		*/
	void	report();		/* libq (error.c)		*/
static	void	skymsg();		/* Defined below		*/
static	void	cubicmsg();		/* Defined below		*/
static	void	sound();		/* Defined below		*/


/***********************************************************************
                            Time-Out Handler
************************************************************************/

int SendPageHung = 0;               /* Meaning not hung */
jmp_buf ToHere;                     /* Buffer for saving processor state */

void TimeOutHandler( int dummy )
{
   SendPageHung = 1;                /* Meaning: yup, we're hung */
   longjmp( ToHere, 0 );            /* Restore state to right after */
                                    /*    the setjmp call */
}

/***********************************************************************
              Send a Pager Request to PAGEIT via Serial Port
                            SendPage( buff ) 

       Will time out after four seconds if anything caused a hang.
       buff = String to output to serial port

   Returns:
        0 => All went well
       -1 => Time out
       -2 => Error while writing to port
************************************************************************/

int SendPage( char *buff )
{
   int	           fd;          /* File descriptor */
   struct sgttyb   tty;

/* Arrange for timeout alarm
   *************************/
   signal( SIGALRM, TimeOutHandler );
   alarm( (unsigned)4 );

/* Declare not hung
   ****************/
   SendPageHung = 0;

/* Take a snapshot of PC, registers, and stack as they are now
   ***********************************************************/
   setjmp( ToHere );

/* We may be here either by regular program flow, or because the
   timer has run out, the TimeOut Handler has been executed, and
   has returned us to here. And you thought goto's were bad!!!
   *************************************************************/
   if ( SendPageHung == 1 )
   {
      printf( "SendPage has timed out\n" );
      return( -1 );                     /* Meaning time-out; good luck */
   }

/* Open the PAGEIT port and set baud rate
   **************************************/
   fd = open( PAGEIT, 1 );               /* 2'nd arg = 1 for write */
   if ( fd == EOF )
   {
      perror( "SendPage: Can't open port, mode UPDATE" );
      goto fail;
   }

/* Set exclusive mode
   ******************/
   if ( gtty( fd, &tty ) != 0 )
   {
      perror( "SendPage: Can't get tty modes" );
      goto fail;
   }

   tty.sg_ispeed = tty.sg_ospeed = PAGEIT_BAUD;

   if ( stty(fd, &tty) != 0 )
   {
      perror( "SendPage: Can't set tty modes" );
      goto fail;
   }

/* Write to PAGEIT
   ***************/
   if ( write(fd, buff, strlen(buff)) == EOF )
   {
      perror( "SendPage: Can't write to serial port" );
      goto fail;
   }

   close( fd );
   alarm( (unsigned)0 );
   return( 0 );

/* Something went wrong
   ********************/
fail:
   alarm( (unsigned)0 );
   close( fd );
   return( -2 );
}
