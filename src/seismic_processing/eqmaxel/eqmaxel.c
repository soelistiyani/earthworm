
/*********************C O P Y R I G H T   N O T I C E ***********************/
/* Copyright 2017 by Paul Friberg and Dong Hoon Sheen. All rights are reserved. Permission     */
/* is hereby granted for the use of this product for nonprofit,  */
/* or noncommercial publications that contain appropriate acknowledgement   */
/* of the authors. Modification of this code is permitted as long as this    */
/* notice is included in each resulting source module.                      */
/* USE AT YOUR OWN RISK, THE AUTHORS ACCEPT NO LIABILITY FOR USE OR MISUSE OF THIS PROGRAM */
/****************************************************************************/
/*
eqmaxel - maximum likelihood earthquake location module for Earthworm.


eqmaxel is an earthworm implementation of the Maximum Likelihood Earthquake locator: code named MAXEL
        by Dr. DongHoon Sheen of Chonnam University: BSSA paper in 2016, Sheen et al.

Application of the Maximum-Likelihood Location Method to the Earthquake Early Warning System in South Korea
by Dong-Hoon Sheen, Jung-Ho Park, Yun Jeong Seong, In-Seub Lim, and Heon-Cheol Chi
Bulletin of the Seismological Society of America, Vol. 106, No. 3, pp. –, June 2016, doi: 10.1785/0120150327
 

Version history
---------------
0.0.0 2017.07.18  - Paul Friberg and Dong-Hoon Sheen, ALL RIGHTS RESERVED 
0.0.1 2017.08.24  - bug fix to coordinate transform code
0.0.2 2017.12.08  - change in find_confidence to look for smaller area  when too few confidence points
*/
#define VERSION_STR "0.0.2 2017.12.08 "
#define MAX_PICKS 10000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <earthworm.h>
#include <transport.h>
#include <chron3.h>
#include <kom.h>
#include <site.h>
#include <tlay.h>
#include <time_ew.h>
#include "eqmaxel.h"
/* from maxel main.c */
#include "mle.h"
#include "ll2utm.h"

static const int InBufl = MAX_BYTES_PER_EQ;
# define LOGBUFSIZE 2048

/* Functions in this source file:
 ********************************/
void  eqm_config( char * );
int   eqm_eventscnl( char *, char * );
int   eqm_readhyp( char * );
int   eqm_readphs( char * );
void  eqm_calc( EVNINFO *event );
void  eqm_bldhyp( EVNINFO *event, char *hypcard );
void  eqm_bldterm( char * );

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
unsigned char TypeEventSCNL;
unsigned char TypeHyp2000Arc;
unsigned char TypeHeartBeat;
unsigned char TypeKill;
unsigned char TypeError;

/* Things read from the configuration file
 *****************************************/
#define MAXLEN 100
#define MAXDIST 2000    	  /* maximum distance */
#define FIX_Z 8.0		  /* fix depth */
unsigned char MyModId;            /* eqmaxel's module id (same as eqproc's)  */
static   int  LogSwitch = 1;      /* 0 if no logging should be done to disk */
static   char NextProcess[MAXLEN];/* command to send output to              */
static   int  LabelAsBinder = 0;  /* if 0, label phases as generic P and S  */
                                  /* if non-zero, use labels from binder    */
static   int  LabelVersion  = 0;  /* if non-zero, writes the version number */
                                  /* from eqproc,eqprelim on the archive    */
                                  /* summary line                           */
char          SourceCode;         /* label to tack on summary cards       */
STAPARM *StaArray = NULL;         /* structure of per-channel parameters    */
int      Nsta = 0;                /* #channels read from StaFile(s)         */
int      ForceExtrapolation = 0;  /* =1 for debugging purposes              */
int      LogArcMsg = 0;           /* =nonzero if to log TYPE_HYP2000ARC msg */
char trtable_filename[500];

char          RingName[20];  /* Name of transport ring to write to   */
SHM_INFO      Region;        /* working info for transport ring      */
static long          RingKey;       /* key to pick transport ring            */
static unsigned char InstId;        /* local installation id                 */



int nobs,nstnDB,nevn,nunused;    // number of observed time, number of station database
int minphid;
int verbose,ctype;
int ntrdpth;
int ntredst=MAXDIST;
int use_phase_wgt = 0;
float *trtable;
double Xmin,Xmax,Ymin,Ymax,Zmin,Zmax;
double Xc,Yc,dX,dY,dZ,halfX,halfY,halfZ;
double crt_td;
int nX,nY,nZ;
OBSINFO *obslist;
STNINFO *stnlist;
EVNINFO evnlist[MAX_EV]; /* NEED to remove use of this in MAXEL codes, pass in event instead */
char *stnfile,*obsfile,*trfile;

/* intialization from control file read for MAXEL see: read_cntl.c*/
void init_maxel() {
int i;
double dist;

   crt_td = 2.; // criterion for travel time difference in binder

   Xmin = -800.;
   Xmax =  800.;
   Ymin = -800.;
   Ymax =  800.;
   Zmin =  0.;
   Zmax =  30.;
   dX = 20.;
   dY = 20.;
   dZ = 5.;
   
   ntrdpth = 31;
   ntredst = MAXDIST+1;


   for(dist=Xmin,i=0;dist<=Xmax;dist+=dX) i++;
   nX = i;
   for(dist=Ymin,i=0;dist<=Ymax;dist+=dY) i++;
   nY = i;
   for(dist=0,i=0;dist<=Zmax;dist+=dZ) i++;
   nZ = i;
}

