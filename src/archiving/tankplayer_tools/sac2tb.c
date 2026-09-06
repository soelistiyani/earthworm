/*
 * Standalone program to read SAC data files and write
 * earthworm TRACE_BUF2 messages.
 * That file can then be made into a tankplayer file using remux_tbuf.
 *
 * Pete Lombard; May 2001
 */

#define VERSION_NUM  "0.0.8 2018-06-26"

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "trace_buf.h"
#include "sachead.h"
#include "swap.h"
#include "time_ew.h"

#define DEF_MAX_SAMPS 100

#define TRIM( s ) { \
                     char *p = s + strlen( s ); \
                     while ( p > s && isspace( p[-1] ) ) { *(--p) = '\0'; } \
                  }

/* Internal Function Prototypes */
static void usage( char * );
static int readSACHdr(FILE *, struct SAChead *);
static double sacRefTime( struct SAChead * );

int main(int argc, char **argv)
{
  TRACE2_HEADER *trh;
  FILE *fp;
  struct SAChead sh;
  char *SACfile;
  char outbuf[MAX_TRACEBUF_SIZ];
  float *seis, *sp;       /* input trace buffer */
  int32_t *lp;
  double sTime, sampInt;
  int arg;
  int swapIsNeeded;
  int i, npts, datalen;
  int max_samps = DEF_MAX_SAMPS;
  const char sac_undef[] = SACSTRUNDEF;
  char net[3];
  char chan[4];
  char loc[3];
  char sta[6];
  int  seisan_chan_fix = 0;
  FILE *fp_out = NULL;
  int appendOutput = 0;
  float multiplier = 1.0, sac_min, sac_max;
  char tb_date[DATESTR23];
  double new_srate = 0.0;

  trh = (TRACE2_HEADER *) outbuf;
  net[0]  = '\0';
  sta[0]  = '\0';
  chan[0] = '\0';
  loc[0]  = '\0';
  SACfile = NULL;
  
  if (argc < 2)
    usage( argv[0] );

  arg = 1;
  while (arg < argc && argv[arg][0] == '-')
  {
    switch(argv[arg][1])
    {
    case 'N':
      arg++;
      strncpy( net, argv[arg], sizeof( net ) );
      net[sizeof( net )-1] = '\0';
      break;
    case 'S':
      arg++;
      strncpy( sta, argv[arg], sizeof( sta ) );
      sta[sizeof( sta )-1] = '\0';
      break;
    case 's':
      arg++;
      new_srate = atof( argv[arg] );
      break;
    case 'C':
      arg++;
      strncpy( chan, argv[arg], sizeof( chan ) );
      chan[sizeof( chan )-1] = '\0';
      break;
    case 'L':
      arg++;
      strncpy( loc, argv[arg], sizeof( loc ) );
      loc[sizeof( loc )-1] = '\0';
      break;
    case 'c':
      seisan_chan_fix = 1;
      break;
    case 'n':
      arg++;
      max_samps = atoi( argv[arg] );
      break;
    case 'm':
      arg++;
      multiplier = atof( argv[arg] );
      break;
    case 'a':
      appendOutput = 1;
      break;
    default:
      usage( argv[0] );
    }
    arg++;
  }
  if (argc - arg  == 1)
  {
#ifdef _WINNT
    usage( argv[0] );
    exit( 1 );
#else
    SACfile = argv[arg];
    arg++;
    fp_out = stdout;
#endif
  } else if ( argc - arg == 2)
  {
    SACfile = argv[arg];
    arg++;
  } else
    usage( argv[0] );
  if ( max_samps < 1 )
  {
    fprintf( stderr, "max-samples is too small (<1): %i\n", max_samps );
    exit( 1 );
  }
  if ( max_samps > sizeof( outbuf ) - sizeof( *trh ) )
  {
    fprintf( stderr, "max-samples is too large (>%i): %i\n",
                     (int) ( sizeof( outbuf ) - sizeof( *trh ) ), max_samps );
    exit( 1 );
  }
  if ( (fp = fopen(SACfile, "rb")) == (FILE *)NULL)
  {
    fprintf(stderr, "%s: error opening %s\n", argv[0], SACfile);
    exit( 1 );
  }
  if ( fp_out == NULL ) 
  {
    fp_out = fopen( argv[arg], appendOutput ? "ab" : "wb" );
    if ( fp_out == (FILE *)NULL )
    {
      fprintf(stderr, "%s: error opening %s\n", argv[0], argv[arg]);
      fclose( fp );
      exit( 1 );
    }
  }

  /* mtheo 2007/10/19 */
  swapIsNeeded = readSACHdr(fp, &sh);
  if ( swapIsNeeded < 0)
  {
    fclose(fp);
    exit( 1 );
  }

  npts = sh.npts;
  datalen = npts * sizeof( *seis );
  seis = malloc( datalen );
  if ( seis == NULL )
  {
    fprintf(stderr, "%s: out of memory for %d SAC samples\n", argv[0], npts);
    exit( 1 );
  }

  if (sh.delta < 0.001)
  {
    fprintf(stderr, "SAC sample period too small: %f\n", sh.delta);
    exit( 1 );
  }

  memset( trh, 0, sizeof( *trh ) );

  if ( net[0] != '\0' ) {
     strncpy( trh->net, net, TRACE2_NET_LEN );
  } else {
     /* get the net code from the SAC file */
     strncpy( trh->net, sh.knetwk, TRACE2_NET_LEN );
  }
  trh->net[TRACE2_NET_LEN-1] = '\0';
  TRIM( trh->net );

  if ( sta[0] != '\0' ) {
    strncpy( trh->sta, sta, TRACE2_STA_LEN );
  } else {
    strncpy(trh->sta, sh.kstnm, TRACE2_STA_LEN);
  }
  trh->sta[TRACE2_STA_LEN-1] = '\0';
  TRIM( trh->sta );

  if ( chan[0] != '\0' ) {
     strncpy( trh->chan, chan, TRACE2_CHAN_LEN );
     trh->chan[TRACE2_CHAN_LEN-1] = '\0';
  } else if ( seisan_chan_fix ) {
     /* some seisan chans look like: EH Z 
                                     0123
      */
     strncpy(trh->chan, sh.kcmpnm, TRACE2_CHAN_LEN);
     trh->chan[2] = trh->chan[3];
     trh->chan[3] = '\0';
  } else {
     strncpy( trh->chan, sh.kcmpnm, TRACE2_CHAN_LEN );
     trh->chan[TRACE2_CHAN_LEN-1] = '\0';
  }
  TRIM( trh->chan );

  if ( loc[0] != '\0' ) {
     strncpy( trh->loc, loc, TRACE2_LOC_LEN );
  } else if ( memcmp( sh.khole, sac_undef, TRACE2_LOC_LEN ) != 0 &&
              memcmp( sh.khole, "  ", 2 ) != 0 ) {
     strncpy(trh->loc, sh.khole, TRACE2_LOC_LEN);
  } else {
     strncpy( trh->loc, LOC_NULL_STRING, TRACE2_LOC_LEN );
  }
  trh->loc[TRACE2_LOC_LEN-1] = '\0';
  TRIM( trh->loc );

  trh->version[0] = TRACE2_VERSION0;
  trh->version[1] = TRACE2_VERSION1;
  
  trh->quality[0] = '\0';
  if (new_srate != 0.0) {
    sampInt = 1.0/new_srate;
    trh->samprate = new_srate;
  } else {
    sampInt = (double)sh.delta;
    trh->samprate = 1.0/sampInt;
  }

#ifdef _INTEL
  strcpy(trh->datatype, "i4");
#endif
#ifdef _SPARC
  strcpy(trh->datatype, "s4");
#endif

  fprintf( stderr, "SAC reference time  %4.4" PRId32 ",%3.3" PRId32
                   ",%2.2" PRId32 ":%2.2" PRId32 ":%2.2" PRId32
                   ".%4.4" PRId32 " + B %f secs\n",
                   sh.nzyear, sh.nzjday, sh.nzhour, sh.nzmin, sh.nzsec,
                   sh.nzmsec, sh.b );
  sTime = (double)sh.b + sacRefTime( &sh );
  fprintf( stderr, "tracebuf start time %s\n",
                   datestr23( sTime, tb_date, sizeof( tb_date ) ) );
  fprintf( stderr, "tracebuf SCNL       %s.%s.%s.%s\n",
                   trh->sta, trh->chan, trh->net, trh->loc );
  
  /* Read the sac data into a buffer */
  if ( fread( seis, sizeof( *seis ), npts, fp ) != npts )
  {
    fprintf(stderr, "error reading SAC data: %s\n", strerror(errno));
    exit( 1 );
  }
  fclose(fp);

  /* mtheo 2007/11/15 */
  if(swapIsNeeded == 1) {
      for (i = 0; i < npts; i++)
	  SwapFloat( &(seis[i]) );
  }

  if ( npts > 0 )
  {
    sac_min = seis[0];
    sac_max = seis[0];
  }

  sp = seis;
  while (npts >= max_samps)
  {
    datalen = max_samps * sizeof( *seis );
    trh->starttime = sTime;
    sTime += sampInt * max_samps;
    trh->endtime = sTime - sampInt;
    trh->nsamp = max_samps;
    trh->pinno = 0;      
    
    lp = (int32_t *) (trh+1);
    for ( i = 0; i < max_samps; i++ )
    {
      if ( *sp < sac_min )
        sac_min = *sp;
      else if ( *sp > sac_max )
        sac_max = *sp;
      *lp++ = (int32_t) ( multiplier * (*sp++) );
    }
    npts -= max_samps;

    if ( fwrite( trh, 1, sizeof( *trh ) + datalen, fp_out ) 
        != sizeof( *trh ) + datalen )
    {
      fprintf(stderr, "Error writing tankfile: %s\n", strerror(errno));
      exit( 1 );
    }
  }
  if (npts > 0)
  {  /* Get the last few crumbs */
    datalen = npts * sizeof( *seis );
    trh->starttime = sTime;
    sTime += sampInt * npts;
    trh->endtime = sTime - sampInt;
    trh->nsamp = npts;
    trh->pinno = 0;      
    
    lp = (int32_t *) (trh+1);
    for ( i = 0; i < npts; i++ ) {
      if ( *sp < sac_min )
        sac_min = *sp;
      else if ( *sp > sac_max )
        sac_max = *sp;
      *lp++ = (int32_t) ( multiplier * (*sp++) );
    }

    if ( fwrite( trh, 1, sizeof( *trh ) + datalen, fp_out ) 
        != sizeof( *trh ) + datalen )
    {
      fprintf(stderr, "Error writing tankfile: %s\n", strerror(errno));
      exit( 1 );
    }
  }
  
  fprintf( stderr, "SAC      min:max    %f:%f, multiplier %f\n",
                   sac_min, sac_max, multiplier );
  fprintf( stderr, "tracebuf min:max    %" PRId32 ":%" PRId32 "\n",
                   (int32_t) ( multiplier * sac_min ), (int32_t) ( multiplier * sac_max ) );

  return( 0 );
}


