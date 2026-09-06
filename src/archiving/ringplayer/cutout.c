	
/*****************************************************************
*	Name: cutout                                                  *
*                                                                *
*  Author: Andrew Good                                           *
*                                                                *
*  Description:                                                  *
*                                                                *
******************************************************************/


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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define MAX_SIZE 60000   // Largest message size in characters

int main( int argc, char *argv[] )
{
	int	debug  = 1;				// Toggle debug messages
	// int   first  = 1;				// Whether this is the first record in the input file 
	// int	fixed_interval = 0;	// Whether to play back at original cadence or fixed rate
	// int	use_window     = 0;	// Whether or not to play back only a specific time window	
	int	fd;						// Input file descriptor 
	int	ofd;						// Output file descriptor 
	int	nread = 0;				// Amount read into buffer in previous "read" command
	
	// double firstTime = 0.0;		// Time of receipt of first record
	// double prevTime  = 0.0;		// Time of receipt of previous record
	// double nextTime  = 0.0;		// Time of receipt of subsequent record
	// double interval	= 100.0;	// Inter-message interval (fixed rate mode)
	// double playSpeed  = 1.0;	// Playback rate (original cadence mode)

	time_t startTime = 0;
	time_t endTime   = 0;

	char	inFileName[1024];		// Input file name
	char	outFileName[1024];		// Input file name
	char	buffer[MAX_SIZE];		// Buffer for message contents

	char *end;

	// long	outputLength;
	// char  outmsg[MAX_SIZE];  
	
	typedef struct {
		int64_t sec;
		int64_t usec;
		int64_t length;
		char type;
		char mod;
		char instid;
	} Header;		// Header structure holds data about messages collected
						// when the messages were written to the input file

	Header header;

// Check command line statements
   if(argc != 5)
   {
		printf("Error: argc != 5\n");
		printf("Usage 1:  cutout <infile_name> <outfile_name> <start_time> <end_time>\n");
      return -1;
   }
	/*
   else if(strcmp(argv[1], "-s") == 0)
	{
		fixed_interval = 1;
		interval = atof(argv[2]);
		strcpy(inFileName, argv[3]);
		strcpy(outRing,  argv[4]);

		if(argc > 5)
		{
			use_window = 1;
			startTime =  (time_t) strtol(argv[5], &end, 10);
			endTime   =  (time_t) strtol(argv[6], &end, 10);
		}
	}
   else if(strcmp(argv[1], "-r") == 0)
	{
		playSpeed = atof(argv[2]);
		strcpy(inFileName, argv[3]);
		strcpy(outRing,  argv[4]);

		if(argc > 5)
		{
			use_window = 1;
			startTime =  (time_t) strtol(argv[5], &end, 10);
			endTime   =  (time_t) strtol(argv[6], &end, 10);
		}
	}
	*/
	else
	{
		strcpy(inFileName,   argv[1]);
		strcpy(outFileName,  argv[2]);
		
		startTime =  (time_t) strtol(argv[3], &end, 10);
		endTime   =  (time_t) strtol(argv[4], &end, 10);
		/*
		if(argc > 3)
		{
			use_window = 1;
			startTime =  (time_t) strtol(argv[3], &end, 10);
			endTime   =  (time_t) strtol(argv[4], &end, 10);
		}
		*/
	}

	/*
	if(debug > 1)
	{
		if(use_window)
		{
			printf("Time window: startTime=[%ld], endTime=[%ld]\n", startTime, endTime);
		}
		else
		{
			printf("No time window, play back entire file.\n");
		}
	}
	*/


// Open input file
	fd = open(inFileName, O_RDONLY);
	lseek(fd, 0, SEEK_SET);

// Open output file
	ofd = open(outFileName, O_RDWR|O_CREAT, 0664);
	lseek(ofd, 0, SEEK_SET);

// Main loop
	while(1)
	{
	// Read (constant-size) header for message 
		read(fd, &header, sizeof(header)); 

	// Read message to place on output ring
		nread = read(fd, buffer, header.length);
		
	// Append null character in case previous message was longer
		//buffer[header.length] = '\0';

		if(nread <= 0)
			break;

		if(header.sec <= startTime)
			continue;
		
		if(header.sec > endTime)
			break;

	// Write header + message to output file
		write(ofd, (char *)&header, sizeof(header));
		write(ofd, buffer, header.length);

/*
		if(first)
		{
			firstTime = (double) header.sec*1000. + (double) header.usec/1000.;
			prevTime = firstTime;
			nextTime = firstTime;
			first = 0;
		}

		if(debug > 1)
		{
			printf("startTime=[%ld], endTime=[%ld], header.sec=[%ld]\n", startTime, endTime, header.sec);
		}

		if(debug)
		{
			printf("[%d]", (int) header.sec);
			printf("[%d]", (int) header.usec);
			printf("[%d]", (int) header.length);
			printf("[%d]", (int) header.type);
			printf("[%d]", (int) header.mod);
			printf("[%d]", (int) header.instid);
			printf(" --> Message: %s", (char *) buffer);
			// printf("\n");
		}

		nextTime = (double) header.sec*1000. + (double) header.usec/1000.;

		if(debug > 1)
		{
			printf("nextTime=[%f], prevTime=[%f], sleeptime=[%f]\n", nextTime, prevTime, nextTime-prevTime);
		}


	// The following calculation assumes that messages are in time order; since the timestamps
	// are system clock times when messages were received, as long as the system clock is 
	// operating properly, this will always be the case.

		if(fixed_interval)
		{
			sleep_ew(interval*1000.);
		}
		else
		{
			if(nextTime - prevTime > 0)
			{
				sleep_ew((nextTime - prevTime) / playSpeed);
			}
		}

	// Set reference time for next loop iteration
		prevTime = nextTime;
	*/	
	
	}
	close(fd); // Close input file
	close(ofd); // Close output file

	return 0;
}

