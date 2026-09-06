/*
 * compute maximum likelihood functions
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"
#include "ll2utm.h"

extern int nobs,verbose,ctype,nX,nY,nZ,nevn,minphid;
extern OBSINFO *obslist;
extern EVNINFO evnlist[MAX_EV];
extern double Xmin,Xmax,Ymin,Ymax,Zmin,Zmax;
extern double Xc,Yc,dX,dY,dZ,halfX,halfY,halfZ;
extern double crt_td;

double lhf_hypb(double xx,double yy,double zz,EVNINFO *tevn, int phid);

double ttmp[MAX_PH],btmp[MAX_PH],tlst[MAX_PH];
void phase_statistics(int istep, EVNINFO *tevn, int phid);


int run_mle(int istep,EVNINFO *tevn, int phid, double *x, double *y,double *z, int icheck)
{
   int i,j,k,ngrd,nhgrd;
   FILE *fp;
   char tbuf[BUFSIZ];
   double dist, xx, yy, zz, MLH, pbaz;
   double tlon,tlat;
   double *Pspace;
   double xmin,xmax,ymin,ymax,dx,dy,depth;
   int nx,ny;

   if( istep == 1 )
   {
	xmin = Xmin; xmax = Xmax;  dx = dX;
	ymin = Ymin; ymax = Ymax;  dy = dY;
   }
   else
   {
	xmin = *x-20.;
	xmax = *x+20.;
	ymin = *y-20.;
	ymax = *y+20.;
	dx = 2.;
	dy = 2.;
	if( xmin < Xmin ) xmin = Xmin;
	if( xmax > Xmax ) xmax = Xmax;
	if( ymin < Ymin ) ymin = Ymin;
	if( ymax > Ymax ) ymax = Ymax;
   }

   for(dist=xmin,i=0;dist<=xmax;dist+=dx) i++;
   nx = i;
   for(dist=ymin,i=0;dist<=ymax;dist+=dy) i++;
   ny = i;

   ngrd = nx*ny;
   nhgrd = nx*ny;

   if(verbose && icheck != 1 )
   {
	fprintf(stderr,"%-7.1f/%-7.1f %-7.1f/%-7.1f (%d/%d : %d)\n",
	xmin,xmax,ymin,ymax,nx,ny,ngrd);
   }

   Pspace = (double *)malloc(ngrd*sizeof(double));

   for(i=0;i<ngrd;i++) 
   {
	Pspace[i] = 1.;
   }
   
   MLH = 0.;
   depth = 8.;
   for(xx=xmin,i=0,j=0;xx<=xmax;xx+=dx)
   {
	for(yy=ymin;yy<=ymax;yy+=dy,j++,i++) 
	{
		Pspace[i]  = lhf_hypb(xx,yy,depth,tevn,phid);

		if( MLH < Pspace[i] )
		{
                   MLH = Pspace[i];
                   *x = xx;
                   *y = yy;
                   *z = depth;
		}
	}
   }

   tevn->x = *x;
   tevn->y = *y;
   tevn->z = *z;
//   tevn->MLE = pow(MLH/tevn->snph,1./tevn->snph);
   tevn->MLE = MLH;
   
   if( ctype == KMXY ) 
	tlon = *x, tlat = *y;
   else
	calc_km2deg(*x,*y,&tlon,&tlat);
   tevn->lat = tlat;
   tevn->lon = tlon;

   phase_statistics(istep, tevn,phid);

   if(verbose) 
   {
	fprintf(stdout,"LOCATION (step %d/%2d phs) %8.4f %7.4f (%.1f,%.1f) (%.4f/%.4f) MLE %f\n",
		   istep, tevn->nph, tlon,tlat,*x,*y,tevn->stmad,tevn->atmad, tevn->MLE);
   }
	
   if( verbose == 1  && icheck != 1 )
   {
	sprintf(tbuf,"out%d.dat",istep);
	fp = fopen(tbuf,"w");

	for(xx=xmin,i=0;xx<=xmax;xx+=dx)
	{
	   for(yy=ymin;yy<=ymax;yy+=dy,i++)
	   {
		if( ctype == KMXY )
		    tlon = xx, tlat = yy;
		else 
	            calc_km2deg(xx,yy,&tlon,&tlat);
		    fprintf(fp,"%3.2f %3.2f %g\n",tlon,tlat,log10(Pspace[i]/MLH)/1.);
	   }
	}
	fclose(fp);
   }
   free(Pspace);

// too close to the boundary
   if( fabs(*x-xmin) < dx*2 || fabs(*x-xmax) < dx*2 
	|| fabs(*y-ymin) < dy*2 || fabs(*y-ymax) < dy*2 )
   {
	return 0;
   }
   return 1;
}

// dof: degree of freedom
double student_hypb(double ti, double tobs, int dof)
{
   double stdt;

   stdt = pow( 1. + pow(ti-tobs,2.)/dof, -0.5*(1.+dof) );
   return stdt;
}

double lhf_hypb(double xx,double yy,double zz,EVNINFO *tevn, int phid)
{
   int i,j,i1,i2,idx;
   double likely,lsum,d1,d2,p1,p2,tt,wgt;
   double PDF[MAXPDF];
   int nph;
   nph = tevn->snph;

   for(i=0;i<nph;i++)
   {
	i1 = tevn->sphlist[i];
	for(j=i+1;j<nph;j++)
	{
	   idx = nph*i-i*(i+1)/2+j-i-1;
	   i2 = tevn->sphlist[j];
	   wgt = obslist[i1].wgt*obslist[i2].wgt;
	   if( wgt < 0.1 )
	   {
		 PDF[idx] = 1.;
		 continue;
	   }
	   d1 = sqrt( pow(obslist[i1].x-xx,2.) + pow(obslist[i1].y-yy,2.) );
	   p1 = itrtime(d1);
	   d2 = sqrt( pow(obslist[i2].x-xx,2.) + pow(obslist[i2].y-yy,2.) );
	   p2 = itrtime(d2);
	   tt = (obslist[i1].ti-obslist[i2].ti);
	   PDF[idx] = wgt*student_hypb(p1-p2,tt,1);
	}
   }

   lsum = 0.;
   for(i=0;i<nph;i++)
   {
	likely = 1.;
	for(j=0;j<nph;j++)
	{
	   if( i == j ) continue;

	   if( i > j ) idx = nph*j-j*(j+1)/2+i-j-1;	
           else        idx = nph*i-i*(i+1)/2+j-i-1;

	   likely *= PDF[idx];
	}
	lsum += likely;
   }
   likely = lsum;

   return likely;
}


void phase_statistics(int istep, EVNINFO *tevn, int phid)
{
   int iph,iph2,i,tnph,itout,ibout;
   double lat,lon,faz,baz,epi;
   double tmed,bmed,tabs,babs;
   double max_terr;

   tmed=bmed=tabs=babs=0.;
   itout = ibout = -1;

   if( phid >= 0 ) tnph = tevn->snph + 1;
   else            tnph = tevn->snph;
// make list 
   for(iph=0;iph<tevn->snph;iph++)
   {
		i = tevn->sphlist[iph];
		if( ctype == DLTLN)
			get_azimuth(tevn->lat,tevn->lon, obslist[i].lat,obslist[i].lon, &faz, &baz, &epi);
		else
		{
			epi = sqrt( pow(tevn->y - obslist[i].y,2.) + 
			pow(tevn->x - obslist[i].x,2.) );
			baz = 0.;
		}
		tevn->tdiff[iph] = obslist[i].ti - itrtime(epi);
   }
   if( phid >= 0 )
   {
		i = phid;
		if( ctype == DLTLN )
			get_azimuth(tevn->lat,tevn->lon, obslist[i].lat,obslist[i].lon, &faz, &baz, &epi);
		else
		{
			epi = sqrt( pow(tevn->y - obslist[i].y,2.) + 
					pow(tevn->x - obslist[i].x,2.) );
			baz = 0.;
		}
		tevn->tdiff[iph] = obslist[i].ti - itrtime(epi);
   }

/* determine orgin time from minimum observed time - t_median */
   tmed = get_median(tevn->tdiff, tnph);
   tevn->orgt   = tmed;
   tevn->origin_epoch = obslist[0].epoch+tmed;
