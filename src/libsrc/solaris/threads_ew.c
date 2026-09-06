              /***********************************************
               *                threads_ew.c                 *
               *              Solaris version                *
               *                                             *
               *  This file contains functions StartThread,  *
               *  WaitThread, and KillThread                 *
               ***********************************************/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <thread.h>

#include "earthworm_simple_funcs.h"	/* logit() */
#include "earthworm_complex_funcs.h"	/* Earthworm threads API */

void SignalHandle( int );

   /********************************************************************
    *                           StartThread                            *
    *                                                                  *
    * Arguments:                                                       *
    *     fun:        Name of thread function. Must take (void *)      *
    *                 as an argument and return void                   *
    *     stack_size: Stack size of new thread in bytes                *
    *                 In OS2, if zero the stack size is set to 8192    *
    *                 In SOLARIS, this argument is ignored             *
    *     thread_id:  Opaque thread ID returned to calling program.    *
    *                                                                  *
    *  The function <fun> is not passed any arguments.                 *
    *                                                                  *
    * Returns:                                                         *
    *    -1 if error                                                   *
    *     0 if ok                                                      *
    ********************************************************************/

int StartThread( thr_ret (*fun)( void * ),
		 unsigned int stack_size,
		 ew_thread_t *thread_id ) {

   int rc;                       /* Function return code */
   thread_t tid;                 /* SendMsg thread id */
   size_t stackSize = 0;

/* Set up a signal-handling function to be inherited by threads
   ************************************************************/
   sigset( SIGUSR1, &SignalHandle ); 
 
/* Start the thread
   ****************/
   /* Note: THR_DETACHED is required for thr_exit to work. That is,
      a detached thread can truly kill itself without lingering in
      some afterlife, waiting for some other thread to pick up it's exit
      status before it can truly cease to be...*/
   rc = thr_create( NULL, stackSize, fun, NULL,
                    THR_DETACHED|THR_NEW_LWP, &tid );
   if ( rc != 0 )
      return -1;

   *thread_id = (ew_thread_t) tid;

   if ( *thread_id != tid )		/* Critical on 64-bit systems */
   logit( "ep", "StartThread: "
                "Warning: thread ID overflows thread_id data type.\n");

   return 0;
}

   /********************************************************************
    *                        StartThreadWithArg                        *
    *                                                                  *
    * Arguments:                                                       *
    *     fun:        Name of thread function. Must take (void *)      *
    *                 as an argument and return void.                  *
    *	  arg:	      an unsigned long (void*), passed to the thread   *
    *     stack_size: Stack size of new thread in bytes                *
    *                 In OS2, if zero the stack size is set to 8192    *
    *                 In SOLARIS, this argument is ignored             *
    *     thread_id:  Opaque thread ID returned to calling program.    *
    *                                                                  *
    * Returns:                                                         *
    *    -1 if error                                                   *
    *     0 if ok                                                      *
    ********************************************************************/

int StartThreadWithArg( thr_ret (*fun)( void * ),
			void *arg,
			unsigned int stack_size,
			ew_thread_t *thread_id ) {

   int rc;                       /* Function return code */
   thread_t tid;                 /* SendMsg thread id */
   size_t stackSize = 0;

/* Set up a signal-handling function to be inherited by threads
   ************************************************************/
   sigset( SIGUSR1, &SignalHandle ); 
 
/* Start the thread
   ****************/
   /* Note: THR_DETACHED is requrired for thr_exit to work. That is,
      a detached thread can truly kill itsself without lingering in
      some afterlife, waiting for some other thread to pick up it's exit
      status before it can truly cease to be...*/
   rc = thr_create( NULL, stackSize, fun, (void *) arg,
                    THR_DETACHED|THR_NEW_LWP, &tid );
   if ( rc != 0 )
      return -1;

   *thread_id = (ew_thread_t) tid;

   if ( *thread_id != tid )		/* Critical on 64-bit systems */
   logit( "ep", "StartThreadWithArg: "
                "Warning: thread ID overflows thread_id data type.\n");

   return 0;
}

  /*************************************************************
   *                          WaitThread                       *
   *                    Wait for thread to die.                *
   *                                                           *
   *             This is a dummy function in Solaris.          *
   *                                                           *
   * Argument:                                                 *
   *    thread_id = Pointer to thread id                       *
   *************************************************************/

int WaitThread( ew_thread_t *thread_id ) {
   return 0;
}

   /************************************************************
    *                        KillThread                        *
    *                Force a thread to exit now.               *
    *                                                          *
    * Argument:                                                *
    *    thread_id = Opaque thread id to kill                  *
    *                                                          *
    * Returns:                                                 *
    *     0 if ok                                              *
    *     non-zero value indicates an error                    *
    ************************************************************/

int KillThread( ew_thread_t tid ) {
   return( thr_kill( (thread_t) tid, SIGUSR1 ) );
}

   /***************************************************************
    *                         KillSelfThread                      *
    *     For a thread exit without affecting other threads       *
    *                                                             *
    *      Thread must have been created with the THR_DETACHED    *
    *      bit set; else a zombie lingers, waiting for someone    * 
    *      to pick up it's exit status                            *
    ***************************************************************/

int KillSelfThread( void ) {
   thr_exit( NULL );
   return 0;                     /* well, not really */
}

   /*************************************************************
    *                        SignalHandle                       *
    *         Decide what to do when a signal is caught         *  
    *  Added for use with the KillThread function so that       *
    *  killed threads will exit gracefully                      *
    *************************************************************/

void SignalHandle( int sig ) {

   void *status;
 
   switch (sig) {

   case SIGUSR1:
        /*printf( "thread:%d caught SIGUSR1; calling thr_exit()\n",
                 (int) thr_self() );*/ /*DEBUG*/
        thr_exit( status );
   }
}
