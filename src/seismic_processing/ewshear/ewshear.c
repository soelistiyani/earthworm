/******************************************************************************
 *                                                                            *
 *      This code does not appear to work; it is not clear it ever did.       *
 *                                                                            *
 *****************************************************************************/

/******************************************************************************
 *
 *	File:			ewshear.c
 *
 *	Function:		Program to read TRACEBUFs for a collection of
 *				floors and compute the combined shear velocity 
 *
 *	Author(s):		Scott Hunter, ISTI
 *
 *	Source:			ewshear.c
 *
 *	Notes:
 *
 *	Change History:
 *			5/23/11	Started source
 *
 *****************************************************************************/

#define DEBUG

#define VERSION "1.0.1 2018-06-29"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "earthworm.h"
#include "ws2ts.h"
#include "kom.h"
#include "imp_exp_gen.h"
#include "mem_circ_queue.h"
#include "ew_spectra.h"
#include "swap.h"
#include "transport.h"
#include "iir.h"
#include "ew_spectra_io.h"
#include "ws_clientII.h"
#include "socket_ew.h"

#define MAX_EWS_PROCS	10	/* maximum number of spectra processing definitions */

#define MSGSTK_OFF    0              /* MessageStacker has not been started      */
#define MSGSTK_ALIVE  1              /* MessageStacker alive and well            */
#define MSGSTK_ERR   -1              /* MessageStacker encountered error quit    */
volatile int MessageStackerStatus = MSGSTK_OFF;

thr_ret MessageStacker( void * );    /* used to pass messages between main thread */
thr_ret Process( void * );

/* Error messages used by export
 ***********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring        */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer      */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded  */
#define  ERR_BADMSG        3   /* message w/ bad timespan                 */
#define  ERR_QUEUE         4   /* error queueing message for sending      */
static char  errText[256];     /* string for log/error messages           */

#define backward 0

char *progname;

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static long          InRingKey;     /* key of transport ring for input    */
static long          OutRingKey;    /* key of transport ring for output   */
static unsigned char InstId;        /* local installation id              */
static unsigned char MyModId;       /* Module Id for this program         */

static  SHM_INFO  InRegion;     /* shared memory region to use for input  */
static  SHM_INFO  OutRegion;    /* shared memory region to use for output */

MSG_LOGO  GetLogo[1];     			/* requesting module,type,instid */
pid_t MyPid;        /* Our own pid, sent with heartbeat for restart purposes */
	
time_t now;                /* current time, used for timing heartbeats */
time_t MyLastBeat;         /* time of last local (into Earthworm) hearbeat */

static int     HeartBeatInt;           /* seconds between heartbeats        */
static unsigned char TypeHeartBeat;
static unsigned char TypeError;
static unsigned char TypeAlert;

static char *MSrawmsg = NULL;          /* MessageStacker's "raw" retrieved message */
static char *SSmsg = NULL;             /* Process's incoming message buffer */
static MSG_LOGO Logo;                  /* logo of message to re-send */

static long    MaxMsgSize = 100;       /* max size for input msgs */
static int     RingSize = 10;          /* max messages in output circular buffer */

static double AlertThreshold = 0.2;
static double Height;
static int FS;
static int LogSwitch = 1;
static int dataSize = 0;

/* Information specific to a floor */
/* ---> read, windowActive, and full seem to not be updated. Are they really being used? */
typedef struct {
        int     lvl;                   /* nbr of this floor; 0 for ground */
        double  convFactor;            /* conversion factor */
        char    read;                  /* = "floor has been read for current time" */
        char    windowActive;          /* = "window contains data" */
        char    sta[TRACE2_STA_LEN];   /* Site name (NULL-terminated) */
        char    net[TRACE2_NET_LEN];   /* Network name (NULL-terminated) */
        char    chan[TRACE2_CHAN_LEN]; /* Component/channel code (NULL-terminated) */
        char    loc[TRACE2_LOC_LEN];   /* Location code (NULL-terminated) */
        EW_TIME_SERIES  window;	       /* Window being processed */
        int     w_off;                 /* Offset into window: start of unfilled part, -1 if filled */
        char    *packet_left;          /* Unprocessed part of last packet */
        int     pl_len;
        
        char    full;
        char	*packet;
        long	w_sofar, w_end;
        long	p_sofar, p_end;
} EW_FLOOR_DEF;

#define THREAD_STACK 8192
static ew_thread_t tidStacker;          /* Thread moving messages from transport */
static ew_thread_t tidProcess;          /* Processor thread id */

static int windowSamples;
int floorsFull;

int numPeaks = 0;
double peakBand[2];
char defaultConversionSet = 0;
double defaultConversion;
double Epsilon = 0;


/*****************************************************************************
   Discrete fourier transform subroutine
   based on the fast fourier transform algorithm
   which computes the quantity:

	     n-1
	x(j)=sum  x(k) exp( 2*pi*i*isign*k*j/n )
	     k=0

   n must be a integral power of 2.
*****************************************************************************/
static void ah_four(double *data, int n, int isign) {
	int ip0, ip1, ip2, ip3, i3rev;
	int i1, i2a, i2b, i3;
	double sinth, wstpr, wstpi, wr, wi, tempr, tempi, theta;

	ip0= 2;
	ip3= ip0*n;
	i3rev= 1;
	for (i3= 1; i3 <= ip3; i3+= ip0) {
		if (i3 < i3rev) {
			tempr= data[i3-1];
			tempi= data[i3];
			data[i3-1]= data[i3rev-1];
			data[i3]= data[i3rev];
			data[i3rev-1]= tempr;
			data[i3rev]= tempi;
		}
		ip1= ip3/2;
		do {
			if (i3rev <= ip1)
				break;
			i3rev-= ip1;
			ip1/= 2;
		} while (ip1 >= ip0);
		i3rev+= ip1;
	}
	ip1= ip0;
	while (ip1 < ip3) {
		ip2= ip1 * 2;
		theta= 6.283185/((double) (isign*ip2/ip0));
		sinth= (double) sin((double) (theta/2.));
		wstpr= -2.*sinth*sinth;
		wstpi= (double) sin((double) theta);
		wr= 1.;
		wi= 0.;
		for (i1= 1; i1 <= ip1; i1+= ip0) {
			for (i3= i1; i3 <ip3; i3+= ip2) {
				i2a= i3;
				i2b= i2a+ip1;
				tempr= wr*data[i2b-1] - wi*data[i2b];
				tempi= wr*data[i2b] + wi*data[i2b-1];
				data[i2b-1]= data[i2a-1] - tempr;
				data[i2b]= data[i2a] - tempi;
				data[i2a-1]+= tempr;
				data[i2a]+= tempi;
			}
			tempr= wr;
			wr= wr*wstpr - wi*wstpi + wr;
			wi= wi*wstpr + tempr*wstpi + wi;
		}
		ip1=ip2;
	}
	return;
}