/* Define structures used to store pick & hypocenter info
 *************************************************************/
static struct {
        char    cdate[19];
        double  t;
        int     dlat;
        float   mlat;
        char    ns;
        int     dlon;
        float   mlon;
        char    ew;
        float   z;
        float   rms;
        float   dmin;
        int     gap;
        int     nph;
        long    id;
        char    version;
} Hyp;

typedef struct {
        char    site[TRACE2_STA_LEN];
        char    comp[TRACE2_CHAN_LEN];
        char    net[TRACE2_NET_LEN];
        char    loc[TRACE2_LOC_LEN];
        char    cdate[19];
        double  t;
        char    phase[3];
        char    fm;
        char    qual;
        long    pamp[3];
        long    paav;
        long    caav[6];
        float   ccntr[6];       /* added by this program */
        int     clen;
        char    datasrc;
} PCK;
static PCK PckArray[MAX_PICKS];
static PCK Pck;
void  copy_phase(PCK *, PCK *); /* a helper function to copy the PCK struct from one element to another */
void  eqm_bldphs ( PCK *, char * );

int main( int argc, char *argv[] )
{
   MSG_LOGO       logo;
   static char inmsg[MAX_BYTES_PER_EQ];
   static char outmsg[MAX_BYTES_PER_EQ];

   int type;
   int rc;
   int exit_status = 0;
   int play_one_event_file = 0;
   RingName[0] = 0;
   SourceCode = ' ';

/* Check command line arguments
   ****************************/
   if ( argc == 1 )
   {
      fprintf( stderr, "Usage: eqmaxel config_file.d [test_event_scnl_file]\n" );
      fprintf( stderr, "eqmaxel: Version %s\n", VERSION_STR );
      exit( 0 );
   }

/* Initialize some variables
 ***************************/
/*   memset(  NextProcess, 0, (size_t)MAXLEN  ); */

/* Initialize name of log-file & open it
 ***************************************/
   logit_init( argv[1], 0, MAX_BYTES_PER_EQ, 1 );

/* Read the configuration file(s)
 ********************************/
   eqm_config( argv[1] );
   logit( "" , "eqmaxel: Read command file <%s>\n", argv[1] );

   set_origin_coord(Xc, Yc); 	/* set up the maxel grid */
   logit( "" , "eqmaxel: grid coordinates set to Xc=%f Yc=%f\n", Xc, Yc);
   init_maxel(); /* set a number of critical global vars for MAXEL computation */
   logit( "" , "eqmaxel: initial values set\n");

/* Look up message types in earthworm.h tables
 *********************************************************/
   if ( GetType( "TYPE_EVENT_SCNL", &TypeEventSCNL ) != 0 ) {
      fprintf( stderr,
              "eqmaxel: Invalid message type <TYPE_EVENT_SCNL>; exiting!\n" );
      exit( -1 );
   }
   if ( GetType( "TYPE_HYP2000ARC", &TypeHyp2000Arc ) != 0 ) {
      fprintf( stderr,
              "eqmaxel: Invalid message type <TYPE_HYP2000ARC>; exiting!\n" );
      exit( -1 );
   }
   if ( GetType( "TYPE_KILL", &TypeKill ) != 0 ) {
      fprintf( stderr,
              "eqmaxel: Invalid message type <TYPE_KILL>; exiting!\n" );
      exit( -1 );
   }
   if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
      fprintf( stderr,
              "eqmaxel: Invalid message type <TYPE_ERROR>; exiting!\n" );
      exit( -1 );
   }
   if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
      fprintf( stderr,
              "eqmaxel: Invalid message type <TYPE_HEARTBEAT>; exiting!\n" );
      exit( -1 );
   }


/* Reinitialize logit to desired logging level
 *********************************************/
   logit_init( argv[1], 0, MAX_BYTES_PER_EQ, LogSwitch );
   
/* Spawn the next process
 ************************/
/*   if ( pipe_init( NextProcess, (unsigned long)0 ) == -1 )
   {
      logit( "e", "eqmaxel: Error starting next process <%s>; exiting!\n",
              NextProcess );
      exit( -1 );
   }
   logit( "e", "eqmaxel: piping output to <%s>\n", NextProcess );
*/
   if (use_phase_wgt)
       logit( "", "eqmaxel: Using Phase Weighting\n");

/* if argc=3, then we have a single run play of a TYPE_EVENT_SCNL file */
   if (argc == 3) {
       int nread;
       FILE *fp;
       fp = fopen(argv[2], "r");
       nread = fread( inmsg, (size_t) 1, (size_t)MAX_BYTES_PER_EQ, fp );
       inmsg[nread] = '\0';
       fprintf(stdout, "Using Event SCNL file: %s\nContaining:\n%s\n", argv[2], inmsg);
       if( !feof(fp) ) {
          logit("e","eqmaxel: Error processing filename <%s>; ", argv[2] );
          if( nread==MAX_BYTES_PER_EQ ) {
             logit("e","file longer than msgbuffer[%d]\n", MAX_BYTES_PER_EQ );
          } else {
             logit("e","trouble reading file\n" );
          }
          exit(2);
       }
       fclose(fp);
       play_one_event_file = 1;
   } else {
	/* we are in real-time mode, require RingName */
       if (RingName[0] == 0) 
       {
          fprintf( stderr,
                  "eqmaxel: RingName parameter not specified, exiting.\n");
          exit( -1 );
       }
       if( (RingKey = GetKey(RingName)) == -1 )
       {
          fprintf( stderr,
                  "eqmaxel: Invalid ring name <%s>. Exiting.\n", RingName );
          exit( -1 );
       }
       /* attach to the ring */
       tport_attach( &Region, RingKey );
       logit( "t", "eqmaxel: attached to Transport ring %s for writing\n", RingName);

       /* real-time mode also requires we have an EW_INSTALLATION set in the env */
       if ( GetLocalInst( &InstId ) != 0 )
       {
          fprintf( stderr,
                  "eqmaxel: error getting local installation id. Exiting.\n" );
          exit( -1 );
       }
   }

