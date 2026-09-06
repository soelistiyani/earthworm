/*
* report.c
* Modified from report.c, Revision 1.6 in src/seismic_processing/pick_ew
*
* This file is part of pick_FP, Earthworm module for the FilterPicker phase detector and picker.
*
* (C) 2008-2012 - Claudio Satriano <satriano@ipgp.fr>
*          with contributions from Luca Elia <elia@fisica.unina.it>,
* under the same license terms of the Earthworm software system.
*
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "earthworm.h"
#include "trace_buf.h"
#include "chron3.h"
#include "transport.h"

#include "pick_FP.h"

int GetPickIndex (unsigned char modid, char *dir);  /* function in index.c */

extern int offline_mode;

static char line[LINELEN];

void reportPick2 (PEEW *eew_Sta, int pickListNum, GPARM* Gparm, EWH* Ewh, int isUpdate)
{
	MSG_LOGO logo;      /* Logo of message to send to output ring */

	double secs;
	struct Greg gtime;

	double uncertainty;

	double delta;

	int lineLen;

	int tsec, thun;
	int PickIndex;
	char first_motion;
	int weight;

	if ( isUpdate == 0 )
	{
		PickIndex = GetPickIndex (Gparm->MyModId, Gparm->PickIndexDir);
		eew_Sta->pickIndex = PickIndex;
	}
	else
	{
		PickIndex = eew_Sta->pickIndex;
	}

	delta = eew_Sta->dt;

	//secs = (double) TraceHead->starttime + GSEC1970;
	//secs += delta * (pick_list->indices[0] + pick_list->indices[1]) / 2.0;
	
	// IMPORTANT!!
	// Consider putpick module, I add upd_sec to Ptime.
	secs = eew_Sta->pickList[pickListNum].P + eew_Sta->pickList[pickListNum].upd_sec + GSEC1970;
	datime(secs, &gtime);
	tsec = (int)floor( (double) gtime.second );
	thun = (int)((100.*(gtime.second - tsec)) + 0.5);
	if ( thun == 100 )
		tsec++, thun = 0;

	// pick quality
	// set uncertainty to half width between right and left indices
	// uncertaintyrk 작으면 높은 quality를 가진다.
	uncertainty = delta * fabs(eew_Sta->pickList[pickListNum].indices[1] - eew_Sta->pickList[pickListNum].indices[0]) / 2.0;
	for (weight=0; weight<=3; weight++)
		if(uncertainty <= Gparm->WeightTable[weight]) break;


	// first motion
	first_motion = '?';
	if (eew_Sta->pickList[pickListNum].polarity == POLARITY_POS)
		first_motion = 'U';
	if (eew_Sta->pickList[pickListNum].polarity == POLARITY_NEG)
		first_motion = 'D';

	/*
	* Convert pick to space-delimited text string.
	* This is a bit risky, since the buffer could overflow.
	* Milliseconds are always set to zero.
	*/
	sprintf (line,              "%d",  (int) Ewh->TypeEewPick);
	sprintf (line+strlen(line), " %d", (int) Gparm->MyModId);
	sprintf (line+strlen(line), " %d", (int) Ewh->MyInstId);
	sprintf (line+strlen(line), " %d", PickIndex);
	sprintf (line+strlen(line), " %s", eew_Sta->sta);
	sprintf (line+strlen(line), ".%s", eew_Sta->chan);
	sprintf (line+strlen(line), ".%s", eew_Sta->net);
	sprintf (line+strlen(line), ".%s", eew_Sta->loc);

	sprintf (line+strlen(line), " %c%d", first_motion, weight);

	sprintf (line+strlen(line), " %4d%02d%02d%02d%02d%02d.%02d0",
		gtime.year, gtime.month, gtime.day, gtime.hour,
		gtime.minute, tsec, thun);

	sprintf (line+strlen(line), " %d", (int)(eew_Sta->pickList[pickListNum].amplitude + 0.5));
	sprintf (line+strlen(line), " %d", 0);
	sprintf (line+strlen(line), " %d", 0);

	// additional line

	// IMPORTANT!!
	// Consider putpick module, I add upd_sec to Ptime.
	sprintf( line+strlen(line), " %f %f %f %f %f %f %11.5f %d %d",
		eew_Sta->lon, eew_Sta->lat,
		eew_Sta->pickList[pickListNum].pa, eew_Sta->pickList[pickListNum].pv, eew_Sta->pickList[pickListNum].pd, eew_Sta->pickList[pickListNum].tc,
		eew_Sta->pickList[pickListNum].P + eew_Sta->pickList[pickListNum].upd_sec, eew_Sta->inst, (int)eew_Sta->pickList[pickListNum].upd_sec);

	strcat (line, "\n");
	lineLen = strlen(line);

	if (offline_mode) {
		/* Print the pick */
		//#if defined(_OS2) || defined(_WINNT)
		//if ( isUpdate )
			printf( "%s", line );
		//#endif
	} else {
		/* Send the pick to the output ring */
		logo.type   = Ewh->TypeEewPick;
		logo.mod    = Gparm->MyModId;
		logo.instid = Ewh->MyInstId;

		if ( tport_putmsg( &Gparm->OutRegion, &logo, lineLen, line ) != PUT_OK )
			logit( "et", "pick_FFP: Error sending pick to output ring.\n" );
		else
			;
		
		logit( "ot", "PICK %s\n", line); //luca
	}

	return;

}