/*****************************************************************************
   This subroutine takes a real time series and computes its
   discrete fourier transform. The time series has n real points
   and the transform has n/2+1 complex values starting with
   frequency zero and ending at the nyquist frequency.

   Parameters:
   	x	floating point data array
   	n	the number of samples, an integral power of 2
*****************************************************************************/
static void ah_fftr(double *x, int n) {
	int nn, is, nm, j, i;
	int k1j, k1i, k2j, k2i;
	double s, fn, ex, wr, wi, wwr, wrr, wwi, a1, a2, b1, b2;

	nn= n/2;
	is= 1;
	ah_four(x, nn, is);
	nm= nn/2;
	s= x[0];
	x[0]+= x[1];
	x[n]= s - x[1];
	x[1]= 0.0 ;
	x[n+1]= 0.0;
	x[nn+1]= (-x[nn+1]);
	fn= (double) n;
	ex= 6.2831853/fn;
	j= nn;
	wr= 1.0;
	wi= 0.0;
	wwr= (double) cos((double) ex);
	wwi= (double) (-sin((double) ex));
	for (i= 2; i <= nm; i++) {
		wrr= wr*wwr-wi*wwi;
		wi= wr*wwi+wi*wwr;
		wr= wrr;
		k1j= 2*j-1;
		k1i= 2*i-1;
		k2j= 2*j;
		k2i= 2*i;
		a1= 0.5*(x[k1i-1]+x[k1j-1]);
		a2= 0.5*(x[k2i-1]-x[k2j-1]);
		b1= 0.5*(-x[k1i-1]+x[k1j-1]);
		b2= 0.5*(-x[k2i-1]-x[k2j-1]);
		s= b1;
		b1= b1*wr+b2*wi;
		b2= b2*wr-s*wi;
		x[k1i-1]= a1-b2;
		x[k2i-1]= (-a2-b1);
		x[k1j-1]= a1+b2;
		x[k2j-1]=  a2-b1;
		j-= 1;
	}
	return;
}

/* Information specific to a floor */
typedef struct {
	QUEUE	mainQ;
	QUEUE	backupQ;
	int 	backupSize;
	int		unseenBackups;
} EWS_QUEUE;

void init_ews_queue( EWS_QUEUE *q ) 
{
	initqueue( &(q->mainQ), (unsigned long)RingSize,(unsigned long)MaxMsgSize+1 );
	q->backupSize = -1;
	q->unseenBackups = 0;
}

int ews_dequeue( EWS_QUEUE *q, DATA x, long* size, MSG_LOGO* userLogoPtr )
{
	int ret;
    RequestMutex();
	if ( q->unseenBackups == 0 )
		ret = dequeue( &(q->mainQ), x, size, userLogoPtr );
	else {
		q->unseenBackups--;
		q->backupSize--;
		ret = dequeue( &(q->backupQ), x, size, userLogoPtr );
	}
    ReleaseMutex_ew();
    return ret;
}

int ews_enqueue( EWS_QUEUE *q, DATA x, long size, MSG_LOGO userLogo )
{
	int ret;
	RequestMutex();
	ret = enqueue( &(q->mainQ), x, size, userLogo );
    ReleaseMutex_ew();
    return ret;
}
	

int ews_renqueue( EWS_QUEUE *q, DATA x, long size, MSG_LOGO userLogo )
{
	int ret;
	puts("Re-enqueue");
    RequestMutex();
	if ( q->backupSize == -1 ) {
		initqueue( &(q->backupQ), (unsigned long)RingSize,(unsigned long)MaxMsgSize+1 );
		q->backupSize = 0;
	} else
		q->backupSize++;
	ret = enqueue( &(q->backupQ), x, size, userLogo );
    ReleaseMutex_ew();
    return ret;
}

void ews_enable_backup( EWS_QUEUE *q ) 
{
    RequestMutex();
	q->unseenBackups = q->backupSize;
    ReleaseMutex_ew();
}

char *ResetBuffer = NULL;

void ews_reset_backups( EWS_QUEUE *q )
{
	long size;
    RequestMutex();
	while ( q->unseenBackups > 0 ) {
		dequeue( &(q->backupQ), ResetBuffer, &size, GetLogo );
		enqueue( &(q->backupQ), ResetBuffer, size, GetLogo[0] );
		q->unseenBackups--;
	}
    ReleaseMutex_ew();
}

EWS_QUEUE AlertQueue, DamageQueue;

int scale;							/* 0=no scaling */
int white;							/* 1=scale amplitude output only by mean */
double start, stop;					/* time period from config */
int smooth;							/* 0=no smoothing */
double smooth_width;				/* width (in secs) of smoothing window */
int taperType;						/* what kind of tapering to do:
										BARTLETT=1, HANNING=2, 
										PARZAN=3, BMHARRIS=4 
										*/
