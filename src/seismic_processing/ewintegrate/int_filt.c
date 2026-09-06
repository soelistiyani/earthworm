/*
 *   THIS FILE IS UNDER RCS - DO NOT MODIFY UNLESS YOU HAVE
 *   CHECKED IT OUT USING THE COMMAND CHECKOUT.
 *
 *    $Id: filtdb.c,v 1.4 2004/05/11 18:14:18 dietz Exp $
 *
 *    Revision history:
 *     $Log: filtdb.c,v $
 *
 *
 */

/*
 * int_filt.c: Integrate-filter
 *              1) Filters and debiases TRACE_BUF messages
 *              2) Fills in outgoing TRACE_BUF messages
 *              3) Puts outgoing messages on transport ring
 */

/*******                                                        *********/
/*      Functions defined in this source file                           */
/*******                                                        *********/

/*      Function: FilterIntegrate                                       */
/*                                                                      */
/*      Inputs:         Pointer to World Structure                      */
/*                      Pointer to incoming TRACE message               */
/*                      SCNL index of incoming message                  */
/*                      Pointer to outgoing TRACE buffer                */
/*                                                                      */
/*      Outputs:        Message sent to the output ring                 */
/*                                                                      */
/*      Returns:        EW_SUCCESS on success, else EW_FAILURE          */

/*******                                                        *********/
/*      System Includes                                                 */
/*******                                                        *********/
#include <math.h>	/* round                                        */

/*******                                                        *********/
/*      Earthworm Includes                                              */
/*******                                                        *********/
#include "earthworm.h"  /* logit, threads                               */

/*******                                                        *********/
/*      Debias Includes                                                 */
/*******                                                        *********/
#include "ewintegrate.h"

/*******                                                        *********/
/*      Structure definitions                                           */
/*******                                                        *********/

/*******                                                        *********/
/*      Function definitions                                            */
/*******                                                        *********/

         /*******************************************************
          *                     ResetSCNL                       *
          *                                                     *
          *  A new span has started; (re-)initialize            *
          * information about its running average               *
          *******************************************************/
int XfrmResetSCNL( XFRMSCNL *xSCNL, TracePacket *inBuf, XFRMWORLD* xWorld ) {

  int i;
  INTSCNL *pSCNL = (INTSCNL *) xSCNL;
  INTWORLD *pWorld = (INTWORLD *) xWorld;

  pSCNL->awDataSize = inBuf->trh2.datatype[1] - '0';

  if ( pWorld->xfrmParam->avgWindowSecs > 0 ) {
      size_t bytesNeeded;        /* size of window in bytes */
      int samples;               /* nbr of samples in window */
      int numTBheaders;          /* nbr of packets cached when computing initial average */

      /* Calc nbr samples needed for length of averaging window, and estimate */
      /* nbr of packets for that many samples                                 */
      samples = (int) ( inBuf->trh2.samprate * pWorld->xfrmParam->avgWindowSecs );
      if ( samples < 2 ) {
        logit( "e", "ewintegrate: WARNING: AvgWindow too low to hold more than 1 "
                    "sample; lengthened to hold 2\n" );
        samples = 2;
      }

      /* (Re-)Allocation of space for headers of the initial packets */

      /* numTBheaders is the number of tracebufs needed to cache samples number */
      /* of data points, less one, since the current tracebuf does not need to  */
      /* be cached.  The calculation assumes all tracebufs are the same size as */
      /* the current tracebuf (inBuf).  Runt tracebufs can increase the number  */
      /* needed.  That unlikely event is handled in FilterXfrm().               */
      numTBheaders = (int) ceil( samples / inBuf->trh2.nsamp ) - 1;
      if ( numTBheaders > pSCNL->obhAllocated ) {
        pSCNL->outBufHdr = (TRACE2_HEADER *) realloc( pSCNL->outBufHdr, numTBheaders * sizeof( TRACE2_HEADER ) );
        if ( pSCNL->outBufHdr == NULL ) {
          logit( "e", "ewintegrate: failed to allocate queued initial headers; exiting.\n" );
          return EW_FAILURE;
        }
        pSCNL->obhAllocated = numTBheaders;
      }

      /* (Re-)allocation of space for averaging window of samples */
      pSCNL->awSamples = samples;
      bytesNeeded = samples * pSCNL->awDataSize;
      if ( bytesNeeded > pSCNL->awBytesAllocated ) {
        pSCNL->avgWindow = realloc( pSCNL->avgWindow, bytesNeeded );
        if ( pSCNL->avgWindow == NULL ) {
          logit( "e", "ewintegrate: failed to allocate averaging window; exiting.\n" );
          return EW_FAILURE;
        }
        pSCNL->awBytesAllocated = bytesNeeded;
      }

      /* Reset counters/flags */
      pSCNL->awPosn = pSCNL->awFull = pSCNL->nCachedOutbufs = 0;
      pSCNL->sum = 0.0;
      pSCNL->awEmpty = 1;
  }

  pSCNL->period = 1.0 / inBuf->trh2.samprate;

  if ( pSCNL->nHPLevels > 0 ) {
    complex *poles = pSCNL->poles;

    highpass( pSCNL->hpFreq, pSCNL->period, pSCNL->hpOrder, poles, &pSCNL->hp_gain );

    for ( i = 0; i < pSCNL->nHPLevels; i++ ) {
      pSCNL->hpLevel[i].d1 = 0.0;
      pSCNL->hpLevel[i].d2 = 0.0;
      pSCNL->hpLevel[i].a1 = -2.0 * poles[i+i].real;
      pSCNL->hpLevel[i].a2 = poles[i+i].real * poles[i+i].real +
                             poles[i+i].imag * poles[i+i].imag;
      pSCNL->hpLevel[i].b1 = -2.0;
      pSCNL->hpLevel[i].b2 = 1.0;
    }
  }

  return EW_SUCCESS;
} 

         /*******************************************************
          *                     InitSCNL                        *
          *                                                     *
          * Initialize information for the running average      *
          *******************************************************/
