/******************************************************************************
 *
 *	File:			ewaccel.c
 *
 *	Function:		Program to read 
 *
 *	Author(s):		Scott Hunter, ISTI
 *
 *	Source:			ewthresh.c
 *
 *	Notes:
 *
 *	Change History:
 *			5/23/11	Started source
 *
 *****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "earthworm.h"
#include "time_ew.h"
#include "kom.h"
#include "transport.h"
#include "swap.h"
#include "mem_circ_queue.h"
#include "trace_buf.h"
#include "butterworth_c.h"


#define ACCEL_VERSION "1.7 2018-02-02"

/* Information about a highpass filter stage */
typedef struct _HP_STAGE
{
  double d1, d2;
  double f;
  double a1, a2, b1, b2;
} HP_STAGE;

/* Information specific to a floor */
typedef struct {
        double  mass;                  /* mass for this floor                      */
        double  convFactor;            /* conversion factor                        */
        char    *id;                   /* name of this floor; NULL for ground      */
        int     read;		       /* floor has been read for current time     */
        char    sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated)              */
        char    net[TRACE2_NET_LEN];   /* Network name (NULL-terminated)           */
        char    chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */
        char    loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated)          */
        void    *avgWindow;            /* rolling average window                   */
        double  sum;                   /* sum of samples in avgWindow              */
	HP_STAGE *hpLevel;             /* highpass level definitions               */
} EW_FLOOR_DEF;

int          alertOn = 0;	/* samples until alert is allowed again */
EW_FLOOR_DEF *flr = NULL;	/* array of floor definitions */
int          nFloor = 0;	/* number of floors, including ground */
int          nFloorAlloc = 5;   /* nbr of elements allocated in flr */
double       baseShearThreshold; /* threshold to trip alert */
double       avgWindowSecs;     /* # samples in averaging window */
char         dataType[3];       /* data type of samples in TRACEBUF2 record */
int          dataSize;          /* size of samples in TRACEBUF2 record */
int          awSamples;         /* nbr of samples in window */
size_t       awBytesAllocated = 0; /* space allocated for averaging window */
int          awPosn;		/* current element in averaging window */
int          awFull;		/* = "is window full?" */
int          nHPLevels;		/* nbr of highpass levels */
double       hpFreq=0.0;	/* highpass threshold frequency */
double       hpGain;		/* highpass gain */
int          hpOrder=0;		/* order of highpass filter */
complex      *hpPoles;		/* array of poles for highpass filter */
double       awStartTime;	/* time of first item in packet */
double       *awStartTimes;	/* cached awStartTime's */
int          numCachedStartTimes; /* no. of cached awStartTime's in awStartTimes */
int          floorsLeft;	/* nbr of floors not seen for current timeframe */
double       defaultConversion = 0.0; /* default conversion factor */
int          defaultConversionSet = 0; /* ="is defaultConversion set?" */
int          exportForces = 0;  /* write computed force info */
int          alertDelay;	/* seconds until another alert can possibly go off */
int          delaySamples;	/* alertDelay in samples */
double       samplePeriod;	/* period of samples in window */
double       jitter;            /* tolerance when comparing times (1/2 samplePeriod) */
char         *debugMsg = NULL;  /* space for debugging msg; no debugging if NULL */
char         *debugMsgTail;     /* tail of message (for concatenating) */
int          debugMsgSpace;     /* amount of space left in debugMsg */
double       sampleRate;	/* sample rate of data in window */
int          sampleCount;	/* # samples in a TRACEBUF packet */
double       packetDuration;	/* duration of a TRACEBUF packet */
float        *dataMatrix;       /* matrix of demeaned, filtered TRACEBUF vectors */


/* Functions in this source file
 *******************************/
static void ewaccel_config( char * );
static void ewaccel_lookup( void );
static void ewaccel_status( unsigned char, short, char * );
static void writeAlertMessage( double, int );
static void addForceToDump( double, int );
static void ewaccel_free( void );
static int  ResetSCNL( TracePacket * );
static int  InitSCNL();
static void ProcessDataMatrix( double, float * );
static double Filter( double, HP_STAGE * );
static int  ProcessPacket( TracePacket * );

/* Thread things
 ***************/
#define THREAD_STACK 8192
static ew_thread_t tidStacker;          /* Thread moving messages from transport */
                                     /*   to queue */

#define MSGSTK_OFF    0              /* MessageStacker has not been started      */
#define MSGSTK_ALIVE  1              /* MessageStacker alive and well            */
#define MSGSTK_ERR   -1              /* MessageStacker encountered error quit    */
volatile int MessageStackerStatus = MSGSTK_OFF;

QUEUE OutQueue;              /* from queue.h, queue.c; sets up linked    */
                                     /*    list via malloc and free              */
thr_ret MessageStacker( void * );    /* used to pass messages between main thread */
                                     /*   and Process thread */
thr_ret Process( void * );

/* Message Buffers to be allocated
 *********************************/
static char *MSrawmsg = NULL;        /* MessageStacker's "raw" retrieved message */
static char *SSmsg = NULL;        /* MessageStacker's "raw" retrieved message */

/* Timers
   ******/
time_t now;        /* current time, used for timing heartbeats */
time_t MyLastBeat;         /* time of last local (into Earthworm) hearbeat */

static  SHM_INFO  InRegion;     /* shared memory region to use for input  */
static  SHM_INFO  OutRegion;    /* shared memory region to use for output */

MSG_LOGO  GetLogo;             /* array for requesting module,type,instid */

char *Argv0;            /* pointer to executable name */
pid_t MyPid;        /* Our own pid, sent with heartbeat for restart purposes */
static ew_thread_t tidProcess;

/* Things to read or derive from configuration file
 **************************************************/
static char    InRing[MAX_RING_STR];          /* name of transport ring for input  */
static char    OutRing[MAX_RING_STR];         /* name of transport ring for output */
static char    MyModName[MAX_MOD_STR];        /* speak as this module name/id      */
static int     LogSwitch;           /* 0 if no logfile should be written */
static int     HeartBeatInt;        /* seconds between heartbeats        */
static long    MaxMsgSize;          /* max size for input/output msgs    */
static int     QueueSize = 10;      /* max messages in output circular buffer       */
static int     Debug = 0;           /* enable debugging messages         */

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static long          InRingKey;     /* key of transport ring for input    */
static long          OutRingKey;    /* key of transport ring for output   */
static unsigned char InstId;        /* local installation id              */
static unsigned char InstWildcard;
static unsigned char MyModId;       /* Module Id for this program         */
static unsigned char ModWildcard;
static unsigned char TypeHeartBeat;
static unsigned char TypeError;
static unsigned char Type_Tracebuf2;
static unsigned char TypeThreshAlert;