double taperFrac;					/* fraction of each end to taper */
int nFloor;							/* # floors */
char inRing[MAX_RING_STR];			/* name of input ring */
char outRing[MAX_RING_STR];			/* name of output ring */
double highcutoff, lowcutoff;		/* freq for high and low cutoffs */
int highpoles, lowpoles;			/* # poles for high & low cutoffs (0=none) */

char *taperName[5] = {"", "BARTLETT", "HANNING", "PARZAN", "BMHARRIS"};
char *Argv0 = "ewshear";

static char    MyModName[MAX_MOD_STR];       /* speak as this module name/id      */
double winSecs = 0;
EW_FLOOR_DEF alertFlr[2];
EW_FLOOR_DEF *flr = alertFlr;
int nFloorAlloc = 5;


/*****************************************************************************
 *  ewshear_config() processes command file(s) using kom.c functions;         *
 *                    exits if any errors are encountered.               *
 *****************************************************************************/
void ewshear_config( char *configfile )
{
   int      ncommand;     /* # of required commands you expect to process   */
   char     init[10];     /* init flags, one byte for each required command */
   int      nmiss;        /* number of required commands that were missed   */
   char    *com;
   char     processor[20];
   int      nfiles;
   int      success;
   int      i;

/* Set to zero one init flag for each required command
 *****************************************************/
   ncommand = 10;
   for( i=0; i<ncommand; i++ )  init[i] = 0;
   alertFlr[0].lvl = alertFlr[1].lvl = -1;

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
            strcpy( processor, "ewshear_config" );

			/* Process anything else as a command
			 ************************************/
  /*0*/     if ( k_its("MyModuleId") ) {
				strcpy( MyModName, k_str() );
				if ( GetModId( MyModName, &MyModId ) != 0 ) {
				  logit( "e", "%s: Invalid module name <%s>; exiting!\n",
						   Argv0, MyModName );
				  continue;
				}
				init[0] = 1;
  /*1*/		} else if ( k_its("InRing") ) {
				strcpy( inRing, k_str() );
				if ( ( InRingKey = GetKey(inRing) ) == -1 ) {
					logit( "e", "%s:  Invalid ring name <%s>; exiting!\n",
							 Argv0, inRing);
					continue;
				}
				init[1] = 1;
  /*2*/		} else if ( k_its("OutRing") ) {
				strcpy( outRing, k_str() );
				if ( ( OutRingKey = GetKey(outRing) ) == -1 ) {
					logit( "e", "%s:  Invalid ring name <%s>; exiting!\n",
							 Argv0, outRing);
					continue;
				}
				init[2] = 1;
  /*3*/		} else if ( k_its("HeartBeatInt") ) {
				HeartBeatInt = k_int();
				init[3] = 1;
  /*4 & 5*/	} else if ( k_its("GroundFloor") || k_its("TopFloor") ) {
				int flrNum = k_its("GroundFloor") ? 0 : 1;
				if ( alertFlr[flrNum].lvl == flrNum ) {
					logit( "e", "%s: <%sFloor> #%d repeated; exiting!\n",
						Argv0, flrNum ? "Ground" : "Top", flrNum );
					exit(-1);
				}
				strcpy( flr[flrNum].sta,  k_str() );
				strcpy( flr[flrNum].chan, k_str() );
				strcpy( flr[flrNum].net,  k_str() );
				strcpy( flr[flrNum].loc,  k_str() );
				if ( !k_err() ) {
					/*
					str2 = k_str();
					if ( str2 == NULL ) 
						if ( defaultConversionSet ) 
							flr[i].convFactor = defaultConversion;
						else {
							logit( "e", "%s: <%sFloor> id '%s' missing conversion without default; exiting!\n",
								Argv0, i==0 ? "" : "Ground", str );
							exit(-1);
						}
					else
						flr[i].convFactor = atof( str2 );
					*/
					flr[i].read = 0;
					flr[i].windowActive = 0;
					nFloor++;
				}
				alertFlr[i].window.data = NULL;
				alertFlr[i].packet_left = NULL;
				alertFlr[i].lvl = flrNum;
				init[ flrNum+4 ] = 1;
  /*6*/		} else if ( k_its("Height") ) {
				Height = k_val();
				init[6] = 1;
  /*7*/		} else if ( k_its("WindowSize") ) {
				winSecs = k_val();
				init[7] = 1;
  /*8*/		} else if ( k_its("FS") ) {
				FS = k_val();
				init[8] = 1;
  /*9*/		} else if ( k_its("Taper") ) {
				char *taperTypeName;
				taperTypeName = k_str();
				for ( taperType = 4; taperType>0; taperType-- )
					if ( strcmp( taperTypeName, taperName[taperType] )==0 )
						break;
				if ( taperType == 0 ) {
					logit( "e", "Invalid type of taper: '%s'\n", taperTypeName );
					continue;
				}
				taperFrac = k_val();
				init[9] = 1;
		} else if ( k_its("LogFile") ) {
				LogSwitch = k_int();
  		} else if ( k_its("RingSize") ) {
				RingSize = k_long();
		} else if ( k_its("MaxMsgSize") ) {
				MaxMsgSize = k_long();
		} else if ( k_its("Epsilon") ) {
				Epsilon = k_val();
		} else if ( k_its("PctShearThreshold") ) {
				AlertThreshold = k_val()/100.0;
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
            if ( k_err() ) {
               logit( "e" ,
                       "%s: Bad <%s> command for %s() in <%s>; exiting!\n",
                        Argv0, com, processor, configfile );
               exit( -1 );
            }
    }
    nfiles = k_close();
   }

