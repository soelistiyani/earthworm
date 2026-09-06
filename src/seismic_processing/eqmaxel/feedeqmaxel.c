/*
 *  feedeqmaxel:  Stub program for testing eqmaxel.
 *    Reads from stdin the names files containing a single 
 *    TYPE_EVENT_SCNL message each.  Reads the contents
 *    of the file and pipes it to the command "eqmaxel eqmaxel.d"
 *
 *  Example startup command:
 *     ls *.event | feedeqmaxel    (processes all .event files in cwd)
 * Author of this version: Paul Friberg, ISTI
 *
 * This module was taken from USGS version written by Lucky Vidmar and Lynn Dietz
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <earthworm.h>

int main( int argc, char *argv[] )
{
   static char   msg[MAX_BYTES_PER_EQ]; /* message from arcfile   */
   char          line[256];             /* one line at a time     */
   char          fname[50];             /* archive file name      */
   char          junk; 
   int           n;                     /* working pointer to msg */
   int           length;
   FILE         *fp;
   unsigned char TypeEventSCNL;
   unsigned char TypeKill;

/* Look up message types in earthworm.h tables
 *********************************************/
   GetType( "TYPE_EVENT_SCNL", &TypeEventSCNL );
   GetType( "TYPE_KILL",       &TypeKill );

/* Spawn the next process
 ************************/
   if ( pipe_init( "eqmaxel eqmaxel.d", (unsigned long)0 ) == -1 )
   {
      printf( "feedeqmaxel: Error starting eqmaxel; exiting!\n" );
      exit ( -1 );
   }
   printf( "feedeqmaxel: piping output to eqmaxel\n" );

/* Loop over all file names fed to stdin
 ***************************************/
   while( scanf( "%s", fname ) != EOF )
   {
   /* Open next file
    ************************/
      if( (fp=fopen( fname, "r")) == (FILE *) NULL ) 
      {
         printf( "feedeqmaxel: error opening file <%s>\n", fname );
         continue;
      }

   /* Read event message from file
    *******************************/
      n=0;
      while( n < MAX_BYTES_PER_EQ-1 )
      {
         if( fscanf( fp, "%[^\n]", line ) == EOF )  {
              break;
         }
         fscanf( fp, "%c", &junk ); /*read newline*/
         length = strlen(line);
         strncpy( msg+n, line, length );
         n += length;
         msg[n++] = '\n';
      } 
      msg[n] = '\0';
      /*printf("Next event:\n%s", msg );*/ /*DEBUG*/

   /* Send msg down pipe
    ********************/
      if ( pipe_put( msg, TypeEventSCNL ) != 0 )
         printf( "feedeqmaxel: Error writing msg to pipe.\n"); 

      sleep_ew(1000);
      fclose(fp);
   }

   if ( pipe_put( "", TypeKill ) != 0 )
       printf( "feedeqmaxel: Error writing kill msg to pipe.\n");

   exit( 0 );
}
