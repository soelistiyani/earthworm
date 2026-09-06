/*
 * call_FilterPicker.c
 * Bridge routine between Earthworm and FilterPicker
 *
 * This file is part of pick_FP, Earthworm module for the FilterPicker phase detector and picker.
 *
 * Copyright (C) 2008-2012 Claudio Satriano <satriano@ipgp.fr> and Anthony Lomax <anthony@alomax.net>
 *                         with the contribution of Luca Elia <elia@fisica.unina.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//Earthworm stuff
#include "earthworm.h"
#include "trace_buf.h"
#include "chron3.h"
#include "transport.h"

#include "pick_FP.h"

#define MAX_TRACEBUF_SAMPLES 2016
#define RESTRICT_PICK_SEC 4.0
#define RESTRICT_WITHIN_PICK_SEC 10.0

static float sample[MAX_TRACEBUF_SAMPLES];

void reportPick (PickData* pick_list, TRACE2_HEADER* TraceHead, STATION* Sta, GPARM* Gparm, EWH* Ewh);
void reportPick2 (PEEW *eew_Sta, int pickListNum, GPARM* Gparm, EWH* Ewh, int);
void reportPick_eew (PEEW *eew_Sta, int pickListNum, GPARM* Gparm, EWH* Ewh);
int sniff_eew( int LongSample, PEEW *eew_Sta, GPARM *Gparm );
extern int offline_mode;

int time_cmp(const void *x1, const void *x2)
{
	PickData* p1 = (PickData*)x1;
	PickData* p2 = (PickData*)x2;

	if(p1->lddate < 0 && p2->lddate >= 0 ) return 1;
	if(p2->lddate < 0 && p1->lddate >= 0 ) return -1;

	if(p1->P < p2->P)   return -1;
	if(p1->P > p2->P)  return 1;
	return 0;
}

void call_FilterPicker (PEEW *eew_Sta, STATION *Sta, char *TraceBuf, GPARM *Gparm, EWH *Ewh)
{
	double        tracesecs;

	PickData** pick_list = NULL; //pick_list is allocated by the Pick() function
	int num_picks = 0;

	TRACE2_HEADER *TraceHead = (TRACE2_HEADER *) TraceBuf;
   	int32_t *TraceSample = (int32_t *) (TraceBuf + sizeof(TRACE2_HEADER));

	// useMemory: set to TRUE_INT=1 if function is called for packets of data in sequence, FALSE_INT = 0 otherwise
	BOOLEAN_INT useMemory = TRUE_INT;

	int i, n, j;

	int length; //packet size

	// picker parameters
	// SEE: _DOC_ in FilterPicker.c for more details on the picker parameters
	// defaults
	// filp_test filtw 4.0 ltw 10.0 thres1 8.0 thres2 8.0 tupevt 0.2 res PICKS...
	double filterWindow = Sta->Parm.filterWindow;	// NOTE: auto set below
	double longTermWindow = Sta->Parm.longTermWindow;// NOTE: auto set below
	double threshold1 = Sta->Parm.threshold1;
	double threshold2 = Sta->Parm.threshold2;
	double tUpEvent = Sta->Parm.tUpEvent;	// NOTE: auto set below

	// integer version of parameters
	long iFilterWindow;
	long ilongTermWindow;
	long itUpEvent;

	double delta; //sampling step
	double dt; //sampling step (parameter)

	char channel_id[20];

#ifdef DEBUG
	printf( "pinno: %d\n nsamp: %d\n starttime: %lf\n endtime: %lf\n samprate: %lf\n sta: %s\n net: %s\n chan: %s\n loc: %s\n version: %c%c\n datatype: %s\n quality: %c%c\npad: %c%c\n",
        TraceHead->pinno,                 /* Pin number */
        TraceHead->nsamp,                 /* Number of samples in packet */
        TraceHead->starttime,             /* time of first sample in epoch seconds
                                          (seconds since midnight 1/1/1970) */
        TraceHead->endtime,               /* Time of last sample in epoch seconds */
        TraceHead->samprate,              /* Sample rate; nominal */
        TraceHead->sta,   		  /* Site name (NULL-terminated) */
        TraceHead->net,		          /* Network name (NULL-terminated) */
        TraceHead->chan, 		  /* Component/channel code (NULL-terminated)*/
        TraceHead->loc,			  /* Location code (NULL-terminated) */
        TraceHead->version[0], TraceHead->version[1],            /* version field */
        TraceHead->datatype,	           /* Data format code (NULL-terminated) */
        TraceHead->quality[0], TraceHead->quality[1],           /* Data-quality field */
        TraceHead->pad[0], TraceHead->pad[1]			  /* padding */
);
#endif

	// allocate array for data
	length = (int) TraceHead->nsamp;
	for (i=0; i<=length; i++)
		sample[i] = (float) TraceSample[i];

	//
        // auto set values
        // get dt
	delta = 1.0/TraceHead->samprate;
	dt = delta;
	dt = dt < 0.02 ? 0.02 : dt;     // avoid too-small values for high sample rate data
	//
	
	if (filterWindow < 0) //autoset if negative
		filterWindow = 300.0 * dt;
	iFilterWindow = (long) (0.5 + filterWindow * 1000.0);
	if (iFilterWindow > 1)
		filterWindow = (double) iFilterWindow / 1000.0;
	//
	if (longTermWindow < 0) //autoset if negative
		longTermWindow = 500.0 * dt;  // seconds
	ilongTermWindow = (long) (0.5 + longTermWindow * 1000.0);
	if (ilongTermWindow > 1)
		longTermWindow = (double) ilongTermWindow / 1000.0;
	//
	if (tUpEvent < 0) //autoset if negative
		tUpEvent = 20.0 * dt;   // time window to take integral of charFunct version
	itUpEvent = (long) (0.5 + tUpEvent * 1000.0);
	if (itUpEvent > 1)
		tUpEvent = (double) itUpEvent / 1000.0;
	//