static void usage( char *argv0 )
{
#ifdef _WINNT
  fprintf(stderr, "Usage: %s [-c][-m multiplier] [-N NN] [-C CCC] [-S SSSSS] [-L LL] [-n max-samples] [-a] infile outfile\n", argv0);
#else
  fprintf(stderr, "Usage: %s [-c][-m multiplier] [-s sps] [-N NN] [-C CCC] [-S SSSSS] [-L LL] [-n max-samples] infile >> outfile\n", argv0);
  fprintf(stderr, "    or %s [-c][-m multiplier] [-s sps] [-N NN] [-C CCC] [-S SSSSS] [-L LL] [-n max-samples] [-a] infile outfile\n", argv0);
#endif
  fprintf(stderr, "    -N NN is the network code to use from the cmdline instead of SAC file\n");
  fprintf(stderr, "    -L LL is the location code to use from the cmdline instead of SAC file\n");
  fprintf(stderr, "    -C CCC is the chan code to use from the cmdline instead of SAC file\n");
  fprintf(stderr, "    -S SSSSS is the station name to use from the cmdline instead of SAC file\n");
  fprintf(stderr, "    -s rate use this sample rate instead of from SAC file\n");
  fprintf(stderr, "    -c is a flag to fix a SEISAN problem with chans written in as EH Z\n");
  fprintf(stderr, "    -m multiplier is a scale factor applied to the SAC float data\n");
  fprintf(stderr, "    -a append output to named outfile\n");
  fprintf(stderr, "    Version: " VERSION_NUM "\n");
  exit( 1 );
}

  
/*
 * readSACHdr: read the header portion of a SAC file into memory.
 *  arguments: file pointer: pointer to an open file from which to read
 *             filename: pathname of open file, for logging.
 * returns: 0 on success
 *          1 on success and if byte swapping is needed
 *         -1 on error reading file
 *     The file is left open in all cases.
 */
