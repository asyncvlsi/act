/*************************************************************************
 *
 *  Copyright (c) 2004 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */


/*
 *  Nanosim interface for alint
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "NSOutputInt.h"
#include "coi.h"
#include "atrace.h"

#define Assert(a,b) do { if (!(a)) { fprintf (stderr, "Assertion failed, file %s, line %d\n", __FILE__, __LINE__); fprintf (stderr, "Assertion: " #a "\n"); fprintf (stderr, "ERR: " b "\n"); exit (4); } } while (0)


typedef struct alint_struct {
  atrace *a;
  double Tscale;
  double Vscale;
  double Stoptime;
} alint;

static alint *A;

static void read_namesfile (alint *a);

/*
  Utility functions: name conversion.

  1. Remove v( ) for signal
  2. If trailing #, return NULL

*/
static char *name_convert (char *path, char *signal)
{
  char *s;
  int l;
  int pos;

  if (strcmp (path, "top/spice") == 0 || (strcmp (path, "top.spice") == 0)) {
	path = "";
  }
  
  l = strlen (signal);
  pos = l-1;

  if ((l > 3) && (signal[0] == 'v') &&
      (signal[1] == '(') && (signal[l-1] == ')')) {
    pos = l - 2;
  }

  if (signal[pos] == '#') 
    return NULL;

  s = (char *)malloc (strlen (path) + l + 2);
  if (!s) {
    return NULL;
  }

  if (pos == l-1) {
    if (path[0] != '\0')
       sprintf (s, "%s.%s", path, signal);
    else
       sprintf (s, "%s", signal);
    l = strlen (s);
  }
  else {
    if (path[0] != '\0')
       sprintf (s, "%s.%s", path, signal+2);
    else
       sprintf (s, "%s", signal+2);

    l = strlen(s);
    s[l-1] = '\0';
    l--;
  }
  for (pos = 0; pos < l; pos++) 
    if (s[pos] == '/')
      s[pos] = '.';
  if (strcmp (s, "vdd") == 0) {
    strcpy (s, "Vdd");
  }
  if (strcmp (s, "gnd") == 0) {
    strcpy (s, "GND");
  }
  return s;
}



/*
  Global comment: Return 0 if success, -1 if error 
*/


/*=============================================================================
 *
 *   Namespace management
 *
 *=============================================================================
 */

/*------------------------------------------------------------------------
 *
 * Nanosim: start creating signal tree
 *
 *------------------------------------------------------------------------
 */
int alint_BeginAddSignal (void *file)
{
  alint *a = (alint *)file;
  Assert (a == A, "Multiple open simulations not supported");

  /* nothing to be done here */

  return 0;
}

/*------------------------------------------------------------------------
 *
 * Hsim: start creating waves
 *
 *------------------------------------------------------------------------
 */
int ALINT_BeginCreateWave (void *file)
{
  return alint_BeginAddSignal (file);
}

/*------------------------------------------------------------------------
 *
 * Nanosim: End creating signal tree.
 *
 *------------------------------------------------------------------------
 */
int alint_EndAddSignal (void *file)
{
  alint *a = (alint *)file;
  Assert (a == A, "Multiple open simulations not supported");
  
  /* nothing to be done here either */

  return 0;
}

/*------------------------------------------------------------------------
 *
 * Hsim: stop creating waves
 *
 *------------------------------------------------------------------------
 */
int ALINT_EndCreateWave (void *file)
{
  return alint_EndAddSignal (file);
}

/*------------------------------------------------------------------------
 *
 * Nanosim: 
 *    Add a signal; type = NSVOLTAGE,  NSMATH,  NSCURRENT, NSLOGIC,
 *    NSBUS
 *
 *------------------------------------------------------------------------
 */
void *alint_AddSignal (void *file, char *path, char *signal, int width, int type)
{
  alint *a = (alint *)file;
  name_t *n;
  char *s;

  Assert (a == A, "Multiple open simulations not supported");

  Assert  (type == NSVOLTAGE, "Only voltage mode traces currently supported");

  s = name_convert (path, signal);

  if (!s) return NULL;

  n = atrace_create_node (A->a, s);

  free (s);

  return (void *) n;
}