int XfrmInitSCNL( XFRMWORLD* xEwi, XFRMSCNL* xSCNL ) {

  int i;
  INTSCNL *pSCNL = (INTSCNL *) xSCNL;
  INTWORLD *pEwi = (INTWORLD *) xEwi;
  complex *poles;

  /* base setting */
  pSCNL->inEndtime = 0.0;

  /* Debias fields */
  pSCNL->avgWindow = NULL;
  pSCNL->outBufHdr = NULL;
  pSCNL->obhAllocated = pSCNL->nCachedOutbufs = pSCNL->awPosn
      = pSCNL->awBytesAllocated = pSCNL->awSamples = pSCNL->awFull
      = (int)(pSCNL->awDataSize = 0);
  pSCNL->awEmpty = 1;
  pSCNL->sum = 0.0;
  pSCNL->hpFreq = pEwi->xfrmParam->hpFreqCutoff;
  pSCNL->hpOrder = pEwi->xfrmParam->hpOrder;

  if ( pEwi->xfrmParam->avgWindowSecs <= 0 ) {
    pSCNL->awFull    = 1;
    /* To prevent divide-by-zero when "demeaning" and avgWindowSecs <= 0 */
    pSCNL->awSamples = 1;
  }

  /* Integrate & filter fields */
  pSCNL->outBuf  = NULL;
  pSCNL->hpLevel = NULL;
  pSCNL->poles   = NULL;
  pSCNL->nHPLevels = ( pEwi->xfrmParam->hpOrder + 1 ) / 2;

  if ( pSCNL->nHPLevels > 0 ) {
    HP_STAGE* stages = (HP_STAGE *) calloc( sizeof( HP_STAGE ), pSCNL->nHPLevels );
    if ( stages == NULL ) {
      logit( "et", "ewintegrate: Failed to allocate SCNL info stages; exiting.\n" );
      return EW_FAILURE;
    }
    poles = (complex *) calloc( sizeof( complex ), pEwi->xfrmParam->hpOrder );
    if ( poles == NULL ) {
      logit( "t", "ewintegrate: Failed to allocate SCNL info poles; exiting.\n" );
      free( stages );
      return EW_FAILURE;
    }
    pSCNL->hpLevel = stages;
    pSCNL->poles   = poles;
    for ( i = 0; i < pSCNL->nHPLevels; i++ ) {
      stages[i].d1 = 0;
      stages[i].d2 = 0;
      stages[i].f  = 0;
    }
  }

  return EW_SUCCESS;
}

         /*******************************************************
          *                      IntFilt                        *
          *                                                     *
          * Integrate datum (using the previous acc) if         *
          * integrating; if filtering, apply that afterward.    *
          * Yield the result of the processing                  *
          *******************************************************/
