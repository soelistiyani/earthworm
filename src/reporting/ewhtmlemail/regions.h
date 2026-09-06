/* Region structs
 *******************/
 
#define     MAX_SIDES       20
#define     MAX_REGIONS     30   
#define     MAX_NAME_LEN    30
 
typedef struct _region
{
    char    name[MAX_NAME_LEN]; 
    int     num_sides;
    float   x[MAX_SIDES + 1];
    float   y[MAX_SIDES + 1];
} REGION;
  
static          REGION          EmailReg[MAX_REGIONS];
static          int             numRegions;
