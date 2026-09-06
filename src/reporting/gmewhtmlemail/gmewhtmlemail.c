/******************************************************************************
 *                                GMEWHTMLEMAIL                               *
 *                                                                            *
 * Simplified graphical email alert for earthworm using google APIs.          *
 *                                                                            *
 *                                                                            *
 * Description:                                                               *
 * Produces an web file (html) containing graphical information on detected   *
 * earthquakes. The presented graphical information consists on a map of the  *
 * computed hypocenter and reporting stations as well as the seismic traces   *
 * retrieved from a WaveServer. The graphical information is produced using   *
 * google maps and google charts APIs. As a consequence, correct visualization*
 * of the web file requires an internet connection. The web files may be sent *
 * to a set of recipients via email. In this case, gmewhtmlemail uses         *
 * sendmail, which is common with several linux distributions.                *
 * The code is a combination of seisan_report to retrieve hyp2000arc          *
 * messages, and gmew to access and retrieve data from a set of waveservers.  *
 * The code includes custom functions to generate the requests to the google  *
 * APIs and embeded these on the output html file.                            *
 * Besides the typical .d configuration file, gmewhtmlemail requires a css    *
 * file with some basic style configurations to include in the output html.   *
 *                                                                            *
 * Written by R. Luis from CVARG in July, 2011                                *
 * Contributions and testing performed by Jean-Marie Saurel, from OVSM/IPGP   *
 * Later modifications for features and blat, by Paul Friberg                 *
 *                                                                            *
 * Two important URLs to follow:                                              *
 *    https://developers.google.com/maps/documentation/staticmaps/            *
 *    https://developers.google.com/chart/interactive/docs/reference/         *
 *****************************************************************************/

#define VERSION_STR "1.8.2a - 2019-06-14"

#define MAX_STRING_SIZE 1024    /* used for many static string place holders */
#define CM_S_S_1_g 980		/* how many cm/s/s in 1 g, used for conversion */

/* Includes
 **********/
#include <ctype.h>
#include <math.h>
#include <stdio.h>		/*  pclose(),  popen() on *nix */
				/* _pclose(), _popen() on MSVC */
#include <stdint.h>		/* uint16_t, uint32_t, uint64_t, UINT64_C() */
#include <stdlib.h>
#include <string.h>		/* strtok_r() on *nix */
				/* strtok_s() on MSVC */
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define pclose     _pclose	/* _pclose()   has the same signature as pclose()   */
#define popen      _popen	/* _popen()    has the same signature as popen()    */
#define strtok_r    strtok_s	/*  strtok_s() has the same signature as strtok_r() */
#endif

#include "geo.h"
#include "earthworm.h"
#include "transport.h"
#include "ws_clientII.h"
#include "trace_buf.h"
#include "swap.h"
#include "kom.h"
#include "read_arc.h"
/* #include "rw_mag.h" */
#include "rw_strongmotionII.h"
#include "site.h"
#include "chron3.h"
#include "gd.h"
#include "gdfonts.h"
#include "fleng.h"
#include "ioc_filter.h"		/*header containing filtering functions*/
#include "kml_event.h"
#include "sqlite3.h"
#include "time_ew.h"		/* gmtime_ew(), localtime_ew() */

char neic_id[30];

/* Defines
 *********/
#define MAX_STATIONS 200
#define MAX_STREAMS 1024        /* Max number of stream conversion factors to process */
#define MAX_GET_SAMP 2400       /* Number of samples in a google chart */
#define MAX_REQ_SAMP 600        /* Maximum number of samples per google request */
#define MAX_GET_CHAR 5000       /* Maximum number of characters in a google GET request */
#define MAX_POL_LEN 120000      /* Maximum number of samples in a trace gif */
#define MAX_WAVE_SERVERS 10
#define MAX_COLOR_PGAS 10
#define MAX_ADDRESS 80
#define MAX_PORT 6
#define MAX_EMAIL_CHAR 160
#define DEFAULT_SUBJECT_PREFIX "EWalert"
#define DEFAULT_GOOGLE_STATIC_MAPTYPE "hybrid"
#define DEFAULT_MAPQUEST_STATIC_MAPTYPE "hyb"
#define OTHER_WORLD_DISTANCE_KM 100000000.0
#define DUMMY_MAG 9.0
#define KM2MILES 0.621371
#define MAX_SM_PER_EVENT    40
#define MAX_REGIONS     20
#define MAX_AREAS       50
#define MAX_SUB_DIST    100
#define MAX_SUB_DAMS    1
#define BEFORE_MAX_COLOR    "F0F0F0"
#define AFTER_MAX_COLOR    "FFFFFF"
#define font_open "<font size=\"1\" face=\"Sans-serif\">"
#define font_close "</font>"
#define MAX_SCNL_STRING 32 /* max size of a SCNL string */

/* Structures
 ************/

// Waveservers
typedef struct {
    char wsIP[MAX_ADDRESS];
    char port[MAX_PORT];
} WAVESERV;

// Email Recipients
// TODO: Upgrade to have different parameters for each email recipient
typedef struct {
    char address[MAX_EMAIL_CHAR];
    char subscription;
    char sendOnEQ;
    char sendOnSM;
    double min_magnitude;
    double max_distance;
    int numRegions;
    int numAreas;
    int numDams;
    uint32_t subRegions;
    uint64_t subAreas;
    uint16_t subDams[MAX_SUB_DAMS];
    int shouldSendEmail;         //flag indicating whether or not email is to be sent to this address
} EMAILREC;

typedef struct HYPOARCSM2 {
    HypoArc ha;
    SM_INFO sm_arr[MAX_SM_PER_EVENT];
    int     sm_count;
    time_t  deliveryTime;
    char    fromArc;
    char    sncl[MAX_SCNL_STRING];
} HypoArcSM2;

typedef struct site_order {
    double  key;
    int     idx;
} SiteOrder;

typedef struct SNCLConv {
	char SNCL[20];
	double convFactor;
} Conv;

typedef struct dam_info {
    int    dam_id;
    int    region_id;
    int    area_id;
    double lat;
    double lon;
    double dist;
    char   name[200];
    char   station[10];
} DamInfo;

char region_names[MAX_REGIONS][MAX_STRING_SIZE] = {""};
/*,
    "Great Plains GP",
    "Lower Colorado LC",
    "Mid-Pacific MP",
    "Pacific Northwest PN",
    "Upper Colorado UC" };*/

char region_abbrs[MAX_REGIONS][5] = {""}; //,"GP","LC", "MP","PN", "UC" };

int region_order[MAX_REGIONS] = {-1}; //,1,2,3,4,5,99};

char area_abbrs[MAX_AREAS][5] = {""};
/*    "","DK","EC","MT","NK","OT","WY","LCD","PX","SC",
    "Y","CC","KB","LB","NC","SCC","LC","SR","UC","A",
    "P","WC"}; */
int area_order[MAX_AREAS] = {-1}; //,19,11,1,2,12,13,16,7,3,14,4,5,20,8,9,15,17,18,21,6,10,99};
char area_names[MAX_AREAS][MAX_STRING_SIZE] = {""};

/* Functions in this file
 ************************/
void config(char*);
void lookup(void);
void status( unsigned char, short, char* );
int process_message( int arc_index ); //SM_INFO *sm ); //, MAG_INFO *mag, MAG_INFO *mw);
void MapRequest(char*, SITE**, int, double, double, HypoArcSM2*, SiteOrder*, SiteOrder* );
void GoogleMapRequest(char*, SITE**, int, double, double, HypoArcSM2*, SiteOrder*, SiteOrder* );
void MapQuestMapRequest(char*, SITE**, int, double, double, HypoArcSM2*, SiteOrder*, SiteOrder* );
char simpleEncode(int);
int searchSite(char*, char*, char*, char*);
char* getWSErrorStr( int, char* );
int getWStrbf( char*, int*, SITE*, double, double );
int trbufresample( int*, int, char*, int, double, double, int, double* );
double fbuffer_add( double*, int, double );
int createGifPlot(char *chartreq, HypoArc *arc, char *buffer, int bsamplecount, SITE *waveSite, double starttime, double endtime, char phaseName[2]);
int writePlotRequest(char *, size_t, long, char *, char *, char *, char *, char *, char *, time_t, int); 
int makeGoogleChart( char*, int*, int, char *, double, int, int, double* );
void gdImagePolyLine( gdImagePtr, gdPointPtr, int, int );
int trbf2gif( char*, int, gdImagePtr, double, double, int, int );
int pick2gif(gdImagePtr, double, char *, double, double, int);
int gmtmap(char *request,SITE **sites, int nsites,double hypLat, double hypLon,int idevt);
void InsertShortHeaderTable(FILE *htmlfile, HypoArc *arc, char Quality);
void InsertHeaderTable(FILE *htmlfile, HypoArc *arc, char Quality, int showDM, char *, SM_INFO*);
int InsertStationTable(FILE *, char, int, SiteOrder*, SITE**, SM_INFO **, int *, SiteOrder*, double*);
int InsertDamTable(FILE *htmlfile, HypoArc *arc, double *max_epga_g);
unsigned char* base64_encode( size_t* output_length, const unsigned char *data, size_t input_length );
double deg2rad(double deg);
double rad2deg(double rad);
double distance(double lat1, double lon1, double lat2, double lon2, char unit);
char* read_disclaimer( char* path );
int formatTimestamps( time_t t, double lat, double lon, char* timestr, char* timestrUTC );
double EstimatePGA( double lat, double lon, double evt_lat, double evt_lon, double evt_depth, double evt_mag );
int compareDamOrder( const void*dam_p_1, const void*dam_p_2 );
int shouldSendEmail( int i, double distance_from_center, HypoArc *arc, HypoArcSM2 *arcsm, uint32_t subRegionsUsed, uint64_t subAreasUsed );
void prepare_dam_info( HypoArc *arc, char* regionsStr, char* regionsUsed, uint32_t* subRegionsUsed, uint64_t* subAreasUsed );
int conversion_read(char *path);

/* Globals
 *********/
static SHM_INFO   InRegion; // shared memory region to use for input
static pid_t      MyPid;    // Our process id is sent with heartbeat

/* Things to read or derive from configuration file
 **************************************************/
static int        LogSwitch;              // 0 if no logfile should be written
static long       HeartbeatInt;           // seconds between heartbeats
static long       MaxMessageSize = 4096;  // size (bytes) of largest msg
static int        Debug = 0;              // 0=no debug msgs, non-zero=debug
static int        DebugEmail = 0;              // 0=no debug msgs, non-zero=debug
static MSG_LOGO  *GetLogo = NULL;         // logo(s) to get from shared memory
static short      nLogo = 0;              // # of different logos
static int        MAX_SAMPLES = 60000;    // Number of samples to get from ws
static long       wstimeout = 5;          // Waveserver Timeout in Seconds
static char       HTMLFile[MAX_STRING_SIZE];          // Base name of the out files
static char       EmailProgram[MAX_STRING_SIZE];      // Path to the email program
//static char       StyleFile[MAX_STRING_SIZE];         // Path to the style (css) file
static char       SubjectPrefix[MAX_STRING_SIZE];     // defaults to EWalert, settable now
static char       KMLdir[MAX_STRING_SIZE];          // where to put the kml files, dir must exist
static char       KMLpreamble[MAX_STRING_SIZE];          // where to find the KML preamble needed for this.
static int        nwaveservers = 0;       // Number of waveservers
static int        ncolorpga = 0;          // Number of color PGA values
static int        nemailrecipients = 0;   // Number of email recipients
static int        nStaticEmailRecipents = 0;
static int        NoWaveformPlots = 0;	  // set to 1 to turn off waveform plots (google static charts of time-series)
static double     TimeMargin = 10;        // Margin to download samples.
static double     DurationMax = 144.;     // Maximum duration to show., truncate to this if greater (configurable)
static long       EmailDelay = 30;        // Time after reciept of trigger to send email
static int        UseBlat = 0;        // use blat syntaxi as the mail program instead of a UNIX like style
static int        UseRegionName = 0;        // put Region Name from FlynnEnghdal region into table at top, off by default
static char       BlatOptions[MAX_STRING_SIZE];      // apply any options needed after the file....-server -p profile etc...
static char       StaticMapType[MAX_STRING_SIZE];      // optional, specification of google map type
static int        UseUTC = 0;         // use UTC in the timestamps of the html page
static int        DontShowMd = 0;         // set to 1 to not show Md value
static int        DontUseMd = 0;         // set to 1 to not use Md value in MinMag decisions
static int        ShowDetail = 0;     // show more detail on event
static int        ShortHeader = 0;     // show a more compact header table
static char       MinQuality = 'D'; // by default send any email with quality equal to or greater than this
static char       DataCenter[MAX_STRING_SIZE];  // Datacenter name to be presented in the resume table.(optional)
static int        SPfilter = 0;                       // Shortperiod filtering (optional)
static char       Cities[MAX_STRING_SIZE];          //cities filepath to be printed with gmt in the output map
static int        GMTmap=0;                     //Enable GMT map generation (Optional)
static int        StationNames=0;                 // Station names in the GMT map (Optional if GMTmap is enable)  (Optional)
static char       MapLegend[MAX_STRING_SIZE];       // Map which contains the map legend to be drawn in the GMT map (optional)
static int        Mercator=1;                            // Mercator projection. This is the default map projection.
static int        Albers=0;                          // Albers projection.
static int        MaxStationDist = 100;       // Maximum distance (in km) from origin for stations to appear in email
static int        MaxFacilityDist = 100;       // Maximum distance (in km) from origin for facilities to appear in email
static char       db_path[MAX_STRING_SIZE] = {0};                      // absolute path to sqlite DB file
static char       dbAvailable;              // 0 if quake ID trabnsaltion DB not available
static double     center_lat = 0.0;  // for distance check from center point
static double     center_lon = 0.0;  // for distance check from center point
static int        ShowMiles = 0;       // display distainces in miles as well as km
static int        MaxArcAge = 0;       // ignore Arc message more than this many secs old (0 means take all)
static char*      EarthquakeDisclaimer = NULL;
static char*      TriggerDisclaimer = NULL;
static int        PlotAllSCNLs = 0;     // Plot all SCNLs; default is false (plot max SCNL for each SNL)
static char       MapQuestKey[MAX_STRING_SIZE] = {0};
static char       MQStaticMapType[MAX_STRING_SIZE];      // optional, specification of MapQuest map type
static int        facility_font_size = 3;
static int        ShowRegionAndArea = 0;
static int        showMax = 1;
static double     IgnoreHours = 0;
static char       PathToPython[MAX_STRING_SIZE] = {0};
static char       PathToLocaltime[MAX_STRING_SIZE] = {0};
static char       PathToEventEmail[MAX_STRING_SIZE] = {0};
static char       ReportEstimatedPGAs = 0;
static char       EstimatePGAsPath[MAX_STRING_SIZE] = {0};
static char       SubscriptionPath[MAX_STRING_SIZE] = {0};
static double     SubscriptionTimestamp = 0;
static double     PGAThreshold = 0.0001; /* (note this is in cm/s/s) */
static int        MaxFacilitiesInTable = -1;
static double     IgnorePGABelow = 0.01;
static Conv 	  ConversionFactors[MAX_STREAMS] = {0};

//static char       dam_info_kind = 0;

/* RSL: 2012.10.10 Gif options */
static int        UseGIF = 0;            // Use GIF files instead of google charts
static int        TraceWidth = 600;        // Width of the GIF files
static int        TraceHeight = 61;        // Height of the GIF files

static int        UseEWAVE = 0;			//Use the EWAVE plotting service to generate images
static char       ewaveAddr[64];
static char       ewavePort[16];

//Option to restrict thresholds to a particular network:
static int		 RestrictThresholds = 0;
static char      RestrictedNetwork[8] = {0};

// Array of waveservers
static WAVESERV   waveservers[MAX_WAVE_SERVERS];
// Array of Color PGA values and colors
static double colorpga_values[MAX_COLOR_PGAS];
static char   colorpga_colors[MAX_COLOR_PGAS][MAX_STRING_SIZE];
// Array of email recipients
static EMAILREC   *emailrecipients = NULL;

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static long               InRingKey = 0;    // key of transport ring for input
static unsigned char      InstId = 0;       // local installation id
static unsigned char      MyModId = 0;      // Module Id for this program
static unsigned char      TypeHeartBeat = 0;
static unsigned char      TypeError = 0;
static unsigned char      TypeHYP2000ARC = 0;
static unsigned char      TypeTraceBuf2 = 0;
static unsigned char      TypeSTRONGMOTIONII = 0;
static unsigned char      TypeTHRESH = 0;


/* Error messages used by gmewhtmlemail
 ************************************/
#define  ERR_MISSGAP       0   // sequence gap in transport ring         
#define  ERR_MISSLAP       1   // missed messages in transport ring      
#define  ERR_TOOBIG        2   // retreived msg too large for buffer     
#define  ERR_NOTRACK       3   // msg retreived; tracking limit exceeded 
static char  Text[150];        // string for log/error messages

#define MAX_QUAKES  100 // should make this configurable at some point
#define MAX_MAG_CHANS 1000  // this SHOULD be enough
#define MAX_DAMS  600 // number of facilities allowed in csv file, should be made config later

HypoArcSM2       *arc_list[MAX_QUAKES];     // arc_list[0..arc_counter-1] point to elements of arc_data
HypoArcSM2       arc_data[MAX_QUAKES];
long             arc_counter = 0;

static time_t next_delivery = -1;           // When to report on arc_list[next_arc]
static int next_arc = -1;

DamInfo dam_tbl[MAX_DAMS];
int dam_count = 0, dam_close_count = 0, dam_sub_close_count = 0;
DamInfo *dam_order[MAX_DAMS];
int damMapLimit=25;

void free_arcsm2( HypoArcSM2 *p ) {
    if ( p->sm_count > 0 ) {
        //_free( p->sm_arr, "sm list" );
    }
    if ( arc_list[arc_counter-1] != p ) {
        *p = arc_data[arc_counter-1];
    }
    //_free( p, "sm2" );
}

char* my_strtok_r( char **brkt_ptr ) {
    if ( **brkt_ptr == ',' ) {
        **brkt_ptr = 0;
        return (*brkt_ptr)++;
    } else
        return strtok_r( NULL, ",", brkt_ptr );
}

int region_compare( const void* a, const void* b ) {
    int ia = *((int*)a), ib = *((int*)b);
    return strcmp( region_abbrs[ia], region_abbrs[ib] );
}

int area_compare( const void* a, const void* b ) {
    int ia = *((int*)a), ib = *((int*)b);
    return strcmp( area_abbrs[ia], area_abbrs[ib] );
}

//reads stream conversion factor file
//initializes ConversionFactor array
//returns 0 if successful, -1 otherwise
int conversion_read( char *path ) {
	FILE *fp = NULL;
	int lNum = 0;
	size_t sLen = 0;
	char factorStr[20];
	char lBuff[80];
	char *s1 = NULL;
	char *s2 = NULL;

	if (!(fp = fopen( path, "r" ))) {
		logit("et", "Error opening stream conversion factor file %s\n", path);	
		return -1;
	}

	for ( lNum = 0; (lNum < MAX_STREAMS) && (s1 = fgets(lBuff, 80, fp)); lNum++) {
		if(!(s2 = strpbrk(s1, " \t"))) {
			//if a line can't be parsed, skip it:
			logit("et", "Error parsing line %s\n", lBuff);
			continue;	
		}
		sLen = s2 - s1;
		strncpy(ConversionFactors[lNum].SNCL, s1, sLen); 
		sLen = strcspn(s2, " \t"); //find num of whitespace chars to skip
		strncpy(factorStr, (char *)&(s2[sLen]), 20);

		ConversionFactors[lNum].convFactor = atof(factorStr);	
	}

	return 0;
}

int read_RAD_info( char *path ) {
    int i, j, k, max_region = 0, max_area = 0;
    FILE *fp;
    char buffer[300], word[20];
    char *line, *brkt;
    int lineno = 0;
    int stage = 0;  // 0=Regions, 1=Areas, 2=Dams

    // Initialize region & area tables
    for ( i=0; i<MAX_REGIONS; i++ ) {
        region_order[i] = i;
        strcpy( region_abbrs[i], "??" );
    }
    for ( i=0; i<MAX_AREAS; i++ ) {
        area_order[i] = i;
        strcpy( area_abbrs[i], "??" );
    }
    fp = fopen( path, "r" );
    if ( fp == NULL ) {
        logit("et", "Error opening RAD file %s\n", path);
        return -1;
    }
    logit("et", "Reading RAD file from %s\n", path);

    lineno++;
    line = fgets(buffer, 290, fp);
    if ( strncmp( line, "#Region", 6 ) ) {
        logit("e","gmewhtmlemail: First line is not Regions header\n");
        fclose(fp);
        return -1;
    }
    lineno++;
    line = fgets(buffer, 290, fp);

	while ( line != NULL ) {
        if ( strncmp( line, "#Area", 5 )==0 ) {
            stage += 1;
            lineno++;
            line = fgets(buffer, 290, fp);
            break;
        }
        i = atoi( strtok_r(line, ",", &brkt) );
        if ( i >= MAX_REGIONS-1 ) {
            logit("e","gmewhtmlemail: Maximum region code exceeded; exiting!\n");
            fclose(fp);
            return -1;
        } else if ( i > max_region )
            max_region = i;
        strcpy( region_abbrs[i], my_strtok_r( &brkt ) );
        strcpy( region_names[i], my_strtok_r( &brkt ) );
        region_names[i][strlen(region_names[i])-1] = ' ';
        strcat( region_names[i], region_abbrs[i] );
        lineno++;
        line = fgets(buffer, 290, fp);
    }
    max_region++;
    region_order[max_region] = 99;
    qsort( region_order+1, max_region-1, sizeof(int), region_compare );

	if ( Debug ) {
        logit("","Regions (%d):\n", max_region);
        for ( i=1; i<max_region; i++ )
            logit( "","   %2d (%-3s) '%s'\n", i, region_abbrs[i], region_names[i] );
    }
    while ( line != NULL ) {
        if ( strncmp( line, "#Dams", 4 )==0 ) {
            stage += 1;
            lineno++;
            line = fgets(buffer, 290, fp);
            break;
        }
        for ( j = 0; line[j] && line[j] != ','; j++ );
        line[j] = 0;
        i = atoi( line );
        if ( max_area >= MAX_AREAS-1 ) {
            logit("e","gmewhtmlemail: Maximum area code exceeded; exiting!\n");
            fclose(fp);
            return -1;
        } else if ( i > max_area )
            max_area = i;
        for ( j++, k=0; line[j] && line[j] != ','; j++, k++ )
            area_abbrs[i][k] = line[j];
        area_abbrs[i][k] = 0;
        strcpy( area_names[i], line+j+1 );
        area_names[i][strlen(area_names[i])-1] = 0;
        lineno++;
        line = fgets(buffer, 290, fp);
    }
    max_area++;
    area_order[max_area] = 99;
    qsort( area_order+1, max_area-1, sizeof(int), area_compare );
    if ( Debug ) {
        logit("","Areas (%d)!:\n", max_area);
        for ( i=1; i<max_area; i++ )
            logit("", "   %2d %-3s '%s'\n", i, area_abbrs[i], area_names[i] );
    }
    while ( line != NULL ) {
        if ( dam_count >= MAX_DAMS ) {
            logit("e","gmewhtmlemail: Maximum area code exceeded; exiting!\n");
            fclose(fp);
            return -1;
        }
        // example: MARTINEZ DAM,3,MP,SCC,38.01064137,-122.1084274,MRTZ
        strcpy( dam_tbl[dam_count].name, strtok_r( line, ",", &brkt ) );
        dam_tbl[dam_count].region_id = atoi( my_strtok_r(&brkt) );
        my_strtok_r( &brkt );   // skip area abbrev
        strcpy( word, my_strtok_r( &brkt ) );
        for ( i=1; i<max_area; i++ )
            if ( strcmp( word, area_abbrs[i] ) == 0 ) {
                dam_tbl[dam_count].area_id = i;
                break;
            }
        dam_tbl[dam_count].lat = atof( my_strtok_r(&brkt) );
        dam_tbl[dam_count].lon = atof( my_strtok_r(&brkt) );
        strcpy( dam_tbl[dam_count].station, my_strtok_r( &brkt ));
        // remove cariage return and newlines from station name string
        i = strlen(dam_tbl[dam_count].station);
        if (dam_tbl[dam_count].station[i-1]=='\n') dam_tbl[dam_count].station[i-1]='\0';
        i = strlen(dam_tbl[dam_count].station);
        if (dam_tbl[dam_count].station[i-1]=='\r') dam_tbl[dam_count].station[i-1]='\0';
        dam_tbl[dam_count].dam_id = dam_count;
        dam_order[dam_count] = dam_tbl+dam_count;
        dam_count++;
        lineno++;
        line = fgets(buffer, 290, fp);
    }

    if ( Debug ) {
        logit("","Dams (%d):\n", dam_count);
        for ( i=0; i<dam_count; i++ )
            logit( "","   %2d %-3s %-3s (%10.4f,%10.4f) '%s' site=%s\n", i,
                   region_abbrs[dam_tbl[i].region_id], area_abbrs[dam_tbl[i].area_id],
                   dam_tbl[i].lon, dam_tbl[i].lat, dam_tbl[i].name, dam_tbl[i].station);
    }
    fclose( fp );
    return 0;
}

