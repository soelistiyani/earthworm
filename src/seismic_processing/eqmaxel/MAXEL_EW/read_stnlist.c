/*
 * read station data
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"

extern int nstnDB,ctype;
extern STNINFO *stnlist;

int find_stn(char *tstn, char *ntw)
{
   int i;
      
   for(i=0;i<nstnDB;i++)
   {
//	if( strcmp(stnlist[i].stn,tstn)==0 && strcmp(stnlist[i].ntw,ntw)==0 )
//		return i;
	if( strcmp(stnlist[i].stn,tstn) == 0 ) return i;
   }
   return -1;
}


void read_stnlist(char *iname)
{
   int i;
   FILE *fp;
   char *tbuf;


   fp = fopen(iname,"r");
   if( fp == NULL ) sherror("%s station list open error\n",iname);
   nstnDB = 0;

   tbuf = (char *)malloc( BUFSIZ*sizeof(char) );
      
   while(fgets(tbuf,BUFSIZ,fp)) nstnDB++;
   if( nstnDB == 0 ) sherror("There isn't station information!\n");

   stnlist = (STNINFO *)malloc(nstnDB*sizeof(STNINFO));

   fseek(fp,0L,SEEK_SET);
   for(i=0;i<nstnDB;i++)
   {
	fgets(tbuf,BUFSIZ,fp);

	if( ctype == KMXY )
	sscanf(tbuf,"%s %lf %lf",
		stnlist[i].stn, &(stnlist[i].lon), &(stnlist[i].lat));
	else
	sscanf(tbuf,"%s %lf %lf",
		stnlist[i].stn, &(stnlist[i].lat), &(stnlist[i].lon));
   }
   fclose(fp);

   free(tbuf);
}

