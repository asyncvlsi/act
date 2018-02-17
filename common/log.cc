/*************************************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *************************************************************************/
#include "log.h"
#include "sim.h"

FILE *Log::fp;
int Log::newline = 1;
Log *Log::last_log = NULL;	// last log entry written out
Time_t Log::last_log_time = 0; // time of last log message

int Log::log_level[256];	// initialized by config file

#if defined(ASYNCHRONOUS)
#define log_prefix_changed   ((last_log != this || last_log_time != CurrentTime()))
#elif defined(SYNCHRONOUS)
#define log_prefix_changed   ((last_log != this || last_log_time != get_time()))
#endif

#ifdef SYNCHRONOUS
volatile unsigned int logTimer;
#endif

/*------------------------------------------------------------------------
 *
 *   Log::Log -- 
 *
 *     Constructor. Initialize information about the log.
 *
 *------------------------------------------------------------------------
 */
#if defined(__OMIT_DEFAULT_PARAMS__)
Log::Log(char level)
#else
Log::Log(char level, int num)
#endif
{
  levtype = level;
  lev = num;
  hex_int = 0;
#ifdef SYNCHRONOUS
  startLogging = logTimer;
#endif
}


/*------------------------------------------------------------------------
 *   
 *   Log::OpenLog --
 *
 *     Open log file named `name' for writing, store the output pointer
 *     in Log::cout.
 *
 *------------------------------------------------------------------------
 */
void Log::OpenLog (const char *name)
{
  Log::fp = fopen (name, "w");
  if (!Log::fp) {
    fprintf (stderr, "WARNING: log file `%s'can't be opened\n", name);
  }
}

/*------------------------------------------------------------------------
 *
 *   Log::CloseLog --
 *
 *     Close log file 
 *
 *------------------------------------------------------------------------
 */
void Log::CloseLog (void)
{
  if(Log::fp) fclose (Log::fp);
}


/*------------------------------------------------------------------------
 *
 *   Log::Initialize_LogLevel --
 *
 *     Setup log level array.
 *
 *------------------------------------------------------------------------
 */
void Log::Initialize_LogLevel (const char *s)
{
  int i;

  for (i=0; i < 256; i++)
    log_level[i] = 0;
  if (!s) {
    return;
  }
  while (*s) {
    log_level[(int)*s]++;
    s++;
  }
  if (log_level['*']) {
    for (i=0; i < 256; i++)
      log_level[i]++;
  }
}


/*------------------------------------------------------------------------
 *
 *  Prefix --
 *
 *    Dump my id prefix to the output
 *
 *------------------------------------------------------------------------
 */
void Log::Prefix (void)
{
  if (!newline) {
    if (log_prefix_changed)
      fputc ('\n', Log::fp);
    else
      return;
  }
#if defined (ASYNCHRONOUS)
#if defined(__FreeBSD__)
  fprintf (Log::fp, "%qu <%s> ", CurrentTime (), thread_name ());
#else
  fprintf (Log::fp, "%lu <%s> ", (unsigned long)CurrentTime (), thread_name ());
#endif
#elif defined (SYNCHRONOUS)
#if defined(__FreeBSD__)
  fprintf (Log::fp, "%qu %d:<%s> ", get_time ()/2,
	   (int)(get_time()&1), get_tname ());
#else
  fprintf (Log::fp, "%lu %d:<%s> ", (unsigned long)get_time ()/2,
	   (int)(get_time()&1), get_tname ());
#endif
#endif
}

#define no_logging(tipe,num) \
                ((tipe != '*' && (Log::log_level[tipe] < num)) || !Log::fp)


void Log::NormalUpdate (void)
{
  last_log = this;
#if defined(ASYNCHRONOUS)
  last_log_time = CurrentTime();
#elif defined (SYNCHRONOUS)
  last_log_time = get_time ();
#endif
  newline = 0;
}


Log &Log::operator <<(LogHexInt l)
{
  hex_int = 1;
  return *this;
}

Log &Log::operator <<(LogDecInt l)
{
  hex_int = 0;
  return *this;
}

/*------------------------------------------------------------------------
 *
 *  Operators <<: 
 *
 *    Rudimentary printing, with nl's filtered out.
 *
 *------------------------------------------------------------------------
 */