/* After all files are closed, check init flags for missed commands
 ******************************************************************/
   nmiss = 0;
   for ( i=1; i<ncommand; i++ )  if( !init[i] ) nmiss++;
   if ( nmiss ) {
       logit( "e", "%s: ERROR, no ", Argv0 );
       if ( !init[0] )  logit( "e", "<MyModuleId> "   );
       if ( !init[1] )  logit( "e", "<InRing> "       );
       if ( !init[2] )  logit( "e", "<OutRing> "      );
       if ( !init[3] )  logit( "e", "<HeartBeatInt> " );
       if ( !init[4] )  logit( "e", "<GroundFloor> "  );
       if ( !init[5] )  logit( "e", "<TopFloor> "     );
       if ( !init[6] )  logit( "e", "<Height> "       );
       if ( !init[7] )  logit( "e", "<WindowSize> "   );
       if ( !init[8] )  logit( "e", "<FS> "           );
       if ( !init[9] )  logit( "e", "<Taper> "        );
       logit( "e" , "command(s) in <%s>; exiting!\n", configfile );
       exit( -1 );
   }

}


/***********************************************************************
 * deconvolve_spectra () - deconvolve spectra b into a
 *		return 0 if no problems, 1 otherwise
 ***********************************************************************/
int deconvolve_spectra( EW_TIME_SERIES *spec_a, EW_TIME_SERIES *spec_b ) {
	int ndata = spec_a->dataCount * 2;
	int i;
	double denom, result_0, result_1;
	double *a = spec_a->data, *b = spec_b->data;
	for ( i=0; i<=ndata; i+=2 ) {
		denom = b[i]*b[i] + b[i+1]*b[i+1];
		if ( denom == 0.0 )
			a[i] = a[i+1] = 0.0;
		else {
			result_0 = a[i]*b[i] + a[i+1]*b[i+1];
			result_1 = b[i]*a[i+1] - a[i]*b[i+1];
			a[i]   = result_0 / denom;
			a[i+1] = result_1 / denom;
		}
			
	}
	return 0;
}

/***********************************************************************
 * convolve_spectra () - convolve spectra b into a
 *		return 0 if no problems, 1 otherwise
 ***********************************************************************/
int convolve_spectra( EW_TIME_SERIES *spec_a, EW_TIME_SERIES *spec_b, char normalize ) {
	int ndata = spec_a->dataCount * 2;
	int i;
	double result_0, result_1;
	double *a = spec_a->data, *b = spec_b->data;
	for ( i=0; i<=ndata; i+=2 ) {
		double amp_fact = 1.0;
		result_0 = a[i]*b[i] + a[i+1]*b[i+1];
		result_1 = a[i]*b[i+1] - a[i+1]*b[i];
		if(normalize)  {
			amp_fact = ( a[i]*a[i] + a[i+1]*a[i+1] );
			amp_fact *= ( b[i]*b[i] + b[i+1]*b[i+1] );
			amp_fact = (float) sqrt( (double)amp_fact );
		}
		a[i]   = result_0/amp_fact;
		a[i+1] = result_1/amp_fact;
	}
	return 0;
}

/***********************************************************************
 * xcorr () - cross correlate signal b into a in time-domain
 *		return shear-wave velocity (swv) if no problems, 1 otherwise
 * 
 * Written by Erol Kalkan on 06/14/2017 
 ***********************************************************************/

int xcorr( EW_TIME_SERIES *spec_a, EW_TIME_SERIES *spec_b, double Height, int FS ) {

   int ndata = spec_a->dataCount;
   int i, j, lag, maxlag, absLag;
   double sx, sy, sxy, denom, cc, maxcc, swv;
   double *a = spec_a->data, *b = spec_b->data;
#ifdef DEBUG
   FILE *fptr3;
#endif /* DEBUG */

#ifdef DEBUG
   /* Write input waveforms to input.txt */
   {
      int j;
      FILE *fptr2 = fopen( "input.txt", "w" );
      if ( fptr2 != NULL ) {
         for ( j = 0; j < ndata; j++ )
            fprintf( fptr2, "%d %f %f\n", j, a[j], b[j] );	
         fclose( fptr2 );
      }
   }
#endif /* DEBUG */

   /* Calculate the denominator */
   sx = 0;
   sy = 0;
   for (i=0;i<ndata;i++) {
      sx += a[i]*a[i];
      sy += b[i]*b[i];
   }
   denom = (sqrt(sx*sy) + Epsilon); 
   maxlag = ndata-1;
   maxcc = 0;
   swv = 1;
   /* Calculate the correlation coefficients */
#ifdef DEBUG
   fptr3 = fopen( "xcorr.txt", "w" );
#endif /* DEBUG */
   for ( absLag = 1; absLag < maxlag; absLag++ )
      for ( lag = -absLag; lag <= absLag; lag += 2 * absLag ) {
         sxy = 0;
         for ( i = 0; i < ndata; i++ ) {
            j = i + lag;
            if ( ( j >= 0 ) && ( j < ndata ) )
               sxy += a[i] * b[j];
         }
         cc = sxy / denom; /* cc = correlation coefficient */
         if ( cc >= maxcc ) {
            maxcc = cc;
            /* n.b. lag cannot be 0 */
      	    swv = ( Height * FS ) / lag;
         }
#ifdef DEBUG
         fprintf( fptr3, "%d %f\n", lag, cc );			  
#endif /* DEBUG */
      }
#ifdef DEBUG
   fclose( fptr3 );
#endif /* DEBUG */
   printf( "shear-wave velocity = %f\n", swv );

   return swv;
}

/***********************************************************************
 * smooth_spectra () - smooth the spectra timeseries
 *		return 0 if no problems, 1 otherwise
 ***********************************************************************/
