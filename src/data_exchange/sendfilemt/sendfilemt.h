/*   sendfilemt.h    */

#ifndef SENDFILEMT_H
#define SENDFILEMT_H

#include <stdio.h>

#include "earthworm.h"

typedef struct
{
   char     	tag[13];           /* Tag to appear in log file */
   char     	ip[20];            /* IP address of system to receive msg's */
   int      	port;              /* Well-known port number */
   char     	outDir[80];        /* Directory containing files to be sent */
   ew_thread_t 	threadId;
   int      	threadAlive;       /* 1 if alive; 0 if dead */
} SERVER;                      /* One instance of SERVER per thread */

#define SENDFILE_VERSION "1.0"
#define SENDFILE_NOFILE  -2
#define SENDFILE_FAILURE -1
#define SENDFILE_SUCCESS  0
#define BUFLEN         4096    /* Write this many bytes at a time to socket */
#define NOTESIZE        120
#define THREAD_STACK   8192    /* How big is our thread stack */
#define THREAD_DEAD       0
#define THREAD_ALIVE      1
#define TRUE              1
#define FALSE             0
#define ERR_THREAD_DEAD   0    /* Error logged and reported to statmgr */

thr_ret ThreadFunction( void * );
int  ReportStatus( unsigned char, short, char * );
void GetConfig( char * );
void LogConfig( void );
int  ConnectToGetfile( char [], int, int * );
int  SendBlockToSocket( int, char *, int );
int  GetAckFromSocket( int );
void CloseSocketConnection( int );
FILE *fopen_excl( const char *, const char * );
int  ThreadAlive( ew_thread_t );

#endif
