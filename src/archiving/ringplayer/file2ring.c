	
/*****************************************************************
*	Name: file2ring                                               *
*                                                                *
*  Author: Andrew Good                                           *
*                                                                *
*  Description:                                                  *
*                                                                *
*  This program takes as input a file of the type created        *
*  by "ring2file", which consists of messages with headers       *
*  that contain the message logo and original time of receipt.   *
*  Message content is the exact sequence of bytes that was read  *
*  from the ring by "ring2file".  These messages are placed on   *
*  an output Earthworm ring with a cadence determined from       *
*  analysis of the receipt times in the headers, to simulate     *
*  the behavior of input ring that was monitored by "ring2file". *
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

const char *MYNAME = "file2ring";

unsigned char     TypeTraceBuf2;     
unsigned char     TypePickSCNL;     
unsigned char     TypeQuake2K;     
unsigned char     TypeArc;     
unsigned char     TypeCarlStaTrig;     
unsigned char     TypeCarlSubTrig;     
unsigned char     TypeHeartbeat;
unsigned char     TypeError;

void lookup (void);
void usage (void);


// Main method
int main( int argc, char *argv[] )
{
	int	debug				= 1;	// Toggle debug messages
	int	makeCurrent    = 0;	// Offset timestamps to simulate "new" data
	
	int   first  = 1;				// Whether this is the first record in the input file
	int	fixed_interval = 0;	// Whether to play back at original cadence or fixed rate
	int	use_window     = 0;	// Whether or not to play back only a specific time window	
	int	fd;						// Input file descriptor 
	int	nread = 0;				// Amount read into buffer in previous "read" command
	int	i, j;

	long	outputKey;				// Key to the transport ring we write to


// Double-precision times for printouts; less accurate than
// storing as second/microsecond pairs.

	double firstTime  = 0.0;	// Time of receipt of first record
	double prevTime   = 0.0;	// Time of receipt of previous record
	double nextTime   = 0.0;	// Time of receipt of subsequent record
	double secOffset	= 0.0;	// Seconds to offset for simulating "new" data
	double sleepTime  = 0.0;	// Variable inter-record sleeptime for playback

	double starttime_orig = 0.0;
	double endtime_orig = 0.0;

// Microsecond-precision versions of time quanitites described above.
// These are only really necessary when timing playback of wave ring
// records; other types do not require this level of precision.

	int64_t firstTime_sec  = 0;
	int64_t firstTime_usec = 0;
	int64_t prevTime_sec   = 0;
	int64_t prevTime_usec  = 0;
	int64_t nextTime_sec   = 0;
	int64_t nextTime_usec  = 0;
	int64_t offset_sec     = 0;
	int64_t offset_usec    = 0;
	int64_t sleepTime_sec  = 0;
	int64_t sleepTime_usec = 0;


	double interval	= 100.0;	// Inter-message interval (fixed rate mode)
	double playSpeed  = 1.0;	// Playback rate (original cadence mode)
	// double drift 	= 0;
	
	struct timeval tp;   // Parameter for calling gettimeofday()
	struct timezone tzp; // Parameter for calling gettimeofday()

	time_t startTime = 0;
	time_t endTime   = 0;

	char	fileName[1024];		// Input file name
	char	outRing[1024];			// Output ring name
	char	buffer[MAX_SIZE];		// Buffer for message contents

	char *end;

	// long	outputLength;
	// char  outmsg[MAX_SIZE];  


	SHM_INFO	outRegion;		// Shared memory information about output ring
	MSG_LOGO	outputLogo;		// Message description ("transport.h")
	TRACE2_HEADER *trh;

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


// Header structure holds data about messages collected
// when the messages were written to the input file

	typedef struct {
		int64_t sec;
		int64_t usec;
		int64_t length;
		unsigned char type;
		unsigned char mod;
		unsigned char instid;
	} Header;	

	Header header;


// Check command line statements
//
// TODO: Revise this section to be cleaner and parse flags in 
// a more standard way with a library?

   if(argc < 3)
   {
		printf("Error: argc < 3\n");
		usage();	
      return -1;
   }
   if(argc > 8)
   {
		printf("Error: argc > 8\n");
		usage();	
      return -1;
   }

	if(strcmp(argv[1], "-c") == 0)
	{
		makeCurrent = 1;
		
		for(i=1; i<argc-1; ++i) { argv[i] = argv[i+1]; }
		--argc;
		
		if(debug > 1)
		{
			printf("makeCurrent = 1\n"); 
			for(j=1; j<argc; ++j)	
			{ 
				printf("argv[%d] = %s\n", j, argv[j]); 
			}
		}
	}


// Right now, out of convenience, -f and -r options cannot be
// used togther (spacing the records by a fixed amount and 
// playing it back at the original cadence, but a different 
// rate, aren't really compatible with each other).
//
// The usefulness of the -f option is questionable, but it
// is left in for now because it doesn't really hurt.

   if(strcmp(argv[1], "-f") == 0)
	{
		fixed_interval = 1;
		interval = atof(argv[2]);
		for(i=1; i<argc-2; ++i) { argv[i] = argv[i+2]; }
		argc-=2;
	}
	else if(strcmp(argv[1], "-r") == 0)
	{
		playSpeed = atof(argv[2]);
		for(i=1; i<argc-2; ++i) { argv[i] = argv[i+2]; }
		argc-=2;
	}	

	strcpy(fileName, argv[1]);
	strcpy(outRing,  argv[2]);
	
	if(argc > 3)
	{
		use_window = 1;

		if(argc < 5)
		{
			printf("Error: Insufficient arguments.\n");
			usage();
		}

	// Only play back records with epoch timestamps between
	// startTime and endTime (1-second precision)
		startTime =  (time_t) strtol(argv[3], &end, 10);
		endTime   =  (time_t) strtol(argv[4], &end, 10);
	}

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


// Look up logos
	lookup();


// Attach to output ring
	if((outputKey = GetKey(outRing)) == -1)
	{
		fprintf( stderr, "Error: Invalid RingName; exiting...\n");
		fflush(stderr);
		return -1;
	}

	tport_attach(&outRegion, outputKey);

	if(debug > 1)
	{
		fprintf(stderr, "Attached to output ring.\n");
		fflush(stderr);
	}


// Get reference time 
	gettimeofday(&tp, &tzp);
	// hrtime_ew( &d_now );


// Open input file
	fd = open(fileName, O_RDONLY);
	lseek(fd, 0, SEEK_SET);


// Main loop
	while(1)
	{
	// Read (constant-size) header for message 
		nread = read(fd, &header, sizeof(header)); 

		if(nread <= 0)
			break;

		if(debug > 1)
		{
			printf("DEBUG: nread = [%d]\n", nread);
			printf("DEBUG: header.length = [%d]\n", (int) header.length);
			fflush(stdout);
		}

	// Read message to place on output ring
		nread = read(fd, buffer, header.length);
		
		if(nread <= 0)
			break;

		if(debug > 1)
		{
			printf("DEBUG: nread = [%d]\n", nread);
			fflush(stdout);		
		}
	
	// Append null character in case previous message was longer
		buffer[header.length] = '\0';


	// TODO:
	// To test whether or not the same inputs to AQMS result in the same outputs,
	// it may be useful to simulate "new" events that are otherwise identical to
	// "old" events, to see whether or not the information that makes it into the
	// database is consistent with the results for those old events.  This would 
	// entail modifying timestamps for different types of records with an offset
	// (which itself would entail recognizing the record type, which this program
	// currently does not do for any records other than TRACE_BUF2).
		
		if(use_window)
		{
			if(header.sec < startTime)
				continue;
			
			if(header.sec > endTime)
				break;
		}

		if(first)
		{
			if(debug > 1)
			{
				printf("DEBUG: First Record -->\n");
				fflush(stdout);
			}
		
			firstTime = (double) header.sec*1000000. + (double) header.usec/1.;
			secOffset = (tp.tv_sec*1. + tp.tv_usec/1000000.) - firstTime/1000000.;
			
			firstTime_sec  = header.sec;		
			firstTime_usec = header.usec;

			offset_sec  = tp.tv_sec  - firstTime_sec;		
			if(offset_sec < 0)
			{
				printf("ERROR: firstTime > currentTime\n");
				return -1;
			}
			offset_usec = tp.tv_usec - firstTime_usec;		
			if(offset_usec < 0)
			{
				offset_sec -= 1;
				offset_usec += 1000000;
			}

			if(debug > 1)
			{
				printf("DEBUG: tp.tv_sec      = [%d]\n", (int) tp.tv_sec);
				printf("DEBUG: tp_tv_usec     = [%d]\n", (int) tp.tv_usec);
				printf("DEBUG: firstTime_sec  = [%d]\n", (int) firstTime_sec);
				printf("DEBUG: firstTime_usec = [%d]\n", (int) firstTime_usec);
				printf("DEBUG: offset_sec     = [%d]\n", (int) offset_sec);
				printf("DEBUG: offset_usec    = [%d]\n", (int) offset_usec);
				fflush(stdout);
			}

			prevTime = firstTime;
			nextTime = firstTime;
			
			prevTime_sec  = firstTime_sec;
			prevTime_usec = firstTime_usec;
			nextTime_sec  = firstTime_sec;
			nextTime_usec = firstTime_usec;
			
			first = 0;
		}

		if(debug > 1)
		{
			printf("startTime=[%ld], endTime=[%ld], header.sec=[%lld]\n", 
			        startTime, endTime, header.sec);
			fflush(stdout);
		}

		if(debug > 1)
		{
			printf("[%d]", (int) header.sec);
			printf("[%d]", (int) header.usec);
			printf("[%d]", (int) header.length);
			printf("[%d]", (int) header.type);
			printf("[%d]", (int) header.mod);
			printf("[%d]", (int) header.instid);
			printf(" --> Message: %s", (char *) buffer);
			printf("\n");
			fflush(stdout);
		}


	// Construct output message logo 
		outputLogo.type   = header.type;
		outputLogo.mod    = header.mod;
		outputLogo.instid = header.instid;

		nextTime = (double) header.sec*1000000. + (double) header.usec/1.;
		nextTime_sec  = header.sec;
		nextTime_usec = header.usec;
		

	// The following calculation assumes that messages are in time order; since the timestamps
	// are system clock times when messages were received, as long as the system clock is 
	// operating properly, this will always be the case.

	if(outputLogo.type == TypeTraceBuf2) // Wave records
	{
		trh = (TRACE2_HEADER *) buffer;
		
		starttime_orig = trh->starttime;
		endtime_orig = trh->endtime;
	}

		useconds_t sleep_usec = 0; 	
		

		if(fixed_interval)
		{
			// Since outputting records at fixed intervals in no way replicates anything
			// that happens in real life, this option is included primarily for debugging
			// purposes.

			sleep_ew(interval*1000.);
		}
		else
		{
			gettimeofday(&tp, &tzp);
			double unow = tp.tv_sec*1000000. + tp.tv_usec/1.; 
			
			int64_t now_sec = tp.tv_sec;
			int64_t now_usec = tp.tv_usec;

			if(debug > 1)
			{
				printf("now_sec=[%d] now_usec=[%d]\n", (int) now_sec, (int) now_usec);
				fflush(stdout);
			}
	
			// if(playSpeed == 1.0)
			// {
				sleepTime = (nextTime + 1000000.*secOffset) - unow;
		
				int64_t updatedTime_sec  = 0;
				int64_t updatedTime_usec = nextTime_usec + offset_usec;
				int64_t carry_sec = (int64_t) (updatedTime_usec / 1000000);

				if(debug > 1)
				{
					printf("DEBUG: updatedTime_sec = [%d]", (int) updatedTime_sec);
					printf(" updatedTime_usec = [%d]", (int) updatedTime_usec);
					printf(" carry_sec  = [%d]\n", (int) carry_sec);
					fflush(stdout);
				}

				updatedTime_usec -= carry_sec*1000000;
				updatedTime_sec = nextTime_sec + offset_sec + carry_sec;
			
				sleepTime_sec = updatedTime_sec - now_sec;
				sleepTime_usec = (1000000*sleepTime_sec + updatedTime_usec - now_usec) / playSpeed;
 	
				if(debug > 1)
				{
					printf("DEBUG: updatedTime_sec = [%d]", (int) updatedTime_sec);
					printf(" updatedTime_usec = [%d]", (int) updatedTime_usec);
					printf(" --> sleepTime_sec = [%d]", (int) sleepTime_sec);
					printf(" sleepTime_usec = [%d]\n", (int) sleepTime_usec);
					fflush(stdout);
				}

				sleep_usec = sleepTime_usec; 	
				if(debug > 1)
				{
					printf("DEBUG: sleep_usec = [%d]\n", (int) sleepTime_usec);
					fflush(stdout);
				}
				if((int) sleep_usec > 0)
				{
					if(debug)
					{
						printf("DEBUG: Sleeping [%d] microseconds...\n", (int) sleep_usec);
						fflush(stdout);
					}
					usleep(sleep_usec);
				}
		//	 }
		//	 else
		//	 {
				// drift = unow - prevTime - 1000000.*(secOffset);
				// drift = unow - nextTime - 1000000.*(secOffset);
				// nextTime -= drift;	
			
				/*
				if(nextTime - prevTime > 0)
				{
					sleepTime = ((nextTime - prevTime)	/ playSpeed);
				
					// sleep_ew((nextTime - prevTime) / 1000. * playSpeed)
					usleep(sleepTime);
				}
				*/
		//	}
		}

		if (makeCurrent)
		{
			if(outputLogo.type == TypeTraceBuf2) // Wave records
			{
			// Update record timestamps
				trh->starttime += secOffset;
				trh->endtime   += secOffset;
	
			// DEBUG: The following block is code for altering the message content
			// of a tracebuf record, and was used for intentionally replicating the
			// input from a particular "broken" station.  Could be useful for creating
			// simulations in future, if expanded upon.  

				/*
				if(strcmp(trh->sta, "SNO") == 0)
				{
					int *data;
					data = (int *)(buffer + sizeof(TRACE2_HEADER));
					for(i=0; i<trh->nsamp; ++i)
					{
						// printf("data[%d] = %d\n", i, data[i]);
						data[i] = -1000;
					}
				}
				*/

/*
				if(strcmp(trh->sta, "BAI") == 0)
				{
					int *data;
					data = (int *)(buffer + sizeof(TRACE2_HEADER));
					for(i=0; i<trh->nsamp; ++i)
					{
						// printf("data[%d] = %d\n", i, data[i]);
						data[i] = 8388607;
					}
				}
*/

			} 
			else if(outputLogo.type == TypePickSCNL) { }
			else if(outputLogo.type == TypeQuake2K) { }
			else if(outputLogo.type == TypeArc) { }
			else if(outputLogo.type == TypeCarlStaTrig) { }
			else if(outputLogo.type == TypeCarlSubTrig) { }
		}

	// Write output message to output ring
		if(activeTypes[(int) outputLogo.type] == 1) 
		{
			if(debug)
			{
				if(outputLogo.type == TypeTraceBuf2) // Wave records
				{
					gettimeofday(&tp, &tzp);
					double trueNow  = tp.tv_sec*1. + tp.tv_usec/1000000.; 
					double trhStart = trh->starttime;
				//	printf("t_now(s)=[%f] start_new(s)=[%f] diff(s)=[%f] t_header(s)=[%f] start_orig=[%f] end_orig=[%f] lat_orig=[%f] t_now-t_header=[%f]\n", 
				//			trueNow, trhStart, (trueNow - trhStart), nextTime/1000000., starttime_orig, endtime_orig, 
				//			(nextTime/1000000. - endtime_orig), (trueNow-nextTime/1000000.));
					printf("t_now(s)=[%f] start_new(s)=[%f] diff(s)=[%f] t_header(s)=[%f] lat_orig=[%f]\n", 
							trueNow, trhStart, (trueNow - trhStart), nextTime/1000000., (nextTime/1000000. - endtime_orig));
					fflush(stdout);
				}
			}

			tport_putmsg(&outRegion, &outputLogo, (int) header.length, buffer);
		}
	
		
	// }
	/*
	else	// Non-wave records (Pick, Hypo, Trig, Subtrig, etc.)
	{
		if(fixed_interval)
		{
			sleep_ew(interval*1000.);
		}
		else
		{
			if(nextTime - prevTime > 0)
			{
				//sleep_ew((nextTime - prevTime) / (playSpeed));
				// sleep_ew(1000.);
			}
		}

	// Write output message to output ring
		if(activeTypes[(int) outputLogo.type] == 1) 
		{
			tport_putmsg(&outRegion, &outputLogo, (int) header.length, buffer);
		}

		// DEBUG
		if(debug > 1)
		{
			printf("nextTime(us)=[%f] prevTime(us)=[%f] sleeptime(us)=[%f] --> \n", 
					nextTime, prevTime, ((nextTime-prevTime)/playSpeed));
			fflush(stdout);
		}
		// END DEBUG
	}
	*/


	// Set reference times for next loop iteration
		prevTime = nextTime;

		prevTime_sec = nextTime_sec;
		prevTime_usec = nextTime_usec;	
	}

	if(debug > 1)
	{
		printf("DEBUG: Out of main loop.\n");
		fflush(stdout);		
	}

	tport_detach(&outRegion);
	close(fd); // Close input file

	return 0;
}  // end main