static double IntFilt( INTSCNL *pSCNL, double datum, INTWORLD *pEwi ) {

  int i;
  double out, next_out;

  if ( pEwi->xfrmParam->doIntegration ) {
      if ( pSCNL->awEmpty ) {
        out = 0.0;
        pSCNL->prev_vel = 0.0;
        pSCNL->awEmpty = 0;
      } else {
        out = pSCNL->prev_vel + 0.5 * ( datum + pSCNL->prev_acc ) * pSCNL->period;
        pSCNL->prev_vel = out;
      }
      pSCNL->prev_acc = datum;
  } else
    out = datum;

  if ( pSCNL->nHPLevels > 0 ) {
    HP_STAGE *lvl = pSCNL->hpLevel;
    for ( i = 0; i < pSCNL->nHPLevels; i++ ) {
      next_out = out + lvl->d1;
      lvl->d1 = lvl->b1*out - lvl->a1*next_out + lvl->d2 ;
      lvl->d2 = lvl->b2*out - lvl->a2*next_out ;
      out = next_out;
      lvl++;
    }
    out *= pSCNL->hp_gain;
  }

  return out;
}

         /*******************************************************
          *                    FilterXfrm                       *
          *                                                     *
          *  A new packet to be processed has arrived; process  *
          * and write to output ring as necessary.              *
          *******************************************************/
