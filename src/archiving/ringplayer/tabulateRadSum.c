/*****************************************************************/
/*                                                               */
/*  tabulatePicks.c                                             */
/*                                                               */
/*****************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define MAX_STR	32000
#define TYPE_SIZE	6
#define NCOL		6
#define COLSIZE	128

int	debug = 1;	// Toggle debug messages


/************************************************************************************/
/* Main method                                                                      */
/************************************************************************************/

int main( int argc, char **argv )
{
	char   infile[1024];	 	 // Name of file
	char   line[MAX_STR];    // Line from input file					 
	char	 outstr[MAX_STR];  // Line to write to output 
	// char	 tempstr[MAX_STR]; // Line to write to output file
	char	*begin, *end;	    // Utility character pointers
	// char  *tag;					 // Name of station/channel tag to match

	FILE	*fin;		// Pointer to input  file

	int	 i;		// Utility counter
	int	 len;		// Length of line
	int	 nRec;	// Number of records in file
	int	 column;	
		
// Check command line arguments
	if(argc < 2)
	{
		fprintf(stderr, "Error: Insufficient arguments\n");
		fflush(stderr);
		return -1;
	}

// Get file names
	strcpy(infile,  argv[1]);
	// strcpy(tag, argv[2]);

// Check that file names have been obtained
	if(strlen(infile) == 0)
	{
		fprintf(stderr, "Error: One or more file names not specified.\n");
		fflush(stderr);
		return -1;		
	}

	if(debug)
	{ 
		fprintf(stderr, "infile:  %s\n", infile); 
		fflush(stderr);
	}

	nRec =  0;  // Initialize row counts for each table


// First pass: Open infile and obtain row count

	fin = fopen(infile, "r");

	if(fin != (FILE *) NULL)
	{
		// fin = fopen(infile, "r"); 
		if(debug){ fprintf(stderr, "%s opened.\n", infile); }
	}
	else
	{
		if(debug){ fprintf(stderr, "%s could not be opened.\n", infile); }
		return -1;
	}

	while (1)
	{
		if(fgets(line, MAX_STR, fin) == NULL) { break; }
		++nRec;
	}

	fclose(fin);

// Parse
	fin  = fopen(infile, "r"); 

	for(i=0; i<nRec; ++i)
	{
		if(fgets(line, MAX_STR, fin) == (char *) NULL) { break; }

		strcpy(outstr, "");
		len = strlen(line);

		if(len == 0 || *line == ' ' || *line == '\t' || *line == '\n') { continue; }
	
		begin  = line;
		end    = line;
		column = 1;

		while((end - line) < len)
		{
			while(*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0') 
			{ 
				++end; 
			}
			*end = '\0';

			if(*begin != ' ' && *begin != '\0')
			{		
				// if(debug) { printf("Column[%d] = [%s]\n", column, begin); }
				
				if(column == 2 || column == 3 || column == 6)
				{
					strcat(outstr, begin);
					strcat(outstr, " ");
				}
				++column;
			}

			// while(*end != '\n' && (*end == ' ' || *end == '\t')) 
			// { 
			// 	++end; 
			// }

			begin = end + 1;
			end = begin;

		}

		strcat(outstr, "\n");
		printf(outstr); 
	}

	return 0;
} // end main
/************************************************************************************/

