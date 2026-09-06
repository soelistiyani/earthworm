/*
 * station sampling functions
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
#include "ll2utm.h"

struct polygon{
	int npts;
	double *x,*y;
};
typedef struct polygon PLYGN;

int nply;
PLYGN *ply;

extern OBSINFO *obslist;
extern double *lat,*lon,*dst;
extern int verbose;
double ry_max,ry_min,rx_max,rx_min;

void set_subregion(EVNINFO *tevn);
void check_subregion(double xmin,double ymin,double xmax,double ymax);
void check_sample(EVNINFO *tevn);

void sample_obsph(EVNINFO *tevn)
{
	int i,j,n,iobs,iph,ipl,snobs,maxPH;
	int sph_region[NSUB];
    double maxD,dist;

	switch (NSUB)
	{
		case 8:
			n = 3;
			break;
		case 16:
			n = 4;
			break;
		default:
			fprintf(stderr,"Unknown subregion\n");
			exit(-1);
	}

	if( tevn->nph <= NSUB )
	{
		tevn->snph = tevn->nph;
		for(iph=0;iph<tevn->nph;iph++)
		{
			tevn->sphlist[iph] = tevn->phlist[iph];
			tevn->flagsph[iph] = 1;
		}
		return;
	}

    tevn->snph = 0;
//determine subregion of each phase
	for(i=0;i<tevn->nph;i++)
	{
        iph = tevn->phlist[i];

		tevn->phregion[i] = -1;
		tevn->flagsph[i] = 0;
		tevn->sphlist[i]  = -1;
        if( obslist[iph].ti >= ATLIM ) continue;
		for(ipl=0;ipl<NSUB;ipl++)
		{
			if( pnpoly(n, ply[ipl].x, ply[ipl].y, obslist[iph].x, obslist[iph].y) != 0 )
			{
				tevn->phregion[i] = ipl;
				break;
			}
		}
	}
	
	snobs = 0;
	for(ipl=0;ipl<NSUB;ipl++) sph_region[ipl]=-1;


// simple selection
	for(i=0;i<tevn->nph;i++)
	{
		ipl = tevn->phregion[i];
		if(ipl < 0 )
		{
			iph = tevn->phlist[i];
            if( obslist[iph].ti < ATLIM )
			fprintf(stderr,"Sample Error: %d %4s %.2f (sec) %.4f %.4f\n",
                    i,obslist[iph].stn,obslist[iph].ti,obslist[iph].x,obslist[iph].y);
		}
		else if(sph_region[ipl]<0)
		{
			iph = tevn->phlist[i];
            if( obslist[iph].ti < ATLIM )
            {
                sph_region[ipl] = tevn->phlist[i]; 
                tevn->flagsph[i] = 1;
                tevn->sphlist[snobs] = i;
                snobs += 1;
            }
            else if(verbose)
            {
                printf("Late arrival: %s %.2f\n",obslist[iph].stn,obslist[iph].ti);
            }
		}
	}

// fill black subregion
	for(i=0;i<tevn->nph&&snobs<NSUB;i++)
//	for(i=tevn->nph-1;i>=0&&snobs<NSUB;i--)
	{
		if( tevn->flagsph[i] == 0 )
		{
			iph = tevn->phlist[i];
            if( obslist[iph].ti < ATLIM )
            {
                tevn->flagsph[i] = 1;
                tevn->sphlist[snobs] = i;
                snobs += 1;
                if(snobs == NSUB ) break;
            }
		}
	}

	tevn->snph = snobs;

    if(verbose==2) check_sample(tevn);
}

/* 16 rectangluar subregion */

