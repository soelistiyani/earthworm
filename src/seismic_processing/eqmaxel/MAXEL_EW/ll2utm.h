//LatLong- UTM conversion..h
//definitions for lat/long to UTM and UTM to lat/lng conversions
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LATLONGCONV
#define LATLONGCONV

void LLtoUTM(const double Lat, const double Long, 
			 double *UTMNorthing, double *UTMEasting, char* UTMZone, int myZone);
void UTMtoLL(const double UTMNorthing, const double UTMEasting, const char* UTMZone,
			  double *Lat,  double *Long, int myZone);
char UTMLetterDesignator(double Lat);

void set_origin_coord(double Long, double Lat);
void calc_deg2km(double Long, double Lat, double *dx, double *dy);
void calc_km2deg(double dx, double dy, double *Long, double *Lat);
#endif
