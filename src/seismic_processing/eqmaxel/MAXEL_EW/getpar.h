#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "basic.h"

#ifndef GETPAR 
#define GETPAR

#define TRUE             1
#define FALSE            0
#define WARN            -1

int Is_digit      (char str);
int Is_char       (char str);
int Is_blank      (char str);
int Where_char    (char *str,char ch);
void Tail_cut     (char *str);
int Skip_blank    (char *str);
int Save_para     (char *str);
int Read_para_file(char *sname);

int Get_par_char  (char *str,char *val);
int Get_par_int   (char *str,int *val);
int Get_par_float (char *str,float *val);
int Get_par_double(char *str,double *val);
int Get_par_str   (char *str,char**val);

int Get_par_nchar  (char *str,char *val,int pos);
int Get_par_nint   (char *str,int *val,int pos);
int Get_par_nfloat (char *str,float *val,int pos);
int Get_par_ndouble(char *str,double *val,int pos);
int Get_par_nstr   (char *str,char**val,int pos);

int Get_par_vchar  (char *str,char **val);
int Get_par_vint   (char *str,int **val);
int Get_par_vfloat (char *str,float **val);
int Get_par_vdouble(char *str,double **val);
int Get_par_vstr   (char *str,char***val);

void Check_para(void);
void Free_para(void);
#endif
