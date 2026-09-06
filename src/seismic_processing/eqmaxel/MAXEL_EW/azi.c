/*
 * Compute azimuth, back azimuth, and epicentral distance between two location
 * from http://williams.best.vwh.net/gccalc.htm
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double D2R = M_PI / 180.;   // deg2rad
double R2D = 180. / M_PI;   // rad2deg
double erad = 6378137.;
double dconv = 1.852;


double mod(double x, double y)
{
    return x-y*floor(x/y);
}
double modcrs(double x)
{
    return mod(x,2*M_PI);
}

// evlat, evlon,  stlat, stlon
void get_azimuth(double lat1, double lon1, double lat2, double lon2,double *faz, double *baz, double *dist)
{
    double result = 0.;
    double a,invf,EPS,s;
    double r,f,tu1,tu2,cu1,su1,cu2,su2,s1,b1,f1;
    double x,sx,cx,sy,cy,y,sa,c2a,cz,e,c,d;
    int iter=1,maxiter=100;
    
    a = erad/1000./dconv;
    invf = 298.257223563;
    EPS = 0.00000000005;
    f = 1./invf;

    lat1 *= D2R;
    lon1 *= D2R;
    lat2 *= D2R;
    lon2 *= D2R;

    if( lat1+lat2==0. && fabs(lon1-lon2) == M_PI) lat1+=0.0001;
    if( lat1==lat2    && (lon1==lon2 || fabs(fabs(lon1-lon2)-2*M_PI) < EPS ))
    {
	*dist=0.;
	*faz=0.;
	*baz=M_PI;
    }

    r = 1. - f;
    tu1 = r * tan (lat1);
    tu2 = r * tan (lat2);
    cu1 = 1. / sqrt (1. + tu1 * tu1);
    su1 = cu1 * tu1;
    cu2 = 1. / sqrt (1. + tu2 * tu2);
    s1 = cu1 * cu2;
    b1 = s1 * tu2;
    f1 = b1 * tu1;
    x = lon2 - lon1;
    d = x + 1; // force one pass
    while ((fabs(d - x) > EPS) && (iter < maxiter))
    {
      iter=iter+1;
      sx = sin (x);
//       alert("sx="+sx)
      cx = cos (x);
      tu1 = cu2 * sx;
      tu2 = b1 - su1 * cu2 * cx;
      sy = sqrt(tu1 * tu1 + tu2 * tu2);
      cy = s1 * cx + f1;
      y = atan2 (sy, cy);
      sa = s1 * sx / sy;
      c2a = 1 - sa * sa;
      cz = f1 + f1;
      if (c2a > 0.)
	cz = cy - cz / c2a;
      e = cz * cz * 2. - 1.;
      c = ((-3. * c2a + 4.) * f + 4.) * c2a * f / 16.;
      d = x;
      x = ((e * cy * c + cz) * sy * c + y) * sa;
      x = (1. - c) * x * f + lon2 - lon1;
    }
    *faz = modcrs(atan2(tu1, tu2))*R2D;
    *baz = modcrs(atan2(cu1 * sx, b1 * cx - su1 * cu2) + M_PI)*R2D;
    x = sqrt ((1 / (r * r) - 1) * c2a + 1);
    x +=1;
    x = (x - 2.) / x;
    c = 1. - x;
    c = (x * x / 4. + 1.) / c;
    d = (0.375 * x * x - 1.) * x;
    x = e * cy;
    s = ((((sy*sy*4.-3.)*(1.-e-e)*cz*d/6.-x)*d/4.+cz)*sy*d+y)*c*a*r;
    *dist=s*dconv;
}

