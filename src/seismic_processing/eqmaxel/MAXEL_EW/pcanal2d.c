/*
 * PCA for computing error ellipse
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "basic.h"
void eigsrtABS(float d[], float **v, int n);

extern int verbose;

void pcanal2d(float **cov,int ncol,float *egval,float **egvect)
{
    int i,j,nrot;

    jacobi(cov,ncol,egval,egvect,&nrot);
    if( verbose == 1)
    {
      printf("\n## Eigenvalues and eigenvectors --------\n");
      for (i=1;i<=ncol;i++) 
      {
        printf("eigenvalue %3d = %12.6f\n",i,egval[i]);
        printf("eigenvector:\n");
        for (j=1;j<=ncol;j++) 
        {
            printf("%12.6f",egvect[j][i]);
            if ((j % 5) == 0) printf("\n");
        }
        printf("\n");
      }
    }

    eigsrtABS(egval,egvect,ncol);
    if( verbose == 1 )
    {
      printf("\n## Sorted values -----------------------\n");
      for (i=1;i<=ncol;i++) 
      {
        printf("eigenvalue %3d = %12.6f\n",i,egval[i]);
        printf("eigenvector:\n");
        for (j=1;j<=ncol;j++) 
        {
            printf("%12.6f",egvect[j][i]);
            if ((j % 5) == 0) printf("\n");
        }
        printf("\n");
      }
    }
}

