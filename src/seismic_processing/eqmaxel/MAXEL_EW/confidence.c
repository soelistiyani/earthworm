/*
 * Compute error ellipse based on the PCA
 * by Dong-Hoon Sheen
 *    Chonnam National University
 *
 *    Change the size and dh of the check grid in find_confidence
 *    20 -> 80  (xs, xe, ys, ye)
 *    dh(2) -> dh(8)
*/
#include "mle.h"
#include "ll2utm.h"

void error_ellipse(float **pts,int N,EVNINFO *e);
float **make_covmat(float **A,float **ATA,float *cent,int nrow, int ncol);
void get_mean(float **mat,int ndat,float *mx, float *my);

void jacobi(float **a, int n, float d[], float **v, int *nrot);
void eigsrtABS(float d[], float **v, int n);

extern int verbose;

void find_confidence(EVNINFO *event)
{
    int i,j,k,nx,ny,ipts,npts;
    double xs,xe,ys,ye;
    double xi,yi,MLE,crt;
    double lon,lat;
    double lon2,lat2;
    double tx,ty,dh,lhf;
    float **cpts;

    xs = event->x2-80;
    xe = event->x2+80;
    ys = event->y2-80;
    ye = event->y2+80;
    MLE =event->MLE;
    dh = 8.;
    crt = -2.;

    nx = (xe-xs+dh*0.1)/dh+1;
    ny = (ye-ys+dh*0.1)/dh+1;

    cpts = matrix(1,nx*ny,1,2);

    for(xi=xs,i=0,ipts=0; xi<=xe; xi+=dh,i++)
    {
        for(yi=ys,j=0; yi<=ye; yi+=dh,j++)
        {
            lhf = lhf_hypb(xi,yi,8.,event,-1)/MLE;
            if( log10(lhf) > crt )
            {
                ipts++;
                cpts[ipts][1] = xi;
                cpts[ipts][2] = yi;
            }
        }
    }
    npts = ipts;

    if( verbose ) printf("Confidence interval points: %d\n",npts);
    error_ellipse(cpts,npts,event);

    if( event->cerarea < 1. )
    {
      xs = event->x2-20;
      xe = event->x2+20;
      ys = event->y2-20;
      ye = event->y2+20;
      dh = 2.;
    
      for(xi=xs,i=0,ipts=0; xi<=xe; xi+=dh,i++)
      {
        for(yi=ys,j=0; yi<=ye; yi+=dh,j++)
        {
            lhf = lhf_hypb(xi,yi,8.,event,-1)/MLE;
            if( log10(lhf) > crt )
            {
                ipts++;
                cpts[ipts][1] = xi;
                cpts[ipts][2] = yi;
            }
        }
      }
      npts = ipts;

      if( verbose ) printf("Confidence interval points: %d\n",npts);
      error_ellipse(cpts,npts,event);
    }
}

void error_ellipse(float **pts,int N,EVNINFO *event)
{
    int nrot;
    double lon,lat,chi2;
    float *cpts,**cov;
    float **egvect,*egval;

    cpts = vector(1,2);
    cov = matrix(1,2,1,2);
    egvect = matrix(1,2,1,2);
    egval  = vector(1,2);

    chi2 = 4.605;  // 90 %

    get_mean(pts,N,&cpts[1],&cpts[2]);
    make_covmat(pts,cov,cpts,N,2);

    jacobi(cov,2,egval,egvect,&nrot);
    eigsrtABS(egval,egvect,2);

    calc_km2deg(cpts[1],cpts[2],&lon,&lat);

    event->cerazi = atan2(egvect[1][1],egvect[2][1])*180./M_PI;
    event->cerlmj = sqrt(chi2*fabs(egval[1]));
    event->cerlmn = sqrt(chi2*fabs(egval[2]));
    event->cerlon = lon;
    event->cerlat = lat;
    event->cerarea= M_PI * event->cerlmj * event->cerlmn;
}


float **make_covmat(float **A,float **ATA,float *cent,int nrow, int ncol)
{
    int i,j,k;

    for(i=1;i<=ncol;i++)
    {
        for(j=1;j<=ncol;j++) 
        {
            ATA[i][j] = 0.;
            for(k=1;k<=nrow;k++)
            {
                ATA[i][j] += (A[k][i]-cent[i]) * (A[k][j]-cent[j]);
            }
            ATA[i][j] /= (float)nrow;
        }
    }
    return ATA;
}

void get_mean(float **mat,int ndat,float *mx, float *my)
{
    int i;
    *mx = *my = 0.;
    for(i=1;i<=ndat;i++)
    {
        *mx += mat[i][1];
        *my += mat[i][2];
    }
    *mx /= (float)ndat;
    *my /= (float)ndat;
}