void set_subregion(EVNINFO *tevn)
{
	int i,j,k,ipl,iph;

    for(i=0;i<tevn->nph;i++)
    {
        iph = tevn->phlist[i];
        if( obslist[iph].ti < ATLIM )
        {
            ry_max=ry_min=obslist[iph].y;
            rx_max=rx_min=obslist[iph].x;
            break;
        }
    }

	for(;i<tevn->nph;i++)
	{
		iph = tevn->phlist[i];
        if( obslist[iph].ti < ATLIM )
        {
            if(rx_max<obslist[iph].x) rx_max = obslist[iph].x;
            if(rx_min>obslist[iph].x) rx_min = obslist[iph].x;

            if(ry_max<obslist[iph].y) ry_max = obslist[iph].y;
            if(ry_min>obslist[iph].y) ry_min = obslist[iph].y;
        }
	}
	rx_max += 0.1;
	ry_max += 0.1;
	rx_min -= 0.1;
	ry_min -= 0.1;

	nply = NSUB;
	ply = (PLYGN *)malloc(nply*sizeof(PLYGN));
	for(i=0,ipl=0;i<4;i++)
	{
		for(j=0;j<4;j++,ipl++)
		{
			ply[ipl].npts = 4;
			ply[ipl].x=(double *)malloc(4*sizeof(double));
			ply[ipl].y=(double *)malloc(4*sizeof(double));

			ply[ipl].x[0]=rx_min+(rx_max-rx_min)*0.25*i;
			ply[ipl].x[1]=rx_min+(rx_max-rx_min)*0.25*(i+1.);
			ply[ipl].x[2]=rx_min+(rx_max-rx_min)*0.25*(i+1.);
			ply[ipl].x[3]=rx_min+(rx_max-rx_min)*0.25*i;

			ply[ipl].y[0]=ry_min+(ry_max-ry_min)*0.25*j;
			ply[ipl].y[1]=ry_min+(ry_max-ry_min)*0.25*j;
			ply[ipl].y[2]=ry_min+(ry_max-ry_min)*0.25*(j+1.);
			ply[ipl].y[3]=ry_min+(ry_max-ry_min)*0.25*(j+1.);
		}
	}
	if( verbose == 2 ) check_subregion(rx_min, ry_min, rx_max, ry_max);
}