/*-------------------------Main Program Loop----------------------------*/

   do
   {
   /* Get a message from the pipe
    *****************************/
      if (play_one_event_file==0) {
         if (verbose) logit( "t", "eqmaxel: Debug, entering pipe_get()\n");
         rc = pipe_get( inmsg, InBufl, &type );
         if (verbose) logit( "t", "eqmaxel: Debug, got back from pipe_get() rc=%d, type=%d\n", rc, type);
      } else {
         type = TypeEventSCNL;
         rc = 0;
      }

      if ( rc < 0 )
      {
         if ( rc == -1 )
            logit( "et", "eqmaxel: Message in pipe too big for buffer\n" );
         if ( rc == -2 )
            logit( "et", "eqmaxel: <null> on pipe_get.\n" );
         if ( rc == -3 )
            logit( "et", "eqmaxel: EOF on pipe_get.\n" );
         exit_status = rc;
         break;
      }

   /* Process the event messages
    ****************************/
      if ( type == (int) TypeEventSCNL )
      {
           if (verbose) logit("t", "eqmaxel: incoming TYPE_EVENT_SCNL message:\n%s\n", inmsg ); /*DEBUG*/
           if ( eqm_eventscnl( inmsg, outmsg ) == -1 ) {
                logit( "et", "eqmaxel: Error processing TYPE_EVENT_SCNL message.\n" );
                continue;
           } 
           if ( LogArcMsg ) {
                logit( "", "ARCOUT:\n%s", outmsg ); 
           }
           if (play_one_event_file) {
                fprintf(stdout,"run completed, exiting\n");
                exit(0);
           }
/* Send the archive file to the transport ring
   *******************************************/
           logo.instid = InstId;
           logo.mod    = MyModId;
           logo.type   = TypeHyp2000Arc;
           if ( tport_putmsg( &Region, &logo, (long) strlen(outmsg), outmsg ) != PUT_OK )
           {
              logit( "et",
                     "eqmaxel: Error sending archive msg to transport ring.\n" );
           }

      /*
           if ( pipe_put( outmsg, (int) TypeHyp2000Arc ) != 0 ) {
                logit( "et", "eqmaxel: Error writing msg to pipe.\n"); 
           }
       */
         /*printf("eqmaxel: outgoing message:\n%s\n", outmsg );*/ /*DEBUG*/
      }

   /* Pass all other types of messages along
    ****************************************/
      else if ( type == (int) TypeHeartBeat )
      {
/*
         if ( pipe_put( inmsg, type ) != 0 )
*/
          if (verbose) logit("t", "eqmaxel: incoming HB message:\n%s\n", inmsg ); /*DEBUG*/
          logo.instid = InstId;
          logo.mod    = MyModId;
          logo.type   = type;
          if ( tport_putmsg( &Region, &logo, strlen(inmsg), inmsg ) != PUT_OK )
          {
            logit( "et",
                   "eqmaxel: Error passing msg (type %d) to transport ring.\n",
                    (int) type );
          }
      }
      else if ( type == (int) TypeKill )
      {
           logit( "et", "eqmaxel: Termination requested; ending main loop\n" );
      }

/*
      if ( pipe_error() )
      {
           logit( "et", "eqmaxel: Output pipe error; exiting\n" );
           break;
      }
*/

/* Terminate when a kill message is received
 *******************************************/
   } while ( type != (int) TypeKill );

/*-----------------------------End Main Loop----------------------------*/

/* Send a kill message to the child.
   Wait for the child to die.
 ***********************************/
 /* pipe_put( "", (int) TypeKill ); 
    sleep_ew( 500 ); 
    pipe_close();
 */
   tport_detach( &Region );
   logit( "et", "eqmaxel: exiting\n" );
   return( exit_status );
}