/* Error messages used by export
 ***********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring        */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer      */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded  */
#define  ERR_QUEUE         3   /* problem w/ queue                        */
static char  errText[256];     /* string for log/error messages           */
	
#define NUMREQ 12			/* # of required commands you expect to process   */
#ifdef _WINNT
/* Handle deprecation of strdup in windows */
static char* mystrdup( const char* src ) {
	char* dest = malloc( strlen(src) + 1 );
	if ( dest != NULL )
		strcpy( dest, src );
	return dest;
}
#else
#define mystrdup strdup
#endif

int main( int argc, char **argv )
{
/* Other variables: */
   int           res;
   long          recsize;   /* size of retrieved message             */
   MSG_LOGO      reclogo;   /* logo of retrieved message             */

   /* Check command line arguments
   ******************************/
   Argv0 = argv[0];
   if ( argc != 2 )
   {
      fprintf( stderr, "Usage: %s <configfile>\n", Argv0 );
      fprintf( stderr, "Version %s\n", ACCEL_VERSION );
      return 0;
   }

   /* Initialize name of log-file & open it
   ****************************************/
   logit_init( argv[1], 0, 512, 1 );

   /* Read the configuration file(s)
   ********************************/
   ewaccel_config( argv[1] );
   logit( "et" , "%s(%s): Read command file <%s>\n",
           Argv0, MyModName, argv[1] );
   logit("t", "starting ewaccel version %s\n", ACCEL_VERSION);

   /* Look up important info from earthworm.h tables
   *************************************************/
   ewaccel_lookup();

   /* Reinitialize the logging level
   *********************************/
   logit_init( argv[1], 0, 512, LogSwitch );

   /* Get our own Pid for restart purposes
   ***************************************/
   MyPid = getpid();
   if(MyPid == -1)
   {
      logit("e", "%s(%s): Cannot get pid; exiting!\n", Argv0, MyModName);
      return -1;
   }
   
   /* Prep for debiasing/filtering/alert-checking 
   ***********************************************************/
	InitSCNL();

   /* Allocate space for input/output messages for all threads
   ***********************************************************/

   /* Buffers for the MessageStacker thread: */
   if ( ( MSrawmsg = (char *) malloc(MaxMsgSize+1) ) ==  NULL )
   {
      logit( "e", "%s(%s): error allocating MSrawmsg; exiting!\n",
             Argv0, MyModName );
      ewaccel_free();
      return -1;
   }

   /* Attach to Input/Output shared memory ring
   ********************************************/
   tport_attach( &InRegion, InRingKey );
   tport_attach( &OutRegion, OutRingKey );

   /* step over all messages from transport ring
   *********************************************/
   /* As Lynn pointed out: if we're restarted by startstop after hanging,
      we should throw away any of our messages in the transport ring.
      Else we could end up re-sending a previously sent message, causing
      time to go backwards... */
   do
   {
     res = tport_getmsg( &InRegion, &GetLogo, 1,
                         &reclogo, &recsize, MSrawmsg, MaxMsgSize );
   } while (res !=GET_NONE);

   /* One heartbeat to announce ourselves to statmgr
   ************************************************/
   ewaccel_status( TypeHeartBeat, 0, "" );
   time(&MyLastBeat);


   /* Start the message stacking thread if it isn't already running.
    ****************************************************************/
   if (MessageStackerStatus != MSGSTK_ALIVE )
   {
     if ( StartThread(  MessageStacker, (unsigned)THREAD_STACK, &tidStacker ) == -1 )
     {
       logit( "e",
              "%s(%s): Error starting  MessageStacker thread; exiting!\n",
          Argv0, MyModName );
       tport_detach( &InRegion );
       tport_detach( &OutRegion );
       return -1;
     }
     MessageStackerStatus = MSGSTK_ALIVE;
   }

   /* Buffers for Process thread: */
   if ( ( SSmsg = (char *) malloc(MaxMsgSize+1) ) ==  NULL )
   {
	  logit( "e", "%s(%s): error allocating SSmsg; exiting!\n",
			  Argv0, MyModName );
			tport_detach( &InRegion );
			tport_detach( &OutRegion );
			exit( -1 );
   }

   /* Create a Mutex to control access to queue
   ********************************************/
   CreateMutex_ew();

   /* Initialize the message queue
   *******************************/
   initqueue( &OutQueue, (unsigned long) QueueSize,(unsigned long) MaxMsgSize+1 );

   /* Start the socket writing thread
   ***********************************/
   if ( StartThread(  Process, (unsigned)THREAD_STACK, &tidProcess ) == -1 )
   {
	  logit( "e", "%s(%s): Error starting Process thread; exiting!\n",
			  Argv0, MyModName );
	  tport_detach( &InRegion );
	  tport_detach( &OutRegion );
	  free( SSmsg );
	  exit( -1 );
   }

   /* Start main ewaccel service loop
   **********************************/
   while( tport_getflag( &InRegion ) != TERMINATE  &&
          tport_getflag( &InRegion ) != MyPid         )
   {
     /* Beat the heart into the transport ring
      ****************************************/
      time(&now);
      if (difftime(now,MyLastBeat) > (double)HeartBeatInt )
      {
          ewaccel_status( TypeHeartBeat, 0, "" );
      time(&MyLastBeat);
      }

      /* take a brief nap; added 970624:ldd
       ************************************/
      sleep_ew(500);
   } /*end while of monitoring loop */

   /* Shut it down
   ***************/
   tport_detach( &InRegion );
   tport_detach( &OutRegion );
   ewaccel_free();
   logit("t", "%s(%s): termination requested; exiting!\n",
          Argv0, MyModName );
   return 0;
}
/********************** end of main *******************************
 ******************************************************************/

/************************  Main Process Thread   *********************
*          Pull a messsage from the queue, parse & run scripts       *
**********************************************************************/

static FILE *fptr; /* For debugging */