#ifdef DEBUG
	printf("picker_func_test: filp_test filtw %f ltw %f thres1 %f thres2 %f tupevt %f res PICKS\n",
	       filterWindow, longTermWindow, threshold1, threshold2, tUpEvent);
#endif

	sprintf(channel_id, "%s.%s.%s.%s", TraceHead->sta, TraceHead->net, TraceHead->chan, TraceHead->loc);
	Pick(
		delta,
		sample,
		length,
		filterWindow,
		longTermWindow,
		threshold1,
		threshold2,
		tUpEvent,
		&(Sta->mem),
		useMemory,
		&pick_list,
		&num_picks,
		channel_id
	);

	/* Original Code */
	//for (n = 0; n < num_picks; n++) {
	//	reportPick (*(pick_list+n), TraceHead, Sta, Gparm, Ewh);
	//}

	/* Modify Code by Jman */

	/* Check timeout */
	for ( i = 0; i < MAX_PICKDATA; i++ )
	{
		// Check time out
		//printf("%d %f\n", i, (time(NULL) - eew_Sta->pickList[i].lddate));
		if ( eew_Sta->pickList[i].lddate > 0 && ((double)time(NULL) - eew_Sta->pickList[i].lddate) > RESTRICT_WITHIN_PICK_SEC + 5.0 )
		{
			//printf("Remove by timer Pick Pd,.. %d %d %f -- %f %f %f %f\n", i, j, tracesecs, eew_Sta->pickList[i].pa, eew_Sta->pickList[i].pv, eew_Sta->pickList[i].pd, eew_Sta->pickList[i].tc);
			eew_Sta->pickList[i].lddate = -1;
		}
	}

	/* Add pick result to eew_Sta */
	for ( n = 0; n < num_picks; n++ )
	{
		for ( i = 0; i < MAX_PICKDATA; i++ )
		{
			if ( eew_Sta->pickList[i].lddate == -1 )
			{
				eew_Sta->pickList[i].amplitude = (*(pick_list+n))->amplitude;
				eew_Sta->pickList[i].amplitudeUnits = (*(pick_list+n))->amplitudeUnits;
				eew_Sta->pickList[i].indices[0] = (*(pick_list+n))->indices[0];
				eew_Sta->pickList[i].indices[1] = (*(pick_list+n))->indices[1];
				eew_Sta->pickList[i].period = (*(pick_list+n))->period;
				eew_Sta->pickList[i].polarity = (*(pick_list+n))->polarity;
				eew_Sta->pickList[i].lddate = time(NULL);

				eew_Sta->pickList[i].P = (double) TraceHead->starttime;
				eew_Sta->pickList[i].P += delta * (eew_Sta->pickList[i].indices[0] + eew_Sta->pickList[i].indices[1]) / 2.0;

				eew_Sta->pickList[i].pa = 0;
				eew_Sta->pickList[i].pv = 0;
				eew_Sta->pickList[i].pd = 0;
				eew_Sta->pickList[i].tc = 0;
				eew_Sta->pickList[i].upd_sec = 0;

				//printf("Add pickList in Sta_eew: %s.%s.%s.%s %f\n", eew_Sta->sta, eew_Sta->chan, eew_Sta->net, eew_Sta->loc, eew_Sta->pickList[i].P);
				break;
			}
		}
	}

	/* To do: sort pick_list except -1 */
	qsort(eew_Sta->pickList, MAX_PICKDATA, sizeof(PickData), time_cmp);

	/* Remove Pick within ? seconds */
	for ( i = MAX_PICKDATA; i >= 0; i-- )
	{
		if ( eew_Sta->pickList[i].lddate > 0 && n > 0 )
		{
			double p2 = eew_Sta->pickList[i].P;
			double p1 = eew_Sta->pickList[i-1].P;

			if ( p2 - p1 < RESTRICT_WITHIN_PICK_SEC )
			{
				eew_Sta->pickList[i].lddate = -1;
				//printf("Remove Pick by time delta Pd,.. %d %f\n", i, eew_Sta->pickList[i].P);
			}
		}
	}

	/* sniff_eew for pa, pv, pd */
	for ( n = 0; n < length; n++ )
	{
		tracesecs = TraceHead->starttime + n / TraceHead->samprate;
		sniff_eew(TraceSample[n], eew_Sta, Gparm);

		for ( i = 0; i < MAX_PICKDATA; i++ )
		{
			if ( eew_Sta->pickList[i].lddate < 0 ) break;
			if ( eew_Sta->pickList[i].lddate > 0 )
			{
				if( fabs(eew_Sta->pickList[i].pa) < fabs(eew_Sta->a ) )     {  eew_Sta->pickList[i].pa = fabs(eew_Sta->a );  }
				if( fabs(eew_Sta->pickList[i].pv) < fabs(eew_Sta->v ) )     {  eew_Sta->pickList[i].pv = fabs(eew_Sta->v );  }
				if( fabs(eew_Sta->pickList[i].pd) < fabs(eew_Sta->d ) )     {  eew_Sta->pickList[i].pd = fabs(eew_Sta->d );  }
				//printf("Get Pick Pd,.. %f %f %f %f\n", eew_Sta->pickList[i].pa, eew_Sta->pickList[i].pv, eew_Sta->pickList[i].pd, eew_Sta->pickList[i].tc);
				if (eew_Sta->tc > 4.0) eew_Sta->tc= 0.0;
				eew_Sta->pickList[i].tc = eew_Sta->tc;		
		
				// Using 4 seconds
				for( j = eew_Sta->pickList[i].upd_sec; j <= RESTRICT_WITHIN_PICK_SEC; j++ )
				{
					if ( tracesecs - eew_Sta->pickList[i].P > eew_Sta->pickList[i].upd_sec )
					{
						// Reporting
						// Not support old style pick, because it does not have pickIndex
						if ( eew_Sta->pickList[i].upd_sec <= RESTRICT_PICK_SEC )
							if ( Gparm->ProduceOldStylePick ) {}//reportPick_eew( eew_Sta, i, Gparm, Ewh );
							else
								reportPick2 (eew_Sta, i, Gparm, Ewh, (int)eew_Sta->pickList[i].upd_sec);

						eew_Sta->pickList[i].upd_sec++;
					}
				}

				// Check upd_sec
				if ( eew_Sta->pickList[i].upd_sec > RESTRICT_WITHIN_PICK_SEC ) {
					//printf("Remove by upd_sec Pick Pd,.. %f %d %d %f -- %f %f %f %f\n", eew_Sta->pickList[i].P, i, j, tracesecs, eew_Sta->pickList[i].pa, eew_Sta->pickList[i].pv, eew_Sta->pickList[i].pd, eew_Sta->pickList[i].tc);
					eew_Sta->pickList[i].lddate = -1;
					eew_Sta->eew_flag = 0;
				}

			}
		}
	}

	//for ( n = 0; n < num_picks; n++ )
	//{
	//	for ( i = 0; i < MAX_PICKDATA; i++ )
	//	{
	//		if ( eew_Sta->pickList[i].lddate == -1 )
	//		{
	//			eew_Sta->pickList[i].amplitude = (*(pick_list+n))->amplitude;
	//			eew_Sta->pickList[i].amplitudeUnits = (*(pick_list+n))->amplitudeUnits;
	//			eew_Sta->pickList[i].indices[0] = (*(pick_list+n))->indices[0];
	//			eew_Sta->pickList[i].indices[1] = (*(pick_list+n))->indices[1];
	//			eew_Sta->pickList[i].period = (*(pick_list+n))->period;
	//			eew_Sta->pickList[i].polarity = (*(pick_list+n))->polarity;
	//			eew_Sta->pickList[i].lddate = time(NULL);

	//			eew_Sta->pickList[i].P = (double) TraceHead->starttime;
	//			eew_Sta->pickList[i].P += delta * (eew_Sta->pickList[i].indices[0] + eew_Sta->pickList[i].indices[1]) / 2.0;

	//			eew_Sta->pickList[i].pa = 0;
	//			eew_Sta->pickList[i].pv = 0;
	//			eew_Sta->pickList[i].pd = 0;
	//			eew_Sta->pickList[i].tc = 0;

	//			printf("Add pickList in Sta_eew: %s.%s.%s.%s %f\n", eew_Sta->sta, eew_Sta->chan, eew_Sta->net, eew_Sta->loc, eew_Sta->pickList[i].P);

	//			// reject within ?second picking
	//			if ( eew_Sta->pickList[i].P - eew_Sta->last_report_P <= RESTRICT_PICK_SEC )
	//			{
	//				printf("Add pickList in Sta_eew but rejected: %s.%s.%s.%s %f\n", eew_Sta->sta, eew_Sta->chan, eew_Sta->net, eew_Sta->loc, eew_Sta->pickList[i].P);
	//				eew_Sta->pickList[i].lddate = -1;
	//			}

	//			if ( eew_Sta->pickList[i].lddate > 0 )
	//			{
	//				// report pick without pa, pv, pd, tc
	//				// Not support old style pick, because it does not have pickIndex
	//				if ( Gparm->ProduceOldStylePick ) {}//reportPick_eew( eew_Sta, i, Gparm, Ewh );
	//				else
	//					reportPick2 (eew_Sta, i, Gparm, Ewh, 0);
	//			}
	//				
	//			break;
	//		}
	//	}
	//}

	///* sniff_eew for pa, pv, pd */
	//for ( n = 0; n < length; n++ )
	//{
	//	double del = -1;
	//	tracesecs = TraceHead->starttime + n / TraceHead->samprate;
	//	sniff_eew(TraceSample[n], eew_Sta, Gparm);
	//	//printf(">>>> %f  %f %f %f\n", tracesecs, eew_Sta->a, eew_Sta->v, eew_Sta->d);
	//	for ( i = 0; i < MAX_PICKDATA; i++ )
	//	{
	//		if ( eew_Sta->pickList[i].lddate > 0 )
	//		{
	//			if ( tracesecs - eew_Sta->pickList[i].P < 3.0 )
	//			{
	//				if( fabs(eew_Sta->pickList[i].pa) < fabs(eew_Sta->a ) )     {  eew_Sta->pickList[i].pa = fabs(eew_Sta->a );  }
	//				if( fabs(eew_Sta->pickList[i].pv) < fabs(eew_Sta->v ) )     {  eew_Sta->pickList[i].pv = fabs(eew_Sta->v );  }
	//				if( fabs(eew_Sta->pickList[i].pd) < fabs(eew_Sta->d ) )     {  eew_Sta->pickList[i].pd = fabs(eew_Sta->d );  }
	//				//printf("Get Pick Pd,.. %f %f %f %f\n", eew_Sta->pickList[i].pa, eew_Sta->pickList[i].pv, eew_Sta->pickList[i].pd, eew_Sta->pickList[i].tc);
	//				if (eew_Sta->tc > 4.0) eew_Sta->tc= 0.0;
	//				eew_Sta->pickList[i].tc = eew_Sta->tc;
	//			}
	//			// Wait 3sec for pa, pv, pd
	//			else if ( tracesecs - eew_Sta->pickList[i].P >= 3.0 )
	//			{
	//				//printf(">>> %f %f\n", tracesecs, eew_Sta->pickList[i].P);
	//				double removePickP = eew_Sta->pickList[i].P;

	//				if ( Gparm->ProduceOldStylePick ) reportPick_eew( eew_Sta, i, Gparm, Ewh );
	//				else
	//				{
	//					// change Pick time to trace time for putpick.
	//					// Important. So update pick's time is not picking time but trace time.
	//					// Not support old style pick, because it does not have pickIndex
	//					eew_Sta->pickList[i].P = tracesecs;
	//					reportPick2 (eew_Sta, i, Gparm, Ewh, 1);
	//				}

	//				eew_Sta->pickList[i].lddate = -1;
	//				//printf("Remove pickList in Sta_eew: %s.%s.%s.%s %f %f\n", eew_Sta->sta, eew_Sta->chan, eew_Sta->net, eew_Sta->loc, eew_Sta->pickList[i].P, tracesecs);
	//				eew_Sta->last_report_P = eew_Sta->pickList[i].P;

	//				// reject within ?second picking
	//				for ( j=0; j<MAX_PICKDATA; j++)
	//				{
	//					if ( eew_Sta->pickList[j].lddate > 0 )
	//					{
	//						if ( fabs(removePickP - eew_Sta->pickList[j].P) <= RESTRICT_PICK_SEC )
	//						{
	//							eew_Sta->pickList[j].lddate = -1;
	//						}
	//					}
	//				}
	//			}

	//			if ( eew_Sta->pickList[i].lddate > 0 )
	//			{
	//				del = (double)time(NULL) - eew_Sta->pickList[i].lddate;
	//				if ( del > 10.0 )
	//				{
	//					if ( Gparm->ProduceOldStylePick ) reportPick_eew( eew_Sta, i, Gparm, Ewh );
	//					else
	//						reportPick2 (eew_Sta, i, Gparm, Ewh, 0);

	//					logit("t", "Pick_FFP: Timeout. Pick removed in pickList. %s.%s.%s.%s %d",
	//						eew_Sta->sta, eew_Sta->chan, eew_Sta->net, eew_Sta->loc, eew_Sta->pickList[i].P);
	//					eew_Sta->pickList[i].lddate = -1;
	//				}
	//			}
	//
	//		}
	//	} // end of search PickData

	//} // end of sample

	// clean up
	free_PickList(pick_list, num_picks);	// PickData objects freed here

	return ;
}
