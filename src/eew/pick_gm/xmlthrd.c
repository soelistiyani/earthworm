
/*
 *   THIS FILE IS UNDER RCS - DO NOT MODIFY UNLESS YOU HAVE
 *   CHECKED IT OUT USING THE COMMAND CHECKOUT.
 *
 *   Modify of firthrd.c earthworm
 */
#ifdef _WINNT
  #include <windows.h>
  #define mutex_t HANDLE
#else
  #include <string.h>
  #ifdef _SOLARIS
	#ifndef _LINUX	/* synch.h not posix threads */
	  #include <synch.h>      /* mutex's                                      */
	#endif
  #endif
#endif

/*******                                                        *********/
/*      Earthworm Includes                                              */
/*******                                                        *********/
#include "earthworm.h"  /* logit, threads                               */

/*******                                                        *********/
/*      Includes                                                    */
/*******                                                        *********/
#include "pick_gm.h"
#include <time.h>

#define MAX_LEN_STRING 512

/*******                                                        *********/
/*      Function definitions                                            */
/*******                                                        *********/
int GMtoXml(const char *, STATION *, int, GPARM *);
int GMXtoXml(const char *, STATION *, int, GPARM *);
int CompareSCNL( const void *, const void * );
int GetStaList( STATION **, int *, GPARM *, bool );
int cmpfunc(const void *a, const void *b)
{
	const char *pa = *(const char**)a;
	const char *pb = *(const char**)b;

	return strcmp(pa, pb);
}

/* Function: XmlThreadGM */
thr_ret XmlThreadGM (void* ptr)
{
	GPARM *Gparm;
  	char outBuf[200];
  	long outBufLen;
	MSG_LOGO foologo;
	int ret;
	int i;
	char **strArray;
	int nstr = 0;
	char *prevStr = NULL;
	STATION *StaArray = NULL;
	int Nsta = 0;
	int loop_condition = 1;

	Gparm = ( GPARM *) ptr;
	Gparm->isCompleteThrdGM = false;

	/* alloc strArray */
	strArray = (char**)malloc(sizeof(char*) * 3000);
	for (i=0; i < 3000; i++)
	{
		strArray[i] = (char*)malloc(sizeof(char) * MAX_LEN_STRING);
	}

	GetStaList( &StaArray, &Nsta, Gparm, true );
	qsort (StaArray, Nsta, sizeof(STATION), CompareSCNL);

	while(loop_condition) 
	{
		/* Get all message from the MsgQueue */
		 while(loop_condition) 
		{
			RequestMutex ();
			ret = dequeue (&(Gparm->MsgQueueGM), outBuf, &outBufLen, &foologo);
			ReleaseMutex_ew ();

			if (ret < 0)
		    {   /* empty queue */
			    break;
		    }

		 	outBuf[outBufLen] = '\0';
		 	strcpy(strArray[nstr], outBuf);
		 	nstr++;
		 	//printf(">>>> %d, %s\n", getNumOfElementsInQueue(&(Gparm->MsgQueueGM)), outBuf);
		}
		
		/* No message */
		if (nstr == 0) 
		{
		 	sleep_ew(300);
		 	continue;
		}

		/* Set flag for thread */
		Gparm->isCompleteThrdGM = false;

		/* Sort message */
		qsort(strArray, nstr, sizeof(char *), cmpfunc);

		/* Check duplicate */
		prevStr = strArray[0];
		for(i=1; i<nstr; i++) 
		{
			if ( !strcmp(prevStr, strArray[i]) ) {}
			else 
			{
				// different
				GMtoXml(prevStr, StaArray, Nsta, Gparm);
				prevStr = strArray[i];
			}
		}
		GMtoXml(prevStr, StaArray, Nsta, Gparm);
		//printf(">>>> prevStr: %s, nstr: %d, queue: %d, nsta: %d\n", prevStr, nstr, getNumOfElementsInQueue(&(Gparm->MsgQueueGM)), Nsta);

		// reset nstr
		nstr = 0;
	} /* while (1) - message dequeuing process */
	return THR_NULL_RET;
}