thr_ret Process( void *dummy )
{
   int      ret;
   long     msgSize;
   MSG_LOGO Logo;       /* logo of retrieved message             */

   if ( Debug )
     fptr = fopen( "force.txt", "w" );

   while ( 1 ) /* main loop */
   {
     /* Get message from queue
      *************************/
     RequestMutex();
     ret=dequeue( &OutQueue, SSmsg, &msgSize, &Logo);
     ReleaseMutex_ew();
     if(ret < 0 )
     { /* -1 means empty queue */
       sleep_ew(500); /* wait a bit (changed from 1000 to 500 on 970624:ldd) */
       continue;
     }

     /* Process retrieved msg (res==GET_OK,GET_MISS,GET_NOTRACK)
     ***********************************************************/ 
     ret = ProcessPacket( (TracePacket *) SSmsg );
     if ( ret != EW_SUCCESS )
       break;
     
   } /* End of main loop */

   if ( Debug )
     fclose( fptr );

   return THR_NULL_RET;
}


/********************** Message Stacking Thread *******************
 *           Move messages from transport to memory queue         *
 ******************************************************************/
thr_ret MessageStacker( void *dummy )
{
   long          recsize;   /* size of retrieved message             */
   MSG_LOGO      reclogo;       /* logo of retrieved message             */
   int      ret;
   int       res;
   TRACE2_HEADER  	*tbh2 = (TRACE2_HEADER*)MSrawmsg;
   int 				sncl_idx;
   int           NumOfTimesQueueLapped= 0; /* number of messages lost due to
                                             queue lap */

   /* Tell the main thread we're ok
   ********************************/
   MessageStackerStatus = MSGSTK_ALIVE;

   /* Start main export service loop for current connection
   ********************************************************/
   while( 1 )
   {

      /* Get a message from transport ring
      ************************************/
      res = tport_getmsg( &InRegion, &GetLogo, 1,
                          &reclogo, &recsize, MSrawmsg, MaxMsgSize );

      /* Wait if no messages for us
       ****************************/
      if( res == GET_NONE ) {sleep_ew(100); continue;}

      /* Check return code; report errors
      ***********************************/
      if( res != GET_OK )
      {
         if( res==GET_TOOBIG )
         {
            sprintf( errText, "msg[%ld] i%d m%d t%d too long for target",
                            recsize, (int) reclogo.instid,
                (int) reclogo.mod, (int)reclogo.type );
            ewaccel_status( TypeError, ERR_TOOBIG, errText );
            continue;
         }
         else if( res==GET_MISS )
         {
            sprintf( errText, "missed msg(s) i%d m%d t%d in %s",(int) reclogo.instid,
                    (int) reclogo.mod, (int)reclogo.type, InRing );
            ewaccel_status( TypeError, ERR_MISSMSG, errText );
         }
         else if( res==GET_NOTRACK )
         {
            sprintf( errText, "no tracking for logo i%d m%d t%d in %s",
                     (int) reclogo.instid, (int) reclogo.mod, (int)reclogo.type,
                     InRing );
            ewaccel_status( TypeError, ERR_NOTRACK, errText );
         }
      }

      /* Process retrieved msg (res==GET_OK,GET_MISS,GET_NOTRACK)
      ***********************************************************/

      /* First, localize
      ***********************************************/
      if ( WaveMsg2MakeLocal(tbh2) != 0) 
         continue;

      /* Next, see if it matches one of our SNCLs
      *********************************************/
      for ( sncl_idx=0; sncl_idx<nFloor; sncl_idx++ )
      	if ( strcmp( flr[sncl_idx].sta,  tbh2->sta )  == 0 &&
      		 strcmp( flr[sncl_idx].net,  tbh2->net )  == 0 &&
      		 strcmp( flr[sncl_idx].chan, tbh2->chan ) == 0 &&
      		 strcmp( flr[sncl_idx].loc,  tbh2->loc )  == 0 )
      		break;
      if ( sncl_idx >= nFloor )
      	continue;
      tbh2->pad[0] = (char)sncl_idx;
      	
      RequestMutex();
      ret=enqueue( &OutQueue, MSrawmsg, recsize, reclogo );
      ReleaseMutex_ew();

      if ( ret!= 0 )
      {
         if (ret==-2)  /* Serious: quit */
         {    /* Currently, eneueue() in mem_circ_queue.c never returns this error. */
        sprintf(errText,"internal queue error. Terminating.");
            ewaccel_status( TypeError, ERR_QUEUE, errText );
        goto error;
         }
         if (ret==-1)
         {
            sprintf(errText,"queue cannot allocate memory. Lost message.");
            ewaccel_status( TypeError, ERR_QUEUE, errText );
            continue;
         }
         if (ret==-3)  /* Log only while client's connected */
         {
         /* Queue is lapped too often to be logged to screen.
          * Log circular queue laps to logfile.
          * Maybe queue laps should not be logged at all.
          */
            NumOfTimesQueueLapped++;
            if (!(NumOfTimesQueueLapped % 5))
            {
               logit("t",
                     "%s(%s): Circular queue lapped 5 times. Messages lost.\n",
                      Argv0, MyModName);
               if (!(NumOfTimesQueueLapped % 100))
               {
                  logit( "et",
                        "%s(%s): Circular queue lapped 100 times. Messages lost.\n",
                         Argv0, MyModName);
               }
            }
            continue;
         }
      }


   } /* end of while */

   /* we're quitting
   *****************/
error:
   MessageStackerStatus = MSGSTK_ERR; /* file a complaint to the main thread */
   KillSelfThread(); /* main thread will restart us */
   return THR_NULL_RET;
}

/*****************************************************************************
 *  ewaccel_config() processes command file(s) using kom.c functions;         *
 *                    exits if any errors are encountered.               *
 *****************************************************************************/
