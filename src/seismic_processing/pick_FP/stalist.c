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
#include <kom.h>
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
   int     lineno; /* line number */
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
      fclose( fp );

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
   /* modification to use the kom library to allow variables in the FP list  */
      if ( k_open(Gparm->StaFile[ifile].name)  == 0 )
      {
         logit( "e", "pick_FP: Error opening station file <%s>\n",
                  Gparm->StaFile[ifile].name );
         return -1;
      }

      i = 0;
      lineno = 0;
      while ( k_rd() )
      {
         int pickflag;
         int pin;
         char *com;
         char *tmp;

         com = k_str();          /* Get the first token from line */
         lineno++;
         if ( !com ) continue;             /* Ignore blank lines */
         if ( com[0] == '#' ) continue;    /* Ignore comments */
         pickflag = atoi(com);	/* first token is the pick flag which is an int */
         pin = k_int();
         com = k_str();          /* station */
         tmp = strdup(com);
	 strcpy(sta[i].sta, tmp);
         com = k_str();          /* chan */
         tmp = strdup(com);
	 strcpy(sta[i].chan, tmp);
         com = k_str();          /* net */
         tmp = strdup(com);
	 strcpy(sta[i].net, tmp);
         com = k_str();          /* loc */
         tmp = strdup(com);
	 strcpy(sta[i].loc, tmp);
         sta[i].Parm.filterWindow = k_val();
         sta[i].Parm.longTermWindow = k_val();
         sta[i].Parm.threshold1 = k_val();
         sta[i].Parm.threshold2 = k_val();
         sta[i].Parm.tUpEvent = k_val();
#if DEBUG_KOM
         logit( "e", "pick_FP: line %d Debug %s.%s.%s.%s\n", lineno, sta[i].sta, sta[i].chan, sta[i].net, sta[i].loc);
         logit( "e", "pick_FP: line %d %f %f %f %f %f\n", sta[i].Parm.filterWindow, sta[i].Parm.longTermWindow,
			sta[i].Parm.threshold1, sta[i].Parm.threshold2, sta[i].Parm.tUpEvent);
#endif
         if (k_err()) 
         {
             logit( "e", "pick_FP: error on line %d of %s, wrong number of tokens\n line has: %s", lineno, Gparm->StaFile[ifile].name, k_get());
             return(-1);
         }
         if ( pickflag == 0 ) continue;
         i++;
      }
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
