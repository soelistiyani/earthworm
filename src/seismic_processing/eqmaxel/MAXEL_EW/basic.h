/*
 * basic i/o & memory handling functions
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#ifndef BASICFUNC
#define BASICFUNC

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

void sherror(char *fmt, ...);
void shwarn(char *fmt, ...);
int Is_inside(int x,int z,int left,int top,int right,int bottom);
FILE *echkfopen(char *fname,char *attr,void (*efnc)(char *fmt,...));
float *vector(int low,int high);
void freevec(float **array,int low,int high);
float **matrix(int lr,int hr,int lc,int hc);
void freemat(float ***mat,int lr,int hr,int lc,int hc);
float ***cubic(int l1,int h1,int l2,int h2,int l3,int h3);
void freecub(float ****cub,int l1,int h1,int l2,int h2,int l3,int h3);

void reset_cubic(float ***cub,int nrl, int nrh, int ncl, int nch, int ndl, int ndh);

double *dvector(int low,int high);
void freedvec(double **array,int low,int high);
double **dmatrix(int lr,int hr,int lc,int hc);
void freedmat(double ***mat,int lr,int hr,int lc,int hc);

int file_exists (char * fileName);
int file_exist_read (char * fileName,char *tbuf);

#endif

