// FILE: CustomPipeUtil.h
// COPYRIGHT: (c), Symmetric Research, 2016-2018
//
// Include file with defines required for working with USBxCH BIN data in C.
//

// OS specific defines.  SROS_WINDOWS or SROS_LINUX compile define is required.

#if defined( SROS_WINDOWS )
#include <windows.h>            // for CreateNamedPipe etc
typedef __int32 SRINT32;
typedef __int64 SRINT64;
typedef HANDLE  SRPIPEHANDLE;
#define REQUIRED_PIPE_PREFIX    "\\\\.\\pipe\\SrPipe"

#elif defined( SROS_LINUX )
#include <time.h>		// for nanosleep
typedef int     SRINT32;
typedef int64_t SRINT64;
typedef FILE *  SRPIPEHANDLE;
#define REQUIRED_PIPE_PREFIX    "./SrPipe"

#else
#pragma message( "COMPILE ERROR: SROS_xxxxx MUST BE DEFINED !! " )

#endif  // SROS_xxxxx


// Define a few sizes.

#define MAXANALOGCHANNELS    8   // max num analog channels for any USBxCH
#define MAXSTR             256   // max size of any string


// Binary structure for data layout.

#define BIN_ID         (0x31425253L)      // 'SRB1'
#define BIN_REV        (0x00000001L)      // BIN layout Rev 1
#define BIN             struct BinRecord  // BIN = shorthand for binary BIN record
#define NSIZE_BINDATA   sizeof( struct BinRecord )
#define SR_ZERO64      ((SRINT64)0L)

struct BinRecord {

        int     Id;                   // BIN ID field
        int     Rev;                  // BIN Rev number

        SRINT64 Sample;               // USBxCH sample count

        int     Analog[MAXANALOGCHANNELS];  // supports both USB4CH and USB8CH
        
        int     reserved_1;
        int     reserved_2;
        int     reserved_3;
        int     reserved_4;
        
        int     PpsToggleCount;       // PPS count
        int     reserved_5;

        char    reserved_6;
        char    DigitalAll;           // four digital inputs
        char    PpsToggle;            // PPS square wave
        char    reserved_7;
        
        char    VoltageGood;          // power history
        char    NumSat;               // number GPS satellites
        char    Trigger;              // bit # = 1 means trigger active on ch#
        char    reserved_8;
        
        int     Lat10K;               // Latitude  times 10000 +N,-S 
        int     Lon10K;               // Longitude times 10000 -W,+E 
        int     Alt10K;               // Altitude  times 10000 in meters 
        int     DegC2;                // Degrees C times 2

        int     reserved_9;
        int     reserved_10;
        
        SRINT64 reserved_11;
        SRINT64 reserved_12;
        SRINT64 reserved_13;
        SRINT64 reserved_14;
        SRINT64 reserved_15;
        SRINT64 reserved_16;
        SRINT64 TimeStamp;           // time in nanoseconds since 1970

        SRINT64 reserved_17;
        SRINT64 reserved_18;
        
        };



// Prototypes:

void         CustomReportOpen( char *name );
void         CustomReportPrint( char *msg );
void         CustomReportClose( void );

SRPIPEHANDLE CustomOpenInputPipe( char *InputPipeName );
int          CustomReadInputPipe( SRPIPEHANDLE hInputPipe, BIN *Pt );
void         CustomCloseInputPipe( SRPIPEHANDLE hInputPipe );

SRPIPEHANDLE CustomOpenOutputPipe( char *OutputPipeName );
void         CustomWriteOutputPipe( SRPIPEHANDLE hOutputPipe, BIN *Pt );
void         CustomCloseOutputPipe( SRPIPEHANDLE hOutputPipe );

void         CustomErrorExit( char *msg );
void         CustomSleep( int ms );
char        *CustomPrint64( SRINT64 Value );