/*
void set_subregion(EVNINFO *tevn)
{
	int i,j,k,ipl,iph;

	iph = tevn->phlist[0];

	ry_max=ry_min=obslist[iph].y;
	rx_max=rx_min=obslist[iph].x;

	for(i=1;i<tevn->nph;i++)
	{
		iph = tevn->phlist[i];
		if(rx_max<obslist[iph].x) rx_max = obslist[iph].x;
		if(rx_min>obslist[iph].x) rx_min = obslist[iph].x;

		if(ry_max<obslist[iph].y) ry_max = obslist[iph].y;
		if(ry_min>obslist[iph].y) ry_min = obslist[iph].y;
	}
	rx_max += 0.1;
	ry_max += 0.1;
	rx_min -= 0.1;
	ry_min -= 0.1;

	nply = NSUB;
	ply = (PLYGN *)malloc(nply*sizeof(PLYGN));
	
	ply[0].npts = 3;
	ply[0].x=(double *)malloc(3*sizeof(double));
	ply[0].y=(double *)malloc(3*sizeof(double));
	ply[0].x[0]=rx_min;
	ply[0].x[1]=rx_min+(rx_max-rx_min)*0.5;
	ply[0].x[2]=rx_min;
	ply[0].y[0]=ry_min;
	ply[0].y[1]=ry_min;
	ply[0].y[2]=ry_min+(ry_max-ry_min)*0.5;
	
	ply[1].npts = 3;
	ply[1].x=(double *)malloc(3*sizeof(double));
	ply[1].y=(double *)malloc(3*sizeof(double));
	ply[1].x[0]=rx_min+(rx_max-rx_min)*0.5;
	ply[1].x[1]=rx_min+(rx_max-rx_min)*0.5;
	ply[1].x[2]=rx_min;
	ply[1].y[0]=ry_min;
	ply[1].y[1]=ry_min+(ry_max-ry_min)*0.5;
	ply[1].y[2]=ry_min+(ry_max-ry_min)*0.5;
	
	ply[2].npts = 3;
	ply[2].x=(double *)malloc(3*sizeof(double));
	ply[2].y=(double *)malloc(3*sizeof(double));
	ply[2].x[0]=rx_min+(rx_max-rx_min)*0.5;
	ply[2].x[1]=rx_max;
	ply[2].x[2]=rx_min+(rx_max-rx_min)*0.5;
	ply[2].y[0]=ry_min;
	ply[2].y[1]=ry_min+(ry_max-ry_min)*0.5;
	ply[2].y[2]=ry_min+(ry_max-ry_min)*0.5;
	
	ply[3].npts = 3;
	ply[3].x=(double *)malloc(3*sizeof(double));
	ply[3].y=(double *)malloc(3*sizeof(double));
	ply[3].x[0]=rx_min+(rx_max-rx_min)*0.5;
	ply[3].x[1]=rx_max;
	ply[3].x[2]=rx_max;
	ply[3].y[0]=ry_min;
	ply[3].y[1]=ry_min;
	ply[3].y[2]=ry_min+(ry_max-ry_min)*0.5;
	
	ply[4].npts = 3;
	ply[4].x=(double *)malloc(3*sizeof(double));
	ply[4].y=(double *)malloc(3*sizeof(double));
	ply[4].x[0]=rx_min;
	ply[4].x[1]=rx_min+(rx_max-rx_min)*0.5;
	ply[4].x[2]=rx_min;
	ply[4].y[0]=ry_min+(ry_max-ry_min)*0.5;
	ply[4].y[1]=ry_max;
	ply[4].y[2]=ry_max;
	
	ply[5].npts = 3;
	ply[5].x=(double *)malloc(3*sizeof(double));
	ply[5].y=(double *)malloc(3*sizeof(double));
	ply[5].x[0]=rx_min;
	ply[5].x[1]=rx_min+(rx_max-rx_min)*0.5;
	ply[5].x[2]=rx_min+(rx_max-rx_min)*0.5;
	ply[5].y[0]=ry_min+(ry_max-ry_min)*0.5;
	ply[5].y[1]=ry_min+(ry_max-ry_min)*0.5;
	ply[5].y[2]=ry_max;
	
	ply[6].npts = 3;
	ply[6].x=(double *)malloc(3*sizeof(double));
	ply[6].y=(double *)malloc(3*sizeof(double));
	ply[6].x[0]=rx_min+(rx_max-rx_min)*0.5;
	ply[6].x[1]=rx_max;
	ply[6].x[2]=rx_min+(rx_max-rx_min)*0.5;
	ply[6].y[0]=ry_min+(ry_max-ry_min)*0.5;
	ply[6].y[1]=ry_min+(ry_max-ry_min)*0.5;
	ply[6].y[2]=ry_max;
	
	ply[7].npts = 3;
	ply[7].x=(double *)malloc(3*sizeof(double));
	ply[7].y=(double *)malloc(3*sizeof(double));
	ply[7].x[0]=rx_max;
	ply[7].x[1]=rx_max;
	ply[7].x[2]=rx_min+(rx_max-rx_min)*0.5;
	ply[7].y[0]=ry_min+(ry_max-ry_min)*0.5;
	ply[7].y[1]=ry_max;
	ply[7].y[2]=ry_max;

	if( verbose == 3 ) check_subregion(rx_min, ry_min, rx_max, ry_max);
}
*/

