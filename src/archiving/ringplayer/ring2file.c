   
/******************************************************************
 *  Name: ring2file                                               *
 *                                                                *
 *  Author: Andrew Good                                           *
 *                                                                *
 *  Description:                                                  *
 *                                                                *
 *  This program attaches to an Earthworm transport ring and      *
 *  monitors it.  Any message that is detected on the ring is     *
 *  read and output to a file byte-for-byte, along with a header  *
 *  that keeps track of the message logo and the system clock     *
 *  time when the message was detected.                           *
 *                                                                *
 *  Structurally, this program is based on "sniffwave", from the  *
 *  Earthworm package.                                            * 
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
#include <earthworm.h>
#include <transport.h>

// #define MAX_SIZE MAX_BYTES_PER_EQ   // Largest message size in characters
#define MAX_SIZE 6000000   // Largest message size in characters


// Things to look up in the earthworm.h tables with getutil.c functions

static unsigned char InstId;        // local installation id              
static unsigned char InstWildCard;  // wildcard for installations         
static unsigned char ModWildCard;   // wildcard for module                
static unsigned char TypeWildCard;  // wildcard for message type          
// static unsigned char TypeTraceBuf;  // binary 1-channel waveform msg      
// static unsigned char TypeTraceBuf2; // binary 1-channel waveform msg      
// static unsigned char TypeCompUA;    // binary compressed waveform msg     
// static unsigned char TypeCompUA2;   // binary compressed waveform msg     
// static unsigned char TypeMseed;     // binary compressed mseed waveform msg     
// static unsigned char InstToGet;
// static unsigned char TypeToGet;
// static unsigned char ModToGet;


int main( int argc, char *argv[] )
{
	SHM_INFO inRegion;			// Shared memory information about input ring
	MSG_LOGO getlogo[1];
	MSG_LOGO logo;					// Message description ("transport.h")
	char     *inRing;				// Name of input ring           
	long     inkey;				// Key to input ring            
	char     msg[MAX_SIZE+1];	// Message from ring

	int		debug = 1;			// Debug flag
	int		i, j;					// Utility counters
	int      rc;					// Return code
	int		fd;					// Input file descriptor
	int      no_flush = 0;		// Flush ring buffer by default 
	long     length;

	unsigned char inseq;			// Transport seq# in input ring 
	char     verbose = 1;		// Verbose will print names 

	char     TypeName[MAX_TYPE_STR+1];	// Message type
	char     ModName[MAX_MOD_STR+1];		// Module name
	char     InstName[MAX_INST_STR+1];	// Institution name

	char		fileName[1024];	// Name of input file

	struct timeval tp;	// Parameter for calling gettimeofday()
	struct timezone tzp;	// Parameter for calling gettimeofday()

	typedef struct {
		int64_t sec;
		int64_t usec;
		int64_t length;
		unsigned char type;
		unsigned char mod;
		unsigned char instid;
	} Header;	// Header structure holds data about messages collected 
            	// when the messages were written to the input file

	Header header;

	int64_t tStart;
	int64_t duration = 4200;
	int fixedDuration = 0;

// Check command line statements
	if ( (argc < 3) || (argc > 8) )
	{
		// TODO: Correct these usage statements so that they are...correct.
		printf( "Usage 1:  ring2file <ring_name> <outfile_name>\n" );
		printf( "Usage 2:  ring2file [-n] <ring_name> <outfile_name> verbose\n" );
		printf( "Usage 3:  ring2file [-d] <ring_name> <outfile_name> verbose\n" );
		printf( "Usage 4:  ring2file [-n] [-d] <ring_name> <outfile_name> verbose\n" );
		// TODO: Add other usages here
		printf( "ring2file shows full ring, inst and module names if 'v' or \n" );
		printf( "'verbose' is specified as the final parameter. Otherwise you'll\n" );
		printf( "see numeric IDs for these three, as defined in earthworm*.d files. \n" );
		printf( "(Modules from external installations are not identified other than by number.)\n" );
		printf( "A new option is -n to not flush the ring of all messages first, must be before ringname if used\n" );
		printf( "the default is to flush the ring of all messages and only show those that come in after invocation\n" );
		return -1;
	}

	if (strcmp(argv[1], "-n") == 0)
	{
		no_flush = 1;			// DO NOT FLUSH the buffer first, show all old messages first

		for(i=1; i<argc-1; ++i)	{ argv[i] = argv[i+1]; }
		--argc;

		if(debug)
		{
			printf("no_flush = 1\n"); 
			for(j=1; j<argc; ++j)	
			{ 
				printf("argv[%d] = %s\n", j, argv[j]); 
			}
		}
	}		 	 
	if (strcmp(argv[1], "-d") == 0)
	{
		fixedDuration = 1;

		for(i=1; i<argc-1; ++i) { argv[i] = argv[i+1]; }
		--argc;

		if(debug)
		{
			printf("fixedDuration = 1\n"); 
			for(j=1; j<argc; ++j)	
			{ 
				printf("argv[%d] = %s\n", j, argv[j]); 
			}
		}
	}		 
	inRing   = argv[1];
	strcpy(fileName, argv[2]);

	if(debug)
	{
		printf("inRing = %s\n", inRing); 
		printf("fileName = %s\n", (char *) fileName); 
	}

	if(fixedDuration) 
	{
		if(argc < 4)
		{
			printf("Error: No duration given\n");
			return -1;
		}

		duration = atoi(argv[3]); 
		if(debug)
		{
			printf("duration = %lld seconds\n", duration);
		}
	}

	gettimeofday(&tp, &tzp);
	tStart = tp.tv_sec;

	if(debug)
	{
		printf("tStart = %lld\n", tStart); 
	}

// Look up local installation id
	if ( GetLocalInst( &InstId ) != 0 )
	{
		printf( "ring2file: error getting local installation id; exiting!\n" );
		return -1;
	}
	if ( GetInst( "INST_WILDCARD", &InstWildCard ) != 0 )
	{
		printf( "ring2file: Invalid installation name <INST_WILDCARD>" );
		printf( "; exiting!\n" );
		return -1;
	}


// Look up module ids & message types earthworm.h tables
	if ( GetModId( "MOD_WILDCARD", &ModWildCard ) != 0 )
	{
		printf( "ring2file: Invalid module name <MOD_WILDCARD>; exiting!\n" );
		return -1;
	}
	if ( GetType( "TYPE_WILDCARD", &TypeWildCard ) != 0 )
	{
		printf( "ring2file: Message of type <TYPE_WILDCARD> not found in earthworm.d or earthworm_global.d; exiting!\n" );
		return -1;
	}
	/*
		if ( GetType( "TYPE_TRACEBUF", &TypeTraceBuf ) != 0 )
		{
		printf( "ring2file: Message of type <TYPE_TRACEBUF> not found in earthworm.d or earthworm_global.d; exiting!\n" );
		return -1;
		}
		if ( GetType( "TYPE_TRACEBUF2", &TypeTraceBuf2 ) != 0 )
		{
		printf( "ring2file: Message of type <TYPE_TRACEBUF2> not found in earthworm.d or earthworm_global.d; exiting!\n" );
		return -1;
		}
		if ( GetType( "TYPE_MSEED", &TypeMseed ) != 0 )
		{
		printf( "ring2file: Message of type <TYPE_MSEED> not found in earthworm.d or earthworm_global.d; warning!\n" );
		TypeMseed=0;
		}
		if ( GetType( "TYPE_TRACE_COMP_UA", &TypeCompUA ) != 0 )
		{
		printf( "ring2file: Message of type <TYPE_TRACE_COMP_UA> not found in earthworm.d or earthworm_global.d; exiting!\n" );
		return -1;
		}
		if ( GetType( "TYPE_TRACE2_COMP_UA", &TypeCompUA2 ) != 0 )
		{
		printf( "ring2file: Message of type <TYPE_TRACE2_COMP_UA> not found in earthworm.d or earthworm_global.d; exiting!\n" );
		return -1;
		}
		*/
	//   if ((argc == 3) || (argc == 4) || (argc == 5))
	//   {

