/*
 * stalist.c
 * Modified from stalist.c, Revision 1.6 in src/seismic_processing/pick_ew
 *
 * This file is part of pick_FP, Earthworm module for the FilterPicker phase detector and picker.
 *
 * (C) 2008-2012 Claudio Satriano <satriano@ipgp.fr>,
 * under the same license terms of the Earthworm software system. 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <earthworm.h>
#include <transport.h>
#include "pick_FP.h"

#define MAX_LEN_STRING_STALIST 512

/* Function prototype
   ******************/
void InitVar( STATION * );
int  IsComment( char [] );


  /***************************************************************
   *                         GetStaList()                        *
   *                                                             *
   *                     Read the station list                   *
   *                                                             *
   *  Returns -1 if an error is encountered.                     *
   ***************************************************************/

int GetStaList( STATION **Sta, int *Nsta, GPARM *Gparm )
{
   char    string[MAX_LEN_STRING_STALIST];
   int     i,ifile;
   int     nstanew;
   STATION *sta;
   FILE    *fp;

/* Loop thru the station list file(s)
   **********************************/
   for( ifile=0; ifile<Gparm->nStaFile; ifile++ )
   {
      if ( ( fp = fopen( Gparm->StaFile[ifile].name, "r") ) == NULL )
      {
         logit( "et", "pick_FP: Error opening station list file <%s>.\n",
                Gparm->StaFile[ifile].name );
         return -1;
      }

   /* Count channels in the station file.
      Ignore comment lines and lines consisting of all whitespace.
      ***********************************************************/
      nstanew = 0;
      while ( fgets( string, MAX_LEN_STRING_STALIST, fp ) != NULL )
         if ( !IsComment( string ) ) nstanew++;

      rewind( fp );

   /* Re-allocate the station list
      ****************************/
      sta = (STATION *) realloc( *Sta, (*Nsta+nstanew)*sizeof(STATION) );
      if ( sta == NULL )
      {
         logit( "et", "pick_FP: Cannot reallocate the station array\n" );
         return -1;
      }
      *Sta = sta;           /* point to newly realloc'd space */
      sta  = *Sta + *Nsta;  /* point to next empty slot */

   /* Initialize internal variables in station list
      *********************************************/
      for ( i = 0; i < nstanew; i++ ) {
         sta[i].mem = NULL;
         InitVar( &sta[i] );
      }

   /* Read stations from the station list file into the station
      array, including parameters used by the picking algorithm
      *********************************************************/
      i = 0;
      while ( fgets( string, MAX_LEN_STRING_STALIST, fp ) != NULL )
      {
         int ndecoded;
         int pickflag;
         int pin;

         if ( IsComment( string ) ) continue;
         ndecoded = sscanf( string, "%d %d %s %s %s %s %lf %lf %lf %lf %lf", 
                 &pickflag,
                 &pin,
                  sta[i].sta,
                  sta[i].chan,
                  sta[i].net,
                  sta[i].loc,
                 &sta[i].Parm.filterWindow,
                 &sta[i].Parm.longTermWindow,
                 &sta[i].Parm.threshold1,
                 &sta[i].Parm.threshold2,
                 &sta[i].Parm.tUpEvent
                 );
         if ( ndecoded < 11 )
         {
            logit( "et", "pick_FP: Error decoding station file.\n" );
            logit( "e", "ndecoded: %d\n", ndecoded );
            logit( "e", "Offending line:\n" );
            logit( "e", "%s\n", string );
            return -1;
         }
         if ( pickflag == 0 ) continue;
         i++;
      }
      fclose( fp );
      logit( "", "pick_FP: Loaded %d channels from station list file:  %s\n",
             i, Gparm->StaFile[ifile].name);
      Gparm->StaFile[ifile].nsta = i;
      *Nsta += i;
   } /* end for over all StaFiles */
   return 0;
}


 /***********************************************************************
  *                             LogStaList()                            *
  *                                                                     *
  *                         Log the station list                        *
  ***********************************************************************/

void LogStaList( STATION *Sta, int Nsta )
{
   int i;

   logit( "", "\nPicking %d channel", Nsta );
   if ( Nsta != 1 ) logit( "", "s" );
   logit( "", " total:\n" );

   for ( i = 0; i < Nsta; i++ )
   {
      logit( "", "%-5s",     Sta[i].sta );
      logit( "", " %-3s",    Sta[i].chan );
      logit( "", " %-2s",    Sta[i].net );
      logit( "", " %-2s",    Sta[i].loc );
      logit( "", "  %5.3lf",    Sta[i].Parm.filterWindow );
      logit( "", "  %5.3lf",    Sta[i].Parm.longTermWindow );
      logit( "", "  %5.3lf",    Sta[i].Parm.threshold1 );
      logit( "", "  %5.3lf",    Sta[i].Parm.threshold2 );
      logit( "", "  %5.3lf",    Sta[i].Parm.tUpEvent );
      logit( "", "\n" );
   }
   logit( "", "\n" );
}


    /*********************************************************************
     *                             IsComment()                           *
     *                                                                   *
     *  Accepts: String containing one line from a pick_FP station list  *
     *  Returns: 1 if it's a comment line                                *
     *           0 if it's not a comment line                            *
     *********************************************************************/

