/*-*-mode:c++-*-**********************************************************
 *
 *  Copyright (c) 1999 Rajit Manohar
 *
 *  $Id: log.h,v 1.15 2005/08/29 16:06:47 fang Exp $
 *
 *************************************************************************/
#ifndef __LOG_H__
#define __LOG_H__

/*
 *  Routines for logging information about a simulation.
 *
 * Every type of log message should have one of these Log() objects
 * associated with it. The level in the constructor is the log
 * level. The particular messages that are actually logged are
 * specified in the configuration file. By default, all messages are
 * logged (level = '*').
 */
#include <stdio.h>
#include <iostream>
#include "mytime.h"

using std::ostream;

class LogHexInt { };
class LogDecInt { };

extern LogHexInt LogHEX;
extern LogDecInt LogDEC;
#ifdef SYNCHRONOUS
extern volatile unsigned int logTimer;
#endif

class Log {
 public:
  Log(char level = '*', int num = 1); 
  // Use "level" to filter log messages by type. The level is
  // checked against variable LogLevel in the configuration file.
				
  static void OpenLog (const char *name); // Call once in main() to open "name"
				    // as the log file name.

  static void CloseLog (void);	// Call once in the cleanup() routine
				// to close the file

  Log &operator <<(int x);	// Write various types of things to the log.
  Log &operator <<(const char *s); // This last crazy thing is to make endl work.
  Log &operator <<(long long unsigned int);
  Log &operator <<(long unsigned int);
  Log &operator <<(unsigned int);
  Log &operator <<(long);

  Log &operator <<(ostream& fn(ostream&));

  Log &operator <<(LogHexInt);
  Log &operator <<(LogDecInt);

  Log &form (const char *format, ...); // NO NEWLINES!!!!
  Log &printf (const char *format, ...); // NO NEWLINES!!!!

  Log &print (const char *format, ...); // newline inserted automatically
  Log &myprint (const char *format, ...); // newline inserted automatically

  Log &printfll (unsigned long long v);		// decimal printf
  Log &printfllx (unsigned long long v);	// hex printf

  static void Initialize_LogLevel (const char *s); 
				// Sets the log_level vector to the right
				// thing to handle appropriate filtering of 
				// log messages. Should be called by the
				// routine reading the configuration file.

  static FILE *fp;

  ~Log() { }

  static int log_level[256];	// Initialized by configuration file

#ifdef SYNCHRONOUS
  unsigned int startLogging;
#endif

 private:
  // fields that are initialized once
  // static ofstream file;

  // for proper [id] output
  static Log *last_log;
  static int newline;
  static Time_t last_log_time;

  void NormalUpdate (void);

  char levtype;
  int  lev;

  int hex_int;
  
  void Prefix (void);

};

#endif /* __LOG_H__ */
