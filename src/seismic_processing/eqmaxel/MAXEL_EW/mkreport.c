/*
 * verbose functions 
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
#include "ll2utm.h"

extern int nobs,nevn,ctype,verbose;
extern OBSINFO *obslist;
extern double Xmin,Xmax,Ymin,Ymax,dX,dY,dZ;
extern int nX,nY,nZ;


void make_evmapsc(EVNINFO *event)
{
    int iev,iph,ii;
    FILE *fp;
    double latmin,latmax,lonmin,lonmax;
    double lon,lat,x1,x2,x3,x4,y1,y2,y3,y4;

    char *color[10]={"blue","red","green","brown","gray","cyan","purple"};

    if( nevn == 0 ) return;

    if( ctype == DLTLN )
    {
        calc_km2deg(-800,-800,&x1,&y1);
        calc_km2deg(-800, 800,&x2,&y2);
        calc_km2deg( 800, 800,&x3,&y3);
        calc_km2deg( 800,-800,&x4,&y4);

        lonmin = MIN(x1,x2);
        lonmax = MAX(x3,x4);
        latmin = MIN(y1,y4);
        latmax = MAX(y2,y3);
        latmin -= 1.0;    latmax += 1.0;
        lonmin -= 1.0;    lonmax += 1.0;
    }

    fp = fopen("map.sc","w");
    fprintf(fp,"#!/bin/bash\npsfile=map.ps\nrange=%.1f/%.1f/%.1f/%.1f\n",
		   lonmin,lonmax,latmin,latmax);
    fprintf(fp,"gmt set FORMAT_GEO_MAP dddF\n");
    fprintf(fp,"gmt pscoast -JM15 -R$range -B5f1WsNE -Wthinnest -Dh -A500 -G230 -K -P -Y12 -X3 > $psfile\n");
    fprintf(fp,"gmt makecpt -Chaxby -I -T-10/0/0.1 > prob.cpt\n");
    fprintf(fp,"gmt psxy xy1.dat -J -R -Sc0.1 -Cprob.cpt -K -O >> $psfile\n");
    fprintf(fp,"gmt psxy -J -R -Wthinnest -K -O <<END >> $psfile\n");
    fprintf(fp,"%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\nEND\n",
                x1,y1, x2,y2, x3,y3, x4,y4, x1,y1);
    for(iph=0;iph<event->nph;iph++)
    {
	   ii = event->phlist[iph];
	   fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.2 -Gblue -O -K >> $psfile\n",
			obslist[ii].lon,obslist[ii].lat);
    }
    if( ctype == DLTLN )
    {
        calc_km2deg(event->x1-20, event->y1-20, &x1, &y1);
        calc_km2deg(event->x1-20, event->y1+20, &x2, &y2);
        calc_km2deg(event->x1+20, event->y1+20, &x3, &y3);
        calc_km2deg(event->x1+20, event->y1-20, &x4, &y4);

        lonmin = MIN(x1,x2);
        lonmax = MAX(x3,x4);
        latmin = MIN(y1,y4);
        latmax = MAX(y2,y3);
        latmin -= 0.05;    latmax += 0.05;
        lonmin -= 0.1;    lonmax += 0.1;
    }
    fprintf(fp,"gmt psxy -J -R -Wthin,black,4_4:0 -K -O <<END >> $psfile\n");
    fprintf(fp,"%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\nEND\n",
                x1-2,y1-1.5, x1-2,y2+1.5, x3+2,y2+1.5, x3+2,y1-1.5, x1-2,y1-1.5);

// Zoomed
    fprintf(fp,"range=%.2f/%.2f/%.2f/%.2f\n",x1-2,x3+2,y1-1.5,y2+1.5);
    fprintf(fp,"gmt pscoast -JM7 -R$range -B1f0.5WsNe -Wthinnest -Dh -A500 -G230 -K -O -Y-8 -X-1 >> $psfile\n");
    fprintf(fp,"gmt psxy xy1.dat -J -R -Sc0.25 -Cprob.cpt -K -O >> $psfile\n");
    fprintf(fp,"gmt psxy -J -R -Wthin,red -K -O <<END >> $psfile\n");
    fprintf(fp,"%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\nEND\n",
                x1,y1, x2,y2, x3,y3, x4,y4, x1,y1);
    for(iph=0;iph<event->nph;iph++)
    {
	   ii = event->phlist[iph];
	   fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.2 -W2,blue -O -K >> $psfile\n",
			obslist[ii].lon,obslist[ii].lat);
    }
    fprintf(fp,"gmt psscale -Cprob.cpt -D3.5/-2./6/0.4h -Eb -Al -B5:\"Normalized likelihood\": -O -K >> $psfile\n");

// Secondary search area
    fprintf(fp,"range=%.2f/%.2f/%.2f/%.2f\n",lonmin,lonmax,latmin,latmax);
    fprintf(fp,"gmt set FORMAT_GEO_MAP ddd.xF\n");
    fprintf(fp,"gmt pscoast -JM7 -R$range -B0.2f0.1wsNE -Wthinnest -Dh -A500 -G230 -K -O -X9 >> $psfile\n");
    fprintf(fp,"echo %f %f %f %f %f | gmt psxy -J -R -SE -Ggreen -Wthick,red,4_4:0 -O -K >> $psfile\n",
            event->cerlon,event->cerlat, event->cerazi,
            event->cerlmj*2., event->cerlmn*2.);
    fprintf(fp,"gmt makecpt -Chaxby -I -T-8.0/0/0.01 > prob.cpt\n");
    fprintf(fp,"gmt psxy xy2.dat -J -R -Sc0.15 -Cprob.cpt -K -O >> $psfile\n");
    fprintf(fp,"echo %.5f %.5f | gmt psxy -J -R -Sa0.4 -W2 -K -O >> $psfile\n",event->lon1,event->lat1);
    fprintf(fp,"echo %.5f %.5f | gmt psxy -J -R -Sa0.4 -W2,red -K -O >> $psfile\n",event->lon,event->lat);
    fprintf(fp,"gmt psxy -J -R -Wthin,red -K -O <<END >> $psfile\n");
    fprintf(fp,"%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\n%.4f %.4f\nEND\n",
                x1,y1, x2,y2, x3,y3, x4,y4, x1,y1);
    for(iph=0;iph<event->nph;iph++)
    {
	   ii = event->phlist[iph];
	   fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.3 -Gblue -O -K >> $psfile\n",
			obslist[ii].lon,obslist[ii].lat);
    }
    fprintf(fp,"gmt psscale -Cprob.cpt -D3.5/-2./6/0.4h -Eb -Al -B2:\"Normalized likelihood\": -O >> $psfile\n");


    /*
    if( ctype == DLTLN )
    {
        latmin=latmax=evnlist[0].lat;
        lonmin=lonmax=evnlist[0].lon;
        for(iev=0;iev<nevn;iev++)
        {
            latmin = MIN(latmin,evnlist[iev].lat);
            latmax = MAX(latmax,evnlist[iev].lat);
            lonmin = MIN(lonmin,evnlist[iev].lon);
            lonmax = MAX(lonmax,evnlist[iev].lon);
            for(iph=0;iph<evnlist[iev].nph;iph++)
            {
                ii = evnlist[iev].phlist[iph];
                latmin = MIN(latmin,obslist[ii].lat);
                latmax = MAX(latmax,obslist[ii].lat);
                lonmin = MIN(lonmin,obslist[ii].lon);
                lonmax = MAX(lonmax,obslist[ii].lon);
            }
        }
        latmin -= 0.5;    latmax += 0.5;
        lonmin -= 0.5;    lonmax += 0.5;
    }
    */


    /*
   for(iev=0;iev<nevn;iev++)
   {
	fprintf(fp,"echo %f %f | gmt psxy -J -R -Sa0.5 -G%s -O -K >> $psfile\n",
			evnlist[iev].lon,evnlist[iev].lat,color[iev]);

	for(iph=0;iph<evnlist[iev].nph;iph++)
	{
	   ii = evnlist[iev].phlist[iph];
	   fprintf(fp,"echo %f %f | gmt psxy -J -R -St0.5 -G%s -O -K >> $psfile\n",
			obslist[ii].lon,obslist[ii].lat,color[iev]);
	   fprintf(fp,"echo %f %f %s | gmt pstext -J -R -O -K >> $psfile\n",
			obslist[ii].lon,obslist[ii].lat-0.05,obslist[ii].stn);
	}
   }
	
   fprintf(fp,"echo %f %f %s | gmt pstext -J -R -O >> $psfile\n",
	    0.5*(lonmin+lonmax),0.5*(latmin+latmax)+0.45*(latmax-latmin),"MLE");
        */

   fprintf(fp,"gv $psfile &\n");

   fclose(fp);
}