int makeRoomForRecipients( int n ) {
    int newSize = n > 0 ? nemailrecipients + n : -n;
    emailrecipients = realloc( emailrecipients, newSize * sizeof( EMAILREC ) );
    if ( emailrecipients != NULL ) {
        nemailrecipients = newSize;
        return 1;
    } else
        return 0;
}

char* assignMailingList() {
    char* str = k_str();
    if ( k_err() ) {
        // There was no modifier; assign to both lists
        emailrecipients[nemailrecipients-1].sendOnEQ = 1;
        emailrecipients[nemailrecipients-1].sendOnSM = 1;
    } else if ( strcmp( str, "EQK" ) == 0 ) {
        emailrecipients[nemailrecipients-1].sendOnEQ = 1;
        emailrecipients[nemailrecipients-1].sendOnSM = 0;
    } else if ( strcmp( str, "SM" ) == 0 ) {
        emailrecipients[nemailrecipients-1].sendOnEQ = 0;
        emailrecipients[nemailrecipients-1].sendOnSM = 1;
    } else {
        return str;
    }
    return NULL;
}

void updateSubscriptions() {
    FILE *fp;
    char line[200], *line_pos, code[10];
    int numSubs, i, ncodes, n, j;
    double newTimeStamp;
    
    if ( SubscriptionPath[0] == 0 )
        return;
    fp = fopen( SubscriptionPath, "r" );
    if ( fp == NULL )
        return;
    if ( fgets( line, 190, fp ) == NULL ) {
        fclose( fp );
        return;
    }    
    newTimeStamp = atof(line);    
    if ( newTimeStamp <= SubscriptionTimestamp ) {
        fclose( fp );
        return;
    }
    if ( fgets( line, 190, fp ) == NULL ) {
        fclose( fp );
        return;
    }        
    numSubs = atoi(line);
    if ( numSubs <= 0 ) {
        fclose( fp );
        return;
    }
    if ( makeRoomForRecipients( -(nStaticEmailRecipents + numSubs) ) == 0 ) {
        fclose( fp );
        return;
    }
    for ( ; numSubs > 0; numSubs-- ) {
        if ( fgets( line, MAX_EMAIL_CHAR-1, fp ) == NULL ) {
            nemailrecipients -= numSubs;
            break;
        }
        emailrecipients[nemailrecipients-numSubs].subscription = 1;
        line[strlen(line)-1] = 0;
        strcpy( emailrecipients[nemailrecipients-numSubs].address, line );
        if ( fgets( line, 190, fp ) == NULL ) {
            nemailrecipients -= numSubs;
            break;
        } 
        emailrecipients[nemailrecipients-numSubs].min_magnitude = atof( line );
        if ( fgets( line, 190, fp ) == NULL ) {
            nemailrecipients -= numSubs;
            break;
        } 
        line_pos = line;
        i = 0;
        ncodes = 0;
        emailrecipients[nemailrecipients-numSubs].subRegions = 0;
        while ( 1 ) {
            if ( isalnum( *line_pos ) )
                code[i++] = *(line_pos++);
            else { 
                code[i++] = 0;
                for ( i=0; region_abbrs[i][0]!=0 && strcmp(code,region_abbrs[i]); i++ );
                if ( region_abbrs[i][0] ) {
                    emailrecipients[nemailrecipients-numSubs].subRegions |= (1<<i);
                }
                if ( *line_pos == ',' ) { 
                    i = 0;
                    line_pos++;
                } else
                    break;
            }
        }
        emailrecipients[nemailrecipients-numSubs].numRegions = ncodes;
        if ( fgets( line, 190, fp ) == NULL ) {
            nemailrecipients -= numSubs;
            break;
        } 
        line_pos = line;
        i = 0;
        ncodes = 0;
        emailrecipients[nemailrecipients-numSubs].subAreas = 0;
        while ( 1 ) {
            if ( isalnum( *line_pos ) )
                code[i++] = *(line_pos++);
            else {
                code[i++] = 0;
                for ( i=0; area_abbrs[i][0]!=0 && strcmp(code,area_abbrs[i]); i++ );
                if ( area_abbrs[i][0] ) {
                    emailrecipients[nemailrecipients-numSubs].subAreas |= (UINT64_C(1)<<i);
                }
                if ( *line_pos == ',' ) { 
                    i = 0;
                    line_pos++;
                } else
                    break;
            }
        }
        emailrecipients[nemailrecipients-numSubs].numAreas = ncodes;        
        if ( fgets( line, 190, fp ) == NULL ) {
            nemailrecipients -= numSubs;
            break;
        } 
        n = atoi(line);
        emailrecipients[nemailrecipients-numSubs].numDams = 0;
        for ( i=0; i < n; i++ ) {
            if ( fgets( line, 190, fp ) == NULL ) {
                nemailrecipients -= numSubs;
                fclose( fp );
                break;
            } 
            for ( j=0; strcmp(line,dam_tbl[i].name) && j<dam_count; j++ );
            if ( j==dam_count ) {
                logit("et","Unknown Dam for %s: '%s'\n", 
                    emailrecipients[nemailrecipients-numSubs].address, line );
            } else if ( emailrecipients[nemailrecipients-numSubs].numDams == MAX_SUB_DAMS ) {
                logit("et","Too many dams for %s; ignoring '%s'\n", 
                    emailrecipients[nemailrecipients-numSubs].address, line );
            } else {
                emailrecipients[nemailrecipients-numSubs].subDams[ emailrecipients[nemailrecipients-numSubs].numDams++ ] = i;
            }
        }
    }    
    fclose( fp );
}

