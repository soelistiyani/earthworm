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
#include  "earthworm.h"
#include  "externs.h" 

static unsigned char InstId = 255;

void
setuplogo(MSG_LOGO *logo) {
   /* only get the InstId once */
   if (InstId == 255  && GetLocalInst(&InstId) != 0) {
      logit("et",
              "%s: Invalid Installation code; exiting!\n", Progname);
      exit(-1);
   }
   logo->mod = QModuleId;
   logo->instid = InstId;
}
