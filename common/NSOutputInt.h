/*
 * Copyright (c) 2001 Synopsys Inc.
 */

/*
 * A header file for NanoSim Generic Output API functions
 */

/*
 * This header will be included by the wrapper program so the wrapper
 * can access and compose the data used by NanoSim
 */

/* common structure */

#define   NSVOLTAGE 0
#define   NSMATH    1
#define   NSCURRENT 2
#define   NSLOGIC   3
#define   NSBUS     4 
#define   NSDIDT    5
#define   NSIRMS    6
#define   NSIAVG    7

#define CUSTOM_NANOSIM_CMOS 382
#define CUSTOM_NANOSIM_BJT  317
#define CUSTOM_NANOSIM_JFET 410

#define NS_BINARY_OUTPUT(s)   (s->binary)

enum SignalType {
   VOLTAGE = 0,
   MATH,
   CURRENT,
   LOGIC,
   BUS
};

typedef struct SimParam
{
double   StopTime;           /* stopTime as in the .TRAN                        */
double   TimeScale;          /* time scale factor                               */
double   VoltageScale;       /* voltage scale factor                            */
double   CurrentScale;       /* current scale factor                            */
double   MathScale;          /* math sig. scale factor                          */

void    (*error) (int);     /* call-back for vendor library error              */
int     (*split) (void);    /* call-back for split from vendor                 */
int     (*elem_type) (char *) ; /* query elem type */
char    *out_filter;         /* output filter string                            */
char     delimiter;          /* scope separator                                 */
unsigned int compress:1;     /* compress info. from nanosim; might be overwritten 
                                by customer                                     */
unsigned int time_unit_sec:1; 
unsigned int binary       :1; 
} SimParam;

typedef struct eldo_output_hdl {
   void  *(*CreateWaveFilePtr)(char*,struct SimParam *);
   int    (*BufferScaleFactorPtr)(void*, double);
   int    (*BeginCreateWavePtr)(void*);
   void  *(*CreateWavePtr)    (void*,char*,char*, int, int);
   void   (*Save) (void);
   void   (*Restore) (void);
   int   (*CreateAliasWavePtr)(void*,char*,char*, int);
   int    (*EndCreateWavePtr)(void*);
   int    (*AddNextDigitalValueChangePtr)(void*,void*,double,int);
   int    (*AddNextAnalogValueChangePtr)(void*,void*,double,double, ...);
   int    (*AddNextBusValueChangePtr)(void*,void*,double,char *);
   int    (*CloseWaveFilePtr)(void*);
   int    (*SyncWaveFilePtr) (void *);

   char  *Name;
   char  *lib_name;
   char  *path;
   char  *postfix;

   unsigned int binary   :1;
} eldo_output_hdl;