int IsComment( char string[] )
{
   int i;

   for ( i = 0; i < (int)strlen( string ); i++ )
   {
      char test = string[i];

      if ( test!=' ' && test!='\t' && test!='\n' )
      {
         if ( test == '#'  )
            return 1;          /* It's a comment line */
         else
            return 0;          /* It's not a comment line */
      }
   }
   return 1;                   /* It contains only whitespace */
}

void ini_zero_eew(PEEW *eew, int nsta)
{
	int i, j;

	for(i=0;i<nsta;i++)
	{        
		eew[i].X1 =0;
		eew[i].X2 =0;
		eew[i].Y1 =0;
		eew[i].Y2 =0;      
		eew[i].XX1 =0;
		eew[i].XX2 =0;
		eew[i].YY1 =0;
		eew[i].YY2 =0;            
		eew[i].x1 =0;
		eew[i].x2 =0;
		eew[i].y1 =0;
		eew[i].y2 =0;      
		eew[i].ave  =0;
		eew[i].ave0 =0;
		eew[i].ave1 =0;
		eew[i].acc0 =0;    
		eew[i].vel0 =0;      
		eew[i].acc0_vel =0;    
		eew[i].vel0_dis =0;            
		eew[i].vvsum =0;
		eew[i].ddsum =0;      
		eew[i].a =0;
		eew[i].v =0;
		eew[i].d =0;
		eew[i].ddv =0;      
		eew[i].pa =0;
		eew[i].pv =0;
		eew[i].pd =0;      
		eew[i].tc =0;             
		eew[i].ptime  = 0;   	   
		eew[i].avd[0] = 0.0; 
		eew[i].avd[1] = 0.0; 
		eew[i].avd[2] = 0.0;            
		eew[i].flag  = 0;         
		eew[i].eew_flag = 0;             
		eew[i].buf  = 0;   
		
		eew[i].rold	= 0.0;
		eew[i].rdat	= 0.0;
		eew[i].old_sample = 0;
		eew[i].esta	= 0.0;
		eew[i].elta	= 0.0;  
		eew[i].eref	= 0.0; 
		eew[i].eabs	= 0.0;            
		
		eew[i].count_cpa = 0;
		eew[i].pa_01sec_count  = 0;		 
		eew[i].pa_02sec_count  = 0;	 
		eew[i].pa_05sec_count  = 0;
		eew[i].pa_1sec_count   = 0;
		eew[i].pa_15sec_count  = 0; 

		for ( j=0; j<MAX_PICKDATA; j++)
		{
			eew[i].pickList[j].lddate = -1;
		}
		eew[i].last_report_P = 0;
	}   	
}

int GetEEWList( PEEW *eew, int *Neew, char *EEWFile )
{
	FILE *fp;
	char ss[100];
	int i;

	fp = fopen(EEWFile,"r");
	if(fp==NULL)
	{
		printf("File %s open error ! \n", EEWFile);
		return -1;
	}

	i=0;
	while(fgets(ss,99,fp))
	{
		if (IsComment(ss))  continue;
		//./Run_sniffwave_w WAVE_RING_102a  ENT Ch1 CWB_SMT 01 24.6373 121.5737 100 3209.963727 1 &
		//start Run_sniffwave_w WAVE_RING TWB1 HHZ TW 01 25.0068 121.9972 100 1249297.270285 2 &		
		sscanf(ss,"%s %s %s %s %lf %lf %d %lf %d"
			,  eew[i].sta,  eew[i].chan,  eew[i].net,    eew[i].loc
			, &eew[i].lat, &eew[i].lon, &eew[i].srate, &eew[i].gain, &eew[i].inst);
		/*        
		printf("%s %s %s %s %lf %f %d %f \n"
		,  eew[i].sta,  eew[i].chn,  eew[i].net,    eew[i].loc
		,  eew[i].lat,  eew[i].lon,  eew[i].srate,  eew[i].gain);	        
		*/
		
		eew[i].dt=1./(1.*eew[i].srate);
		i++;
	}	

	*Neew = i;
	//printf("------------ %d \n", i);

	fclose(fp);

	return 1;
}