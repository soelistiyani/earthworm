/*
 * pick_GM.c
 * Modified from pick_eew
 */

#define PICK_GM_VERSION "0.5 - 2017-10-06"

#if defined(WIN32) || defined(WIN64)
#include <direct.h>	/* _mkdir() */
#else
#include <sys/stat.h>	/* mkdir() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "earthworm.h"
#include "transport.h"
#include "trace_buf.h"
#include "swap.h"
#include "trheadconv.h"
#include "chron3.h"
#include "pick_gm.h"

#if defined(WIN32) || defined(WIN64)
	// Copied from linux libc sys/stat.h:
	#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
	#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

const char kPathSeparator =
#if defined(WIN32) || defined(WIN64)
							'\\';
#else
							'/';
#endif

/* Function prototypes
   *******************/
int  GetConfig( char *, GPARM * );
void LogConfig( GPARM * );
int  GetStaList( STATION **, int *, GPARM *, bool );
void LogStaList( STATION *, int );
int  CompareSCNL( const void *, const void * );
int  CompareSNL( const void *, const void * );
int  Restart( STATION *, GPARM *, int, int );
void Interpolate( STATION *, char *, int );
int  GetEwh( EWH * );
//void Sample( int, STATION * );
void reportGM(STATION* , GPARM* , EWH*  );
void reportGMX(STATION* , GPARM* , EWH*  );

//void call_FilterPicker (STATION *Sta, char *TraceBuf, GPARM *Gparm, EWH *Ewh);
int sniff_gm( double, int, STATION *, int);
int sniff_gmx(STATION *, STATION *);

int writeGM(STATION *, GPARM *, EWH *); 
int writeGMX(STATION *, GPARM *, EWH *); 

thr_ret XmlThreadGM(void* ptr);
thr_ret XmlThreadGMX(void* ptr);

/***********************************************************
*              The main program starts here.              *
*                                                         *
*  Arguments:                                             *
*     argv[1] = Name of picker configuration file         *
*     argv[2] = (Optional) Tracebuffer file for offline   *
*                          mode                           *
***********************************************************/

