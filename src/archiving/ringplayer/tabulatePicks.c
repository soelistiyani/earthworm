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
	char   outfile[1024]; 	 // Name of file
	char   line[MAX_STR];    // Line from input file					 
	char	 outstr[MAX_STR];  // Line to write to output file
	char	 tempstr[MAX_STR]; // Line to write to output file
	char	*begin, *end;	    // Utility character pointers

	FILE	*fin;		// Pointer to input  file
	FILE	*fout;	// Pointer to output file

	int	 i;		// Utility counter
	int	 len;		// Length of line
	int	 nRec;	// Number of records in file
	int	 column;	
	int    isPick;
	int	 ignoreDateTime = 1;
		
// Check command line arguments
	if(argc < 3)
	{
		fprintf(stderr, "Error: Insufficient arguments\n");
		fflush(stderr);
		return -1;
	}

// Get file names
	strcpy(infile,  argv[1]);
	strcpy(outfile, argv[2]);

// Check that file names have been obtained
	if(strlen(infile) == 0 || strlen(outfile) == 0)
	{
		fprintf(stderr, "Error: One or more file names not specified.\n");
		fflush(stderr);
		return -1;		
	}

	if(debug)
	{ 
		fprintf(stderr, "infile:  %s\n", infile); 
		fprintf(stderr, "outfile: %s\n", outfile); 
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
	fout = fopen(outfile, "w"); 

	for(i=0; i<nRec; ++i)
	{
		if(fgets(line, MAX_STR, fin) == (char *) NULL) { break; }

		strcpy(outstr, "");
		len = strlen(line);

		if(len == 0 || *line == ' ' || *line == '\t' || *line == '\n') { continue; }
	
		begin  = line;
		end    = line;
		column = 1;
		isPick = 1;

		while((end - line) < len)
		{
			while(*end != ' ' && *end != '\t' && *end != '\n' && *end != '\0') 
			{ 
				++end; 
			}
			*end = '\0';

			// if(debug) { fprintf(fout, "Column = [%d]\n", column); }
			
			if(column == 1)
			{
				if(atoi(begin) != 8) { isPick = 0; } // is a PICK_SCNL record
				// if(strcmp(begin, "8") != 0) { continue; } // not a PICK_SCNL record
			}

			if(column != 4)
			{		
				if(debug)
				{
					// fprintf(fout, "Begin substr = [%s]\n", begin);
					// fprintf(stderr, "\n");
				}
				
				// char substr[128];
				// sprintf(substr, "%20s", begin);
				// strcat(outstr, substr);


				/*
				if(column == 7)			// This is the DateTime
				{
					char *ptr;
						
					ptr = begin;

					while(*ptr != '.')	// Strip down to 1 sec precision
					{ 
						++ptr; 
					}
					*ptr = '\0';

					strcpy(tempstr, begin);
					strcat(tempstr, " ");
					strcat(tempstr, outstr);
					strcpy(outstr, tempstr);
				}
				*/
				if(!(column == 7 && ignoreDateTime == 1))	
				{
					strcat(outstr, begin);
					strcat(outstr, " ");
				}
			}

			begin = end + 1;
			end = begin;
			++column;
		}

		strcat(outstr, "\n");
		if(isPick) { fprintf(fout, outstr); }
	}

	return 0;
} // end main
/************************************************************************************/

