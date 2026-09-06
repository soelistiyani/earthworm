#define _USE_MATH_DEFINES


#include <sys/timeb.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <earthworm.h>
#include <transport.h>
#include <trace_buf.h>
#include <swap.h>
#include <trheadconv.h>

#include <time_ew.h>
#include <math.h>
#include <chron3.h>
#include <float.h>

#include <math.h>
#include "pick_gm.h"

#if defined(WIN32) || defined(WIN64)
// Copied from linux libc sys/stat.h:
#define ISNAN(m) _isnan(m)
#else
#define ISNAN(m) isnan(m)
#endif

double getMMI_BCAV(double p_BCAV)
{
	double wk_MMI;

	wk_MMI = 0.0;
	if(p_BCAV < (double)0.00000001) return wk_MMI;
	wk_MMI = 2.316*log10(p_BCAV/1.414213562) + 0.942;	// divided by SQRT(2)
	if(wk_MMI < 0.5) return 0;
	return wk_MMI;
}

double getMMI_BSPGA(double p_BSPGA)
{
	double wk_MMI;

	wk_MMI = 0.0;
	if(p_BSPGA < (double)0.00000001) return wk_MMI;
	wk_MMI = 2.208*log10(p_BSPGA/1.414213562) - 0.202;	// divided by SQRT(2)
	if(wk_MMI < 0.5) return 0;
	return wk_MMI;
}


int sniff_gmx(STATION *Sta, STATION *StaGmx)
{
	int i = 0;
	double tmpTM = 0.;
	double tmpPGA = 0.;
	double tmpCAV = 0.;	// by Seo JeongBeom
	bool isFind = false;

	// Don't need Z comp.
	if (Sta->chan[2] == 'Z')
	{
		return 1;
	}

	// E or N comp.
	for ( i = 0; i < GMXLEN; i++ )
	{
		if ( StaGmx->gmx[i].tm == -1 ) continue;

		// find comp
		if ( StaGmx->gmx[i].tm == Sta->tm)
		{
			// calculate PGA
			tmpPGA = sqrt((StaGmx->gmx[i].pga * StaGmx->gmx[i].pga) + (Sta->pga * Sta->pga));	// by Seo JeongBeom
//			tmpPGA = sqrt(fabs(StaGmx->gmx[i].pga * Sta->pga));	// by Seo JeongBeom

			// calculate CAV
			tmpCAV = sqrt((StaGmx->gmx[i].cav * StaGmx->gmx[i].cav) + (Sta->cav * Sta->cav));

			tmpTM = StaGmx->gmx[i].tm;

			// expire
			StaGmx->gmx[i].tm = -1;
			StaGmx->gmx[i].viewCnt = 0;
			StaGmx->gmx[i].pga = 0;
			StaGmx->gmx[i].cav = 0;

			isFind = true;
			break;
		}
		else 
		{
			// inc viewCnt
			StaGmx->gmx[i].viewCnt += 1;

			// check expire
			if (StaGmx->gmx[i].viewCnt > 180) // limit for 3minutes.
			{
				StaGmx->gmx[i].tm = -1;
				StaGmx->gmx[i].viewCnt = 0;
				StaGmx->gmx[i].pga = 0;
				StaGmx->gmx[i].cav = 0;				
			}
		}
	}

	if (isFind)
	{

		StaGmx->pga = tmpPGA;
		StaGmx->cav = tmpCAV;
		StaGmx->tm = tmpTM;
		StaGmx->is_oversec = true;


		// move pga array
		for (i=1; i<ACCUMLEN; i++) {
			StaGmx->arpga[i-1] = StaGmx->arpga[i];
			StaGmx->arcav[i-1] = StaGmx->arcav[i]; 			
		}
		StaGmx->arpga[ACCUMLEN-1] = fabs(StaGmx->pga);
		StaGmx->arcav[ACCUMLEN-1] = fabs(StaGmx->cav);		

		// find max-pga, lbspga in 30sec. 주의 시간보장이 된 상태에서만 써야 정확한 결과를 내줌..
		StaGmx->maxpga = StaGmx->arpga[0];
		StaGmx->lbspga = 0;
		StaGmx->bcav = 0;

		for (i=0; i<ACCUMLEN; i++) {
			if (StaGmx->arpga[i] > StaGmx->maxpga) StaGmx->maxpga = StaGmx->arpga[i];
			if (StaGmx->arpga[i] > 0.098) StaGmx->lbspga += StaGmx->arpga[i];
			//if (StaGmx->arpga[i] > 0.0001) StaGmx->lbspga += StaGmx->arpga[i];
			if (StaGmx->arcav[i] > 0.098) StaGmx->bcav += StaGmx->arcav[i];			
		}
		StaGmx->bcav_mmi = getMMI_BCAV(StaGmx->bcav);
		StaGmx->lbspga_mmi = getMMI_BSPGA(StaGmx->lbspga);

		//printf("%s.%s.%s.%s %f %f %f %f %f %f\n"
		//					, StaGmx->sta, StaGmx->chan, StaGmx->net, StaGmx->loc, StaGmx->lat, StaGmx->lon, StaGmx->tm
		//					, StaGmx->pga, StaGmx->maxpga, StaGmx->lbspga);
	}
	else 
	{
		// get empty index
		for ( i=0; i<GMXLEN; i++)
		{
			if ( StaGmx->gmx[i].tm == -1)
			{
				StaGmx->gmx[i].tm = Sta->tm;
				StaGmx->gmx[i].pga = Sta->pga;
				StaGmx->gmx[i].cav = Sta->cav;				
				StaGmx->gmx[i].viewCnt = 0;
				break;
			}
		}

		StaGmx->is_oversec = false;
	}

	return 1;
}

