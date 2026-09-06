/*
 * read travel time table
 * from 0 km 
 * assume that the table is generated at every 1 km epicentral distance and
 * the event depth is 8 km
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "basic.h"

extern int ntredst;
extern float *trtable;

int roundoff (double d); // from qtime.c

void read_trtable(char *trfile)
{
    int i;
    FILE *fp;
    char *tbuf;
    double dst,trtime;
        
    fp = fopen(trfile,"r");
    if( fp == NULL ) sherror("%s travel time table open error\n",trfile);

    ntredst = 0;
        
    tbuf = (char *)malloc( BUFSIZ*sizeof(char) );

    while(fgets(tbuf,BUFSIZ,fp)) ntredst++;
    if( ntredst == 0 ) sherror("There isn't layer information!\n");
                
    trtable = (float *)malloc(ntredst*sizeof(float));
    fseek(fp,0L,SEEK_SET);
    for(i=0;i<ntredst;i++)
    {
        fgets(tbuf,BUFSIZ,fp);
        sscanf(tbuf,"%lf %lf", &dst,&trtime);
        trtable[i] = trtime;
    }
    fclose(fp);
    free(tbuf);
}

double itrtime(double r)
{
    int jdst;
    double trt;

    jdst = floor(r);

    if(jdst<ntredst-1)
        trt = (1.-r+jdst)*trtable[jdst] 
          + (r-jdst)*trtable[(jdst+1)];
    else
    {
        trt = trtable[ntredst-1] 
            + (trtable[ntredst-1]-trtable[ntredst-2])*(r-ntredst);
    }

   return trt;
}