static void ewaccel_config( char *configfile ) {

   char     init[NUMREQ]; /* init flags, one byte for each required command */
   int      nmiss;        /* number of required commands that were missed   */
   char    *com;
   char     processor[20];
   int      nfiles;
   int      success;
   int      i, f;
   char    *str, *str2;

/* Set to zero one init flag for each required command
 *****************************************************/
   for( i=0; i<NUMREQ; i++ )  init[i] = 0;

/* Open the main configuration file
 **********************************/
   nfiles = k_open( configfile );
   if ( nfiles == 0 ) {
    logit( "e" ,
                "%s: Error opening command file <%s>; exiting!\n",
                Argv0, configfile );
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
                  logit( "e" ,
                          "%s: Error opening command file <%s>; exiting!\n",
                           Argv0, &com[1] );
                  exit( -1 );
               }
               continue;
            }
            strcpy( processor, "ewaccel_config" );

        /* Process anything else as a command
         ************************************/
  /*0*/     if( k_its("LogFile") ) {
                LogSwitch = k_int();
                init[0] = 1;
            }
  /*1*/     else if( k_its("MyModuleId") ) {
                str = k_str();
                if(str) strcpy( MyModName, str );
                init[1] = 1;
            }
  /*2*/     else if( k_its("InRing") ) {
                str = k_str();
                if(str) strcpy( InRing, str );
                init[2] = 1;
            }
  /*3*/     else if( k_its("OutRing") ) {
                str = k_str();
                if(str) strcpy( OutRing, str );
                init[3] = 1;
            }
  /*4*/     else if( k_its("HeartBeatInt") ) {
                HeartBeatInt = k_int();
                init[4] = 1;
            }

           /* Maximum size (bytes) for incoming/outgoing messages
            *****************************************************/
  /*5*/     else if( k_its("MaxMsgSize") ) {
                MaxMsgSize = k_long();
                init[5] = 1;
            }

	    /* Maximum number of messages in outgoing circular buffer
             ********************************************************/ 
  /*6*/     else if ( k_its( "QueueSize" ) ) {
                QueueSize = k_long();
                init[6] = 1;
            }

  /*7,8*/   else if( k_its("Floor") || k_its("GroundFloor") ) {
                if ( k_its("Floor") ) {
                  str = k_str();
                  for ( f = 1; f < nFloor; f++ )
                    if ( ( flr[f].id != NULL ) && ( strcmp( flr[f].id, str ) ) == 0 ) {
                      logit( "e", "%s: <Floor> id '%s' repeated; exiting!\n",
                                  Argv0, str );
                      exit( -1 );
                    }
                  str = mystrdup( str );
                } else {
                  if ( ( nFloor > 0 ) && ( flr[0].id != NULL ) ) {
                    logit( "e", "%s: <GroundFloor> repeated; exiting!\n", Argv0 );
                    exit( -1 );
                  }
                  str = "base";	/* base floor is always at index 0 */
                  f = 0;
                }

                if ( ( flr == NULL ) || ( nFloor == nFloorAlloc ) ) {
                  nFloorAlloc *= 2;
                  flr = (EW_FLOOR_DEF *) realloc( flr, sizeof( EW_FLOOR_DEF ) * nFloorAlloc );
                  if ( flr == NULL ) {
                    if ( f == 0 )
                      logit( "e", "%s: Failed to allocate memory for ground floor; exiting\n",
                                  Argv0 );
                    else
                      logit( "e", "%s: Failed to allocate memory for floor '%s'; exiting\n",
                                  Argv0, str );
                    exit( -1 );
                  }
                }
                flr[f].id = str;
                if ( f == 0 ) {
                  init[8] = 1;
                  /* flr[0].mass (-sum(flr[1:nFloor-1].mass)) is computed below */
                } else {
                  init[7] = 1;
                  flr[f].mass = k_val();
                }
                strcpy( flr[f].sta, k_str() );
                strcpy( flr[f].chan, k_str() );
                strcpy( flr[f].net, k_str() );
                strcpy( flr[f].loc, k_str() );
                if ( !k_err() ) {
                  str2 = k_str();
                  if ( str2 == NULL )
                    if ( defaultConversionSet ) 
                      flr[f].convFactor = defaultConversion;
                    else {
                      logit( "e", "%s: <%sFloor> id '%s' missing conversion without default; exiting!\n",
                                  Argv0, ( f == 0 ) ? "" : "Ground", str );
                      exit( -1 );
                    }
                  else
                    flr[f].convFactor = atof( str2 );
                  nFloor++;
                }
            }
  /*9*/     else if( k_its("BaseShearThreshold") ) {
                baseShearThreshold = k_val();
                init[9] = 1;
            }
  /*10*/    else if( k_its("AvgWindow") ) {
                avgWindowSecs = k_val();
                if ( avgWindowSecs < 0. ) {
                  logit( "e" , "%s: <AvgWindow> must be >= 0; <AvgWindow> = 0.\n", Argv0 );
                  avgWindowSecs = 0.;
                }
                init[10] = 1;
            }
  /*11*/    else if( k_its("AlertDelay") ) {
                alertDelay = k_int();
                init[11] = 1;
            }
  		    else if( k_its("HighPassFreq") ) {
                hpFreq = k_val();
            }
            else if( k_its("HighPassOrder") ) {
                i = k_int();
                if ( i % 2 ) {
                  logit( "e" , "%s: <HighPassOrder> must be even; ignoring.\n",
                               Argv0 );
                  continue;
                }
                hpOrder = i;
            }
            else if( k_its("DefaultConversion") ) {
                if ( defaultConversionSet ) {
                  logit( "e", "%s: <DefaultConversion> repeated; exiting!\n",
                              Argv0);
                  exit( -1 );
                }    		
                defaultConversion = k_val();
                defaultConversionSet = 1;
            }
            else if( k_its("DebugForces") ) {
                exportForces = 1;
            }
            else if ( k_its( "Debug" ) ) {
                i = k_int();
                if ( i >= 0 )
                  Debug = i;
            }


            /* Unknown command
             *****************/
            else {
                logit( "e" , "%s: <%s> Unknown command in <%s>.\n",
                             Argv0, com, configfile );
                continue;
            }

            /* See if there were any errors processing the command
             *****************************************************/
            if( k_err() ) {
              logit( "e", "%s: Bad <%s> command for %s() in <%s>; exiting!\n",
                          Argv0, com, processor, configfile );
              exit( -1 );
            }
    }
    nfiles = k_close();
   }

/* After all files are closed, check init flags for missed commands
 ******************************************************************/
   nmiss = 0;
   for ( i=0; i<NUMREQ; i++ )  if( !init[i] ) nmiss++;
   if ( nmiss ) {
       logit( "e", "%s: ERROR, no ", Argv0 );
       if ( !init[0] )  logit( "e", "<LogFile> "      );
       if ( !init[1] )  logit( "e", "<MyModuleId> "   );
       if ( !init[2] )  logit( "e", "<InRing> "     );
       if ( !init[3] )  logit( "e", "<OutRing> "     );
       if ( !init[4] )  logit( "e", "<HeartBeatInt> " );
       if ( !init[5] )  logit( "e", "<MaxMsgSize> "  );
       if ( !init[6] )  logit( "e", "<QueueSize> "  );
       if ( !init[7] )  logit( "e", "<Floor> "  );
       if ( !init[8] )  logit( "e", "<GroundFloor> "  );
       if ( !init[9] )  logit( "e", "<BaseShearThreshold> "  );
       if ( !init[10])  logit( "e", "<AvgWindow> "  );
       if ( !init[11])  logit( "e", "<AlertDelay> "  );
       logit( "e" , "command(s) in <%s>; exiting!\n", configfile );
       exit( -1 );
   }
   if ( (hpFreq!=0.0 && hpOrder==0) || (hpFreq==0.0 && hpOrder!=0) ) {
   	logit( "e", "%s: Filter requires both <HighPassFreq> and <HighPassOrder>; exiting!\n",
   		Argv0 );
   	exit( -1 );
   }

