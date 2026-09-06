/*
 * stalist.c
 * Modified from stalist.c, Revision 1.6 in src/seismic_processing/pick_ew
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <earthworm.h>
#include <transport.h>
#include "pick_gm.h"

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

int GetStaList( STATION **Sta, int *Nsta, GPARM *Gparm, bool isAllComp )
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
			 logit( "et", "pick_gm: Error opening station list file <%s>.\n",
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
		if (!isAllComp) nstanew /= 3;
		sta = (STATION *) realloc( *Sta, (*Nsta+nstanew)*sizeof(STATION) );
		if ( sta == NULL )
		{
			 logit( "et", "pick_gm: Cannot reallocate the station array\n" );
			 return -1;
		}
		*Sta = sta;           /* point to newly realloc'd space */
		sta  = *Sta + *Nsta;  /* point to next empty slot */

	 /* Initialize internal variables in station list
		*********************************************/
		for ( i = 0; i < nstanew; i++ ) {
			 //sta[i].mem = NULL;
			 InitVar( &sta[i] );
		}

	 /* Read stations from the station list file into the station
		array, including parameters used by the picking algorithm
		*********************************************************/
		i = 0;
		while ( fgets( string, MAX_LEN_STRING_STALIST, fp ) != NULL )
		{
			int ndecoded;

			if ( IsComment( string ) ) continue;
			ndecoded = sscanf(string,"%s %s %s %s %lf %lf %d %lf %d"
				, sta[i].sta,  sta[i].chan,  sta[i].net,    sta[i].loc
				, &sta[i].lat, &sta[i].lon, &sta[i].srate, &sta[i].gain, &sta[i].inst);

			if ( !isAllComp )
			{
				if ( (sta[i].chan)[2] != 'Z') continue;
			}
			

			sta[i].dt=1./(1.*sta[i].srate);    
			//printf("string: %s \n", string);
			//printf("---2 , ndecoded: %d\n",ndecoded);                
			if ( ndecoded < 9 )
			{
				printf("pick_gm: Error decoding station file.\n");
				printf("pick_gm: ndecoded: %d\n", ndecoded);
				printf("pick_gm: offending line: [%s]\n", string);
				// logit( "et", "pick_ew: Error decoding station file.\n" );
				// logit( "e", "ndecoded: %d\n", ndecoded );
				// logit( "e", "Offending line:\n" );
				// logit( "e", "%s\n", string );
				return -1;
			}

			/*
			if (isAllComp) 
			{
				i++;
				strcpy(sta[i].sta, sta[i-1].sta);
				strcpy(sta[i].chan, sta[i-1].chan);
				(sta[i].chan)[2] = 'E';
				strcpy(sta[i].net, sta[i-1].net);
				strcpy(sta[i].loc, sta[i-1].loc);
				sta[i].lat = sta[i-1].lat;
				sta[i].lon = sta[i-1].lon;
				sta[i].srate = sta[i-1].srate;
				sta[i].gain = sta[i-1].gain;
				sta[i].inst = sta[i-1].inst;
				sta[i].dt = sta[i-1].dt;

				i++;
				strcpy(sta[i].sta, sta[i-1].sta);
				strcpy(sta[i].chan, sta[i-1].chan);
				(sta[i].chan)[2] = 'N';
				strcpy(sta[i].net, sta[i-1].net);
				strcpy(sta[i].loc, sta[i-1].loc);
				sta[i].lat = sta[i-1].lat;
				sta[i].lon = sta[i-1].lon;
				sta[i].srate = sta[i-1].srate;
				sta[i].gain = sta[i-1].gain;
				sta[i].inst = sta[i-1].inst;
				sta[i].dt = sta[i-1].dt;

			}
			*/
			i++;
		} // end of while
		fclose( fp );
		logit( "", "pick_gm: Loaded %d channels from station list file:  %s\n",
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
		// logit( "", "  %5.3lf",    Sta[i].Parm.filterWindow );
		// logit( "", "  %5.3lf",    Sta[i].Parm.longTermWindow );
		// logit( "", "  %5.3lf",    Sta[i].Parm.threshold1 );
		// logit( "", "  %5.3lf",    Sta[i].Parm.threshold2 );
		// logit( "", "  %5.3lf",    Sta[i].Parm.tUpEvent );
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

	// jman, 20170629 Check empty string
	int len;
	len = (int)strlen(string);
	if (len==0) return 1;

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