static char* monthNames[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

/* Main program starts here
 **************************/
int main(int argc, char **argv) {
    time_t        timeNow;          // current time
    time_t        timeLastBeat;     // time last heartbeat was sent
    char         *msgbuf;           // buffer for msgs from ring
    long          recsize;          // size of retrieved message
    MSG_LOGO      reclogo;          // logo of retrieved message
    unsigned char seq;
    int           i, res;
    HypoArcSM2    *arc;             // a hypoinverse ARC message

    /* Check command line arguments
    ******************************/
    if (argc != 2)
    {
        fprintf(stderr, "Usage: gmewhtmlemail <configfile>\n");
        fprintf(stderr, "Version: %s\n", VERSION_STR );
        return EW_FAILURE;
    }

    /* Initialize name of log-file & open it
    ***************************************/
    logit_init(argv[1], 0, 256, 1);


    /* Read the configuration file(s)
    ********************************/
    config(argv[1]);


    /* Lookup important information from earthworm.d
    ***********************************************/
    lookup();


    /* Set logit to LogSwitch read from configfile
    *********************************************/
    logit_init(argv[1], 0, 256, LogSwitch);
    logit("", "gmewhtmlemail: Read command file <%s>\n", argv[1]);

    logit("t", "gmewhtmlemail: version %s\n", VERSION_STR);

    /* Get our own process ID for restart purposes
    *********************************************/
    if( (MyPid = getpid()) == -1 )
    {
        logit ("e", "gmewhtmlemail: Call to getpid failed. Exiting.\n");
        free( GetLogo );
        exit( -1 );
    }

    /* zero out the arc_list messages -
    takes care of condition where Mag may
    come in but no ARC (eqfiltered out)
    ***********************************/
    for (i=0; i< MAX_QUAKES; i++) arc_list[i] = NULL;

    /* Allocate the message input buffer
    ***********************************/
    if ( !( msgbuf = (char *) calloc( 1, (size_t)MaxMessageSize+10 ) ) )
    {
        logit( "et",
               "gmewhtmlemail: failed to allocate %ld bytes"
               " for message buffer; exiting!\n", MaxMessageSize+10 );
        free( GetLogo );
        exit( -1 );
    }

    /* Initialize DB for quake ID translation
     *****************************************/
    if ( (db_path[0]==0) || (SQLITE_OK != (res = sqlite3_initialize())) )
    {
        if ( db_path[0] )
            logit("et","Failed to initialize sqlite3 library: %d\n", res);
        else
            logit("t","sqlite3 library not specified\n");
        dbAvailable = 0;
    } else {
        dbAvailable = 1;
    }

    /* Attach to shared memory rings
    *******************************/
    tport_attach( &InRegion, InRingKey );
    logit( "", "gmewhtmlemail: Attached to public memory region: %ld\n",
           InRingKey );


    /* Force a heartbeat to be issued in first pass thru main loop
    *************************************************************/
    timeLastBeat = time(&timeNow) - HeartbeatInt - 1;


    /* Flush the incoming transport ring on startup
    **********************************************/
    while( tport_copyfrom(&InRegion, GetLogo, nLogo,  &reclogo,
                          &recsize, msgbuf, MaxMessageSize, &seq ) != GET_NONE )
        ;


    /*-------------------- setup done; start main loop ------------------------*/


    while ( tport_getflag( &InRegion ) != TERMINATE  &&
            tport_getflag( &InRegion ) != MyPid )
    {
        /* send heartbeat
        ***************************/
        if( HeartbeatInt  &&  time(&timeNow)-timeLastBeat >= HeartbeatInt )
        {
            timeLastBeat = timeNow;
            status( TypeHeartBeat, 0, "" );
        }

        if ( next_delivery != -1 && timeNow >= next_delivery ) {
            if ( Debug )
                logit("","Process %s %ld\n", arc_list[next_arc]->fromArc ? "arc" : "trigger", arc_list[next_arc]->ha.sum.qid);
            process_message( next_arc );
            free_arcsm2( arc_list[next_arc] );
            if ( arc_counter == 1 ) {
                next_delivery = -1;
                next_arc = -1;
                arc_list[0] = NULL;
            } else {
                if ( next_arc != arc_counter-1 )
                    arc_list[next_arc] = arc_list[arc_counter-1];
                arc_list[arc_counter-1] = NULL;
                next_delivery = arc_list[0]->deliveryTime;
                next_arc = 0;
                for ( i=1; i<arc_counter-1; i++ )
                    if ( arc_list[i]->deliveryTime < next_delivery ) {
                        next_delivery = arc_list[i]->deliveryTime;
                        next_arc = i;
                    }
            }
            arc_counter--;
        }

        /* Get msg & check the return code from transport
        ************************************************/
        res = tport_copyfrom( &InRegion, GetLogo, nLogo, &reclogo,
                              &recsize, msgbuf, MaxMessageSize, &seq );
        switch( res )
        {
        case GET_OK:      /* got a message, no errors or warnings         */
            break;

        case GET_NONE:    /* no messages of interest, check again later   */
            sleep_ew(1000); /* milliseconds */
            continue;

        case GET_NOTRACK: /* got a msg, but can't tell if any were missed */
            sprintf( Text,
                     "Msg received (i%u m%u t%u); transport.h NTRACK_GET exceeded",
                     reclogo.instid, reclogo.mod, reclogo.type );
            status( TypeError, ERR_NOTRACK, Text );
            break;

        case GET_MISS_LAPPED:     /* got a msg, but also missed lots      */
            sprintf( Text,
                     "Missed msg(s) from logo (i%u m%u t%u)",
                     reclogo.instid, reclogo.mod, reclogo.type );
            status( TypeError, ERR_MISSLAP, Text );
            break;

        case GET_MISS_SEQGAP:     /* got a msg, but seq gap               */
            sprintf( Text,
                     "Saw sequence# gap for logo (i%u m%u t%u s%u)",
                     reclogo.instid, reclogo.mod, reclogo.type, seq );
            status( TypeError, ERR_MISSGAP, Text );
            break;

        case GET_TOOBIG:  /* next message was too big, resize buffer      */
            sprintf( Text,
                     "Retrieved msg[%ld] (i%u m%u t%u) too big for msgbuf[%ld]",
                     recsize, reclogo.instid, reclogo.mod, reclogo.type,
                     MaxMessageSize );
            status( TypeError, ERR_TOOBIG, Text );
            continue;

        default:         /* Unknown result                                */
            sprintf( Text, "Unknown tport_copyfrom result:%d", res );
            status( TypeError, ERR_TOOBIG, Text );
            continue;
        }

        /* Received a message. Start processing
        **************************************/
        msgbuf[recsize] = '\0'; /* Null terminate for ease of printing */

        if (Debug) {
            logit("t", "gmewhtmlemail: Received message, reclogo.type=%d\n",
                  (int)(reclogo.type));
        }

        if ( reclogo.type == TypeHYP2000ARC )
        {
            time_t delivery_time;
            if ( arc_counter == MAX_QUAKES-1 ) {
                logit( "et", "Maximum number of events (%d) exceeded; ignoring latest\n", MAX_QUAKES );
                continue;
            }
            arc = arc_data+arc_counter;
            if ( Debug )
                logit("t","Got a ARC msg: %s\n", msgbuf);
            if ( arc_counter == MAX_QUAKES-1 ) {
                logit( "et", "Maximum number of events (%d) exceeded; ignoring latest\n", MAX_QUAKES );
            } else {
                parse_arc_no_shdw(msgbuf, &(arc->ha));
                time(&delivery_time);
                if ( (MaxArcAge > 0) && (delivery_time - (arc->ha.sum.ot-GSEC1970) > MaxArcAge) ) {
                    logit("t","Ignoring ARC %ld; too old\n", arc->ha.sum.qid );
                    continue;
                }
                delivery_time += EmailDelay;
                for ( i=0; i<arc_counter; i++ )
                    if ( arc_list[i]->ha.sum.qid == arc->ha.sum.qid ) {
                        if ( Debug )
                            logit("","----Received duplicate ARC for %ld before original processed\n", arc->ha.sum.qid );
                        arc_list[i]->deliveryTime = delivery_time;
                        arc_list[i]->ha = arc->ha;
                        break;
                    }
                if ( i==arc_counter )  {
                    arc->deliveryTime = delivery_time;
                    arc->sm_count = 0;
                    arc_list[arc_counter] = arc;
                    arc_counter++;
                    if ( Debug )
                        logit("","Added arc #%ld lat=%lf lon=%lf depth=%lf %d phases\n",
                              arc->ha.sum.qid, arc->ha.sum.lat, arc->ha.sum.lon, arc->ha.sum.z, arc->ha.sum.nph);
                    if ( arc_counter == 1 ) {
                        next_delivery = delivery_time;
                        next_arc = 0;
                    }
                    arc->fromArc = 1;
                } else {
                    if ( arc_counter > 1 ) {
                        next_arc = 0;
                        next_delivery = arc_list[0]->deliveryTime;
                        for ( i=1; i<arc_counter; i++ )
                            if ( arc_list[i]->deliveryTime < next_delivery ) {
                                next_delivery = arc_list[i]->deliveryTime;
                                next_arc = i;
                            }
                    }
                    if ( Debug && next_delivery != delivery_time )
                        logit( "", "----New head of queue: %ld\n", arc_list[next_arc]->ha.sum.qid );
                }
            }
        } else if ( reclogo.type == TypeTHRESH )
        {
            if ( Debug )
                logit("t","Got a THRESH message: %s\n", msgbuf );
            if ( arc_counter == MAX_QUAKES-1 ) {
                logit( "et", "Maximum number of events (%d) exceeded; ignoring latest\n", MAX_QUAKES );
            } else {
                arc = arc_data+arc_counter; //_calloc(1, sizeof(HypoArcSM2), "sm2 for thresh");
                if ( arc == NULL ) {
                    logit( "et", "Failed to allocated a HypoArcSM2\n" );
                } else {
                    int args, year, pos;
                    struct tm when_tm;
                    //time_t ot;
                    char month_name[20];
                    char sncl[4][MAX_SCNL_STRING];
                    int j, k;

                    //parse_arc_no_shdw(msgbuf, &(arc->ha));
                    arc->ha.sum.qid = 0;
                    args = sscanf( msgbuf, "SNCL=%s Thresh=%*f Value=%*f Time=%*s %s %d %d:%d:%d %d", 
                                    arc->sncl, month_name, &when_tm.tm_mday, &when_tm.tm_hour, &when_tm.tm_min, 
                                    &when_tm.tm_sec, &year );
                    if ( args != 7 ) {
                        logit("et", "Could not parse THRESH message (%s); ignoring\n", msgbuf );
                        continue;
                    }
                    for ( i=0; i<12; i++ )
                        if ( strcmp( monthNames[i], month_name ) == 0 ) {
                            when_tm.tm_mon = i;
                            break;
                        }
                    when_tm.tm_year = year - 1900;

                    //arc->ha.sum.ot = timegm_ew( &when_tm );
                    arc->ha.sum.ot = timegm( &when_tm );
                    arc->ha.sum.qid = -arc->ha.sum.ot;
                    arc->ha.sum.ot += GSEC1970;

                    j = 0;
					k = 0;
                    for ( i=0; arc->sncl[i]; i++ )
                        if ( arc->sncl[i] == '.'  ) {
                            sncl[j][k] = 0;
                            j++;
                            k = 0;
                        } else
                            sncl[j][k++] = arc->sncl[i];
                    sncl[j][k] = 0;
                    pos = site_index( sncl[0], sncl[1], sncl[2], sncl[3][0] ? sncl[3] : "--") ;
                    if ( pos != -1 ) {
                        arc->ha.sum.lat = Site[pos].lat;
                        arc->ha.sum.lon = Site[pos].lon;
                    } else {
                        arc->ha.sum.lat = arc->ha.sum.lon = -1;
                    }
                    arc->sm_count = 0;
                    time(&(arc->deliveryTime));
                    arc->deliveryTime += +EmailDelay;
                    arc_list[arc_counter] = arc;
                    arc_counter++;
                    if ( Debug )
                        logit("","Added thresh #%ld %s (%lf)\n", arc->ha.sum.qid, arc->sncl, arc->ha.sum.ot);
                    if ( arc_counter == 1 ) {
                        next_delivery = arc->deliveryTime;
                        next_arc = 0;
                    }
                    arc->fromArc = 0;
                }
            }
        } else if ( reclogo.type == TypeSTRONGMOTIONII )
        {
            char *ptr = msgbuf, gotOne = 0;

            while ( 1 ) {
                SM_INFO sm;
                long n_qid;
                char sncl[MAX_SCNL_STRING];
                int rv = rd_strongmotionII( &ptr, &sm, 1 );
                if ( rv != 1 ) {
                    if ( !gotOne )
                        logit( "et", "Could not parse SM message; ignoring\n" );
                    break;
                }
                gotOne = 1;
                n_qid = atol( sm.qid );
                sprintf( sncl, "%s.%s.%s.%s", sm.sta, sm.net, sm.comp, sm.loc );
                for ( i=0; i<arc_counter; i++ )
                    if ( arc_list[i]->ha.sum.qid == n_qid )
                        break;
                if ( (i < arc_counter) && !arc_list[i]->fromArc ) {
                    if ( strcmp( sncl, arc_list[i]->sncl ) ) {
                        if ( Debug )
                            logit( "", "Ignoring SM from non-triggering station: %s\n", sncl);
                        continue;
                    }
                }
                if ( i < arc_counter ) {
                    int n = arc_list[i]->sm_count;
                    if ( n>=MAX_SM_PER_EVENT ) {
                        logit( "et", "Failed to expand sm_arr\n" );
                    } else {
                        int j;
                        for ( j=0; j<n; j++ )
                            if ( !(strcmp( arc_list[i]->sm_arr[j].sta, sm.sta ) ||
                                    strcmp( arc_list[i]->sm_arr[j].net, sm.net ) ||
                                    strcmp( arc_list[i]->sm_arr[j].comp, sm.comp ) ||
                                    strcmp( arc_list[i]->sm_arr[j].loc, sm.loc )) )
                                break;
						if((!RestrictThresholds) || (RestrictThresholds && (strcmp(RestrictedNetwork, sm.net) == 0))) {
                        	arc_list[i]->sm_arr[j] = sm;
                        	if ( j==n ) {
                            	arc_list[i]->sm_count++;
                            	if (Debug)
                                	logit("t","Added sm2 %s to %s (#%d) pga=%lf pgv=%lf pgd=%lf %d rsa\n",
                                    	sncl, sm.qid, n+1, sm.pga, sm.pgv, sm.pgv, sm.nrsa);
                        	}
                        	else if ( Debug )
                            	logit("t","Replaced sm2 %s for %s (#%d) pga=%lf pgv=%lf pgd=%lf %d rsa\n",
                                	sncl, sm.qid, n+1, sm.pga, sm.pgv, sm.pgv, sm.nrsa);
						}
                    }
                } else {
                    logit( "", "sm2 %ld w/ no matching trigger:", n_qid );
                    for ( i=0; i<arc_counter; i++ )
                        logit( "", "%ld ", arc_list[i]->ha.sum.qid );
                    logit( "", "\n" );
                }
            }
        } else {
            logit("","Unexpected message type: %d\n", reclogo.type );
        }

    }

    /* free allocated memory */
    free( GetLogo );
    free( msgbuf );
    free( Site );    // allocated by calling read_site()

    /* detach from shared memory */
    tport_detach( &InRegion );

    /* Shut down sqlite3, if in use */
    if ( dbAvailable )
        sqlite3_shutdown();

    /* write a termination msg to log file */
    logit( "t", "gmewhtmlemail: Termination requested; exiting!\n" );
    fflush( stdout );

    return( 0 );
}

/*****************************************************************************
 *  config() processes command file(s) using kom.c functions;                *
 *                    exits if any errors are encountered.                   *
 *****************************************************************************/
#define ncommand 11        /* # of required commands you expect to process */
void config(char *configfile) {
	char init[ncommand]; /* init flags, one byte for each required command */
    int nmiss; /* number of required commands that were missed   */
    char *com;
    char *str;
    int nfiles;
    int success;
    char processor[15];
    int i;
    char cp[80];
    char path[200];
    int cfgNumLogo;

    BlatOptions[0]=0;
    KMLdir[0]=0;
    strcpy(SubjectPrefix, DEFAULT_SUBJECT_PREFIX);
    strcpy(StaticMapType, DEFAULT_GOOGLE_STATIC_MAPTYPE);
    strcpy(MQStaticMapType, DEFAULT_MAPQUEST_STATIC_MAPTYPE);

    /* Set to zero one init flag for each required command
    *****************************************************/
    for (i = 0; i < ncommand; i++)
        init[i] = 0;
    nLogo = 0;

    /* Open the main configuration file
    **********************************/
    nfiles = k_open(configfile);
    if (nfiles == 0)
    {
        logit("e",
              "gmewhtmlemail: Error opening command file <%s>; exiting!\n",
              configfile);
        exit(-1);
    }

    /* Process all command files
    ***************************/
	while (nfiles > 0) /* While there are command files open */
    {
        while (k_rd()) /* Read next line from active file  */
        {
            com = k_str(); /* Get the first token from line */

            /* Ignore blank lines & comments
             *******************************/
            if (!com) continue;
            if (com[0] == '#') continue;

            /* Open a nested configuration file
             **********************************/
            if (com[0] == '@') {
                success = nfiles + 1;
                nfiles = k_open(&com[1]);
                if (nfiles != success)
                {
                    logit("e",
                          "gmewhtmlemail: Error opening command file <%s>; exiting!\n",
                          &com[1]);
                    exit(-1);
                }
                continue;
            }

            /* Process anything else as a command
             ************************************/
            /*0*/ if (k_its("LogFile"))
            {
                LogSwitch = k_int();
                if (LogSwitch < 0 || LogSwitch > 2)
                {
                    logit("e",
                          "gmewhtmlemail: Invalid <LogFile> value %d; "
                          "must = 0, 1 or 2; exiting!\n", LogSwitch);
                    exit(-1);
                }
                init[0] = 1;
            }
            /*1*/ else if (k_its("MyModuleId"))
            {
                if ((str = k_str()) != NULL) {
                    if (GetModId(str, &MyModId) != 0)
                    {
                        logit("e",
                              "gmewhtmlemail: Invalid module name <%s> "
                              "in <MyModuleId> command; exiting!\n", str);
                        exit(-1);
                    }
                }
                init[1] = 1;
            }
            /*2*/ else if (k_its("InRing"))
            {
                if ( (str = k_str()) != NULL)
                {
                    if ((InRingKey = GetKey(str)) == -1)
                    {
                        logit("e",
                              "gmewhtmlemail: Invalid ring name <%s> "
                              "in <InRing> command; exiting!\n", str);
                        exit(-1);
                    }
                }
                init[2] = 1;
            }
            /*3*/ else if (k_its("HeartbeatInt"))
            {
                HeartbeatInt = k_long();
                init[3] = 1;
            }
            /*4*/ else if (k_its("GetLogo"))
            {
                if ((str = k_str()) != NULL)
                {
                    MSG_LOGO *tlogo = NULL;
                    cfgNumLogo = nLogo + 10;
                    tlogo = (MSG_LOGO *)
                            realloc(GetLogo, cfgNumLogo * sizeof (MSG_LOGO) );
                    if (tlogo == NULL)
                    {
                        logit("e", "gmewhtmlemail: GetLogo: error reallocing"
                              " %zu bytes; exiting!\n",
                              cfgNumLogo * sizeof (MSG_LOGO));
                        exit(-1);
                    }

                    GetLogo = tlogo;

                    if (GetInst(str, &GetLogo[nLogo].instid) != 0)
                    {
                        logit("e",
                              "gmewhtmlemail: Invalid installation name <%s>"
                              " in <GetLogo> cmd; exiting!\n", str);
                        exit(-1);
                    }
                    GetLogo[nLogo + 3].instid = GetLogo[nLogo + 2].instid = GetLogo[nLogo + 1].instid = GetLogo[nLogo].instid;
                    if ((str = k_str()) != NULL)
                    {
                        if (GetModId(str, &GetLogo[nLogo].mod) != 0)
                        {
                            logit("e",
                                  "gmewhtmlemail: Invalid module name <%s>"
                                  " in <GetLogo> cmd; exiting!\n", str);
                            exit(-1);
                        }
                        GetLogo[nLogo + 3].mod = GetLogo[nLogo + 2].mod = GetLogo[nLogo + 1].mod = GetLogo[nLogo].mod;
                        if (GetType("TYPE_HYP2000ARC", &GetLogo[nLogo++].type) != 0)
                        {
                            logit("e",
                                  "gmewhtmlemail: Invalid message type <TYPE_HYP2000ARC>"
                                  "; exiting!\n");
                            exit(-1);
                        }
                        if (GetType("TYPE_STRONGMOTIONII", &GetLogo[nLogo++].type) != 0)
                        {
                            logit("e",
                                  "gmewhtmlemail: Invalid message type <TYPE_HYP2000ARC>"
                                  "; exiting!\n");
                            exit(-1);
                        }
                        if (GetType("TYPE_THRESH_ALERT", &GetLogo[nLogo++].type) != 0)
                        {
                            logit("e",
                                  "gmewhtmlemail: Invalid message type <TYPE_THRESH_ALERT>"
                                  "; exiting!\n");
                            exit(-1);
                        }
                    }
                    else
                    {
                        logit("e", "gmewhtmlemail: No module name "
                              "in <GetLogo> cmd; exiting\n");
                        exit(-1);
                    }
                }
                else
                {
                    logit("e", "gmewhtmlemail: No installation name "
                          "in <GetLogo> cmd; exiting\n");
                    exit(-1);
                }
                init[4] = 1;
            }
            /*5*/ else if (k_its("Debug"))
            {
                Debug = k_int();
                init[5] = 1;
            }
            else if (k_its("DebugEmail")) DebugEmail = k_int();
            else if ( k_its("MaxMessageSize") )
            {
                MaxMessageSize = k_long();
            }
            else if ( k_its("MAX_SAMPLES") )
            {
                MAX_SAMPLES = k_int();
            }
            else if ( k_its("MinQuality") )
            {
                str = k_str();
                MinQuality = str[0];
            }
            else if ( k_its("WSTimeout") )
            {
                wstimeout = k_int() * 1000;
            }
            /*6*/ else if ( k_its("HTMLFile") )
            {
                strncpy(HTMLFile, k_str(), MAX_STRING_SIZE);
                init[6] = 1;
            }
            else if ( k_its("EmailProgram") )
            {
                strncpy(EmailProgram, k_str(), MAX_STRING_SIZE);
            }
            else if ( k_its("KML") )
            {
                strncpy(KMLdir, k_str(), MAX_STRING_SIZE);
                strncpy(KMLpreamble, k_str(), MAX_STRING_SIZE);
            }
            else if ( k_its("StyleFile") )
            {
                /* RSL: Removed styles */
                //strcpy(StyleFile, k_str());
            }
            else if ( k_its("SubjectPrefix") )
            {
                strcpy(SubjectPrefix, k_str());
            }
            else if ( k_its("NoWaveformPlots") )
            {
				NoWaveformPlots = 1;
            }
            else if ( k_its("TimeMargin") )
            {
                TimeMargin = k_val();
            }
            else if ( k_its("WaveServer") )
            {
                if (nwaveservers < MAX_WAVE_SERVERS)
                {
                    strcpy(cp,k_str());
                    strcpy(waveservers[nwaveservers].wsIP, strtok (cp, ":"));
                    strcpy(waveservers[nwaveservers].port, strtok (NULL, ":"));
                    nwaveservers++;
                }
                else
                {
                    logit("e", "gmewhtmlemail: Excessive number of waveservers. Exiting.\n");
                    exit(-1);
                }
            }
            else if ( k_its("UseRegionName") )
            {
                UseRegionName = 1;
            }
            else if ( k_its("UseBlat") )
            {
                UseBlat = 1;
            }
            else if ( k_its("BlatOptions") )
            {
                strcpy(BlatOptions, k_str());
            }
            else if ( k_its("StaticMapType") )
            {
                strcpy(StaticMapType, k_str());
                if ( strcmp(StaticMapType,"hybrid")==0)
                    strcpy(MQStaticMapType,"hyb");
                else if (strcmp(StaticMapType,"terrain")==0||strcmp(StaticMapType,"roadmap")==0)
                    strcpy(MQStaticMapType,"map");
                else if (strcmp(StaticMapType,"satellite")==0)
                    strcpy(MQStaticMapType,"sat");
            }
            else if ( k_its("DontShowMd") )
            {
                DontShowMd = 1;
            }
            else if ( k_its("DontUseMd") )
            {
                DontUseMd = 1;
            }
            else if ( k_its("UseUTC") )
            {
                UseUTC = 1;
            }
            else if ( k_its("MaxDuration") )
            {
                DurationMax = k_val();
            }
            else if ( k_its("EmailDelay") )
            {
                EmailDelay = k_val();
            }
            else if ( k_its("CenterPoint") )
            {
                center_lat = k_val();
                center_lon = k_val();
            }
            else if ( k_its("DataCenter") )
            {
                strcpy(DataCenter, strtok(k_str(),"\""));
            }
            /*   else if (k_its("SPfilter"))
                     {
                        SPfilter = 1;
                     } */
            else if (k_its("GMTmap"))
            {
                GMTmap = 1;
            }
            else if (k_its("Cities"))
            {
                strncpy(Cities, k_str(), MAX_STRING_SIZE);
            }
            else if (k_its("StationNames"))
            {
                StationNames = 1;
            }
            else if (k_its("MapLegend"))
            {
                strcpy(MapLegend, k_str());
            }
            else if ( k_its("Mercator") )
            {
                Mercator = 1;
            }
            else if ( k_its("Albers") )
            {
                Albers=1;
                Mercator=0;
            }
            else if ( k_its("ShowDetail") )
            {
                ShowDetail = 1;
            }
            else if ( k_its("ShortHeader") )
            {
                ShortHeader = 1;
            }
			else if ( k_its("EwaveAddr") )
			{
				if ((str = k_str()) != NULL) {
					strncpy(ewaveAddr, str, 64);
					UseEWAVE = 1;
				}
			}
			else if ( k_its("EwavePort") )
			{
				if ((str = k_str()) != NULL) {
					strncpy(ewavePort, str, 16);
				}
			}
			else if ( k_its("OnlyAllowThresholdsFromNetwork") )
			{
				if ((str = k_str()) != NULL) {
					strncpy(RestrictedNetwork, str, 8);
					RestrictThresholds = 1;
				}
			}
            else if ( k_its("UseGIF") )
            {
                UseGIF = 1;
            }
            else if ( k_its("TraceWidth") )
            {
                TraceWidth = (int) k_int();
            }
            else if ( k_its("TraceWidth") )
            {
                TraceWidth = (int) k_int();
            }
            /*7*/ else if ( k_its("site_file") )
            {
                strcpy(path, k_str());
                site_read ( path );
                init[7] = 1;
            }
            /*8*/ else if ( k_its("dam_file") )
            {
                logit("e","gmewhtmlemail: dam_file has been deprecated; use RAD_file.\n" );
            }
            /*8*/ else if ( k_its("full_dam_file") )
            {
                logit("e","gmewhtmlemail: full_dam_file has been deprecated; use RAD_file.\n" );
            }
            /*8*/ else if ( k_its("RAD_file") )
            {
                strcpy(path, k_str());
                read_RAD_info ( path );
                init[8] = 1;
            }
            /*9*/ else if ( k_its("db_path") )
            {
                strncpy(db_path, k_str(), MAX_STRING_SIZE);
                init[9] = 1;
            }
			else if ( k_its("conversions_file") )
			{
				strcpy(path, k_str());
				if(conversion_read ( path ) == -1) {
					logit("et", "gmewhtmlemail: failed to process conversion factor file\n");
				}
			}
            else if ( k_its("MaxStationDist") )
            {
                MaxStationDist = (int) k_int();
            }
            else if ( k_its("MaxFacilityDist") )
            {
                MaxFacilityDist = (int) k_int();
            }
            else if ( k_its("IgnoreArcOlderThanSeconds") )
            {
                MaxArcAge = (int) k_int();
            }
            else if ( k_its("EmailRecipientWithMinMagAndDist") )
            {
                if ( makeRoomForRecipients(1) )
                {
                    emailrecipients[nemailrecipients-1].subscription = 0;
                    strcpy(emailrecipients[nemailrecipients-1].address, k_str());
                    emailrecipients[nemailrecipients-1].min_magnitude = k_val();
                    emailrecipients[nemailrecipients-1].max_distance = k_val();
                    str = assignMailingList();
                    if ( str )  {
                        logit("e","gmewhtmlemail: Illegal EmailRecipientWithMinMagAndDist mailing list modifier: '%s'\n", str );
                        exit(-1);
                    }
                    nStaticEmailRecipents++;
                }
                else
                {
                    logit("e", "gmewhtmlemail: Failed to allocate space for EmailRecipientWithMinMagAndDist; exiting.\n");
                    exit(-1);
                }
            }
            else if ( k_its("EmailRecipientWithMinMag") )
            {
                if ( makeRoomForRecipients(1) )
                {
                    emailrecipients[nemailrecipients-1].subscription = 0;
                    strcpy(emailrecipients[nemailrecipients-1].address, k_str());
                    emailrecipients[nemailrecipients-1].min_magnitude = k_val();
                    emailrecipients[nemailrecipients-1].max_distance  = OTHER_WORLD_DISTANCE_KM;
                    str = assignMailingList();
                    if ( str )  {
                        logit("e","gmewhtmlemail: Illegal EmailRecipientWithMinMag mailing list modifier: '%s'\n", str );
                        exit(-1);
                    }
                    nStaticEmailRecipents++;
                }
                else
                {
                    logit("e", "gmewhtmlemail: Failed to allocate space for EmailRecipientWithMinMag; exiting.\n");
                    exit(-1);
                }
            }
            else if ( k_its("EmailRecipient") )
            {
                if ( makeRoomForRecipients(1) )
                {
                    emailrecipients[nemailrecipients-1].subscription = 0;
                    strcpy(emailrecipients[nemailrecipients-1].address, k_str());
                    emailrecipients[nemailrecipients-1].min_magnitude = DUMMY_MAG;
                    emailrecipients[nemailrecipients-1].max_distance  = OTHER_WORLD_DISTANCE_KM;
                    str = assignMailingList();
                    if ( str )  {
                        logit("e","gmewhtmlemail: Illegal EmailRecipient mailing list modifier: '%s'\n", str );
                        exit(-1);
                    }
                    nStaticEmailRecipents++;
                }
                else
                {
                    logit("e", "gmewhtmlemail: Failed to allocate space for EmailRecipient; exiting.\n");
                    exit(-1);
                }
            }
            else if ( k_its("ShowMiles") )
            {
                ShowMiles = 1;
            }
            else if ( k_its("PlotAllSCNLs") )
            {
                PlotAllSCNLs = 1;
            }
            else if ( k_its("EarthquakeDisclaimer") )
            {
                strcpy(path, k_str());
                EarthquakeDisclaimer = read_disclaimer ( path );
            }
            else if ( k_its("TriggerDisclaimer") )
            {
                strcpy(path, k_str());
                TriggerDisclaimer = read_disclaimer ( path );
            }
            else if ( k_its("MapQuestKey") )
            {
                strcpy(MapQuestKey, k_str());
            }
            else if ( k_its("ShowRegionAndArea") )
            {
                ShowRegionAndArea = 1;
                facility_font_size = 1;
            }
            else if ( k_its("IgnoreOlderThanHours") )
            {
                IgnoreHours = k_val();
            }
            else if ( k_its("PathToPython") )
            {
                strncpy(PathToPython, k_str(), MAX_STRING_SIZE);
            }
            else if ( k_its("PathToLocaltime") )
            {
                strncpy(PathToLocaltime, k_str(), MAX_STRING_SIZE);
            }
            /* 10 */
            else if ( k_its("PathToEventEmail") )
            {
                strncpy(PathToEventEmail, k_str(), MAX_STRING_SIZE);
                init[10] = 1;
            }
            else if ( k_its("ReportEstimatedPGAs") )
            {
                ReportEstimatedPGAs = 1;
                strcpy( EstimatePGAsPath, k_str() );
            }

            else if ( k_its("SubscriptionPath") )
            {
                strcpy( SubscriptionPath, k_str() );
            }

            else if ( k_its("DontHiliteMax") )
            {
                showMax = 0;
            }
            else if ( k_its("MaxFacilitiesOnMap") )
            {
                damMapLimit = k_int();
            }
            else if ( k_its("MinPGA") )
            {
                PGAThreshold = k_val();
            }
            else if ( k_its("IgnorePGABelow") )
            {
                IgnorePGABelow = k_val();
            }
            else if ( k_its("MaxFacilitiesInTable") )
            {
                MaxFacilitiesInTable = k_int();
            }
            else if ( k_its("ColorPGA") )
            {
                if (ncolorpga < MAX_COLOR_PGAS)
                {
                    colorpga_values[ncolorpga] = k_val();
                    // if other color PGA values and this value is not less than previous value
                    if (ncolorpga > 0 && colorpga_values[ncolorpga] >= colorpga_values[ncolorpga - 1])
                    {
                        logit("e", "gmewhtmlemail: Invalid ColorPGA value (%lf). Exiting.\n",
                              colorpga_values[ncolorpga]);
                        exit(-1);
                    }
                    strncpy(colorpga_colors[ncolorpga], k_str(), MAX_STRING_SIZE);
                    ncolorpga++;
                }
                else
                {
                    logit("e", "gmewhtmlemail: Excessive number of ColorPGA values. Exiting.\n");
                    exit(-1);
                }
            }

            

            /* Some commands may be processed by other functions
             ***************************************************/
            else if( site_com() )  strcpy( processor, "site_com" );
            /* Unknown command
             *****************/
            else
            {
                logit("e", "gmewhtmlemail: <%s> Unknown command in <%s>.\n",
                      com, configfile);
                continue;
            }

            /* See if there were any errors processing the command
             *****************************************************/
            if (k_err())
            {
                logit("e",
                      "gmewhtmlemail: Bad <%s> command in <%s>; exiting!\n",
                      com, configfile);
                exit(-1);
            }
        }
        nfiles = k_close();
    }

    /* After all files are closed, check init flags for missed commands
    ******************************************************************/
    nmiss = 0;
    for (i = 0; i < ncommand; i++) if (!init[i]) nmiss++;
    if (nmiss)
    {
        logit("e", "gmewhtmlemail: ERROR, no ");
        if (!init[0]) logit("e", "<LogFile> ");
        if (!init[1]) logit("e", "<MyModuleId> ");
        if (!init[2]) logit("e", "<InRing> ");
        if (!init[3]) logit("e", "<HeartbeatInt> ");
        if (!init[4]) logit("e", "<GetLogo> ");
        if (!init[5]) logit("e", "<Debug> ");
        if (!init[6]) logit("e", "<HTMLFile> ");
        if (!init[7]) logit("e", "<site_file> ");
        if (!init[8]) logit("e", "<RAD_file> ");
        if (!init[9]) logit("e", "<db_path> ");
        if (!init[10]) logit("e", "<PathToEventEmail> ");
        logit("e", "command(s) in <%s>; exiting!\n", configfile);
        exit(-1);
    }

    if ( SubscriptionPath[0] )
        updateSubscriptions();
        
    return;
}

/*********************************************************************
 *  lookup( )   Look up important info from earthworm.h tables       *
 *********************************************************************/
void lookup(void) {
    /* Look up installations of interest
     *********************************/
    if (GetLocalInst(&InstId) != 0) {
        logit("e",
              "gmewhtmlemail: error getting local installation id; exiting!\n");
        exit(-1);
    }

    /* Look up message types of interest
     *********************************/
    if (GetType("TYPE_HEARTBEAT", &TypeHeartBeat) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
        exit(-1);
    }
    if (GetType("TYPE_ERROR", &TypeError) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_ERROR>; exiting!\n");
        exit(-1);
    }
    if (GetType("TYPE_HYP2000ARC", &TypeHYP2000ARC) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_HYP2000ARC>; exiting!\n");
        exit(-1);
    }
    if (GetType("TYPE_TRACEBUF2", &TypeTraceBuf2) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
        exit(-1);
    }
    if (GetType("TYPE_STRONGMOTIONII", &TypeSTRONGMOTIONII) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_STRONGMOTIONII>; exiting!\n");
        exit(-1);
    }
    if (GetType("TYPE_THRESH_ALERT", &TypeTHRESH) != 0) {
        logit("e",
              "gmewhtmlemail: Invalid message type <TYPE_THRESH_ALERT>; exiting!\n");
        exit(-1);
    } else
        logit( "t", "gmewhtmlemail: TYPE_THRESH_ALERT = %d\n", TypeTHRESH );
    return;
}

/******************************************************************************
 * status() builds a heartbeat or error message & puts it into                *
 *                   shared memory.  Writes errors to log file & screen.      *
 ******************************************************************************/
void status(unsigned char type, short ierr, char *note) {
    MSG_LOGO logo;
    char msg[256];
    long size;
    time_t t;

    /* Build the message
     *******************/
    logo.instid = InstId;
    logo.mod = MyModId;
    logo.type = type;

    time(&t);

    if (type == TypeHeartBeat) {
        sprintf(msg, "%ld %ld\n", (long) t, (long) MyPid);
    } else if (type == TypeError) {
        sprintf(msg, "%ld %hd %s\n", (long) t, ierr, note);
        logit("et", "gmewhtmlemail: %s\n", note);
    }

    size = strlen(msg); /* don't include the null byte in the message */

    /* Write the message to shared memory
     ************************************/
    if (tport_putmsg(&InRegion, &logo, size, msg) != PUT_OK) {
        if (type == TypeHeartBeat) {
            logit("et", "gmewhtmlemail:  Error sending heartbeat.\n");
        } else if (type == TypeError) {
            logit("et", "gmewhtmlemail:  Error sending error:%d.\n", ierr);
        }
    }

    return;
}


/******************************************************************************
 * formatTimestamps() builds (up to) 2 strings representing the time in ot;   *
 *          one in UTC, the other in the local time for the location at       *
 *          (lat,lon) (if timestr isn't NULL).  Returns 0 if successful       *
 ******************************************************************************/
int formatTimestamps( time_t ot, double lat, double lon, char* timestr, char* timestrUTC ) {
    struct tm   mytimeinfo;
    char        cmdStr[512];
    FILE *fp;

    if ( timestrUTC==NULL ) {
        logit("e","formatTimestamps: timestr == NULL\n");
        return 3;
    }

    gmtime_ew ( &ot, &mytimeinfo );
    strftime( timestrUTC, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo ); // Prepare origin time (UTC)

    if ( timestr==NULL )
        return 0;

    snprintf( cmdStr, 512, "%s %s %lf %lf %s", PathToPython, PathToLocaltime, lon, lat, timestrUTC );
    if ( Debug )
        logit( "", "Local time command: %s\n", cmdStr );
    fp = popen( cmdStr, "r" );
    if ( fp == NULL) {
        logit("e","formatTimestamps: popen failed\n");
        return 1;
    }
    if ( fgets( timestr, 80, fp ) == NULL ) {
        logit("e","formatTimestamps: failed to read from popen\n");
        return 2;
    }
    pclose( fp );
    return 0;
}


/******************************************************************************
 * EstimatePGA() yields the estimated PGA for (lat,lon) of an event at        *
 *          (evt_lat,evt_lon) at depth evt_depth and magnitude evt_mag.       *
 *          Yields a negative number if an error occurs.                      *
 *    PGA from Java utility returned in units of g, 1g = 980cm/s/s/
 ******************************************************************************/
double EstimatePGA( double lat, double lon, double evt_lat, double evt_lon, double evt_depth, double evt_mag )  {
    char        cmdStr[512], resultStr[64];
    double ePGA;
    FILE *fp;

    snprintf( cmdStr, 512,  "java -cp %s/GmpeGmm.jar -DPGACALC_NO_RESULT=OOPS com.isti.gmpegmm.DeterministicSpectra \"Dam\" %lf %lf %lf %lf %lf %lf",
             EstimatePGAsPath, lon, lat, evt_mag, evt_lon, evt_lat, evt_depth );
    if ( Debug )
        logit( "", "ePGA call: %s\n", cmdStr );

    fp = popen( cmdStr, "r" );
    if ( fp == NULL ) {
        logit("e","EstimatePGA: popen failed\n");
        return -1;
    }
    if ( fgets( resultStr, 80, fp ) == NULL ) {
        logit("e","EstimatePGA: failed to read from popen\n");
        return -2;
    }
    pclose( fp );

    if ( sscanf( resultStr, "%lf", &ePGA ) != 1 ) {
        logit("e","EstimatePGA: failed to parse GmpeGmm result (%s)\n", resultStr);
        return -3;
    }
    if ( Debug )
        logit("","EstimatePGA result: %lf (%s)\n", ePGA, resultStr );
    return ePGA;
}

static char* determine_pga_color(double pga)
{
    if (ncolorpga > 0)
    {
        int i;
        for ( i = 0; i < ncolorpga; i++)
        {
            if (pga >= colorpga_values[i])
            {
                return colorpga_colors[i];
            }
        }
    }
    return NULL;
}

static void print_table_header_cell(FILE *htmlfile, char *text)
{
    fprintf( htmlfile, "<th><font size=\"1\" face=\"Sans-serif\" color=\"FFFFFF\">%s</font></th>", text);
}

static void print_table_cell(FILE *htmlfile, double value)
{
    fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"1\" face=\"Sans-serif\">%1.6lf</font></td>", value);
}

