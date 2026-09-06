#define _USE_MATH_DEFINES

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ctype.h>

#include <math.h>
#include "pick_FP.h"


/* Get coeff by cutoff
	***********/
void get_coeff(int srate, double cutoff, double *B0, double *B1, double *B2, double *A1, double *A2)
{
	
	if ( cutoff == 0.075 ) // CWB
	{
		// Initializing 0.075Hz two poles Butterworth high pass filter
		if(srate == 100)
		{
			*B0 = 0.9966734; *B1 = -1.993347; *B2 = 0.9966734;
			*A1 = -1.993336; *A2 = 0.9933579;
			return;
		}
		else if(srate == 50)
		{
			*B0 = 0.993357897; *B1 = -1.98671579; *B2 = 0.993357897; 
			*A1 = -1.98667157; *A2 = 0.986759841;
			return;
		}	
		else if(srate == 40)
		{
			*B0 = 0.9917042; *B1 = -1.983408; *B2 = 0.9917042;
			*A1 = -1.983340; *A2 = 0.9834772;
			return;
		}	
		else if(srate == 20)
		{
			*B0 = 0.983477175; *B1 = -1.96695435; *B2 = 0.983477175; 
			*A1 = -1.96668136; *A2 = 0.967227399;
			return;
		}
	}
	else if ( cutoff == 0.3 )
	{
		// Initializing 0.3Hz two poles Butterworth high pass filter
		if(srate == 100)
		{
			*B0 = 0.98675978; *B1 = -1.973519561; *B2 = 0.98675978;
			*A1 = -1.97334425; *A2 = 0.973694872;
			return;
		}
	}
	else 
	{
		// Directly calculate
		double ff = cutoff / (double)srate;
		double ita = 1.0 / tan(M_PI*ff);
		double q = sqrt(2.0);
		*B0 = 1.0 / (1.0 + q * ita + ita * ita);
		*B1 = 2 * *B0;
		*B2 = *B0;
		*A1 = -2.0 * ( ita * ita - 1.0 ) * *B0;
		*A2 = (1.0 - q * ita + ita * ita ) * *B0;
		*B0 = *B0 * ita * ita;
		*B1 = -*B1 * ita * ita;
		*B2 = *B2 * ita * ita;
	}
}

int sniff_eew( int LongSample, PEEW *eew_Sta, GPARM *Gparm )
{

	//int             i;

	double    B0;
	double    B1;
	double    B2;
	double    A1;
	double    A2;

	//-- For TcPd analysis 
	double    OUTPUT,acc,acc_vel,ddv;

	//double delay;
	get_coeff(eew_Sta->srate, Gparm->CutOffFreq, &B0, &B1, &B2, &A1, &A2);

	eew_Sta->avd[0]=eew_Sta->a;
	eew_Sta->avd[1]=eew_Sta->v;
	eew_Sta->avd[2]=eew_Sta->d; 

	eew_Sta->v=LongSample;   // fetch trace data point by point	                                                                                                                                                                                                                         
	eew_Sta->v=eew_Sta->v/eew_Sta->gain;       /*  for gain factor   */
	eew_Sta->ave = eew_Sta->ave*999.0/1000.0+eew_Sta->v/1000.0;	// remove instrument response
	eew_Sta->v=eew_Sta->v-(eew_Sta->ave);				// remove offset                             

	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */  
	OUTPUT = B0*eew_Sta->v + B1*eew_Sta->X1 + B2*eew_Sta->X2;
	OUTPUT = OUTPUT - ( A1*eew_Sta->Y1 + A2*eew_Sta->Y2 );
	eew_Sta->Y2 = eew_Sta->Y1;
	eew_Sta->Y1 = OUTPUT;
	eew_Sta->X2 = eew_Sta->X1;
	eew_Sta->X1 = eew_Sta->v;
	eew_Sta->v = OUTPUT;
	/*--------------End of Recursive Filter*/                                        

	// jman, 20170802
	//if(eew_Sta->inst==1)
	//{
	//	eew_Sta->a = eew_Sta->v;
	//}

	//-- for Acc to Vel
	if(eew_Sta->inst==1)
	{
		acc=eew_Sta->v;
		eew_Sta->v=(acc+eew_Sta->acc0)*eew_Sta->dt/2.0+eew_Sta->vel0;
		eew_Sta->acc0=acc;
		eew_Sta->vel0=eew_Sta->v;
		eew_Sta->ave0 = eew_Sta->ave0*(10.-eew_Sta->dt)/10.0+eew_Sta->v*eew_Sta->dt/10.0;
		eew_Sta->v=eew_Sta->v-eew_Sta->ave0;     

		/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */  
		OUTPUT = B0*eew_Sta->v + B1*eew_Sta->XX1 + B2*eew_Sta->XX2;
		OUTPUT = OUTPUT - ( A1*eew_Sta->YY1 + A2*eew_Sta->YY2 );
		eew_Sta->YY2 = eew_Sta->YY1;
		eew_Sta->YY1 = OUTPUT;
		eew_Sta->XX2 = eew_Sta->XX1;
		eew_Sta->XX1 = eew_Sta->v;
		eew_Sta->v = OUTPUT;
		/*--------------End of Recursive Filter*/                                                                                               
	}                           
	//--------------------------------------------------------------------------------------------                                                                                                                                 

	// jman, 20170802                            
	//if(eew_Sta->inst==2) {
		eew_Sta->a=(eew_Sta->v - eew_Sta->avd[1])/eew_Sta->dt;
	//}

	acc_vel = eew_Sta->v;
	eew_Sta->d = (acc_vel+eew_Sta->acc0_vel)*eew_Sta->dt/2.0+eew_Sta->vel0_dis;
	eew_Sta->acc0_vel = acc_vel;
	eew_Sta->vel0_dis = eew_Sta->d;
	eew_Sta->ave1 = eew_Sta->ave1*(10.-eew_Sta->dt)/10.0+eew_Sta->d*eew_Sta->dt/10.0;
	eew_Sta->d=eew_Sta->d-eew_Sta->ave1;                           

	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */  
	OUTPUT = B0*eew_Sta->d + B1*eew_Sta->x1 + B2*eew_Sta->x2;
	OUTPUT = OUTPUT - ( A1*eew_Sta->y1 + A2*eew_Sta->y2 );
	eew_Sta->y2 = eew_Sta->y1;
	eew_Sta->y1 = OUTPUT;
	eew_Sta->x2 = eew_Sta->x1;
	eew_Sta->x1 = eew_Sta->d;
	eew_Sta->d = OUTPUT;
	/*--------------End of Recursive Filter*/    
	ddv = (eew_Sta->d- eew_Sta->avd[2] )/eew_Sta->dt;

	eew_Sta->ddsum += eew_Sta->d*eew_Sta->d;
	eew_Sta->vvsum += ddv * ddv;      

	eew_Sta->tc=2.*3.141592654/sqrt((eew_Sta->vvsum)/(eew_Sta->ddsum));                                                                                                                        
	//--------------------------------------------------------------------------------------------                           

	return 1;
}
