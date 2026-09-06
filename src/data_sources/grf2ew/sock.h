/* $Id: sock.h 7198 2018-03-20 23:34:38Z baker $ */
/*-----------------------------------------------------------------------
    Copyright (c) 2000-2007 - DAQ Systems, LLC. - All rights reserved.
-------------------------------------------------------------------------

    Make WinSock2 look something like Berkeley Sockets and visa versa.

-----------------------------------------------------------------------*/
#if !defined _SOCK_H_INCLUDED_
#define _SOCK_H_INCLUDED_

#include "platform.h"
#include "earthworm.h"

#if defined (LINUX) || defined (SOLARIS) || defined (MACOSX)
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <sys/time.h>
typedef struct sockaddr_in ENDPOINT;
#ifndef SOCKET
typedef int SOCKET;
#endif
#	define INVALID_SOCKET	-1
#	define SOCKET_ERROR 	INVALID_SOCKET
#	define SOCK_ERRNO() 		(errno)
#	define SOCK_ERRSTRING(error)	(strerror(error))
#	define SOCK_CLOSE(socket)	(close(socket));
#ifndef INADDR_NONE
#       define INADDR_NONE	-1
#endif
#elif defined (WIN32)
typedef struct sockaddr_in ENDPOINT;
#	define SOCK_ERRNO() 		(WSAGetLastError())
#	define SOCK_ERRSTRING(error)    (WinSockErrorString(error))
#	define SOCK_CLOSE(socket)	(closesocket(socket));
const CHAR *WinSockErrorString(INT32 error);
#else
#   error Sockets not supported on current platform
#endif

/* Undefined address and port number */
#define VOID_ADDRESS	0
#define VOID_PORT	0
#define MAX_HOST_LEN	127

typedef struct TAG_HOST_LIST {
	struct TAG_HOST_LIST *next;
	UINT32 address;
	UINT32 netmask;
} HOST_LIST;

BOOL SocketsInit(void);
CHAR *FormatEndpoint(ENDPOINT * addr, CHAR *string);
BOOL ParseEndpoint(ENDPOINT * addr, CHAR *string, UINT16 port);

#if defined (WIN32)
CHAR *inet_ntop(int family, const void *address, char *string, size_t length);
#endif

#endif /* !defined _SOCK_H_INCLUDED_ */