static void print_table_cell_color(FILE *htmlfile, double value, char *color)
{
    if (color == NULL)
    {
        fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"1\" face=\"Sans-serif\">%1.6lf</font></td>", value);
    }
    else
    {
        fprintf( htmlfile, "<td style=\"text-align:right;background-color:%s\"><font size=\"1\" face=\"Sans-serif\">%1.6lf</font></td>", color, value);
    }
}

/****************************************************************************
 * InsertStationTable(): adds station table to HTML email
 *			calculates snl_order, the order streams will be displayed in
 *
 * @params:
 *
 * in: htmlfile, fromArc, nsites, site_order[], sorted_sites[], smForSite[]
 * in/out: snl_sites, snl_order[]
 *
 * returns 0 if successful, -1 otherwise
 * */
int InsertStationTable(FILE *htmlfile, char fromArc, int nsites,
						SiteOrder site_order[MAX_STATIONS], SITE *sorted_sites[MAX_STATIONS],
						SM_INFO *smForSite[MAX_STATIONS], int *snl_sites,
						SiteOrder snl_order[MAX_STATIONS], double distances[MAX_STATIONS])
{
	int i = 0, j = 0, ix = 0, jx = 0, snl_i = 0; //various counter variables
	int num_snl = 0;	//so we don't have to always deference snl_sites 
        char *color;
        double pgapg;

	//Calculate the appropriate stream order:
    *snl_sites = num_snl = 1;
    if ( (fromArc) && (nsites > 0) ) {
    	logit("t", "Computing SNL order\n");
        snl_order[0] = site_order[0];
        for ( i=1; i<nsites; i++ ) {
        	if ( PlotAllSCNLs )
            	snl_order[num_snl++] = site_order[i];
            else {
            	ix = site_order[i].idx;
                for ( j=0; j<num_snl; j++ ) {
                	jx = snl_order[j].idx;
                    if ( !strcmp( smForSite[ix]->sta, smForSite[jx]->sta) &&
                    		!strcmp( smForSite[ix]->net, smForSite[jx]->net) &&
                        	!strcmp( smForSite[ix]->loc, smForSite[jx]->loc) ) {
                    	if ( smForSite[ix]->pga > smForSite[jx]->pga ) {
							logit("t", "Moving %s.%s to head (%lf %lf)\n", 
									smForSite[ix]->sta, smForSite[ix]->comp, 
									smForSite[ix]->pga, smForSite[jx]->pga );
                        	snl_order[j].idx = ix;
                        }
                        break;
                    }
                }
                if ( j == num_snl ) {
                    snl_order[num_snl] = site_order[i];
                    num_snl++;
                }
        	}
        }
	} else {
    	for ( i=0; i<nsites; i++ ) {
        	snl_order[i] = site_order[i];
		}
		num_snl = nsites;
    }

    if ( nsites > 0 ) {
    	fprintf( htmlfile, "<table id=\"StationTable\" border=\"0\" cellspacing=\"1\" cellpadding=\"3\" width=\"600px\">\n" );
        fprintf( htmlfile, "<tr bgcolor=\"000060\">");

        fprintf( htmlfile, "<th>#</th>");
        print_table_header_cell( htmlfile, "Station");
        if ( fromArc ) {
        	print_table_header_cell( htmlfile, "Distance (km)");
            if ( ShowMiles )
            	print_table_header_cell( htmlfile, "Distance (mi)");
        }
        print_table_header_cell( htmlfile, "AI");
        print_table_header_cell( htmlfile, "PGA (%g)");
        print_table_header_cell( htmlfile, "PGA (g)");
        print_table_header_cell( htmlfile, "PGA (cm/s/s)");
        print_table_header_cell( htmlfile, "PGV (cm/s)");
        print_table_header_cell( htmlfile, "PGD (cm)");
        print_table_header_cell( htmlfile, "RSA 0.3s (cm/s/s)");
        print_table_header_cell( htmlfile, "RSA 1s (cm/s/s)");
        print_table_header_cell( htmlfile, "RSA 3s (cm/s/s)");
        fprintf( htmlfile, "</tr>\n" );

        snl_i = 0;
        for( i = 0; i < nsites; i++ )
        {
        	ix = site_order[i].idx;
            fprintf( htmlfile,"<tr bgcolor=\"#%s\">", i%2==0 ? "DDDDFF" : "FFFFFF");
            if ( snl_order[snl_i].idx == ix ) {
            	snl_i++;
                fprintf( htmlfile, "<th><font size=\"1\" face=\"Sans-serif\">%d</font></th>", snl_i );
            } else {
                fprintf( htmlfile, "<th><font size=\"1\" face=\"Sans-serif\"></font></th>" );
			}
            fprintf( htmlfile, "<td><font size=\"1\" face=\"Sans-serif\">%s.%s.%s.%s</font></td>",
                     sorted_sites[i]->name, sorted_sites[i]->comp,
                     sorted_sites[i]->net, sorted_sites[i]->loc );
            if ( fromArc ) {
            	fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"1\" face=\"Sans-serif\">%1.1lf</font></td>", distances[ix]);
                if ( ShowMiles ) {
                	fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"1\" face=\"Sans-serif\">%1.1lf</font></td>", distances[ix]*KM2MILES);
                }
			}
            print_table_cell( htmlfile, smForSite[ix]->ai);
            pgapg = 100*smForSite[ix]->pga/CM_S_S_1_g;
            color = determine_pga_color(pgapg);
            print_table_cell_color( htmlfile, pgapg, color);
            print_table_cell_color( htmlfile, smForSite[ix]->pga/CM_S_S_1_g, color);
            print_table_cell_color( htmlfile, smForSite[ix]->pga, color);
            print_table_cell( htmlfile, smForSite[ix]->pgv);
            print_table_cell( htmlfile, smForSite[ix]->pgd);
            for ( j=0; j<3; j++ ) {
            	if ( j < smForSite[i]->nrsa) {
                	print_table_cell( htmlfile, smForSite[ix]->rsa[j]);
				} else {
                    fprintf( htmlfile, "<td>&nbsp;</td>");
				}
			}
            fprintf( htmlfile, "</tr>\n" );
        }
        fprintf( htmlfile, "</table><br/><br/>\n" );
    } else if ( fromArc ) {
    	fprintf( htmlfile, "<p>No sites for table</p><br/><br/>");
    }

	*snl_sites = num_snl;
	return 0;
}

/*
 * InsertDamTable(): inserts HTML table element displaying dam info
 *		calculates an estimate of PGA at facility
 *
 * @params:
 * in- htmlfile, HypoArc *, 
 *
 * in/out: double *max_epga_g
 *
 * returns 0 if successful, -1 otherwise
 * */
int InsertDamTable(FILE *htmlfile, HypoArc *arc, double *max_epga_g)
{
	char temp[MAX_GET_CHAR] = {0};
	double ePGA = 0.0;
	int damTblColumns = 3;
	int i = 0;

	//Output dam table:
    fprintf( htmlfile, "<table id=\"DamTable\" border=\"0\" cellspacing=\"1\" cellpadding=\"3\" width=\"600px\">\n" );
    fprintf( htmlfile, "<tr bgcolor=\"000060\">");
    fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">#</font></th>", facility_font_size);
    fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">Facility</font></th>", facility_font_size);
    if ( ShowRegionAndArea ) {
        fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">Region</font></th>", facility_font_size);
        fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">Area</font></th>", facility_font_size);
    }
    fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">Dist.(km)</font></th>", facility_font_size);
    if ( ShowMiles ) {
    	fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">Dist.(mi)</font></th>", facility_font_size);
	}
	if ( ReportEstimatedPGAs ) {
        fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">PGA(est.)(%%g)</font></th>", facility_font_size);
        fprintf( htmlfile, "<th><font size=\"%d\" face=\"Sans-serif\" color=\"FFFFFF\">PGA(est.)(g)</font></th>", facility_font_size);
    }
    fprintf( htmlfile, "</tr>\n" );

    damTblColumns = 3;
    if ( ShowRegionAndArea )    damTblColumns += 2;
    if ( ShowMiles )            damTblColumns += 1;
    if ( ReportEstimatedPGAs )  damTblColumns += 2;

    for ( i=0; i<dam_close_count; i++ ) {
    	if ( MaxFacilitiesInTable >= 0 && i >= MaxFacilitiesInTable ) {
        	fprintf( htmlfile,"<tr bgcolor=\"#FF6666\"><td colspan=\"%d\"><font size=\"%d\" face=\"Sans-serif\">%d more facilities (not listed)</font></td></tr>", damTblColumns, facility_font_size, dam_close_count-i );
            break;
        }
        if ( i == damMapLimit ) {
        	fprintf( htmlfile,"<tr bgcolor=\"#FF6666\"><td colspan=\"%d\"><font size=\"%d\" face=\"Sans-serif\">Facilities below this point will not appear on map</font></td></tr>", damTblColumns, facility_font_size );
        }
        fprintf( htmlfile,"<tr bgcolor=\"#%s\">", i%2==0 ? "DDDDFF" : "FFFFFF");
        if ( MapQuestKey[0] == 0 && i < 26 ) {
        	fprintf( htmlfile, "<td><font size=\"%d\" face=\"Sans-serif\">%c</font></td>", facility_font_size, i+'A' );
        } else {
        	fprintf( htmlfile, "<td><font size=\"%d\" face=\"Sans-serif\">%d</font></td>", facility_font_size, i+1 );
        }
		fprintf( htmlfile, "<td><font size=\"%d\" face=\"Sans-serif\">%s</font></td>", facility_font_size, dam_order[i]->name );
        if ( ShowRegionAndArea ) {
            fprintf( htmlfile, "<td><font size=\"%d\" face=\"Sans-serif\">%s</font></td>", facility_font_size, region_names[dam_order[i]->region_id] );
            fprintf( htmlfile, "<td><font size=\"%d\" face=\"Sans-serif\">%s</font></td>", facility_font_size, area_abbrs[dam_order[i]->area_id] );
        }
        fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"%d\" face=\"Sans-serif\">%1.1lf</font></td>", facility_font_size, dam_order[i]->dist);
        if ( ShowMiles )
        	fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"%d\" face=\"Sans-serif\">%1.1lf</font></td>", facility_font_size, dam_order[i]->dist*KM2MILES);
        if ( ReportEstimatedPGAs ) {
            ePGA = EstimatePGA(  dam_order[i]->lat, dam_order[i]->lon, arc->sum.lat,  arc->sum.lon, arc->sum.z, arc->sum.Mpref );
            if ( ePGA < 0 ) {
                strcpy( temp, "N/A" );
			} else {
                sprintf( temp, "%1.6lf", ePGA*100 );
			}
            if (ePGA > *max_epga_g)
                *max_epga_g = ePGA;
            fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"%d\" face=\"Sans-serif\">%s</font></td>", facility_font_size, temp);
            if ( ePGA >= 0 ) {
                sprintf( temp, "%1.6lf", ePGA );
			}
            fprintf( htmlfile, "<td style=\"text-align:right\"><font size=\"%d\" face=\"Sans-serif\">%s</font></td>", facility_font_size, temp);
        }
        fprintf( htmlfile, "</tr>\n" );
    }
    fprintf( htmlfile, "</table><br/><br/>\n" );

	return 0;
}
/* the magnitude type from hypoinverse: need to change if ML gets produced in future */
#define MAG_TYPE_STRING "Md"
#define MAG_MSG_TYPE_STRING "ML"
#define MAG_MSG_MWTYPE_STRING "Mw"

void InsertHeaderTable(FILE *htmlfile, HypoArc *arc, char Quality, int showDM, char *scnl, SM_INFO *sminfo ) {
    char        timestr[80], timestrUTC[80];                    /* Holds time messages */
    time_t      ot = ( time_t )( arc->sum.ot - GSEC1970 );
    //struct tm     *timeinfo;
    struct tm   mytimeinfo;
    char        *grname[36];          /* Flinn-Engdahl region name */
    int parity = 0, i;
    char        *bg[2] = {" bgcolor=\"DDDDFF\" class=\"alt\"", ""};
    char        *timeName;

    if ( formatTimestamps( ot, arc->sum.lat, arc->sum.lon, timestr, timestrUTC ) ) {
//         ot = ( time_t )( arc->sum.ot - GSEC1970 );
        //timeinfo =
        localtime_ew ( &ot, &mytimeinfo );
        strftime( timestr, 80, "%Y.%m.%d %H:%M:%S (local to server)", &mytimeinfo ); // Prepare origin time (local)

        //timeinfo =
        gmtime_ew ( &ot, &mytimeinfo );
        strftime( timestrUTC, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo ); // Prepare origin time (UTC)
    }

    // Table header
    fprintf( htmlfile, "<table id=\"DataTable\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">\n" );
    if(strlen(DataCenter) > 0 )
    {
        fprintf( htmlfile, "<tr bgcolor=\"000060\"><th><font size=\"3\" face=\"Sans-serif\" color=\"FFFFFF\">Data Center: %s</font><th><tr>\n", DataCenter );
    }
    if ( scnl ) {
        fprintf( htmlfile, "<tr bgcolor=\"000060\"><th><font size=\"3\" face=\"Sans-serif\" color=\"FFFFFF\">Threshold Trigger Exceedance</font><th><tr>\n" );

        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Source:</font></td><td><font size=\"3\" face=\"Sans-serif\">%s</font></td><tr>\n", bg[parity], scnl );
        parity = 1-parity;
    } else {
        // Event ID
        fprintf( htmlfile, "<tr bgcolor=\"000060\"><th><font size=\"3\" face=\"Sans-serif\" color=\"FFFFFF\">EW Event ID: %ld</font><th><tr>\n", arc->sum.qid );

        // NEIC id
        if ( neic_id[0] ) {
            fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">NEIC ID:</font></td><td><font size=\"3\" face=\"Sans-serif\"><a href=\"http://earthquake.usgs.gov/earthquakes/eventpage/%s\">%s</a></font></td><tr>\n", bg[parity], neic_id, neic_id );
            parity = 1-parity;
        }
    }

    timeName = ( neic_id[0] == 0 ) ? "Trigger" : "Origin";

    // Origin time (local)
    fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">%s time (local):</font></td><td><font size=\"3\" face=\"Sans-serif\">%s</font></td><tr>\n",
             bg[parity], timeName, timestr );
    parity = 1-parity;

    // Origin time (UTC)
    fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">%s time (UTC):</font></td><td><font size=\"3\" face=\"Sans-serif\">%s</font></td><tr>\n",
             bg[parity], timeName, timestrUTC );
    parity = 1-parity;

    // Seismic Region
    if (UseRegionName && ( arc->sum.lat != -1 || arc->sum.lon != -1 )) {
        FlEngLookup(arc->sum.lat, arc->sum.lon, grname, NULL);
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Location:</font></td><td><font size=\"3\" face=\"Sans-serif\">%-36s</font></td><tr>\n",
                 bg[parity], *grname );
        parity = 1-parity;
    }

    if ( arc->sum.lat != -1 || arc->sum.lon != -1 ) {
        // Latitude
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Latitude:</font></td><td><font size=\"3\" face=\"Sans-serif\">%7.4f</font></td><tr>\n",
                 bg[parity], arc->sum.lat );
        parity = 1-parity;

        // Longitude
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Longitude:</font></td><td><font size=\"3\" face=\"Sans-serif\">%8.4f</font></td><tr>\n",
                 bg[parity],
                 arc->sum.lon );
        parity = 1-parity;
    }

    if ( showDM ) {
        // Depth
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Depth:</font></td><td><font size=\"3\" face=\"Sans-serif\">%4.1f km</font></td><tr>\n",
                 bg[parity], arc->sum.z );
        parity = 1-parity;

        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Magnitude:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%4.1f</font></td><tr>\n",
                 bg[parity], arc->sum.Mpref );
        parity = 1-parity;
    }
    if ( sminfo ) {
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">PGA:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%4.1lf</font></td><tr>\n",
                 bg[parity], sminfo->pga );
        parity = 1-parity;
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">PGV:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%4.1lf</font></td><tr>\n",
                 bg[parity], sminfo->pgv );
        parity = 1-parity;
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">PGD:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%4.1lf</font></td><tr>\n",
                 bg[parity], sminfo->pgd );
        parity = 1-parity;
        for ( i=0; i<sminfo->nrsa; i++ ) {
            fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">RSA %d:</font></td>"
                     "<td><font size=\"3\" face=\"Sans-serif\">%4.1lf</font></td><tr>\n",
                     bg[parity], i+1, sminfo->rsa[i]);
            parity = 1-parity;
        }
    }


    /* Event details
     ***************/
    if( ShowDetail )
    {

        // RMS
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">RMS Error:</font></td><td><font size=\"3\" face=\"Sans-serif\">%5.2f s</font></td><tr>\n",
                 bg[parity], arc->sum.rms);
        parity = 1-parity;

        // Horizontal error
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Horizontal Error:"
                 "</font></td><td><font size=\"3\" face=\"Sans-serif\">%5.2f km</font></td><tr>\n",
                 bg[parity], arc->sum.erh );
        parity = 1-parity;

        // Vertical error
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Depth Error:</font></td><td><font size=\"3\" face=\"Sans-serif\">%5.2f km</font></td><tr>\n",
                 bg[parity], arc->sum.erz );
        parity = 1-parity;

        // Azimuthal gap
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Azimuthal Gap:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%d Degrees</font></td><tr>\n",
                 bg[parity], arc->sum.gap );
        parity = 1-parity;

        // Number of phases
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Total Phases:</font></td><td><font size=\"3\" face=\"Sans-serif\">%d</font></td><tr>\n",
                 bg[parity], arc->sum.nphtot);
        parity = 1-parity;

        // Used phases
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Total Phases Used:</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%d</font></td><tr>\n",
                 bg[parity], arc->sum.nph );
        parity = 1-parity;

        // Number of S phases
        fprintf( htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Num S Phases Used:</font></td><td><font size=\"3\" face=\"Sans-serif\">%d</font></td><tr>\n",
                 bg[parity], arc->sum.nphS );
        parity = 1-parity;

        // Average quality
        fprintf(htmlfile, "<tr%s><td><font size=\"3\" face=\"Sans-serif\">Quality:</font></td>"
                "<td><font size=\"3\" face=\"Sans-serif\">%c</font></td><tr>\n",
                bg[parity], Quality);
        parity = 1-parity;
    }

    // Finish reference table
    fprintf( htmlfile, "</table><br/><br/>\n" );
}