int offline_mode;  /* Global flag to determine whether we work in offline mode or not*/
MSG_LOGO  fooMsg;               /* logo of retrieved message     */
int main (int argc, char **argv)
{
	int           i;                /* Loop counter */
	STATION       *StaArray = NULL; /* Station array */
	STATION       *StaGmxArray = NULL;	/* Station GMX array */
	char          *TraceBuf;        /* Pointer to waveform buffer */
	TRACE_HEADER  *TraceHead;       /* Pointer to trace header w/o loc code */
	TRACE2_HEADER *Trace2Head;      /* Pointer to header with loc code */
	int           *TraceLong;       /* Long pointer to waveform data */
	short         *TraceShort;      /* Short pointer to waveform data */
	long          MsgLen;           /* Size of retrieved message */
	MSG_LOGO      logo;             /* Logo of retrieved msg */
	MSG_LOGO      hrtlogo;          /* Logo of outgoing heartbeats */
	int           Nsta = 0;         /* Number of stations in list */
	int           NstaGmx = 0;
	time_t        then;             /* Previous heartbeat time */
	long          InBufl;           /* Maximum message size in bytes */
	GPARM         Gparm;            /* Configuration file parameters */
	EWH           Ewh;              /* Parameters from earthworm.h */
	char          *configfile;      /* Pointer to name of config file */
	pid_t         myPid;            /* Process id of this process */

	char    	type[3];
	STATION 	key;              	/* Key for binary search */
	STATION 	*Sta;             	/* Pointer to the station being processed */
	STATION 	*StaGmx;            /* Pointer to the station being processed */
	int			rc;               	/* Return code from tport_copyfrom() */
	time_t  	now;              	/* Current time */
	double  	GapSizeD;         	/* Number of missing samples (double) */
	int  		GapSize;          	/* Number of missing samples (integer) */
	unsigned char 	seq;        	/* msg sequence number from tport_copyfrom() */

	/* file interface - c. satriano */
	char	*tracename = NULL;
	FILE 	*tracefile = NULL;
	int 	loop_condition,count;

	ew_thread_t  tidThrdGM;                /* Filter thread id              */
	ew_thread_t  tidThrdGMX;                /* Filter thread id              */
	int numOfElementsInQueue;

		// set environment forcely, only for Visual Studio

#if defined(WIN32) || defined(WIN64)
	_putenv("EW_DATA=d:\\earthworm\\korea\\data\\");
	_putenv("EW_DATA_DIR=d:\\earthworm\\korea\\data\\");
	_putenv("EW_HOME=d:\\earthworm");
	_putenv("EW_INSTALLATION=INST_UNKNOWN");
	_putenv("EW_LOG=d:\\earthworm\\korea\\log\\");
	_putenv("EW_PARAMS=d:\\earthworm\\korea\\params");
	_putenv("EW_VERSION=earthworm_7.9");
	_putenv("TZ=GMT");
	_putenv("SYS_NAME=TEMP");
#endif

	/* Check command line arguments */
	if (argc < 2) {
		fprintf( stderr, "Usage: pick_gm <configfile> [tank_file]\n" );
		fprintf( stderr, "       This module can be used as a standalone program (offline mode)\n" );
		fprintf( stderr, "       by specifing the path to a tank file as second argument.\n" );
		fprintf( stderr, " Version: %s\n", PICK_GM_VERSION);
		return -1;
	}
	configfile = argv[1];

	offline_mode = 0;
	if (argc == 3) {
		offline_mode = 1;
		tracename = argv[2];

		if ( (tracefile = fopen (tracename, "rb")) == NULL ) {
			perror (tracename);
			exit (-1);
		}
	}

	/* Initialize name of log-file & open it */
	logit_init (configfile, 0, 256, 1);

	/* Get parameters from the configuration files */
	if ( GetConfig (configfile, &Gparm) == -1 ) {
		logit( "e", "pick_gm: GetConfig() failed. Exiting.\n" );
		return -1;
	}

	/* Look up info in the earthworm.h tables */
	if ( GetEwh( &Ewh ) < 0 ) {
		logit( "e", "pick_gm: GetEwh() failed. Exiting.\n" );
		return -1;
	}

	/* Specify logos of incoming waveforms and outgoing heartbeats */
	if( Gparm.nGetLogo == 0 ) {
		Gparm.nGetLogo = 2;
		Gparm.GetLogo  = (MSG_LOGO *) calloc( Gparm.nGetLogo, sizeof(MSG_LOGO) );
		if( Gparm.GetLogo == NULL ) {
			logit( "e", "pick_gm: Error allocating space for GetLogo. Exiting\n" );
			return -1;
		}
		Gparm.GetLogo[0].instid = Ewh.InstIdWild;
		Gparm.GetLogo[0].mod    = Ewh.ModIdWild;
		Gparm.GetLogo[0].type   = Ewh.TypeTracebuf2;

		Gparm.GetLogo[1].instid = Ewh.InstIdWild;
		Gparm.GetLogo[1].mod    = Ewh.ModIdWild;
		Gparm.GetLogo[1].type   = Ewh.TypeTracebuf;
	}

	hrtlogo.instid = Ewh.MyInstId;
	hrtlogo.mod    = Gparm.MyModId;
	hrtlogo.type   = Ewh.TypeHeartBeat;

	/* Get our own pid for restart purposes */
	myPid = getpid();
	if ( myPid == -1 ) {
		logit( "e", "pick_gm: Can't get my pid. Exiting.\n" );
		free( Gparm.GetLogo );
		free( Gparm.StaFile );
		return -1;
	}

	/* Log the configuration parameters */
	LogConfig (&Gparm);

	/* Open file pointer */
	//if (Gparm.OutputByFile && tracename != NULL) {

	//	sprintf(fname,"%s.gm", tracename);	
	//	Gparm.fpGM = fopen(fname,"w");
	//	strcpy(Gparm.strFpGM, fname);
	//	if( Gparm.fpGM == NULL )
	//	{
	//		printf("File open error %s !\n", fname);
	//		return 0;
	//	}

	//	sprintf(fname2,"%s.gmx", tracename);	
	//	Gparm.fpGMX = fopen(fname2,"w");
	//	strcpy(Gparm.strFpGMX, fname2);
	//	if( Gparm.fpGMX == NULL )
	//	{
	//		printf("File open error %s !\n", fname2);
	//		return 0;
	//	}
	//}

	/* Allocate the waveform buffer */
	InBufl = MAX_TRACEBUF_SIZ*2 + sizeof(int)*(Gparm.MaxGap-1);
	TraceBuf = (char *) malloc( (size_t) InBufl );
	if ( TraceBuf == NULL ) {
		logit( "et", "pick_gm: Cannot allocate waveform buffer\n" );
		free( Gparm.GetLogo );
		free( Gparm.StaFile );
		return -1;
	}

	/* Point to header and data portions of waveform message */
	TraceHead  = (TRACE_HEADER *)TraceBuf;
	Trace2Head = (TRACE2_HEADER *)TraceBuf;
	TraceLong  = (int *) (TraceBuf + sizeof(TRACE_HEADER));
	TraceShort = (short *) (TraceBuf + sizeof(TRACE_HEADER));

	/* Read the station list and return the number of stations found.
		Allocate the station list array. */
	if ( GetStaList( &StaArray, &Nsta, &Gparm, true ) == -1 ) {
		logit( "e", "pick_gm: GetStaList() failed. Exiting.\n" );
		free( Gparm.GetLogo );
		free( Gparm.StaFile );
		free( StaArray );
		return -1;
	}
	printf("StaArray len: %d\n", Nsta);

	if ( GetStaList( &StaGmxArray, &NstaGmx, &Gparm, false ) == -1 )
	{
		logit( "e", "pick_gm: GetStaList() for GmxArray failed. Exiting.\n" );
		free( Gparm.GetLogo );
		free( Gparm.StaFile );
		free( StaGmxArray );
		return -1;
	}
	printf("StaGmxArray len: %d\n", NstaGmx);

	if ( Nsta == 0 || NstaGmx == 0 ) {
		logit( "et", "pick_gm: Empty station list(s). Exiting." );
		free( Gparm.GetLogo );
		free( Gparm.StaFile );
		free( StaArray );
		return -1;
	}

	/* Sort the station list by SCNL */
	qsort (StaArray, Nsta, sizeof(STATION), CompareSCNL);
	qsort( StaGmxArray, NstaGmx, sizeof(STATION), CompareSCNL );

	/* Log the station list */
	LogStaList( StaArray, Nsta );

	if (!offline_mode) { /* if not in offline mode */
		/* Attach to existing transport rings */
		if ( Gparm.OutKey != Gparm.InKey ) {
			tport_attach( &Gparm.InRegion,  Gparm.InKey );
			tport_attach( &Gparm.OutRegion, Gparm.OutKey );
		} else {
			tport_attach( &Gparm.InRegion, Gparm.InKey );
			Gparm.OutRegion = Gparm.InRegion;
		}

		/* Flush the input ring */
		while ( tport_copyfrom( &Gparm.InRegion, Gparm.GetLogo, (short)Gparm.nGetLogo, 
						 &logo, &MsgLen, TraceBuf, MAX_TRACEBUF_SIZ, &seq) != 
						 GET_NONE );

		/* Get the time when we start reading messages.
		   This is for issuing heartbeats. */
		time( &then );
	}

	/* Create MsgQueue mutex */
	CreateMutex_ew();

	/* Allocate the message Queue */
	initqueue (&(Gparm.MsgQueueGM), 2000, 300);
	initqueue (&(Gparm.MsgQueueGMX), 2000, 300);
	// queueptr, max elements, element max size

	/* Start decimator thread which will read messages from   *
	 * the Queue, fir them and write them to the OutRing */
	
	if (StartThreadWithArg (XmlThreadGM, (void *) &Gparm, (unsigned) THREAD_STACK, 
													&tidThrdGM) == -1)
	{
		logit( "e", "pick_gm: Error starting XmlThreadGM thread.  Exitting.\n");
		exit (EW_FAILURE);
	}
	
	if (StartThreadWithArg (XmlThreadGMX, (void *) &Gparm, (unsigned) THREAD_STACK, 
													&tidThrdGMX) == -1)
	{
		logit( "e", "pick_gm: Error starting XmlThreadGMX thread.  Exitting.\n");
		exit (EW_FAILURE);
	}

	count=0;
	loop_condition=1; /* The loop condition depends on whether we are in offline_mode or not*/
	/* Loop to read waveform messages and invoke the picker */

	// 20171204, jman, Consider waveform not inputed
	time (&now);
	then = now - Gparm.HeartbeatInt - 1;
	while (loop_condition) {

		if (offline_mode) {
			// TODO: check if the file is a valid trace buffer
			loop_condition = (fread (Trace2Head, sizeof(TRACE2_HEADER), 1, tracefile) > 0) &&
					 (fread (TraceLong, TraceHead->nsamp*sizeof(int), 1, tracefile) > 0);
		} else {
			loop_condition = tport_getflag( &Gparm.InRegion ) != TERMINATE &&
					 tport_getflag( &Gparm.InRegion ) != myPid;
		}

		if (!offline_mode) {

			/* Get tracebuf or tracebuf2 message from ring */
			rc = tport_copyfrom( &Gparm.InRegion, Gparm.GetLogo, (short)Gparm.nGetLogo, 
						 &logo, &MsgLen, TraceBuf, MAX_TRACEBUF_SIZ, &seq );

			if ( rc == GET_NONE ) {

				// 20171204, jman, Consider waveform not inputed
				/* Send a heartbeat to the transport ring */
				time (&now);
				if ( (now - then) >= Gparm.HeartbeatInt ) {
					int  lineLen;
					char line[40];
					then = now;
					sprintf (line, "%ld %d\n", (long) now, (int) myPid);
					lineLen = strlen (line);
					if ( tport_putmsg (&Gparm.OutRegion, &hrtlogo, lineLen, line) !=
						PUT_OK ) {
						logit ("et", "pick_FFP: Error sending heartbeat. Exiting.");
						break;
					}
				}

				sleep_ew( 100 );
				continue;
			}

			if ( rc == GET_NOTRACK )
				logit( "et", "pick_gm: Tracking error (NTRACK_GET exceeded)\n");

			if ( rc == GET_MISS_LAPPED )
				logit( "et", "pick_gm: Missed msgs (lapped on ring) "
					"before i:%d m:%d t:%d seq:%d\n",
					(int)logo.instid, (int)logo.mod, (int)logo.type, (int)seq );

			if ( rc == GET_MISS_SEQGAP )
				logit( "et", "pick_gm: Gap in sequence# before i:%d m:%d t:%d seq:%d\n",
					(int)logo.instid, (int)logo.mod, (int)logo.type, (int)seq );

			if ( rc == GET_TOOBIG ) {
				logit( "et", "pick_gm: Retrieved msg is too big: i:%d m:%d t:%d len:%ld\n",
					(int)logo.instid, (int)logo.mod, (int)logo.type, MsgLen );
				continue;
			}

			/* If necessary, swap bytes in tracebuf message */
			if ( logo.type == Ewh.TypeTracebuf ) {
				if ( WaveMsgMakeLocal (TraceHead) < 0 ) {
					logit( "et", "pick_gm: WaveMsgMakeLocal() error.\n" );
					continue;
				}
			} else {
				if ( WaveMsg2MakeLocal (Trace2Head) < 0 ) {
					logit( "et", "pick_gm: WaveMsg2MakeLocal error.\n" );
					continue;
				}
			}

			/* Convert TYPE_TRACEBUF messages to TYPE_TRACEBUF2 */
			if ( logo.type == Ewh.TypeTracebuf )
				Trace2Head = TrHeadConv (TraceHead);
		}
		
		/* Look up SCNL number in the station list */
		{
			int j;
			for ( j = 0; j < 5; j++ ) key.sta[j]  = Trace2Head->sta[j];
			key.sta[5] = '\0';
			for ( j = 0; j < 3; j++ ) key.chan[j] = Trace2Head->chan[j];
			key.chan[3] = '\0';
			for ( j = 0; j < 2; j++ ) key.net[j]  = Trace2Head->net[j];
			key.net[2] = '\0';
			for ( j = 0; j < 2; j++ ) key.loc[j]  = Trace2Head->loc[j];
			key.loc[2] = '\0';
		}

		Sta = (STATION *) bsearch (&key, StaArray, Nsta, sizeof(STATION),
						CompareSCNL);
		if ( Sta == NULL )      /* SCNL not found */
			continue;
		
		/* Do this the first time we get a message with this SCNL */
		if ( Sta->first == 1 ) {
			Sta->endtime = Trace2Head->endtime;
			Sta->first = 0;
			continue;
		}

		/* If the samples are shorts, make them longs */
		strcpy (type, Trace2Head->datatype);
		if ( (strcmp(type,"i2")==0) || (strcmp(type,"s2")==0) ) {
			for ( i = Trace2Head->nsamp - 1; i > -1; i-- )
				TraceLong[i] = (int)TraceShort[i];
		}

		/* 
		 * Compute the number of samples since the end of the previous message.
		 * If (GapSize == 1), no data has been lost between messages.
		 * If (1 < GapSize <= Gparm.MaxGap), data will be interpolated.
		 * If (GapSize > Gparm.MaxGap), the picker will go into restart mode.
		*/
		GapSizeD = Trace2Head->samprate * (Trace2Head->starttime - Sta->endtime);

		if ( GapSizeD < 0. )          /* Invalid. Time going backwards. */
			GapSize = 0;
		else
			GapSize  = (int) (GapSizeD + 0.5);

		/* Interpolate missing samples and prepend them to the current message */
		if ( (GapSize > 1) && (GapSize <= Gparm.MaxGap) )
			Interpolate (Sta, TraceBuf, GapSize);

		/* Announce large sample gaps */
		if ( GapSize > Gparm.MaxGap ) {
			int      lineLen;
			time_t   errTime;
			char     errmsg[80];
			MSG_LOGO logo;

			time( &errTime );
			sprintf( errmsg,
				"%ld %d Found %4d sample gap. Restarting channel %s.%s.%s.%s\n",
				(long) errTime, PK_RESTART, GapSize, Sta->sta, Sta->chan, Sta->net, Sta->loc );
			lineLen = strlen( errmsg );
			logo.type   = Ewh.TypeError;
			logo.mod    = Gparm.MyModId;
			logo.instid = Ewh.MyInstId;
			if(!offline_mode) tport_putmsg (&Gparm.OutRegion, &logo, lineLen, errmsg);
			else printf("%s", errmsg);
		}

		if ( Restart (Sta, &Gparm, Trace2Head->nsamp, GapSize) ) 
		{
			for ( i = 0; i < Trace2Head->nsamp; i++ )
			{
				
				double pkt_time = (1.0*i)/(Trace2Head->samprate)+(Trace2Head->starttime);
				sniff_gm(pkt_time, TraceLong[i], Sta, 1);


				if (Sta->is_oversec)
				{   
					writeGM(Sta, &Gparm, &Ewh);
					Sta->is_oversec = false;
								 
					StaGmx = (STATION *) bsearch( &key, StaGmxArray, NstaGmx, sizeof(STATION),
																			CompareSNL );
					if ( StaGmx != NULL)
					{   
							sniff_gmx(Sta, StaGmx);
							if (StaGmx->is_oversec)
							{
								writeGMX(StaGmx, &Gparm, &Ewh);
								StaGmx->is_oversec = false;
							}
					}
				}
			}
		} else {

			/* Call sniff_gm */
			for ( i = 0; i < Trace2Head->nsamp; i++ )
			{

				double pkt_time = (1.0*i)/(Trace2Head->samprate)+(Trace2Head->starttime);

				sniff_gm(pkt_time, TraceLong[i], Sta, 0);

				if (Sta->is_oversec)
				{
					writeGM(Sta, &Gparm, &Ewh);
					Sta->is_oversec = false;
					
					StaGmx = (STATION *) bsearch( &key, StaGmxArray, NstaGmx, sizeof(STATION),
												CompareSNL );
					if ( StaGmx != NULL) 
					{
						sniff_gmx(Sta, StaGmx);
						if (StaGmx->is_oversec)
						{
							writeGMX(StaGmx, &Gparm, &Ewh);
							StaGmx->is_oversec = false;
						}
					}
				}
			}
		}

		/* Save time and amplitude of the end of the current message */
		Sta->enddata = TraceLong[Trace2Head->nsamp - 1];
		Sta->endtime = Trace2Head->endtime;

		if (!offline_mode) {
			/* Send a heartbeat to the transport ring */
			time (&now);
			if ( (now - then) >= Gparm.HeartbeatInt ) {
				int  lineLen;
				char line[40];

				then = now;

				sprintf (line, "%ld %d\n", (long) now, (int) myPid);
				lineLen = strlen (line);
				if ( tport_putmsg (&Gparm.OutRegion, &hrtlogo, lineLen, line) !=
					PUT_OK ) {
					logit ("et", "pick_gm: Error sending heartbeat. Exiting.");
					break;
				}
			}
		}
	} /* End of loop on waveform messages */

	if (!offline_mode) { /* if not in offline_mode */
		/* Detach from the ring buffers */
		if ( Gparm.OutKey != Gparm.InKey ) {
			tport_detach( &Gparm.InRegion );
			tport_detach( &Gparm.OutRegion );
		} else {
			tport_detach( &Gparm.InRegion );
		}

		logit( "t", "Termination requested. Exiting.\n" );
	}

	if (Gparm.OutputByFile) {
		if ( Gparm.fpGM != NULL) 
		{
			/* Reflects the last changes */
			RequestMutex ();
			enqueue (&(Gparm.MsgQueueGM), Gparm.strFpGM, strlen(Gparm.strFpGM), fooMsg); 
			ReleaseMutex_ew ();
			fclose(Gparm.fpGM);
		}
		if ( Gparm.fpGMX != NULL) 
		{
			RequestMutex ();
			enqueue (&(Gparm.MsgQueueGMX), Gparm.strFpGMX, strlen(Gparm.strFpGMX), fooMsg); 
			ReleaseMutex_ew ();
			fclose(Gparm.fpGMX);
		}
	}

	/* waiting for thread to process the data */
	sleep_ew(200);
	if ( Gparm.OutputByFile ) 
	{
		while(true)
		{	
			numOfElementsInQueue = getNumOfElementsInQueue(&(Gparm.MsgQueueGM));
			if (numOfElementsInQueue > 0 || !Gparm.isCompleteThrdGM ) {
				sleep_ew(200);
				continue;
			}
			printf("ThreadGM end..\n");

			numOfElementsInQueue = getNumOfElementsInQueue(&(Gparm.MsgQueueGMX));
			if (numOfElementsInQueue > 0 || !Gparm.isCompleteThrdGMX ) {
				sleep_ew(200);
				continue;
			}
			printf("ThreadGMX end..\n");

			sleep_ew(200);
			break;
		}
	}

	free (Gparm.GetLogo);
	free (Gparm.StaFile);
	free (StaArray);
	free (TraceBuf);

	return 0;
}