void reportPick_eew (PEEW *eew_Sta, int pickListNum, GPARM* Gparm, EWH* Ewh)
{
	MSG_LOGO logo;      /* Logo of message to send to output ring */
	double uncertainty;
	double delta;
	int lineLen;
	int weight;

	// pick quality
	// set uncertainty to half width between right and left indices
	delta = 1.0/eew_Sta->dt;
	uncertainty = delta * fabs(eew_Sta->pickList[pickListNum].indices[1] - eew_Sta->pickList[pickListNum].indices[0]) / 2.0;
	for (weight=0; weight<=3; weight++)
		if(uncertainty <= Gparm->WeightTable[weight]) break;

	/*
	* Convert pick to space-delimited text string.
	* This is a bit risky, since the buffer could overflow.
	* Milliseconds are always set to zero.
	*/

	sprintf (line, " %s", eew_Sta->sta);
	sprintf (line+strlen(line), " %s", eew_Sta->chan);
	sprintf (line+strlen(line), " %s", eew_Sta->net);
	sprintf (line+strlen(line), " %s", eew_Sta->loc);

	// additional line
	// To do: Tc is not accurate..
	sprintf( line+strlen(line), " %f %f %f %f %f %f %11.5f %d %d 0",
		eew_Sta->lon, eew_Sta->lat, eew_Sta->pickList[pickListNum].pa, eew_Sta->pickList[pickListNum].pv, eew_Sta->pickList[pickListNum].pd, eew_Sta->pickList[pickListNum].tc,
		eew_Sta->pickList[pickListNum].P, weight, eew_Sta->inst);

	strcat (line, "\n");
	lineLen = strlen(line);

	if (offline_mode) {
		/* Print the pick */
		//#if defined(_OS2) || defined(_WINNT)
		printf( "%s", line );
		//#endif
	} else {
		/* Send the pick to the output ring */
		logo.type   = Ewh->TypeEew;
		logo.mod    = Gparm->MyModId;
		logo.instid = Ewh->MyInstId;

		if ( tport_putmsg( &Gparm->OutRegion, &logo, lineLen, line ) != PUT_OK )
			logit( "et", "pick_FFP: Error sending pick to output ring.\n" );
		else
			;
		//logit( "t", "PICK %s\n", line); //luca
	}

	return;

}

void reportPick (PickData* pick_list, TRACE2_HEADER* TraceHead, STATION* Sta, GPARM* Gparm, EWH* Ewh)
{
	MSG_LOGO logo;      /* Logo of message to send to output ring */

	double secs;
	struct Greg gtime;

	double uncertainty;

	double delta;

	int lineLen;

	int tsec, thun;
	int PickIndex;
	char first_motion;
	int weight;


	PickIndex = GetPickIndex (Gparm->MyModId, Gparm->PickIndexDir);

	delta = 1.0/TraceHead->samprate;

	secs = (double) TraceHead->starttime + GSEC1970;
	secs += delta * (pick_list->indices[0] + pick_list->indices[1]) / 2.0;
	datime(secs, &gtime);
	tsec = (int)floor( (double) gtime.second );
	thun = (int)((100.*(gtime.second - tsec)) + 0.5);
	if ( thun == 100 )
		tsec++, thun = 0;

	// pick quality
	// set uncertainty to half width between right and left indices
	uncertainty = delta * fabs(pick_list->indices[1] - pick_list->indices[0]) / 2.0;
	for (weight=0; weight<=3; weight++)
		if(uncertainty <= Gparm->WeightTable[weight]) break;


	// first motion
	first_motion = '?';
	if (pick_list->polarity == POLARITY_POS)
		first_motion = 'U';
	if (pick_list->polarity == POLARITY_NEG)
		first_motion = 'D';

	/*
	* Convert pick to space-delimited text string.
	* This is a bit risky, since the buffer could overflow.
	* Milliseconds are always set to zero.
	*/
	sprintf (line,              "%d",  (int) Ewh->TypePickScnl);
	sprintf (line+strlen(line), " %d", (int) Gparm->MyModId);
	sprintf (line+strlen(line), " %d", (int) Ewh->MyInstId);
	sprintf (line+strlen(line), " %d", PickIndex);
	sprintf (line+strlen(line), " %s", Sta->sta);
	sprintf (line+strlen(line), ".%s", Sta->chan);
	sprintf (line+strlen(line), ".%s", Sta->net);
	sprintf (line+strlen(line), ".%s", Sta->loc);

	sprintf (line+strlen(line), " %c%d", first_motion, weight);

	sprintf (line+strlen(line), " %4d%02d%02d%02d%02d%02d.%02d0",
		gtime.year, gtime.month, gtime.day, gtime.hour,
		gtime.minute, tsec, thun);

	sprintf (line+strlen(line), " %d", (int)(pick_list->amplitude + 0.5));
	sprintf (line+strlen(line), " %d", 0);
	sprintf (line+strlen(line), " %d", 0);
	//sprintf (line+strlen(line), " %f", pick_list->pa);
	//sprintf (line+strlen(line), " %f", pick_list->pv);
	//sprintf (line+strlen(line), " %f", pick_list->pd);
	strcat (line, "\n");
	lineLen = strlen(line);

	if (offline_mode) {
		/* Print the pick */
		//#if defined(_OS2) || defined(_WINNT)
		printf( "%s", line );
		//#endif
	} else {
		/* Send the pick to the output ring */
		logo.type   = Ewh->TypePickScnl;
		logo.mod    = Gparm->MyModId;
		logo.instid = Ewh->MyInstId;

		if ( tport_putmsg( &Gparm->OutRegion, &logo, lineLen, line ) != PUT_OK )
			logit( "et", "pick_FFP: Error sending pick to output ring.\n" );
		else
			;
		//logit( "t", "PICK %s\n", line); //luca
	}

	return;
}