void check_subregion(double xmin,double ymin,double xmax,double ymax)
{
	int i,j,n;
    double lon,lat;
	FILE *fp;

	switch (NSUB)
	{
		case 8:
			n = 3;
			break;
		case 16:
			n = 4;
			break;
		default:
			fprintf(stderr,"Unknown subregion\n");
			exit(-1);
	}

	fp = fopen("check_sr.sc","w");
	fprintf(fp,"#!/bin/bash\npsfile=checksr.ps\n");
	fprintf(fp,"gmt psbasemap -JX10 -R%.0f/%.0f/%.0f/%.0f -B100 -P -K>$psfile\n",
			xmin-0.5, xmax+0.5, ymin-0.5, ymax+0.5);
	for(i=0;i<NSUB-1;i++)
	{
		fprintf(fp,"gmt psxy -J -R -W1,red -G%d/%d/%d -K -O <<END>> $psfile\n",
			(int)(255.*(i+1)/NSUB), (int)(255.*(i+1)/NSUB), (int)(255.*(i+1)/NSUB));
		for(j=0;j<n-1;j++) fprintf(fp,"%.4f %.4f\n",ply[i].x[j], ply[i].y[j]);
		fprintf(fp,"%.4f %.4f\nEND\n\n",ply[i].x[j], ply[i].y[j]);
	}
	fprintf(fp,"gmt psxy -J -R -W1,red -G%d/%d/%d -O <<END>> $psfile\n",
			(int)(255.*(i+1)/NSUB), (int)(255.*(i+1)/NSUB), (int)(255.*(i+1)/NSUB));
	for(j=0;j<n-1;j++) fprintf(fp,"%.4f %.4f\n",ply[i].x[j], ply[i].y[j]);
	fprintf(fp,"%.4f %.4f\nEND\n\n",ply[i].x[j], ply[i].y[j]);
	fclose(fp);

}

void check_sample(EVNINFO *tevn)
{
    FILE *fp;
    int iph,i,j,n;
    double latmin,latmax,lonmin,lonmax;
    double lon,lat,x1,x2,x3,x4,y1,y2,y3,y4;

	switch (NSUB)
	{
		case 8:
			n = 3;
			break;
		case 16:
			n = 4;
			break;
		default:
			fprintf(stderr,"Unknown subregion\n");
			exit(-1);
	}

    i = tevn->phlist[0];
    lonmin = lonmax = obslist[i].lon;
    latmin = latmax = obslist[i].lat;

    for(iph=1;iph<tevn->nph;iph++)
    {
        i = tevn->phlist[iph];
        lonmin = MIN(lonmin,obslist[i].lon);
        lonmax = MAX(lonmax,obslist[i].lon);
        latmin = MIN(latmin,obslist[i].lat);
        latmax = MAX(latmax,obslist[i].lat);
    }
    latmin -= 0.5;    latmax += 0.5;
    lonmin -= 0.5;    lonmax += 0.5;

	fp = fopen("smap.sc","w");
    fprintf(fp,"#!/bin/bash\npsfile=stnmap.ps\nrange=%.1f/%.1f/%.1f/%.1f\n",
		   lonmin,lonmax,latmin,latmax);
    fprintf(fp,"gmt set FORMAT_GEO_MAP dddF\n");
    fprintf(fp,"gmt pscoast -JM12 -R$range -B2f1WSNE -Wthinnest -Dh -A500 -G230 -K -P -Y8 -X3 > $psfile\n");
    for(iph=0;iph<tevn->nph;iph++)
    {
	   i = tevn->phlist[iph];
       if( tevn->flagsph[iph] == 1 )
            fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.3 -Wthinnest,black -Gblack -O -K >> $psfile\n",
		    	obslist[i].lon,obslist[i].lat);
       else if( obslist[i].ti >= ATLIM )
            fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.3 -Wthinnest,black -Gwhite -O -K >> $psfile\n",
		    	obslist[i].lon,obslist[i].lat);
       else 
            fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.3 -Wthinnest,black -Ggray -O -K >> $psfile\n",
		    	obslist[i].lon,obslist[i].lat);
    }
    fprintf(fp,"gmt psxy -J -R -Wthin,10_5:0 -O <<END >> $psfile\n");
	for(i=0;i<NSUB;i++)
	{
		for(j=0;j<n;j++) 
        {
            calc_km2deg(ply[i].x[j], ply[i].y[j],&lon, &lat);
            fprintf(fp,"%.4f %.4f\n",lon,lat);
        }
        calc_km2deg(ply[i].x[0], ply[i].y[0],&lon, &lat);
            fprintf(fp,"%.4f %.4f\n",lon,lat);
        fprintf(fp,">\n");
    }
    fprintf(fp,"END\ngv $psfile &\n");

	fclose(fp);

}