static int readSACHdr(FILE *fp, struct SAChead *pSH)
{
  int i;
/******************************************************************************
 *                                                                            *
 * SAChead2 uses arrays to overlay the fields in SAChead to make it easier to *
 * initialize them.  This presumption is specifically not provided for in the *
 * C  standard,  and  is therefore non-conforming, i.e., neither portable nor *
 * reliable.  See include/sachead.h.                                          *
 *                                                                            *
 ******************************************************************************/
  struct SAChead2 *pSH2;
  long fileSize;
  int ret = 0;

  // obtain file size:
  fseek (fp , 0 , SEEK_END);
  fileSize = ftell (fp);
  rewind (fp);

  pSH2 = (struct SAChead2 *) pSH;
  
  if ( fread( pSH2, sizeof( *pSH2 ), 1, fp ) != 1 )
  {
    fprintf(stderr, "readSACHdr: error reading SAC file: %s\n",
            strerror(errno));
    return -1;
  }
  
  /* mtheo 2007/10/19
   * Guessing if byte swapping is needed
   * fileSize should be equal to size of SAC file header + ( pSH->npts * sizeof( float ) ) */
  if ( fileSize != ( SACHEADERSIZE + ( pSH->npts * sizeof( float ) ) ) ) {
      ret = 1;
      fprintf( stderr, "WARNING: Swapping is needed! (fileSize %ld, psh.npts %" PRId32 ")\n",
                      fileSize, pSH->npts );
      for (i = 0; i < NUM_FLOAT; i++)
	  SwapFloat( &(pSH2->SACfloat[i]) );
      for (i = 0; i < MAXINT; i++)
	  SwapInt32( &(pSH2->SACint[i]) );
      if ( fileSize != ( SACHEADERSIZE + ( pSH->npts * sizeof( float ) ) ) ) {
	  fprintf( stderr, "ERROR: Swapping is needed again! (fileSize %ld, psh.npts %" PRId32 ")\n",
		          fileSize, pSH->npts );
	  ret = -1;
      }
  } else {
      fprintf( stderr, "Swapping is not needed! (fileSize %ld, psh.npts %" PRId32 ")\n",
                      fileSize, pSH->npts );
  }
  
  return ret;
}


