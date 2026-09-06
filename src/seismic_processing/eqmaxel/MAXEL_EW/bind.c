/*
 * Associate, bind phases, and then determine the event location
 * In this version, association and bind do not work. 
 * Just use all input phases.
 *
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
//#include <qtime.h>

extern int nobs,nevn,nunused,verbose,ctype;
extern OBSINFO *obslist;
extern EVNINFO evnlist[MAX_EV];
extern double crt_td;
EVNINFO tevn,best_evn;
int min_phase,nchecked;

extern double Xmin,Xmax,Ymin,Ymax,Zmin,Zmax;
extern double Xc,Yc,dX,dY,dZ;

double ttmp[MAX_PH],btmp[MAX_PH],tlst[MAX_PH],blst[MAX_PH];

void quick_sort_id(double *a, int *id,int l,int r);

void init_event(void)
{
   int i;

   min_phase = 3; 

   for(i=0;i<MAX_EV;i++)
   {
     evnlist[i].lat = 0.;
     evnlist[i].lon = 0.;
     evnlist[i].nph = 0;
   }
   nevn = 0;
   nunused = 0;

   tevn.lat = 0.;
   tevn.lon = 0.;
   tevn.nph = 0;
}

int assoc_all(EVNINFO *tevn)
{
	int i,itout,iaout,nttout;
	double x,y,z,lon,lat,avgdist,mindist,dist;
	FILE *fp;

	set_subregion(tevn);
	sample_obsph(tevn);

	if( tevn->snph < min_phase ) return -1;

	nchecked = tevn->snph;
	if(verbose) fprintf(stdout,"\nTry to locate an earthquake with %d phases\n",tevn->snph);
     
//**************************************************************
// First search
//**************************************************************
	run_mle(1,tevn,-1,&x,&y,&z,1);
	if(verbose) 
	{
		fp = fopen("lochist.dat","w");
		if( ctype == KMXY )
			fprintf(fp,"Pre. %.2f %.2f %.2f\n",x,y,z);
		else
			fprintf(fp,"Pre. %.4f %.4f %.2f\n",tevn->lon,tevn->lat,z);
		fclose(fp);
	}
	tevn->x1 = x;
	tevn->y1 = y;
	tevn->z1 = z;
	tevn->lon1 = tevn->lon;
	tevn->lat1 = tevn->lat;

//**************************************************************
// Second search
//**************************************************************
	run_mle(2,tevn,-1,&x,&y,&z,1);
	if(verbose) 
	{
		fp = fopen("lochist.dat","a");
		if( ctype == KMXY )
			fprintf(fp,"Res. %.2f %.2f %.2f\n",x,y,z);
		else
			fprintf(fp,"Pre. %.4f %.4f %.2f\n",tevn->lon,tevn->lat,z);
		fclose(fp);
	}
	tevn->x2 = x;
	tevn->y2 = y;
	tevn->z2 = z;

// make event
	if( tevn->nph >= min_phase )
	{
		evnlist[nevn].x = tevn->x;
		evnlist[nevn].y = tevn->y;
		evnlist[nevn].z = tevn->z;
		evnlist[nevn].lon = tevn->lon;
		evnlist[nevn].lat = tevn->lat;
		evnlist[nevn].nph = tevn->nph;
		evnlist[nevn].orgt = tevn->orgt;

		for(i=0;i<tevn->nph;i++)
		{
			evnlist[nevn].tdiff[i] = tevn->tdiff[i];
			evnlist[nevn].phlist[i] = tevn->phlist[i];
			obslist[ tevn->phlist[i] ].evid = nevn;
		}
		nevn++;
	}
	else 
	{
		fprintf(stderr,"Number of phase should be greater than %d\n",min_phase);
		return -1;
	}


	return nevn-1;
}

void print_phase(int iev)
{
	int i,iph,iobs,id[MAX_PH];
	double torg,trt[MAX_PH];
	double faz,baz,epi;
//	EXT_TIME evot;

	torg = evnlist[iev].orgt;
	fprintf(stdout,"\nEvent(%2d) Location: %9.4f %9.4f %6.2f %8.4f %8.4f\n",
	iev+1,evnlist[iev].lon,evnlist[iev].lat,evnlist[iev].z,evnlist[iev].stmad,evnlist[iev].atmad);

	for(iph=0;iph<evnlist[iev].nph;iph++)
	{
		i = evnlist[iev].phlist[iph];
		id[iph] = iph;
		trt[iph] = obslist[i].ti-torg;
	}
	quick_sort_id(trt, id, 0, evnlist[iev].nph-1);

//	evot = evnlist[iev].et_ot;
//	fprintf(stdout,"       Origin Time:  %04d/%02d/%02d %02d:%02d:%05.2f\n\n",
//			evot.year,evot.month,evot.day,evot.hour,evot.minute,
//		        evot.second+evot.usec/USECS_PER_TICK/10000.);

	if( ctype == KMXY )
		fprintf(stdout,"PID STN      X         Y        DIST   O.tr   P.tr    O-P\n");
	else
		fprintf(stdout,"PID STN      LON       LAT      DIST   O.tr   P.tr    O-P\n");
	for(i=0;i<evnlist[iev].nph;i++)
	{
		iph = id[i];
		iobs = evnlist[iev].phlist[iph];
		if( ctype == DLTLN )
			get_azimuth(evnlist[iev].lat, evnlist[iev].lon,
				obslist[iobs].lat,obslist[iobs].lon, &faz, &baz, &epi);
		else
		{
			epi = sqrt( pow(evnlist[iev].lat-obslist[iobs].lat,2.) +
				pow(evnlist[iev].lon-obslist[iobs].lon,2.) );
			baz = 0.;
		}

		if( evnlist[iev].flagsph[iph] > 0 )
		{
			fprintf(stdout,"%-3d %-5s %9.4f %9.4f %6.2f %6.2f %6.2f %6.2f *\n",
                i+1, obslist[iobs].stn, obslist[iobs].lon, obslist[iobs].lat, epi, 
				obslist[iobs].ti-torg, 
				evnlist[iev].ptrtime[iph], 
				evnlist[iev].tdiff[iph]);
		}
		else
		{
			fprintf(stdout,"%-3d %-5s %9.4f %9.4f %6.2f %6.2f %6.2f %6.2f\n",
                i+1, obslist[iobs].stn, obslist[iobs].lon, obslist[iobs].lat, epi, 
				obslist[iobs].ti-torg, 
				evnlist[iev].ptrtime[iph], 
				evnlist[iev].tdiff[iph]);
		}
   }
}


void quick_sort(double *a,int l,int r)
{
   double tmp, axis;
   int left, right;
   if(l<r)
   {
	left = l-1;
	right = r;
	axis = a[r];
	while (left < right)
	{
	   while (a[++left] < axis);
	   while (a[--right] > axis) if(right==0) break;

	   if (left >= right ) break;
	   tmp = a[left];
	   a[left] = a[right];
	   a[right] = tmp;
	}
	tmp = a[left];
	a[left] = a[r];
	a[r] = tmp;
	quick_sort(a,l,left-1);
	quick_sort(a,left+1,r);
   }
}

void quick_sort_id(double *a, int *id,int l,int r)
{
   double axis;
   int left, right,tmp;
   if(l<r)
   {
        left = l-1;
        right = r;
        axis = a[id[r]];
        while (left < right)
        {
           while (a[id[++left]] < axis);
           while (a[id[--right]] > axis) if(right==0) break;

           if (left >= right ) break;
           tmp = id[left];
           id[left] = id[right];
           id[right] = tmp;
        }
        tmp = id[left];
        id[left] = id[r];
        id[r] = tmp;
        quick_sort_id(a,id,l,left-1);
        quick_sort_id(a,id,left+1,r);
   }
}

double get_median(double *a, int n)
{
    int i;
    double med;
    for(i=0;i<n&&i<MAX_PH;i++) ttmp[i] = a[i];

    quick_sort(ttmp,0,n-1);

    if( (n+2)%2 == 1 )   // odd number
	med = ttmp[ (n-1)/2 ];
    else		// even number
	med = 0.5*(ttmp[ n/2 ] + ttmp[n/2-1]);

    return med;
}

void bind_all(EVNINFO *event)
{
   int i;
   double x,y,z;

   for(i=0;i<nobs;i++) 
	event->phlist[i] = i;
	   
   event->nph = nobs;

   assoc_all(event);

   if( verbose )
   {
	print_phase(0);
   }

   nevn = 1;


}