int FilterXfrm( XFRMWORLD* arg, TracePacket *inBuf, int jSta,
                unsigned char msgtype, TracePacket *outBuf ) {

  INTSCNL *pSCNL;
  short *waveShort = NULL, *outShort = NULL, *awShort = NULL;
  int32_t  *waveLong = NULL,  *outLong = NULL, *awLong = NULL;
  int ret, i, j, k;
  int datasize, outBufLen;
  int posn = 0;
  INTWORLD *pEwi = (INTWORLD *) arg;
  int debiasing = ( pEwi->xfrmParam->avgWindowSecs > 0 );
  double datum, bias;

  pSCNL = ( (INTSCNL *) pEwi->scnls ) + jSta;
/*
  fprintf(stderr, "DEBUG: Processing %s.%s.%s.%s from SCNLs array element %d\n",
                  pSCNL->inSta, pSCNL->inChan, pSCNL->inNet, pSCNL->inLoc, jSta);
*/

  ret = BaseFilterXfrm( arg, inBuf, jSta, msgtype, outBuf );
/*
  fprintf(stdout, "after BaseFilterXfrm().... SUCCESS\n");
*/
  if ( ret != EW_SUCCESS )
    return ret;

  /* Local copies of various values */
  datasize = (int)pSCNL->awDataSize;

  /* datasize-dependent pointers */
  if ( datasize == 2 ) {
    waveShort = (short*)   ( inBuf->msg  + sizeof( TRACE2_HEADER ) );
    outShort  = (short*)   ( outBuf->msg + sizeof( TRACE2_HEADER ) );
  } else {
    waveLong  = (int32_t*) ( inBuf->msg  + sizeof( TRACE2_HEADER ) );
    outLong   = (int32_t*) ( outBuf->msg + sizeof( TRACE2_HEADER ) );
  }

  if ( debiasing ) {
    posn = pSCNL->awPosn;
    /* Get properly typed pointer to window */
    if ( datasize == 2 )
      awShort = (short*)   pSCNL->avgWindow;
    else
      awLong  = (int32_t*) pSCNL->avgWindow;
  }

  for ( i = 0; i < inBuf->trh2.nsamp; i++ ) {

    if ( debiasing ) {
      /* If window is full, remove the oldest entry from sum */
        if ( pSCNL->awFull )
          pSCNL->sum -= (datasize==2 ? awShort[posn] : awLong[posn]);

        /* Add newest entry to sum & window */
        if ( datasize == 2 ) {
          awShort[posn] = *waveShort++;
          datum = awShort[posn++];
        } else {
          awLong[posn] = *waveLong++;
          datum = awLong[posn++];
        }
        pSCNL->sum += datum;
    } else
      datum = (datasize == 2 ) ? *waveShort++ : *waveLong++;

    /* If window is full (=1 if no debiasing), remove bias (=0.0 if no debiasing) */
    /* from latest value  (pSCNL->awSamples is guaranteed != 0 in XfrmInitSCNL()) */
    if ( pSCNL->awFull ) {
      bias = pSCNL->sum / pSCNL->awSamples;
      datum = IntFilt( pSCNL, datum - bias, pEwi );
      /*logit("","N[%d]: %f\n", i, datum );*/
      if ( datasize == 2 )
        outShort[i] = (short)   round( datum );
      else
        outLong[i]  = (int32_t) round( datum );
    }

    /* If we filled window for first time ( !pSCNL->awFull && ( posn == pSCNL->awSamples ) ), */
    /* flush the cache after debiasing it                                                     */
    else if ( posn == pSCNL->awSamples ) {
      bias = pSCNL->sum / pSCNL->awSamples;
      if (pEwi->xfrmParam->debug)
        logit( "et", "first flush of cache (bias %f=%f/%d)\n", bias, pSCNL->sum, pSCNL->awSamples );

      pSCNL->awFull = 1;

      /* Flush the data cached in the window after debiasing it */
      j = k = 0;
      for ( posn = 0; posn < pSCNL->awSamples; ) {

        /* If outBuf is already full, ship it out the transport ring */
        /* *Do not check the current packet nsamp; it is not cached* */
        if ( ( k < pSCNL->nCachedOutbufs ) && ( j == pSCNL->outBufHdr[k].nsamp ) ) {
          if ( pEwi->xfrmParam->debug )
            logit("et","Flushing outbuf #%d (%d samples)\n", k, pSCNL->outBufHdr[k].nsamp );
          /* Ship the packet out to the transport ring */
          outBuf->trh2 = pSCNL->outBufHdr[k];
          outBufLen = pSCNL->outBufHdr[k].nsamp * datasize + sizeof( TRACE2_HEADER );
          ret = XfrmWritePacket( arg, (XFRMSCNL *) pSCNL, msgtype, outBuf, outBufLen );
          if ( ret != EW_SUCCESS )
            return ret;
          k++;
          j = 0;
        }

        /* Remove bias from the cached values */
        datum = ( datasize == 2 ) ? awShort[posn++] : awLong[posn++];
        datum = IntFilt( pSCNL, datum - bias, pEwi );
        if ( datasize == 2 )
          outShort[j++] = (short)   round( datum );
        else
          outLong[j++]  = (int32_t) round( datum );

      }
      pSCNL->nCachedOutbufs = 0;

      /* outBuf may be full at this point, but that's okay; the data must be   */
      /* from the current (inBuf) data packet and it will be shipped out below */
    }
    if ( posn == pSCNL->awSamples )
      posn = 0;
  }
  pSCNL->awPosn = posn;
  /* If no debiasing, the window is full (awFull is guaranteed == 1 in XfrmInitSCNL()) */
  if ( pSCNL->awFull ) {
    /* Ship the packet out to the transport ring */
    outBuf->trh2 = inBuf->trh2;
    outBufLen = inBuf->trh2.nsamp * datasize + sizeof( TRACE2_HEADER );
    ret = XfrmWritePacket( arg, (XFRMSCNL *) pSCNL, msgtype, outBuf, outBufLen );
    if ( ret != EW_SUCCESS )
      return ret;
  } else {
    /* Only needed when debiasing and the averaging window is not yet full of data */
    if ( pEwi->xfrmParam->debug )
      logit( "et", "packet didn't fill window (%d vs %d)\n", pSCNL->awPosn, pSCNL->awSamples );
    /* Add this packet header to cached header list */
    /* There might be runt packets; check if more space is needed for the cached headers */
    if ( pSCNL->nCachedOutbufs >= pSCNL->obhAllocated ) {
      pSCNL->obhAllocated = pSCNL->nCachedOutbufs + 1;
      pSCNL->outBufHdr = (TRACE2_HEADER *) realloc( pSCNL->outBufHdr, pSCNL->obhAllocated * sizeof( TRACE_HEADER ) );
    }
    pSCNL->outBufHdr[pSCNL->nCachedOutbufs++] = inBuf->trh2;
  }

  return EW_SUCCESS;
}
