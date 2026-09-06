/*
 * main function for MAXEL
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "mle.h"
#include "ll2utm.h"

int nobs,nstnDB,nevn,nunused;    // number of observed time, number of station database
int minphid;
int verbose,ctype;
int ntrdpth,ntredst;
float *trtable;
double Xmin,Xmax,Ymin,Ymax,Zmin,Zmax;
double Xc,Yc,dX,dY,dZ,halfX,halfY,halfZ;
double crt_td;
int nX,nY,nZ;
OBSINFO *obslist;
STNINFO *stnlist;
EVNINFO evnlist[MAX_EV];
char *stnfile,*obsfile,*trfile;

int main(int argc, char **argv)
{
   if( argc != 2 )
   {
	sherror("Usage: %s control_file\n",argv[0]);
	return 0;
   }


   read_control(argv[1]);

   set_origin_coord(Xc,Yc);

   read_stnlist(stnfile);

   read_obslist(obsfile);

   bind_all(&evnlist[0]);

   find_confidence(&evnlist[0]);
   find_gap(&evnlist[0]);

   evnlist[0].MLE = pow(evnlist[0].MLE/evnlist[0].snph,1./(evnlist[0].snph-1));
   if( verbose == 2 ) 
   {
      make_evmapsc(&evnlist[0]);
      make_section(&evnlist[0],evnlist[0].x1,evnlist[0].y1,evnlist[0].z1,Xmin,Xmax,Ymin,Ymax,dX,dZ,0,1);
      make_section(&evnlist[0],evnlist[0].x,evnlist[0].y,evnlist[0].z,evnlist[0].x1-20.,evnlist[0].x1+20.,
		evnlist[0].y1-20.,evnlist[0].y1+20.,2.,2.,0,2);
   }

   if( verbose == 0 )
	printf("Event Location %-9.4f %-8.4f %-5.2f  %-6.1f %-6.1f  %8.4f %8.4f %.1f %.1f %.1f %.2f\n",
		evnlist[0].lon,evnlist[0].lat,evnlist[0].z,evnlist[0].x,evnlist[0].y,
        evnlist[0].stmad,evnlist[0].atmad,evnlist[0].cerarea, evnlist[0].mindist, evnlist[0].pgap,
        evnlist[0].MLE);
   else
   {
       printf("Error Ellipse: Azimuth %.1f  Major&Minor axis: %.2f %.2f, MLE: %.2f\n",
                evnlist[0].cerazi, evnlist[0].cerlmj, evnlist[0].cerlmn,evnlist[0].MLE);
        printf("           Center: %.4f %.4f    Area: %.1f km\n",
                evnlist[0].cerlon, evnlist[0].cerlat, evnlist[0].cerarea);
        printf("Primary & Secondary gap: %.1f %.1f   Min.dist.: %.1f km\n",
                evnlist[0].pgap, evnlist[0].sgap, evnlist[0].mindist);
   }

   return 0;
}
