              /********************************************
              *               threads_ew.c                *
              *            Windows NT version             *
              * This file contains functions StartThread, *
              * WaitThread, and KillThread                *
              *********************************************/

#include <windows.h>
#include <process.h>

#include "earthworm_simple_funcs.h"	/* logit() */
#include "earthworm_complex_funcs.h"	/* Earthworm threads API */

   /********************************************************************
    *                           StartThread                            *
    *                                                                  *
    *  Arguments:                                                      *
    *     fun:        Name of thread function. Must take (void *)      *
    *                 as an argument and return void                   *
    *     stack_size: Stack size of new thread in bytes                *
    *                 If 0, stack size is set to 8192.                 *
    *                 In OS2, 4096 or 8192 is recommended.             *
    *                 In SOLARIS, this argument is ignored             *
    *                 In Windows NT, if stack_size=0, use the stack    *
    *                 size of the calling thread.                      *
    *     thread_id:  Opaque thread ID returned to calling program.    *
    *                                                                  *
    *  The function <fun> is not passed any arguments.                 *
    *                                                                  *
    *  Returns:                                                        *
    *    -1 if error                                                   *
    *     0 if ok                                                      *
    ********************************************************************/

int StartThread( thr_ret (*fun)( void * ),
		 unsigned int stack_size,
		 ew_thread_t *thread_id ) {

   uintptr_t tid;

   tid = _beginthread( fun, stack_size, NULL );

   if ( tid == (uintptr_t) -1 )             /* Couldn't create thread */
      return -1;

   *thread_id = (ew_thread_t) tid;

   if ( *thread_id != tid )		/* Critical on 64-bit systems */
   logit( "ep", "StartThread: "
                "Warning: thread ID overflows thread_id data type.\n");

   return 0;
}

   /********************************************************************
    *                       StartThreadWithArg                         *
    *                                                                  *
    *  Arguments:                                                      *
    *     fun:        Name of thread function. Must take (void *)      *
    *                 as an argument and return void                   *
    *     arg:        an unsigned long (void*) passed to the thread.   *
    *     stack_size: Stack size of new thread in bytes                *
    *                 If 0, stack size is set to 8192.                 *
    *                 In OS2, 4096 or 8192 is recommended.             *
    *                 In SOLARIS, this argument is ignored             *
    *                 In Windows NT, if stack_size=0, use the stack    *
    *                 size of the calling thread.                      *
    *     thread_id:  Opaque thread ID returned to calling program.    *
    *                                                                  *
    *  Returns:                                                        *
    *    -1 if error                                                   *
    *     0 if ok                                                      *
    ********************************************************************/

int StartThreadWithArg( thr_ret (*fun)( void * ),
			void *arg,
			unsigned int stack_size, 
            ew_thread_t *thread_id ) {

   uintptr_t tid;

   tid = _beginthread( fun, stack_size, arg );

   if ( tid == (uintptr_t) -1 )             /* Couldn't create thread */
      return -1;

   *thread_id = (ew_thread_t) tid;

   if ( *thread_id != tid )		/* Critical on 64-bit systems */
   logit( "ep", "StartThreadWithArg: "
                "Warning: thread ID overflows thread_id data type.\n");

   return 0;
}

   /*************************************************************
    *                        KillSelfThread                     *
    *             For a thread exit without affecting           *
    *                        other threads                      *
    *************************************************************/

int KillSelfThread( void ) {
    _endthread();
    return 0;
}

  /*************************************************************
   *                          WaitThread                       *
   *                   Wait for thread to die.                 *
   *                                                           *
   *  This function is used in coaxtoring.c                    *
   *                                                           *
   *    thread_id = Pointer to opaque thread id                *
   *                                                           *
   *  Returns:                                                 *
   *    -1 if error                                            *
   *     0 if ok                                               *
   *************************************************************/

int WaitThread( ew_thread_t *thread_id ) {
   if ( WaitForSingleObject( (HANDLE) (*thread_id), INFINITE )
                                              == WAIT_FAILED )
      return -1;
   return 0;
}

   /************************************************************
    *                         KillThread                       *
    *                Force a thread to exit now!               *
    *                                                          *
    *  Windows NT documentation gives a strong warning against *
    *  using TerminateThread(), since no stack cleanup, etc,   *
    *  is done.                                                *
    *                                                          *
    * Argument:                                                *
    *    thread_id = Opaque thread id to kill                  *
    *                                                          *
    * Returns:                                                 *
    *     0 if ok                                              *
    *     non-zero value indicates an error                    *
    ************************************************************/

int KillThread( ew_thread_t thread_id ) {

    const DWORD exit_code = 0;

    if ( TerminateThread( (HANDLE) thread_id, exit_code ) == 0 )
       return -1;

    return 0;
}
