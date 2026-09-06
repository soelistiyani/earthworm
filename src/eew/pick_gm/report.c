/*
 * report.c
 * Modified from report.c, Revision 1.6 in src/seismic_processing/pick_ew
 *
 * This file is part of pick_gm.
 *
 */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "earthworm.h"
#include "trace_buf.h"
#include "chron3.h"
#include "transport.h"

#include "pick_gm.h"

extern int offline_mode;

//static char line[LINELEN];

void reportGM(STATION* Sta, GPARM* Gparm, EWH* Ewh ) 
{
	MSG_LOGO logo;      /* Logo of message to send to output ring */	
	char line[LINELEN];
	int lineLen;

	sprintf (line, "%s", Sta->sta);
	sprintf (line+strlen(line), " %s", Sta->chan);
	sprintf (line+strlen(line), " %s", Sta->net);
	sprintf (line+strlen(line), " %s", Sta->loc);
	sprintf (line+strlen(line), " %f", Sta->lat);
	sprintf (line+strlen(line), " %f", Sta->lon);
	sprintf (line+strlen(line), " %f", Sta->tm);
	sprintf (line+strlen(line), " %f", fabs(Sta->pga));
	sprintf (line+strlen(line), " %f", fabs(Sta->pgv));
	sprintf (line+strlen(line), " %f", fabs(Sta->pgd));

	strcat (line, "\n");
	lineLen = strlen(line);

	if (Gparm->OutputByConsole) {
		printf( "[GM] %s", line );
	}
	
	if (!offline_mode) 
	{

		/* Send the gm to the output ring */
		logo.type   = Ewh->TypeStrongMotionGM;
		logo.mod    = Gparm->MyModId;
		logo.instid = Ewh->MyInstId;

		if ( tport_putmsg( &Gparm->OutRegion, &logo, lineLen, line ) != PUT_OK )
		 	logit( "et", "pick_gm: Error sending GM to output ring.\n" );
	}

	if (Gparm->fpGM != NULL) {
		fprintf(Gparm->fpGM, "%s", line);
		fflush(Gparm->fpGM);
	}

}

void reportGMX(STATION* StaGmx, GPARM* Gparm, EWH* Ewh ) 
{
	MSG_LOGO logo;      /* Logo of message to send to output ring */	
	char line[LINELEN];
	int lineLen;

	sprintf (line, "%s", StaGmx->sta);
	sprintf (line+strlen(line), " %s", StaGmx->chan);
	sprintf (line+strlen(line), " %s", StaGmx->net);
	sprintf (line+strlen(line), " %s", StaGmx->loc);
	sprintf (line+strlen(line), " %f", StaGmx->lat);
	sprintf (line+strlen(line), " %f", StaGmx->lon);
	sprintf (line+strlen(line), " %f", StaGmx->tm);
	sprintf (line+strlen(line), " %f", StaGmx->pga);
	sprintf (line+strlen(line), " %f", StaGmx->maxpga);
	sprintf (line+strlen(line), " %f", StaGmx->lbspga);
	sprintf (line+strlen(line), " %f", StaGmx->bcav);	// by Seo JeongBeom 
	sprintf (line+strlen(line), " %f", StaGmx->lbspga_mmi);	// by Seo JeongBeom
	sprintf (line+strlen(line), " %f", StaGmx->bcav_mmi);

	strcat (line, "\n");
	lineLen = strlen(line);

	if (Gparm->OutputByConsole) {
		printf( "[GMX] %s", line );
	}

	if (!offline_mode) 
	{
		/* Send the gmx to the output ring */
		logo.type   = Ewh->TypeStrongMotionGMX;
		logo.mod    = Gparm->MyModId;
		logo.instid = Ewh->MyInstId;

		if ( tport_putmsg( &Gparm->OutRegion, &logo, lineLen, line ) != PUT_OK )
		 	logit( "et", "pick_gm: Error sending GMX to output ring.\n" );
	}

	if (Gparm->fpGMX != NULL) {
		fprintf(Gparm->fpGMX, "%s", line);
		fflush(Gparm->fpGMX);
	}
}