/*
 * sacRefTime: return SAC reference time as a double.
 *
 *             Uses a trick of mktime() (called by timegm_ew): if tm_mday
 *             exceeds the normal range for the month, tm_mday and tm_mon
 *             get adjusted to the correct values. So while mktime() ignores
 *             tm_yday, we can still handle the julian day of the SAC header.
 *
 *             Any undefined time values in the SAC header are replaced by the
 *             current UTC year, month, and day, and 0 fo the hour, minute,
 *             second, and millisecond.
 *
 *  Returns: SAC reference time as a double.
 */
static double sacRefTime( struct SAChead *pSH )
{
  struct tm tms;
  time_t clock;
  double sec;

  time( &clock );
  gmtime_ew( &clock, &tms );
  tms.tm_hour = 0;
  tms.tm_min  = 0;
  tms.tm_sec  = 0;

  if ( pSH->nzyear != SACUNDEF )
    tms.tm_year = pSH->nzyear - 1900;
  if ( pSH->nzjday != SACUNDEF ) {
    tms.tm_mon  = 0;            /* Force the month to January */
    tms.tm_mday = pSH->nzjday;  /* tm_mday is 1 - 31; nzjday is 1 - 366 */
  }
  if ( pSH->nzhour != SACUNDEF )
    tms.tm_hour = pSH->nzhour;
  if ( pSH->nzmin  != SACUNDEF )
    tms.tm_min  = pSH->nzmin;
  if ( pSH->nzsec  != SACUNDEF )
    tms.tm_sec  = pSH->nzsec;

  tms.tm_isdst = 0;
  sec = (double)timegm_ew(&tms);

  if ( pSH->nzmsec != SACUNDEF )
    sec += pSH->nzmsec / 1000.0;
  
  return sec;
}