// Initialize getlogo to all wildcards (get any message)
	getlogo[0].type   = TypeWildCard;
	getlogo[0].mod    = ModWildCard;
	getlogo[0].instid = InstWildCard;

	//	}
	//   if ((!no_flush && (argc == 3)) || (no_flush && (argc == 4))) 
	//	{
	verbose = 0;
	//   }


// Look up transport region keys earthworm.h tables
	if( ( inkey = GetKey(inRing) ) == -1 )
	{
		printf( "ring2file: Invalid input ring name <%s>; exiting!\n", inRing );
		return -1;
	}

// Attach to input ring
	tport_attach( &inRegion,  inkey );

	if(debug)
	{
		printf("Opening output file\n");
	}

// Open output file
	fd = open(fileName, O_RDWR|O_CREAT, 0664);

	if(debug)
	{
		printf("File <%s> opened\n", fileName);
	}

// Flush all old messages from the ring
	if (!no_flush) 
	{
		if(debug > 1)
		{
			printf("Flushing old messages from ring\n");
		}

		while( tport_copyfrom( &inRegion, getlogo, (short)1, &logo, &length, msg, MAX_SIZE, &inseq ) != GET_NONE ) 
		{
			if(debug > 1)
			{
				printf("Flushed message of length %ld\n", length);
				printf("inseq=%u\n", inseq);
				printf("msg= %s\n", msg);
				// printf("Sleep(50)\n");
				fflush(stdout);
				// sleep_ew(50);
			}
		}
		if(debug > 1)
		{
			printf("Finished flushing old messages\n");
			fflush(stdout);
		}
	}

	if(debug)
	{
		printf("Begin main while loop\n");
		fflush(stdout);
	}

