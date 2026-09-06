
/* slink2ew definitions */

#ifndef __SLINK2EW__
#define __SLINK2EW__

#define VERSION "3.0"

#define MAXMESSAGELEN   160     /* Maximum length of a status or error  */
                                /*   message.                           */
#define MAXRINGNAMELEN  28      /* Maximum length of a ring name.       */
                                /* Should be defined by kom.h           */
#define MAXMODNAMELEN   30      /* Maximum length of a module name      */
                                /* Should be defined by kom.h           */
#define MAXADDRLEN      100     /* Length of SeedLink hostname/address  */


/* Function prototypes */

void packet_handler ( char *msrecord, int packet_type,
		      int seqnum, int packet_size );
int  mseed2ewring ( char *msrecord, SHM_INFO *regionOut, SHM_INFO *mregionOut,
		    MSG_LOGO *waveLogo );
void report_status ( MSG_LOGO *pLogo, short code, char * message );
void configure ( char **argvec );
int  proc_configfile ( char *configfile );
static void logit_msg( const char * );
static void logit_err( const char * );
static void usage( const char * );

#endif   /* __SLINK2EW__  */
