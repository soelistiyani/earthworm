/*
 * basic i/o & memory handling functions
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/

#include "basic.h"

int ErrorFlag;

void sherror(char *fmt, ...)
{
    va_list args;

	if(EOF == fflush(stdout))
	    fprintf(stderr, "\nerror: fflush failed on stdout");
	va_start(args,fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
    ErrorFlag = 1;
}

void shwarn(char *fmt, ...)
{
    va_list args;

	if(EOF == fflush(stdout))
	    fprintf(stderr, "\nerror: fflush failed on stdout");
	va_start(args,fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

int Is_inside(int x,int z,int left,int top,int right,int bottom)
{
    if( x < left || x > right || z < top  || z > bottom ) return 0;
    return 1;
}

int find_opt(int measure,int number)
{
    int i;

    if(measure < 2) return -1;
    for(i=measure;;i++)
    {
        if( measure*i == number ) return measure;
        if( measure*i > number ) break;
    }
    return find_opt(measure-1,number);
}

int find_opt_main(int number)
{
    int np1,np2;

    do
    {
	np1 = find_opt(sqrt(number),number);
	if( np1 > 0 )
	{
	    np2=number/np1;
	    if( np1*2 >= np2 ) break;
	}
	number--;
    }while(number>=4);

    if( number < 4 ) sherror("More Procedure needed!\n");
    return number;
}

FILE *echkfopen(char *fname,char *attr,void (*efnc)(char *fmt,...))
{
    FILE *fp;
    fp = fopen(fname,attr);
    if( (fp == NULL) && ((*efnc) != NULL) ) (*efnc)("%s File open failed\n",fname);
    return fp;
}

float *vector(int low,int high)
{
    float *array;

    if( ErrorFlag ) return NULL;
    if( high - low +1 < 1 )
    {
	sherror("Check Memory allocation range\n");
	return NULL;
    }
    array = (float *)malloc((high-low+1)*sizeof(float));
    if( array == NULL )
    {
	sherror("Memory allocation failure in vector\n");
	return NULL;
    }

    return (array-low);
}

void freevec(float **array,int low,int high)
{
    if( high - low +1 < 1 )
    {
	sherror("Check Memory allocation range\n");
	return;
    }
    free(*array+low);
    *array += low;
    *array = NULL;
}

double *dvector(int low,int high)
{
    double *array;

    if( ErrorFlag ) return NULL;
    if( high - low +1 < 1 )
    {
	sherror("Check Memory allocation range\n");
	return NULL;
    }
    array = (double *)malloc((high-low+1)*sizeof(double));
    if( array == NULL )
    {
	sherror("Memory allocation failure in vector\n");
	return NULL;
    }

    return (array-low);
}

void freedvec(double **array,int low,int high)
{
    if( high - low +1 < 1 )
    {
	sherror("Check Memory allocation range\n");
	return;
    }
    free(*array+low);
    *array += low;
    *array = NULL;
}

int SYM(int irow,int icol, int m)
{
    int t;
    if( irow > icol ) t=irow, irow=icol, icol=t;

    return irow*(2*m-irow+1)/2 + icol-irow;
}

double *symm_dmat(int m)
{
    double *sdmat;
    if( m < 1 ) sherror("Check Memory allocation range in dmat(%d)\n",m);
    sdmat = (double *)calloc( m*(m+1)/2, sizeof(double) );
    if( sdmat == NULL ) sherror("Memory allocation failure in symmetric dmatrix(%d)\n",m);
    return sdmat;
}

float *symm_fmat(int m)
{
    float *sfmat;
    if( m < 1 ) sherror("Check Memory allocation range in fmat(%d)\n",m);
    sfmat = (float *)calloc( m*(m+1)/2, sizeof(float) );
    if( sfmat == NULL ) sherror("Memory allocation failure in symmetric fmatrix(%d)\n",m);
    return sfmat;
}

float **bsymm_fmat(int n, int nband, int *bandlist)
{
    int i;
    float **bsymm;

    bsymm = (float **)malloc(nband*sizeof(float*));
    for(i=0;i<nband;i++)
    {
        bsymm[i] = (float *)calloc( n-abs(bandlist[i]), sizeof(float) );
    }
    return bsymm;
}

void free_bsymm(float ***bsymm, int nband)
{
    int i;
    for(i=nband-1;i>=0;i--)
	    free( (*bsymm)[i] );
    free( (*bsymm) );
    (*bsymm) = NULL;
}

double **dmatrix(int nrl, int nrh, int ncl, int nch)
{
    int i,nrow=nrh-nrl+1,ncol=nch-ncl+1;
    double **m;

    if( ErrorFlag ) return NULL;
    if( nrow < 1 || ncol < 1 )
    {
	sherror("Check Memory allocation range\n");
	return NULL;
    }

    m=(double **) malloc((size_t) ((nrow+1)*sizeof(double*)));
    if (!m)
    {
	sherror("allocation failure 1 in matrix\n");
	return NULL;
    }
    m += 1;
    m -= nrl;

    m[nrl]=(double *) malloc((size_t)((nrow*ncol+1)*sizeof(double)));
    if (!m[nrl]) 
    {
	sherror("allocation failure 2 in matrix\n");
	return NULL;
    }
    memset(m[nrl],(int)'\0',(size_t)((nrow*ncol+1)*sizeof(double)));

    m[nrl] += 1;
    m[nrl] -= ncl;
        
    for (i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

    return m;
}

void freedmat(double ***m, int nrl, int nrh, int ncl, int nch)
{
    free((char *) ((*m)[nrl]+ncl-1));
    free((char *) ((*m)+nrl-1));
    *m = NULL;
}

float **matrix(int nrl, int nrh, int ncl, int nch)
{
    int i,nrow=nrh-nrl+1,ncol=nch-ncl+1;
    float **m;

    if( ErrorFlag ) return NULL;
    if( nrow < 1 || ncol < 1 )
    {
	sherror("Check Memory allocation range\n");
	return NULL;
    }

    m=(float **) malloc((size_t) ((nrow+1)*sizeof(float*)));
    if (!m)
    {
	sherror("allocation failure 1 in matrix\n");
	return NULL;
    }
    m += 1;
    m -= nrl;

    m[nrl]=(float *) malloc((size_t)((nrow*ncol+1)*sizeof(float)));
    if (!m[nrl]) 
    {
	sherror("allocation failure 2 in matrix\n");
	return NULL;
    }
    memset(m[nrl],(int)'\0',(size_t)((nrow*ncol+1)*sizeof(float)));

    m[nrl] += 1;
    m[nrl] -= ncl;
        
    for (i=nrl+1;i<=nrh;i++) m[i]=m[i-1]+ncol;

    return m;
}

void freemat(float ***m, int nrl, int nrh, int ncl, int nch)
{
    free((char *) ((*m)[nrl]+ncl-1));
    free((char *) ((*m)+nrl-1));
    *m = NULL;
}

float ***cubic(int nrl, int nrh, int ncl, int nch, int ndl, int ndh)
{
    long i,j,nrow=nrh-nrl+1,ncol=nch-ncl+1,ndep=ndh-ndl+1;
    float ***t;

    if( ErrorFlag ) return NULL;
    if( nrow < 1 || ncol < 1 || ndep < 1)
    {
	sherror("Check Memory allocation range\n");
	return NULL;
    }

    t=(float ***) malloc((size_t) ((nrow+1)*sizeof(float**)));
    if (!t)
    { 
	sherror("allocation failure 1 in cubic\n");
	return NULL;
    }
    t += 1;
    t -= nrl;

    t[nrl]=(float **) malloc((size_t)((nrow*ncol+1)*sizeof(float*)));
    if (!t[nrl]) 
    { 
	sherror("allocation failure 2 in cubic\n");
	return NULL;
    }
    t[nrl] += 1;
    t[nrl] -= ncl;
        
    t[nrl][ncl]=(float *) malloc((size_t)((nrow*ncol*ndep+1)*sizeof(float)));
    if (!t[nrl][ncl]) 
    {
	sherror("allocation failure 3 in cubic\n");
	return NULL;
    }
    memset(t[nrl][ncl],(int)'\0',(size_t)((nrow*ncol*ndep+1)*sizeof(float)));
    t[nrl][ncl] += 1;
    t[nrl][ncl] -= ndl;
        
    for (j=ncl+1;j<=nch;j++) t[nrl][j]=t[nrl][j-1]+ndep;
    for (i=nrl+1;i<=nrh;i++){
    t[i]=t[i-1]+ncol;
    t[i][ncl]=t[i-1][ncl]+ncol*ndep;
    for (j=ncl+1;j<=nch;j++) t[i][j]=t[i][j-1]+ndep;
    }
        
    return t;
}

void reset_cubic(float ***cub,int nrl, int nrh, int ncl, int nch, int ndl, int ndh)
{
    long nrow=nrh-nrl+1,ncol=nch-ncl+1,ndep=ndh-ndl+1;
    if( nrow < 1 || ncol < 1 || ndep < 1)
    {
	sherror("Check Memory allocation range in rest_cubic\n");
	return;
    }
    memset(cub[nrl][ncl]+ndl-1,(int)'\0',(size_t)((nrow*ncol*ndep+1)*sizeof(float)));
}

void freecub(float ****t, int nrl, int nrh, int ncl, int nch, int ndl, int ndh)
{
    free((char *) ((*t)[nrl][ncl]+ndl-1));
    free((char *) ((*t)[nrl]+ncl-1));
    free((char *) ((*t)+nrl-1));
    *t = NULL;
}

int file_exists (char * fileName)
{
    struct stat buf;
    return stat ( fileName, &buf );
}

int file_exist_read (char * fileName,char *tbuf)
{
    struct stat buf;
    FILE *fp;
    int j,i = stat ( fileName, &buf );
    /* File found */
    if ( i == 0 )
    {
	fp = fopen(fileName,"r");
	j=0;
	while( fgets(tbuf,BUFSIZ,fp) ) j++;
	fclose(fp);
	return j;
    }
    return 0;
}