/*------------------------------------------------------------------------
 *
 * Hsim:
 *   Add a signal
 *
 *------------------------------------------------------------------------
 */
void *ALINT_CreateWave (void *file, char *path, char *signal, char sep, enum HSSignalType type)
{
  if (type != HSVOLTAGE) {
	fprintf (stderr, "WARNING: non-voltage mode trace for %s ignored\n", signal);
	return NULL;
  }
  Assert (type == HSVOLTAGE, "Only voltage mode traces currently supported");

  return alint_AddSignal (file, path, signal, 1, NSVOLTAGE);
}

/*------------------------------------------------------------------------
 * Hsim: create header---not used
 *------------------------------------------------------------------------
 */
void *ALINT_CreateHeader (void *file)
{
  return 0;
}


/*------------------------------------------------------------------------
 *
 * Nanosim: Create an alias for existing signal "sh"
 *
 *------------------------------------------------------------------------
 */
int alint_AddAliasSignal (void *sh, char *path, char *signal, int type)
{
  name_t *n  = (name_t *)sh;
  name_t *m;
  char *s;

  if (!n) return 0;

  Assert (type == NSVOLTAGE, "Only voltage mode traces currently supported");

  s = name_convert (path, signal);
  if (!s) return 0;

  m = atrace_create_node (A->a, s);
  free (s);
  atrace_alias (A->a, n, m);
  
  return 0;
}

/*------------------------------------------------------------------------
 *
 * Hsim: Create an alias for existing signal "sh"
 *
 *------------------------------------------------------------------------
 */
void *ALINT_CreateDuplWave (void *file, char *path, char *signal, char sep,
			  enum HSSignalType type, void *masterwave)
{
  char *s;
  name_t *m;

  if (type != HSVOLTAGE) { return NULL; }

  Assert (type == HSVOLTAGE, "Only voltage mode traces supported");

  alint_AddAliasSignal (masterwave, path, signal, NSVOLTAGE);
  s = name_convert (path, signal);
  m = atrace_lookup (A->a, s);
  free (s);
  return m;
}


/*------------------------------------------------------------------------
 *
 * Nanosim: 
 *   At time  t * time_resolution, "signal" gets value "v" *
 *   voltage_resolution
 *
 *------------------------------------------------------------------------
 */
int alint_AddAnalogSignalChange (void *file, void *signal, double t, double v)
{
  alint *a = (alint *)file;
  name_t *n = (name_t *)signal;
  Assert (a == A, "Multiple open simulations not supported");
  if (!n) return 0;

  read_namesfile (a);

  atrace_signal_change (A->a, n, t * A->Tscale, v * A->Vscale);

  return 0;
}


/*------------------------------------------------------------------------
 *
 *  Hsim: add signal change
 *
 *------------------------------------------------------------------------
 */
int ALINT_AddNextAnalogValueChange (void *file, void *signal, double t, double v)
{
  if (signal == NULL) return 0;
  return alint_AddAnalogSignalChange (file, signal, t, v);
}

/*------------------------------------------------------------------------
 *
 * Nanosim: At time  t * time_resolution, "signal" gets value "v"
 *
 *------------------------------------------------------------------------
 */
int alint_AddDigitalSignalChange (void *file, void *signal, double t, int v)
{
  alint *a = (alint *)file;
  name_t *n = (name_t *)signal;
  Assert (a == A, "Multiple open simulations not supported");
  if (!n) return 0;

  /* no digital signals here! */

  return -1;
}


/*------------------------------------------------------------------------
 *
 *  Hsim: add digital signal change
 *
 *------------------------------------------------------------------------
 */
int ALINT_AddNextDigitalValueChange (void *file, void *signal, double t, int v)
{
  if (signal == NULL) return 0;
  return alint_AddDigitalSignalChange (file, signal, t*1e-12, v);
}