int smooth_spectra( EW_TIME_SERIES * ewspec, int mode ) {
	double *specsm, even_sum, odd_sum;
	int i, imod;
	double delta = 1/ewspec->trh2x.samprate;
	int smoother= (int)(smooth_width/delta);
	int ndata = ewspec->dataCount * 2;
	double *spec = ewspec->data;
	
	/* make sure that smoother is even */
	smoother= (smoother/2)*2;
	if ((specsm= (double *) malloc((smoother)*sizeof(double))) == NULL ) {
		logit("et","%s: memory allocation error\n", progname);
		return 1;
	}
	/* Create & fill smoothing window */
	memcpy( specsm, spec, smoother*2*sizeof(double) );
	even_sum = odd_sum = 0;
	for ( i=0; i<smoother*2; i+=2) {
		if ( mode & EWTS_MODE_FIRST )
			even_sum   += spec[i];
		if ( mode & EWTS_MODE_SECOND )
			odd_sum += spec[i+1];
	}
		
	for (i= smoother, imod=0; i <= ndata-smoother; i+= 2, imod = (imod+2)%(smoother*2) ) {
		if ( mode & EWTS_MODE_FIRST ) {
			even_sum += spec[i+smoother];		/* Add next item to window sums */
			spec[i]   = even_sum / (smoother+1);/* Store next smoothed result */
			even_sum -= specsm[imod];			/* Remove oldest value from sums... */
			specsm[imod]   = spec[i+smoother];	/*... and replace it with newest in window */
		}
		if ( mode & EWTS_MODE_SECOND ) {
			odd_sum  += spec[i+smoother+1];		/* Add next item to window sums */
			spec[i+1] = odd_sum  / (smoother+1);/* Store next smoothed result */
			odd_sum -= specsm[imod+1];		/* Remove oldest value from sums... */
			specsm[imod+1] = spec[i+smoother+1];/*... and replace it with newest in window */
		}
	}
	free(specsm);
	return 0;
}

/***********************************************************************
 * convert_spectra () - convert complex spectra to amp/phase representation
 *		return 0 if no problems, 1 otherwise
 ***********************************************************************/
int convert_spectra( EW_TIME_SERIES * ewspec ) {
	int i, ndata = ewspec->dataCount * 2;
	double *spec = ewspec->data;
	double amp, phase;
	for (i= 0; i <= ndata; i+= 2) {
		amp= (double) sqrt((double) (spec[i]*spec[i]+spec[i+1]*spec[i+1]));
		phase= (double) atan2(spec[i+1], spec[i]);
		spec[i]= amp;
		spec[i+1]= phase;
	}
	ewspec->dataType = EWTS_TYPE_AMPPHS;
	return 0;
}

/* Similar thing for white */

/***********************************************************************
 * calc_spectra () - calculate the spectra for a timeseries, store in 
 *      provided timeseries
 *		return 0 if no problems, 1 otherwise
 ***********************************************************************/
int calc_spectra( EW_TIME_SERIES * ewts, EW_TIME_SERIES * ewspec ) 
{
	double *spec;
	int i, ndata;
	/*double delta = 1/ewts->trh2x.samprate;*/
	
	*ewspec = *ewts;

	ndata = ewts->dataCount;

	/* get nearest power of 2 for listed data length */
	for (i= 1; (int) pow((double) 2., (double) i) < ewts->dataCount; i++);
	ndata= (int) pow((double) 2.,(double) (i+1));
	ewspec->dataCount = ndata/2;
	ewspec->dataType = EWTS_TYPE_COMPLEX;

	/* allocate space for data and smoothing area */
	if ((spec= (double *) malloc((unsigned) (ndata+2)*sizeof(double))) == NULL ) {
		logit("et","%s: memory allocation error\n", progname);
		return 1;
	}
	ewspec->data = spec;
	memcpy( spec, ewts->data, ewts->dataCount*sizeof(double) );
	memset( spec+ewts->dataCount, 0, sizeof(double)*(ndata-ewts->dataCount+2) );
	ah_fftr(spec, ndata);
	return 0;
}

/*****************************************************************************
 *  Swap(): Reverse the byte ordering of the size bytes of *data             *
 *****************************************************************************/
void Swap( void *data, int size )
{
   char temp, *cdat = (char*)data;
   int i,j;
   for ( i=0, j=size-1; i<j; i++, j-- ) {
   		temp = cdat[i]; cdat[i] = cdat[j]; cdat[j] = temp;
   }

}

/*****************************************************************************
 *  MakeLocal(): Convert elements of *tbh2x to this processor's endianess    *
 *****************************************************************************/
int MakeLocal( TRACE2_HEADER *tbh2 ) {
	int size = 1;
#if defined (_SPARC)
	if ( tbh2->datatype[0] == 's' )
		size = -1;
	else if ( tbh2->datatype[0] != 'i' ) {
		logit( "e", "ewshear: unknown datatype: '%s'", tbh2->datatype );
		return 0;
	}
#elif defined (_INTEL)
	if ( tbh2->datatype[0] == 'i' )
		size = -1;
	else if ( tbh2->datatype[0] != 's' ) {
		logit( "e", "ewshear: unknown datatype: '%s'", tbh2->datatype );
		return 0;
	}
#else
#error "_INTEL or _SPARC must be set before compiling"
#endif
	else {
		Swap( &tbh2->starttime, sizeof(double) );
		Swap( &tbh2->nsamp, sizeof(int) );
	}
	if ( tbh2->datatype[1] == '2' )
		return size * 2;
	else if ( tbh2->datatype[1] == '4' )
		return size * 4;
	else {
		logit( "e", "ewshear: unknown datatype size: '%s'", tbh2->datatype );
		return 0;
	}
}

/***************************************************************************
 * ewshear_status() builds a heartbeat or error message & puts it into      *
 *                 shared memory.  Writes errors to log file & screen.     *
 ***************************************************************************/
void ewshear_status( unsigned char type, short ierr, char *note )
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
    sprintf( msg, "%ld %ld\n%c", (long) t, (long) MyPid, (char)0);
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

   return;
}

/********************** Message Stacking Thread *******************
 *           Move messages from transport to memory queue         *
 ******************************************************************/
