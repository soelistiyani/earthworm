/*
	geojson2ew - geoJSON to earthworm 

	Copyright (c) 2014 California Institute of Technology. All rights reserved.
	Authors: Kevin Frechette & Paul Friberg, ISTI.

        Copyright ©2017 The Regents of the University of California. All rights reserved. 
        Authors: Mario Aranha, Berkeley Seismological Laboratory. 

        This program is distributed by ISTI WITHOUT ANY WARRANTY whatsoever.
        Do not redistribute this program without written permission from ISTI.
*/
#include <stdio.h>
#include <stdlib.h>
#include "earthworm.h"
#include "transport.h"
#include "externs.h"
#include "heart.h"
#include "die.h"

/* errmap = an optional integer value that maps to a statmgr error 
	see the die.h and geojson2ew.desc
*/

void geojson2ew_die( int errmap, char * str ) {
        RequestMutex();
        ShutMeDown = TRUE;
        ReleaseMutex_ew();
	if (errmap != -1 && errmap != GEOJSON2EW_DEATH_EW_CONFIG) {
		/* use the statmgr reporting to notify of this death */
#ifdef DEBUG
		fprintf(stderr, "SENDING MESSAGE to statmgr: %d %s\n",
			errmap, str);
#endif /*DEBUG*/
		message_send(TypeErr, errmap, str);
	}

	return;
}
