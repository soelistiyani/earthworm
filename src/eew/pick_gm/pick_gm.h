/*
 * pick_gm.h
 * modified from pick_ew.h, Revision 1.7 in src/seismic_sampling/pick_ew
 * 
 * This file is part of pick_FP, Earthworm module for the FilterPicker phase detector and picker.
 *
 * (C) 2008-2012 Claudio Satriano <satriano@ipgp.fr>,
 * under the same license terms of the Earthworm software system. 
 */

/******************************************************************
 *                         File pick_FP.h                         *
 ******************************************************************/

#include <mem_circ_queue.h>

#define LINELEN 250         /* Size of char arrays to hold picks and codas */
#define ACCUMLEN 30
#define GMXLEN 600          /* 10 minute buffer */
#define BUFFLEN 120
/* Error bits */
#define PK_RESTART 1        /* Set when time series was broken, picker restarted */
#define THREAD_STACK    8192    /* How big is our thread stack */

typedef enum {false, true} bool;

typedef struct {
	double tm;
	double pga;
	double cav;
	int viewCnt;
	double minTm;
} GMX;

typedef struct {
	double firstArrTm;
	double buffer[BUFFLEN];
} GMBUFFER;

/* Station list parameters */
typedef struct {
	char   sta[6];           /* Station name */
	char   chan[4];          /* Component code */
	char   net[3];           /* Network code */
	char   loc[3];           /* Location code */

	long   enddata;          /* Last data value of previous message */
	double endtime;          /* Stop time of previous message */
	int    first;            /* 1 the first time this channel is found */
	int    ns_restart;       /* Number of samples since restart */

	int    inst;   // for instrument type 1-Acc 2-BB 3-Short Period does not use for magnitude determination
	int    srate;   
	double gain;
	double lat;
	double lon;        

	double dt;	// 1/samprate

	double X1;
	double X2;
	double Y1;
	double Y2;

	double XX1;
	double XX2;
	double YY1;
	double YY2;

	double x1;
	double x2;
	double y1;
	double y2;    

	double xx1;
	double xx2;
	double yy1;
	double yy2;   

	double ave;
	double ave0;
	double ave1;
	double acc0;    
	double vel0;

	double acc0_vel;    
	double vel0_dis;     

	double vvsum;
	double ddsum;

	double a;
	double v;
	double d;
	double ddv;
  
	double pa;	// by Seo JeongBeom

	double tc;
	double avd[3];

	/* New variable for pickpga */
	long pkt_1s_time;

	double tm;
	double pga;
	double pgd;
	double pgv;
	double cav;
	double maxpga;
	double lbspga;
	double bcav;
	double bcav_mmi;	// by Seo JeongBeom
	double lbspga_mmi;	// by Seo JeongBeom
	
	double t_pga;
	double t_pgd;
	double t_pgv;
	double t_cav;
	double arpga[ACCUMLEN];	/* In 30sec, abs(pga) */
	double arcav[ACCUMLEN];	/* In 30sec, abs(pga) */	

//	bool is_cav;			/* Within 1sec, Is exist over 0.025 value? */
	bool is_oversec;

	/* New variable for pickpga-x */
	GMX gmx[GMXLEN]; 

	/* New variable for make xml */
	GMBUFFER gmBuffer;

	/* for skip first 50sec data */
	double first_tm;

} STATION;

#define STAFILE_LEN 128
typedef struct {
	char   name[STAFILE_LEN]; /* Name of station file */
	int    nsta;              /* number of channels configure in this file */
} STAFILE;

typedef struct {
	STAFILE  *StaFile;       /* Name of file(s) with SCNL info */

	int       nStaFile;      /* Number of StaFile commands given */
	long      InKey;         /* Key to ring where waveforms live */
	long      OutKey;        /* Key to ring where picks will live */
	int       HeartbeatInt;  /* Heartbeat interval in seconds */
	int       RestartLength; /* Number of samples to process for restart */
	int       MaxGap;        /* Maximum gap to interpolate */
	int       Debug;         /* If 1, print debug messages */
	int 	  OutputByFile;  /* If 1, write to file */
	int 	  OutputByConsole;   /* If 1, write to screen */
	char 	  OutputFolder[200];   /* If 1, write to screen */
	unsigned char MyModId;   /* Module id of this program */
	SHM_INFO  InRegion;      /* Info structure for input region */
	SHM_INFO  OutRegion;     /* Info structure for output region */
	int       nGetLogo;      /* Number of logos in GetLogo   */
	MSG_LOGO *GetLogo;       /* Logos of requested waveforms */
	FILE     *fpGM;			 /* File pointer for GM */
	FILE     *fpGMX;		 /* File pointer for GMX */
	char      strFpGM[300];
	char      strFpGMX[300];
	QUEUE 	  MsgQueueGM;		 /* The message queue */
	QUEUE 	  MsgQueueGMX;		 /* The message queue */
	int       isCompleteThrdGM;   /* Flag for Complete write*/
	int       isCompleteThrdGMX;  /* Flag for Complete write*/
	char      MMISource;           /* Source of MMI */
} GPARM;

typedef struct {
	unsigned char MyInstId;        /* Local installation */
	unsigned char InstIdWild;      /* Wildcard for inst id */
	unsigned char ModIdWild;       /* Wildcard for module id */
	unsigned char TypeHeartBeat;
	unsigned char TypeError;

	unsigned char TypeStrongMotionGM;
	unsigned char TypeStrongMotionGMX;
	unsigned char TypeTracebuf;    /* Waveform buffer for data input (no loc code) */
	unsigned char TypeTracebuf2;   /* Waveform buffer for data input (w/loc code) */
} EWH;