/* "mass" of base floor is -(sum of other floor masses)
 ******************************************************/
   flr[0].mass = 0.0;
   for ( f = 1; f < nFloor; f++ )
      flr[0].mass -= flr[f].mass;

}

/****************************************************************************
 *  ewaccel_lookup()  Look up important info from earthworm.h tables        *
 ****************************************************************************/
static void ewaccel_lookup( void ) {

/* Look up keys to shared memory regions
   *************************************/
   if( ( InRingKey = GetKey(InRing) ) == -1 ) {
    fprintf( stderr,
            "%s:  Invalid ring name <%s>; exiting!\n",
                 Argv0, InRing);
    exit( -1 );
   }
   if( ( OutRingKey = GetKey(OutRing) ) == -1 ) {
    fprintf( stderr,
            "%s:  Invalid ring name <%s>; exiting!\n",
                 Argv0, OutRing);
    exit( -1 );
   }

/* Look up installations of interest
   *********************************/
   if ( GetLocalInst( &InstId ) != 0 ) {
      fprintf( stderr,
              "%s: error getting local installation id; exiting!\n",
               Argv0 );
      exit( -1 );
   }
  if ( GetInst( "INST_WILDCARD", &InstWildcard ) != 0 ) {
     fprintf(stderr, "%s: Invalid instid <INST_WILDCARD>!\n", Argv0 );
     exit( -1 );
  }

/* Look up modules of interest
   ***************************/
   if ( GetModId( MyModName, &MyModId ) != 0 ) {
      fprintf( stderr,
              "%s: Invalid module name <%s>; exiting!\n",
               Argv0, MyModName );
      exit( -1 );
   }
  if ( GetModId( "MOD_WILDCARD", &ModWildcard ) != 0 ) {
     fprintf(stderr, "%s: Invalid moduleid <MOD_WILDCARD>!\n", Argv0 );
     exit( -1 );
  }

/* Look up message types of interest
   *********************************/
   if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
      fprintf( stderr,
              "%s: Invalid message type <TYPE_HEARTBEAT>; exiting!\n", Argv0 );
      exit( -1 );
   }
   if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
      fprintf( stderr,
              "%s: Invalid message type <TYPE_ERROR>; exiting!\n", Argv0 );
      exit( -1 );
   }
  if ( GetType( "TYPE_TRACEBUF2", &Type_Tracebuf2 ) != 0 ) {
     fprintf(stderr, "%s: Invalid message type <TYPE_TRACEBUF2>!\n", Argv0 );
     exit( -1 );
  }
  if ( GetType( "TYPE_THRESH_ALERT", &TypeThreshAlert ) != 0 ) {
     fprintf( stderr, "%s: Invalid message type <TYPE_THRESH_ALERT>!\n", Argv0 );
     exit( -1 );
  }
  
  GetLogo.mod = ModWildcard;
  GetLogo.instid = InstWildcard;
  GetLogo.type = Type_Tracebuf2;

}

/***************************************************************************
 * ewaccel_status() builds a heartbeat or error message & puts it into     *
 *                  shared memory.  Writes errors to log file & screen.    *
 ***************************************************************************/
void ewaccel_status( unsigned char type, short ierr, char *note )
{
   MSG_LOGO    logo;
   char        msg[256];
   long        size;
   time_t      t;

/* Build the message
 *******************/
   logo.instid = InstId;
   logo.mod    = MyModId;
   logo.type   = type;

   time( &t );

   if( type == TypeHeartBeat )
    sprintf( msg, "%ld %ld\n%c", (long) t, (long) MyPid, 0);
   else if( type == TypeError )
   {
    sprintf( msg, "%ld %hd %s\n%c", (long) t, ierr, note, 0);

    logit( "et", "%s(%s): %s\n", Argv0, MyModName, note );
   }

   size = (long)strlen( msg );   /* don't include the null byte in the message */

/* Write the message to shared memory
 ************************************/
   if( tport_putmsg( &InRegion, &logo, size, msg ) != PUT_OK )
   {
        if( type == TypeHeartBeat ) {
           logit("et","%s(%s):  Error sending heartbeat.\n",
                  Argv0, MyModName );
    }
    else if( type == TypeError ) {
           logit("et", "%s(%s):  Error sending error:%d.\n",
                  Argv0, MyModName, ierr );
    }
   }

}

/***************************************************************************
 * writeAlertMessage() builds & puts alert message into shared memory.     *                 
 ***************************************************************************/
static void writeAlertMessage( double force, int off ) {

   MSG_LOGO    logo;
   char        msg[256];
   time_t      t = (time_t)(awStartTime+samplePeriod*off);
   struct tm   theTimeStruct;

/* Build the message
 *******************/
   logo.instid = InstId;
   logo.mod    = MyModId;
   logo.type   = TypeThreshAlert;

   gmtime_ew( &t, &theTimeStruct );

   sprintf( msg, "Base-shear threshold=%1lf ForceDiff=%lf Time=%s%c",
 	 baseShearThreshold, force, asctime( &theTimeStruct ), 0 );
   if ( tport_putmsg( &OutRegion, &logo, (long)strlen(msg)+1, msg ) != PUT_OK ) {
	  logit("et", "%s:  Error writing threshold message to ring.\n",
			  Argv0 );
   }
}

/***************************************************************************
 * addForceToDump() adds a value to a debugging message of computed        *
 *     force sums, writing out as a message into shared memory when full.  *
 *     off is the index into the current packet; if -1, write current msg  *
 ***************************************************************************/
