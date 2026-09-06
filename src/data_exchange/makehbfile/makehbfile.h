/*
 *    Revision history:
 *     $Log$
 *     Revision 1.1  2002/12/20 02:40:33  lombard
 *     Initial revision
 */

/*   makehbfile.h    */
#ifndef MAKEHBFILE_H
#define MAKEHBFILE_H
 
#define MAKEHBFILE_FAILURE   -1
#define MAKEHBFILE_SUCCESS    0
#define PATHLEN              80

void GetConfig( char * );
void LogConfig( void );
int  chdir_ew( char * );
int  mkdir_ew( char * );
int  rename_ew( char *, char * );
#ifdef _WIN32
void sleep( int sec );
#endif

#endif