/****************************************************************************/
/* eqm_config() processes the configuration file using kom.c functions      */
/*              exits if any errors are encountered                         */
/****************************************************************************/
#define ncommand  7       /* # of required commands you expect to process   */
void eqm_config( char *configfile )
{
   char     init[ncommand]; /* init flags, one byte for each required command */
   int      nmiss;          /* number of required commands that were missed   */
   char    *com;
   char    *str;
   int      nfiles;
   int      success;
   int      i;
   char     processor[15]; /* used by site_com() call */
   double z, dtdr, dtdz;

   trtable_filename[0] = 0;

/* Set to zero one init flag for each required command
 *****************************************************/
   for( i=0; i<ncommand; i++ )  init[i] = 0;
   init[2] = 1; /* PipeTo - not needed at this version since we write results to a ring! */
   init[3] = 1; /* LableAsBinder is really optional */

/* Open the main configuration file
 **********************************/
   nfiles = k_open( configfile );
   if ( nfiles == 0 ) {
        logit( "e",
                "eqmaxel: Error opening command file <%s>; exiting!\n",
                 configfile );
        exit( -1 );
   }

/* Process all command files
 ***************************/
   while(nfiles > 0)   /* While there are command files open */
   {
        while(k_rd())        /* Read next line from active file  */
        {
            com = k_str();         /* Get the first token from line */

        /* Ignore blank lines & comments
         *******************************/
            if( !com )           continue;
            if( com[0] == '#' )  continue;

        /* Open a nested configuration file
         **********************************/
            if( com[0] == '@' ) {
               success = nfiles+1;
               nfiles  = k_open(&com[1]);
               if ( nfiles != success ) {
                  logit( "e",
                          "eqmaxel: Error opening command file <%s>; exiting!\n",
                           &com[1] );
                  exit( -1 );
               }
               continue;
            }

        /* Process anything else as a command
         ************************************/
         /* Numbered commands are required
          ********************************/
   /*0*/    if( k_its( "LogFile" ) )
            {
                LogSwitch = k_int();
                init[0] = 1;
            }
 /* opt */  else if ( k_its( "RingName" ) ) {  /* note this is not required if in test mode */
                str = k_str();
                if(str) strcpy(RingName, str);
            }
 /* opt */  else if( k_its( "SourceCode" ) ) {
                str = k_str();
                SourceCode = str[0];
            }
 /* opt */  else if( k_its( "UsePhaseWeight" ) ) {
                   use_phase_wgt = 1;
            }
   /*1*/    else if( k_its( "MyModuleId" ) )
            {
                if ( ( str=k_str() ) ) {
                   if ( GetModId( str, &MyModId ) != 0 ) {
                      logit( "e",
                             "eqmaxel: Invalid module name <%s>; exiting!\n",
                              str );
                      exit( -1 );
                   }
                }
                init[1] = 1;
            }
    /*2*/   else if( k_its("PipeTo") )
            {
                str = k_str();
                if( str && strlen(str)<MAXLEN ) {
                   strcpy( NextProcess, str );
                }
                else {
                   logit( "e", "eqmaxel: Invalid PipeTo command argument; "
                           "must be 1-%d chars long; exiting!\n",
                            MAXLEN-1 );
                   exit( -1 );
                }
                init[2] = 1;
            }
    /*3*/   else if( k_its("LabelAsBinder") )
            {
                LabelAsBinder = k_int();
                init[3] = 1;
            }
    /*4*/   else if( k_its("CenterLongitude") )
            {
                Xc = k_val();
                init[4] = 1;
            }
    /*5*/   else if( k_its("CenterLatitude") )
            {
                Yc = k_val();
                init[5] = 1;
            }
            else if( k_its("LogArcMsg") )   /* optional */
            {
		LogArcMsg = k_int();
            }
    /*6*/   else if( k_its("TravelTimeTable") )
            {
                str = k_str();
		/* NEED to do a check on size, but this may get removed when we support Lay commands later` */
		strcpy(trtable_filename, str);
   		read_trtable(trtable_filename);	/* call to maxel c function found in trtable.c */
                init[6] = 1;
            }
            else if( k_its("Verbose") )  /*Optional*/
            {
                verbose = k_int();
            }
            else if( k_its("LabelVersion") )  /*Optional*/
            {
                LabelVersion = k_int();
            }
            else if( site_com()   ) strcpy( processor, "site_com" );
            else if( t_com()   ) strcpy( processor, "site_com" );
            else
            {
                logit( "e", "eqmaxel: <%s> Unknown command in <%s>.\n",
                        com, configfile );
                continue;
            }

        /* See if there were any errors processing the command
         *****************************************************/
            if( k_err() ) {
               logit( "e",
                       "eqmaxel: Bad <%s> command in <%s>; exiting!\n",
                        com, configfile );
               exit( -1 );
            }
        }
        nfiles = k_close();
    }

/* After all files are closed, check init flags for missed commands
 ******************************************************************/
    if (init[6] == 0 ) {
         z = FIX_Z;
         trtable = (float *)malloc((MAXDIST+1)*sizeof(float));
         for ( i=0; i<=MAXDIST; i++ ) {
             trtable[i] = (float) t_lay( (double) i, z, &dtdr, &dtdz);
#if DEBUG_VELOCITY
             fprintf(stdout, "%d %f\n", i, trtable[i]);
#endif
         }
         init[6] = 1;
    }
 
    
    nmiss = 0;
    for ( i=0; i<ncommand; i++ )  if( !init[i] ) nmiss++;
    if ( nmiss ) {
       logit( "e", "eqmaxel: ERROR, no " );
       if ( !init[0] )  logit( "e", "<LogFile> "       );
       if ( !init[1] )  logit( "e", "<MyModuleId> "    );
       if ( !init[2] )  logit( "e", "<PipeTo> "        );
       if ( !init[3] )  logit( "e", "<LabelAsBinder> " );
       if ( !init[4] )  logit( "e", "<CenterLongitude> " );
       if ( !init[5] )  logit( "e", "<CenterLatitude> " );
       if ( !init[6] )  logit( "e", " Velocity Model definition or <TravelTimeTable> " );
       logit( "e", "command(s) in <%s>; exiting!\n", configfile );
       exit( -1 );
    }

    return;
}