static void addForceToDump( double force, int off ) {

	int len;
	if ( debugMsg == NULL ) {
		if ( off == -1 )
			return;
		debugMsg = malloc( 4096 );
		if ( debugMsg == NULL ) {
			logit( "e", "%s: Failed to allocate debugging message; exiting!\n",
				Argv0 );
			exit( -1 );
		}
		debugMsgTail = debugMsg;
		debugMsgSpace = 4095;
	}
	
	if ( debugMsg == debugMsgTail ) {
		if ( off == -1 )
			return;
		len = sprintf( debugMsg, "%s %s", "Time", "ForceDiff" );
		debugMsgTail += len;
		debugMsgSpace = 4096 - len;
	}
	if ( off == -1 )
		len = debugMsgSpace+1;
	else
		len = snprintf( debugMsgTail, debugMsgSpace, "\n%lf %lf", 
			awStartTime+samplePeriod*off, force );
	if ( len > debugMsgSpace ) {
   		MSG_LOGO    logo;

		*debugMsgTail = 0;

		/* Build the message
		*******************/
		logo.instid = InstId;
		logo.mod    = MyModId;
		logo.type   = 0;
		
		if ( tport_putmsg( &OutRegion, &logo, (long)(debugMsgTail - debugMsg + 1), debugMsg ) != PUT_OK ) {
		  logit("et", "%s: Error writing debugging message to ring.\n",
				  Argv0 );
		}
		
		debugMsgTail = debugMsg;
		if ( off != -1 )
			addForceToDump( force, off );
	} else
		debugMsgTail += len;
}

/***************************************************************************
 * ewaccel_free()  free all previously allocated memory                    *
 ***************************************************************************/
static void ewaccel_free( void ) {

   free (MSrawmsg);             /* MessageStacker's "raw" retrieved message */

}

/***************************************************************************
 * ResetSCNL()  Called at start of a run: first packet or when a packet    *
 *              "breaks" the current run                                   *
 ***************************************************************************/
static int ResetSCNL( TracePacket *inBuf ) {

  int i, f;
  size_t bytesNeeded;        /* size of window in bytes */

  strncpy( dataType, inBuf->trh2.datatype, sizeof( dataType ) );
  dataType[sizeof( dataType ) - 1] = '\0';
  dataSize       = dataType[1] - '0';
  sampleRate     = inBuf->trh2.samprate;
  samplePeriod   = 1.0 / sampleRate;
  jitter         = samplePeriod / 2;
  sampleCount    = inBuf->trh2.nsamp;
  packetDuration = sampleCount * samplePeriod;

  delaySamples = (int) ( sampleRate * alertDelay );

  if ( avgWindowSecs > 0 ) {
    int numStartTimes;       /* nbr of awStartTime's cached when computing initial average */
    /* Calc nbr samples needed for length of averaging window, and estimate */
    /* nbr of packets (i.e., awStartTime's) for that many samples           */
    awSamples = round( sampleRate * avgWindowSecs );
    if ( ( awSamples % sampleCount ) != 0 ) {
      awSamples = ( ( awSamples / sampleCount ) + 1 ) * sampleCount;
      logit( "e", "ewaccel: WARNING: AvgWindow lengthened to %f secs to end "
                  "on a tracebuf boundary\n", awSamples / sampleRate );
    }

    /* (Re-)allocation of space for awStartTime's of the initial packets */

    /* numStartTimes is the number of awStartTime's needed to cache awSamples   */
    /* number of data points, less one, since the current awStartTime does not  */
    /* need to be cached.  ProcessPacket() assures that all tracebufs are the   */
    /* same  size (sampleCount) as the current tracebuf (inBuf); runt tracebufs */
    /* that can increase the number needed are not possible.                    */
    numStartTimes = ( awSamples / sampleCount ) - 1;
    awStartTimes  = (double *) realloc( awStartTimes, numStartTimes * sizeof( double ) );
    if ( awStartTimes == NULL ) {
      logit( "e", "ewaccel: failed to allocate queued initial start times; exiting.\n" );
      return EW_FAILURE;
    }
    numCachedStartTimes = 0;

    /* (Re-)allocation of space for averaging window of samples */
    bytesNeeded = awSamples * dataSize;
    if ( bytesNeeded > awBytesAllocated ) {
      for ( f = 0; f < nFloor; f++ ) {
        flr[f].avgWindow = realloc( flr[f].avgWindow, bytesNeeded );
        if ( flr[f].avgWindow == NULL ) {
          logit( "e", "ewaccel: failed to allocate an averaging window; exiting.\n" );
          return EW_FAILURE;
        }
      }
      awBytesAllocated = bytesNeeded;
    }

    /* Reset counters/flags */
    awPosn = 0;
    awFull = 0;
    for ( f = 0; f < nFloor; f++ )
      flr[f].sum = 0.0;

  }

  /* (Re-)allocation of space for matrix of demeaned, filtered TRACEBUF vectors */
  bytesNeeded = nFloor * sampleCount * sizeof( *dataMatrix );
  dataMatrix  = realloc( dataMatrix, bytesNeeded );
    if ( dataMatrix == NULL ) {
      logit( "e", "ewaccel: failed to allocate processed data matrix; exiting.\n" );
      return EW_FAILURE;
    }

  if ( nHPLevels > 0 ) {

    highpass( hpFreq, samplePeriod, hpOrder, hpPoles, &hpGain );

    /* Initialize the filter coefficients and recursive terms for the first floor */
    memset( flr[0].hpLevel, 0, nHPLevels *sizeof( HP_STAGE ) );
    for ( i = 0; i < nHPLevels; i++ ) {
/*    flr[0].hpLevel[i].d1 = 0.0; */ /* taken care of by memset() */
/*    flr[0].hpLevel[i].d2 = 0.0; */
/*    flr[0].hpLevel[i].f  = 0;   */
      flr[0].hpLevel[i].a1 = -2.0 * hpPoles[i+i].real;
      flr[0].hpLevel[i].a2 = hpPoles[i+i].real * hpPoles[i+i].real +
                             hpPoles[i+i].imag * hpPoles[i+i].imag;
      flr[0].hpLevel[i].b1 = -2.0;
      flr[0].hpLevel[i].b2 = 1.0;
    }

    /* Duplicate the filter coefficients and recursive terms for the other floors */
    for ( f = 1; f < nFloor; f++ )
      memcpy( flr[1].hpLevel, flr[0].hpLevel, nHPLevels * sizeof( HP_STAGE ) );

  }

  floorsLeft = nFloor;
  for ( f = 0; f < nFloor; f++ )
    flr[f].read = 0;
  
  return EW_SUCCESS;
} 

/***************************************************************************
 * InitSCNL()  Initializations for various stuff                           *
 ***************************************************************************/