Log &Log::operator <<(const char *s)
{
  if (no_logging (levtype,lev)) return *this;
  context_disable ();

  Log::Prefix ();
  NormalUpdate ();
  while (*s) {
    fputc (*s++, Log::fp);
    if (*s == '\n') {
      if (*(s+1))
	Prefix();
      else {
	last_log = NULL;
	newline = 1;
      }
    }
  }
  fflush (Log::fp);
  context_enable ();
  return *this;
}

#define SPEW_INTOPERATOR(type,fmt,xfmt) Log &Log::operator <<(type x)         	\
			    {                                    	\
			      if (no_logging(levtype,lev)) return *this;\
			      context_disable ();                	\
			                                         	\
			      Log::Prefix (); \
                              if (hex_int) fprintf (Log::fp,xfmt,x); \
			      else fprintf (Log::fp, fmt, x);		\
			      NormalUpdate ();                   	\
  			      fflush (Log::fp);				\
			      context_enable ();                 	\
			      return *this;                      	\
			    }

SPEW_INTOPERATOR(int,"%d","%x")                                    
SPEW_INTOPERATOR(unsigned int,"%u","%x")                                    
#if defined(__FreeBSD__)
SPEW_INTOPERATOR(long long unsigned int, "%qu","%qx")
#else
SPEW_INTOPERATOR(long long unsigned int, "%llu","%llx")
#endif
SPEW_INTOPERATOR(long unsigned int, "%lu","%x")
SPEW_INTOPERATOR(long, "%ld","%x")

#include <stdarg.h>

Log &Log::form (const char *format, ...)
{
  va_list ap;
  if (no_logging (levtype,lev)) return *this;
  context_disable ();

  Log::Prefix ();

  va_start (ap, format);
  vfprintf (Log::fp, format, ap);
  NormalUpdate ();
  fflush (Log::fp);
  context_enable ();
  return *this;
}

Log &Log::printf (const char *format, ...)
{
  va_list ap;
  if (no_logging (levtype,lev)) return *this;
  context_disable ();

  Log::Prefix ();

  va_start (ap, format);
  vfprintf (Log::fp, format, ap);
  NormalUpdate ();
  fflush (Log::fp);
  context_enable ();
  return *this;
}

Log &Log::print (const char *format, ...)
{
  va_list ap;
  if (no_logging (levtype,lev)) return *this;
#ifdef SYNCHRONOUS
  if ((unsigned int)(get_time()/2) < startLogging) return *this;
#endif

  context_disable ();

  Log::Prefix ();

  va_start (ap, format);
  vfprintf (Log::fp, format, ap);
  fputc ('\n', Log::fp);
  fflush (Log::fp);
  last_log = NULL;
  newline = 1;
  context_enable ();
  return *this;
}

Log &Log::myprint (const char *format, ...)
{
  va_list ap;
  if (no_logging ('$',1)) return *this;
#ifdef SYNCHRONOUS
  if ((unsigned int)(get_time()/2) < startLogging) return *this;
#endif

  context_disable ();

  Log::Prefix ();

  va_start (ap, format);
  vfprintf (Log::fp, format, ap);
  fputc ('\n', Log::fp);
  fflush (Log::fp);
  last_log = NULL;
  newline = 1;
  context_enable ();
  return *this;
}

Log &Log::printfll (unsigned long long v)
{
  char buf[64], c;
  int i, j;

  if (no_logging(levtype,lev)) return *this;
  context_disable ();
  Log::Prefix ();

  i = 0;
  do {
    buf[i] = '0' + (v%10);
    i++;
  } while (v /= 10);
  buf[i] = '\0';
  i--;
  j = 0;
  
  // reverse string
  while (j < i) {
    c = buf[i];
    buf[i] = buf[j];
    buf[j] = c;
    i--;
    j++;
  }
  fputs (buf, Log::fp);
  NormalUpdate ();
  fflush (Log::fp);
  context_enable ();
  return *this;
}


Log &Log::printfllx (unsigned long long v)
{
  if (no_logging(levtype,lev)) return *this;
  context_disable ();
  Log::Prefix ();

  if ((v >> 32) & 0xffffffffLL) {
    fprintf (Log::fp, "%x%08x", (unsigned long)((v>>32)&0xffffffffLL),
	      (unsigned long)(v & 0xffffffffLL));
  }
  else {
    fprintf (Log::fp, "%x", (unsigned long)(v & 0xffffffffLL));
  }

  NormalUpdate ();
  fflush (Log::fp);
  context_enable ();
  return *this;
}

LogHexInt LogHEX;
LogDecInt LogDEC;
