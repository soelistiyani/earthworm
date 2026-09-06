// FILE: sr2ew.c
// COPYRIGHT: (c), Symmetric Research, 2016-2018
//
// This is an interface utility to connect the Earthworm seismic management
// system with the SR USBxCH data acquisition pipeline utilities.  It sends BIN
// data from a SR USBxCH 24 bit system to an Earthworm WAVE_RING buffer as
// TYPE_TRACEBUF2 messages.
//
// To use it, have the Earthworm startstop module run this program.  It will
// run a batch file which starts a chain of SR programs that acquire and
// process data which is passed via named pipes to this utility which outputs
// it to the Earthworm WAVE_RING.  The program chain includes:
//
//        Blast    to acquire data in PAK format
//        Pak2Bin  to convert from PAK to BIN format
//        Interp   to resample to a simple, second aligned timebase
//        sr2ew    to send it to an Earthworm ring
//
// The most recent version of sr2ew can be downloaded for free from the
// symres.com website as it is included as part of the USBxCH software in the
// subdirectory:
//      Examples\501 Special App - Seismic - SR Bin pipeline to USGS Earthworm
//
// changes:
//
// Revision 2.0  2018/07/09      W.Tucker:
//       this one file both starts the pipeline and processes the BIN packets
// Revision 1.0  2016/03/12      W.Tucker:
//       first version
// 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>       // for strncpy
#include <time.h>         // for time_t, struct tm, time, localtime
#include <limits.h>       // for INT_MIN, INT_MAX

#if defined( SROS_WINDOWS )
#include <direct.h>       // for mkdir, chdir
#include <process.h>      // for getpid
#define OsChDir _chdir
#define OsGetPid _getpid
#define SR_DIRSLASH '\\'
#elif defined( SROS_LINUX ) || defined( SROS_ANDROID )
#include <sys/types.h>    // for pid_t
#include <sys/stat.h>     // for mkdir, chdir
#include <unistd.h>       // for getpid
#include <errno.h>        // for errno
#define OsChDir chdir
#define OsGetPid getpid
#define SR_DIRSLASH '/'
#endif  // SROS_xxxxx

#include "earthworm.h"               // inst, mod/ring globals, logit, etc
#include "earthworm_complex_funcs.h" // needed in earthworm.h
#include "earthworm_defs.h"          // needed in earthworm.h
#include "earthworm_simple_funcs.h"  // needed in earthworm.h
#include "platform.h"                // needed in earthworm_complex_funcs.h
#include "kom.h"                     // for EW configuration parsing k_xxx
#include "transport.h"               // ring transport utils, tport, SHM_INFO, MSG_LOGO, etc
#include "trace_buf.h"               // tracebuf waveform structure

#include "CustomPipeLib.h"           // for prototypes, BIN, and other SR defines



// Program copyright and screen title:

char *Copyright =

                "\n\nsr2ew.exe : Copyright (c), Symmetric Research, 2016-2018\n\n";

char *ScreenTitle =

                "USBxCH sr2ew : (c) SR 2016 : Rev {2018/07/17}\n\n";


// Defines.

// This code is designed to support both v7.x which knows about the new
// TYPE_TRACEBUF2 and v6.3 which doesn't.  If the new TRACE2 defines exist
// all is OK, otherwise, we just dummy them out to match the old versions.

#if defined( TRACE2_VERSION0 )
#define TRACE2_OK        1
#else
#define TRACE2_OK        0
#define TRACE2_CHAN_LEN  4
#define TRACE2_LOC_LEN   3
#define TRACE2_VERSION0  '2'   // version[0] for TYPE_TRACEBUF2
#define TRACE2_VERSION1  '0'   // version[1] for TYPE_TRACEBUF2
#define LOC_NULL_STRING  "--"  // NULL string for location code field
#endif

#define TRACE_POS_LOC    (TRACE2_CHAN_LEN)
#define TRACE_POS_VER    (TRACE_POS_LOC+TRACE2_LOC_LEN)

#define MAX_TRACEDAT_SIZ (MAX_TRACEBUF_SIZ-sizeof(TRACE_HEADER))
#define MAX_TRACEDAT_PTS (MAX_TRACEDAT_SIZ/sizeof(int))


//#define TRACE_STA_LEN    7  /* SEED 5+NULL */
//#define TRACE_CHAN_LEN   9  /* SEED 3+NULL (component) plus pad for loc+version */
//#define TRACE_NET_LEN    9  /* SEED 2+NULL */
//#define TRACE_LOC_LEN    3  /* SEED 2+NULL */

typedef struct _SCNL {             // Channel information structure
        char sta[TRACE_STA_LEN];
        char comp[TRACE_CHAN_LEN]; // Older value has larger dimension
        char net[TRACE_NET_LEN];
        char loc[TRACE_LOC_LEN];
        char ver[2];
        int  pin;
        } SCNL;


// Defines and typedefs.

#define MAXSTR                256      // Maximum dimension for some strings
#define QUALITY_OK            0x0      // See trace_buf.h for other codes

#define MAX_CHAN_ANA            8      // Maximum number of analog channels
#define MAX_CHAN_DIG            4      // Maximum number of digital channels
#define MAX_CHAN_OTHR           4      // Maximum number of other channels
#define MAX_EW_CHAN            ( MAX_CHAN_ANA + MAX_CHAN_DIG + MAX_CHAN_OTHR )

#define EW_ERROR_BASE_USBXCH  100      // Convert USBxCH_ERROR_XXX to EW ierr
#define EW_ERROR_FILE_OPEN     50
#define EW_ERROR_BAD_DATA      51

#define EW_TYPE_TRACEBUF        1
#define EW_TYPE_TRACEBUF2       2

#define LOG_NONE                0
#define LOG_EWONLY              1
#define LOG_SRONLY              2
#define LOG_BOTH                3

#define NSPERSEC       1000000000

#define MIN_GOOD_TIME  1454695792       // minimum seconds to consider time is ok

#define PROGRAMNAME "sr2ew"


// Global variables:

        // Things to read or derive from the earthworm .d config file.
        
        char  MyModName[MAX_MOD_STR];   // use this module name/id
        char  RingName[MAX_RING_STR];   // name of transport ring for i/o
        char  OutputMsgTypeName[MAXSTR];// name of tracebuf message type
                        
        int   HeartBeatInt;             // seconds between heartbeats
        int   OutputMsgType;            // EW_TYPE_TRACEBUF or EW_TYPE_TRACEBUF2
        int   LogSwitch;                // controls logging, 0 off, 1 on
        int   Verbosity;                // 0,1,2,3 = few, some, many... logits

        SCNL  ChanList[MAX_EW_CHAN];            // Chan info:site,comp,net,pinno
        char  ChanScnlStr[MAX_EW_CHAN][MAXSTR]; // channel SCNL string
