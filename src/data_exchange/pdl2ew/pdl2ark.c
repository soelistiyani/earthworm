#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pdl2ew.h"

#define ptrAfter(fld) ((char*)(fld)+sizeof(fld))

char *myargs[] = {
    "--property-eventtime=",
    "--property-latitude=",
    "--property-longitude=",
    "--property-depth=",
    "--property-magnitude=",
    "--property-num-phases-used=",
    "--property-azimuthal-gap=",
    "--property-minimum-distance=",
    "--property-horizontal-error=",
    "--property-vertical-error=",
    "--code=",
    "--property-version=",
    "--property-magnitude-type="
    };
enum myarg_idx {
    ORIGIN_TIME_ARG,
    LATITUDE_ARG,
    LONGITUDE_ARG,
    DEPTH_ARG,
    MAGNITUDE_ARG,
    NUM_PHASES_ARG,
    AZIMUTH_GAP_ARG,
    MIN_DIST_ARG,
    HORZ_ERR_ARG,
    VERT_ERR_ARG,
    EVENTID_ARG,
    VERSION_ARG,
    MAGTYPE_ARG
};
    
#define VERSION_STR "0.9.9 - 2018-12-04"

//function to create temporary file containing 
//the body of an email to be used by sendmail
//
//@return-  an integer less than 0 if failure
//          0 if successful
int createEmailBody(char *fileName, char *eventID)
{
   FILE *htmlFile = NULL;

   if ((!fileName) || (!eventID)) return -1;

   if ((htmlFile = fopen(fileName, "w")) == NULL) {
      fprintf(stderr, "Error creating HTML body!\n");
      return -2;
   }

   fprintf(htmlFile, "<html>\n");
   fprintf(htmlFile, "<header>\n");
   fprintf(htmlFile, "</header>\n");
   fprintf(htmlFile, "<body>\n");
   fprintf(htmlFile, "<p> This is an automated computer-generated message notifying users of the USGS' Product Distribution Layer system that a \
                   previously disseminated event with ID %s has been deleted from the PDL system </p>\n", eventID);
   fprintf(htmlFile, "</body>\n");
   fprintf(htmlFile, "<html>");

   fclose(htmlFile);
   return 0;
}



/*
 *
 */