/**************************************************************************/
/* eqm_eventscnl() supervises does all the work for one event message     */
/**************************************************************************/
int eqm_eventscnl( char *inmsg,   /* event message from eqproc/eqprelim */
                   char *outmsg ) /* event message to build here        */
{
   char  *in, *out;             /* working pointers to messages */
   char   line[126];            /* to store lines from inmsg    */
   int    inmsglen;             /* length of input message      */
   int    nline;                /* number of lines processed    */
   int    phases_used = 0;	/* will be set to nobs for maxel later */
   EVNINFO event;	/* structure initialized by us, returned by MAXEL computation */
   int    i;


/* Initialize some stuff
 ***********************/
   inmsglen = strlen( inmsg );
   in       = inmsg;
   out      = outmsg;
   nline    = 0;

/* Read & process one line at a time from inmsg
 **********************************************/
   while( in < inmsg+inmsglen )
   {
      if ( sscanf( in, "%[^\n]", line ) != 1 )  return( -1 );
      /*logit( "", "nline%3d: %s\n", nline, line );*/  /*DEBUG*/
      in += strlen( line ) + 1;

/* Process the hypocenter card (1st line of msg)
 ***********************************************/
      if ( nline == 0 ) {
         if( eqm_readhyp( line ) == -1 ) {
             logit("et", "eqmaxel: error reading hypocenter TYPE_EVENT_SCNL message; skip this event!\n");
             return( -1 );
         }
         logit("t", "Processing eventid: %ld\n", Hyp.id );
         /* allocate obs list */
         obslist = (OBSINFO *)malloc(Hyp.nph*sizeof(OBSINFO));

         nline++;
      }
/* Process all the phase cards (remaining lines of msg)
 ******************************************************/
      else {
         if( eqm_readphs( line ) != -1 )  {
           int isite;
           nline++;
           isite = site_index(Pck.site, Pck.net, Pck.comp, Pck.loc);
           /* need to do check to see if Site does not exist */
           if( isite < 0 ) {
      		logit( "et", "WARNING: %s.%s.%s.%s - Not in station list, not using phase",
                      Pck.site, Pck.comp, Pck.net, Pck.loc );
	   } else {
                double x, y;
                calc_deg2km(Site[isite].lon, Site[isite].lat, &x, &y);
      		if (verbose) logit( "et", "INFO: using %s.%s.%s.%s %s lat=%f lon=%f Phs=%s x=%f y=%f epoch=%f\n",
                      	Pck.site, Pck.comp, Pck.net, Pck.loc, Pck.cdate, 
			Site[isite].lat, Site[isite].lon, Pck.phase, x, y, Pck.t);
                /* assumes that eqassmble will continue to build a time sorted phase list */
                if (phases_used == 0) {
                  obslist[phases_used].ti = Pck.t;
                } else {
                  obslist[phases_used].ti = Pck.t - obslist[0].ti; 	/* storing only time diffs from 1st phs */
                }
 		obslist[phases_used].epoch = Pck.t;
                strcpy(obslist[phases_used].stn, Site[isite].name); /* really only need index into Site[] */
                strcpy(obslist[phases_used].ntw, Site[isite].net); /* really only need index into Site[] */
                obslist[phases_used].lat = Site[isite].lat;
                obslist[phases_used].lon = Site[isite].lon;
                obslist[phases_used].elev = Site[isite].elev;
                obslist[phases_used].x = x;
                obslist[phases_used].y = y;
                if (use_phase_wgt) {
                  switch(Pck.qual - '0') {
                        case 0: 
                            obslist[phases_used].wgt = 1.0;
                            break;
                        case 1: 
                            obslist[phases_used].wgt = 0.75;
                            break;
                        case 2: 
                            obslist[phases_used].wgt = 0.50;
                            break;
                        case 3: 
                            obslist[phases_used].wgt = 0.25;
                            break;
                        default:
                            obslist[phases_used].wgt = 0.0;
                            logit("et", "Error: unknown phase quality value of %d, not using phase %s %s %s %s\n",
					Pck.qual, Pck.site, Pck.comp, Pck.net, Pck.loc);
                  }
                } else {
                  obslist[phases_used].wgt = 1.0;
                }
                copy_phase(&PckArray[phases_used], &Pck);
                phases_used++;
           }
         }
      }
   } /*end while of message loop*/
   nobs = phases_used;
   Hyp.nph = phases_used; /* note that TYPE_EVENT_SCNL nph can be wrong due to dup filterig in binder_ew */
   obslist[0].ti=0.0; /* for time diffs, set first phase to 0.0 secs */
   logit("et", "DEBUG %d phases will be used\n", phases_used);


/* Make sure we processed at least one phase per event
 *****************************************************/
   if( nline <= 1 ) return( -1 );

   eqm_calc(&event); /* run the MLE solution */

   eqm_bldhyp(&event, out);
   out = outmsg + strlen( outmsg );

   for(i=0; i< phases_used; i++) {
      eqm_bldphs(&PckArray[i], out);
      out = outmsg + strlen( outmsg );
   }


/* Add the terminator card to end of outmsg
 ******************************************/
   eqm_bldterm( out ); 
   logit("t", "Finished processing eventid: %ld\n", Hyp.id );

   if( verbose == 2 ) 
   {
      /* make a GMT script of the results */
      logit("t", "Running GMT script and data creation for eventid: %ld\n", Hyp.id );
      logit("t", "Warning, GMT files over written with each run in this version \n");
      make_evmapsc(&event);
      make_section(&event, event.x1,event.y1,event.z1,Xmin,Xmax,Ymin,Ymax,dX,dZ,0,1);
      make_section(&event, event.x,event.y,event.z,event.x1-20.,event.x1+20.,
                event.y1-20.,event.y1+20.,2.,2.,0,2);
   }
   free(obslist);
   return( 0 );
}


