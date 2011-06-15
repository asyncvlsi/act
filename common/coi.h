/*                                  */
/*    Copyright 1998, 1999, 2000    */
/*    NASSDA Corporation            */
/*                                  */

#ifndef COI_H
#define COI_H
/* #define WDFTIME */

/* Structures and Enumerations **********************************************/
enum HSSignalType {
  HSVOLTAGE = 0,
  HSCURRENT,
  HSLOGIC,
  HSPOWER,
  HSDEFAULT
};



/***** DON'T CHANGE the following HsimParam data structure. Use it as it is *****/

typedef struct HsimParam
{
double   StopTime;           /* stopTime as in the .TRAN, .DC, or .AC           */
double   TimeScale;          /* time scale factor                               */
double   VoltageScale;       /* voltage scale factor                            */
double   CurrentScale;       /* current scale factor                            */
double   Temperature;        /* temperature as in .TEMP                         */

int      NumOfSweep;         /* total number of sweep step in .TRAN ... SWEEP  */
int      NumOfTemperature;   /* total number of temperatures in .TEMP           */
int      NumOfAlter;         /* total number of .ALTER                          */

int      CurrOfSweep;        /* current number of SWEEP                         */
int      CurrOfTemperature;  /* current number of Temperature                   */
int      CurrOfAlter;        /* current number of .ALTER                        */

int      CurrOfIteration;    /* current number of iteration generated           */

double   Vres;               /* voltage resolution */
double   Ires;               /* current resolution */

  
/************************   USED BY HSIM ONLY ***********************************/
char     *HierId;            /* used by HSIM only */
double   TopSupply;          /* used by HSIM only */
double   OutFileSplit;       /* used by HSIM only */
double   Cfg_Banner;         /* used by HSIM only */
double   Cfg_Xout;           /* used by HSIM only */
double   Cfg_OutputFsdbSize; /* used by HSIM only*/
double   Cfg_OutputWdfSize;  /* used by HSIM only */
#ifdef WDFTIME
double   Cfg_OutputWdfTime;  /* used by HSIM only */
#endif
double   Cfg_OutputWdfCompress; /* used by HSIM only*/
/********************************************************************************/

double   StartTime;           /* startTime as in the .DC or .AC sweep              */
char*    SweepVarName;        /* Sweep Varialbe Name as in the .DC or .AC sweep    */
char*    SweepType;           /* For .DC and .AC sweep type, LIN, DEC, OCT, OTHERS */
int      NumOfMonteCarlo;     /* total number of Monte Carlo                        */
int      CurrOfMonteCarlo;    /* current number of Monte Carlo                      */
char*    SweepVarName2;       /* external Sweep Variable name as in .DC or .AC sweep */

int      Fsdbdouble;          /* 0 (default) = output fsdb in float, 1 = output in double */
int      Wdfdouble;           /* 0 (default) = output wdf in float, 1 = output in double */
} HsimParam;


typedef enum COI_HSIMPARM_TYPE {
      COI_HSIMPARAM_STOPTIME = 0,           /*  0 */
      COI_HSIMPARAM_TIMESCALE,              /*  1 */
      COI_HSIMPARAM_VOLTAGESCALE,           /*  2 */
      COI_HSIMPARAM_CURRENTSCALE,           /*  3 */
      COI_HSIMPARAM_TEMPERATURE,            /*  4 */
      COI_HSIMPARAM_NUMOFSWEEP,             /*  5 */
      COI_HSIMPARAM_NUMOFTEMPERATURE,       /*  6 */
      COI_HSIMPARAM_NUMOFALTER,             /*  7 */
      COI_HSIMPARAM_CURROFSWEEP,            /*  8 */
      COI_HSIMPARAM_CURROFTEMPERATURE,      /*  9 */
      COI_HSIMPARAM_CURROFALTER,            /* 10 */
      COI_HSIMPARAM_CURROFITERATION,        /* 11 */
      COI_HSIMPARAM_VRES,                   /* 12 */
      COI_HSIMPARAM_IRES,                   /* 13 */
      COI_HSIMPARAM_HIERID,                 /* 14 */
      COI_HSIMPARAM_TOPSUPPLY,              /* 15 */
      COI_HSIMPARAM_OUTFILESPLIT,           /* 16 */
      COI_HSIMPARAM_CFG_BANNER,             /* 17 */
      COI_HSIMPARAM_CFG_XOUT,               /* 18 */
      COI_HSIMPARAM_CFG_OUTPUTFSDBSIZE,     /* 19 */
      COI_HSIMPARAM_CFG_OUTPUTWDFSIZE,      /* 20 */
      COI_HSIMPARAM_CFG_OUTPUTWDFCOMPRESS,  /* 21 */
      COI_HSIMPARAM_STARTTIME,              /* 22 */
      COI_HSIMPARAM_SWEEPVARNAME,           /* 23 */
      COI_HSIMPARAM_SWEEPTYPE,              /* 24 */
      COI_HSIMPARAM_NUMOFMONTECARLO,        /* 25 */
      COI_HSIMPARAM_CURROFMONTECARLO,       /* 26 */
      COI_HSIMPARAM_SWEEPVARNAME2,          /* 27 */
      COI_HSIMPARAM_FSDBDOUBLE,             /* 28 */
      COI_HSIMPARAM_WDFDOUBLE,              /* 29 */
      COI_HSIMPARAM_RESTARTNUM,             /* 30 */
#ifdef WDFTIME
      COI_HSIMPARAM_CFG_OUTPUTWDFTIME,      /* 31 */
#endif
      
  COI_HSIMPARAM_END_IDX
} COI_HSIMPARM_TYPE;

typedef union COI_HSIMParamVal {
  int    i;
  long   l;
  double d;
  char  *p;
} COI_HSIMParamVal;

typedef struct COI_HSIMParam {
  COI_HSIMPARM_TYPE   type;
  COI_HSIMParamVal   val;
} COI_HSIMParam;



#endif

