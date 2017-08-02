/*------------------------------------------------------------------------
 *
 *   (c) 1997 Rajit Manohar
 *
 *   Timer functions for both C and Fortran.
 *
 *     cputime_msec() : returns CPU time (user+system) in milliseconds since
 *                      the last time it was called.
 *
 *     realtime_msec(): returns real time in milliseconds since the last time
 *                      it was called.
 *
 *
 *   RETURN value is of type RETURN_TYPE (default: double)
 *
 *   If your Fortran compiler likes trailing underscores after the names,
 *   compile with -DF77_USE_UNDERSCORES
 *
 *------------------------------------------------------------------------
 */

#include <stdio.h>

/*------------------------------------------------------------------------
   System-dependent CPU time
   Returns CPU time since last measurement in milliseconds.
------------------------------------------------------------------------*/
#include <sys/time.h>
#include <sys/resource.h>
#include "mytime.h"

/* set default return type */

#define RETURN_TYPE TIME_RETURN_TYPE

#ifdef F77_USE_UNDERSCORES
RETURN_TYPE cputime_msec_ ()
#else
RETURN_TYPE cputime_msec ()
#endif
{
  static int first_time = 1;
  static struct rusage last_measurement;
  struct rusage current_measurement;
  RETURN_TYPE tm;

  getrusage (RUSAGE_SELF, &current_measurement);
  
  if (first_time) {
    last_measurement.ru_utime.tv_sec = 0.0;
    last_measurement.ru_utime.tv_usec = 0.0;
    last_measurement.ru_stime.tv_sec = 0.0;
    last_measurement.ru_stime.tv_usec = 0.0;
    first_time = 0;
  }
  
  tm = (current_measurement.ru_utime.tv_sec
	-last_measurement.ru_utime.tv_sec)*1000.0;
  tm += (current_measurement.ru_stime.tv_sec
	-last_measurement.ru_stime.tv_sec)*1000.0;

  tm += (current_measurement.ru_utime.tv_usec
	-last_measurement.ru_utime.tv_usec)*0.001;
  tm += (current_measurement.ru_stime.tv_usec
	-last_measurement.ru_stime.tv_usec)*0.001;

  getrusage (RUSAGE_SELF, &last_measurement);
  
  return tm;
}


#ifdef F77_USE_UNDERSCORES
RETURN_TYPE realtime_msec_ ()
#else
RETURN_TYPE realtime_msec ()
#endif
{
  static int first_time = 1;
  static struct timeval last_measurement;
  struct timeval current_measurement;
  RETURN_TYPE tm;

  gettimeofday (&current_measurement, NULL);
  
  if (first_time) {
    last_measurement.tv_sec = 0.0;
    last_measurement.tv_usec = 0.0;
    first_time = 0;
  }
  
  tm = (current_measurement.tv_sec-last_measurement.tv_sec)*1000.0;
  tm += (current_measurement.tv_usec-last_measurement.tv_usec)*0.001;

  gettimeofday (&last_measurement, NULL);
  
  return tm;
}