//FIX need these ???        
        int   ChannelEnable[MAX_EW_CHAN];       // channel 


        // Things to look up in the earthworm tables.

        long          RingKey;          // key of transport ring for i/o
        unsigned char InstId;           // local installation id
        unsigned char MyModId;          // module id for this program
        unsigned char TypeHeartBeat;    // msg type id for heartbeats
        unsigned char TypeError;        // msg type id for errors
        unsigned char TypeTraceBuf;     // msg type id for old tracebuf
        unsigned char TypeTraceBuf2;    // msg type id for new tracebuf2


        // Things set or computed in the code.

        char ConfigFileName[MAXSTR];    // Name of .d file with config info
        char EwParamDir[MAXSTR];        // Earthworm parameter directory
        char EwDataDir[MAXSTR];         // Earthworm data directory
        char YmdHmsDir[MAXSTR];         // SR YMD HMS data directory

	int SummaryInterval;            // sec in log between summary reports, 0 -> none
	int SummaryCount;               // num sec so far in general summary
	int SummaryMin[MAX_EW_CHAN];    // min data value per channel
	int SummaryMax[MAX_EW_CHAN];    // max data value per channel

        SHM_INFO Region;                // shared memory region to use for i/o
        MSG_LOGO LogoHeartBeat;         // HeartBeat msg header with module,type,instid
        MSG_LOGO LogoTrace;             // TraceBuf  msg header with module,type,instid
        MSG_LOGO LogoError;             // Error     msg header with module,type,instid
        int      MyPid;                 // for restarts by startstop
        time_t   TimeNow;               // current time in sec since 1970
        time_t   TimeLastBeat;          // time last heartbeat was sent

        int      SampleRateInt;         // Integer sampling rate MUST match Interp value
        double   SampleRate;            // Sampling rate as sps
        int      NumReadySamples;       // Number of samples in current tracebuf
        int      MaxReadySamples;       // Max number of samples to send in tracebuf

        char         *TraceBuffer[MAX_EW_CHAN]; // tracebuf message buffer
	int          *TraceDat[MAX_EW_CHAN];    // where in TraceBuffer the data starts
        TRACE_HEADER *TraceHead[MAX_EW_CHAN];   // where in TraceBuffer old header starts

        SRPIPEHANDLE hInputPipe;        // handle to named pipe
        
        char MsgBuffer[MAXSTR];         // temporary string buffer



// Prototypes of functions in this file.

 int main( int argc, char **argv );
void Initialize( int argc, char **argv);
void PrepareDirectories( void );
void EwGetEnvDir( void );
void SetDefaults( void );
void EwLookup( void );
void EwReadConfig( void );
void EwAllocateTraceBuf( void );
void PrintRunTimeSummary( void );
void RunPipelineCmd( char *PipeCmd );
void EwConnect( void );
void OutputPoint( BIN *Pt );
void SendTraces( void );
void SummaryUpdate( int ichan, int Value );
void SummaryReport( void );
void EwSendHeartbeat( void );

//FIX Never calling this next one !!!
void EwError( int ierr, char *note );

void EwCheckTerminate( void );
int  CleanExit( int );
void LogMessage( char *msg, int level, int err );

// OS specific prototypes (selected with SROS_WINDOWS or SROS_LINUX when compiling) 
int  OsMkDir( char *DirectoryName );
void OsClearSysError( void );



