/*
 * Compute azimuthal gap of seismic stations 
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
#include "ll2utm.h"

void quick_sort(double *a,int l,int r);
double calc_pagap(double *a,int n);
double calc_sagap(double *a,int n);

extern int verbose;
extern EVNINFO evnlist[MAX_EV];
extern OBSINFO *obslist;


void find_gap(EVNINFO *event)
{
    int i,iph,nph;
    double pgap,sgap,faz,baz,epi,*bazlist,*epilist;

    nph = event->snph;

    bazlist = (double *)malloc(nph*sizeof(double));
    epilist = (double *)malloc(nph*sizeof(double));

    for(iph=0;iph<nph;iph++)
    {
        i = event->sphlist[iph];
        get_azimuth(event->lat, event->lon, 
                obslist[i].lat, obslist[i].lon, &faz, &baz, &epi);
        bazlist[iph] = baz;
        epilist[iph] = epi;
    }

	quick_sort(bazlist,0,nph-1);
	event->pgap = calc_pagap(bazlist,nph);
	event->sgap = calc_sagap(bazlist,nph);

	quick_sort(epilist,0,nph-1);
	event->mindist = epilist[0];
}

// calculate primary azimuthal gap
double calc_pagap(double *a,int n)
{
   int i;
   double agap;

   agap = 0.;
   for(i=1;i<n;i++)
   {
	if( agap < fabs(a[i]-a[i-1]) )
	    agap = fabs(a[i]-a[i-1]);
   }

   if( agap < fabs(a[0]+360.-a[n-1]) ) agap = fabs(a[0]+360.-a[n-1]);

   return agap;
}

// calculate secondary azimuthal gap
double calc_sagap(double *a,int n)
{
   int i;
   double agap;

   agap = 0.;
   for(i=2;i<n;i++)
   {
	if( agap < fabs(a[i]-a[i-2]) )
	    agap = fabs(a[i]-a[i-2]);
   }

   if( agap < fabs(a[0]+360.-a[n-2]) ) agap = fabs(a[0]+360.-a[n-2]);
   if( agap < fabs(a[1]+360.-a[n-1]) ) agap = fabs(a[1]+360.-a[n-1]);

   return agap;
}




