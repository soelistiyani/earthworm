/*
	geojson2ew - geoJSON to earthworm 

        Copyright (c) 2014 California Institute of Technology. All rights reserved.
        Authors: Kevin Frechette & Paul Friberg, ISTI.

        Copyright ©2017 The Regents of the University of California. All rights reserved.
        Authors: Mario Aranha, Berkeley Seismological Laboratory.

        This program is distributed by ISTI WITHOUT ANY WARRANTY whatsoever.
        Do not redistribute this program without written permission from ISTI.
*/
#define MAIN
#define THREAD_STACK_SIZE 8192
#include "main.h"

volatile int err_exit = -1;		/* an geojson2ew_die() error number */
time_t  tsLastMsg; 			/* time stamp of last message */

/* signal handler that intiates a shutdown */
void initiate_termination(int sigval) {
    signal(sigval, initiate_termination);
    err_exit = GEOJSON2EW_DEATH_SIG_TRAP;
    geojson2ew_die(err_exit, "Termination signal received");

    return;
}

void get_json_messages() {
	char *msg;
	time_t now;
	int is_connected = 0;
	FILE *fp = NULL;
	int dump_data = 0;

 conn:
	while( !is_connected && !ShutMeDown ) {
	  /* sleep so statmgr gets heartbeat and connecting
	   to (geojson) server does not consume 100% of CPU;
	   HB thread takes care of TERMINATE from EW.
	  */
	  sleep_ew(2000);
	  logit("et", "%s: Connecting to server ...\n", Progname);
	  if ( open_json_connection(&Conn_params) == 0 ) {
	    is_connected = 1;
	    logit("et", "%s: Connected to server successfully\n", Progname);
	  }
	}

	time(&tsLastMsg);  /* set initial time to now */
	if( strlen(Conn_params.data_dumpfile) > 0 ) {
	  if((fp = fopen(Conn_params.data_dumpfile, "a")) != NULL) {
	    dump_data = 1;
	    logit("et", "%s: Dumping data to %s ...\n", Progname, Conn_params.data_dumpfile);
	  } else {
	    geojson2ew_die(err_exit, "Data Dump File open error");
	    dump_data = 0;
	  }
	}

	while ( !ShutMeDown ) {
	    if (( msg = read_json_message()) != NULL ) {
	         if ( dump_data ) {
		    (void)fprintf(fp, "%s\n", msg);
	         }
		 if ( Conn_params.data_timeout )
		   time(&tsLastMsg);  /* update last timestamp */
		 if ( (err_exit = process_json_message(msg)) ) {
		   geojson2ew_die(err_exit, "geoJSON Process error");
		 }
		 free_json_message();
	    } else if ( Conn_params.status != 0 ) {
	       logit("et", "%s: Connection error, closing connection\n",
		       Progname);
	       close_json_connection();
	       is_connected = 0;
	       goto conn;
	    } else if ( Conn_params.data_timeout ) {
	      time(&now);  /* get current timestamp */
               if ( difftime(now, tsLastMsg) > Conn_params.data_timeout ) {
		 logit("et", "%s: Data timeout after %d secs, closing connection\n",
		       Progname, Conn_params.data_timeout);
		  close_json_connection();
		  is_connected = 0;
		  goto conn;
	       }
            } 
	}

	if ( dump_data && fp != NULL ) {
	  logit("et", "%s: Closing data file %s ...\n", Progname, Conn_params.data_dumpfile);
	  (void)fclose(fp);
	}

	/* cleanup on shutdown */
	close_json_connection();
	is_connected = 0;

	return;
}

int main (int argc, char ** argv) {
	/* init some globals */
	MyPid = getpid();      /* set it once on entry */
	Verbose = 0;
	VerboseSncl = NULL;
	ShutMeDown = FALSE;
	Region.mid = -1;	/* init so we know if an attach happened */

	/* for multiple threads changing global vars, e.g., ShutMeDown */
	CreateMutex_ew();

	/* handle some options */
	handle_opts(argc, argv);

	if ( !ShutMeDown ) {
	  /* init earthworm connection */
	  tport_attach( &Region, RingKey );
	  logit("et", "%s: Attached to Ring Key=%ld\n", Progname, RingKey);

	  /* start EW heartbeat thread */
	  if ( (StartThread(Heartbeat,(unsigned)THREAD_STACK_SIZE,&TidHB)) == -1) {
	    /* we have a problem with starting this thread */
	    logit("et", "%s: Heartbeat startup failed\n", Progname);
	    geojson2ew_die(-1, "Heartbeat startup failure" );
	  }
	}
	
	/* deal with some signals */
	signal(SIGINT, initiate_termination);
	signal(SIGTERM, initiate_termination);

	if ( !ShutMeDown ) {
	   logit("et", "%s: Start getting JSON Messages\n", Progname);
	   get_json_messages();
	   logit("et", "%s: End getting JSON Messages\n", Progname);
	}

	/* this next bit must come after ANY possible tport_putmsg() */
	if (Region.mid != -1) {
		/* we are attached to an EW ring buffer */
		logit("et", "%s: Detached from EW Ring\n", Progname);
		tport_detach( &Region );
	}

	CloseMutex();

	exit(0);
}