int GMtoXml(const char * fn, STATION *StaArray, int Nsta, GPARM *Gparm) 
{
	FILE *fp;
	STATION sta = { 0 };
	char string[MAX_LEN_STRING];
	STATION *Sta;
	STATION key;
	int i;
	int staOpen = false;
	char lastSta[6];
	char lastNet[3];
	char fnXml[200];
	const double G = 9.81;  /* Convert from cm/sec/sec to percent G */

	memset(lastSta, 0, 6);
	memset(lastNet, 0, 3);
	memset(fnXml, 0, 200);
	for(i=0; i<Nsta; i++)
	{
		StaArray[i].pga = -1.;
		StaArray[i].pgv = -1.;
		StaArray[i].pgd = -1.;	
	}
	fp = fopen(fn, "r");
	while ( fgets( string, MAX_LEN_STRING, fp ) != NULL )
	{
		int ndecoded;
		ndecoded = sscanf(string,"%s %s %s %s %lf %lf %lf %lf %lf %lf"
		, sta.sta,  sta.chan,  sta.net,  sta.loc
		, &sta.lat, &sta.lon, &sta.tm, &sta.pga, &sta.pgv, &sta.pgd);

		// incomplete line
		if (ndecoded != 10) 
		{
			logit("et", "Pick_gm: Incomplete line occurred... %s.%s.%s.%s %d in %s\n", sta.sta, sta.chan, sta.net, sta.loc, ndecoded, fn);
			continue;
		}

		strcpy(key.sta, sta.sta);
		strcpy(key.chan, sta.chan);
		strcpy(key.loc, sta.loc);
		strcpy(key.net, sta.net);

		Sta = (STATION *) bsearch (&key, StaArray, Nsta, sizeof(STATION),
				CompareSCNL);

		if ( Sta == NULL ) {
			//printf("NULL %s %s\n", key.sta, key.chan);
			continue;
		}

		// skip first 45sec data
		if ( Sta->first_tm < 0 ) {
			Sta->first_tm = sta.tm;
			continue;
		} else if ( (Sta->first_tm + 45.0) >= sta.tm ) {
			continue;
		}

		if (sta.pga > Sta->pga) Sta->pga = sta.pga;
		if (sta.pgv > Sta->pgv) Sta->pgv = sta.pgv;
		if (sta.pgd > Sta->pgd) Sta->pgd = sta.pgd;
	}
	fclose(fp);

	sprintf(fnXml, "%s.xml", fn);
	fp = fopen(fnXml, "w");

	/* start XML */
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"US-ASCII\" standalone=\"yes\"?>\n");
	fprintf(fp, "<stationlist created=\"%ld\">\n", (long)time((time_t *)0));

	for(i=0; i<Nsta; i++)
	{
		/* If station, net or instType has changed, start new station entry */
		if ( ((strcmp(lastSta, StaArray[i].sta) != 0) || (strcmp(lastNet, StaArray[i].net) != 0))
			&& ( StaArray[i].pga > 0 || StaArray[i].pgv > 0 ))
		{
			if (staOpen) fprintf(fp, "\t</station>\n");
			fprintf(fp, "\t<station code=\"%s\" name=\"%s\" insttype=\"%s\" lat=\"%f\" "
			"lon=\"%f\" source=\"%s\" commtype=\"DIG\" >\n", 
				StaArray[i].sta,
				StaArray[i].sta,
				StaArray[i].net,
				StaArray[i].lat,
				StaArray[i].lon,
				StaArray[i].sta);
			staOpen = true;

			strcpy(lastSta, StaArray[i].sta);	
			strcpy(lastNet, StaArray[i].net);	
		}
		
		if ( StaArray[i].pga > 0 || StaArray[i].pgv > 0 ) 
		{
			fprintf(fp, "\t\t<comp name=\"%s\">\n", StaArray[i].chan);

			if ( StaArray[i].pga > 0) fprintf(fp, "\t\t\t<acc value=\"%.4f\" />\n", StaArray[i].pga / G);
			if ( StaArray[i].pgv > 0) fprintf(fp, "\t\t\t<vel value=\"%.4f\" />\n", StaArray[i].pgv);

			fprintf(fp, "\t\t</comp>\n");
		}
	}
	if (staOpen) fprintf(fp, "\t</station>\n");


	/* end XML */
	fprintf(fp, "</stationlist>\n");

	fclose(fp);
	Gparm->isCompleteThrdGM = true;
	return 0;
}

