// FILE: CustomPipeLib.c (WINDOWS VERSION)
// COPYRIGHT: (c), Symmetric Research, 2016-2018
//
// Utility functions to access USBxCH BIN data from a named pipes in C.
//
// These are "easier to use" than the underlying Windows functions,
// providing an easier interface.  The same functions have corresponding
// equivalents in Linux with different underlying OS level functions.
//
//


// Include files:

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>            // for CreateNamedPipe etc

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
        DWORD        AccessMode, ShareMode;


        // Initialize mode variables.
        
        AccessMode = GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        ShareMode  = FILE_SHARE_READ | FILE_SHARE_WRITE;

        while ( 1 ) { 

                // Open the pipe for binary reading.

                hInputPipe = CreateFile(
                                   InputPipeName,
                                   AccessMode,   
                                   ShareMode,    
                                   NULL,                  // LPSECURITY_ATTRIBUTES
                                   OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, // DWORD
                                   NULL                   // SRPIPEHANDLE hTemplateFile
                                  );
                        
                // If successful, leave infinite loop.

                if ( hInputPipe != INVALID_HANDLE_VALUE )
                        break;

                // Otherwise, wait and try again.

                CustomSleep( 500 );
                }
        
        sprintf( MsgBuffer, "Opened input pipe %s\n", InputPipeName );
        CustomReportPrint( MsgBuffer );

        return( hInputPipe );
}

int CustomReadInputPipe( SRPIPEHANDLE hInputPipe, BIN *Pt ) {

        BOOL  Ok;
        int   Err;
        DWORD NumToRead, NumRead, NumRemain, BytesRead;
        char *BytePtr;

        // Initialize variables.

        BytePtr   = (char *)Pt;
        NumToRead = NSIZE_BINDATA;
        NumRead   = 0;
        NumRemain = NumToRead - NumRead;

        // Loop around until all requested bytes have been read.
        
        while ( NumRead < NumToRead ) {

                Ok = ReadFile(
                              hInputPipe,  // SRPIPEHANDLE
                              BytePtr,     // LPVOID
                              NumRemain,
                              &BytesRead,
                              NULL         // LPOVERLAPPED
                             );
                Err = GetLastError();

                BytePtr   += BytesRead;
                NumRead   += BytesRead;
                NumRemain -= BytesRead;

                if ( !Ok ) {
                        sprintf( MsgBuffer,
                                 "Failed to read input pipe with system error %d\n",
                                 Err );
                        CustomReportPrint( MsgBuffer );
                        
                        FormatMessage(
                                      FORMAT_MESSAGE_FROM_SYSTEM,
                                      NULL,
                                      Err,
                                      0,
                                      (LPTSTR)&MsgBuffer,
                                      MAXSTR,
                                      NULL
                                     );
                        CustomReportPrint( MsgBuffer );
                        
                        return( 0 );
                        }
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

        CloseHandle( hInputPipe ); // close on own end
        CustomReportPrint( "Closed input pipe\n" );
}

SRPIPEHANDLE CustomOpenOutputPipe( char *OutputPipeName ) {

        SRPIPEHANDLE hOutputPipe;
        DWORD        PipeSize, OpenMode, PipeMode;
        BOOL         Ok;


        // Initialize mode variables.

        OpenMode = PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE;
        PipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
        PipeSize = NSIZE_BINDATA * 10000 * 2; // sizeof(BIN) * max sample rate * 2 seconds


        // Prepare output pipe.

        hOutputPipe = CreateNamedPipe(
                                OutputPipeName,
                                OpenMode,
                                PipeMode,
                                1,          // DWORD nMaxInstances,
                                PipeSize,   // DWORD nOutBufferSize,
                                PipeSize,   // DWORD nInBufferSize,
                                0,          // DWORD nDefaultTimeOut, (ms)
                                NULL        // LPSECURITY_ATTRIBUTES
                                     );

        if ( hOutputPipe == INVALID_HANDLE_VALUE )
                CustomErrorExit( "Failed to open output pipe\n" );


        // Wait until reader is connected before returning.

        Ok  = ConnectNamedPipe( hOutputPipe, NULL );

        if ( !Ok )
                CustomErrorExit( "Can't connect output pipe\n" );

        sprintf( MsgBuffer, "Opened output pipe %s\n", OutputPipeName );
        CustomReportPrint( MsgBuffer );

        return( hOutputPipe );
}

void CustomWriteOutputPipe( SRPIPEHANDLE hOutputPipe, BIN *Pt ) {

        BOOL   Ok;
        int    Err;
        DWORD  NumToWrite, NumWritten, NumRemain, BytesWritten;
        char  *BytePtr;

        // Initialize variables.

        BytePtr      = (char *)Pt;
        NumToWrite   = NSIZE_BINDATA;
        NumWritten   = 0;
        NumRemain    = NumToWrite - NumWritten;

        // Write until all requested bytes written.

        while ( NumWritten < NumToWrite ) {

                Ok = WriteFile(
                               hOutputPipe,
                               BytePtr,       // LPCVOID lpBuffer,
                               NumRemain,
                               &BytesWritten,
                               NULL           // LPOVERLAPPED
                              );
                Err = GetLastError();

                BytePtr    += BytesWritten;
                NumWritten += BytesWritten;
                NumRemain  -= BytesWritten;

                if ( Err != ERROR_SUCCESS )
                        CustomErrorExit( "Can't write to output pipe\n" );
                }
}

void CustomCloseOutputPipe( SRPIPEHANDLE hOutputPipe ) {
        
        FlushFileBuffers( hOutputPipe );    // wait until client is finished reading
        DisconnectNamedPipe( hOutputPipe ); // force close on client end
        CloseHandle( hOutputPipe );         // close on own end
        CustomReportPrint( "Closed output pipe\n" );
}

void CustomErrorExit( char *msg ) {

        CustomReportPrint( msg );
        CustomReportClose( );
        exit( 1 );
}

void CustomSleep( int ms ) {

        Sleep( ms );
}

char *CustomPrint64( SRINT64 Value ) {

// This is a helper routine to convert a 64-bit integer into a string.  It 
// returns a pointer to a static string buffer.  When printing TWO strings,
// copy the first to another buffer before calling this function for the
// second value !

        static char StringBuffer[MAXSTR];

// In both 64-bit and 32-bit Windows, longs are 4 bytes so a 64 bit number 
// is referred to as a "long long int" which is printed with %lld.
//
        sprintf( StringBuffer, "%lld", Value ); // Windows 64 or 32 specifier

        return( StringBuffer );

}