#define SZ 10240
static void read_namesfile (alint *a)
{
  char *s;
  FILE *fp;
  char buf[SZ], buf1[SZ], buf2[SZ];
  static int names_read = 0;
  int of1, of2;

  if (names_read) return;
  names_read = 1;
  
  /* read alias file if present */
  s = (char *)malloc (sizeof (char)*(strlen (a->a->file) + 9));
  sprintf (s, "%s.aliases", a->a->file);
  if (fp = fopen (s, "r")) {
    name_t *n1, *n2;
    /* alias file exists */
    int line = 1;
    buf[SZ-1] = '\0';
    while (fgets (buf, SZ, fp)) {
      if (buf[SZ-1] != '\0') {
	fprintf (stderr, "Sorry, line %d too long in alias file!\n", line);
	exit (5);
      }
      buf1[0] = '\0';
      buf2[0] = '\0';
      if (sscanf (buf, "connect %s %s", buf1, buf2) != 2) {
	fprintf (stderr, "Format error in alias file, line %d\n", line);
	exit (5);
      }
      if (strncmp (buf1, "top.spice.", 10) == 0) {
	of1 = 10;
      }
      else {
	of1 = 0;
      }
      if (strncmp (buf2, "top.spice.", 10) == 0) {
	of2 = 10;
      }
      else {
	of2 = 0;
      }
      n1 = atrace_lookup (a->a, buf1+of1);
      n2 = atrace_lookup (a->a, buf2+of2);
      if (n1 || n2) {
	/* one of them exists */
	n1 = atrace_create_node (a->a, buf1+of1);
	n2 = atrace_create_node (a->a, buf2+of2);
        atrace_alias (a->a, n1, n2);
      }
      line++;
    }
    fclose (fp);
  }
  free (s);
}



/*=============================================================================
 * 
 *   I/O functions
 *
 *    open, close, flush, buffer management
 *
 *=============================================================================
 */


/*------------------------------------------------------------------------
 *
 * Nanosim function to open a new trace file
 *
 *------------------------------------------------------------------------
 */
void *alint_OpenOutputFile (char *file, SimParam *p)
{
  alint *a;
  int l;

  a = (alint *)malloc (sizeof(alint));
  if (!a) return NULL;

  if (p->StopTime != (int)p->StopTime) {
	p->StopTime = (int) (1 + p->StopTime);
  }

  l = strlen (file);
  if (l > 6 && strcmp (file+l-6, ".alint") == 0) {
	file[l-6] = '\0';
  }

#if 0
  a->a = atrace_create (file, ATRACE_TIME_ORDER, p->StopTime*p->TimeScale, p->TimeScale);
#endif
  a->a = atrace_create (file, ATRACE_DELTA, p->StopTime*p->TimeScale, p->TimeScale);
  a->Tscale = p->TimeScale;
  a->Vscale = p->VoltageScale;
  a->Stoptime = p->StopTime;

  A = a;
  
  return (void *)a;
}

/*------------------------------------------------------------------------
 *
 * Hsim function for opening a new file
 *
 *------------------------------------------------------------------------
 */
void *ALINT_CreateWaveFile (char *basename, struct HsimParam *hsimparam)
{
  SimParam p;

  p.StopTime = hsimparam->StopTime*1e12;
  p.TimeScale = hsimparam->TimeScale*1e-12;
  p.VoltageScale = hsimparam->VoltageScale;

  return alint_OpenOutputFile (basename, &p);
}

/*------------------------------------------------------------------------
 *
 * Nanosim function for closing a trace file 
 *
 *------------------------------------------------------------------------
 */
int alint_CloseOutputFile (void *file)
{
  alint *a = (alint *)file;
  Assert (a == A, "Multiple open simulations not supported");

  atrace_close (a->a);
  a->a = NULL;
  
  return 0;
}

/*------------------------------------------------------------------------
 *
 * Hsim function for closing a trace file 
 *
 *------------------------------------------------------------------------
 */
int ALINT_CloseWaveFile (void *file)
{
  return alint_CloseOutputFile (file);
}


/*
  Nanosim function: memory buffer size := default * scale_factor
*/
int alint_BufferScaleFactor (void *file, double scale)
{
  alint *a = (alint *)file;
  Assert (a == A, "Multiple open simulations not supported");

  return 0;
}  


/*
  Nanosim function
*/
int alint_SyncOutputFile (void *file)
{
  alint *a = (alint *)file;
  Assert (a == A, "Multiple open simulations not supported");

  atrace_flush (a->a);

  return 0;
}


/*
  Nanosim Save/Restore functions. Not used.
*/
void alint_Restore (void)
{
  return;
}


void alint_Save (void)
{
  return;
}

