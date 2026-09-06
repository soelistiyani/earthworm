// FILE: CustomPipeLib.c (LINUX VERSION)
// COPYRIGHT: (c), Symmetric Research, 2016-2018
//
// Source code utility functions for accessing USBxCH BIN data
// from a SrPipe in C.
//

// Include files:

#include <inttypes.h>           // for PRId64

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>              // for errno
#include <sys/stat.h>           // for mkfifo
#include <sys/types.h>          // for mkfifo
#include <time.h>               // for nanosleep

#include "CustomPipeLib.h"      // for prototypes, BIN, and other SR defines


// Defines and global variables:

FILE *RptFp = NULL;             // report file pointer

char MsgBuffer[MAXSTR];         // for building message string



// Custom Pipe Lib Functions:
//
//

void CustomReportOpen( char *name ) {

        char RptName[MAXSTR];

        sprintf( RptName, "SrRpt-%s.rpt", name );

        RptFp = fopen( RptName, "a" );
        if ( !RptFp )
                exit( 2 );
}

void CustomReportPrint( char *msg ) {

        if ( RptFp ) {
                fprintf( RptFp, "%s", msg );
                fflush( RptFp );
                }
}

void CustomReportClose( void ) {

        if ( RptFp ) {
                fclose( RptFp );
                RptFp = NULL;
                }
}

SRPIPEHANDLE CustomOpenInputPipe( char *InputPipeName ) {

        SRPIPEHANDLE hInputPipe;
        
        while ( 1 ) {

                // Open the pipe for binary reading.

                hInputPipe = fopen( InputPipeName, "rb" );

                // If successful, leave infinite loop.

                if ( hInputPipe )
                        break;
                
                // Otherwise, wait and try again.

                CustomSleep( 500 );
                }

        sprintf( MsgBuffer, "Opened input pipe %s\n", InputPipeName );
        CustomReportPrint( MsgBuffer );

        return( hInputPipe );
}

int CustomReadInputPipe( SRPIPEHANDLE hInputPipe, BIN *Pt ) {

        int NumToRead, NumRead, NumRemain, BytesRead, Err;
        char *BytePtr;

        // Initialize variables.

        BytePtr   = (char *)Pt;
        NumToRead = NSIZE_BINDATA;
        NumRead   = 0;
        NumRemain = NumToRead - NumRead;

        // Loop around until all requested bytes have been read.
        
        while ( NumRead < NumToRead ) {

                BytesRead = fread( BytePtr, 1, NumRemain, hInputPipe );
                Err       = errno;

                if ( feof( hInputPipe ) )
                        CustomReportPrint( "End of file when reading\n" );
                if ( ferror( hInputPipe ) )
                        CustomReportPrint( "Error when reading\n" );

                BytePtr   += BytesRead;
                NumRead   += BytesRead;
                NumRemain -= BytesRead;

                
                if ( NumRemain == 0 )
                        break;

                if ( Err == ENOENT )  // other end closed
                        CustomReportPrint( "Failed to read input pipe\n" );
                        return( 0 );
                }


        // Verify we've got a BIN structure by checking its id.

        if ( Pt->Id == BIN_ID )
                return( 1 ); // success

        else {
                CustomReportPrint( "Invalid BIN structure read\n" );
                return( 0 );
                }
}

void CustomCloseInputPipe( SRPIPEHANDLE hInputPipe ) {

        fclose( hInputPipe );
        CustomReportPrint( "Closed input pipe\n" );
}

SRPIPEHANDLE CustomOpenOutputPipe( char *OutputPipeName ) {
        
        int          mode;
        SRPIPEHANDLE hOutputPipe;
        
        // Make it read write for all.

        mode = S_IRUSR | S_IWUSR |      // user  RW
               S_IRGRP | S_IWGRP |      // group RW
               S_IROTH | S_IWOTH;       // other RW

        // Make fifo.

        if ( mkfifo( OutputPipeName, mode ) != 0 )
                CustomErrorExit( "Failed to make output pipe fifo.\n" );

        // Open pipe.

        hOutputPipe = fopen( OutputPipeName, "wb" );
        if ( !hOutputPipe )
                        CustomErrorExit( "Failed to open output pipe.\n" );

        sprintf( MsgBuffer, "Opened output pipe %s\n", OutputPipeName );
        CustomReportPrint( MsgBuffer );

        return( hOutputPipe );
}

void CustomWriteOutputPipe( SRPIPEHANDLE hOutputPipe, BIN *Pt ) {

        int  NumToWrite, NumWritten, NumRemain, BytesWritten, Err;
        char *BytePtr;

        BytePtr      = (char *)Pt;
        NumToWrite   = NSIZE_BINDATA;
        NumWritten   = 0;
        NumRemain    = NumToWrite - NumWritten;

        while ( NumWritten < NumToWrite ) {

                BytesWritten = fwrite( BytePtr, 1, NumRemain, hOutputPipe );
                Err          = errno;

                BytePtr    += BytesWritten;
                NumWritten += BytesWritten;
                NumRemain  -= BytesWritten;

                if ( NumRemain == 0 )
                        break;
                
                if ( Err == ENOENT )
                        CustomErrorExit( "Can't write, other end closed\n" );
                }

        fflush( hOutputPipe );
}

void CustomCloseOutputPipe( SRPIPEHANDLE hOutputPipe ) {

        fclose( hOutputPipe );
        CustomReportPrint( "Closed output pipe\n" );
}


void CustomErrorExit( char *msg ) {

        CustomReportPrint( msg );
        CustomReportClose( );
        exit( 1 );
}

void CustomSleep( int ms ) {

        struct timespec twait, tleft;
        
        // Linux nanosleep takes structure with seconds and nanoseconds.
        
        tleft.tv_sec  = tleft.tv_nsec = 0;
        twait.tv_sec  = (long)(ms / 1000);
        twait.tv_nsec = (ms - (twait.tv_sec*1000)) * 1000000;
        
        nanosleep( &twait, &tleft );
}

char *CustomPrint64( SRINT64 Value ) {

// This is a helper routine to convert a 64-bit integer into a string.  It 
// returns a pointer to a static string buffer.  When printing TWO strings,
// copy the first to another buffer before calling this function for the
// second value !

        static char StringBuffer[MAXSTR];

        sprintf( StringBuffer, "%" PRId64, Value ); // Linux 64 or 32 specifier

        return( StringBuffer );

}
