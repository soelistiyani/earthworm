/*
 * initvar.c
 * Modified from initvar.c, Revision 1.1 in src/seismic_processing/pick_ew
 * 
 * This file is part of pick_FP, Earthworm module for the FilterPicker phase detector and picker.
 * 
 * (C) 2008-2012 Claudio Satriano <satriano@ipgp.fr>,
 * under the same license terms of the Earthworm software system. 
 */

#include <earthworm.h>
#include <transport.h>
#include "pick_gm.h"


   /*******************************************************************
    *                            InitVar()                            *
    *                                                                 *
    *       Initialize STATION variables to 0 for one station.        *
    *******************************************************************/

void InitVar( STATION *Sta )
{
	int i;
	//TODO: can we remove these two pointers?
	//PICK *Pick = &Sta->Pick;         /* Pointer to pick structure */
	//CODA *Coda = &Sta->Coda;         /* Pointer to coda structure */

	Sta->enddata    = 0L; /* Sample at end of previous message */
	Sta->endtime    = 0.; /* Time at end of previous message */
	Sta->first      = 1;  /* No messages with this channel have been detected */
	Sta->ns_restart = 0;  /* Restart sample count */

	//Sta->phase_number = 0;

        //if (Sta->mem != NULL) {
               //free_FilterPicker5_Memory(&(Sta->mem));
               //Sta->mem = NULL;
        //}
	/* Pick variables*/
	//Pick->time = 0.;         /* Pick time */

	//for ( i = 0; i < 3; i++ )
	//	Pick->xpk[i] = 0.;    /* Absolute value of extrema after ipic */

	//Pick->FirstMotion = '?'; /* First motion  ?=Not determined  U=Up  D=Down */
				 /* u=Questionably up  d=Questionably down */
	//Pick->weight = 0;        /* Pick weight (0-3) */
	//Pick->status = 0;

	/* Coda variables */
	// for ( i = 0; i < 6; i++ )
	// 	Coda->aav[i] = 0;     /* Average absolute value of preferred windows */

	// Coda->len_sec = 0;       /* Coda length in seconds */
	// Coda->len_out = 0;       /* Coda length in seconds (possibly * -1) */
	// Coda->len_win = 0;       /* Coda length in number of windows */
	// Coda->status  = 0;

	//Sta->inst = 0;
	//Sta->srate = 100;   
	//Sta->gain = 0.;
	
	//Sta->dt = 0.;	// 1/samprate

	Sta->ave = 0.;
	Sta->ave0 = 0.;
	Sta->ave1 = 0.;
	Sta->acc0 = 0.;    
	Sta->vel0 = 0.;

	Sta->acc0_vel = 0.;    
	Sta->vel0_dis = 0.;     

	Sta->vvsum = 0.;
	Sta->ddsum = 0.;

	Sta->a = 0.;
	Sta->v = 0.;
	Sta->d = 0.;
	Sta->ddv = 0.;

	Sta->pa = 0.;	// by Seo JeongBeom

	Sta->tc = 0.;             
	Sta->avd[0] = 0.0; 
	Sta->avd[1] = 0.0; 
	Sta->avd[2] = 0.0;  

	Sta->X1 = 0.0;  
	Sta->X2 = 0.0;  
	Sta->Y1 = 0.0;  
	Sta->Y2 = 0.0;  

	Sta->XX1 = 0.0;
	Sta->XX2 = 0.0;
	Sta->YY1 = 0.0;
	Sta->YY2 = 0.0;

	Sta->x1 = 0.0;
	Sta->x2 = 0.0;
	Sta->y1 = 0.0;
	Sta->y2 = 0.0;

	Sta->xx1 = 0.0;
	Sta->xx2 = 0.0;
	Sta->yy1 = 0.0;
	Sta->yy2 = 0.0;

	/* New variable for pickpga */
	Sta->pkt_1s_time = 0;
	Sta->tm = 0.;
	Sta->pga = 0.;
	Sta->pgd = 0.;
	Sta->pgv = 0.;
	Sta->cav = 0.;
	Sta->bcav_mmi = 0.;	// by Seo JeongBeom
	Sta->lbspga_mmi = 0.;	// by Seo JeongBeom
	Sta->maxpga = 0.;
	Sta->lbspga = 0.;
	Sta->bcav = 0.;	
	Sta->t_pga = 0.;
	Sta->t_pgd = 0.;
	Sta->t_pgv = 0.;
	Sta->t_cav = 0.;

	for ( i = 0; i < ACCUMLEN; i++ ){
		Sta->arpga[i] = 0.;
		Sta->arcav[i] = 0.;		
	}

//	Sta->is_cav = false;
	Sta->is_oversec = false;

	for ( i = 0; i < GMXLEN; i++ )
	{
		Sta->gmx[i].tm = -1;
		Sta->gmx[i].pga = 0;
		Sta->gmx[i].viewCnt = 0;
		Sta->gmx[i].cav = 0;		
	}

	Sta->gmBuffer.firstArrTm = -1;
	for ( i = 0; i < BUFFLEN; i++) {
		Sta->gmBuffer.buffer[i] = 0;	

	}

	Sta->first_tm = -1;
}
