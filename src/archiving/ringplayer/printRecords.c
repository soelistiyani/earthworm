	
/********************************************************************
 *                    printRecords - a debugging tool               *
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
#include <time_ew.h>
#include <trace_buf.h>
#include <earthworm.h>
#include <transport.h>


#define MAX_SIZE MAX_BYTES_PER_EQ   // Largest message size in characters


// TODO: Augment this list or offload to config file
#define TYPE_WILDCARD			0
#define TYPE_ADBUF				1
#define TYPE_ERROR				2
#define TYPE_HEARTBEAT			3
#define TYPE_TRACE2_COMP_UA	4
#define TYPE_NANOBUF				5
#define TYPE_ACK					6
#define TYPE_PICK_SCNL			8
#define TYPE_CODA_SCNL			9
#define TYPE_PICK2K				10
#define TYPE_CODA2K				11
#define TYPE_PICK2				12
#define TYPE_CODA2				13
#define TYPE_HYP2000ARC			14
#define TYPE_H71SUM2K			15
// #define TYPE_EVENT_SCNL			16
#define TYPE_HINVARC				17
#define TYPE_H71SUM				18
#define TYPE_TRACEBUF2			19
#define TYPE_TRACEBUF			20
#define TYPE_LPTRIG				21
#define TYPE_CUBIC				22
#define TYPE_CARLSTATRIG		23
#define TYPE_TRIGLIST			24
#define TYPE_TRIGLIST2K			25
#define TYPE_TRACE_COMP_UA		26
#define TYPE_STRONGMOTION		27
#define TYPE_MAGNITUDE			28
#define TYPE_STRONGMOTIONII	29
#define TYPE_LOC_GLOBAL			30
#define TYPE_LPTRIG_SCNL		31
#define TYPE_CARLSTATRIG_SCNL	32
#define TYPE_TRIGLIST_SCNL		33
#define TYPE_MSEED				35
#define TYPE_ACTIVATE_MODULE	50
#define TYPE_SPECTRA				100
#define TYPE_QUAKE2K				101
#define TYPE_LINK					102
#define TYPE_EVENT2K				103
#define TYPE_PAGE					104
#define TYPE_KILL					105
#define TYPE_DSTDRINK			106
#define TYPE_RESTART				107
#define TYPE_REQSTATUS			108
#define TYPE_STATUS				109
#define TYPE_EQDELETE			110
#define TYPE_EVENT_SCNL			111
#define TYPE_RECONFIG			112
#define TYPE_STOP					113
#define TYPE_CANCELEVENT		114
#define TYPE_EVENT_SIGNAL		130
#define TYPE_EC_EVENT			131
#define TYPE_EC_CANCEL			132
#define TYPE_TRIG_SIGNAL		145
#define TYPE_TRIMAG_MAG			155
#define TYPE_K2INFO_PACKET		199


int main( int argc, char *argv[] )
{
	int	i;
	int	fd;
	int	nread = 0;
	int	printHeader	= 0;
	char	fileName[1024];
	char	buffer[MAX_SIZE];

// Preference-related things --> TODO: offload to config file or something
//

	// Earthworm 7.9 is set up to support up to 256 different message types.
	int	activeTypes[256];

	for(i=0; i<256; ++i) { activeTypes[i] = 0;}

	activeTypes[TYPE_WILDCARD]				= 1;
	activeTypes[TYPE_ADBUF]					= 1;
	activeTypes[TYPE_ERROR]					= 0;
	activeTypes[TYPE_HEARTBEAT]			= 0;
	activeTypes[TYPE_TRACE2_COMP_UA]		= 1;
	activeTypes[TYPE_NANOBUF]				= 1;
	activeTypes[TYPE_ACK]					= 1;
	activeTypes[TYPE_PICK_SCNL]			= 1;
	activeTypes[TYPE_CODA_SCNL]			= 1;
	activeTypes[TYPE_PICK2K]				= 1;
	activeTypes[TYPE_CODA2K]				= 1;
	activeTypes[TYPE_PICK2]					= 1;
	activeTypes[TYPE_CODA2]					= 1;
	activeTypes[TYPE_HYP2000ARC]			= 1;
	activeTypes[TYPE_H71SUM2K]				= 1;
//	activeTypes[TYPE_EVENT_SCNL]			= 0;
	activeTypes[TYPE_HINVARC]				= 1;
	activeTypes[TYPE_H71SUM]				= 1;
	activeTypes[TYPE_TRACEBUF2]			= 1;
	activeTypes[TYPE_TRACEBUF]				= 1;
	activeTypes[TYPE_LPTRIG]				= 1;
	activeTypes[TYPE_CUBIC]					= 1;
	activeTypes[TYPE_CARLSTATRIG]			= 1;
	activeTypes[TYPE_TRIGLIST]				= 1;
	activeTypes[TYPE_TRIGLIST2K]			= 1;
	activeTypes[TYPE_TRACE_COMP_UA]		= 1;
	activeTypes[TYPE_STRONGMOTION]		= 1;
	activeTypes[TYPE_MAGNITUDE]			= 1;
	activeTypes[TYPE_STRONGMOTIONII]		= 1;
	activeTypes[TYPE_LOC_GLOBAL]			= 1;
	activeTypes[TYPE_LPTRIG_SCNL]			= 1;
	activeTypes[TYPE_CARLSTATRIG_SCNL]	= 1;
	activeTypes[TYPE_TRIGLIST_SCNL]		= 1;
	activeTypes[TYPE_MSEED]					= 1;
	activeTypes[TYPE_ACTIVATE_MODULE]	= 1;
	activeTypes[TYPE_SPECTRA]				= 1;
	activeTypes[TYPE_QUAKE2K]				= 1;
	activeTypes[TYPE_LINK]					= 1;
	activeTypes[TYPE_EVENT2K]				= 1;
	activeTypes[TYPE_PAGE]					= 1;
	activeTypes[TYPE_KILL]					= 1;
	activeTypes[TYPE_DSTDRINK]				= 1;
	activeTypes[TYPE_RESTART]				= 1;
	activeTypes[TYPE_REQSTATUS]			= 1;
	activeTypes[TYPE_STATUS]				= 1;
	activeTypes[TYPE_EQDELETE]				= 1;
	activeTypes[TYPE_EVENT_SCNL]			= 1;
	activeTypes[TYPE_RECONFIG]				= 1;
	activeTypes[TYPE_STOP]					= 1;
	activeTypes[TYPE_CANCELEVENT]			= 1;
	activeTypes[TYPE_EVENT_SIGNAL]		= 1;
	activeTypes[TYPE_EC_EVENT]				= 1;
	activeTypes[TYPE_EC_CANCEL]			= 1;
	activeTypes[TYPE_TRIG_SIGNAL]			= 1;
	activeTypes[TYPE_TRIMAG_MAG]			= 1;
	activeTypes[TYPE_K2INFO_PACKET]		= 1;


	typedef struct {
		int64_t sec;
		int64_t usec;
		int64_t length;
		char type;
		char mod;
		char instid;
	} Header;

	Header header;

   if((argc < 2))
   {
		printf("Error: argc < 2\n");
      return -1;
   }
   
	if (strcmp(argv[1], "-h") == 0)
	{
		printHeader = 1;
		strcpy(fileName, argv[2]);
	}
	else
	{
		printHeader = 0;
		strcpy(fileName, argv[1]);
	}

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
	
		// TRACE2_HEADER *trh;
		// trh = (TRACE2_HEADER *) buffer;
		
		// double starttime_orig = trh->starttime;
		// double endtime_orig = trh->endtime;

		/*
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
		*/

		if(activeTypes[(int) header.type] == 1) 
		{
			if(printHeader)
			{
				printf("[%d]", (int) header.sec);
				printf("[%d]", (int) header.usec);
				printf("[%d]", (int) header.length);
				printf("[%d]", (int) header.type);
				printf("[%d]", (int) header.mod);
				printf("[%d]", (int) header.instid);
			}

			if((int) header.type == TYPE_TRACEBUF2)
			{
				TRACE2_HEADER *trh;
				trh = (TRACE2_HEADER *) buffer;
		
				double starttime_orig = trh->starttime;
				double endtime_orig = trh->endtime;
			
				printf(" Start:[%f]", starttime_orig);
				printf(" End:[%f]", endtime_orig);
				printf(" Latency:[%f]", (header.sec/1. + header.usec/1000000.) - endtime_orig);

				printf("\n");
			}
			else
			{
				printf("%s", (char *) buffer);
			}

		}
	}

	close(fd);

	return 0;
}

