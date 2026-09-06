/*
 *   THIS FILE IS UNDER RCS - DO NOT MODIFY UNLESS YOU HAVE
 *   CHECKED IT OUT USING THE COMMAND CHECKOUT.
 *
 *    $Id: getfile_socket.c 7606 2018-11-26 20:47:34Z alexander $
 *
 *    Revision history:
 *     $Log$
 *     Revision 2.0  2018/05/03 16:12:46  alexander
 *     Upgraded to IPv6
 *
 *     Revision 1.7  2010/04/27 18:43:41  paulf
 *     removed any stropts include lines
 *
 *     Revision 1.6  2007/03/28 17:04:51  paulf
 *     fixed for Linux
 *
 *     Revision 1.5  2007/03/28 17:03:11  paulf
 *     cleaned up OS dependencies relateing to MACOSX
 *
 *     Revision 1.4  2005/07/27 19:21:07  friberg
 *     made #endif statement into a comment for gcc compilers
 *
 *     Revision 1.3  2005/07/27 19:19:33  friberg
 *     added _LINUX ifdefs for S_un struct member
 *
 *     Revision 1.2  2002/11/03 19:03:23  lombard
 *     Added RCS header
 *
 *
 *
 */

      /*************************************************************
       *                    getfile_socket.c                       *
       *************************************************************/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WINNT
#include <windows.h>
#include <io.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(_MACOSX)
#include <sys/filio.h>
#endif 
#if defined(_LINUX)
#include <asm/ioctls.h>
#endif
#endif /* _WINNT */

#include "socket_ew.h"
#include "getfile_ew.h"


void   SocketClose( SOCKET );    /* socket_ew.c  system-dependent */
static SOCKET acceptSock;        /* Accept connections on this socket (sd) */
static SOCKET readSock;          /* Read from this socket  (ns) */

       /*********************************************************
        *                    CloseReadSock()                    *
        *       Close the socket we actually read from.         *
        *********************************************************/

void CloseReadSock( void )
{
   close( readSock );
   return;
}


           /***********************************************
            *              InitServerSocket()             *
            ***********************************************/

void InitServerSocket( void )
{
   extern char ServerIP[20];      /* IP address of system to receive msg's */
   extern int  ServerPort;        /* The well-known port number */
   struct sockaddr_in server;     /* Server socket address structure */
   const int optVal = 1;
   unsigned long address;

   if((acceptSock = setuptcpserver_ew(ServerIP, ServerPort, 5)) < 0)
   {
      logit("e", "setuptcpserver_ew() error\n");
      exit(-1);
   }

   return;
}


              /***********************************************
               *             AcceptConnection()              *
               *       Accept a TCP socket connection.       *
               *                                             *
               *  This function blocks on accept().          *
               ***********************************************/

int AcceptConnection( int *clientIndex )
{
   extern int nClient;                /* Number of trusted clients */
   extern CLIENT Client[];             /* Addresses of trusted clients */
   extern int TimeOut;             /* Send/receive timeout, in seconds */
   struct sockaddr_storage client;         /* Client socket address structure */
   int clientlen = sizeof(client);    /* Client socket structure length */
   unsigned long lOnOff=1;
   int i;
   struct sockaddr_in *ipv4Addr;
   struct sockaddr_in6 *ipv6Addr;
   int trusted = 0;                   /* 1 if client is trusted */

   readSock = accept_ew ( acceptSock, (struct sockaddr *)&client, 
						&clientlen, TimeOut );
   if ( readSock == INVALID_SOCKET )
   {
      return GETFILE_FAILURE;
   }

/* Accept data only from trusted clients
   *************************************/
   logit ( "e", "\n" );

   for ( i = 0; i < nClient; i++ )
   {
#if 0
      if(((struct sockaddr *)&client)->sa_family == AF_INET) {
         ipv4Addr = (struct sockaddr_in *)&client;
#if defined(_LINUX) || defined(_MACOSX)
         if ( ipv4Addr->sin_addr.s_addr == Client[i].ipint )
#else
         if ( ipv4Addr->sin_addr.S_un.S_addr == Client[i].ipint )
#endif
         {
            trusted = 1;
            break;
         }
      } else if (((struct sockaddr *)&client)->sa_family == AF_INET6) {
         ipv6Addr = (struct sockaddr_in6)&client;
         //fortunately it does not appear that Windows differ from Posix for IPv6 structs:
         if (memcmp(&(ipv6Addr->sin6_addr), &(Client[i].ipint), sizeof(struct in6_addr)) == 0)
         {
            trusted = 1;
            break;
         }
      }
#endif
   }
 
   if ( !trusted )
   {
      //logit ( "e", "Rejected connection attempt by %s\n", inet_ntoa(client.sin_addr) );
      SocketClose ( readSock );
      return GETFILE_FAILURE;
   }

   *clientIndex = i;
   //logit ( "et", "Accepted connection from %s\n", inet_ntoa(client.sin_addr) );

/* Set socket to non-blocking mode
   *******************************/
   if ( ioctlsocket( readSock, FIONBIO, &lOnOff ) == SOCKET_ERROR )
   {
      logit ( "e", "ioctlsocket() FIONBIO error: %s\n", strerror(errno) );
      SocketClose( readSock );
      return GETFILE_FAILURE;
   }
   return GETFILE_SUCCESS;
}


  /*********************************************************************
   *                              Recv_all()                           *
   *                                                                   *
   *  Reads nbyte characters from socket sock into buf.                *
   *  The function will time out if the data can't be read within      *
   *  TimeOut seconds.                                                 *
   *                                                                   *
   *  Returns 0 if all ok.                                             *
   *  Returns -1 if an error or timeout occurred.                      *
   *********************************************************************/

