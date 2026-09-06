#include <earthworm.h>
#include <kom.h>
#include <tlay.h>

int main(int argc, char *argv[]) {

int nfiles;
int i;
char    *com;

int maxdist=2000;
double z=8., dtdr, dtdz;

   if (argc < 2) {
      printf("usage: bldtrtrable velocity_model.d\n");
      exit(1);
   }

   nfiles = k_open( argv[1] );
   if ( nfiles == 0 ) {
        exit( -1 );
   }

/* Process all command files
 ***************************/
   while(nfiles > 0)   /* While there are command files open */
   {
        while(k_rd())        /* Read next line from active file  */
        {
            com = k_str();         /* Get the first token from line */
            if( !com )           continue;
            if( com[0] == '#' )  continue;
            t_com();
            if( k_err() ) {
               logit( "e",
                       "bldtrtable: Bad <%s> command in <%s>; exiting!\n",
                        com, argv[1] );
               exit( -1 );
            }
        }
        nfiles = k_close();
   }

   for(i=1;i<=maxdist;i++) {
      fprintf(stdout, "%d %f\n", i, t_lay( (double) i, z, &dtdr, &dtdz));
   }
   exit(0);
}