thr_ret MessageStacker( void *dummy )
{
   long          recsize;   /* size of retrieved message             */
   MSG_LOGO      reclogo;       /* logo of retrieved message             */
   TRACE2_HEADER  	*tbh2 = (TRACE2_HEADER*)MSrawmsg;
   int       res;
   int       ret;
   /*
   char      date[100], *msgName;
   int modid, len, okMsg;
   */
   int				data_size;
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
      res = tport_getmsg( &InRegion, GetLogo, 1,
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
            ewshear_status( TypeError, ERR_TOOBIG, errText );
            continue;
         }
         else if( res==GET_MISS )
         {
            sprintf( errText, "missed msg(s) i%d m%d t%d in %s",(int) reclogo.instid,
                    (int) reclogo.mod, (int)reclogo.type, inRing );
            ewshear_status( TypeError, ERR_MISSMSG, errText );
         }
         else if( res==GET_NOTRACK )
         {
            sprintf( errText, "no tracking for logo i%d m%d t%d in %s",
                     (int) reclogo.instid, (int) reclogo.mod, (int)reclogo.type,
                     inRing );
            ewshear_status( TypeError, ERR_NOTRACK, errText );
         }
      }
      
      /* First, localize
      ***********************************************/
/*
      data_size = MakeLocal( tbh2 );
      if ( data_size == 0 )
      	 continue;
*/
      if ( WaveMsg2MakeLocal(tbh2) != 0) 
         continue;

      if ( tbh2->datatype[1] == '2' )
         data_size = 2;
      else if ( tbh2->datatype[1] == '4' )
         data_size = 4;

      /* Next, see if it matches one of our SNCLs
      *********************************************/
      for ( sncl_idx=0; sncl_idx<nFloor; sncl_idx++ )
      	if ( strcmp( flr[sncl_idx].sta,  tbh2->sta )  == 0 &&
      		 strcmp( flr[sncl_idx].net,  tbh2->net )  == 0 &&
      		 strcmp( flr[sncl_idx].chan, tbh2->chan ) == 0 &&
      		 strcmp( flr[sncl_idx].loc,  tbh2->loc )  == 0 )
      		break;
      if ( sncl_idx >= nFloor ) {
      	/* printf( "Skipping packet %s.%s.%s.%s\n", tbh2->sta, tbh2->net, tbh2->chan, tbh2->loc ); */
      	continue;
      }      
	  /*printf("Posting Packet for floor %d (%s) [%d]\n", sncl_idx, tbh2->loc, tbh2->nsamp);*/

      /* Process retrieved msg (res==GET_OK,GET_MISS,GET_NOTRACK)
      ***********************************************************/

      /* put it into the 'to be shipped' queue */
      /* the Process thread is in the biz of de-queueng and processing */
      if ( sncl_idx == 0 || sncl_idx == nFloor-1 ) {
      	tbh2->pad[0] = sncl_idx ? 1 : 0;
      	ret = ews_enqueue( &AlertQueue, MSrawmsg, recsize, reclogo );
      } else {
      	tbh2->pad[0] = (char)sncl_idx;
      	ret = ews_enqueue( &DamageQueue, MSrawmsg, recsize, reclogo );
      }

      if ( ret!= 0 )
      {
         if (ret==-2)  /* Serious: quit */
         {    /* Currently, eneueue() in mem_circ_queue.c never returns this error. */
        sprintf(errText,"internal queue error. Terminating.");
            ewshear_status( TypeError, ERR_QUEUE, errText );
        goto error;
         }
         if (ret==-1)
         {
            sprintf(errText,"queue cannot allocate memory. Lost message.");
            ewshear_status( TypeError, ERR_QUEUE, errText );
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

static   short *waveShort;
static   int32_t *waveLong;
#if 0
static   EW_TIME_SERIES ewspec2;
#endif

/************************  Main Process Thread   *********************
*          Pull messages from the AlertQueue; once an alert is       *
*          determined, post alert message                            *
**********************************************************************/

static int alertOn = 0; /* Alert hasn't been reset */
static double lastSWV = -1;	/* Previous window's SWV */

thr_ret Process( void *dummy )
{
   int      ret;
   long     msgSize;
   double   startTm = 0.0, stopTm = 0.0;
   TRACE2_HEADER  	*tbh2 = (TRACE2_HEADER*)SSmsg;
   int f;
   int i, j;
   char partial_packets = 0;
#ifdef DEBUG
   FILE    *fptr;
#endif /* DEBUG */
   
   startTm = -1;

#ifdef DEBUG
   fptr = fopen( "swv.txt", "w" );
#endif /* DEBUG */

   while (1)   /* main loop */
   {
     /* Get message from queue
      *************************/
     ret = ews_dequeue( &AlertQueue, SSmsg, &msgSize, &Logo );
     if(ret < 0 )
     { /* -1 means empty queue */
       sleep_ew(500); /* wait a bit (changed from 1000 to 500 on 970624:ldd) */
       continue;
     }

     f = tbh2->pad[0];
/*
     logit( "te", "Packet %f nsamp: %d for floor %d (%ld bytes)\n", tbh2->starttime, tbh2->nsamp, f, msgSize );
 */
	 
	 /* Packet is for floor whose window is full; put back into queue */
	 if ( alertFlr[f].full ) {
		RequestMutex();
		ews_renqueue( &AlertQueue, SSmsg, msgSize, Logo );
		ReleaseMutex_ew();
	 	continue;
	 }

      /* Process retrieved msg (res==GET_OK,GET_MISS,GET_NOTRACK)
      ***********************************************************/

	  /* If no window defined, or gap found, start fresh window. */
/* ---> This does not detect INTERNAL gaps within the window! */
	  if ( startTm == -1 || tbh2->starttime > stopTm ) {
		if ( startTm != -1 ) {
		/* This can happen, e.g., when the AlertQueue is lapped */
/* ---> This also always happens for the first packet after the shear calculation window */
/* ---> The code is not reusable */
	  		logit( "w", "ewshear: Gap detected: %s.%s.%s.%s, jump from %f to %f\n",
	  			alertFlr[f].sta, alertFlr[f].net, alertFlr[f].chan, alertFlr[f].loc, startTm, tbh2->starttime );
/* ---> There is no recovery; every packet from now on is rejected; the sequence is not reset */
		} else {
	  		dataSize = tbh2->datatype[1]-'0';
	  		windowSamples = (int)(tbh2->samprate * winSecs);
	  		for ( i = 0; i < nFloor; i++ ) {
		  		alertFlr[i].window.data = malloc( sizeof(double) *  windowSamples );
		  		if ( alertFlr[i].window.data == NULL ) {
	  				logit( "e", "ewshear: allocate failed for timeseries window; exiting!\n" );
	  				exit(-1);
	  			}
	  		}
	  	
			startTm = tbh2->starttime;
			stopTm = startTm + winSecs;
		}
	  }
	  
	  /* Fill window from packet */
	  if ( dataSize == 2 )
		waveShort = (short*)   (SSmsg + sizeof(TRACE2_HEADER));
	  else
		waveLong  = (int32_t*) (SSmsg + sizeof(TRACE2_HEADER));
	  
	  if ( alertFlr[f].w_off == 0 ) {
	  	alertFlr[f].window.trh2x = *((TRACE2X_HEADER*)tbh2);
	  	alertFlr[f].window.dataCount = windowSamples;
	  }
	  for ( i=0, j=alertFlr[f].w_off; i<tbh2->nsamp && j<windowSamples; i++, j++ ) {
	  	if ( dataSize == 2 )
		  	alertFlr[f].window.data[j] = waveShort[i];
		else
			alertFlr[f].window.data[j] = waveLong[i];
		/* flr[f].window.data[j] = j + f*100; */
	  }

	  if ( j == windowSamples ) {
		/* Window is full; do processing that involves it alone */
		printf( "Floor %d is full\n", f ); 

		if ( i < tbh2->nsamp ) {
	  		/* Remember unused portion of packet for later
	  		if ( tbh2->nsamp != 100 ) {
	  			printf("\nBad nsamp: %d for floor %d (%ld bytes)\n", tbh2->nsamp, f, msgSize );
	  			printf("\nMsg: '%s'\n", SSmsg );
	  		} */
	  		flr[f].pl_len = (msgSize - sizeof(TRACE2_HEADER))/dataSize - i;
	  		/*printf( "\nSaving %d floor %d packets for later (%d,%d,%d)\n", flr[f].pl_len, f, i, tbh2->nsamp, windowSamples );*/
	  		flr[f].packet_left = malloc( dataSize * flr[f].pl_len );
	  		if ( flr[f].packet_left == NULL )
	  			logit( "e", "ewshear: allocate failed for samples outside window\n" );
	  		else if (dataSize == 2 )
		  		memcpy( flr[f].packet_left, waveShort + i, dataSize*flr[f].pl_len );
		  	else
		  		memcpy( flr[f].packet_left, waveLong  + i, dataSize*flr[f].pl_len );
		  	partial_packets--;
	  	}

		flr[f].w_off = 0;
		floorsFull++;

#if 0
	  	demean_EWTS( flr[f].window );

/* ---> If the Taper config option is not used, it should be removed from the .d file */
		if ( taperType ) {
			taper_EWTS( flr[f].window, taperType, taperFrac, EWTS_MODE_BOTH );
	  		demean_EWTS( flr[f].window );
	  	}
#endif /* 0 */

#if 0
/* ---> If ewspec2 is not used, it should be removed */
		calc_spectra( &(flr[f].window), &ewspec2 );
/* ---> So what happens when this memory is needed again?  startTm is not reset to -1 */
/* ---> Ergo, the memory is not reallocated; ewspec2 is reused in its place           */
		free( flr[f].window.data );
		flr[f].window = ewspec2;
#endif /* 0 */

#if 0
		if ( f == 1 ) {
		    int j;
		    for ( j = 0; j < windowSamples; j++ )
			fprintf( fptr, "%f \n",flr[f].window.data[j] );	
		}
#endif /* 0 */

#if 0
		if ( f == 1 ) {
		    int j;
 		    for ( j = 0; j < flr[f].window.dataCount; j++ )
 			fprintf( fptr, "%f %f\n", flr[f].window.data[2*j], flr[f].window.data[2*j+1] );
 		}
#endif /* 0 */

	  	/* If all floors full, check for alert */
	  	if ( floorsFull == 2 ) {

			double swv = xcorr( &(flr[0].window), &(flr[1].window), Height, FS );
			logit( "", "swv=%lf\n", swv);
#ifdef DEBUG
			fprintf( fptr,"%lf\n", swv );
#endif /* DEBUG */

			if ( lastSWV >= 0 ) {
				double dv = fabs((swv-lastSWV)/swv);
				
				if ( !alertOn ) {
					if ( dv > AlertThreshold ) {
						int n;
						char outMsg[128];
   						MSG_LOGO    logo;

					    logo.instid = InstId;
   						logo.mod    = MyModId;
   						logo.type   = TypeAlert;
   						
						alertOn = 1;
						n = snprintf( outMsg, sizeof( outMsg ), "Old SWV: %lf New SWV: %lf Time: %lf AlertThreshold: %f Window length: %f", lastSWV, swv, startTm, AlertThreshold, winSecs );
						if ( n > sizeof( outMsg ) )
							logit( "e", "ewshear: Alert message string is truncated\n" );
						if( tport_putmsg( &OutRegion, &logo, (long)strlen(outMsg)+1, outMsg ) != PUT_OK )
							logit( "e", "ewshear: Error posting alert message\n" );
						logit( "", "ewshear: Alert on (%s)\n", outMsg );


						/* printf( "\nAlert ON  : %lf,%lf %lf %lf\n", swv, lastSWV, dv, AlertThreshold ); */
					} 
				} else if ( alertOn ) {
					if ( dv < AlertThreshold ) {
						alertOn = 0;
						logit( "", "ewshear: alert off (time = %lf)\n", startTm );
						/* printf( "\nAlert OFF : %lf,%lf %lf %lf\n", swv, lastSWV, dv, AlertThreshold ); */
					}
				}
			} 
			lastSWV = swv;
			floorsFull = 0;
		}
	  	
	  } else {
	  	/* printf( "%d (of %d) packets in window for floor %d\n", j, windowSamples, f ); */
	  	alertFlr[f].w_off = j;
	  }
   }   /* End of main loop */

#ifdef DEBUG
   fclose( fptr );
#endif /* DEBUG */

   return THR_NULL_RET;

}

int main(int argc, char **argv)
{
/* Other variables: */
   int           res;
   long          recsize;   /* size of retrieved message             */
   MSG_LOGO      reclogo;   /* logo of retrieved message             */
	
	/* Look up installations of interest
	*********************************/
	if ( GetLocalInst( &InstId ) != 0 ) {
	  fprintf( stderr,
			  "%s: error getting local installation id; exiting!\n",
			   Argv0 );
	  exit( -1 );
	}
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
   if ( GetType( "TYPE_SHEAR_ALERT", &TypeAlert ) != 0 ) {
      fprintf( stderr,
              "%s: Invalid message type <TYPE_SHEAR_ALERT>; exiting!\n", Argv0 );
      exit( -1 );
   }

	if ((progname= strrchr(*argv, (int) '/')) != (char *) 0)
		progname++;
	else
		progname= *argv;

    /* Check command line arguments */
    if (argc != 2) {
		fprintf (stderr, "Usage: %s configfile\n", progname);
		fprintf (stderr, "Version: %s\n", VERSION);
		return EW_FAILURE;
    }

	logit_init( argv[1], 0, 1024, 1 );

	/* Get our own Pid for restart purposes
   	***************************************/
   	MyPid = getpid();
   	if(MyPid == -1)
   	{
      logit("e", "%s: Cannot get pid; exiting!\n", Argv0);
      return(0);
   	}

	/* Read-in & interpret config file */
	scale= white= smooth= taperType= 0;
	lowpoles = highpoles = (int)(lowcutoff = highcutoff = 0);
	InRingKey = OutRingKey = -1;
	start = -1;
	ewshear_config(argv[1]);

	if ( lowcutoff > 0 && highcutoff > 0 && lowcutoff >= highcutoff ) {
		logit( "e", "ewshear: Low Cutoff Frequency Must Be < High Cutoff Frequency.\n" );
		exit(1);
	}

	ResetBuffer = malloc( MaxMsgSize+1 );
	if ( ResetBuffer == NULL ) {
		logit( "e", "%s: error allocating ResetBuffer; exiting\n", Argv0 );
		exit(1);
	}
	
	if ( 1 ) {
 		/* none of the three below should ever fail, unless earthworm.d is not avail */
	   	int get_i = GetInst( "INST_WILDCARD", &(GetLogo[0].instid) );
	   	int get_m = GetModId( "MOD_WILDCARD", &(GetLogo[0].mod) );
	   	int get_t = GetType( "TYPE_TRACEBUF2", &(GetLogo[0].type) );
	   	int getMsg = 0;
		if ( ( MSrawmsg = (char *) malloc(MaxMsgSize+1) ) ==  NULL )
		{
		  logit( "e", "%s: error allocating MSrawmsg; exiting!\n",
				 Argv0 );
		  getMsg = 1;
		}
	   	if ( get_i || get_m || getMsg || get_t) {
	   		if ( get_i )
	   			logit( "e", "%s: INST_WILDCARD unknown; exiting!\n", Argv0 );
	   		if ( get_m )
	   			logit( "e", "%s: MOD_WILDCARD unknown; exiting!\n", Argv0 );
	   		if ( get_t )
	   			logit( "e", "%s: TYPE_TRACEBUF2 unknown; exiting!\n", Argv0 );
	   		exit(1);
	   	}
		tport_attach( &InRegion, InRingKey );		
	}
	
	tport_attach( &OutRegion, OutRingKey );
	
   /* Buffers for Process thread: */
   if ( ( SSmsg = (char *) malloc(MaxMsgSize+1) ) ==  NULL )
   {
      logit( "e", "%s(%s): error allocating SSmsg; exiting!\n",
              Argv0, MyModName );
            tport_detach( &InRegion );
	      	tport_detach( &OutRegion );
	   		exit(1);
   }

   /* Create a Mutex to control access to queue
   ********************************************/
   CreateMutex_ew();

   /* Initialize the message queues
   *******************************/
   init_ews_queue( &AlertQueue );
   init_ews_queue( &DamageQueue );

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

   /* step over all messages from transport ring
   *********************************************/
   /* As Lynn pointed out: if we're restarted by startstop after hanging,
	  we should throw away any of our messages in the transport ring.
	  Else we could end up re-sending a previously sent message, causing
	  time to go backwards... */
   do
   {
	 res = tport_getmsg( &InRegion, GetLogo, 1,
						 &reclogo, &recsize, MSrawmsg, MaxMsgSize );
   } while (res !=GET_NONE);

   /* One heartbeat to announce ourselves to statmgr
   ************************************************/
   ewshear_status( TypeHeartBeat, 0, "" );
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
	   free( SSmsg );
	   return( -1 );
	 }
	 MessageStackerStatus = MSGSTK_ALIVE;
   }


   /* Start main ringdup service loop
   **********************************/
   while( tport_getflag( &InRegion ) != TERMINATE  &&
		  tport_getflag( &InRegion ) != MyPid         )
   {
	 /* Beat the heart into the transport ring
	  ****************************************/
	  time(&now);
	  if (difftime(now,MyLastBeat) > (double)HeartBeatInt )
	  {
		  ewshear_status( TypeHeartBeat, 0, "" );
	  time(&MyLastBeat);
	  }

	  /* take a brief nap; added 970624:ldd
	   ************************************/
	  sleep_ew(500);
   } /*end while of monitoring loop */
		
		
	/* Clean up after ourselves */
	if ( InRingKey != -1 )
		tport_detach( &InRegion );
	if ( OutRingKey != -1 )
		tport_detach( &OutRegion );
	return 0;
}
