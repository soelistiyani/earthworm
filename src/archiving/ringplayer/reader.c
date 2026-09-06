	
/********************************************************************
 *                    reader - a debugging tool                     *
 *                                                                  *
 ********************************************************************/

/*
 *
 * Copyright (c) 2017 California Institute of Technology.
 *
 * All rights reserved.
 *
 * This program is distributed WITHOUT ANY WARRANTY whatsoever.
 *
 * Do not redistribute this program without written permission.
 *
 */


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <earthworm.h>
#include <transport.h>

// #define MAX_SIZE MAX_BYTES_PER_EQ   // Largest message size in characters
#define MAX_SIZE 640000  // Largest message size in characters


// TODO: Augment this list or offload to config file
#define TYPE_WILDCARD	0
#define TYPE_ERROR		2
#define TYPE_HEARTBEAT	3

#define TYPE_RESTART		107
#define TYPE_REQSTATUS	108
#define TYPE_STATUS		109


int main( int argc, char *argv[] )
{
	int	fd;
	char	fileName[1024];
	char	buffer[MAX_SIZE];
	int	nread = 0;
	
	// Preferences - TODO: Offload to config file?
	int	printHeader				= 1;
	int	ignoreHeartbeats		= 1;
	int	ignoreErrorMsg			= 1;
	int	ignoreWildcards		= 0;	
	int	ignoreRestarts			= 0;	
	int	ignoreReqstatus		= 0;	
	int	ignoreStatusMsg		= 0;	

	typedef struct {
		int64_t sec;
		int64_t usec;
		int64_t length;
		unsigned char type;
		unsigned char mod;
		unsigned char instid;
	} Header;

	Header header;

   if((argc != 2))
   {
		printf("Error: argc != 2\n");
      return -1;
   }
   
   strcpy(fileName, argv[1]);

	fd = open(fileName, O_RDONLY);
	lseek(fd, 0, SEEK_SET);

	/*
	read(fd, buffer, sizeof(int64_t));
	printf("sec:%d ",(int) buffer);
	
	read(fd, buffer, sizeof(int64_t));
	printf("usec:%d ", (int) buffer);
	
	read(fd, buffer, sizeof(int64_t));
	printf("length:%d ", (int) buffer);
	
	read(fd, buffer, sizeof(char));
	printf("type:%c ", (char) buffer);
	
	read(fd, buffer, sizeof(char));
	printf("mod:%c ", (char) buffer);
	
	read(fd, buffer, sizeof(char));
	printf("instid:%c ", (char) buffer);
	
	read(fd, buffer, 1024);
	printf("message:%s\n", (char *) buffer);
	*/

	while(1)
	{
		read(fd, &header, sizeof(header)); 
		
		nread = read(fd, buffer, header.length);
		
		buffer[header.length] = '\0';

		if(nread <= 0)
			break;
		
		if(((int) header.type == TYPE_WILDCARD) && (ignoreWildcards == 1))
		{
			continue;
		}
		if(((int) header.type == TYPE_ERROR) && (ignoreErrorMsg == 1))
		{
			continue;
		}
		if(((int) header.type == TYPE_HEARTBEAT) && (ignoreHeartbeats == 1))
		{
			continue;
		}
		if(((int) header.type == TYPE_RESTART) && (ignoreRestarts == 1))
		{
			continue;
		}
		if(((int) header.type == TYPE_REQSTATUS) && (ignoreReqstatus == 1))
		{
			continue;
		}
		if(((int) header.type == TYPE_STATUS) && (ignoreStatusMsg == 1))
		{
			continue;
		}

		if(printHeader)
		{
			printf("[%d]", (int) header.sec);
			printf("[%d]", (int) header.usec);
			printf("[%d]", (int) header.length);
			printf("[%d]", (int) header.type);
			printf("[%d]", (int) header.mod);
			printf("[%d]", (int) header.instid);
		}
		printf("%s", (char *) buffer);
		
		// if(((int) header.type == 19) || ((int) header.type == 20))
		{
		 	printf("\n");
		}
	}

	close(fd);

	return 0;
}