static int InitSCNL() {

  int f;

  /* Debias fields */
  for ( f = 0; f < nFloor; f++ ) {
    flr[f].avgWindow = NULL;
    flr[f].sum = 0.0;
  }
  awStartTimes = NULL;
  dataMatrix   = NULL;
  numCachedStartTimes = 0;
  awPosn              = 0;
  awBytesAllocated    = 0;
  awSamples           = 0;
  awFull              = 0;
  dataType[0]         = '\0';
  dataSize            = 0;
  if ( avgWindowSecs <= 0 ) {
    awFull    = 1;
    /* To prevent divide-by-zero when "demeaning" and avgWindowSecs <= 0 */
    awSamples = 1;
  }

  /* Integrate & filter fields */
  for ( f = 0; f < nFloor; f++ )
    flr[f].hpLevel = NULL;
  hpPoles = NULL;
  nHPLevels = ( hpOrder + 1 ) / 2;

  if ( nHPLevels > 0 ) {
    for ( f = 0; f < nFloor; f++ ) {
      flr[f].hpLevel = (HP_STAGE *) calloc( nHPLevels, sizeof( HP_STAGE ) );
      if ( flr[f].hpLevel == NULL ) {
        logit ("e", "ewaccel: Failed to allocate SCNL info stages; exiting.\n" );
        while ( f-- > 0 )
          free( flr[f].hpLevel );
        return EW_FAILURE;
      }
    }
    hpPoles = (complex *) calloc( sizeof( complex ), hpOrder );
    if ( hpPoles == NULL ) {
      logit( "e", "ewaccel: Failed to allocate SCNL info poles; exiting.\n" );
      return EW_FAILURE;
    }
  }

  /* Force call to ResetSCNL() the first time ProcessPacket() is called     */
  /* Thereafter, ProcessPacket() sets awStartTime = -1 when a run is broken */
  awStartTime = -1;
  
  return EW_SUCCESS;   
}

/*****************************************************************************
 * ProcessDataMatrix()  Process nFloor rows of sampleCount data vectors.     *
 *                      The data have been optionally demeaned and filtered. *
 *                      startTime is the time of the first sample.           *
 *****************************************************************************/
static void ProcessDataMatrix( double startTime, float *dataMatrix ) {

  int i, f;
  double force;

  for ( i = 0; i < sampleCount; i++ ) {
    force = 0.0;
    for ( f = 0; f < nFloor; f++ ) {
      force += flr[f].mass * flr[f].convFactor * dataMatrix[f*sampleCount+i];
    }

    force = fabs( force );
    /* logit( "", "%d %f %f %f %f %f %f %f %f %f\n", i, data[0][i], data[1][i], data[2][i], data[3][i], data[4][i], data[5][i], data[6][i], data[7][i], force ); */
    /* fprintf( fptr, "%d %f %f %f %f %f %f %f %f %f\n", i, data[0][i], data[1][i], data[2][i], data[3][i], data[4][i], data[5][i], data[6][i], data[7][i], force ); */
    if ( Debug )
      fprintf( fptr, "%d %f %f \n", i, force, baseShearThreshold );

    if ( exportForces )
      addForceToDump( force, i );

    if ( alertOn ) {
      /* if ( partialSum < baseShearThreshold ) {
        fprintf( stderr, "Alert reset (%lf < %lf @ %lf)!\n", partialSum, baseShearThreshold, i );
        alertOn = 0;
      } else */
      alertOn--;
      /* if ( alertOn == 0 )
        fprintf( stderr, "Alert reset @ %lf!\n", startTime+samplePeriod*i );
      else
        fprintf(stderr, "ao=%d\n", alertOn ); */
    } else if ( force > baseShearThreshold ) {
      alertOn = delaySamples;
      /* The time given in this message will be completely wrong for data fed */
      /* from the debiasing window; need to replay data packet start times too */
      logit("", "Alert goes off (%lf > %lf @ %lf)!\n", 
                force, baseShearThreshold, startTime+samplePeriod*i);
      writeAlertMessage( force, i );
    }
  }
}

/***************************************************************************
 * Filter()  Optionally high-pass filter the data.                         *
 ***************************************************************************/

static double Filter( double datum, HP_STAGE *hpLevel ) {

  double out;

  out = datum;
  /* Do the filter */
  if ( nHPLevels > 0 ) {
    int i;
    double next_out;
    for ( i = 0; i < nHPLevels; i++ ) {
      next_out = out + hpLevel[i].d1;
      hpLevel[i].d1 = hpLevel[i].b1*out - hpLevel[i].a1*next_out + hpLevel[i].d2;
      hpLevel[i].d2 = hpLevel[i].b2*out - hpLevel[i].a2*next_out;
      out = next_out;
    }
    out *= hpGain;
  }

  return out;
}

/***************************************************************************
 * ProcessPacket()  Process latest packet.                                 *
 ***************************************************************************/