int writeGM(STATION *Sta, GPARM *Gparm, EWH *Ewh) 
{
	struct Greg g;
	double  sec1970 = GSEC1970;
	char year[5];
	char mmdd[5];
	char hour[3];
	char minute[3];
	char temp[300];
	int ret;

	if (Gparm->OutputByFile) {
		// setting file pointer
		datime (Sta->tm + sec1970, &g);
		sprintf(year, "%4d", g.year);
		sprintf(mmdd, "%02d%02d", g.month, g.day);
		sprintf(hour, "%02d", g.hour);
		sprintf(minute, "%02d", g.minute);

		// check year
		sprintf(temp, "%s%c%s", Gparm->OutputFolder, kPathSeparator, year);

		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
		#if defined(WIN32) || defined(WIN64)
		_mkdir(temp);
		#else
		mkdir(temp, 0755);
		#endif

		// check mmdd
		sprintf(temp, "%s%c%s%c%s", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd);
		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
		#if defined(WIN32) || defined(WIN64)
		_mkdir(temp);
		#else
		mkdir(temp, 0755);
		#endif

		// check hour
		sprintf(temp, "%s%c%s%c%s%c%s", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd, kPathSeparator, hour);
		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
		#if defined(WIN32) || defined(WIN64)
		_mkdir(temp);
		#else
		mkdir(temp, 0755);
		#endif

		// check fopen
		sprintf(temp, "%s%c%s%c%s%c%s%c%s%s%s%s.gm", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd, kPathSeparator, hour, kPathSeparator
			, year, mmdd, hour, minute);
		if ( Gparm->fpGM == NULL) {
			strcpy(Gparm->strFpGM, temp);
			Gparm->fpGM = fopen(temp,"a");
			if( Gparm->fpGM == NULL )
			{
				printf("File open error %s !\n", temp);
				return 0;
			}
		} else {
			if (!strcmp(temp, Gparm->strFpGM)) {
				// same
			} 
			else 
			{
				// different
				fclose(Gparm->fpGM);

				RequestMutex ();
				ret = enqueue (&(Gparm->MsgQueueGM), Gparm->strFpGM, strlen(Gparm->strFpGM), fooMsg); 
				// queue ptr, data, datasize, message logo
				//printf("queue size: %d\n", getNumOfElementsInQueue(&(Gparm->MsgQueueGM)));
				ReleaseMutex_ew ();
				sleep_ew(1);

				strcpy(Gparm->strFpGM, temp);
				Gparm->fpGM = fopen(temp,"a");
				if( Gparm->fpGM == NULL )
				{
					printf("File open error %s !!\n", temp);
					return 0;
				}
			}
		}
	}
	
	reportGM(Sta, Gparm, Ewh);
	return 1;
}