//   tevn->et_ot  = add_time_to_org( obslist[minphid].et, tmed);

/* determine time residual between time difference (obs - tcalc) and t_median */
   max_terr = fabs( (tevn->tdiff[0]-tmed) );
   iph2 = 0;
   for(i=0;i<tnph;i++)
   {
		tlst[i] = fabs(tevn->tdiff[i]-tmed);
		if( max_terr < tlst[i] ) 
		{
			max_terr = tlst[i];
			iph2 = i;
		}
   }
/* determine median absolute deviation of differential times */
   tevn->stmad  = get_median(tlst,tnph);

   if( max_terr > crt_td ) tevn->tout = iph2;
   else                    tevn->tout = -1;

   for(iph=0;iph<tevn->nph;iph++)
   {
		i = tevn->phlist[iph];
		if( ctype == DLTLN)
			get_azimuth(tevn->lat,tevn->lon, obslist[i].lat,obslist[i].lon, &faz, &baz, &epi);
		else
		{
			epi = sqrt( pow(tevn->y - obslist[i].y,2.) + 
			pow(tevn->x - obslist[i].x,2.) );
			baz = 0.;
		}
		tevn->ptrtime[iph] = itrtime(epi);
		tevn->tdiff[iph] = obslist[i].ti-itrtime(epi)-tmed;
        tlst[iph] = fabs(tevn->tdiff[iph]);
//        fprintf(stderr,"%s %8.4f %8.4f %8.4f\n",
//                obslist[i].stn,obslist[i].ti-tmed,tevn->ptrtime[iph],tevn->tdiff[iph]);
   }
   if( phid >= 0 )
   {
		i = phid;
		if( ctype == DLTLN )
			get_azimuth(tevn->lat,tevn->lon, obslist[i].lat,obslist[i].lon, &faz, &baz, &epi);
		else
		{
			epi = sqrt( pow(tevn->y - obslist[i].y,2.) + 
					pow(tevn->x - obslist[i].x,2.) );
			baz = 0.;
		}
		tevn->ptrtime[iph] = itrtime(epi);
   }
   tevn->atmad  = get_median(tlst,tevn->nph);
}