/***************************************************************************/
/* eqm_readhyp() decodes a TYPE_EVENT_SCNL hypocenter card into Hyp struct */
/***************************************************************************/
int eqm_readhyp( char *card )
{
   float  lat,lon;
   int    rc;

/*------------------------------------------------------------------------------------
Sample binder-based hypocenter as built below (whitespace delimited, variable length);
Event id from binder is added at end of card.
19920429011704.653 36.346578 -120.546932 8.51 27 78 19.8 0.16 10103 1\n
--------------------------------------------------------------------------------------*/

/* Scan line for required values
 *******************************/
   rc = sscanf( card, "%s %f %f %f %d %d %f %f %ld %c",
                Hyp.cdate,
                &lat,
                &lon,
                &Hyp.z,
                &Hyp.nph,
                &Hyp.gap,
                &Hyp.dmin,
                &Hyp.rms,
                &Hyp.id,
                &Hyp.version );

   if( rc < 10 )
   {
      logit( "t", "eqm_readhyp: Error: read only %d of 10 fields "
                  "from hypocenter line: %s\n", rc, card );
      return( -1 );
   }

/* Read origin time
 ******************/
   Hyp.t = julsec18( Hyp.cdate );
   if ( Hyp.t == 0. ) 
   {
      logit( "t", "eqm_readhyp: Error decoding origin time: %s\n",
                   Hyp.cdate );
      return( -1 );
   }

/* Convert decimal Lat & Lon to degree/minutes
 **********************************************/
   if( lat >= 0.0 ) {
      Hyp.ns = 'N';
   } else {
      Hyp.ns = 'S';
      lat    = -lat;
   }
   Hyp.dlat = (int)lat;
   Hyp.mlat = (lat - (float)Hyp.dlat) * 60.0;

   if( lon >= 0.0 ) {
      Hyp.ew = 'E';
   } else {
      Hyp.ew = 'W';
      lon    = -lon;
   }
   Hyp.dlon = (int)lon;
   Hyp.mlon = (lon - (float)Hyp.dlon) * 60.0;

   return( 0 );
}

/***************************************************************************/
/* eqm_readphs() decodes a phase card into the Pck structure               */
/***************************************************************************/
int eqm_readphs( char *card )
{
   int rc;

/*--------------------------------------------------------------------------
Sample Earthworm format phase card (variable-length whitespace delimited):
CMN VHZ NC -- U1 P 19950831183134.902 953 1113 968 23 201 276 289 0 0 7 W\n
----------------------------------------------------------------------------*/
   rc = sscanf( card, 
               "%s %s %s %s %c%c %s %s %ld %ld %ld %ld %ld %ld %ld %ld %ld %d %c",
                 Pck.site,
                 Pck.comp,
                 Pck.net,
                 Pck.loc,
                &Pck.fm,
                &Pck.qual,
                 Pck.phase,
                 Pck.cdate,
                &Pck.pamp[0],
                &Pck.pamp[1],
                &Pck.pamp[2],
                &Pck.caav[0],
                &Pck.caav[1],
                &Pck.caav[2],
                &Pck.caav[3],
                &Pck.caav[4],
                &Pck.caav[5],
                &Pck.clen,
                &Pck.datasrc );

   if( rc < 19 )
   {
      logit( "t", "eqm_readphs: Error: read only %d of 19 fields "
                  "from hypocenter line: %s\n", rc, card );
      return( -1 );
   }

/* Read pick time
 ****************/
   Pck.t = julsec18( Pck.cdate );
   if ( Pck.t == 0. ) 
   {
      logit( "t", "eqm_readphs: Error decoding arrival time: %s\n",
                   Pck.cdate );
      return( -1 );
   }

   return( 0 );
}
void copy_phase(PCK *to, PCK *from) {
       	strcpy(to->site, from->site);
       	strcpy(to->comp, from->comp);
       	strcpy(to->net, from->net);
       	strcpy(to->loc, from->loc);
	to->fm = from->fm;
	to->qual = from->qual;
       	strcpy(to->phase, from->phase);
       	strcpy(to->cdate, from->cdate);
	to->pamp[0] = from->pamp[0];
	to->pamp[1] = from->pamp[1];
	to->pamp[2] = from->pamp[2];
	to->caav[0] = from->caav[0];
	to->caav[1] = from->caav[1];
	to->caav[2] = from->caav[2];
	to->caav[3] = from->caav[3];
	to->caav[4] = from->caav[4];
	to->caav[5] = from->caav[5];
	to->clen = from->clen;
	to->datasrc = from->datasrc;
	/* computed value */
	to->t = from->t;
}

