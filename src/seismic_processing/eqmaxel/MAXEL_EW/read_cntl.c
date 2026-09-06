/*
 * read control file
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
#include "getpar.h"

extern int nobs,verbose,ctype;
extern int ntrdpth,ntredst;
extern char *stnfile,*obsfile,*trfile;
extern double Xmin,Xmax,Ymin,Ymax,Zmin,Zmax;
extern double Xc,Yc,dX,dY,dZ,halfX,halfY,halfZ;
extern int nX,nY,nZ;
extern double crt_td;

void read_control(char *iname)
{
   int i;
   double dist;

   Read_para_file(iname);

   if( !Get_par_int("nobs",&nobs) )
   {
	sherror("nobs is not defined\n");
   }

   if( !Get_par_int("coord",&ctype) ) ctype = DLTLN;
   if(ctype != KMXY ) ctype = DLTLN;

   if( !Get_par_str("station list",&stnfile) )
   {
	sherror("station list not defined\n");
   }

   if( !Get_par_str("observed time",&obsfile) )
   {
	sherror("observed time not defined\n");
   }

   if( !Get_par_str("trtable",&trfile) )
   {
	sherror("trtime is not defined\n");
   }

   read_trtable(trfile);

   if( !Get_par_double("Xc",&Xc) ) sherror("Xc is not defined\n");
   if( !Get_par_double("Yc",&Yc) ) sherror("Yc is not defined\n");

   crt_td = 2.; // criterion for travel time difference in binder

   Xmin = -800.;
   Xmax =  800.;
   Ymin = -800.;
   Ymax =  800.;
   Zmin =  0.;
   Zmax =  30.;
   dX = 20.;
   dY = 20.;
   dZ = 5.;
   
   ntrdpth = 31;
   ntredst = 2001;

   for(dist=Xmin,i=0;dist<=Xmax;dist+=dX) i++;
   nX = i;
   for(dist=Ymin,i=0;dist<=Ymax;dist+=dY) i++;
   nY = i;
   for(dist=0,i=0;dist<=Zmax;dist+=dZ) i++;
   nZ = i;

   if( !Get_par_int("verbose",  &verbose) )
   {
	verbose = 0;
   }
}