int writeGMX(STATION *Sta, GPARM *Gparm, EWH *Ewh) 
{
	struct Greg g;
	double  sec1970 = GSEC1970;
	char year[5];
	char mmdd[5];
	char hour[3];
	char minute[3];
	char temp[300];
	int ret;

	if (Gparm->OutputByFile) {
		// setting file pointer
		datime (Sta->tm + sec1970, &g);
		sprintf(year, "%4d", g.year);
		sprintf(mmdd, "%02d%02d", g.month, g.day);
		sprintf(hour, "%02d", g.hour);
		sprintf(minute, "%02d", g.minute);

		// check year
		sprintf(temp, "%s%c%s", Gparm->OutputFolder, kPathSeparator, year);
		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
			#if defined(WIN32) || defined(WIN64)
			_mkdir(temp);
			#else
			mkdir(temp, 0755);
			#endif
			//sprintf(systemCmd, "[ -d %s ] || mkdir %s", temp, temp);
			//system(systemCmd);
		//}

		// check mmdd
		sprintf(temp, "%s%c%s%c%s", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd);
		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
			#if defined(WIN32) || defined(WIN64)
			_mkdir(temp);
			#else
			mkdir(temp, 0755);
			#endif
			//sprintf(systemCmd, "[ -d %s ] || mkdir %s", temp, temp);
			//system(systemCmd);
		//}

		// check hour
		sprintf(temp, "%s%c%s%c%s%c%s", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd, kPathSeparator, hour);
		//if (stat(temp, &st) == 0 && S_ISDIR(st.st_mode)) {}
		//else 
		//{
			#if defined(WIN32) || defined(WIN64)
			_mkdir(temp);
			#else
			mkdir(temp, 0755);
			#endif
			//sprintf(systemCmd, "[ -d %s ] || mkdir %s", temp, temp);
			//system(systemCmd);
		//}

		// check fopen
		sprintf(temp, "%s%c%s%c%s%c%s%c%s%s%s%s.gmx", Gparm->OutputFolder, kPathSeparator, year, kPathSeparator, mmdd, kPathSeparator, hour, kPathSeparator
			, year, mmdd, hour, minute);
		if ( Gparm->fpGMX == NULL) {
			strcpy(Gparm->strFpGMX, temp);
			Gparm->fpGMX = fopen(temp,"a");
			if( Gparm->fpGMX == NULL )
			{
				printf("File open error %s !\n", temp);
				return 0;
			}
		} else {
			if (!strcmp(temp, Gparm->strFpGMX)) {
				// same
			} 
			else 
			{
				// different
				fclose(Gparm->fpGMX);

				RequestMutex ();
				ret = enqueue (&(Gparm->MsgQueueGMX), Gparm->strFpGMX, strlen(Gparm->strFpGMX), fooMsg); 
				ReleaseMutex_ew ();
				sleep_ew(1);

				strcpy(Gparm->strFpGMX, temp);
				Gparm->fpGMX = fopen(temp,"a");
				if( Gparm->fpGMX == NULL )
				{
					printf("File open error %s !\n", temp);
					return 0;
				}
			}
		}
	}
	
	reportGMX(Sta, Gparm, Ewh);
	return 1;
}