/*************************************************************************/
/* eqm_calc() does maxel computation  					 */
/*************************************************************************/
void eqm_calc( EVNINFO *event )
{
int i;
char origin_char[25];

   logit("t", "DEBUG: about to run assoc_all()\n");
   event->nph = nobs;
   event->lat = 0.0;
   event->lon = 0.0;
   for(i=0;i<nobs;i++)
        event->phlist[i] = i;
   assoc_all(event); /* this is the call that does all the work */
   logit("t", "DEBUG: about to run find_confidence()\n");
   find_confidence(event);
   logit("t", "DEBUG: about to run find_gap()\n");
   find_gap(event);
   /* normalize the MLE value based on the number of phases used */
   event->MLE = pow(event->MLE/event->snph,1./(event->snph-1));

   logit("t", "Event Location %-9.4f %-8.4f %-5.2f  %-6.1f %-6.1f  %8.4f %8.4f %.1f %.1f %.1f %.2f\n",
        event->lon,event->lat,event->z,event->x,event->y,
        event->stmad,event->atmad,event->cerarea, event->mindist, event->pgap,
        event->MLE);
   date17(event->origin_epoch, origin_char);
   logit("t", "Event Origin: Epoch=%f  %s\n", event->origin_epoch, 
	origin_char);
   logit("t", "Error Ellipse: Azimuth %.1f  Major&Minor axis: %.2f %.2f, MLE: %.2f\n",
                event->cerazi, event->cerlmj, event->cerlmn, event->MLE);
   logit("t", "           Center: %.4f %.4f    Area: %.1f km\n",
                event->cerlon, event->cerlat, event->cerarea);
   logit("t", "Primary & Secondary gap: %.1f %.1f   Min.dist.: %.1f km\n",
                event->pgap, event->sgap, event->mindist);
/*   return; */
}

/***************************************************************************/
/* eqm_bldhyp() builds a hyp2000 (hypoinverse) summary card & its shadow   */
/***************************************************************************/
void eqm_bldhyp( EVNINFO *event, char *hypcard )
{
   char line[170];   /* temporary working place */
   char shdw[170];   /* temporary shadow card   */
   int  tmp;
   int  i;
   double lat, lon;
   char cdatestr[18];

/* Put all blanks into line and shadow card
 ******************************************/
   for ( i=0; i<170; i++ )  line[i] = shdw[i] = ' ';

/*----------------------------------------------------------------------------------
Sample HYP2000 (HYPOINVERSE) hypocenter and shadow card.  Binder's eventid is stored 
in cols 136-145.  Many fields will be blank due to lack of information from binder.
(summary is 165 chars, including newline; shadow is 81 chars, including newline):
199806262007418537 3557118 4836  624 00 26 94  2   633776  5119810  33400MOR  15    0  32  50 99
   0 999  0 11MAM WW D189XL426 80         51057145L426  80Z464  102 \n
$1                                                                              \n
0123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 12345
6789 123456789 123456789 123456789 123456789 123456789 123456789 123456789
-----------------------------------------------------------------------------------*/

/* Write out hypoinverse summary card
 ************************************/
   date17(event->origin_epoch, cdatestr);
   strncpy( line,     cdatestr,     14 );
   strncpy( line+14,  cdatestr+15,  2  );

/* MLE lat/lon are in decimal degrees, revert to deg and min */
   lat = event->lat;
   lon = event->lon;

   if( lat >= 0.0 ) {
      Hyp.ns = 'N';
   } else {
      Hyp.ns = 'S';
      lat    = -lat;
   }
   Hyp.dlat = (int)lat;
   Hyp.mlat = (lat - (float)Hyp.dlat) * 60.0;

   if( lon >= 0.0 ) {
      Hyp.ew = 'E';
   } else {
      Hyp.ew = 'W';
      lon    = -lon;
   }
   Hyp.dlon = (int)lon;
   Hyp.mlon = (lon - (float)Hyp.dlon) * 60.0;



   sprintf( line+16,  "%2d%c", Hyp.dlat, Hyp.ns );
   tmp = (int) (Hyp.mlat*100.0);
   sprintf( line+19,  "%4d", (int) MIN( tmp, 9999 ) );
   sprintf( line+23,  "%3d%c", Hyp.dlon, Hyp.ew );
   tmp = (int) (Hyp.mlon*100.0);
   sprintf( line+27,  "%4d", (int) MIN( tmp, 9999 ) );
   tmp = (int) (Hyp.z*100.0);
   sprintf( line+31,  "%5d", (int) MIN( tmp, 99999 ) ); 
   sprintf( line+39,  "%3d", (int) MIN( Hyp.nph, 999 ) );
   sprintf( line+42,  "%3d", (int) MIN( event->pgap, 999 ) );

/* put the MLE  minimum distance in */
   sprintf( line+45,  "%3d", (int) MIN( event->mindist, 999 ) );

/* putting the MLE metric value in the rms slot */
   tmp = (int) (event->MLE*100.);
   sprintf( line+48,  "%4d", (int) MIN( tmp, 9999 ) );

/* MLE location error metrics go here: hacked into hypoinverse message summary */
/* major error ellips axes  from MLE */
   sprintf( line+52,  "%3d", (int) MIN( event->cerazi, 999 ) ); /* azi principal */
   sprintf( line+55,  "00"); /* dip is zero, no 3D solution  */
   tmp = (int) (event->cerlmj*100.);
   sprintf( line+57,  "%4d", (int) MIN( tmp, 9999 ) ); /* size of major axis */
   tmp = (int) (event->cerazi + 90.);
   if (tmp >= 360) tmp -= 180;
   sprintf( line+61,  "%3d", (int) MIN( tmp, 999 ) ); /* azi */
   sprintf( line+64,  "00"); /* dip is zero, no 3D solution  */
   tmp = (int) (event->cerlmn*100.);
   sprintf( line+66,  "%4d", (int) MIN( tmp, 9999 ) ); /* size of minor axis , in intermediate for hinv*/
   sprintf( line+76,  "0000"); /* size of smallest ellipsoid axis is zero for MLE*/
   sprintf( line+80,  "%c", SourceCode); /* source of solution: Hinv=Auxiliary remark from analyst  */
   tmp = (int) ( event->cerarea/1000.0); /* area is in units of 1000 square km */
   sprintf( line+85,  "%4d", (int) MIN( tmp, 9999 ) ); /* going into Horizontal Error value for hinv */

   sprintf( line+136, "%10ld", Hyp.id );
   if(LabelVersion) line[162] = Hyp.version;
   else             line[162] = ' ';

   for( i=0; i<164; i++ ) if( line[i]=='\0' ) line[i] = ' ';
   sprintf( line+164, "\n" );

/* Write out summary shadow card
 *******************************/
   sprintf( shdw,     "$1"   );
   for( i=0; i<80; i++ ) if( shdw[i]=='\0' ) shdw[i] = ' ';
   sprintf( shdw+80,  "\n" );

/* Copy both to the target address
 *********************************/
   strcpy( hypcard, line );
   strcat( hypcard, shdw );
   return;
}