//------------------------------------------------------------------------------
// ROUTINE: main()
// PURPOSE: Main entry point for sr2ew.  This program provides a connection
//          between Symmetric Research 24 bit USBxCH A/D (DAQ) systems and the
//          Earthworm program maintained by ISTI.
//
//          It handles command line parameters and reads ini parameters from a
//          typical Earthworm .d file.  It sets up a connection to an Earthworm
//          ring (shared memory region) and calls Sr2EwPipeline.bat to start
//          an SR acquisition pipeline.  After that, it behaves like a standard
//          SR BIN pipeline utility which opens its input pipe, reads BIN 
//          records and processes them, sending periodic heartbeat and TraceBuf
//          messages to the EW ring.
//
//          Users can edit the Sr2EwPipeline.bat as appropriate for their
//          application but must ensure sr2ew is the last pipeline utility 
//          called.  Edit the Earthworm startstop to call this sr2ew exe.  Type
//          startstop to start and type quit in the Earthworm startstop window
//          to exit.
//------------------------------------------------------------------------------
int main( int argc, char *argv[] ) {

        BIN  Pt;
        char InputPipeName[MAXSTR];
        char PipelineCmd[MAXSTR];


        // Start logging, get some EW environment variables, make and change
        // to a YMDHMS directory, read parameters from a .d configuration file,
        // allocate trace buffers and display config info.

        Initialize( argc, argv );
        
        
        
	// Start SR to EW pipeline utility chain.

	sprintf( PipelineCmd, "%sSr2EwPipeline", EwParamDir );
	RunPipelineCmd( PipelineCmd );
	

	// Form name and open the input pipe.

        sprintf( InputPipeName, "%s%s", REQUIRED_PIPE_PREFIX, PROGRAMNAME );
	hInputPipe = CustomOpenInputPipe( InputPipeName );

	  // >> careful programmers might add error checking to confirm the
	  // >> input pipes was successfully opened ...


        // Connect to Earthworm only AFTER the input pipe has connected.
        // Otherwise, EW might time this exe out if the pipe connect is too slow.

        EwConnect();

        


	// While there is data...
	// Infinite loop reading BIN packets from the input pipe and
	// sending data to Earthworm.  Exits if the other end of the
	// input pipe is closed or a stop signal is received.

        LogMessage( "Begin sending to Earthworm ...\n", 0, 0 );

	
	while ( CustomReadInputPipe( hInputPipe, &Pt ) ) {

                EwSendHeartbeat( );     // let EW know we are still alive
		EwCheckTerminate( );	// Terminates if stop signal received

                OutputPoint( &Pt );	// Output to Earthworm
		}


	// The input pipe has failed ( upstream is probably closing down ),
	// announce ending and close all gracefully ...

	LogMessage( "sr2ew stopping ...\n", 0, 0 );

	CleanExit( 0 );
        
        

        // Return from main:
        //
        // Linux *requires* main to be of type integer with return 0 for
        // success.  Windows doesn't care ...
        //
        //

        exit( 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: Initialize
// PURPOSE: Start EW logging and announce program.  Get some EW directories from
//          environment variables, then make and change to a YMDHMS directory.
//          Set some variable defaults then read parameters from the .d config
//          file and look up other EW items. Also allocate space for trace
//          buffers and print parameter info.
//------------------------------------------------------------------------------
void Initialize( int argc, char **argv ) {

        // Do initialization tasks.

        logit_init( PROGRAMNAME, 0, 256, 1 );   // init EW logging file

        LogMessage( "Starting  Initialize\n", 3, 0 );
        
        LogMessage( ScreenTitle, 0, 0 );        // announce program
        LogMessage( "sr2ew starting ...\n", 0, 0 ); // was logit( "t", ...

	PrepareDirectories( );                  // make + change to YMDHMS dir

        CustomReportOpen( PROGRAMNAME );        // open SR report file

        SetDefaults( );                         // init variables 

        EwReadConfig( );                        // read parameters from .d file

        EwLookup( );                            // look up other EW items

        EwAllocateTraceBuf( );                  // allocate trace buffers

        PrintRunTimeSummary( );                 // show parameter info

        
        LogMessage( "Leaving  Initialize\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: PrepareDirectories
// PURPOSE: Get the Earthworm parameter and data directories from environment
//          variables.  Make a directory under the data directory with the
//          current date and time represented as YMDHMS.  Then change the 
//          working directory of this executable to this newly created
//          YMDHMS directory so the SR rpt files and data will be placed there.
//------------------------------------------------------------------------------
void PrepareDirectories( void ) {

	time_t UtcTimeInSecondsSince_1970;
	struct tm *LocTime;
        int SysRet, err;

        LogMessage( "Starting PrepareDirectories\n", 3, 0 );


	// Get the current time.

	time( &UtcTimeInSecondsSince_1970 );
        LocTime = localtime( &UtcTimeInSecondsSince_1970 );



        // Get Earthworm directories from environment variables.
        // Note: They are expected to include the proper trailing slash.

        EwGetEnvDir( );



        // Prepare the full directory name composed of parent dir
        // with slash, YYYY-MM-DD-at-HH-MM-SS, and trailing slash
	//
	//    YMDHMS = ( year, month, day, hours, minutes, seconds)
	//
	//

	sprintf( YmdHmsDir, "%s%04d-%02d-%02d-at-%02d-%02d-%02d%c",

		 EwDataDir,
		 LocTime->tm_year+1900,
		 LocTime->tm_mon+1,
		 LocTime->tm_mday,
		 LocTime->tm_hour,
		 LocTime->tm_min,
                 LocTime->tm_sec,
                 SR_DIRSLASH

	       );

        sprintf( MsgBuffer, "YMDHMS directory is ...\n%s\n", YmdHmsDir );
	LogMessage( MsgBuffer, 1, 0 );


	// Make the directory.

	SysRet = OsMkDir( YmdHmsDir );
	if ( SysRet == 0 ) {
		sprintf( MsgBuffer, "Made directory %s\n", YmdHmsDir );
		LogMessage( MsgBuffer, 1, 0 );
		}
	else {
		err = errno;
		sprintf( MsgBuffer, "Failed to make directory %s, error %d\n",
			 YmdHmsDir, err );
		LogMessage( MsgBuffer, 0, 1 );
		}


	// Change to it.

	SysRet = OsChDir( YmdHmsDir );
	if ( SysRet == 0 ) {
		sprintf( MsgBuffer, "Change working directory to %s\n", YmdHmsDir );
		LogMessage( MsgBuffer, 1, 0 );
		}
	else {
		err = errno;
		sprintf( MsgBuffer, "Failed to change working dir to %s, error %d\n",
			 YmdHmsDir, err );
		LogMessage( MsgBuffer, 0, 1 );
                }

        LogMessage( "Leaving  PrepareDirectories\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwGetEnvDir
// PURPOSE: Get Earthworm parameter and data directories from the environment
//          variables.  These are set when ew_nt.bat or ew_linux.bash is run
//          and are expected to include the proper trailing slash.
//------------------------------------------------------------------------------
void EwGetEnvDir( void ) {

        char *EnvValue;

        LogMessage( "Starting EwGetEnvDir\n", 3, 0 );

        
        // Get EW parameter directory with trailing slash.

        EnvValue = getenv( "EW_PARAMS" );
        if ( EnvValue == NULL ) {
                LogMessage( "Cannot get EW param directory. Exiting.\n", 0, 1 );
                }
        else {
                sprintf( MsgBuffer, "Got EW param dir of %s\n", EnvValue );
                LogMessage( MsgBuffer, 1, 0 );
                }
	sprintf( EwParamDir, "%s", EnvValue );
        

        // Get EW data directory with trailing slash.

        EnvValue = getenv( "EW_DATA_DIR" );
        if ( EnvValue == NULL ) {
                LogMessage( "Cannot get EW data directory. Exiting.\n", 0, 1 );
                }
        else {
                sprintf( MsgBuffer, "Got EW data dir of %s\n", EnvValue );
                LogMessage( MsgBuffer, 1, 0 );
                }
        sprintf( EwDataDir, "%s", EnvValue );


        LogMessage( "Leaving  EwGetEnvDir\n", 3, 0 );
}
        
//------------------------------------------------------------------------------
// ROUTINE: SetDefaults
// PURPOSE: Set some default values and get the process ID.
//------------------------------------------------------------------------------
void SetDefaults( void ) {

        int ichan;

        LogMessage( "Starting SetDefaults\n", 3, 0 );


        // Initialize some values.

        HeartBeatInt    = 60L;                   // in seconds, 0 -> none
        OutputMsgType   = EW_TYPE_TRACEBUF2;     // new style tracebuf
        LogSwitch       = LOG_NONE;              // no logfile written
        Verbosity       = 0;                     // 0 -> no debugging messages
        SampleRateInt   = 100;                   // in Hz, MUST match interp rate
        SummaryInterval = 0;                     // in seconds, 0 -> none
        SummaryCount    = 0;                     // 0 so far
        Region.addr     = NULL;                  // EW region



        // Set additional defaults.

        sprintf( ConfigFileName,    "%s%s.d", EwParamDir, PROGRAMNAME );
        sprintf( MyModName,         "%s",     "MOD_SR2EW"             );
        sprintf( RingName,          "%s",     "WAVE_RING"             );
        sprintf( OutputMsgTypeName, "%s",     "TYPE_TRACEBUF2"        );

        

        // Set per channel defaults.

        for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {

                sprintf( ChanList[ichan].sta, "CH%02d", ichan );
                sprintf( ChanList[ichan].comp, "xxx" );
                sprintf( ChanList[ichan].net, "SR" );
                sprintf( ChanList[ichan].loc, LOC_NULL_STRING );
                
                ChanList[ichan].ver[0] = TRACE2_VERSION0;
                ChanList[ichan].ver[1] = TRACE2_VERSION1;
                ChanList[ichan].pin    = ichan;
                
                SummaryMin[ichan] = INT_MAX;
                SummaryMax[ichan] = INT_MIN;
                }



        // Get process id for sr2ew.

        MyPid = OsGetPid();


        LogMessage( "Leaving  SetDefaults\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwReadConfig
// PURPOSE: Read all the pertinent configuration info from the associated .d
//          file and perform validity checks.
//------------------------------------------------------------------------------
void EwReadConfig( void ) {

        int  ichan;
        char *com, *str;

        LogMessage( "Starting EwReadConfig\n", 3, 0 );

        sprintf( MsgBuffer, "ConfigFileName is |%s|\n", ConfigFileName );
        LogMessage( MsgBuffer, 0, 0 );

        
        // Open .d configuration file.

        if ( ! k_open( ConfigFileName ) ) {
                sprintf( MsgBuffer, "Error opening command file <%s>; exiting!\n",
                         ConfigFileName );
                LogMessage( MsgBuffer, 0, 1 );
                }
        
        // Process config file.

        while ( k_rd() ) {      // read next line from active file

                com = k_str();  // get the first token from line

                // Check for special lines.

                if ( !com )           continue; // ignore blank lines
                if ( com[0] == '#' )  continue; // ignore comment lines
                if ( com[0] == ';' )  continue; // ignore .ini style comment too
                if ( com[0] == '@' )            // don't allow nested config files
                        LogMessage( "Nested configuration files NOT allowed\n", 0, 1 );
		if ( k_its( "=" ) )    { ; }    // skip = so .ini style files can be used


                // Process current command.

                if ( k_its( "Verbosity" ) ) {
                        LogMessage( "Found Verbosity\n", 1, 0 );
                        Verbosity = k_int();
                        }
                else if ( k_its( "MyModuleId" ) ) {
                        LogMessage( "Found MyModuleId\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( MyModName, str, MAX_MOD_STR );
                        }
                else if ( k_its( "RingName" ) ) {
                        LogMessage( "Found RingName\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( RingName, str, MAX_RING_STR );
                        }
                else if ( k_its( "OutputMsgType" ) ) {
                        LogMessage( "Found OutputMsgType\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( OutputMsgTypeName, str, MAXSTR );
                        }
                else if ( k_its( "HeartBeatInt" ) ) {
                        LogMessage( "Found HeartBeatInt\n", 1, 0 );
                        HeartBeatInt = k_int();
                        }
                else if ( k_its( "LogSwitch" ) ) {
                        LogMessage( "Found LogSwitch\n", 1, 0 );
                        LogSwitch = k_int();
                        }
                else if ( k_its( "SummaryInterval" ) ) {
                        LogMessage( "Found SummaryInterval\n", 1, 0 );
                        SummaryInterval = k_int();
                        }
                else if ( k_its( "Channel0" ) ) {
//FIX
                        LogMessage( "Found Channel0\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[0], str, MAXSTR );
                        }
                else if ( k_its( "Channel1" ) ) {
                        LogMessage( "Found Channel1\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[1], str, MAXSTR );
                        }
                else if ( k_its( "Channel2" ) ) {
                        LogMessage( "Found Channel2\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[2], str, MAXSTR );
                        }
                else if ( k_its( "Channel3" ) ) {
                        LogMessage( "Found Channel3\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[3], str, MAXSTR );
                        }
                else if ( k_its( "Channel4" ) ) {
                        LogMessage( "Found Channel4\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[4], str, MAXSTR );
                        }
                else if ( k_its( "Channel5" ) ) {
                        LogMessage( "Found Channel5\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[5], str, MAXSTR );
                        }
                else if ( k_its( "Channel6" ) ) {
                        LogMessage( "Found Channel6\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[6], str, MAXSTR );
                        }
                else if ( k_its( "Channel7" ) ) {
                        LogMessage( "Found Channel7\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[7], str, MAXSTR );
                        }
                else if ( k_its( "Channel8" ) ) {
                        LogMessage( "Found Channel8\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[8], str, MAXSTR );
                        }
                else if ( k_its( "Channel9" ) ) {
                        LogMessage( "Found Channel9\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[9], str, MAXSTR );
                        }
                else if ( k_its( "Channel10" ) ) {
                        LogMessage( "Found Channel10\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[10], str, MAXSTR );
                        }
                else if ( k_its( "Channel11" ) ) {
                        LogMessage( "Found Channel11\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[11], str, MAXSTR );
                        }
                else if ( k_its( "Channel12" ) ) {
                        LogMessage( "Found Channel12\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[12], str, MAXSTR );
                        }
                else if ( k_its( "Channel13" ) ) {
                        LogMessage( "Found Channel13\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[13], str, MAXSTR );
                        }
                else if ( k_its( "Channel14" ) ) {
                        LogMessage( "Found Channel14\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[14], str, MAXSTR );
                        }
                else if ( k_its( "Channel15" ) ) {
                        LogMessage( "Found Channel15\n", 1, 0 );
                        str = k_str();
                        if ( str ) strncpy( ChanScnlStr[15], str, MAXSTR );
                        }
                else if ( k_its( "SampleRate" ) ) {
                        LogMessage( "Found SampleRate\n", 1, 0 );
                        SampleRateInt = k_int();
                        }
                else  {
                        sprintf( MsgBuffer, "<%s> Unknown command in <%s>.\n",
                                 com, ConfigFileName );
                        LogMessage( MsgBuffer, 0, 0 );
                        continue;
                        }

                // Exit if bad command found.

                if ( k_err() ) {
                        sprintf( MsgBuffer, "Bad <%s> command in <%s>.\n",
                                 com, ConfigFileName );
                        LogMessage( MsgBuffer, 0, 1 );
                        }

                } // end while k_rd

        // Close configuration file.

        k_close();



        // Now that we know what the user really wants, set up the log files
        // appropriately.  SR CustomReport Print and Close calls ignore the
        // request once the log has been closed.

        if ( LogSwitch == LOG_NONE) {
                logit_init( PROGRAMNAME, 0, 256, 0 );   // re-init EW log to none
                CustomReportClose( );                   // close SR log
                }
        else if ( LogSwitch == LOG_EWONLY ) {
                CustomReportClose( );                   // close SR log
                }
        else if ( LogSwitch == LOG_SRONLY ) {
                logit_init( PROGRAMNAME, 0, 256, 0 );   // re-init EW log to none
                }
        // else ( LogSwitch == LOG_BOTH )               // all stay as is



        // Fill in channel structure defaults from ini strings.

        for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {

                if ( strlen( ChanScnlStr[ichan] ) < 14 )
                        continue;
                
                sscanf( ChanScnlStr[ichan], "%4s %3s %2s %2s",
                          &(ChanList[ichan].sta[0]),
                          &(ChanList[ichan].comp[0]),
                          &(ChanList[ichan].net[0]),
                          &(ChanList[ichan].loc[0])
                      );

                }



        // Set additional values based on integer sample rate.  Send one second
        // of data or less if that would be too much for the tracebuf array.
        
        SampleRate = (double)SampleRateInt; // rate as a double

        MaxReadySamples = SampleRateInt;
        if ( MaxReadySamples > MAX_TRACEDAT_PTS )
                MaxReadySamples = MAX_TRACEDAT_PTS;



        // Fill in OutputMsgType from related ini string.

        if ( strcmp( OutputMsgTypeName, "TYPE_TRACEBUF2" ) == 0 ) {
                OutputMsgType = EW_TYPE_TRACEBUF2;
                if ( ! TRACE2_OK )
                        LogMessage( "TYPE_TRACEBUF2 selected but NOT available\n", 0, 1 );
                }

        else if ( strcmp( OutputMsgTypeName, "TYPE_TRACEBUF" ) == 0 )
                OutputMsgType = EW_TYPE_TRACEBUF;

        else {
                LogMessage( "OutputMsgType MUST be TYPE_TRACEBUF or TYPE_TRACEBUF2\n", 0, 1 );
                }

        LogMessage( "Leaving  EwReadConfig\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwLookup
// PURPOSE: Look up important info from earthworm.d and earthworm_global.d
//          tables including InstId, ModId, RingKey, and several message types.
//          InstId and most common message types come from earthworm_global.d
//          while earthworm.d has ModId, RingKey and the remaining message types.
//------------------------------------------------------------------------------
void EwLookup( void ) {

        LogMessage( "Starting EwLookup\n", 3, 0 );

        // Look up installation.

        if ( GetLocalInst( &InstId ) != 0 ) {
                LogMessage( "Error getting local installation id; exiting!\n", 0, 1 );
                // logit( "e",
                // exit( -1 );
                }


        // Look up module.

        if ( GetModId( MyModName, &MyModId ) != 0 ) {
                sprintf( MsgBuffer, "Invalid module name <%s>; exiting!\n",
                         MyModName );
                LogMessage( MsgBuffer, 0, 1 );
                // logit( "e",
                // exit( -1 );
                }


        // Look up ring, key to shared memory region.

        if ( ( RingKey = GetKey( RingName ) ) == -1 ) {
                sprintf( MsgBuffer, "Invalid ring name <%s>; exiting!\n",
                         RingName );
                LogMessage( MsgBuffer, 0, 1 );
                // logit( "e",
                // exit( -1 );
                }


        // Look up message types.

        if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
                LogMessage( "Invalid message type <TYPE_HEARTBEAT>; exiting!\n", 0, 1 );
                // logit( "e",
                // exit( -1 );
                }

        if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
                LogMessage( "Invalid message type <TYPE_ERROR>; exiting!\n", 0, 1 );
                // logit( "e",
                // exit( -1 );
                }

        if ( GetType( "TYPE_TRACEBUF", &TypeTraceBuf ) != 0 ) {
                LogMessage( "Invalid message type <TYPE_TRACEBUF>; exiting!\n", 0, 1 );
                // logit( "e",
                // exit( -1 );
                }

        if ( TRACE2_OK ) {
                if ( GetType( "TYPE_TRACEBUF2", &TypeTraceBuf2 ) != 0 ) {
                        LogMessage( "Invalid message type <TYPE_TRACEBUF2>; exiting!\n", 0, 1 );
                        // logit( "e",
                        // exit( -1 );
                        }
                }
        else
                TypeTraceBuf2 = TypeError;


        // Set up Logo structures.

        LogoHeartBeat.instid = InstId;
        LogoHeartBeat.mod    = MyModId;
        LogoHeartBeat.type   = TypeHeartBeat;

        LogoError.instid     = InstId;
        LogoError.mod        = MyModId;
        LogoError.type       = TypeError;

        LogoTrace.instid     = InstId;
        LogoTrace.mod        = MyModId;
        LogoTrace.type       = TypeTraceBuf2;           // new style is default
        
        if ( OutputMsgType == EW_TYPE_TRACEBUF )
                LogoTrace.type = TypeTraceBuf;          // old style requested
        
        LogMessage( "Leaving  EwLookup\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwAllocateTraceBuf
// PURPOSE: Allocate trace buffer including space for both header and data.
//          Also initialize the header part.
//------------------------------------------------------------------------------
void EwAllocateTraceBuf( void ) {

        int ichan;

        LogMessage( "Starting EwAllocateTraceBuf\n", 3, 0 );

        // Allocate space for the output trace buffers including both header + data.

        for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {

                TraceBuffer[ichan] = (char *) malloc( MAX_TRACEBUF_SIZ ); // alloc max size

                if ( TraceBuffer[ichan] == NULL )
                        LogMessage( "Cannot allocate all trace buffers\n", 0, 1 );


                // Set up pointers to the tracebuf header and data parts.

                TraceHead[ichan] = (TRACE_HEADER *) TraceBuffer[ichan];


                // Set output values common to all channels.

                TraceHead[ichan]->nsamp      = MAX_TRACEDAT_PTS;   // number of samples in message
                TraceHead[ichan]->samprate   = SampleRate;         // sample rate; nominal
                TraceHead[ichan]->quality[0] = QUALITY_OK;         // one bit per condition
                TraceHead[ichan]->quality[1] = QUALITY_OK;         // one bit per condition
                TraceHead[ichan]->pad[0]     = 'S';
                TraceHead[ichan]->pad[1]     = 'R';
                strncpy( TraceHead[ichan]->datatype, "i4", 3 );    // data format code (intel 4byte int)

                if ( OutputMsgType == EW_TYPE_TRACEBUF2 ) {     // new tracebuf2 version info

                        TraceHead[ichan]->chan[TRACE_POS_VER]   = TRACE2_VERSION0;
                        TraceHead[ichan]->chan[TRACE_POS_VER+1] = TRACE2_VERSION1;
                        }
                else { // ( OutputMsgType == EW_TYPE_TRACEBUF ) // old default loc + version info

                        strncpy( &TraceHead[ichan]->chan[TRACE_POS_LOC], LOC_NULL_STRING, 3 );
                        TraceHead[ichan]->chan[TRACE_POS_VER]   = '1';
                        TraceHead[ichan]->chan[TRACE_POS_VER+1] = '0';
                        }

                TraceHead[ichan]->starttime  = 0.0L;
                TraceHead[ichan]->endtime    = 0.0L;


                // Fill the trace buffer header.

                strncpy( TraceHead[ichan]->sta, ChanList[ichan].sta,  TRACE_STA_LEN );  // Site
                strncpy( TraceHead[ichan]->net, ChanList[ichan].net,  TRACE_NET_LEN );  // Network
                if ( OutputMsgType == EW_TYPE_TRACEBUF2 ) {

                        strncpy(  TraceHead[ichan]->chan,                ChanList[ichan].comp, TRACE2_CHAN_LEN );
                        strncpy( &TraceHead[ichan]->chan[TRACE_POS_LOC], ChanList[ichan].loc,  TRACE2_LOC_LEN );
                        }
                else // ( OutputMsgType == EW_TYPE_TRACEBUF )
                        strncpy( TraceHead[ichan]->chan, ChanList[ichan].comp, TRACE_CHAN_LEN );

                TraceHead[ichan]->pinno = ChanList[ichan].pin;                          // Pin num
                }

        NumReadySamples = 0;

        LogMessage( "Leaving  EwAllocateTraceBuf\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: PrintRunTimeSummary
// PURPOSE: Format and print parameter info.
//------------------------------------------------------------------------------
void PrintRunTimeSummary( void ) {

        int ichan;

        LogMessage( "PrintRunTimeSummary starting ...\n", 3, 0 );
 
        // Print out filled values.

        LogMessage( ScreenTitle, 0, 0 );        // announce program
	
        sprintf( MsgBuffer, "  %s runtime summary ... \n\n", PROGRAMNAME       ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "     EW Param Dir:  %s\n",      EwParamDir        ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "      EW Data Dir:  %s\n",      EwDataDir         ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "      YMDHMS  Dir:  %s\n",      YmdHmsDir         ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "   ConfigFileName:  %s\n\n",    ConfigFileName    ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "       MyModuleId:  %s\n",      MyModName         ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "         RingName:  %s\n",      RingName          ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "OutputMsgTypeName:  %s\n",      OutputMsgTypeName ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "    OutputMsgType:  %d\n",      OutputMsgType     ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "     HeartBeatInt:  %d\n",      HeartBeatInt      ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "        LogSwitch:  %d\n",      LogSwitch         ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "        Verbosity:  %d\n\n",    Verbosity         ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "              PID:  %d\n\n",    MyPid             ); LogMessage( MsgBuffer, 0, 0 );
        sprintf( MsgBuffer, "          RingKey:  %ld\n",     RingKey           ); LogMessage( MsgBuffer, 0, 0 );

        for ( ichan = 0 ; ichan < 10 ; ichan++ ) {
                sprintf( MsgBuffer, "         Channel%1d:   %4s %3s %2s %2s\n", ichan, 
                         ChanList[ichan].sta,
                         ChanList[ichan].comp,
                         ChanList[ichan].net,
                         ChanList[ichan].loc );
                LogMessage( MsgBuffer, 0, 0 );
                }
        
        for ( ichan = 10 ; ichan < 16 ; ichan++ ) {
                sprintf( MsgBuffer, "         Channel%2d:  %4s %3s %2s %2s\n", ichan, 
                         ChanList[ichan].sta,
                         ChanList[ichan].comp,
                         ChanList[ichan].net,
                         ChanList[ichan].loc );
                LogMessage( MsgBuffer, 0, 0 );
                }
	LogMessage( "\n", 0, 0 );
	LogMessage( "\n", 0, 0 );

	LogMessage( "PrintRunTimeSummary ending ...\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: RunPipelineCmd
// PURPOSE: Run the specified SR to EW pipeline script.  Users may edit the
//          pipeline startup script file as needed, but the last pipeline 
//          utility should ALWAYS have sr2ew as its output location.  Exits on
//          failure.
//------------------------------------------------------------------------------
void RunPipelineCmd( char *PipeCmd ) {

	int SysRet;

        LogMessage( "Starting RunPipelineCmd ...\n", 3, 0 );

        SysRet = system( PipeCmd );
	if ( SysRet == 0 ) {
		LogMessage( "Running pipe script\n", 1, 0 );
		sprintf( MsgBuffer, "\t\t\t%s ...\n", PipeCmd );
		LogMessage( MsgBuffer, 1, 0 );
		}
	else {
		LogMessage( "Failed to run pipe script, check log files\n", 0, 0 );
		sprintf( MsgBuffer, "Script = %s, System Return = %d\n",
			 PipeCmd, SysRet );
		LogMessage( MsgBuffer, 0, 1 );
                }

        LogMessage( "Leaving  RunPipelineCmd ...\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwConnect
// PURPOSE: Attach to the Earthworm ring and send an initial heartbeat to tell
//          EW that we are alive.  Do this step only AFTER the input pipe has
//          connected because that might take awhile if there are many utilities
//          ahead of it in the pipeline.  And we don't want EW to time out this
//          exe if the pipe connect is slow.
//------------------------------------------------------------------------------
void EwConnect( void ) {

        LogMessage( "Starting EwConnect\n", 3, 0 );

        
        // Attach to Earthworm ring, then clear system error generated by
        // trying to create an existing file.
        
        tport_attach( &Region, RingKey );
        sprintf( MsgBuffer, "Attached to public memory region %s: %ld\n",
                 RingName, RingKey );
        LogMessage( MsgBuffer, 1, 0 );

	OsClearSysError( );


        // Force a heartbeat to be issued now.

        TimeLastBeat = time( &TimeNow ) - HeartBeatInt - 1;

        EwSendHeartbeat( );     // let EW know we are still alive

        LogMessage( "Leaving  EwConnect\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwSendHeartbeat
// PURPOSE: Send a heartbeat message if enough time has elapsed since the last
//          one.  
//------------------------------------------------------------------------------
void EwSendHeartbeat( void ) {

 static char HeartMsg[MAXSTR];   // string for heartbeat messages
        long msgLen;             // length of the heartbeat message
                                 // long as tport_putmsg is declared in transport.h

        LogMessage( "Starting EwSendHeartbeat\n", 4, 0 );


        // Send heartbeat message if enough time has elapsed.

        if ( time( &TimeNow ) - TimeLastBeat >= HeartBeatInt ) {       // TimeNow is a time_t
                sprintf( HeartMsg, "%ld %d\n", (long)TimeNow, MyPid ); // Fill message string
                msgLen    = strlen( HeartMsg );                                                                 // Set message size

                if ( tport_putmsg( &Region, &LogoHeartBeat, msgLen, HeartMsg ) == PUT_OK ) {
                        if ( Verbosity >= 4 ) {
                                sprintf( MsgBuffer, "Sent heartbeat (time,pid) = %s", HeartMsg );
                                LogMessage( MsgBuffer, 4, 0 );
                                }
                        }
                else {
                        sprintf( MsgBuffer, "Error sending heartbeat (time,pid) = %s", HeartMsg );
                        LogMessage( MsgBuffer, 0, 1 );
//FIX add timestamp?
//                      logit( "et"
                        }

                TimeLastBeat = TimeNow;         // Update last heartbeat time
                }

        LogMessage( "Leaving  EwSendHeartbeat\n", 4, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: OutputPoint
// PURPOSE: Put BIN point into TraceBuf and send to Earthworm when the 
//          TraceBuf is full.
//------------------------------------------------------------------------------
void OutputPoint( BIN *Pt ) {

 static int    FirstTimeBad  = 1;
 static int    FirstTimeGood = 1;
        int    ichan, idig, powerH, tempC10, thisSec, diffNano;
        double thisTime;
        char   thQuality, MsgBuffer[MAXSTR];

        LogMessage( "Starting OutputPoint\n", 4, 0 );

//FIX - are we losing data ?  can we avoid divide ?

        thisSec = (int)((Pt->TimeStamp) / ((SRINT64)(NSPERSEC)));
        diffNano = (int)(Pt->TimeStamp - ((SRINT64)(thisSec*NSPERSEC)));

        thisTime = ((double)(thisSec)) + ((double)(diffNano)/(double)(NSPERSEC));

        if ( Verbosity >= 4 ) {
                sprintf( MsgBuffer,
                         "TimeStamp = %s, thisSec = %d,\t\ndiffNano %d, thisTime %lf\n",
                         CustomPrint64( Pt->TimeStamp ), thisSec, diffNano, thisTime );
                LogMessage( MsgBuffer, 4, 0 );
                }


//FIX check sample rate
        
        // Don't send data until GPS time is good.

        if ( thisSec < MIN_GOOD_TIME ) {
                if ( FirstTimeBad ) {
                        LogMessage( "GPS time not ready yet, no data sent\n", 0, 0 );
                        FirstTimeBad = 0;
                        }
                EwSendHeartbeat();
                LogMessage( "Leaving OutputPoint with GPS time not ready\n", 4, 0 );
                return;
                }
        else {
                if ( FirstTimeGood ) {
                        LogMessage( "GPS time now available, sending data soon\n", 0, 0 );
                        FirstTimeGood = 0;
                        }
                }

        

        // For first point in tracebuf, update trace data pointers,
        // start time, and quality flag.

        if ( NumReadySamples == 0 ) {

                sprintf( MsgBuffer, "First point in tracebuf for time %lf, sec %d\n",
                         thisTime, thisSec );
                LogMessage( MsgBuffer, 2, 0 );

                for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {
                        TraceDat[ichan]  = (int *) (TraceBuffer[ichan]+sizeof(TRACE_HEADER));
                        }

                thQuality = QUALITY_OK;

                for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {
                        TraceHead[ichan]->starttime  = thisTime;
                        TraceHead[ichan]->quality[0] = thQuality;
                        }

                } // end if NumReadySamples == first point


        // Save current point in tracebuf: analog, digital, & special channels.

//DBG
        if ( Verbosity >= 4 ) {
                sprintf( MsgBuffer,
                         "Normal point (ready samples %d) in tracebuf for time %lf, sec %d\n",
                         NumReadySamples, thisTime, thisSec );
                LogMessage( MsgBuffer, 4, 0 );
                }

        
        for ( ichan = 0 ; ichan < MAX_CHAN_ANA ; ichan++ ) {
                *(TraceDat[ichan]) = Pt->Analog[ichan];
                SummaryUpdate( ichan, *(TraceDat[ichan]) );
                }

        for ( idig = 0 ; idig < MAX_CHAN_DIG ; idig++, ichan++ ) {
                *(TraceDat[ichan]) = (Pt->DigitalAll >> idig & 0x01);
                SummaryUpdate( ichan, *(TraceDat[ichan]) );
                }

//        for ( iothr = 0 ; iothr < MAX_CHAN_OTHR ; iothr++, ichan++ )

        tempC10 = Pt->DegC2 * 5;                    // degree C x 2 x 5 = 10 x degrees C
        powerH  = (Pt->VoltageGood & 0x02) ? 1 : 0; // Is power history bit good ?

        *(TraceDat[ichan]) = Pt->PpsToggle;  SummaryUpdate( ichan, *(TraceDat[ichan]) );  ichan++;
        *(TraceDat[ichan]) = powerH;         SummaryUpdate( ichan, *(TraceDat[ichan]) );  ichan++;
        *(TraceDat[ichan]) = tempC10;        SummaryUpdate( ichan, *(TraceDat[ichan]) );  ichan++;
        *(TraceDat[ichan]) = Pt->NumSat;     SummaryUpdate( ichan, *(TraceDat[ichan]) );  ichan++;

        
        if ( Pt->NumSat < 3 ) {       // not enough sats

                thQuality = TIME_TAG_QUESTIONABLE;
                if ( Verbosity >= 4 )  {
                        sprintf( MsgBuffer,
                                 "Setting time tag questionable (Nsat = %d)\n",
                                 Pt->NumSat );
                        LogMessage( MsgBuffer, 4, 0 );
                        }
                } // end NumSat < 3

        
        if ( Verbosity >= 4 ) {
                for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {
                        sprintf( MsgBuffer,
                                 "  S=%s C=%s N=%s L=%s pin=%d, ver=%c%c, value=%d\n",
                                   TraceHead[ichan]->sta,
                                   TraceHead[ichan]->chan,
                                   TraceHead[ichan]->net,
                                   &TraceHead[ichan]->chan[TRACE_POS_LOC],
                                   TraceHead[ichan]->pinno,
                                   TraceHead[ichan]->chan[TRACE_POS_VER],
                                   TraceHead[ichan]->chan[TRACE_POS_VER+1],
                                   *(TraceDat[ichan])
                                 );
                        LogMessage( MsgBuffer, 4, 0 );
                        }
                } // end if Verbosity


        // Prepare for next point.

        for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ )
                TraceDat[ichan]++;

        NumReadySamples++;


        
        // If the tracebuf is full, set its end time and send it.

        if ( NumReadySamples == MaxReadySamples ) {

                sprintf( MsgBuffer,
                         "Reached max number of samples %d in tracebuf for time %lf, sec %d\n",
                         NumReadySamples, thisTime, thisSec );
                LogMessage( MsgBuffer, 2, 0 );
                
                for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {
                        TraceHead[ichan]->endtime    = thisTime;
                        TraceHead[ichan]->quality[0] = thQuality;
                        }

                SendTraces( );
                SummaryReport();

                } // end if NumReadySamples == last point

        
        LogMessage( "Leaving OutputPoint\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: SentTraces
// PURPOSE: Send traces saved in TraceBuf arrays to the Earthworm ring.  The
//          data is sent one message per channel.
//------------------------------------------------------------------------------
void SendTraces( void ) {

        int  ichan, rc;
        long buffersize;

        LogMessage( "Starting SendTraces ...\n", 3, 0 );


        // Let Earthworm know we're still alive.

        EwSendHeartbeat();

        
        // Set tracebuf format type and size.

        buffersize = sizeof(TRACE_HEADER) + (NumReadySamples*sizeof(int));

        sprintf( MsgBuffer, "Sending tracebuffer of size %ld\n", buffersize );
        LogMessage( MsgBuffer, 2, 0 );


        // Loop around sending tracebuf message to the transport ring.

        for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {

                TraceHead[ichan]->nsamp = NumReadySamples;       // number of samples in message
                
                rc = tport_putmsg( &Region, &LogoTrace, buffersize, TraceBuffer[ichan] );

                if ( rc == PUT_TOOBIG ) {
//FIX                   logit( "e",
                        sprintf( MsgBuffer,
                                 "Trace message for channel %d too big\n",
                                 ichan );
                        LogMessage( MsgBuffer, 0, 1 );
                        }

                if ( rc == PUT_NOTRACK ) {
//FIX                   logit( "e",
                        sprintf( MsgBuffer,
                                 "Tracking error while sending channel %d\n",
                                 ichan );
                        LogMessage( MsgBuffer, 0, 1 );
                        }

                } // end for ichan < MAX_EW_CHAN

        // Reset number of ready samples to prepare for next tracebuf.

        NumReadySamples = 0;
           

        LogMessage( "Leaving  SendTraces ...\n", 3, 0 );

        return;
}

//------------------------------------------------------------------------------
// ROUTINE: SummaryUpdate
// PURPOSE: Update the associated min and max if the specified data value
//          is more extreme.
//------------------------------------------------------------------------------
void SummaryUpdate( int ichan, int Value ) {

        sprintf( MsgBuffer,
                 "Starting SummaryUpdate for chan %d, pt %d\n",
                 ichan, NumReadySamples );
        LogMessage( MsgBuffer, 4, 0 );
        

        if ( SummaryMin[ichan] > Value )
                SummaryMin[ichan] = Value;

        else if ( SummaryMax[ichan] < Value )
                SummaryMax[ichan] = Value;

        LogMessage( "Leaving  SummaryUpdate ...\n", 4, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: SummaryReport
// PURPOSE: Output summary report to the log file if enough time has passed
//          since the last report.
//------------------------------------------------------------------------------
void SummaryReport( void ) {

        int  ichan;
        
        LogMessage( "Starting SummaryReport ...\n", 3, 0 );

        SummaryCount++;

        if ( SummaryCount > SummaryInterval ) {
                LogMessage( "Summary Report:\n", 0, 0 );
                for ( ichan = 0 ; ichan < MAX_EW_CHAN ; ichan++ ) {
                        sprintf( MsgBuffer, "Channel %2d: min = %d, max = %d\n",
                               ichan, SummaryMin[ichan], SummaryMax[ichan] );
                        LogMessage( MsgBuffer, 0, 0 );
                        }
                
                SummaryCount = 0;
                }

        LogMessage( "Leaving  SummaryReport ...\n", 3, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwError
// PURPOSE: Build and send an Earthworm error message to the shared memory
//          region (ring).
//------------------------------------------------------------------------------
void EwError( int ierr, char *note ) {

 static char ErrMsg[MAXSTR];    // string for error messages
        long msgLen;            // length of the error message
                                // long as tport_putmsg is declared in transport.h

        LogMessage( "Starting EwError\n", 4, 0 );

        // Build the message.

        time( &TimeNow );                                            // TimeNow is a time_t
        sprintf( ErrMsg, "%ld %d %s\n", (long)TimeNow, ierr, note ); // Fill errmsg str
        msgLen = strlen( ErrMsg );                                   // Set errmsg size


        // Log it and send it to shared memory.

//FIX                logit( "et"
        if ( tport_putmsg( &Region, &LogoError, msgLen, ErrMsg ) == PUT_OK ) {
                sprintf( MsgBuffer, "sr2ew: %s\n", note );
                LogMessage( MsgBuffer, 0, 0 );
                }
        else {
                sprintf( MsgBuffer, "sr2ew:      Error sending error:%d.\n", ierr );
//FIX                logit( "et"
                LogMessage( MsgBuffer, 0, 0 );
                }

        LogMessage( "Leaving  EwError\n", 4, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: EwCheckTerminate
// PURPOSE: Check to see if this Earthworm ring is requesting a termination and
//          exit if it is.  
//------------------------------------------------------------------------------
void EwCheckTerminate( void ) {

           int flag;

           LogMessage( "Starting EwCheckTerminate\n", 4, 0 );


           // Check for EW terminate request.

           flag = tport_getflag( &Region );

           if ( (flag == TERMINATE) || (flag == MyPid) )
                   CleanExit( 0 );


           LogMessage( "Leaving  EwCheckTerminate\n", 4, 0 );
}

//------------------------------------------------------------------------------
// ROUTINE: LogMessage
// PURPOSE: If the specified level is exceeded, the message is logged as requested.  
//          Then, if a non-zero error code was passed, exit.
//------------------------------------------------------------------------------
void LogMessage( char *msg, int level, int err ) {

	struct tm *ptm;
	time_t now;
	char FullMsg[MAXSTR];


        // Quick return if log level test not met (unless this is an error).

        if ( (err == 0) && (Verbosity < level) )
                return;


       // Get current system time.

	time( &now );
	ptm = gmtime( &now );



	// Prepare full message.

	sprintf( FullMsg, "%04d/%02d/%02d %02d:%02d:%02d [%s]: %s",
		  (ptm->tm_year + 1900), (ptm->tm_mon + 1), ptm->tm_mday,
                   ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
                   PROGRAMNAME, msg );
	


        // Log message to Earthworm.

        logit( err ? "e" : "", "%s", FullMsg );


        // Log message to SR report system.

        CustomReportPrint( FullMsg );


        // Exit on error.

        if ( err )
                CleanExit( err );
}

//------------------------------------------------------------------------------
// ROUTINE: CleanExit
// PURPOSE: Do all tasks needed for a clean exit.  These include closing the
//          input pipe, killing the pipeline chain, and closing report files.
//------------------------------------------------------------------------------
int CleanExit( int ExitCode ) {

        char PipelineCmd[MAXSTR];
        
        LogMessage( "CleanExit starting ...\n", 3, 0 );


	// Close the input pipe.

	CustomCloseInputPipe( hInputPipe );



	// Call batch file to kill the SR to EW pipeline chain.

	sprintf( PipelineCmd, "%sSr2EwPipelineKill", EwParamDir );
	RunPipelineCmd( PipelineCmd );



        // Detach from Earthworm ring.
        
        if ( Region.addr != NULL )
                tport_detach( &Region );


        // Notify user.

        if ( ExitCode == 0 ) 
                LogMessage( "sr2ew: Normal termination; exiting!\n", 0, 0 );
        else {
                sprintf( MsgBuffer,
                         "sr2ew: Error termination (err = %d); exiting!\n",
                         ExitCode );
                LogMessage( MsgBuffer, 0, 0 );
                }
	fflush( stdout );


	// Close custom report.

	CustomReportClose( );


        // Goodbye.

	exit( ExitCode );
}


#if defined( SROS_WINDOWS )
//------------------------------------------------------------------------------
// ROUTINE: OsMkDir (Windows version)
// PURPOSE: Make a new directory with the specified name.
//------------------------------------------------------------------------------
int OsMkDir( char *DirectoryName ) {
	return( _mkdir( DirectoryName ) );
}
//------------------------------------------------------------------------------
// ROUTINE: OsClearSysError (Windows version)
// PURPOSE: Clear last system error code.
//------------------------------------------------------------------------------
void OsClearSysError( void ) {
	SetLastError( ERROR_SUCCESS );
}

#elif defined( SROS_LINUX ) || defined( SROS_ANDROID )
//------------------------------------------------------------------------------
// ROUTINE: OsMkDir (Linux version)
// PURPOSE: Make a new directory with the specified name and full permissions.
//------------------------------------------------------------------------------
int OsMkDir( char *DirectoryName ) {
        
        int Permissions;

        Permissions = (S_IRWXU | S_IRWXG | S_IRWXO);  // Read,Write,Execute
                                                      // for User,Group,Other
	return( mkdir( DirectoryName, Permissions ) );
}
//------------------------------------------------------------------------------
// ROUTINE: OsClearSysError (Linux version)
// PURPOSE: Clear last system error code.
//------------------------------------------------------------------------------
void OsClearSysError( void ) {
	errno = 0;
}

#endif  // SROS_xxxxx