/* this is a squished down table  for the header info*/
void InsertShortHeaderTable(FILE *htmlfile, HypoArc *arc,char Quality ) {
    char        timestr[80];                    /* Holds time messages */
    time_t      ot;
    struct tm   mytimeinfo; //, *timeinfo;
    char        *grname[36];          /* Flinn-Engdahl region name */
    struct tm * (*timefunc)(const time_t *, struct tm *);
    char        time_type[30];                  /* Time type UTC or local */
    int parity = 0;
    char        *bg[2] = {" bgcolor=\"DDDDFF\" class=\"alt\"", ""};

    timefunc = localtime_ew;
    if( UseUTC )
    {
        timefunc = gmtime_ew;
        strcpy( time_type, "UTC" );
    }
    ot = ( time_t )( arc->sum.ot - GSEC1970 );
    //timeinfo =
    timefunc ( &ot, &mytimeinfo );
    //memcpy( &mytimeinfo, timeinfo, sizeof(mytimeinfo) );
    strftime( timestr, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo ); // Prepare origin time

    // Table header
    fprintf( htmlfile, "<table id=\"DataTable\" border=\"0\" cellspacing=\"1\" cellpadding=\"0\" width=\"600px\">\n" );
    if(strlen(DataCenter) > 0 )
    {
        fprintf( htmlfile,
                 "<tr bgcolor=\"000060\">"
                 "<th colspan=4><font size=\"3\" face=\"Sans-serif\" color=\"FFFFFF\">Data Center: %s</font></th>"
                 "</tr>\n", DataCenter );
    }
    // Event ID
    fprintf( htmlfile,
             "<tr bgcolor=\"000060\">"
             "<th colspan=4><font size=\"3\" face=\"Sans-serif\" color=\"FFFFFF\">EW Event ID: %ld</font></th>"
             "</tr>\n", arc->sum.qid );
    // Seismic Region
    if (UseRegionName && ( arc->sum.lat != -1 || arc->sum.lon != -1 )) {
        FlEngLookup(arc->sum.lat, arc->sum.lon, grname, NULL);
        fprintf( htmlfile,
                 "<tr%s>"
                 "<td colspan=2><font size=\"3\" face=\"Sans-serif\"><b>Location:</b></font></td>"
                 "<td colspan=2><font size=\"3\" face=\"Sans-serif\">%-36s</font></td>"
                 "</tr>\n",
                 bg[parity],
                 *grname );
        parity = 1-parity;
    }


    // Origin time
    fprintf( htmlfile,
             "<tr%s>"
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Origin time:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%s %s</font></td>\n",
             bg[parity], timestr, time_type );
    parity = 1-parity;
    // RMS
    fprintf( htmlfile,
             "<td><font size=\"3\" face=\"Sans-serif\"><b>RMS Error:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%5.2f s</font></td>"
             "</tr>\n",
             arc->sum.rms);
    // Latitude
    // Longitude
    if ( arc->sum.lat != -1 || arc->sum.lon != -1 ) {
        fprintf( htmlfile,
                 "<tr%s>"
                 "<td><font size=\"3\" face=\"Sans-serif\"><b>Latitude, Longitude:</b></font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%7.4f, %8.4f</font></td>\n",
                 bg[parity], arc->sum.lat, arc->sum.lon );
        parity = 1-parity;
    }
    // Horizontal error
    fprintf( htmlfile,
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Horizontal Error:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%5.2f km</font></td>"
             "</tr>\n", arc->sum.erh );

    // Depth
    fprintf( htmlfile,
             "<tr%s>"
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Depth:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%4.1f km</font></td>",
             bg[parity], arc->sum.z );
    parity = 1-parity;
    // Vertical error
    fprintf( htmlfile,
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Depth Error:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%5.2f km</font></td>"
             "</tr>\n",
             arc->sum.erz );

    // Coda magnitude
    if (arc->sum.mdwt == 0 && DontShowMd == 0) {
        fprintf( htmlfile,
                 "<tr%s>"
                 "<td><font size=\"3\" face=\"Sans-serif\"><b>Coda Magnitude:</b></font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">N/A %s</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">nobs=0</font></td>"
                 "</tr>\n",
                 bg[parity], MAG_TYPE_STRING );
        parity = 1-parity;
    } else if (DontShowMd == 0) {
        fprintf( htmlfile,
                 "<tr%s>"
                 "<td><font size=\"3\" face=\"Sans-serif\"><b>Coda Magnitude:</b></font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">%4.1f %s</font></td>"
                 "<td><font size=\"3\" face=\"Sans-serif\">nobs=%d</font></td>"
                 "</tr>\n",
                 bg[parity], arc->sum.Mpref, MAG_TYPE_STRING, (int) arc->sum.mdwt );
        parity = 1-parity;
    }

    // Average quality
    fprintf( htmlfile,
             "<tr%s>"
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Quality:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%c</font></td>", bg[parity], Quality);
    parity = 1-parity;

    // Azimuthal gap
    fprintf( htmlfile,
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Azimuthal Gap:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%d Degrees</font></td>"
             "</tr>\n", arc->sum.gap );

    // Number of phases
    fprintf( htmlfile,
             "<tr%s>"
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Total Phases:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%d</font></td>", bg[parity], arc->sum.nphtot);
    parity = 1-parity;

    // Used phases
    fprintf( htmlfile,
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Total Phases Used:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%d</font></td>"
             "</tr>\n", arc->sum.nph );

    // Number of S phases
    fprintf( htmlfile,
             "<tr%s>"
             "<td><font size=\"3\" face=\"Sans-serif\"><b>Num S Phases Used:</b></font></td>"
             "<td><font size=\"3\" face=\"Sans-serif\">%d</font> </td>"
             "</tr>\n",
             bg[parity], arc->sum.nphS );
    parity = 1-parity;


    // Finish reference table
    fprintf( htmlfile, "</table><br/><br/>\n" );
}

int compareSiteOrder( const void*site_p_1, const void*site_p_2 ) {
    double diff = ((SiteOrder*)site_p_1)->key - ((SiteOrder*)site_p_2)->key;
    return diff<0 ? -1 : diff>0 ? 1 : ((SiteOrder*)site_p_1)->idx - ((SiteOrder*)site_p_2)->idx;
}

int compareDamOrder( const void*dam_p_1, const void*dam_p_2 ) {
    DamInfo* dip1 = *((DamInfo**)dam_p_1);
    DamInfo* dip2 = *((DamInfo**)dam_p_2);
    double diff = dip1->dist - dip2->dist;
    int rv = diff<0 ? -1 : diff>0 ? 1 : 0;
    return rv;
}

int callback1(void* notUsed, int argc, char** argv, char** azColName  ) {
    char *p;
    //notUsed = 0;
    strcpy( neic_id, argv[0] );
    p = neic_id;
    while ( *p != 0 && *p != ' ' )
        p++;
    *p = 0;
    return 0;
}

int shouldSendEmail( int i, double distance_from_center, 
    HypoArc *arc, HypoArcSM2 *arcsm,
    uint32_t subRegionsUsed, uint64_t subAreasUsed ) 
{
    int j,k;
    int reg_match, area_match, dam_match, loc_match, mag_match;
    if ( !emailrecipients[i].subscription ) {
        /* Skip recipent if not subscribed & not on mailing list for this type of event */
        if ( ( (arcsm->fromArc && !emailrecipients[i].sendOnEQ) ||
                    (!arcsm->fromArc && !emailrecipients[i].sendOnSM) ) ) {
            logit("et", "gmewhtmlemail: No email sent to %s because wrong type of event\n",
                      emailrecipients[i].address);
            return 0;
        }

        /* Skip recipent if not subscribed & not on mailing list for this type of event */
        if ( ( emailrecipients[i].max_distance != OTHER_WORLD_DISTANCE_KM &&
                    emailrecipients[i].max_distance < distance_from_center ) ) {
            logit("et", "gmewhtmlemail: No email sent to %s because distance of event %5.1f higher than threshold %5.1f\n",
                  emailrecipients[i].address, distance_from_center, emailrecipients[i].max_distance);
            return 0;
        }
    } else {
        // Skip recipient if subscribed and doesn't meet criteria
        reg_match = ((emailrecipients[i].subRegions & subRegionsUsed) != 0);
        area_match = ((emailrecipients[i].subAreas & subAreasUsed) != 0);
        dam_match = 0;
        if ( !(reg_match || area_match) )
            for ( j=0; !dam_match && j<dam_sub_close_count; j++ )
                for ( k=0; !dam_match && k<emailrecipients[i].numDams; k++ )
                    if ( emailrecipients[i].subDams[k] == dam_order[j]->dam_id )
                        dam_match = 1;
        mag_match = (emailrecipients[i].min_magnitude==0 || emailrecipients[i].min_magnitude <= arc->sum.Mpref);  
        loc_match = (reg_match || area_match || dam_match);              

        if ( (loc_match==0) || (loc_match!=0 && mag_match==0) ) {
            logit("et", "gmewhtmlemail: No email sent to %s because no match on subscription criteria\n",
                      emailrecipients[i].address);
			//XXX XXX TODO: REMOVE!!!
			logit("et", "gmewhtmlemail: min_magnitude: %lf numRegions: %d numAreas: %d numDams: %d subRegionsUsed: %u subAreasUsed: %lu\n", emailrecipients[i].min_magnitude, emailrecipients[i].numRegions, emailrecipients[i].numAreas, emailrecipients[i].numDams, subRegionsUsed, subAreasUsed);
            return 1;
         }
     }
     return 1;
}

void prepare_dam_info( HypoArc *arc, 
    char*       regionsStr,     char*       regionsUsed,
    uint32_t*   subRegionsUsed, uint64_t*   subAreasUsed )
{
    int i, closeEnough = 1;
    
    for ( i=0; i<dam_count; i++ )
        dam_order[i]->dist = distance( arc->sum.lat, arc->sum.lon,
                                    dam_order[i]->lat, dam_order[i]->lon, 'K' );
    qsort( dam_order, dam_count, sizeof(dam_order[0]), &compareDamOrder );
    dam_close_count = dam_count;
    dam_sub_close_count = dam_count;
    for ( i=0; closeEnough && (i<dam_count); i++ ) {  
        closeEnough = 0;
        if ( dam_order[i]->dist <= MaxFacilityDist ) {
            dam_close_count = i+1;
            regionsUsed[dam_order[i]->region_id] = 1;
            closeEnough = 1;
        }       
        if ( dam_order[i]->dist <= MaxFacilityDist ) {
            dam_sub_close_count = i+1;
            *subRegionsUsed |= (1<<dam_order[i]->region_id);
            *subAreasUsed |= (UINT64_C(1)<<dam_order[i]->area_id);
            closeEnough = 1;
        }       
    }
}



/******************************************************************************
 * process_message() Processes a message to find if its a real event or not   *
 ******************************************************************************/
int process_message( int arc_index ) {
    HypoArcSM2  *arcsm = arc_list[arc_index];
    HypoArc     *arc = &(arcsm->ha);
    double      starttime = 0;                  /* Start time for all traces */
    double      endtime = 0;                    /* End time for all traces */
    double      dur;                            /* Duration of the traces */
    int         i, pos;                      /* Generic counters */
    int         gsamples[MAX_GET_SAMP];         /* Buffer for resampled traces */
    char        chartreq[50000];                /* Buffer for chart requests or GIFs */     /* 2013.05.29 for base64 */
    SITE        *sites[MAX_STATIONS];           /* Selected sites */
    double      coda_mags[MAX_STATIONS];        /* station coda mags > 0 if used */
    int         coda_weights[MAX_STATIONS];     /* Coda Weight codes for each trace */
    double      distances[MAX_STATIONS];        /* Distance from each station */
    SM_INFO     *smForSite[MAX_STATIONS];
    SiteOrder   site_order[MAX_STATIONS];
    SITE        *sorted_sites[MAX_STATIONS];
    SITE        *waveSite, triggerSite;
    char        *cPtr, phaseName[2];

    struct tm * (*timefunc)(const time_t *, struct tm *);
    char        time_type[30];                  /* Time type UTC or local */
    int         nsites = 0;                     /* Number of stations in the arc msg */
    char        system_command[MAX_STRING_SIZE];/* System command to call email prog */
    char        kml_filename[MAX_STRING_SIZE];  /* Name of kml file to be generated */
    FILE        *htmlfile;                      /* HTML file */
    FILE        *header_file;                   /* Email header file */
    FILE        *event_address_file;            /* File mapping events to addresses notified in case of deletion */
    time_t      ot, st;                         /* Times */
    struct tm   mytimeinfo; //, *timeinfo;
    char        timestr[80];                    /* Holds time messages */
    char        eventAddrName[250];             /* name of event_address_file */
    char        fullFilename[250];              /* Full html file name */
    char        hdrFilename[250];               /* Full header file name */
    char        *buffer;                        /* Buffer for raw tracebuf2 messages */
    int         bsamplecount;                   /* Length of buffer */
    char        Quality;        /* event quality */
    FILE*       gifHeader;                      /* Header for GIF attachments with sendmail */
    char        gifHeaderFileName[257];         /* GIF Header filename */
    int         rv = TRUE;
    char        temp[MAX_GET_CHAR];
    char        *request = chartreq;
    char        timestrUTC[80];                    /* Holds time messages */
    char        regionsStr[MAX_STRING_SIZE];
    char        regionsUsed[MAX_REGIONS] = {0};
    uint32_t    subRegionsUsed = 0;
    uint64_t    subAreasUsed = 0;
    double      maxmin[2];
    char        hasWaveforms = ((arcsm->fromArc) && (arcsm->sm_count > 0));
    int         ix;
    double      max_dist, max_epga_g, max_pga, max_dist_pad, max_pga_pad, max_pga_ix;
    SiteOrder   snl_order[MAX_STATIONS];
    int         snl_sites;
    sqlite3     *db;
    char        *err_msg = NULL;

    max_epga_g=0.0; /* max Estimated PGA in units of g */

    if ( dbAvailable ) {
        rv = sqlite3_open(db_path, &db);

        if (rv != SQLITE_OK) {
            logit("et", "Cannot open database: %s\n", sqlite3_errmsg(db));
        } else {
            char evt_sql[200];
            neic_id[0] = 0;
            sprintf( evt_sql, "SELECT PdlID FROM Events WHERE ROWID = %ld", arc->sum.qid );
            rv = sqlite3_exec(db, evt_sql, callback1, 0, &err_msg);
            sqlite3_close(db);
        }
    }

    /* Initialize kml file name and time type
     ****************************************/
    kml_filename[0] = 0;
    strcpy( time_type, "Local Time" );
    starttime = arc->sum.ot;

    /* Read phases using read_arc.c function
     * For each phase, try to find the station in the sites file
     * Then store phase data (arrival, type, coda length, etc)
     * and update starttime and endtime to include all phases
     * and corresponding coda lengths.
     ***********************************************************/
    for (i=0; i<arcsm->sm_count; i++)
    {

        // Search this site on the sites file
        pos = site_index( arcsm->sm_arr[i].sta,
                          arcsm->sm_arr[i].net,
                          arcsm->sm_arr[i].comp,
                          arcsm->sm_arr[i].loc[0] ? arcsm->sm_arr[i].loc : "--") ;
        // If site is not found, continue to next phase
        if( pos == -1 )
        {
            logit( "et", "gmewhtmlemail: Unable to find %s.%s.%s.%s (%d) on the site file\n",
                   arcsm->sm_arr[i].sta,
                   arcsm->sm_arr[i].comp,
                   arcsm->sm_arr[i].net,
                   arcsm->sm_arr[i].loc,
                   i
                 );
            continue;
        }
        if ( arcsm->sm_arr[i].pga < IgnorePGABelow )
        {
            logit( "et", "gmewhtmlemail: PGA too low for %s.%s.%s.%s (%d); ignored\n",
                   arcsm->sm_arr[i].sta,
                   arcsm->sm_arr[i].comp,
                   arcsm->sm_arr[i].net,
                   arcsm->sm_arr[i].loc,
                   i
                 );
            continue;
        }


        // New station, store its pointer
        sites[nsites] = &Site[pos];

        // Store epicentral distance
        site_order[nsites].key = distances[nsites] = distance( arc->sum.lat, arc->sum.lon,
                                 Site[pos].lat, Site[pos].lon, 'K' );

        if ( distances[nsites] > MaxStationDist )
            continue;
        site_order[nsites].idx = nsites;
        smForSite[nsites] = arcsm->sm_arr+i;

        // Increment nsites
        nsites++;

        /* Check if the number of sites exceeds the
         * predefined maximum number of stations
         ******************************************/
        if( nsites >= MAX_STATIONS )
        {
            logit( "et", "gmewhtmlemail: More than %d stations in message\n",
                   MAX_STATIONS );
            break;
        }
    } // End of 'for' cycle for processing the phases in the arc message

	if (nsites == 0) {
		if (Debug) logit( "et", "gmewhtmlemail: No stations to available, no event email will be sent\n");
		return 0;
	}

    qsort( site_order, nsites, sizeof(SiteOrder), &compareSiteOrder );
    for ( i=0; i<nsites; i++ )
        sorted_sites[i] = sites[site_order[i].idx];

    /* Correct times for epoch 1970
     ******************************/
    endtime = starttime;
    starttime -= GSEC1970;      // Correct for epoch

    ot = time( NULL );

    if ( IgnoreHours>0 && (ot - starttime)/3600 > IgnoreHours ) {
        logit("o", "gmewhtmlemail: Ignoring event more than %1.2lf hours old (%1.2lf hours)\n", IgnoreHours, (ot - starttime)/3600 );
        return 0;
    }

    starttime -= TimeMargin;    // Add time margin
    endtime -= GSEC1970;        // Correct for epoch
    endtime += TimeMargin;      // Add time margin

    /* Check maximum duration of the traces
     **************************************/
    dur = endtime - starttime;
    if( 1 ) { // dur > ( double ) DurationMax )
        endtime = starttime+DurationMax;
        dur = DurationMax;  /* this is used later for header of waveform table */
    }

    /* Change to UTC time, if required
     *********************************/
    timefunc = localtime_ew;
    if( UseUTC ) {
        timefunc = gmtime_ew;
        strcpy( time_type, "UTC" );
    }

    /* Log debug info
     ****************/
    if( Debug ) {
        logit("o", "Available channels (%d):\n", nsites);
        for( i = 0; i < nsites; i++ )
            logit( "o", "%5s.%3s.%2s.%2s\n",
                   sorted_sites[i]->name, sorted_sites[i]->comp,
                   sorted_sites[i]->net, sorted_sites[i]->loc);

        logit( "o", "Time margin: %f\n", TimeMargin );

        ot = ( time_t ) starttime;
        timefunc ( &ot, &mytimeinfo );
        strftime( timestr, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo );
        logit( "o", "Waveform starttime: %s %s\n", timestr, time_type );

        ot = ( time_t ) endtime;
        timefunc ( &ot, &mytimeinfo );
        strftime( timestr, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo );
        logit( "o", "Waveform endtime:   %s %s\n", timestr, time_type );
    }

    prepare_dam_info( arc, regionsStr, regionsUsed, &subRegionsUsed, &subAreasUsed );

    if ( dam_close_count == 0 ) {
        logit("t", "gmewhtmlemail: No facilities within %d km; aborting email for event id: %ld\n", MaxFacilityDist, arc->sum.qid);
        return 0;
    }

    /* Build list of region codes into a string */
    strcpy( regionsStr, " (" );
    for ( i=0; region_order[i] != 99; i++ )
        if ( regionsUsed[region_order[i]] ) {
            strcat( regionsStr, region_abbrs[region_order[i]] );
            strcat( regionsStr, "," );
        }
    regionsStr[strlen(regionsStr)-1] = ')';
    regionsStr[strlen(regionsStr)] = 0;

    /* At this point, we have retrieved the information required for the html email
     * traces, if there is any. Now, move forward to start producing the output files.
     *********************************************************************************/

    /* build a KML file
     ******************/
    if( KMLdir[0] != 0 )
    {
        logit( "ot", "gmewhtmlemail: writing KML file.\n" );
        kml_writer( KMLdir, &(arc->sum), KMLpreamble, kml_filename, sorted_sites, nsites );
    }

    /* Start html email file
     ***********************/
    if ( neic_id[0] ) {
        ot = ( time_t )( arc->sum.ot - GSEC1970 );
        gmtime_ew ( &ot, &mytimeinfo );
        strftime( timestrUTC, 80, "%Y%m%d%H%M%S", &mytimeinfo ); // Prepare origin time (UTC)
        snprintf(fullFilename, 250, "%s_%s_%s.html", HTMLFile, timestrUTC, neic_id ); // Name of html file
    } else
        snprintf(fullFilename, 250, "%s_%ld.html", HTMLFile, arc->sum.qid ); // Name of html file
    if( Debug ) logit( "ot", "Creating html file %s\n", fullFilename );

    // Open html file
    if( (htmlfile = fopen( fullFilename, "w" )) != NULL )
    {
        /* Save html header
         ******************/
        if( Debug ) logit( "ot", "Writing html header %s\n", fullFilename );
        // Placing css in the html body instead of header, to bypass some email clients
        // blocking the style sheet
        fprintf( htmlfile, "<html>\n<header>\n<style type=\"text/css\">" );

        /* Close style header
         ********************/
        fprintf( htmlfile, "\n</style>" );

        /* Close HTML Header */
        fprintf( htmlfile, "</header>\n" );

        /* Open HTML body */
        fprintf( htmlfile, "<body>\n" );

        Quality = ComputeAverageQuality( arc->sum.rms, arc->sum.erh, arc->sum.erz,
                                         arc->sum.z, (float) (1.0*arc->sum.dmin), arc->sum.nph, arc->sum.gap );

        /* Create table with reference information
         *****************************************/
		//This creates an event summary table named 'DataTable':
        if (ShortHeader) {
            InsertShortHeaderTable(htmlfile, arc, Quality);
        } else {
            char *sncl = arcsm->fromArc ? NULL : arcsm->sncl;
            InsertHeaderTable(htmlfile, arc, Quality, arcsm->fromArc, sncl, sncl!=NULL && nsites==1 ? smForSite[0] : NULL );
        }

		//Output station table:
		if(InsertStationTable(htmlfile, arcsm->fromArc, nsites, site_order, sorted_sites, smForSite, &snl_sites, snl_order, distances) == -1) {
			logit("et", "Failed to insert station table\n");	
		}

		//Output dam table:
		if(InsertDamTable(htmlfile, arc, &max_epga_g) == -1) {
			logit("et", "Failed to insert dam table\n");	
		}

        /* Produce google map with the stations and hypocenter
         *****************************************************/
        i=-1;
        if( GMTmap ) {
            if( Debug ) logit( "ot", "Computing GMT map\n" );
            if((i=gmtmap(chartreq,sorted_sites,nsites,arc->sum.lat,arc->sum.lon,arc->sum.qid)) == 0)
                fprintf(htmlfile, "%s\n<br/><br/>", chartreq);
        }

        if(i==-1) {
            if( Debug ) logit( "ot", "Computing map\n" );
            MapRequest( chartreq, sorted_sites, nsites, arc->sum.lat, arc->sum.lon, arcsm, site_order, snl_order );
            fprintf(htmlfile, "%s\n<br/><br/>", chartreq);
        }


        max_pga=0.0; /* max PGA in units of cm/s/s */
        if ( hasWaveforms ) {
            if( Debug ) logit( "ot", "Computing PGA chart\n" );
            ix = snl_order[snl_sites-1].idx;
            max_dist = distances[ix];
            max_dist_pad = max_dist*1.1;

            /* Base of the google static chart
             *********************************/
            snprintf(request, MAX_GET_CHAR, "<img src=\"http://chart.apis.google.com/chart?chs=600x400&cht=s&chts=000000,24&chtt=Peak+PGA+vs.+Distance" );

            /* Add distances (x axis)
             ************************/
            max_pga_ix = ix = snl_order[0].idx;
            max_pga = smForSite[ix]->pga;
			if ( RestrictThresholds ) {
				if (strcmp(RestrictedNetwork, smForSite[ix]->net) != 0) {
					max_pga = 0.0;  //effectively ignore initial PGA if it's not from network of interest
				}
			}
			logit("t", "Starting max_pga for %s.%s.%s.%s pga=%lf distance=%lf\n", 
					smForSite[ix]->net, smForSite[ix]->sta, 
					smForSite[ix]->comp, smForSite[ix]->loc, 
					smForSite[ix]->pga, distances[ix] );
            snprintf( temp, MAX_GET_CHAR, "%s&chd=t:%lf", request, distances[ix]/max_dist_pad*100 );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            for( i = 1; i < snl_sites; i++ )
            {
                ix = snl_order[i].idx;
                snprintf( temp, MAX_GET_CHAR, "%s,%lf", request, distances[ix]/max_dist_pad*100 );
                snprintf( request, MAX_GET_CHAR, "%s", temp );
				logit("t", "Testing max_pga for %s.%s.%s.%s pga=%lf distance=%lf\n", 
					smForSite[ix]->net, smForSite[ix]->sta, 
					smForSite[ix]->comp, smForSite[ix]->loc, 
					smForSite[ix]->pga, distances[ix] );
				if ( RestrictThresholds && (strcmp(RestrictedNetwork, smForSite[ix]->net) != 0)) {
					continue;	
				}
                if ( max_pga < smForSite[ix]->pga ) {
                    max_pga = smForSite[ix]->pga;
                    max_pga_ix = ix;
                }
            }
            max_pga_pad = max_pga*1.1;

            /* Add PGAs (y axis)
             *******************/
            ix = snl_order[0].idx;
            snprintf( temp, MAX_GET_CHAR, "%s|%lf", request, smForSite[ix]->pga/max_pga_pad*100 );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            for( i = 1; i < snl_sites; i++ )
            {
                ix = snl_order[i].idx;
                snprintf( temp, MAX_GET_CHAR, "%s,%lf", request, smForSite[ix]->pga/max_pga_pad*100 );
                snprintf( request, MAX_GET_CHAR, "%s", temp );
            }

            /* Add annotations
             *****************/
            snprintf( temp, MAX_GET_CHAR, "%s&chm=B,333333,0,0,7", request );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            for( i = 0; i < snl_sites; i++ ) {
                ix = snl_order[i].idx;
                snprintf( temp, MAX_GET_CHAR, "%s|A%s.%s.%s.%s,%s,0,%d,7", request,
                          smForSite[ix]->sta, smForSite[ix]->comp,
                          smForSite[ix]->net, smForSite[ix]->loc,
                          max_pga_ix==ix ? "990000" : "333333", i );
                snprintf( request, MAX_GET_CHAR, "%s", temp );
            }

            /* Specify colors
             *****************/
            snprintf( temp, MAX_GET_CHAR, "%s&chco", request );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            for( i = 0; i < snl_sites; i++ ) {
                ix = snl_order[i].idx;
                snprintf( temp, MAX_GET_CHAR, "%s%c%s", request, i==0 ? '=' : '|',
                          max_pga_ix==ix ? "FF0000" : "000099" );
                snprintf( request, MAX_GET_CHAR, "%s", temp );
            }


            /* Specify axes
             **************/
            snprintf( temp, MAX_GET_CHAR, "%s&chxt=x,y,x,y&chxr=0,0,%lf|1,0,%lf&chxl=2:|Distance+(km)|3:|PGA|(cm/s/s)&chxp=2,50|3,52,48&chg=100,100,1,0", request, max_dist_pad, max_pga_pad);
            snprintf( request, MAX_GET_CHAR, "%s", temp );

            /* End of the request
             ********************/
            snprintf( temp, MAX_GET_CHAR, "%s\"/>", request );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            fprintf(htmlfile, "%s\n<br/><br/>", request);
        } else if ( arcsm->fromArc ) {
            if( Debug ) logit( "ot", "No PGA chart, no sites with waveforms\n" );
            snprintf( temp, MAX_GET_CHAR, "<p>No sites to plot</p>" );
            snprintf( request, MAX_GET_CHAR, "%s", temp );
            fprintf(htmlfile, "%s\n<br/><br/>", request);
        }


        /* Reserve memory buffer for raw tracebuf messages
         *************************************************/
        if ( hasWaveforms && NoWaveformPlots == 0 ) {
            buffer = ( char* ) malloc( MAX_SAMPLES * 4 * sizeof( char ) );
            if( buffer == NULL )
            {
                logit( "et", "gmewhtmlemail: Cannot allocate buffer for traces\n" );
                // return -1; - Event if there is no memory for traces, still try to send email
            }
            else
            {
                /* Produce station traces
                 ************************/
                if( Debug ) logit("ot", "Computing Traces\n" );

                // Save header of traces table
                st = ( time_t )starttime;
                gmtime_ew( &st, &mytimeinfo );
                strftime( timestr, 80, "%Y.%m.%d %H:%M:%S", &mytimeinfo );
                fprintf( htmlfile, "<table id=\"WaveTable\" width=\"%d;\">\n",
                         TraceWidth + 10 );
                fprintf( htmlfile, "<thead>\n" );
                fprintf( htmlfile, "<tr bgcolor=\"000060\"><th><font size=\"1\" face=\"Sans-serif\" color=\"FFFFFF\">Waveform%s: (StartTime: %s UTC, Duration: %d"
                         " seconds)</font></th></tr>\n", arcsm->fromArc ? "s" : "", timestr, (int) dur );
                fprintf( htmlfile, "</thead>\n" );
                fprintf( htmlfile, "</tbody>\n" );

                /* Cycle for each station
                 *  - Try to retrieve waveform data from the wave servers
                 *  - Decide to produce GIF or google chart
                 *  - If GIF, call function to produce GIF file and the request is
                 *          the filename within <img> tags - Create GIF header
                 *  - If google char, resample the data and then produce the google chart
                 *          encoded GET request
                 ************************************************************************/
                if( UseGIF && !UseBlat )
                {
                    /* If using gifs with sendmail, we need to start writing a header file
                     * with the base64 data included
                     *******************************************************/
                    snprintf( gifHeaderFileName, 257, "%s_GifHeader", HTMLFile );
                    gifHeader = fopen( gifHeaderFileName, "w" );
                }

                /* Loop over the stations or, if a triggered event, over the triggering station */
                for( ix = 0; (ix==0 && !arcsm->fromArc) || (ix < nsites); ix++ )
                {
                    if ( !arcsm->fromArc ) {
                        /* Fill triggerSite/waveSite with the triggering SCNL */
                        cPtr = arcsm->sncl;
                        i = 0;
                        while (*cPtr != '.')
                            triggerSite.name[i++] = *(cPtr++);
                        triggerSite.name[i++] = 0;
                        i = 0;
                        cPtr++;
                        while (*cPtr != '.')
                            triggerSite.net[i++] = *(cPtr++);
                        triggerSite.net[i++] = 0;
                        i = 0;
                        cPtr++;
                        while (*cPtr != '.')
                            triggerSite.comp[i++] = *(cPtr++);
                        triggerSite.comp[i++] = 0;
                        i = 0;
                        cPtr++;
                        while (*cPtr)
                            triggerSite.loc[i++] = *(cPtr++);
                        triggerSite.loc[i++] = 0;
                        waveSite = &triggerSite;
                        strcpy(phaseName, "T"); // this is legacy from ewhtmlemail and should be removed
                    } 
                    else {
                        i = site_order[ix].idx;
                        waveSite = sites[i];
                        strcpy(phaseName, "X"); // this is legacy from ewhtmlemail and should be removed
                    }

                    /* Load data from waveservers
                     ****************************/
                    bsamplecount = MAX_SAMPLES * 4; //Sets number of samples back
                    if( Debug ) logit( "o", "Loading data from %s.%s.%s.%s\n",
                                           waveSite->name, waveSite->comp,
                                           waveSite->net, waveSite->loc );
                    if( getWStrbf( buffer, &bsamplecount, waveSite, starttime, endtime ) == -1 )
                    {
                        logit( "t", "gmewhtmlemail: Unable to retrieve data from waveserver"
                               " for trace from %s.%s.%s.%s\n",
                               waveSite->name, waveSite->comp,
                               waveSite->net, waveSite->loc );
                        continue;
                    }

					/* Produce traces using EWAVE plotting service, drawing GIF or using Google Chart
				 	 *********************************************************************************/
					if ( UseEWAVE )
					{
						if ( Debug ) {
							logit( "et", "About to call writePlotRequest for station %s (smForSite->sta): %s\n", waveSite->name, smForSite[i]->sta);
						}
						if (writePlotRequest(chartreq, 50000, arc->sum.qid, ewaveAddr, ewavePort,
											waveSite->net, waveSite->name, waveSite->loc, waveSite->comp,
											st, (int)dur) < 0)
						{
							//this is an error!
							logit( "et", "Unable to write request for EWAVE plot of %s.%s.%s.%s\n",
											waveSite->net, waveSite->name, waveSite->loc, waveSite->comp);
							continue;
						}
					}
					else if( UseGIF )
                    {
						if (createGifPlot(chartreq, arc, buffer, bsamplecount, waveSite, starttime, endtime, phaseName) == -1) {
							logit( "et", "Unable to create GIF plot for %s.%s.%s.%s\n",
											waveSite->net, waveSite->name, waveSite->loc, waveSite->comp);
							continue;	
						}
                    }
                    else
                    {

                        /* Produce Google trace
                         **********************/
                        /* Resample trace
                         ****************/
                        if( Debug ) logit( "o", "Resampling samples for google chart\n" );
                        if( trbufresample( gsamples, MAX_GET_SAMP, buffer, bsamplecount,
                                           starttime, endtime, 30, maxmin ) == -1 ) // The value 30 comes from google encoding
                        {
                            logit( "e", "gmewhtmlemail: Error resampling samples from "
                                   "%s.%s.%s.%s\n",
                                   waveSite->name, waveSite->comp,
                                   waveSite->net, waveSite->loc );
                            continue;
                        }

                        /* Produce google charts request
                         *******************************/
                        if( Debug ) logit( "o", "Creating google chart request\n" );
                        if( makeGoogleChart( chartreq, gsamples, MAX_GET_SAMP, phaseName,
                                             (((smForSite[i]->t)-starttime)/
                                              (endtime-starttime)*100),
                                             TraceWidth, TraceHeight, maxmin ) == -1 )
                        {
                            logit( "e", "gmewhtmlemail: Error generating google chart for "
                                   "%s.%s.%s.%s\n",
                                   waveSite->name, waveSite->comp,
                                   waveSite->net, waveSite->loc );
                            continue;
                        }
                        if( Debug ) logit( "o", "Produced google chart trace for %s.%s.%s.%s\n",
                                               waveSite->name, waveSite->comp,
                                               waveSite->net, waveSite->loc );
                    } // End of decision to make GIF or google chart traces

                    /* Add to request to html file
                     *****************************/
                    ot = ( time_t ) smForSite[i]->t;
                    timefunc ( &ot, &mytimeinfo );
                    strftime( timestr, 80,"%Y.%m.%d %H:%M:%S", &mytimeinfo );
                    if ( arcsm->fromArc ) {
                        fprintf(htmlfile, "<tr bgcolor=\"DDDDFF\" class=\"WaveTableTextRowClass\"><td>%s%s : "
                                "%3s.%2s.%2s Distance=%5.1f km",
                                font_open,
                                waveSite->name, waveSite->comp,
                                waveSite->net, waveSite->loc,
                                distances[i]);

                        // Include coda magnitudes, if available
                        if( coda_mags[i] != 0.0 && DontShowMd == 0)
                        {
                            fprintf( htmlfile, " <b>Md=%3.1f wt=%d</b>", coda_mags[i], coda_weights[i] );
                        }

                        // Create the table row with the chart request
                        // Included font-size mandatory style for composite images
                        fprintf( htmlfile, "%s</td></tr>\n", font_close );
                    }
                    fprintf( htmlfile, "<tr class=\"WaveTableTraceRowClass\" style=\"font-size:0px;\"><td>%s%s%s</td></tr>\n",
                             font_open, chartreq, font_close);
                } // End of trace for cycle

                /* Last boundary and close gif header file, if that is the case */
                if( UseGIF && !UseBlat )
                {
                    fprintf( gifHeader, "--FILEBOUNDARY--\n" );
                    fclose( gifHeader );
                }

                // Free buffer
                free( buffer );

                // Finish trace table
                fprintf( htmlfile, "</tbody>\n" );
                fprintf(htmlfile, "</table><br/><br/>\n");

            } // End of Traces section
        
        }

        /* Footer for html file
         ******************/
        if ( arcsm->fromArc && EarthquakeDisclaimer )
            fprintf(htmlfile,"<hr><div>\n%s\n</div><hr>\n",EarthquakeDisclaimer);
        else if ( !arcsm->fromArc && TriggerDisclaimer )
            fprintf(htmlfile,"<hr><div>\n%s\n</div><hr>\n",TriggerDisclaimer);
        fprintf(htmlfile, "<p id=\"Footer\">%sThis report brought to you by Earthworm with gmewhtmlemail (version %s)%s</p>", font_open, VERSION_STR, font_close);

        /* Finish html file
         ******************/
        if ( Debug ) logit( "ot", "Closing file\n" );
        fprintf(htmlfile,"</body>\n</html>\n");
        fclose(htmlfile);

        /* Send email
         *****************/
        logit("ot","nsites = %d, Max measured pga = %lg (cm/s/s), Max estimated pga = %lg (cm/s/s), threshold = %lg (cm/s/s)\n", nsites, max_pga, max_epga_g*CM_S_S_1_g, PGAThreshold );
        /* note minquality check is inverted because A, B, C, D in chars A has lower value than B and so on */
        if ( strlen( EmailProgram ) <= 0 ) {
            /* No email program, so can't send email */
                logit("et", "gmewhtmlemail: No emails sent, No EmailProgram listed for sending.\n");
/* REMOVE this next check eventually??, because there is no Quality assignement from PDL.....*/
        } else if (Quality>MinQuality) {
                logit("et", "gmewhtmlemail: No emails sent, quality %c is below MinQuailty %c.\n",
                      Quality, MinQuality);
        } else if ( !( (max_pga >= PGAThreshold) || 
		  (max_epga_g > 0.0 && max_epga_g*CM_S_S_1_g >= PGAThreshold) )) {
                logit("et", "gmewhtmlemail: No emails sent, no measured %f or estimated PGA %f above %lg (cm/s/s).\n",
                      max_pga, max_epga_g*CM_S_S_1_g, PGAThreshold);
        } else {
            // Check subscription list; if newer than what we have, reload subscriptions
            updateSubscriptions();        
            
            if ( nemailrecipients > 0 ) {
                double distance_from_center, azm;
                geo_to_km(center_lat, center_lon, arc->sum.lat, arc->sum.lon, &distance_from_center, &azm);
                logit( "ot", "gmewhtmlemail: processing  email alerts.\n" );
                for( i=0; i<nemailrecipients; i++ )// One email for each recipient
                {
                    if ( !shouldSendEmail( i, distance_from_center, arc, arcsm, subRegionsUsed, subAreasUsed ) ) {
                        //save this status so we know which addresses will be getting email for this event
                        emailrecipients[i].shouldSendEmail = 0;
                        continue;
                    } else emailrecipients[i].shouldSendEmail = 1;
                    /* Send it! */
                    if( UseBlat )       // Use blat for sending email
                    {
                        if( kml_filename[0] == 0 )
                        {
                            snprintf(system_command, 1024, "%s %s -html -to %s -subject \"%s "
                                    "- EW Event ID: %ld%s\" %s",
                                    EmailProgram, fullFilename, emailrecipients[i].address,
                                    SubjectPrefix, arc->sum.qid, regionsStr, BlatOptions);
                        }
                        else
                        {
                            /* the latest version of blat supports -attacht option for text file attachment, send the KML file */
                            snprintf(system_command, 1024, "%s %s -html -to %s -subject \"%s "
                                    "- EW Event ID: %ld%s\" -attacht %s %s",
                                    EmailProgram, fullFilename, emailrecipients[i].address,
                                    SubjectPrefix, arc->sum.qid, regionsStr, kml_filename, BlatOptions);
                        }
                    }
                    else                // Use sendmail
                    {
                        /* Create email header file
                         **************************/
                        snprintf( hdrFilename, 250, "%s_header.tmp", HTMLFile );
                        header_file = fopen( hdrFilename, "w" );
                        fprintf( header_file, "To: %s\n", emailrecipients[i].address );
                        if ( arcsm->fromArc )
                            fprintf( header_file, "Subject: %s - EW Event (%s) NEIC ID: %s%s\n",
                                     SubjectPrefix, timestrUTC, neic_id, regionsStr );
                        else
                            fprintf( header_file, "Subject: %s - Trigger from %s%s\n",
                                     SubjectPrefix, arcsm->sncl, regionsStr );
                        if( UseGIF )
                        {
                            /* When using GIFs the header must be different */
                            fprintf( header_file, "MIME-Version: 1.0\n"
                                     "Content-Type: multipart/mixed; boundary=\"FILEBOUNDARY\"\n\n"
                                     "--FILEBOUNDARY\n"
                                     "Content-Type: text/html\n\n" );
                            fclose( header_file );

                            /* System command for sendmail with attachments */
                            snprintf(system_command, 1024, "cat %s %s %s | %s -t ",
                                    hdrFilename, fullFilename, gifHeaderFileName, EmailProgram);
                        }
                        else
                        {
                            fprintf( header_file, "Content-Type: text/html\n\n" );
                            fclose( header_file );

                            /* System command for sendmail without attachments */
                            snprintf(system_command, 1024, "cat %s %s | %s -t ",
                                    hdrFilename, fullFilename, EmailProgram);
                        }
                    }

                    /* Execute system command to send email
                     **************************************/
                    //printf( "Email command: '%s'\n", system_command );
                    system(system_command);
                    logit("et", "gmewhtmlemail: email sent to %s, passed tests\n", emailrecipients[i].address);
                    if (DebugEmail) {
                        logit("et", "gmewhtmlemail: Debug; EmailCommand issued '%s'\n", system_command);
                    }
                 }
               //only do this for events that we got from PDL, which we check by seeing that there's an NEIC id
               if (neic_id[0]) {
                  //set up file name
                  snprintf(eventAddrName, 250, "%s/%s", PathToEventEmail, neic_id);
                  //open "event address file"
                  if((event_address_file = fopen(eventAddrName, "w+")) == NULL) {
                     //this is an error and errno needs to be checked!!
                     logit("et", "gmewhtmlemail: Error; failed to open file %s to log email addresses!\n", eventAddrName);
                  } else {
                     //loop over email recipients again, saving which recipients were emailed for this event
                     for (i = 0; i < nemailrecipients; i++) {
                        if(emailrecipients[i].shouldSendEmail) {
                           fprintf(event_address_file, "%s\n", emailrecipients[i].address);
                        }
                     }
                     //close the file:
                     if(fclose(event_address_file)) {
                        //error closing the file!!! should be logged               
                        logit("et", "gmewhtmlemail: error closing file %s!\n", eventAddrName); 
                     } else {
                        if (DebugEmail) logit("et", "gmewhtmlemail: logged event recipients in file %s\n", eventAddrName);
                     }
                  }
               }
            }
        }
    } else {
        logit("et", "gmewhtmlemail: Unable to write html file: %s\n", fullFilename);
        rv = FALSE;
    }
    logit("et", "gmewhtmlemail: Completed processing of event id: %ld\n", arc->sum.qid);
    return rv;
}



/*******************************************************************************
 * MapRequest: Produce a google map request for a given set of stations        *
 *                   and a hypocenter                                          *
 ******************************************************************************/
void MapRequest(char *request, SITE **sites, int nsites,
                double hypLat, double hypLon, HypoArcSM2 *arcsm,
                SiteOrder *site_order, SiteOrder *snl_order ) 
{
    if ( strlen(MapQuestKey) > 0 )
        MapQuestMapRequest( request, sites, nsites, hypLat, hypLon, arcsm, site_order, snl_order );
    else
        GoogleMapRequest( request, sites, nsites, hypLat, hypLon, arcsm, site_order, snl_order );
}

/*******************************************************************************
 * GoogleMapRequest: Produce a google map request for a given set of stations  *
 *                   and a hypocenter                                          *
 ******************************************************************************/
void GoogleMapRequest(char *request, SITE **sites, int nsites,
                      double hypLat, double hypLon, HypoArcSM2 *arcsm,
                      SiteOrder *site_order, SiteOrder *snl_order ) 
{
    int i;
    int cap;
    char temp[MAX_GET_CHAR];

    /* Base of the google static map
    *******************************/
    snprintf(request, MAX_GET_CHAR, "<img class=\"MapClass\" alt=\"\" "
             "src=\"https://maps.googleapis.com/maps/api/staticmap?"
             "size=600x400&format=png8&maptype=%s&sensor=false", StaticMapType);

    /* Icon for hypocenter
    *********************/
    if (hypLat!=0.0 || hypLon!=0.0)
    {
        snprintf( temp, MAX_GET_CHAR, "%s&markers=icon:http:%%2F%%2Fmaps.google.com%%2Fmapfiles"
                  "%%2Fkml%%2Fpaddle%%2Fgrn-stars-lv.png%%7Cshadow:true%%7C%f,%f",
                  request, hypLat, hypLon);
        snprintf( request, MAX_GET_CHAR, "%s", temp );
    }

    /* Add icons for stations
    ************************/
    snprintf( temp, MAX_GET_CHAR, "%s&markers=color:green%%7Cshadow:false", request );
    snprintf( request, MAX_GET_CHAR, "%s", temp );
    for( i = 0; i < nsites; i++ )
    {
        snprintf( temp, MAX_GET_CHAR, "%s%%7C%f,%f", request, sites[i]->lat, sites[i]->lon );
        snprintf( request, MAX_GET_CHAR, "%s", temp );
    }

    /* Add icons for facilities
    **************************/
    cap = dam_close_count>damMapLimit ? damMapLimit : dam_close_count;
    for ( i=0; i<cap && i<26; i++ )
    {
        snprintf( temp, MAX_GET_CHAR, "%s&markers=icon:http:%%2F%%2Fmaps.google.com%%2Fmapfiles"
                  "%%2Fkml%%2Fpaddle%%2F%c.png%%7Cshadow:true%%7C%f,%f",
                  request, i+'A', dam_order[i]->lat, dam_order[i]->lon);
        snprintf( request, MAX_GET_CHAR, "%s", temp );
    }

    for (i=0 ; i<cap; i++ )
    {
        snprintf( temp, MAX_GET_CHAR, "%s&markers=icon:http:%%2F%%2Fmaps.google.com%%2Fmapfiles"
                  "%%2Fkml%%2Fpaddle%%2Fred-circle.png%%7Cshadow:true%%7C%f,%f",
                  request, dam_order[i]->lat, dam_order[i]->lon);
        snprintf( request, MAX_GET_CHAR, "%s", temp );
    }
    /* End of the request
    ********************/
    snprintf( temp, MAX_GET_CHAR, "%s\"/>", request );
    snprintf( request, MAX_GET_CHAR, "%s", temp );

}


/*******************************************************************************
 * MapQuestMapRequest: Produce a MapQuest map request for a given set of       *
 *                   stations and a hypocenter                                 *
 ******************************************************************************/
void MapQuestMapRequest(char *request, SITE **sites, int nsites,
                        double hypLat, double hypLon, HypoArcSM2 *arcsm,
                        SiteOrder *site_order, SiteOrder *snl_order )
{
    int i;
    int ix;
    int snl_i;
    int cap;

    if( Debug )
        logit( "t", "ewhtmlemail: mapquest map creation starts\n");
    /* Base of the google static map
    *******************************/
    snprintf(request, MAX_GET_CHAR, "<img alt=\"Center/Station/Facility Map\" "
             "src=\"http://www.mapquestapi.com/staticmap/v4/getmap?key=%s&declutter=true&"
             "size=600,400&imagetype=png&type=%s", MapQuestKey, MQStaticMapType);
    request += strlen( request );

    /* Icon for hypocenter
    *********************/
    if (hypLat!=0.0 || hypLon!=0.0)
    {
        //snprintf( request, MAX_GET_CHAR, "yellow_1,%f,%f|",
        snprintf( request, MAX_GET_CHAR, "&xis=http://love.isti.com/~scott/LASSO/hypocenter.png,1,C,%f,%f",
                  hypLat, hypLon);
        request += strlen( request );
    }

    snprintf( request, MAX_GET_CHAR, "&pois=" );
    request += strlen( request );

    /* Add icons for stations
    ************************/

    if( Debug )
        logit( "t", "ewhtmlemail: adding %d sites\n", nsites);
    snl_i = 0;
    for( i = 0; i < nsites; i++ )
    {
        ix = site_order[i].idx;

        if ( snl_order[snl_i].idx == ix ) {
            snprintf( request, MAX_GET_CHAR, "%spink_1-%d,%1.1f,%1.1f|", request, snl_i+1, sites[i]->lat, sites[i]->lon );
            request += strlen( request );
            snl_i++;
        }
    }

    /* Add icons for facilities
    **************************/
    cap = dam_close_count>damMapLimit ? damMapLimit : dam_close_count;
    if( Debug )
        logit( "t", "ewhtmlemail: dam count to use %d \n", cap);
    for ( i=0; i<cap; i++ )
    {
        snprintf( request, MAX_GET_CHAR, "%s%s_1-%d,%1.1f,%1.1f|",
                  request, dam_order[i]->station[0] ? "blue" : "red",
                  i+1, dam_order[i]->lat, dam_order[i]->lon);
        request += strlen( request );
    }
    /* End of the map
    ********************/
    snprintf( request, MAX_GET_CHAR, "%s\"/>\n", request );
    request += strlen( request );

    /* Add the legend
     ****************/
    snprintf( request, MAX_GET_CHAR, "<table border='1' cellspacing='1' cellpadding='3' width='600px'>"
              "<tr>"
              "<td><img src='http://love.isti.com/~scott/LASSO/hypocenter.png'> Event</td>"
              "<td><img src='http://www.mapquestapi.com/staticmap/geticon?uri=poi-red_1.png'> Uninstrumented Facility</td>"
              "<td><img src='http://www.mapquestapi.com/staticmap/geticon?uri=poi-blue_1.png'> Instrumented Facility</td>"
              "<td><img src='http://www.mapquestapi.com/staticmap/geticon?uri=poi-pink_1.png'> Station</td></tr>"
              "</table>" );
    request += strlen( request );
    if( Debug )
        logit( "t", "ewhtmlemail: mapquest map processing completed \n");
}

/*******************************************************************
 * createGifPlot:
 * 	plots data as GIF
 * 	formats chart string to reference created GIF plot
 *
 * 	returns 0 if successful, -1 otherwise
 *
 ******************************************************************/
int createGifPlot(char *chartreq, HypoArc *arc, char *buffer, int bsamplecount,
				SITE *waveSite, double starttime, double endtime, char phaseName[2])
{
	int BACKCOLOR, TRACECOLOR;
	gdImagePtr gif;
	char giffilename[256];
	char contentID[256];
	FILE *giffile;
	unsigned char *gifbuffer, *base64buf;
	size_t gifbuflen, base64buflen;

    /* Produce GIF image
     *******************/
    gif = gdImageCreate( TraceWidth, TraceHeight ); // Creates base image

	// Set colors for the trace and pick
    BACKCOLOR = gdImageColorAllocate( gif, 255, 255, 255 ); // White
    TRACECOLOR = gdImageColorAllocate( gif, 0, 0, 96 );     // Dark blue

    /* Plot trace
     ************/
    if( trbf2gif( buffer, bsamplecount, gif, starttime, endtime, TRACECOLOR, BACKCOLOR ) == 1 )
    {
    	if( Debug ) logit( "o", "Created gif image for trace from %s.%s.%s.%s\n",
						waveSite->name, waveSite->comp, waveSite->net, waveSite->loc );
    }
    else
    {
    	logit( "e", "gmewhtmlemail: Unable to create gif image for %s.%s.%s.%s\n",
						waveSite->name, waveSite->comp, waveSite->net, waveSite->loc );
        return -1;
    }

    /* Open gif file */
    snprintf( giffilename, 256, "%s_%ld_%s.%s.%s.%s_%s.gif", HTMLFile, arc->sum.qid,
					waveSite->name, waveSite->comp, waveSite->net, waveSite->loc, phaseName );
    giffile = fopen( giffilename, "wb+" );
    if (giffile == NULL) {
    	logit( "e", "gmewhtmlemail: Unable to create gif image for %s.%s.%s.%s as a file: %s\n",
						waveSite->name, waveSite->comp, waveSite->net, waveSite->loc, giffilename );
        return -1;
    }

    /* Save gif to file */
    gdImageGif( gif, giffile );

    if( !UseBlat )
    {
    	/* The gif must be converted to base64 */

        /* Rewind stream */
        rewind( giffile );

        /* Allocate memory for reading from file */
        gifbuflen = ( size_t )( gif->sx * gif->sy );
        gifbuffer = ( unsigned char* ) malloc( sizeof( unsigned char ) * gifbuflen );

        /* Read gif from file */
        gifbuflen = fread( gifbuffer, sizeof( unsigned char ), gifbuflen, giffile );

        /* Encode to base64 */
        base64buf = base64_encode( (size_t *) &base64buflen, gifbuffer, gifbuflen );
        base64buf[base64buflen] = 0;

        /* Free gif buffer */
        free( gifbuffer );

        sprintf( chartreq,
						"<img class=\"MapClass\" alt=\"Waveform\" src=\"data:image/gif;base64,%s\">",
                         base64buf );

        /* Free base64 buffer */
        free( base64buf );
    }
    else
    {
    	//TODO: Investigate blat attachment references
        /* Create img request */
        sprintf( chartreq,
						"<img class=\"MapClass\" alt=\"\" src=\"cid:%s\">",
                        contentID );
    }
    /* Close gif file */
    fclose( giffile );

    // free gif from memory
    gdImageDestroy(gif);
	return 0;
}

/**********************************************************************************************
 * writePlotRequest:
 *	formats request to Flask EW plotter given a minimum of SNCL, starttime and duration 
 *	of trace to be plotted
 *
 *	writes formatted request into the string buffer 'plotRequest'
 *	returns 0 if successful, -1 otherwise
 *********************************************************************************************/
int writePlotRequest(char *plotRequest, size_t requestLen, long eventId, char *address, char *port,
					char *network, char *station, char *location, char *channel,
					time_t startTime, int duration)
{
	struct tm *tPtr;
	char startTimeStr[20];
	char requestURL[250];		/* Buffer to hold request for plot from EWAVE */
	int formatStringLen = 0;
	char conversion_SNCL[20] = {0};
	double conversionFactor = 0.0;
	char unitLabel[20] = {0};
	int i;

	tPtr = gmtime(&startTime);
	strftime(startTimeStr, 20, "%FT%T", tPtr);

	if ((address == NULL) || (port == NULL)) {
		return -1;
	}

	//Look up conversion factor:
	snprintf(conversion_SNCL, 20, "%s.%s.%s.%s", network, station, channel, location);
	if (Debug) logit( "et", "looking up conversion factor for %s\n", conversion_SNCL);
	for (i = 0; i<MAX_STREAMS && (strlen(ConversionFactors[i].SNCL) > 0); i++) {
		if ( strncmp(ConversionFactors[i].SNCL, conversion_SNCL, 20) == 0 ) {
			conversionFactor = ConversionFactors[i].convFactor;
			if (Debug) logit("et", "conversion factor found for %s: %lf\n", conversion_SNCL, conversionFactor);
			break;
		}	
	}

	if (conversionFactor == 0.0) {
		conversionFactor = 1.0;
		strcpy(unitLabel, "counts");
	} else {
		//transform factor in order to scale from counts to acceleration in cm/s^2
		conversionFactor = (1.0)/(conversionFactor);
		strcpy(unitLabel, "cm/s/s");	
	}

	formatStringLen = snprintf(requestURL, 250,
					"http://%s:%s/ewave/query?net=%s&sta=%s&cha=%s&loc=%s&start=%s&dur=%d&displayMaxValue=True&scaleFactor=%.17g&units=%s",
					address, port, network, station, channel, location, startTimeStr, duration, conversionFactor, unitLabel);
	if ( formatStringLen <= 0 ) {
		if (Debug) logit( "et", "Unable to format request to EWAVE\n");
		return -1;
	}
	formatStringLen = snprintf(plotRequest, requestLen,
				"<div style=\"float:left;overflow:hidden\">\n"
				"\t<img alt=\"Trace\" title=\"Trace\" style=\"margin:0px -2px\""
				"src=\"%s\"/>\n</div>\n", requestURL);

	if ( formatStringLen <= 0 ) {
		if (Debug) logit( "et", "failed to write image file name\n");
		return -1;
	}

	return 0;
}

/*******************************************************************************
 * makeGoogleChart: Produce a google chart request for a given station  *
 ******************************************************************************/
/*
* Input Parameters
* chartreq:        String to save the request in
* samples:         Array with the samples
* samplecount:     Number of samples
* phaseName:       A character to indicate the name of the phase (P or S)
* phasePos:        The relative position of the phase label 0%-100%
* tracewidth:      Width of the google trace
* traceheight:     Height of the google trace
*/
int makeGoogleChart( char* chartreq, int *samples, int samplecount,
                     char * phasename, double phasepos, int tracewidth, int traceheight, double *maxmin )
{
    int i;
    double avg, max=0, min=0;
    char reqHeader[MAX_GET_CHAR];
    char reqSamples[MAX_GET_CHAR];
    int cursample;              /* Position of the current sample */
    int cursampcount;           /* Number of samples in the current image */
    int curimgwidth;            /* Width of the current image */
    int accimgwidth;            /* Accumulated width of the composite image */
    int ntraces;                /* Number of traces that will be required */
    double cursamppos, nxtsamppos;
    int max_index;
    int min_index;
    char maxMarker[100] = {0};
    char rangeMarker[100] = {0};
    double max_pct;


    // Average value of the samples
    avg = 0;
    for( i = 0; i < samplecount; i++ ) {
        if ( i==0 || max < samples[i] ) {
            max = samples[i];
            max_index = i;
        }
        if ( i==0 || min > samples[i] ) {
            min = samples[i];
            min_index = i;
        }
        avg += samples[i];
    }
    avg /= ( double ) samplecount;
    // fix for if max is negative swing....
    if (fabs(min-avg) > fabs(max-avg)) {
        max_index = min_index;
    }


    /* Instead of sending a single image, the trace will be split
     * in multiple images, each with its separate request and a
     * maximum of MAX_REQ_SAMPLES samples
     ************************************************************/

    ntraces = ( ( int )( samplecount %  MAX_REQ_SAMP ) == 0 )?
              ( ( int )( samplecount / MAX_REQ_SAMP ) ) :
              ( ( int )( samplecount / MAX_REQ_SAMP ) + 1 );
    if( Debug )
        logit( "o", "ewhtmlemail: Splitting trace in %d images\n", ntraces );

    cursample = 0;
    accimgwidth = 0;
    chartreq[0] = '\0';
    while( cursample < samplecount )
    {

        /* Determine how many samples will be used in this image
         *******************************************************/
        cursampcount = ( ( cursample + MAX_REQ_SAMP ) > samplecount )?
                       ( samplecount - cursample ) : MAX_REQ_SAMP;


        /* Compute width of the image
         ****************************/
        curimgwidth = ( int )( ( double )( cursample + cursampcount ) /
                               ( double ) samplecount * ( double ) tracewidth ) - accimgwidth;
        accimgwidth += curimgwidth;     // update accumulated image width


        /* Determine range markers
         ***********************/
        if ( cursample==0 || cursample+cursampcount >= samplecount ) {
            if ( cursample==0 && cursample+cursampcount >= samplecount ) 
                sprintf( rangeMarker, "chm=@t%1.2f,000000,0,0:1,10|@t%1.2f,000000,0,0:0,10|@t%1.2f,000000,0,1:1,10|@t%1.2f,000000,0,1:0,10&", maxmin[0], maxmin[1], maxmin[0], maxmin[1] );
            else
                sprintf( rangeMarker, "chm=@t%1.2f,000000,0,%d:1,10|@t%1.2f,000000,0,%d:0,10&", maxmin[0], cursample==0 ? 0 : 1, maxmin[1], cursample==0 ? 0 : 1 );
        } else {
            rangeMarker[0] = 0;
        }

        /* Determine background
         ***********************/
        if ( !showMax )
            maxMarker[0] = 0;
        else if ( max_index < cursample ) {
            // Max is left of this segment => background is <AFTER_MAX_COLOR>
            sprintf( maxMarker, "chf=bg,ls,0,%s&", AFTER_MAX_COLOR );
        } else if ( max_index >= cursample+cursampcount ) {
            // Max is right of this segment => background is <BEFORE_MAX_COLOR>
            sprintf( maxMarker, "chf=bg,ls,0,%s&", BEFORE_MAX_COLOR );
        } else {
            // Max is within this segment => background is split
            max_pct = (max_index - cursample) / (1.0 * cursampcount);
            sprintf( maxMarker, "chf=bg,ls,0,%s,%f,%s,%f&", 
                BEFORE_MAX_COLOR, max_pct, AFTER_MAX_COLOR, 1.0);
        }

        /* Create img request with cursampcount samples
         *************************************************/
        cursamppos = ( double ) cursample / ( double ) samplecount * 100.0;
        nxtsamppos = ( double )( cursample + cursampcount ) /
                     ( double ) samplecount * 100.0;
        if( phasepos >= cursamppos && phasepos < nxtsamppos )
        {
            /* Get image with phase label
             ****************************/
            snprintf( reqHeader, MAX_GET_CHAR,
                      "<div style=\"float:left;overflow:hidden\">\n"
                      "\t<img alt=\"\" style=\"margin:0px -2px\""
                      "src=\"http://chart.apis.google.com/chart?"
                      "chs=%dx%d&"
                      "cht=ls&"
                      "chxt=x&"
                      "chxl=0:|%s&"
                      "chxp=0,%3.1f&"
                      "chxtc=0,-600&"
                      "chco=000060&"
                      "chma=0,0,0,0&"
                      "chls=1&"               // Linewidth
                      "%s"                    // Place for Max marker: chf=bg,ls,0,F0F0F0,0.883333,FFFFFF,0.116667 or chf=bg,s,F0F0F0
                      "%s"                    // Place for Range Labels, if first or last subplot
                      "chd=s:",
                      curimgwidth + 4, traceheight,
                      phasename, 
                      ( phasepos - cursamppos ) / ( nxtsamppos - cursamppos ) * 100.0,
                      maxMarker, rangeMarker );
        }
        else
        {
            /* Get image with no label
             *************************/
            snprintf( reqHeader, MAX_GET_CHAR,
                      "<div style=\"float:left;overflow:hidden\">\n"
                      "\t<img alt=\"\" style=\"margin:0px -2px\""
                      "src=\"http://chart.apis.google.com/chart?"
                      "chs=%dx%d&"
                      "cht=ls&"
                      "chxt=x&"
                      "chxl=0:|&"
                      "chxp=0,&"
                      "chxtc=0,-600&"
                      "chco=000060,006000&"
                      "chma=0,0,0,0|0,006000,1,0&"
                      "chls=1|1&"               // Linewidth
                      "%s"                    // Place for Max marker: chf=bg,ls,0,F0F0F0,0.883333,FFFFFF,0.116667 or chf=bg,s,F0F0F0
                      "%s"                    // Place for Range Labels, if first or last subplot
                      "chd=s:",
                      curimgwidth + 4, traceheight,
                      maxMarker, rangeMarker);
        }

        /* Create samples
         ****************/
        for( i = 0; i < MAX_GET_CHAR; i++ ) reqSamples[i] = 0;
        for( i = cursample; i < ( cursample + cursampcount ); i++ ) {
              reqSamples[i - cursample] = simpleEncode( ( int )( ( double ) samples[i] - avg + 30.5 ) );
        }
        cursample += cursampcount;

        /* Add image and samples to final request
         ****************************************/
        strncat( chartreq, reqHeader, MAX_GET_CHAR );
        strncat( chartreq, reqSamples, MAX_GET_CHAR );


        /* Terminate img tag
         *******************/
        strncat( chartreq, "\"/>\n</div>\n", MAX_GET_CHAR );

    }

    return 1;
}


/*******************************************************************************
 * simpleEncode: This is a simple integer encoder based on googles function    *
 *******************************************************************************/
char simpleEncode(int i) {
    char base[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    /* truncate input
    ****************/
    if (i<0)
        i=0;
    if (i>61)
        i=61;


    return base[i];
}


/*******************************************************************************
 * searchSite: To replace site index, which does not seem to work well         *
 *******************************************************************************/
int searchSite(char *S, char *C, char *N, char *L) {
    int i;

    for (i=0; i<nSite; i++)
    {
        if (
            strcmp(S,Site[i].name)==0 &&
            strcmp(C,Site[i].comp)==0 &&
            strcmp(N,Site[i].net)==0 &&
            strcmp(L,Site[i].loc)==0 )
            return i;
    }

    return -1;
}


/****************************************************************************************
 * getWStrbf: Retrieve a set of samples from the waveserver and store the raw tracebuf2 *
 *            in a buffer                                                               *
 ****************************************************************************************/
int getWStrbf( char *buffer, int *buflen, SITE *site,
               double starttime, double endtime )
{
    WS_MENU_QUEUE_REC   menu_queue;
    TRACE_REQ           trace_req;
    int                 wsResponse;
    int                 atLeastOne;
    int                 i;
    char                WSErrorMsg[80];
    
    /* Initialize menu queue
     ***********************/
    menu_queue.head = NULL;
    menu_queue.tail = NULL;

    atLeastOne = 0;

    /* Make menu request
    *******************/
    for( i = 0; i < nwaveservers; i++ )
    {
        wsResponse = wsAppendMenu(
                         waveservers[i].wsIP, waveservers[i].port,
                         &menu_queue, wstimeout );
        if( wsResponse != WS_ERR_NONE )
        {
            logit( "et", "gmewhtmlemail: Cannot contact waveserver %s:%s - %d ",
                   waveservers[i].wsIP, waveservers[i].port, wsResponse );
            logit( "et", "%s\n",
                   getWSErrorStr(wsResponse, WSErrorMsg ) );
            continue;
        }
        else
        {
            atLeastOne++;
        }
    }
    if( atLeastOne == 0 )
    {
        logit( "et", "gmewhtmlemail: Unable to contact any waveserver.\n");
        return -1;
    }


    /* Make request structure
    ************************/
    strcpy( trace_req.sta, site->name );
    strcpy( trace_req.chan, site->comp );
    strcpy( trace_req.net, site->net );
    strcpy( trace_req.loc, site->loc );
    trace_req.reqStarttime = starttime;
    trace_req.reqEndtime = endtime;
    trace_req.partial = 1;
    trace_req.pBuf = buffer;
    trace_req.bufLen = *buflen;
    trace_req.timeout = wstimeout;
    trace_req.fill = 0;


    /* Pull data from ws
     *******************/
    wsResponse = wsGetTraceBinL( &trace_req, &menu_queue, wstimeout );
    if( wsResponse != WS_ERR_NONE )
    {
        logit( "et", "gmewhtmlemail: Error loading data from waveserver - %s\n",
               getWSErrorStr(wsResponse, WSErrorMsg));
        wsKillMenu(&menu_queue);
        return -1;
    }

    /* Update output number of bytes
     *********************************/
    *buflen = (int)trace_req.actLen;


    /* Terminate connection
     **********************/
    wsKillMenu(&menu_queue);

    return 1;
}


char* getWSErrorStr(int errorCode, char* msg) {
    switch (errorCode)
    {
    case 1:
        strcpy(msg,"Reply flagged by server");
        return msg;
    case -1:
        strcpy(msg,"Faulty or missing input");
        return msg;
    case -2:
        strcpy(msg,"Unexpected empty menu");
        return msg;
    case -3:
        strcpy(msg,"Server should have been in menu");
        return msg;
    case -4:
        strcpy(msg,"SCNL not found in menu");
        return msg;
    case -5:
        strcpy(msg,"Reply truncated at buffer limit");
        return msg;
    case -6:
        strcpy(msg,"Couldn't allocate memory");
        return msg;
    case -7:
        strcpy(msg,"Couldn't parse server's reply");
        return msg;
    case -10:
        strcpy(msg,"Socket transaction timed out");
        return msg;
    case -11:
        strcpy(msg,"An open connection was broken");
        return msg;
    case -12:
        strcpy(msg,"Problem setting up socket");
        return msg;
    case -13:
        strcpy(msg,"Could not make connection");
        return msg;
    }
    //Unknown error
    return NULL;
}






/****************************************************************************************
 * trbufresample: Resample a trace buffer to a desired sample rate. Uses squared low    *
 *            filtering and oversampling                                                *
 ****************************************************************************************/
int trbufresample( int *outarray, int noutsig, char *buffer, int buflen,
                   double starttime, double endtime, int outamp, double *maxmin )
{
    TRACE2_HEADER   *trace_header,*trace;       // Pointer to the header of the current trace
    char            *trptr;             // Pointer to the current sample
    double          inrate;             // Initial sampling rate
    double          outrate;            // Output rate
    int             i;                  // General purpose counter
    double          s;                  // Sample
    double          *fbuffer;           // Filter buffer
    double          nfbuffer;           // length of the filter buffer
    double          *outsig;            // Temporary array for output signal
    double          outavg;             // Compute average of output signal
    double          outmax;             // Compute maximum of output signal
    double          *samples;           // Array of samples to process
    int             nsamples = 0;       // Number of samples to process
    int             samppos = 0;        // Position of the sample in the array;
    double          sr;                     // buffer sample rate
    int             isBroadband;        // Broadband=TRUE - ShortPeriod = FALSE
    int             flag;
    double          y;              // temp int to hold the filtered sample
    RECURSIVE_FILTER rfilter;     // recursive filter structure
    int             maxfilters=20;
    int             retval;
    double          realMax, realMin;
    int             minMaxStarted = 0;


    /* Reserve memory for temporary output signal
     ********************************************/
    outsig = ( double* ) malloc( sizeof( double ) * noutsig );
    if( outsig == NULL )
    {
        logit( "et", "gmewhtmlemail: Unable to reserve memory for resampled signal\n" );
        return -1;
    }


    /* Reserve memory for input samples
     **********************************/
    // RSL note: tried the serialized option but this is better
    samples = ( double* ) malloc( sizeof( double ) * MAX_SAMPLES );
    if( samples == NULL )
    {
        logit( "et", "gmewhtmlemail: Unable to allocate memory for sample buffer\n" );
        return -1;
    }
    for( i = 0; i < MAX_SAMPLES; i++ ) samples[i] = 0.0;

    /* Isolate input samples in a single buffer
     ******************************************/
    trace_header = (TRACE2_HEADER*)buffer;
    trptr = buffer;
    outavg = 0; // Used to initialize the filter
    if(SPfilter)
    {
        trace = (TRACE2_HEADER*)buffer;
        sr =trace->samprate; /*passing the sample rate from buffer header*/
        /*check if this channel is a broadband one or SP
         * This simply check out the first two channel characters in order to conclude
         * whether it is a broadband (BH) or shorperiod (SP)*/
        switch(trace->chan[0])
        {
        case 'B':
            isBroadband = TRUE;
            break;
        case 'H':
            isBroadband = TRUE;
            break;
        default:
            isBroadband = FALSE;
            break;
        }
    }
    while( ( trace_header = ( TRACE2_HEADER* ) trptr ) < (TRACE2_HEADER*)(buffer + buflen) )
    {
        // If necessary, swap bytes in tracebuf message
        retval = WaveMsg2MakeLocal( trace_header );
        if ( retval < 0 )
        {
            logit( "et", "gmewhtmlemail(trbufresample): WaveMsg2MakeLocal error. (%d)\n", retval );
            return -1;
        }

        // Update sample rate
        inrate = trace_header->samprate;

        // Skip the trace header
        trptr += sizeof( TRACE2_HEADER );

        for( i = 0; i < trace_header->nsamp; i++ )
        {
            // Produce integer sample
            if( strcmp( trace_header->datatype, "i2" ) == 0 ||
                    strcmp(trace_header->datatype, "s2")==0 )
            {
                s = ( double ) (*((short*)(trptr)));
                trptr += 2;
            }
            else
            {
                s = ( double )( *( ( int* )( trptr ) ) );
                trptr += 4;
            }
            if ( minMaxStarted ) {
                if ( s < realMin )
                    realMin = s;
                if ( s > realMax )
                    realMax = s;
            } else {
                realMax = realMin = s;
                minMaxStarted = 1;
            }

            // Compute position of the sample in the sample array
            samppos = ( int )( ( trace_header->starttime
                                 + ( double ) i / trace_header->samprate - starttime ) *
                               trace_header->samprate );

            if( samppos < 0 || samppos >= MAX_SAMPLES )
                continue;

            // Store sample in array
            samples[samppos] = s;
            if( samppos > nsamples ) nsamples = samppos;
            outavg += s;
        }
    }
    maxmin[0] = realMax;
    maxmin[1] = realMin;

    if(SPfilter && isBroadband)
    {
        if( (flag=initAllFilters(maxfilters)) != EW_SUCCESS)
        {
            logit("et","gmewhtmlemail: initAllFilters() cannot allocate Filters; exiting.\n");
        }
        if( (flag=initTransferFn()) != EW_SUCCESS)
        {
            logit("et","gmewhtmlemail: initTransferFn() Could not allocate filterTF.\n");
        }
        if(flag==EW_SUCCESS)
        {
            switch (flag=initChannelFilter(sr, outavg/(double)nsamples,isBroadband,&rfilter, maxfilters))
            {
            case  EW_SUCCESS:

                if (Debug)
                    logit("et","gmewhtmlemail: Pre-filter ready for channel %s:%s:%s:%s\n",
                          trace->sta,trace->chan,trace->net,trace->loc);
                break;

            case EW_WARNING:

                logit("et","Unable to obtain a find an existing pre-filter or create a new (%s) pre-filter for sample rate %f; setting channel %s:%s:%s:%s bad\n",
                      (isBroadband ? "broadband" : "narrowband"),trace->samprate,
                      trace->sta,trace->chan,trace->net,trace->loc);
                break;

            case EW_FAILURE:

                logit("et",  "gmewhtmlemail Parameter passed in NULL; setting channel %s:%s:%s:%s bad\n",
                      trace->sta,trace->chan,trace->net,trace->loc);
                break;

            default:

                logit("et","Unknown error in obtaining a pre-filter for sample rate %f; setting channel %s:%s:%s:%s bad\n",
                      trace->samprate, trace->sta,trace->chan,trace->net,trace->loc);
                break;
            }
        }
        if( flag == EW_SUCCESS )
        {   /*The channel filters function went ok*/

            outavg = 0;
            for(i=0; i<nsamples; i++) {
                y=filterTimeSeriesSample(samples[i],&rfilter);
                samples[i]=y;
                outavg += (double)samples[i];
            }
            // Remove average from samples and normalize
            outavg /= (double)nsamples;
            /*reset the transfer function*/
            freeTransferFn();
        }
        else
        {
            /*Something was wrong and it is not possible to filter the samples*/
            outavg /= nsamples;//move this part
        }

    }
    else
    {   /*it was not selected SPfilter*/
        outavg /= nsamples;
    }

    /* Prepare filter buffer
     ***********************/
    outrate = ( double ) noutsig / ( endtime - starttime ); // Output sample rate
    nfbuffer = inrate / outrate; // Length of the filter buffer

    /* Check if filtering is required
     ********************************/
    if( nfbuffer > 1.5 )
    {
        int nb = ( int )( nfbuffer + 0.5 );
        if( Debug ) logit( "o", "Filtering is required\nFilter settings:  "
                               "Input rate: %f    Output rate: %f    Buffer length: %f\n",
                               inrate, outrate, nfbuffer );
        fbuffer = ( double* ) malloc( sizeof( double ) * nb );
        if( fbuffer == NULL )
        {
            logit( "et", "gmewhtmlemail: Unable to allocate memory for filter buffer\n" );
            return -1;
        }
        for( i = 0; i < nb; i++ )
            fbuffer[i] = outavg; // Initialize filter with average of signal

        /* Filter signal with array buffer
         *********************************/
        for( i = 0; i < nsamples; i++ )
            samples[i] = fbuffer_add( fbuffer, nb, samples[i] );

        /* Done with filter buffer
         *************************/
        free( fbuffer );
    }
    else
    {
        if( Debug ) logit( "o", "Filtering is not required.\n" );
    }

	/* Resampling
     * This will be based on finding the sample to place
     * at each point of the output array. Allows also upsampling
     * if necessary
     ***********************************************************/
    if( Debug ) logit( "o", "Resampling at a rate of %f\n",
                           inrate / outrate );
    outavg = 0;
    for( i = 0; i < noutsig; i++ )
    {
        /* for the i-th sample of the output signal, calculate
         * the position at the input array
         *****************************************************/
        samppos = ( int )( ( double ) i * inrate / outrate + 0.5 );


        /* check if position is valid
         ****************************/
        if( samppos >= nsamples )
            continue; // Invalid position

        /* store sample
         **************/
        outsig[i] = samples[samppos];
        outavg += outsig[i]; // update average
    }
    outavg /= noutsig;

    /* Remove average of output signal
     *********************************/
    outmax = 0;
    for( i = 0; i < noutsig; i++ )
    {
        // only remove the average if the data are not in a gap (value = 0.0)
        //                 // otherwise we get the dreaded honeycomb effect
        if (outsig[i] != 0.0)
        {
              outsig[i] = outsig[i] - outavg; // remove average
        }

        // Compute maximum value
        if( outsig[i] > outmax ) outmax = outsig[i];
        if( outsig[i] < -outmax ) outmax = -outsig[i];
    }

    for(i = 0; i < noutsig; i++ )
    {
        outarray[i] = ( int ) ( outsig[i] / outmax * ( double ) outamp + 0.5 );
    }

    // free memory
    free( outsig );
    free( samples );
    return 1;

}

/* Add a new sample to the buffer filter and compute output */
double fbuffer_add( double *fbuffer, int nfbuffer, double s )
{
    int i;
    double out = 0;

    // circulate buffer
    for( i = 1; i < nfbuffer; i++ )
        fbuffer[i - 1] = fbuffer[i];

    // add sample to buffer
    fbuffer[nfbuffer - 1] = s;

    // compute filter output by averaging buffer
    for( i = 0; i < nfbuffer; i++ )
        out += fbuffer[i];
    out /= ( double ) nfbuffer;

    return out;
}



/****************************************************************************************
 * trb2gif: Takes a buffer with tracebuf2 messages and produces a gif image with the    *
 *          corresponding trace                                                         *
 ****************************************************************************************/
int trbf2gif( char *buf, int buflen, gdImagePtr gif, double starttime, double endtime,
              int fcolor, int bcolor )
{
    TRACE2_HEADER   *trace_header,*trace;   // Pointer to the header of the current trace
    char            *trptr;         // Pointer to the current sample
    double          sampavg;        // To compute average value of the samples
    int         nsampavg=0;     // Number of samples considered for the average
    double          sampmax;        // To compute maximum value
    int             i;              // General purpose counter
    gdPointPtr      pol;            // Polygon pointer
    int             npol;           // Number of samples in the polygon
    double          xscale;         // X-Axis scaling factor
    double          dt;             // Time interval between two consecutive samples
    double      sr;             // to hold the sample rate obtained through the trace_header struc.
    int             isBroadband;    // Broadband= TRUE - ShortPeriod = FALSE
    int             flag;
    int             y;              // temp int to hold the filtered sample
    RECURSIVE_FILTER rfilter;     // recursive filter structure
    int             maxfilters=20;      //Number of filters fixed to 20.

    // Reserve memory for polygon points //TODO Make this dynamic
    pol = ( gdPointPtr ) malloc( MAX_POL_LEN * sizeof( gdPoint ) );
    if( pol == NULL )
    {
        logit( "et", "trbf2gif: Unable to reserve memory for plotting trace.\n" );
        return -1;
    }
    npol = 0;

    // Initialize trace pointer
    trace_header = (TRACE2_HEADER*)buf;
    trptr = buf;

    // Initialize maximum counter to minimum integer
    sampmax = 0;
    sampavg = 0;

    // Compute x-axis scale factor in pixels/second
    xscale = ( double ) gif->sx / ( endtime - starttime );

    // Cycle to process each trace an draw it on the gif image
    while( ( trace_header = ( TRACE2_HEADER* ) trptr ) < (TRACE2_HEADER*)(buf + buflen) )
    {
        // Compute time between consecutive samples
        dt = ( trace_header->endtime - trace_header->starttime ) /
             ( double ) trace_header->nsamp;

        // If necessary, swap bytes in tracebuf message
        if ( WaveMsg2MakeLocal( trace_header ) < 0 )
        {
            logit( "et", "trbf2gif(trb2gif): WaveMsg2MakeLocal error.\n" );
            free( pol );
            return -1;
        }

        // Store samples in polygon
        // This procedure saves all the samples of this trace on the polygon
        // It also stores the maximum value and average values of the samples
        // for correcting the polygon later

        // Skip the trace header
        trptr += sizeof( TRACE2_HEADER );
        if(SPfilter)
        {
            trace = (TRACE2_HEADER*)buf;
            sr = trace->samprate; /*passing the sample rate from buffer header*/
            /*check if this channel is a broadband one or SP
             * This simply check out the first two channel characters in order to conclude
             * whether it is a broadband (BH) or shorperiod (SP)*/
            switch(trace->chan[0])
            {
            case 'B':
                isBroadband = TRUE;
                break;
            case 'H':
                isBroadband = TRUE;
                break;
            default:
                isBroadband = FALSE;
                break;
            }
        }
        // Process samples
        if( strcmp( trace_header->datatype, "i2" ) == 0 ||
                strcmp(trace_header->datatype, "s2")==0 )
        {
            // Short samples
            for( i = 0; i < trace_header->nsamp; i++, npol++, trptr += 2 )
            {
                // Compute x coordinate
                pol[npol].x = ( int )( ( trace_header->starttime - starttime +
                                         ( double ) i * dt ) * xscale );

                // Compute raw y coordinate without scaling or de-averaging
                pol[npol].y = ( int ) (*((short*)(trptr)));

                // Add sample to average counter
                sampavg += ( double ) pol[npol].y;
                nsampavg++; // Increments number of samples to consider for average

                // Consider sample for maximum (absolute) value
                if( pol[npol].y > sampmax || pol[npol].y < -sampmax ) sampmax = pol[npol].y;
            }
        }
        else if( strcmp( trace_header->datatype, "i4" ) == 0 ||
                 strcmp(trace_header->datatype, "s4")==0 )
        {
            // Integer samples
            for( i = 0; i < trace_header->nsamp; i++, npol++, trptr += 4 )
            {
                // Compute x coordinate
                pol[npol].x = ( int )( ( trace_header->starttime - starttime +
                                         ( double ) i * dt ) * xscale );

                // Compute raw y coordinate without scaling or de-averaging
                pol[npol].y = *((int*)(trptr));

                // Add sample to average counter
                sampavg += ( double ) pol[npol].y;
                nsampavg++; // Increments number of samples to consider for average

                // Consider sample for maximum (non-absolute) value
                if( pol[npol].y > sampmax || pol[npol].y < -sampmax ) sampmax = pol[npol].y;
            }
        }
        else
        {
            // Unknown type of samples
            logit( "et", "trbf2gif: Unknown type of samples\n" );
            free( pol );
            return -1;
        }
        // At this point, the polygon is populated with samples from this trace
        // The sampmax, sampavg and nsampavg variables are also updated but have not
        // yet been applied to correct the polygon
    } // End of trace cycle

    /*Filtering part*/
    if(SPfilter && isBroadband)
    {
        /*Starting the FILTER structures handle by the ioc_filter lib*/
        if((flag=initAllFilters(maxfilters)) != EW_SUCCESS)
        {
            logit("et","gmewhtmlemail: initAllFilters() cannot allocate Filters; exiting.\n");
        }
        if( (flag=initTransferFn()) != EW_SUCCESS)
        {
            logit("et","gmewhtmlemail: initTransferFn() Could not allocate filterTF.\n");
        }
        if(flag==EW_SUCCESS)
        {
            switch (flag=initChannelFilter(sr, sampavg/(double)nsampavg, isBroadband, &rfilter, maxfilters))
            {
            case  EW_SUCCESS:

                if (Debug)
                    logit("et","gmewhtmlemail: Pre-filter ready for channel %s:%s:%s:%s\n",
                          trace->sta,trace->chan,trace->net,trace->loc);
                break;

            case EW_WARNING:

                printf("Unable to obtain a find an existing pre-filter or create a new (%s) pre-filter for sample rate %f; setting channel %s:%s:%s:%s bad\n",
                       (isBroadband ? "broadband" : "narrowband"),trace->samprate,
                       trace->sta,trace->chan,trace->net,trace->loc);
                break;

            case EW_FAILURE:

                logit("et",  "gmewhtmlemail Parameter passed in NULL; setting channel %s:%s:%s:%s bad\n",
                      trace->sta,trace->chan,trace->net,trace->loc);
                break;

            default:

                printf("Unknown error in obtaining a pre-filter for sample rate %f; setting channel %s:%s:%s:%s bad\n",
                       trace->samprate,trace->sta,trace->chan,trace->net,trace->loc);
                break;
            }
        }
        if( flag == EW_SUCCESS )
        {   /*The channel filters function went ok*/
            sampavg = 0;
            sampmax = 0;
            for(i=0; i<npol; i++) {
                y=(int)(filterTimeSeriesSample((double)pol[i].y,&rfilter));
                pol[i].y=y;
                sampavg+=(double)pol[i].y;
                if( pol[i].y > sampmax || pol[i].y < -sampmax ) sampmax = pol[i].y;
            }
            // Remove average from samples and normalize
            sampavg /=(double)nsampavg;
            sampmax -= sampavg;
            /*freeing the transfer function*/
            freeTransferFn();
        }
        else
        {
            /*Something was wrong and it is not possible to filter the samples*/
            // Remove average from samples and normalize
            sampavg /= ( double ) nsampavg; // Compute final sample average
            sampmax -= sampavg; // Correct sample maximum with average;
        }
    }/*end of filter part*/
    else
    {   /*it was not selected SPfilter*/
        sampavg /= ( double ) nsampavg; // Compute final sample average
        sampmax -= sampavg; // Correct sample maximum with average;
    }
    //printf("Buflen: %d   Avg: %f   Max: %f\n",buflen, sampavg,sampmax);

    /* Correct Average */
    sampavg = 0;
    nsampavg = 0;
    for( i = 0; i < npol; i++ )
    {
        sampavg += ( double ) pol[i].y;
        nsampavg++;
    }
    sampavg /= nsampavg;

    sampmax = 0;
    for( i = 0; i < npol; i++ ) {
        /* Correct Average */
        pol[i].y = ( int )( ( ( double ) pol[i].y - sampavg ) );

        /* Compute new maximum after removing average */
        if( ( double ) pol[i].y > sampmax )
        {
            sampmax = ( double ) pol[i].y;
        }
        else if( ( double ) ( -pol[i].y ) > sampmax )
        {
            sampmax = -( double ) pol[i].y;
        }
    }

    /* Scale image and shift to vertical middle */
    for( i = 0; i < npol; i++ )
    {
        pol[i].y =  ( int ) ( gif->sy / 2 ) -
                    ( int ) ( ( double ) pol[i].y / sampmax * ( double ) gif->sy / 2 );
    }

    // Clear image
    gdImageFilledRectangle( gif, 0, 0, gif->sx, gif->sy, bcolor );

    // Draw poly line
    gdImagePolyLine( gif, pol, npol, fcolor );


    // Free polygon memory
    free( pol );
    return 1;
}

/****************************************************************************************
 * pick2gif: Plots a pick as a vertical line in the image. Includes the phase            *
 ****************************************************************************************/
int pick2gif(gdImagePtr gif, double picktime, char *pickphase,
             double starttime, double endtime, int fcolor)
{
    // Draw vertical line
    int pickpos = ( int ) ( ( double ) gif->sx / ( endtime - starttime ) *
                            ( picktime - starttime ) + 0.5 );
    gdImageLine(gif, pickpos, 0, pickpos, gif->sy, fcolor);

    // Draw phase character
    gdImageChar(gif, gdFontSmall, pickpos - 8, 1, pickphase[0], fcolor);

    return 1;
}

/****************************************************************************************
 * Utility Functions                                                                    *
 ****************************************************************************************/
// Draw a poly line
void gdImagePolyLine( gdImagePtr im, gdPointPtr pol, int n, int c )
{
    int i;
    for( i = 0; i < ( n - 1 ); i++ )
        gdImageLine( im, pol[i].x, pol[i].y, pol[i + 1].x, pol[i + 1].y, c);
}

/***************************************************************************************
 * gmtmap is a function to create a map with GMT, the format of the image
 * is png. gmtmap returns the string with the complementary html code where is added
 * the map. It is based in system calls.
 * Returns 0 if all is all right and -1 if a system call has failed
 * The map projection could be Mercator or Albers.
 * **************************************************************************************/
int gmtmap(char *request,SITE **sites, int nsites,
           double hypLat, double hypLon,int idevt)
{
    char command[MAX_GET_CHAR]; // Variable that contains the command lines to be executed through system() function.
    int i,j;
    /*apply some settings with gmtset for size font and others*/
    if(Debug) logit("ot","gmtmap(): gmtsets applying default fonts for event %d\n",idevt);
    if((j=system("gmtset ANNOT_FONT_SIZE_PRIMARY 8")) != 0)
    {
        logit("et","gmtmap(): Unable to set ANNOT_FONT_SIZE_PRIMARY for event %d\n",idevt);
        logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
        return -1;
    }
    if((j=system("gmtset FRAME_WIDTH 0.2c")) != 0)
    {
        logit("et","gmtmap(): Unable to set FRAME_WIDTH for event %d\n",idevt);
        logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
        return -1;
    }
    if((j=system("gmtset TICK_LENGTH 0.2c")) != 0)
    {
        logit("et","gmtmap(): Unable to set TICK_LENGTH for event %d\n",idevt);
        logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
        return -1;
    }
    if((j=system("gmtset TICK_PEN 0.5p")) != 0)
    {
        logit("et","gmtmap(): Unable to set TICK_PEN for event %d\n",idevt);
        logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
        return -1;
    }
    /*Mercator Projection*/
    if( Mercator )
    {
        if(Debug)logit("ot","gmtmap(): Mercator Projection used for event %d\n",idevt);
        /*creating the map with the hypocentral information. The epicenter will be the center of this
        * map and upper and  lower limits, for lat and lon, will be 4 degrees. Projection is Mercator*/
        if(Debug) logit("et","gmtmap(): creating the basemap for event %d\n",idevt);
        sprintf(command,"pscoast -R%f/%f/%f/%f -Jm%f/%f/0.5i -Df -G34/139/34 -B2 -P -N1/3/255 -N2/1/255/to"
                " -Slightblue -K > %s_%d.ps\n",hypLon-4.0,hypLon+4.0,hypLat-4.0,hypLat+4.0,hypLon,hypLat,HTMLFile,idevt);
        if( (j=system(command)) != 0)
        {
            logit("et","gmtmap(): Unable to create the basemap for event %d\n",idevt);
            logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
            return -1;
        }
    }
    /*Albers Projection*/
    if( Albers )
    {
        if(Debug)logit("ot","gmtmap(): Albers Projection used for event %d\n",idevt);
        /*creating the map with the hypocentral information. The epicenter will be the center of this
        * map and upper and  lower limits, for lat and lon, will be 4 degrees. Albers Projection */
        if(Debug) logit("et","gmtmap(): creating the basemap for event %d\n",idevt);
        sprintf(command,"pscoast -R%f/%f/%f/%f -Jb%f/%f/%f/%f/0.5i -Df -G34/139/34 -B2 -P -N1/3/255 -N2/1/255/to"
                " -Slightblue -K > %s_%d.ps\n",hypLon-4.0,hypLon+4.0,hypLat-4.0,hypLat+4.0,hypLon,hypLat,hypLat-2,hypLat+2,HTMLFile,idevt);
        if( (j=system(command)) != 0)
        {
            logit("et","gmtmap(): Unable to create the basemap for event %d\n",idevt);
            logit("et","gmtmap(): Please verify whether GMT is installed or the GMT bin PATH has been exported\n");
            return -1;
        }
    }
    /*adding names (if StationNames = TRUE) and simbols for stations*/
    if(Debug) logit("et","gmtmap(): adding station location symbol\n");
    for(i=0; i<nsites; i++)
    {
        sprintf(command,"echo %f %f |psxy -R -J -St0.2 -W1/0 -Glightbrown -O -K >> %s_%d.ps",sites[i]->lon,sites[i]->lat,HTMLFile,idevt);
        if((j=system(command)) != 0)
        {
            logit("et","gmtmap(): Unable to add station symbols for station %s\n",sites[i]->name);
            return -1;
        }
        if(StationNames)
        {
            sprintf(command,"echo %f %f 4 0 1 CM %s |pstext -R -J -O -K -Glightbrown >>%s_%d.ps",sites[i]->lon,sites[i]->lat-0.15,sites[i]->name,HTMLFile,idevt);
            if((j=system(command)) != 0)
            {
                logit("et","gmtmap(): Unable to add station name for %s\n",sites[i]->name);
                return -1;
            }
        }
    }
    /*Adding the name of the cities listed in the file Cities*/
    if( strlen(Cities) >= 1 )
    {
        sprintf(command,"cat %s | awk '{print $1,$2-0.1}' | psxy -R -J -Sc0.05  -G255 -O -K >> %s_%d.ps",Cities,HTMLFile,idevt);
        if(Debug) logit("et","gmtmap(): adding a circle to symbolize a city \n");
        if( (j=system(command)) != 0 )
        {
            logit("et","gmtmap(): Unable to set the symbols for cities: event %d\n",idevt);
            return -1;
        }
        sprintf(command,"cat %s | pstext -R -J -O -G255  -S2  -K >> %s_%d.ps",Cities,HTMLFile,idevt);
        if(Debug) logit("et","gmtmap(): adding the names of cities\n");
        if((j=system(command)) != 0)
        {
            logit("et","gmtmap(): Unable to set the cities' names: event %d\n",idevt);
            return -1;
        }
    }
    /*Map legend for a map with Mercator Projection.
     *
     * The difference between this and the next if clause is the legend's size an position into the map*/
    if( strlen(MapLegend) > 1 && Mercator)
    {
        if(Debug) logit("et","gmtmap(): setting the size font to legend\n");
        sprintf(command,"gmtset ANNOT_FONT_SIZE_PRIMARY 6");
        system(command);
        if(Debug) logit("et","gmtmap(): Adding legend to map\n");
        sprintf(command,"pslegend -Dx0.48i/0.53i/0.9i/0.5i/TC -J -R -O -F %s -Glightyellow -K >> %s_%d.ps",MapLegend,HTMLFile,idevt);
        system(command);
    }

    if( strlen(MapLegend) > 1 && Albers )
    {
        if(Debug) logit("et","gmtmap(): setting the size font to legend\n");
        sprintf(command,"gmtset ANNOT_FONT_SIZE_PRIMARY 6.5");
        system(command);
        if(Debug) logit("et","gmtmap(): Adding legend to map\n");
        sprintf(command,"pslegend -Dx0.60i/0.60i/0.9i/0.55i/TC -J -R -O -F %s -Glightyellow -K >> %s_%d.ps",MapLegend,HTMLFile,idevt);
        system(command);
    }
    /*drawing the epicentral symbol. A red start is the symbol*/
    if(Debug) logit("et","gmtmap(): Adding symbol to the epicenter\n");
    sprintf(command,"echo %f %f |psxy -R -J -Sa0.3 -W1/255/0/0 -G255/0/0 -O >> %s_%d.ps",hypLon,hypLat,HTMLFile,idevt);

    if( (j=system(command)) != 0)
    {
        logit("et","gmtmap(): Unable to set the epicentral symbol for event %d\n",idevt);
        return -1;
    }
    if(Debug) logit("et","gmtmap(): Converting ps to png for event %d\n",idevt);
    sprintf(command,"ps2raster %s_%d.ps -A -P -Tg",HTMLFile,idevt);
    if( (j=system(command)) != 0)
    {
        logit("et","gmtmap(): Unable to convert from ps to png: event %d\n",idevt);
        return -1;
    }
    /*Resizing the png image to 500x500*/
    if(Debug) logit("et","gmtmap(): Changing the resolution of the image for event %d\n",idevt);
    sprintf(command,"convert -resize 500X500 %s_%d.png %s_%d.png",HTMLFile,idevt,HTMLFile,idevt);
    if( (j=system(command)) != 0)
    {
        logit("et","gmtmap(): Unable to resize the output file: event %d\n",idevt);
    }

    /* Base of gmt map
    *******************************/
    snprintf(request, MAX_GET_CHAR, "<img class=\"MapClass\" alt=\"\" "
             "src=\"%s_%d.png\"/>",HTMLFile,idevt);
    return 0;
}

/*******************************************************************************
 * Base64 encoder. Based on the suggestion presented on StackOverflow:         *
 * http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c*
 ******************************************************************************/
static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'
                               };
static int  mod_table[] = {0, 2, 1};


unsigned char* base64_encode( size_t* output_length,
                              const unsigned char *data, size_t input_length )
{
    size_t i;
    int j;
    uint32_t    octet_a, octet_b, octet_c, triple;
    unsigned char* encoded_data;

    *output_length = 4 * ((input_length + 2) / 3);
    //*output_length = ( ( input_length - 1 ) / 3 ) * 4 + 4;

    encoded_data = ( unsigned char* ) malloc(
                       *output_length * sizeof( unsigned char ) );
    if( encoded_data == NULL ) return NULL;


    for( i = 0, j = 0; i < input_length; )
    {
        octet_a = (i < input_length) ? data[i++] : 0;
        octet_b = (i < input_length) ? data[i++] : 0;
        octet_c = (i < input_length) ? data[i++] : 0;

        triple = ( octet_a << 0x10 ) + ( octet_b << 0x08 ) + octet_c;

        encoded_data[j++] = encoding_table[( triple >> 3 * 6 ) & 0x3F];
        encoded_data[j++] = encoding_table[( triple >> 2 * 6 ) & 0x3F];
        encoded_data[j++] = encoding_table[( triple >> 1 * 6 ) & 0x3F];
        encoded_data[j++] = encoding_table[( triple >> 0 * 6 ) & 0x3F];
    }
    for ( j = 0; j < mod_table[input_length % 3]; j++ )
        encoded_data[ *output_length - 1 - j ] = '=';

    return encoded_data;
}


/*******************************************************************************
 * Yield distance between (lat1,lon1) and(lat2,lon2) in specified units:       *
 * (M)iles, (K)ilometers, or (N)?
 ******************************************************************************/
#define pi 3.14159265358979323846
double distance(double lat1, double lon1, double lat2, double lon2, char unit) {
    double theta, dist;
    theta = lon1 - lon2;
    dist = sin(deg2rad(lat1)) * sin(deg2rad(lat2)) + cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(deg2rad(theta));
    dist = acos(dist);
    dist = rad2deg(dist);
    dist = dist * 60 * 1.1515;
    switch(unit) {
    case 'M':
        break;
    case 'K':
        dist = dist * 1.609344;
        break;
    case 'N':
        dist = dist * 0.8684;
        break;
    }
    return (dist);
}

/********************************************************************
 *  This function converts decimal degrees to radians               *
 *******************************************************************/
double deg2rad(double deg) {
    return (deg * pi / 180);
}

/********************************************************************
 *  This function converts radians to decimal degrees               *
 *******************************************************************/
double rad2deg(double rad) {
    return (rad * 180 / pi);
}


/*******************************************************************************
 * Read disclaimer text from specified file, store in DisclaimerText           *
 ******************************************************************************/
char* read_disclaimer( char* path ) {
    FILE *fp = fopen( path, "r" );
    long filesize, numread;
    char* temp_text;
    if ( fp == NULL ) {
        logit( "et", "Could not open disclaimer file: %s\n", path );
        return NULL;
    }
    fseek( fp, 0L, SEEK_END );
    filesize = ftell( fp );
    rewind( fp );

    temp_text = malloc( filesize+5 );
    if ( temp_text == NULL ) {
        logit( "et", "Could no allocate space for disclaimer (%s)\n", path );
        fclose( fp );
        return NULL;
    }

    numread = fread( temp_text, filesize, 1, fp );
    if ( numread != 1 ) {
        logit( "et", "Could not read disclaimer from file: %s\n", path );
        free( temp_text );
    }
    fclose( fp );
    return temp_text;
}