int main( int argc, char** argv ) {
    int i, j;
    double vals[3] = {0,0,0};
    FILE* f;
    char codaSet = 0, isCoda;        
    ARCDATA msg;
    char *eEFName = NULL;
    int size_EEFName = 0;
    int reqd = 1;
    int err = 0;
    int isDelete = 0;
    int eventEmailPosition = 3;
    int excludeNetworkPosition = 4; 
    int start_data_arg_pos = 4;
    char *erArg = "-ExcludeNetworks=";
    int erArgLen = strlen(erArg);
    char erPrefix = '=';
    char eventEmailFileName[250] = { '\0' };
    FILE *eventEmailFile;
    char htmlFileName[50] = { '\0' };
    char *linePtr = NULL;
    int  linePtrSize = 0;
    char sysCommand[200] = { '\0' };

    memset( &msg, ' ', sizeof(msg) );
    msg.term = 0;
    f = fopen( argv[2], "a" );

    //XXX XXX
    //It really would be much preferable to a) document the options and b) use something cleaner like "getopt"
    //so much of this logic relies on undocumented magic numbers, adding a new commandline argument is much harder than it should be
    if ( argc < 4 ) {
      fprintf(stderr, "Usage: pdl2ark <dest directory> <path to logfile> <path to event-email mapping> [-ExcludeNetworks=N1,N2,...] <data-arg1> <data-arg2> ...\n");
      fprintf(stderr, "Version: %s\n", VERSION_STR );
      exit(1);
    }
    if ( (argc >= 5) && (strncmp( argv[excludeNetworkPosition], erArg, erArgLen ) == 0) ) {
        start_data_arg_pos++;   // skip over ExcludeRegions
        for ( i=erArgLen-1; i < strlen(argv[excludeNetworkPosition]); ) {
            if ( argv[excludeNetworkPosition][i] != erPrefix ) {
                fprintf(stderr, "Bad arg: '%s' not '-ExcludeNetworks=N1,N2,...'", argv[excludeNetworkPosition]);
                exit(1);
            }
            erPrefix = ',';
            for ( j=1; j<=3 && argv[excludeNetworkPosition][i+j] != 0 && argv[excludeNetworkPosition][i+j] != erPrefix; j++ );
            if ( j==1 || j==4 ) {
                fprintf(stderr, "Bad/missing network in '%s'", argv[excludeNetworkPosition]);
                exit(1);
            }
	    i += j;
        }
        erArg = argv[excludeNetworkPosition]+erArgLen;
    } else
        erArg = NULL;
    for ( i=start_data_arg_pos; i<argc; i++ ) {
        int quoted = (argv[i][0] == '\"') ? 1 : 0;
        char* test_arg = argv[i] + quoted;
        if ( strcmp( argv[i], "--delete" ) == 0 ) {
            isDelete = 1;
        }
        if ( strncmp( myargs[ORIGIN_TIME_ARG], test_arg, strlen(myargs[ORIGIN_TIME_ARG]) ) == 0 ) {
            char *ot = msg.originTime;
            char *p = argv[i]+strlen(myargs[ORIGIN_TIME_ARG]);
            while ( *p && (ot-msg.originTime<16) ) {
                if ( isdigit(*p) ) 
                    *(ot++) = *p;
                p++;
            }
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[LATITUDE_ARG], test_arg, strlen(myargs[LATITUDE_ARG]) ) == 0 ) {
            double val, deg, min;
            char *tobesaved, save;
            vals[1] = strtod( test_arg + strlen(myargs[LATITUDE_ARG]), NULL );
            val = fabs(vals[1]);
            deg = floor(val);
            min = 60.0 * (val - deg);
            tobesaved = ptrAfter(msg.latitude);
            save = *tobesaved;
            sprintf( msg.latitude, "%2.0lf%c%04.0lf", deg, vals[1]>=0 ? 'N' : 'S', min*100 );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[LONGITUDE_ARG], test_arg, strlen(myargs[LONGITUDE_ARG]) ) == 0 ) {
            double val, deg, min;
            char *tobesaved, save;
            vals[2] = strtod( test_arg + strlen(myargs[LONGITUDE_ARG]), NULL );;
            val = fabs(vals[2]);
            deg = floor(val);
            min = 60.0 * (val - deg);
            tobesaved = ptrAfter(msg.longitude);
            save = *tobesaved;
            sprintf( msg.longitude, "%3.0lf%c%04.0lf", deg, vals[2]>=0 ? 'E' : 'W', min*100 );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
       } else if ( strncmp( myargs[DEPTH_ARG], test_arg, strlen(myargs[DEPTH_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[DEPTH_ARG]), NULL );
            tobesaved = ptrAfter(msg.depth);
            save = *tobesaved;
            sprintf( msg.depth, "%5.0lf", val*100 );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[MAGNITUDE_ARG], test_arg, strlen(myargs[MAGNITUDE_ARG]) ) == 0 ) {
            double val;
            val = strtod( test_arg + strlen(myargs[MAGNITUDE_ARG]), NULL );
            vals[0] = val;
        } else if ( strncmp( myargs[NUM_PHASES_ARG], test_arg, strlen(myargs[NUM_PHASES_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[NUM_PHASES_ARG]), NULL );
            tobesaved = ptrAfter(msg.numPhases);
            save = *tobesaved;
            sprintf( msg.numPhases, "%3.0lf", val );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[AZIMUTH_GAP_ARG], test_arg, strlen(myargs[AZIMUTH_GAP_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[AZIMUTH_GAP_ARG]), NULL );
            tobesaved = ptrAfter(msg.azimuthalGap);
            save = *tobesaved;
            sprintf( msg.azimuthalGap, "%3.0lf", val );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[MIN_DIST_ARG], test_arg, strlen(myargs[MIN_DIST_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[MIN_DIST_ARG]), NULL );
            tobesaved = ptrAfter(msg.minDist);
            save = *tobesaved;
            sprintf( msg.minDist, "%3.0lf", val );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[HORZ_ERR_ARG], test_arg, strlen(myargs[HORZ_ERR_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[HORZ_ERR_ARG]), NULL );
            tobesaved = ptrAfter(msg.horzError);
            save = *tobesaved;
            sprintf( msg.horzError, "%4.0lf", val*100 );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[VERT_ERR_ARG], test_arg, strlen(myargs[VERT_ERR_ARG]) ) == 0 ) {
            double val;
            char *tobesaved, save;
            val = strtod( test_arg + strlen(myargs[VERT_ERR_ARG]), NULL );
            tobesaved = ptrAfter(msg.vertError);
            save = *tobesaved;
            sprintf( msg.vertError, "%4.0lf", val*100 );
            *tobesaved = save;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[EVENTID_ARG], test_arg, strlen(myargs[EVENTID_ARG]) ) == 0 ) {
            int j, off, match;
            for ( j=0, off=strlen(myargs[EVENTID_ARG]); test_arg[j+off] != 0; j++ )
                msg.pad_eventid[j] = test_arg[j+off];
            if ( erArg != NULL )
                for ( j=0; ; j += off+1) {
                    off = 0;
                    match = 1;
                    for ( off=0, match=1; erArg[j+off] != 0 && erArg[j+off] != ','; off++ )
                        if ( erArg[j+off]!=msg.pad_eventid[off] )
                            match = 0;
                    if ( match ) {
                        puts("Excluded network");
                        return 0;
                    } else if ( erArg[j+off] == 0 )
                        break;
                }
            reqd--;
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[VERSION_ARG], test_arg, strlen(myargs[VERSION_ARG]) ) == 0 ) {
            msg.version[0] = test_arg[strlen(myargs[VERSION_ARG])];
            //fprintf( f, "%s\n", argv[i] );
        } else if ( strncmp( myargs[MAGTYPE_ARG], test_arg, strlen(myargs[MAGTYPE_ARG]) ) == 0 ) {
            codaSet = 1;
            isCoda = (strcmp(test_arg,"Mc")==0) || (strcmp(test_arg,"Md"));
            //fprintf( f, "%s\n", argv[i] );
        }
    }
    if ( codaSet ) {
        char *tobesaved, save;
        tobesaved = ptrAfter(msg.Md);
        save = *tobesaved;
        sprintf( msg.Md, "%3.0lf", vals[0]*100 );
        *tobesaved = save;
        tobesaved = ptrAfter(msg.Mpref);
        save = *tobesaved;
        sprintf( msg.Mpref, "%3.0lf", vals[0]*100 );
        *tobesaved = save;
    }
    if ( (reqd == 0) && !(isDelete) ) {
        char path[300], *p2;
        FILE *f2;
        sprintf( path, "%s/__________.ark", argv[1] );
        p2 = path+strlen(argv[1])+1;
        for ( i=0; i<sizeof(msg.pad_eventid); i++ )
            if ( !isblank(msg.pad_eventid[i]) )
                p2[i] = msg.pad_eventid[i];
        //fprintf( f, "Path: %s\n", path );
        f2 = fopen( path, "w" );
        puts( path );
        puts( (char*)&msg );
        fwrite( (void*)&msg, sizeof(msg), 1, f2 );
        memset( path, ' ', 100 );
        path[100] = 0;
        fwrite( path, 101, 1, f2 );
        fclose( f2 );
    } else if ((reqd == 0) && isDelete) {
       //check directory for file with email addresses this event has been sent to
//       sprintf(eventEmailFileName, "%s/%s", argv[eventEmailPosition], msg.pad_eventid);
       sprintf(eventEmailFileName, "%s/", argv[eventEmailPosition]);
       strncat(eventEmailFileName, msg.pad_eventid, strlen(msg.pad_eventid));
       if (eEFName != NULL) {
         free(eEFName);
         eEFName = NULL;
       }
       size_EEFName = strcspn(eventEmailFileName," ");
       eEFName = (char *)calloc(size_EEFName + 1, sizeof(char));
       strncpy(eEFName, eventEmailFileName, size_EEFName);
       if (access(eEFName, F_OK) == 0) {
         //here we need to send out emails to notify of a deletion
         //open the event-email mapping file
         if (!(eventEmailFile = fopen(eEFName, "r"))) {
            fprintf(stderr, "Error opening event email file %s\n", eEFName);
            exit(1);            
         } else {
            sprintf(htmlFileName, "%s.html", msg.pad_eventid);
            if ((err = createEmailBody(htmlFileName, msg.pad_eventid)) != 0) {
               fprintf(stderr, "Error creating HTML message body. error code: %d filename: %s\n", err, htmlFileName);
               exit(1);
            }
            linePtrSize = 80;
            linePtr = (char *)calloc(linePtrSize, sizeof(char));  
#ifdef _SOLARIS
            while (fgets(linePtr, linePtrSize, eventEmailFile) != NULL) {
#else
            while (getline(&linePtr, &linePtrSize, eventEmailFile) != NULL) {
#endif
               //linePtr contains an email address
               //XXX XXX XXX DEBUGGING: 
               fprintf(stdout, "linePtr: %s\n", linePtr);
               sprintf(sysCommand, "sendmail %s -html -to %s -subject \"USGS NEIC Event %s deleted\" ", htmlFileName, linePtr, msg.pad_eventid);
               if (system(sysCommand)) {
                  fprintf(stderr, "sendmail failure. EventID: %s\n", msg.pad_eventid);                
                  exit(1);
               }
               //
               free(linePtr); 
               linePtr = NULL;
            }         
            fclose(eventEmailFile);
            remove(eEFName);
            remove(htmlFileName);
         }
       } else {
         fprintf(stderr, "Received delete message for EventID %s, but no record exists of this having been emailed\n", msg.pad_eventid);
       }
    } else {
        fprintf( f, "Origin but no event id\n" );
    }
    //fprintf( f, "----\n" );
    if(f != NULL) {
        fclose( f );
    }
    return 0;
}
