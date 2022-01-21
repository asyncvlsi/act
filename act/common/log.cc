/*************************************************************************
 *
 *  Simulation log file creation/management
 *
 *  Copyright (c) 1999, 2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include "log.h"
#include "sim.h"
#include "thread.h"

FILE *Log::fp = NULL;
int Log::newline = 1;
Log *Log::last_log = NULL;	// last log entry written out
Time_t Log::last_log_time = 0; // time of last log message

int Log::log_level[256];	// initialized by config file

#define log_prefix_changed   ((last_log != this || (ltype == 1 && last_log_time != Sim::Now())))

/*------------------------------------------------------------------------
 *
 *   Log::Log -- 
 *
 *     Constructor. Initialize information about the log.
 *
 *------------------------------------------------------------------------
 */
Log::Log(Sim *s, char level, int num)
{
  levtype = level;
  lev = num;
  hex_int = 0;
  ltype = 1;
  u.proc = s;
}


Log::Log(const char *s, char level, int num)
{
  levtype = level;
  lev = num;
  hex_int = 0;
  ltype = 0;
  u.name = Strdup (s);
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

void Log::OpenStderr ()
{
  Log::fp = stderr;
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
  UpdateLogLevel (s);
}

void Log::UpdateLogLevel (const char *s)
{
  int i;
  
  if (!s) return;
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
  if (ltype == 1) {
    fprintf (Log::fp, "%llu <%s> ", Sim::Now(), u.proc->Name());
  }
  else {
    fprintf (Log::fp, "<%s> ", u.name);
  }    
}

#define no_logging(tipe,num) \
                ((tipe != '*' && (Log::log_level[tipe] < num)) || !Log::fp)

void Log::NormalUpdate (void)
{
  last_log = this;
  if (ltype == 1) {
    last_log_time = Sim::Now();
  }
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
    fputc (*s, Log::fp);
    if (*s == '\n') {
      if (*(s+1))
	Prefix();
      else {
	last_log = NULL;
	newline = 1;
      }
    }
    s++;
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
SPEW_INTOPERATOR(long long unsigned int, "%llu","%llx")
SPEW_INTOPERATOR(long unsigned int, "%lu","%lx")
SPEW_INTOPERATOR(long, "%ld","%lx")

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
    fprintf (Log::fp, "%lx%08lx", (unsigned long)((v>>32)&0xffffffffLL),
	      (unsigned long)(v & 0xffffffffLL));
  }
  else {
    fprintf (Log::fp, "%lx", (unsigned long)(v & 0xffffffffLL));
  }

  NormalUpdate ();
  fflush (Log::fp);
  context_enable ();
  return *this;
}

LogHexInt LogHEX;
LogDecInt LogDEC;
