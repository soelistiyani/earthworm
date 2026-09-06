/*
 * read observation data
 * by Dong-Hoon Sheen
 *    Chonnam National University
*/
#include "mle.h"

extern int ctype,nobs,nstnDB,verbose,minphid;
extern STNINFO *stnlist;
extern OBSINFO *obslist;

int duplicate_stn(char *new_stn,int uid);

void read_obslist(char *iname)
{
   int i,j,iobs,iwgt;
   int uid;
   FILE *fp;
   char *tbuf,*artime;
   double minar,x,y,baz,bwgt;

   fp = fopen(iname,"r");
   if( fp == NULL ) sherror("%s observed time open error\n",iname);

   tbuf = (char *)malloc( BUFSIZ*sizeof(char) );
   artime = (char *)malloc( BUFSIZ*sizeof(char) );
      
   iobs = 0;
   while(fgets(tbuf,BUFSIZ,fp)) iobs++;
   if( iobs == 0 ) sherror("No observed travel time!\n");
//   else if( iobs > nobs ) 
//   {
//	shwarn("Check nobs(%d) in control file, obs: %d\n",nobs,iobs);
//   }
   else if( iobs < nobs ) 
   {
	nobs = iobs;
	shwarn("only %d obs exist\n",iobs);
   }

   obslist = (OBSINFO *)malloc(nobs*sizeof(OBSINFO));

	    
   fseek(fp,0L,SEEK_SET);
   for(i=0,uid=0,iobs=0,minar=0.,minphid=0;i<nobs;i++)
   {
	fgets(tbuf,BUFSIZ,fp);
	sscanf(tbuf,"%s %s %s %d",
		obslist[uid].stn, obslist[uid].ntw, artime, &iwgt);
	if( duplicate_stn(obslist[uid].stn,uid) == 1 ) 
	{
	     if( verbose ) 
		fprintf(stdout,"Duplicate station: %s\n",obslist[uid].stn);
		continue;
	}

	j = find_stn(obslist[uid].stn, obslist[uid].ntw);
	if( j < 0 )
	{
	     if( verbose ) 
		fprintf(stdout,"STN ERROR: %s  %lf\n",obslist[uid].stn, obslist[uid].ti);
	     obslist[uid].x = obslist[uid].y = obslist[uid].z = obslist[uid].wgt = 0.;
	}
	else
	{
	    iobs ++;
        /*
	    et = time_convert(artime);
	    it = ext_to_int( et );

	    if( iobs == 1 )
	    { 
		obslist[uid].ti = 0.;
		it0 = it;
		minIT = it;
	    }
	    else
	    {
		obslist[uid].ti = tdiff(it,it0)/USECS_PER_SEC;
		if( minar > obslist[uid].ti )
		{
		   minar = obslist[uid].ti;
           minphid = uid;
		   minIT = it;
		}
	    }
        */
        sscanf(artime,"%lf",&(obslist[uid].ti));
        if( iobs == 1 ) minar = obslist[uid].ti;
        else 
        {
            if( minar > obslist[uid].ti ) minar = obslist[uid].ti;
        }

	     obslist[uid].lon  = stnlist[j].lon;
	     obslist[uid].lat  = stnlist[j].lat;
	     obslist[uid].elev = stnlist[j].elev;
	     if( ctype == DLTLN )
		calc_deg2km(stnlist[j].lon, stnlist[j].lat, &x, &y);
	     else
 	     {
		x = stnlist[j].lon;
		y = stnlist[j].lat;
 	     }

	     obslist[uid].x = x;
	     obslist[uid].y = y;
	     obslist[uid].z = stnlist[j].elev;

	     obslist[uid].evid = -1;

//	     obslist[uid].et  = et;

			obslist[uid].wgt = 1.;
            /*
	     switch( iwgt )
	     {
		case 0:
			obslist[uid].wgt = 1.;
			break;
		case 1:
			obslist[uid].wgt = 0.75;
			break;
		case 2:
			obslist[uid].wgt = 0.5;
			break;
		case 3:
			obslist[uid].wgt = 0.25;
			break;
		default:
			obslist[uid].wgt = 0.;
			break;
	     }
         */
	}
	uid++;
   }
   fclose(fp);

   nobs = uid;

   if( verbose ) fprintf(stdout,"OBSERVED PHASE LIST\n    STN   NTW YEAR/MM/DD HH:MN:SEC    ARRTIME WGT\n");

   for(i=0;i<nobs;i++)
   {
	if( obslist[i].wgt > 0 )
	{
	    obslist[i].ti -= minar;
        /*
	    if(verbose) 
	    {
                fprintf(stdout,"%-3d %-6s %2s %04d/%02d/%02d %02d:%02d:%05.2f %7.4f %4.2f\n",
                        i,obslist[i].stn, obslist[i].ntw,
                        obslist[i].et.year, obslist[i].et.month, obslist[i].et.day, obslist[i].et.hour,
                        obslist[i].et.minute, obslist[i].et.second+obslist[i].et.usec/USECS_PER_TICK/10000.,
			obslist[i].ti,obslist[i].wgt);
	    }
        */
	}
   }

   free(tbuf);
}

/*
EXT_TIME time_convert(char *artime)
{
   int usec;
   double dsec;
   char stmp[5];
   EXT_TIME et;

   strncpy(stmp,artime,4*sizeof(char));
   stmp[4]='\0';
   sscanf(stmp,"%d",&(et.year));
   stmp[2]='\0';
   stmp[3]='\0';

   strncpy(stmp,artime+4,2*sizeof(char));
   sscanf(stmp,"%d",&(et.month));

   strncpy(stmp,artime+6,2*sizeof(char));
   sscanf(stmp,"%d",&(et.day));

   strncpy(stmp,artime+8,2*sizeof(char));
   sscanf(stmp,"%d",&(et.hour));

   strncpy(stmp,artime+10,2*sizeof(char));
   sscanf(stmp,"%d",&(et.minute));

   strncpy(stmp,artime+12,2*sizeof(char));
   sscanf(stmp,"%d",&(et.second));

   strncpy(stmp,artime+14,3*sizeof(char));
   sscanf(stmp,"%lf",&dsec);

   et.usec = roundoff(USECS_PER_SEC*dsec);

   et.doy = mdy_to_doy(et.month, et.day, et.year);

   return et;
}

EXT_TIME add_time_to_org(EXT_TIME first, double orgt)
{
   INT_TIME it,it0;
   int sec, usecs;
	    
   it = ext_to_int( first );
   sec   = floor(orgt);
   usecs = (orgt-sec)*USECS_PER_SEC;

   it0 = add_time(it, sec, usecs);

   return int_to_ext( it0 );
}
*/


int duplicate_stn(char *new_stn,int uid)
{
	int i;
	for(i=0;i<uid;i++)
	{
		if( strcmp(new_stn,obslist[i].stn) == 0 ) return 1;
	}

	return 0;
}