// is_restart: 1(restarting...)
// is_restart: 0(not restarting...)	 
int sniff_gm(double pkt_time, int LongSample, STATION *Sta, int is_restart)
{

	//int       i = 0;
	
	double    B0 = 0.;
	double    B1 = 0.;
	double    B2 = 0.;
	double    A1 = 0.;
	double    A2 = 0.;
	
	//-- For TcPd analysis 
	double    OUTPUT,acc,acc_vel,ddv;
	long      t_pkt_time;

	//double delay;
	// Initializing 0.075Hz two poles Butterworth high pass filter
	
	if(Sta->srate == 100)
	{
		B0 = 0.9966734;
		B1 = -1.993347;
		B2 = 0.9966734;
		A1 = -1.993336;
		A2 = 0.9933579;
	}     
	else if(Sta->srate == 50)
	{
		B0 = 0.993357897;
		B1 = -1.98671579;
		B2 = 0.993357897;
		A1 = -1.98667157;
		A2 = 0.986759841;
	}	
	else if(Sta->srate == 40)
	{
		B0 = 0.9917042;
		B1 = -1.983408;
		B2 = 0.9917042;
		A1 = -1.983340;
		A2 = 0.9834772;
	}	
	else if(Sta->srate == 20)
	{
		B0 = 0.983477175;
		B1 = -1.96695435;
		B2 = 0.983477175;
		A1 = -1.96668136;
		A2 = 0.967227399;
	}
	else
	{
		// Directly calculate
		double ff = 0.075 / (double)Sta->srate;
		double ita = 1.0 / tan(M_PI*ff);
		double q = sqrt(2.0);
		B0 = 1.0 / (1.0 + q * ita + ita * ita);
		B1 = 2 * B0;
		B2 = B0;
		A1 = -2.0 * ( ita * ita - 1.0 ) * B0;
		A2 = (1.0 - q * ita + ita * ita ) * B0;
		B0 = B0 * ita * ita;
		B1 = -B1 * ita * ita;
		B2 = B2 * ita * ita;
	}

	////////////// ORIGINAL CODE ////////////////////////
	Sta->avd[0]=Sta->a;
	Sta->avd[1]=Sta->v;
	Sta->avd[2]=Sta->d; 

	Sta->v=LongSample;   // fetch trace data point by point	                                                                                                                                                                                                                         
	Sta->v=Sta->v/Sta->gain;       /*  for gain factor   */
	Sta->ave = Sta->ave*999.0/1000.0+Sta->v/1000.0;	// remove instrument response
	Sta->v=Sta->v-(Sta->ave);				// remove offset      
			
	// temp code
	//printf(">>> %d %f %f %f\n", LongSample, Sta->v, Sta->ave, Sta->dt);	
	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz  */
	OUTPUT = B0*Sta->v + B1*Sta->X1 + B2*Sta->X2;
	OUTPUT = OUTPUT - ( A1*Sta->Y1 + A2*Sta->Y2 );
	Sta->Y2 = Sta->Y1;
	Sta->Y1 = OUTPUT;
	Sta->X2 = Sta->X1;
	Sta->X1 = Sta->v;
	Sta->v = OUTPUT;
	/*--------------End of Recursive Filter*/                                        
	
	// jman, 20170802
	//if (Sta->inst==1)
	//{
	//	Sta->a = Sta->v;
	//}

	//-- for Acc to Vel
	if(Sta->inst==1)
	{

		//if ( strcmp(Sta->chan, "HGZ")) {
		//	printf("%f %f %f %f %f\n", pkt_time, Sta->v, Sta->vel0, Sta->acc0, Sta->ave0); 
		//}

		acc=Sta->v;
		Sta->v=(acc+Sta->acc0)*Sta->dt/2.0+Sta->vel0;
		Sta->acc0=acc;
		Sta->vel0=Sta->v;
		Sta->ave0 = Sta->ave0*(10.-Sta->dt)/10.0+Sta->v*Sta->dt/10.0;
		Sta->v=Sta->v-Sta->ave0;   

		//if ( strcmp(Sta->chan, "HGZ")) {
		//	printf("%f %f %f %f %f\n", pkt_time, Sta->v, Sta->vel0, Sta->acc0, Sta->ave0); 
		//}

	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
		OUTPUT = B0*Sta->v + B1*Sta->XX1 + B2*Sta->XX2;
		OUTPUT = OUTPUT - ( A1*Sta->YY1 + A2*Sta->YY2 );
		Sta->YY2 = Sta->YY1;
		Sta->YY1 = OUTPUT;
		Sta->XX2 = Sta->XX1;
		Sta->XX1 = Sta->v;
		Sta->v = OUTPUT;
	/*--------------End of Recursive Filter*/                                                                                               
	}    
	//--------------------------------------------------------------------------------------------                                                                                                                                 
	
	// jman, 20170802
	//if ( Sta->inst == 2)
	//{
	//	Sta->a=(Sta->v - Sta->avd[1])/Sta->dt;	
	//
	//	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
	//	OUTPUT = B0*Sta->a + B1*Sta->xx1 + B2*Sta->xx2;
	//	OUTPUT = OUTPUT - ( A1*Sta->yy1 + A2*Sta->yy2 );
	//	Sta->yy2 = Sta->yy1;
	//	Sta->yy1 = OUTPUT;
	//	Sta->xx2 = Sta->xx1;
	//	Sta->xx1 = Sta->a;
	//	Sta->a = OUTPUT;
	//	/*--------------End of Recursive Filter*/   

	//}
	Sta->a = (Sta->v - Sta->avd[1])/Sta->dt;	 

	acc_vel = Sta->v;
	Sta->d = (acc_vel+Sta->acc0_vel)*Sta->dt/2.0+Sta->vel0_dis;
	Sta->acc0_vel = acc_vel;
	Sta->vel0_dis = Sta->d;
	Sta->ave1 = Sta->ave1*(10.-Sta->dt)/10.0+Sta->d*Sta->dt/10.0;
	Sta->d=Sta->d-Sta->ave1;                           

	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
	OUTPUT = B0*Sta->d + B1*Sta->x1 + B2*Sta->x2;
	OUTPUT = OUTPUT - ( A1*Sta->y1 + A2*Sta->y2 );
	Sta->y2 = Sta->y1;
	Sta->y1 = OUTPUT;
	Sta->x2 = Sta->x1;
	Sta->x1 = Sta->d;
	Sta->d = OUTPUT;
	/*--------------End of Recursive Filter*/ 

	ddv = (Sta->d- Sta->avd[2] )/Sta->dt;

	Sta->ddsum += Sta->d*Sta->d;
	Sta->vvsum += ddv * ddv;      

	Sta->tc=2.*3.141592654/sqrt((Sta->vvsum)/(Sta->ddsum));    
	/////////// ORIGINAL CODE END //////////////////////////////



	/////////// MODIFIY CODE ///////////////////////////////////
	//Sta->avd[0]=Sta->a;
	//Sta->avd[1]=Sta->v;
	//Sta->avd[2]=Sta->d; 

	//Sta->v=LongSample;   // fetch trace data point by point	                                                                                                                                                                                                                         
	//Sta->v=Sta->v/Sta->gain;       /*  for gain factor   */
	//Sta->ave = Sta->ave*999.0/1000.0+Sta->v/1000.0;	// remove instrument response
	//Sta->v=Sta->v-(Sta->ave);				// remove offset      
	//		
	//// temp code
	////printf(">>> %d %f %f %f\n", LongSample, Sta->v, Sta->ave, Sta->dt);	
	///*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz  */
	//OUTPUT = B0*Sta->v + B1*Sta->X1 + B2*Sta->X2;
	//OUTPUT = OUTPUT - ( A1*Sta->Y1 + A2*Sta->Y2 );
	//Sta->Y2 = Sta->Y1;
	//Sta->Y1 = OUTPUT;
	//Sta->X2 = Sta->X1;
	//Sta->X1 = Sta->v;
	//Sta->v = OUTPUT;
	///*--------------End of Recursive Filter*/                                        
	//
	//// jman, 20170802
	//if (Sta->inst==1)
	//{
	//	// Acc is Acc...
	//	Sta->a = Sta->v;

	//	// Acc to Vel
	//	acc=Sta->v;
	//	Sta->v=(acc+Sta->acc0)*Sta->dt/2.0+Sta->vel0;
	//	Sta->acc0=acc;
	//	Sta->vel0=Sta->v;
	//	Sta->ave0 = Sta->ave0*(10.-Sta->dt)/10.0+Sta->v*Sta->dt/10.0;
	//	Sta->v=Sta->v-Sta->ave0;     

	///*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
	//	OUTPUT = B0*Sta->v + B1*Sta->XX1 + B2*Sta->XX2;
	//	OUTPUT = OUTPUT - ( A1*Sta->YY1 + A2*Sta->YY2 );
	//	Sta->YY2 = Sta->YY1;
	//	Sta->YY1 = OUTPUT;
	//	Sta->XX2 = Sta->XX1;
	//	Sta->XX1 = Sta->v;
	//	Sta->v = OUTPUT;
	///*--------------End of Recursive Filter*/   

	//}

	//// jman, 20170802
	//if ( Sta->inst == 2)
	//{
	//	// Vel to Acc
	//	Sta->a=(Sta->v - Sta->avd[1])/Sta->dt;	
	//
	//	/*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
	//	OUTPUT = B0*Sta->a + B1*Sta->xx1 + B2*Sta->xx2;
	//	OUTPUT = OUTPUT - ( A1*Sta->yy1 + A2*Sta->yy2 );
	//	Sta->yy2 = Sta->yy1;
	//	Sta->yy1 = OUTPUT;
	//	Sta->xx2 = Sta->xx1;
	//	Sta->xx1 = Sta->a;
	//	Sta->a = OUTPUT;
	//	/*--------------End of Recursive Filter*/   

	//}

	//acc_vel = Sta->v;
	//Sta->d = (acc_vel+Sta->acc0_vel)*Sta->dt/2.0+Sta->vel0_dis;
	//Sta->acc0_vel = acc_vel;
	//Sta->vel0_dis = Sta->d;
	//Sta->ave1 = Sta->ave1*(10.-Sta->dt)/10.0+Sta->d*Sta->dt/10.0;
	//Sta->d=Sta->d-Sta->ave1;                           

	///*--------------For Recursive Filter  high pass 2 poles at 0.075 Hz */
	//OUTPUT = B0*Sta->d + B1*Sta->x1 + B2*Sta->x2;
	//OUTPUT = OUTPUT - ( A1*Sta->y1 + A2*Sta->y2 );
	//Sta->y2 = Sta->y1;
	//Sta->y1 = OUTPUT;
	//Sta->x2 = Sta->x1;
	//Sta->x1 = Sta->d;
	//Sta->d = OUTPUT;
	///*--------------End of Recursive Filter*/ 
	/////////// MODIFIY CODE END ///////////////////////////////////

	ddv = (Sta->d- Sta->avd[2] )/Sta->dt;

	Sta->ddsum += Sta->d*Sta->d;
	Sta->vvsum += ddv * ddv;      

	Sta->tc=2.*3.141592654/sqrt((Sta->vvsum)/(Sta->ddsum)); 



 //--------------------------------------------------------------------------------------------                           
	// Get PGA, PGD, PGV
	t_pkt_time = (long)pkt_time;



	 //printf("%s.%s.%s.%s %f %f %f %f\n"
	 //					, Sta->sta, Sta->chan, Sta->net, Sta->loc, pkt_time, LongSample 
	 //					, Sta->a, Sta->v);

	// Start condition
	if (Sta->pkt_1s_time == 0) 
	{
		Sta->pkt_1s_time = t_pkt_time;
		if (!ISNAN(Sta->a)) Sta->t_pga = Sta->a;
		else Sta->t_pga = 0.;
		if (!ISNAN(Sta->d)) Sta->t_pgd = Sta->d;
		else Sta->t_pgd = 0.;
		if (!ISNAN(Sta->v)) Sta->t_pgv = Sta->v;
		else Sta->t_pgv = 0.;
		if (!ISNAN(Sta->a)) Sta->t_cav = fabs(Sta->a/Sta->srate);
		else Sta->t_cav = 0.;

		// jman. 20170420
		if ( is_restart )
		{
			Sta->t_pga = 0.;
			Sta->t_pgv = 0.;
			Sta->t_pgd = 0.;
			Sta->t_cav = 0.;
		}

		return 1;
	}

	// Diff time with 1sec. Assume that gap is exist.
	if (t_pkt_time > Sta->pkt_1s_time)
	{
		//int i = 0;
		// Over second
		// set old second value
		Sta->tm = (double)Sta->pkt_1s_time;
		Sta->pga = Sta->t_pga;
		Sta->pgd = Sta->t_pgd;
		Sta->pgv = Sta->t_pgv;
		Sta->cav = Sta->t_cav;

		// set next second value
		if (!ISNAN(Sta->a)) Sta->t_pga = Sta->a;
		else Sta->t_pga = 0.;
		if (!ISNAN(Sta->d)) Sta->t_pgd = Sta->d;
		else Sta->t_pgd = 0.;
		if (!ISNAN(Sta->a)) Sta->t_pgv = Sta->v;
		else Sta->t_pgv = 0.;
		if (!ISNAN(Sta->a)) Sta->t_cav = fabs(Sta->a/Sta->srate);
		else Sta->t_cav = fabs(Sta->pa/Sta->srate);

		// jman. 20170420
		if ( is_restart )
		{
			Sta->t_pga = 0.;
			Sta->t_pgv = 0.;
			Sta->t_pgd = 0.;
			Sta->t_cav = 0.;
		}
		
		Sta->is_oversec = true;
		Sta->pkt_1s_time = t_pkt_time;
	}
	else 
	{
		// check inf value
		if (!ISNAN(Sta->a)) 
			if (fabs(Sta->a) > fabs(Sta->t_pga)) Sta->t_pga = Sta->a;
		
		if (!ISNAN(Sta->d))
			if (fabs(Sta->d) > fabs(Sta->t_pgd)) Sta->t_pgd = Sta->d;

		if (!ISNAN(Sta->v))
			if (fabs(Sta->v) > fabs(Sta->t_pgv))  Sta->t_pgv = Sta->v;

		if (!ISNAN(Sta->a)) {
			Sta->t_cav += fabs(Sta->a/Sta->srate);
		}else{
			Sta->t_cav += fabs(Sta->pa/Sta->srate);			
		}

		// jman. 20170420
		if ( is_restart )
		{
			Sta->t_pga = 0.;
			Sta->t_pgv = 0.;
			Sta->t_pgd = 0.;
			Sta->t_cav = 0.;
		}
				
		Sta->is_oversec = false;
	}

	if (!ISNAN(Sta->a)) Sta->pa = Sta->a;	// by Seo JeongBeom

	return 1;
}
