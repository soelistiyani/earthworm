#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include "basic.h"

#define MAX_PH 200
#define MAX_EV 10

#define DLTLN 0       // input lat. and lon. in degree
#define KMXY  1       // input x and y in km

#define NSUB 16     // 16 rectangular subregions
//#define NSUB 8	      //  8 trianglular subregions

#define MAXPDF 500

#define ATLIM 60.

struct stninfo{
	char stn[7];
	double lat,lon,elev;
	double rx,ry;
};
typedef struct stninfo STNINFO;


struct obsinfo{
	char stn[7],ntw[3];
	int evid;
	double lat,lon,elev;
	double x,y,z,ti,wgt;
	double epoch;   /* absolute epoch of phase */
};
typedef struct obsinfo OBSINFO;

struct evninfo{
	double lat,lon,x,y,z;
    double lat1,lon1;
 	double x1,y1,z1;
 	double x2,y2,z2;
	double orgt,MLE;
	double atmad,stmad;
    double pgap,sgap,mindist;
    double cerazi,cerlmj,cerlmn,cerlat,cerlon,cerarea;
	double tdiff[MAX_PH],ptrtime[MAX_PH];
	int nph,phlist[MAX_PH],phregion[MAX_PH],flagsph[MAX_PH];
	int snph,sphlist[MAX_PH];
	int tout;
    double origin_epoch;
};
typedef struct evninfo EVNINFO;

void read_control(char *iname);


void read_stnlist(char *iname);
int find_stn(char *tstn, char *ntw);

void read_obslist(char *iname);

int run_mle(int istep,EVNINFO *tevn, int phid, double *x, double *y,double *z, int icheck);
double lhf_hypb(double xx,double yy,double zz,EVNINFO *tevn, int phid);


void read_trtable(char *trfile);
double itrtime(double r);

double baz_res(double tbaz, double sbaz);

void calc_deg2km(double Long, double Lat, double *dx, double *dy);

void get_azimuth(double lat1, double lon1, double lat2, double lon2,double *faz, double *baz, double *dist);


void init_event(void);
void bind_all(EVNINFO *event);
int  assoc_all(EVNINFO *tevn);
void print_phase(int iev);
double get_median(double *a, int n);

void make_evmapsc(EVNINFO * e);
void make_section(EVNINFO * e, double XX, double YY, double ZZ, double xs, double xe, double ys, double ye, double dh, double dz, int iev, int inc);

void find_confidence(EVNINFO *e);
void find_gap(EVNINFO *e);

int pnpoly(int npol, double *xp, double *yp, double x, double y);

void set_subregion(EVNINFO *tevn);
void sample_obsph(EVNINFO *tevn);