/* Function: XmlThreadGM */
thr_ret XmlThreadGMX (void* ptr)
{
	GPARM         *Gparm;
  	char outBuf[200];
  	long outBufLen;
	MSG_LOGO foologo;
	int ret;
	int i;
	char    **strArray;
	int nstr = 0;
	char *prevStr = NULL;
	STATION *StaArray = NULL;
	int Nsta = 0;
	int loop_condition = 1;

	Gparm = ( GPARM *) ptr;
	Gparm->isCompleteThrdGMX = false;

	/* alloc strArray */
	strArray = (char**)malloc(sizeof(char*) * 3000);
	for (i=0; i < 3000; i++)
	{
		strArray[i] = (char*)malloc(sizeof(char) * MAX_LEN_STRING);
	}

	GetStaList( &StaArray, &Nsta, Gparm, false );
	qsort (StaArray, Nsta, sizeof(STATION), CompareSCNL);

	while(loop_condition) 
	{
		/* Get all message from the MsgQueue */
		 while(loop_condition) 
		{
			RequestMutex ();
			ret = dequeue (&(Gparm->MsgQueueGMX), outBuf, &outBufLen, &foologo);
			ReleaseMutex_ew ();

			if (ret < 0)
		    {   /* empty queue */
			    break;
		    }

		 	outBuf[outBufLen] = '\0';
		 	strcpy(strArray[nstr], outBuf);
		 	nstr++;
		 	//printf(">>>> %d, %s\n", getNumOfElementsInQueue(&(Gparm->MsgQueueGMX)), outBuf);
		}
		
		/* No message */
		if (nstr == 0) 
		{
		 	sleep_ew(300);
		 	continue;
		}

		/* Set flag for thread */
		Gparm->isCompleteThrdGMX = false;

		/* Sort message */
		qsort(strArray, nstr, sizeof(char *), cmpfunc);

		/* Check duplicate */
		prevStr = strArray[0];
		for(i=1; i<nstr; i++) 
		{
			if ( !strcmp(prevStr, strArray[i]) ) {}
			else 
			{
				// different
				GMXtoXml(prevStr, StaArray, Nsta, Gparm);
				prevStr = strArray[i];
			}
		}
		GMXtoXml(prevStr, StaArray, Nsta, Gparm);
		//printf(">>>> prevStr: %s, nstr: %d, queue: %d, nsta: %d\n", prevStr, nstr, getNumOfElementsInQueue(&(Gparm->MsgQueueGMX)), Nsta);

		// reset nstr
		nstr = 0;
	} /* while (1) - message dequeuing process */
	return THR_NULL_RET;
}