// Main loop: read messages from ring and write to file
	while( tport_getflag( &inRegion ) != TERMINATE )
	{
		// printf("Sleep(200)\n");
		// fflush(stdout);
		// sleep_ew(200);

		gettimeofday(&tp, &tzp);

		if(debug > 1)
		{
			printf("Current time  = %ld\n", tp.tv_sec); 
		}

		if(fixedDuration && ((tp.tv_sec - tStart) > duration))
		{
			if(debug)
			{
				printf("%lld > %lld", (tp.tv_sec - tStart), duration); 
			}
			break;
		}

		rc = tport_copyfrom( &inRegion, getlogo, (short)1, &logo, &length, msg, MAX_SIZE, &inseq );

		if ( rc == GET_OK )
		{
		// Populate header with timestamp and message logo
			header.sec    = tp.tv_sec;
			header.usec   = tp.tv_usec;
			header.length = length;
			header.type   = logo.type;
			header.mod    = logo.mod;
			header.instid = logo.instid;


		// Write header + message
			write(fd, (char *)&header, sizeof(header));
			write(fd, msg, length);
		}

		else if ( rc == GET_NONE )
		{
			continue;
		}

		else if ( rc == GET_NOTRACK )
		{
			printf( "ring2file error in %s: no tracking for msg; i:%d m:%d t:%d seq:%d\n",
					inRing, (int)logo.instid, (int)logo.mod, (int)logo.type, (int)inseq );
		}

		else if ( rc == GET_MISS_LAPPED )
		{
			printf( "ring2file error in %s: msg(s) overwritten; i:%d m:%d t:%d seq:%d\n",
					inRing, (int)logo.instid, (int)logo.mod, (int)logo.type, (int)inseq );
		}

		else if ( rc == GET_MISS_SEQGAP )
		{
			printf( "ring2file error in %s: gap in msg sequence; i:%d m:%d t:%d seq:%d\n",
					inRing, (int)logo.instid, (int)logo.mod, (int)logo.type, (int)inseq );
		}

		else if ( rc == GET_TOOBIG )
		{
			printf( "ring2file error in %s: input msg too big; i:%d m:%d t:%d seq:%d\n",
					inRing, (int)logo.instid, (int)logo.mod, (int)logo.type, (int)inseq );
			continue;
		}
		if (verbose) {
			if (InstId == logo.instid && GetModIdName(logo.mod) != NULL) {
				strcpy(ModName, GetModIdName(logo.mod));
			} else {
				sprintf( ModName, "UnknownRemoteMod:%d", logo.mod);
			}
			if (GetInstName(logo.instid)==NULL) {
				sprintf(InstName, "UnknownInstID:%d", logo.instid);
			} else {
				strcpy(InstName, GetInstName(logo.instid));
			}
			if (GetTypeName(logo.type)==NULL) {
				sprintf(TypeName, "UnknownRemoteType:%d", logo.type);
			} else {
				strcpy(TypeName, GetTypeName(logo.type));
			}

			// Print message logo names, etc. to the screen
			printf( "%d Received %s %s %s <seq:%3d> <Length:%6ld>\n",
					(int)time (NULL), InstName, ModName, TypeName, (int)inseq, length );

		} else {
		// Backward compatibility 
		// Print message logo, etc. to the screen

			if(debug)
			{
				printf( "%d Received <inst:%3d> <mod:%3d> <type:%3d> <seq:%3d> <Length:%6ld>\n",
					(int)time (NULL),  (int)logo.instid, (int)logo.mod, 
					(int)logo.type, (int)inseq, length );
			}
		}
		fflush( stdout );

		msg[length] = '\0'; // Null-terminate message
		printf( "%s\n", msg );

		fflush( stdout );
	}

// Detach from shared memory region and terminate
	tport_detach( &inRegion );
	close(fd);

	return 0;
}