/*******************************************************
*                      GetEwh()                       *
*                                                     *
*      Get parameters from the earthworm.h file.      *
*******************************************************/
int GetEwh( EWH *Ewh )
{
	if ( GetLocalInst( &Ewh->MyInstId ) != 0 )
	{
		logit( "e", "pick_gm: Error getting MyInstId.\n" );
		return -1;
	}

	if ( GetInst( "INST_WILDCARD", &Ewh->InstIdWild ) != 0 )
	{
		logit( "e", "pick_gm: Error getting InstIdWild.\n" );
		return -2;
	}
	if ( GetModId( "MOD_WILDCARD", &Ewh->ModIdWild ) != 0 )
	{
		logit( "e", "pick_gm: Error getting ModIdWild.\n" );
		return -3;
	}
	if ( GetType( "TYPE_HEARTBEAT", &Ewh->TypeHeartBeat ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TypeHeartbeat.\n" );
		return -4;
	}
	if ( GetType( "TYPE_ERROR", &Ewh->TypeError ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TypeError.\n" );
		return -5;
	}
	if ( GetType( "TYPE_STRONG_MOTION_GM", &Ewh->TypeStrongMotionGM ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TypeStrongMotionGM.\n" );
		return -6;
	}
	if ( GetType( "TYPE_STRONG_MOTION_GMX", &Ewh->TypeStrongMotionGMX ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TypeStrongMotionGMX.\n" );
		return -7;
	}
	if ( GetType( "TYPE_TRACEBUF", &Ewh->TypeTracebuf ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TYPE_TRACEBUF.\n" );
		return -8;
	}
	if ( GetType( "TYPE_TRACEBUF2", &Ewh->TypeTracebuf2 ) != 0 )
	{
		logit( "e", "pick_gm: Error getting TYPE_TRACEBUF2.\n" );
		return -9;
	}	
	return 0;
}