int GMXtoXml(const char * fn, STATION *StaArray, int Nsta, GPARM *Gparm) 
{
	FILE *fp;
	STATION sta = { 0 };
	char string[MAX_LEN_STRING];
	STATION *Sta;
	STATION key;
	int i;
	int staOpen = false;
	char lastSta[6];
	char lastNet[3];
	char fnXml[200];
	const double G = 9.81;  /* Convert from cm/sec/sec to percent G */
	double bcav_mmi = -1;
	double lbspga_mmi = -1;


	memset(lastSta, 0, 6);
	memset(lastNet, 0, 3);
	memset(fnXml, 0, 200);
	for(i=0; i<Nsta; i++)
	{
		StaArray[i].pga = -1.;
		StaArray[i].maxpga = -1.;
		StaArray[i].lbspga = -1.;	
		StaArray[i].bcav = -1.;
		StaArray[i].bcav_mmi = -1.;
		StaArray[i].lbspga_mmi = -1.;
	}

	fp = fopen(fn, "r");
	while ( fgets( string, MAX_LEN_STRING, fp ) != NULL )
	{
		int ndecoded;
		ndecoded = sscanf(string,"%s %s %s %s %lf %lf %lf %lf %lf %lf %lf %lf %lf"
		, sta.sta,  sta.chan,  sta.net,  sta.loc
		, &sta.lat, &sta.lon, &sta.tm, &sta.pga, &sta.maxpga, &sta.lbspga, &sta.bcav, &sta.lbspga_mmi, &sta.bcav_mmi);

		// incomplete line
		if (ndecoded != 13) 
		{
			logit("et", "Pick_gm: Incomplete line occurred... %s.%s.%s.%s %d in %s\n", sta.sta, sta.chan, sta.net, sta.loc, ndecoded, fn);
			continue;
		}

		if ( Gparm->MMISource == 'A' && sta.chan[1] == 'H' ) 
		{
			continue;
		} 
		else if ( Gparm->MMISource == 'V' && sta.chan[1] == 'G' ) 
		{
			continue;
		}

		strcpy(key.sta, sta.sta);
		strcpy(key.chan, sta.chan);
		strcpy(key.loc, sta.loc);
		strcpy(key.net, sta.net);

		Sta = (STATION *) bsearch (&key, StaArray, Nsta, sizeof(STATION),
				CompareSCNL);

		if ( Sta == NULL ) {
			//printf("NULL %s %s\n", key.sta, key.chan);
			continue;
		}

		// skip first 45second's data
		if ( Sta->first_tm < 0 ) {
			Sta->first_tm = sta.tm;
			continue;
		} else if ( (Sta->first_tm + 45.0) >= sta.tm ) {
			continue;
		}

		//if ( strcmp(Sta->sta, "YSU") == 0 ) {
		//printf("%f %s %s %s %s %f %f %f %f\n", (Sta->first_tm-sta.tm), sta.sta, sta.chan, sta.loc, sta.net, sta.pga, sta.cav, sta.lbspga_mmi, sta.bcav_mmi);
		//}
		if (sta.pga > Sta->pga) Sta->pga = sta.pga;
		if (sta.maxpga > Sta->maxpga) Sta->maxpga = sta.maxpga;
		if (sta.lbspga > Sta->lbspga) Sta->lbspga = sta.lbspga;
		if (sta.bcav > Sta->bcav) Sta->bcav = sta.bcav;
		if (sta.lbspga_mmi > Sta->lbspga_mmi) Sta->lbspga_mmi = sta.lbspga_mmi;
		if (sta.bcav_mmi > Sta->bcav_mmi) Sta->bcav_mmi = sta.bcav_mmi;
		//printf("MAX %f %f\n", Sta->lbspga_mmi, Sta->bcav_mmi);
	}
	fclose(fp);

	sprintf(fnXml, "%s.xml", fn);
	fp = fopen(fnXml, "w");

	/* start XML */
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"US-ASCII\" standalone=\"yes\"?>\n");
	fprintf(fp, "<stationlist created=\"%ld\">\n", (long)time((time_t *)0));

	for(i=0; i<Nsta; i++)
	{

		/* If station, net or instType has changed, start new station entry */
		if ( ((strcmp(lastSta, StaArray[i].sta) != 0) || (strcmp(lastNet, StaArray[i].net) != 0))
			&& ( StaArray[i].pga > 0 ))
		{
			if (staOpen) fprintf(fp, "\t</station>\n");

			bcav_mmi = StaArray[i].bcav_mmi;
			lbspga_mmi = StaArray[i].lbspga_mmi;

			// Consider HG*, HH* 
			if(i + 1 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+1].sta) == 0 && StaArray[i+1].bcav_mmi > bcav_mmi) bcav_mmi = StaArray[i+1].bcav_mmi;
			if(i + 2 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+2].sta) == 0 && StaArray[i+2].bcav_mmi > bcav_mmi) bcav_mmi = StaArray[i+2].bcav_mmi;
			if(i + 3 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+3].sta) == 0 && StaArray[i+3].bcav_mmi > bcav_mmi) bcav_mmi = StaArray[i+3].bcav_mmi;

			if(i + 1 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+1].sta) == 0 && StaArray[i+1].lbspga_mmi > bcav_mmi) lbspga_mmi = StaArray[i+1].lbspga_mmi;
			if(i + 2 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+2].sta) == 0 && StaArray[i+2].lbspga_mmi > bcav_mmi) lbspga_mmi = StaArray[i+2].lbspga_mmi;
			if(i + 3 < Nsta) if(strcmp(StaArray[i].sta, StaArray[i+3].sta) == 0 && StaArray[i+3].lbspga_mmi > bcav_mmi) lbspga_mmi = StaArray[i+3].lbspga_mmi;

			fprintf(fp, "\t<station code=\"%s\" name=\"%s\" netid=\"INTENSITY\" commtype=\"KMA\" insttype=\"%s\" source=\"%s of KMA\" lat=\"%f\" lon=\"%f\" intensity=\"%f\" lbspga_mmi=\"%f\" bcav_mmi=\"%f\" >\n", 
				StaArray[i].sta,
				StaArray[i].sta,
				StaArray[i].net,
				StaArray[i].sta,
				StaArray[i].lat,
				StaArray[i].lon,
				bcav_mmi,
				lbspga_mmi,
				bcav_mmi);
			staOpen = true;

			strcpy(lastSta, StaArray[i].sta);	
			strcpy(lastNet, StaArray[i].net);	
		}

		if ( StaArray[i].pga > 0 ) 
		{
			fprintf(fp, "\t\t<comp name=\"%s\">\n", StaArray[i].chan);
			if ( StaArray[i].pga > 0) fprintf(fp, "\t\t\t<acc value=\"%.4f\" />\n", StaArray[i].pga / G);
			//if ( StaArray[i].pga > 0) fprintf(fp, "\t\t\t<acc value=\"%.4f\" />\n", StaArray[i].pga);
			fprintf(fp, "\t\t</comp>\n");
		}

	}
	if (staOpen) fprintf(fp, "\t</station>\n");


	/* end XML */
	fprintf(fp, "</stationlist>\n");

	fclose(fp);
	Gparm->isCompleteThrdGMX = true;
	return 0;
}