int ProcessPacket( TracePacket *inBuf ) {

  short    *waveShort = NULL, *awShort = NULL;
  int32_t  *waveLong  = NULL, *awLong  = NULL;
  int i, j, k;
  int posn = 0;
  int debiasing = ( avgWindowSecs > 0 );
  double datum, bias;
  TRACE2_HEADER* tbh2 = (TRACE2_HEADER *) inBuf;
  int f = tbh2->pad[0];

/*
  logit( "te", "Filtering floor %s @ %f (fl=%d) ao=%d\n",
               flr[f].id, tbh2->starttime, floorsLeft, alertOn );
 */

  if ( awStartTime != -1 ) {

    /* If awStartTime != -1, we are within a continuous sequence (a run) */
    /* Verify the sample rate, data type, and packet length have not changed (fatal) */

    if ( sampleRate != inBuf->trh2.samprate ) {
      logit( "e", "ewaccel: floor '%s' has different sample rate (%lf vs %lf); exiting!\n",
                  flr[f].id, sampleRate, inBuf->trh2.samprate );
      return EW_FAILURE;
    }
    if ( strcmp( dataType, inBuf->trh2.datatype ) != 0 ) {
      logit( "e", "ewaccel: floor '%s' has different data type (%s vs %s); exiting!\n",
                  flr[f].id, dataType, inBuf->trh2.datatype );
      return EW_FAILURE;
    }
    if ( sampleCount != inBuf->trh2.nsamp ) {
      logit( "e", "ewaccel: floor '%s' has different sample count (%d vs %d); exiting!\n",
                  flr[f].id, sampleCount, inBuf->trh2.nsamp );
      return EW_FAILURE;
    }

    if ( floorsLeft < nFloor ) {

      /* If floorsLeft < nFloor, we are expecting a packet with the current */
      /* sample start time from a different floor                           */

      /* The next packet should have the same sample start time */

      /* This will only work for channels from the same digitizer */
      /* Otherwise, alignment of the data traces may be necessary */
      if ( fabs( tbh2->starttime - awStartTime ) > jitter ) {
        logit( "w", "%s: packet for floor %s @ %f arrived before %f done\n",
                    Argv0, flr[f].id, tbh2->starttime, awStartTime );
        awStartTime = -1;
      }

      /* The next packet should be from a different floor */

      else if ( flr[f].read ) {
        logit( "w", "%s: duplicate packet for floor %s @ %f\n",
                    Argv0, flr[f].id, tbh2->starttime );
        awStartTime = -1;
      }

    } else if ( floorsLeft == nFloor ) {

      /* If floorsLeft == nFloor, we are expecting a packet with the next */
      /* sample start time in the sequence                                */

      double expecting = awStartTime + packetDuration;
      if ( fabs( tbh2->starttime - expecting ) > jitter ) {
        logit( "w", "%s: packet for floor %s @ %f is out of sequence, expecting %f\n",
                    Argv0, flr[f].id, tbh2->starttime, expecting );
        awStartTime = -1;
      }

    } else {

      /* FAULT: floorsLeft is invalid.  THIS SHOULD NEVER HAPPEN! */

      logit( "e", "ewaccel: FAULT, floorsLeft (=%d) is invalid; exiting!\n",
                  floorsLeft );
      return EW_FAILURE;
    }

  }

  /* Start a new sequence of floor time-series data */
  if ( awStartTime == -1 )
    ResetSCNL( inBuf );

  /* If floorsLeft == nFloor, this is the start of the next set of data traces */
  if ( floorsLeft == nFloor )
    awStartTime = tbh2->starttime;
  
  /* Note this floor's data packet for this start time has already been read */
  /* (to detect duplicate packets)                                           */
  flr[f].read = 1;

  /* datasize-dependent pointers */
  if ( dataSize == 2 )
    waveShort = (short*)   ( inBuf->msg + sizeof( TRACE2_HEADER ) );
  else
    waveLong  = (int32_t*) ( inBuf->msg + sizeof( TRACE2_HEADER ) );

  /* awPosn is the same for all the floor data traces      */
  /* awSamples is rounded up to end on a tracebuf boundary */

  if ( debiasing ) {
      /* Get properly typed pointer to window */
      if ( dataSize == 2 )
        awShort = (short*)   flr[f].avgWindow;
      else
        awLong  = (int32_t*) flr[f].avgWindow;
      posn = awPosn;
  }

  for ( i = 0; i < sampleCount; i++ ) {

    if ( debiasing ) {
      /* If window is full, remove the oldest entry from sum */
      if ( awFull )
        flr[f].sum -= (dataSize == 2 ) ? awShort[posn] : awLong[posn];

      /* Add newest entry to sum & window */
      if ( dataSize == 2 ) {
        awShort[posn] = *waveShort++;
        datum = awShort[posn++];
      } else {
        awLong[posn]  = *waveLong++;
        datum = awLong[posn++];
      }
      flr[f].sum += datum;
    } else
      datum = ( dataSize == 2 ) ? *waveShort++ : *waveLong++;
    
    /* If window is full (=1 if no debiasing), remove bias (=0.0 if no debiasing) */
    /* from latest value (awSamples is guaranteed != 0 in InitSCNL())             */
    if ( awFull ) {
      bias = flr[f].sum / awSamples;
      /*fprintf( stderr, "C[%d][%3d] = %9.3lf (%9.3lf - %9.3lf)\n", f, i, datum-bias, datum, bias );*/
      dataMatrix[f*sampleCount+i] = Filter( datum - bias, flr[f].hpLevel );
    }

    /* If we filled window for first time ( !awFull && ( posn == awSamples ) ), */
    /* flush the cache after debiasing it                                       */
    else if ( posn == awSamples ) {

      /* The window is not marked "full" until all the floors have been seen */
      if ( floorsLeft == 1 ) {

        /* fprintf( stderr, "Window now full!\n" ); */
        awFull = 1;

        /* Flush the data cached in the window after debiasing it */

        k = 0;
        /* TRACEBUF by TRACEBUF across the averaging windows (one per floor) */
        for ( posn = 0; posn < awSamples; posn += sampleCount ) {
          /* Floor by floor, like the way tracebufs of data arrive */
          for ( f = 0; f < nFloor; f++ ) {
            if ( dataSize == 2 ) {
              awShort = (short*)   flr[f].avgWindow;
              awShort += posn;
            } else {
              awLong  = (int32_t*) flr[f].avgWindow;
              awLong  += posn;
            }
            /* Remove bias from the cached tracebuf values */
            bias = flr[f].sum / awSamples;
            for ( j = 0; j < sampleCount; j++ ) {
              datum = ( dataSize == 2 ) ? awShort[j] : awLong[j];
              /*fprintf( stderr, "C[%d][%3d] = %9.3lf (%9.3lf - %9.3lf)\n", f, j, datum-bias, datum, bias );*/
              dataMatrix[f*sampleCount+j] = Filter( datum - bias, flr[f].hpLevel );
            }
          }
          /* Process cached data matrices; the current data matrix is processed below */
          if ( k < numCachedStartTimes ) {
            ProcessDataMatrix( awStartTimes[k], dataMatrix );
            k++;
           /* fprintf(stderr, "Process cached data matrices"); */
          }
        }
        numCachedStartTimes = 0;
      }
      /* data[][] is full at this point, but that's okay; the data must be   */
      /* from the current (inBuf) data packet and it will be processed below */
    }
    if ( posn >= awSamples )
      posn = 0;
  }
  floorsLeft--;
  if ( floorsLeft == 0 ) {
    /* If no debiasing, the window is full (awFull is guaranteed == 1 in InitSCNL()) */
    if ( awFull ) {
      /* Process the next matrix of data */
      ProcessDataMatrix( awStartTime, dataMatrix );
      /* fprintf(stderr, "data matrix called"); */
    } else {
      /* Only needed when debiasing and the averaging windows are not yet full */
      awStartTimes[numCachedStartTimes++] = awStartTime;
    }
    if ( exportForces )
      addForceToDump( 0.0, -1 );
    awPosn = posn;
    floorsLeft = nFloor;
    for ( f = 0; f < nFloor; f++ )
      flr[f].read = 0;
  }

  /* fprintf( stderr, "Done Filtering ao=%d\n", alertOn ); */
  
  return EW_SUCCESS;
}
