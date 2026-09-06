
/* MSfile is the filename of the input miniSEED to be converted
   tankfile is the filename of the output tank player file
   max_samps is the maximum number of samples to pack into a tracebuf packet
   t2h is the header of the first packet of data converted
   multipier is applied if the data are floating point values and need converting to integer
 */
int convert_mseed_to_tank(char * MSfile, char * tankfile, int max_samps, TRACE2X_HEADER *t2h, int multiplier);