int Recv_all( SOCKET sock, char buf[], int nbytes )
{
   extern int TimeOut;             /* Send/receive timeout, in seconds */
   int    nBytesToRead = nbytes;
   int    nread        = 0;

   while ( nBytesToRead > 0 )
   {
      int    selrc;
      fd_set readfds;
      struct timeval selectTimeout;

/* See if the socket is readable.  If the socket is not
   readeable within TimeOut seconds, return an error.
   ****************************************************/
      selectTimeout.tv_sec  = TimeOut;
      selectTimeout.tv_usec = 0;
      FD_ZERO( &readfds );
      FD_SET( sock, &readfds );
      selrc = select( sock+1, &readfds, 0, 0, &selectTimeout );
      if ( selrc == SOCKET_ERROR )
      {
         logit ( "e", "select() error: %s", strerror(errno) );
         return -1;
      }
      if ( selrc == 0 )
      {
         logit ( "e", "select() timed out\n" );
         return -1;
      }
      if ( FD_ISSET( sock, &readfds ) )    /* A readable event occurred */
      {
         int rc;
         int sockerr;
#ifdef _WINNT
         int optlen = sizeof(int);
#else
         socklen_t optlen = sizeof(int);
#endif

/* Check for socket errors using getsockopt()
   ******************************************/
         if ( getsockopt( sock, SOL_SOCKET, SO_ERROR, 
				(char *)&sockerr, &optlen ) == SOCKET_ERROR )
         {
            logit ( "e", "getsockopt() error: %s", strerror(errno) );
            return -1;
         }
         if ( sockerr != 0 )
         {
            logit ( "e", "Error detected by getsockopt(): %s\n",
                     strerror(sockerr) );
            return -1;
         }

/* Get the data.  It's unlikely that recv() would block,
   since we know that a readable event occurred.
   ****************************************************/
         rc = recv_ew ( sock, &buf[nread], nBytesToRead, 0, TimeOut);
         if ( rc == SOCKET_ERROR )
         {
            logit ( "e", "recv() error: %s", strerror(errno) );
            return -1;
         }
         if ( rc == 0 )
         {
            logit ( "e", "Error. recv()==0\n" );
            return -1;
         }
         nread += rc;
         nBytesToRead -= rc;
      }
   }
   return 0;
}


  /*********************************************************************
   *                        GetBlockFromSocket()                       *
   *  This function reads a block of bytes from the sendfile program.  *
   *  Blocks can be from 1 to 999999 bytes long.                       *
   *  If a zero length block is received, it means the transmission    *
   *  is over.                                                         *
   *********************************************************************/

int GetBlockFromSocket( char buf[], int *nbytes )
{
   char blockSizeAsc[7];
   unsigned int  blockSize;

/* Read the block size from the socket.  The size is a six ASCII
   characters long, right justified and padded with blanks.
   *************************************************************/
   if ( Recv_all( readSock, blockSizeAsc, 6 ) == -1)
   {
      logit ( "e", "Recv_all() error.\n" );
      return GETFILE_FAILURE;
   }

   blockSizeAsc[6] = '\0';            /* Null terminate */

   if ( sscanf( blockSizeAsc, "%u", &blockSize ) < 1 )
   {
      logit ( "e", "Error decoding block size.\n" );
      return GETFILE_FAILURE;
   }

   if ( blockSize > BUFLEN )
   {
      logit ( "e", "Error. Block is too large for buffer.\n" );
      return GETFILE_FAILURE;
   }

/* The sendfile program sends a blocksize of zero
   when it has finished sending the file.
   **********************************************/
   if ( blockSize == 0 ) return GETFILE_DONE;

/* Read the block from the socket
   ******************************/
   if ( Recv_all( readSock, buf, blockSize ) == -1)
   {
      logit ( "e", "Recv_all() error.\n" );
      return GETFILE_FAILURE;
   }
   *nbytes = blockSize;        /* Return to calling program */
   return GETFILE_SUCCESS;
}