/*******************************************************************************/
/* eqm_bldphs() builds a hyp2000 (hypoinverse) archive phase card & its shadow */
/*******************************************************************************/
void eqm_bldphs( PCK *p, char *phscard )
{
   char line[125];     /* temporary phase card    */
   char shdw[125];     /* temporary shadow card   */
   int   i;

/* Put all blanks into line and shadow card
 ******************************************/
   for ( i=0; i<125; i++ )  line[i] = shdw[i] = ' ';

/*-----------------------------------------------------------------------------
Sample Hyp2000 (hypoinverse) station archive cards (for a P-arrival and 
an S-arrival) and a sample shadow card. Many fields are blank due to lack 
of information from binder. Station phase card is 121 chars, including newline; 
shadow is 96 chars, including newline:
MTC  NC  VLZ  PD0199806262007 4507                                                0      79                 W  --       \n
MTC  NC  VLZ    4199806262007 9999        5523 S 1                                4      79                 W  --       \n
$   6 5.27 1.80 4.56 1.42 0.06 PSN0   79 PHP2 1197 39 198 47 150 55 137 63 100 71  89 79  48   \n
0123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 12
-------------------------------------------------------------------------------*/
/* Build station archive card for a P-phase
 ******************************************/
   if( p->phase[0] == 'P' || p->phase[1] == 'P' )
   {
        strncpy( line,    p->site, 5 );
        strncpy( line+5,  p->net,  2 );
        strncpy( line+9,  p->comp, 3 );
        if( LabelAsBinder ) strncpy( line+13, p->phase, 2 );  
        else                strncpy( line+13, " P",      2 );
        if( p->fm == '?' ) line[15] = ' ';
        else                line[15] = p->fm;
        line[16] = p->qual;
        strncpy( line+17, p->cdate,   12 );        /* yyyymmddhhmm    */
        strncpy( line+30, p->cdate+12, 2 );        /* whole secs      */
        strncpy( line+32, p->cdate+15, 2 );        /* fractional secs */
                                                    /* overflows format          */
        line[108] = p->datasrc;
        strncpy( line+111, p->loc, 2 );

        for( i=0; i<120; i++ ) if( line[i]=='\0' ) line[i] = ' ';
        sprintf( line+120, "\n" );
   }

/* Build station archive card for a S-phase
 ******************************************/
   else
   {
   /* Build the phase card */
        strncpy( line,    p->site, 5 );
        strncpy( line+5,  p->net,  2 );
        strncpy( line+9,  p->comp, 3 );
        line[16] = '4';  /* weight out P-arrival; we're loading a dummy time */
        strncpy( line+17, p->cdate, 12 );    /* real year,mon,day,hr,min    */
        sprintf( line+29,  " 9999"      );    /* dummy seconds for P-arrival */
        strncpy( line+42, p->cdate+12, 2 );  /* actual whole secs S-arrival */
        strncpy( line+44, p->cdate+15, 2 );  /* actual fractional secs S    */
        if( LabelAsBinder ) strncpy( line+46, p->phase, 2 );
        else                strncpy( line+46, " S",      2 );
        line[49] = p->qual;
        line[108] = p->datasrc;
        strncpy( line+111, p->loc, 2 );

        for( i=0; i<120; i++ ) if( line[i]=='\0' ) line[i] = ' ';
        sprintf( line+120, "\n" );
   }

/* Build the shadow card
 ***********************/

   for( i=0; i<95; i++ ) if( shdw[i]=='\0' ) shdw[i] = ' ';
   sprintf( shdw+95, "\n" );

/* Copy both to the target address
 *********************************/
   strcpy( phscard, line );
   strcat( phscard, shdw );
   return;
}

/***************************************************************************/
/* eqm_bldterm() builds a hypoinverse event terminator card & its shadow   */
/***************************************************************************/
void eqm_bldterm( char *termcard )
{
        char line[100];   /* temporary working place */
        char shdw[100];   /* temporary shadow card   */
        int  i;

/* Put all blanks into line and shadow card
 ******************************************/
        for ( i=0; i<100; i++ )  line[i] = shdw[i] = ' ';

/* Build terminator
 ******************/
        sprintf( line+62, "%10ld\n",  Hyp.id );

/* Build shadow card
 *******************/
        sprintf( shdw, "$" );
        shdw[1] = ' ';
        sprintf( shdw+62, "%10ld\n",  Hyp.id );

/* Copy both to the target address
 *********************************/
        strcpy( termcard, line );
        strcat( termcard, shdw );
        return;
}