// Function: lookup
void lookup( void )
{
// Specify logos to get

	if(GetType("TYPE_HEARTBEAT", &TypeHeartbeat) != 0)
	{
		fprintf(stderr, "%s: Error getting Type_Heartbeat.\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	if( GetType( (char*)"TYPE_ERROR", &TypeError ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_ERROR>; exiting!\n", MYNAME);
		exit( -1 );
	}
	if(GetType("TYPE_TRACEBUF2", &TypeTraceBuf2) != 0)
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_TRACEBUF2>; exiting!\n", MYNAME);
		fflush(stderr);
		exit (-1 );
	}
	if( GetType( (char*)"TYPE_PICK_SCNL", &TypePickSCNL ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_PICK_SCNL>; exiting!\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	if( GetType( (char*)"TYPE_QUAKE2K", &TypeQuake2K ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_QUAKE2K>; exiting!\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	if( GetType( (char*)"TYPE_HYP2000ARC", &TypeArc ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_HYP2000ARC>; exiting!\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	if( GetType( (char*)"TYPE_CARLSTATRIG", &TypeCarlStaTrig ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_CARLSTATRIG>; exiting!\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	if( GetType( (char*)"TYPE_TRIGLIST_SCNL", &TypeCarlSubTrig ) != 0 )
	{
		fprintf(stderr, "%s: Invalid message type <TYPE_TRIGLIST_SCNL>; exiting!\n", MYNAME);
		fflush(stderr);
		exit( -1 );
	}
	//
	// Add additional clauses here for other types to register
	
}  // end lookup


// Function: usage
void usage(void)
{
	printf("\nUsage Syntax: \n");
	printf("--------------------------------------------------------\n");
	printf("Usage 1:  file2ring <infile_name> <ring_name>\n");
	printf("Usage 2:  file2ring [-c] <infile_name> <ring_name> <start_time> <end_time>\n");
	printf("Usage 3:  file2ring [-c] [-f] <fixed_interval> <infile_name> <ring_name> <start_time> <end_time>\n");
	printf("Usage 4:  file2ring [-c] [-r] <playback_rate> <infile_name> <ring_name> <start_time> <end_time>\n");
	printf("\nRequired Arguments:\n");
	printf("--------------------------------------------------------\n");
	printf("<infile_name> = Binary file with recorded EW ring data\n");
	printf("<ring_name>   = EW ring to play data into\n");
	printf("\nOptional Arguments: \n");
	printf("--------------------------------------------------------\n");
	printf("[-c] = Update record timestamps to \"current\" time; simulate newly arrived data\n");
	printf("[-f] = Use fixed inter-message interval for playback (instead of original cadence)\n");
	printf("<fixed_interval> =  Forced inter-message interval (seconds) \n");
	printf("[-r] = Play back records with original cadence, at a user-defined speed\n");
	printf("<playback_rate>  =  Multiple to apply to original message cadence\n");
	printf("<start_time> = Timestamp of beginning of playback window (Unix time)\n");
	printf("<end_time>   = Timestamp of end of playback window (Unix time)\n");
	printf("\nNotes:\n");
	printf("--------------------------------------------------------\n");
	printf(">>> [-f] and [-r] are mutually exclusive.\n");
	printf(">>> If <start_time> is earlier than the earliest timestamp in the input file,\n");
	printf("    playback will start at the beginning of the file.\n");
	printf(">>> If <end_time> is later than the last timestamp in the input file,\n");
	printf("    playback will end at the end of the file.\n");
}  // end usage