void make_section(EVNINFO * event, double XX, double YY, double ZZ, double xs, double xe, double ys, double ye, double dh, double dz, int iev, int inc)
{
    int i,j,k,nx,ny,nz;
    double xi,yi,MLE;
    double lon,lat,zz;
    double lon2,lat2;
    double tx,ty,tz;
    char fname[10];
    float *lfnc;
    FILE *fp,*fp2;

    nx = (xe-xs+dh*0.1)/dh+1;
    ny = (ye-ys+dh*0.1)/dh+1;
    nz = nZ;
    lfnc = (float *)malloc(nx*ny*sizeof(float));

    if(inc>1) fp2=fopen("err.dat","w");

    sprintf(fname,"xy%d.dat",inc);
    fp = fopen(fname,"w");
    zz = ZZ;
    MLE = -1.;
    for(xi=xs,i=0; xi<=xe; xi+=dh,i++)
    {
        for(yi=ys,j=0; yi<=ye; yi+=dh,j++)
        {
            lfnc[i*ny+j] = lhf_hypb(xi,yi,zz,event,-1);
            if( MLE < lfnc[i*ny+j] ) 
            {
                MLE = lfnc[i*ny+j];
                tx = xi;
                ty = yi;
                tz = zz;
            }
        }
      }
    for(xi=xs,i=0; xi<=xe; xi+=dh,i++)
    {
        for(yi=ys,j=0; yi<=ye; yi+=dh,j++)
        {
            if( ctype == DLTLN )
            {
                calc_km2deg(xi,yi,&lon,&lat);
        	    fprintf(fp,"%8.4f %7.4f %8.4e\n",lon,lat,log10(lfnc[i*ny+j]/MLE));
                if( inc>1&&log10(lfnc[i*ny+j]/MLE)>-1. ) 
                    fprintf(fp2,"%8.4f %7.4f %8.4e\n",lon,lat,log10(lfnc[i*ny+j]/MLE));
            }
            else
            {
                fprintf(fp,"%8.4f %7.4f %8.4e\n",xi,yi,log10(lfnc[i*ny+j]/MLE));
                if( inc>1&&log10(lfnc[i*ny+j]/MLE)>-3. ) 
                    fprintf(fp2,"%8.4f %7.4f %8.4e\n",lon,lat,log10(lfnc[i*ny+j]/MLE));
            }
        }
    }
    fclose(fp);
    if(inc>1) fclose(fp2);

      // station location
    sprintf(fname,"stn%d.dat",iev+1);
    fp = fopen(fname,"w");
    for(i=0;i<event->nph;i++)
    {
        j = event->phlist[i];
        if( ctype == DLTLN )
        {
            fprintf(fp,"%s %8.4f %7.4f\n",obslist[j].stn,obslist[j].lon,obslist[j].lat);
	    }
        else
            fprintf(fp,"%s %8.4f %7.4f\n",obslist[j].stn,obslist[j].x,obslist[j].y);
    }
    fclose(fp);
    free(lfnc);
}
