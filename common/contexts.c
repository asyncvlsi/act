/*************************************************************************
 *
 *  (c) 1998-1999 Rajit Manohar
 *
 *************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include "contexts.h"

struct process_record {
  context_t c;
};

/* current process */
process_t *current_process = NULL;
static process_t *terminated_process = NULL;

#ifdef FAIR

static struct itimerval mt;	/* the timer for the main thread */
static int unfair = 0;
static int enable_mask;
static int disable_mask;

static int mt_in_cs = 0;	/* main thread in cs */
static int mt_pending = 0;	/* pending thread 0 */
static int mt_interrupted = 0;  /* main thread interrupted? */


#define SLICE_SEC  0		/* time slice in */
#define SLICE_USEC 100		/* secs and usecs */


#define IN_CS(proc) (proc ? (proc)->c.in_cs : mt_in_cs)

#define PENDING(proc) (proc ? (proc)->c.pending : mt_pending)

#define INTERRUPTED(proc) (proc ? (proc)->c.interrupted : mt_interrupted)

#define SET_INTRPT(proc) do { if (proc) (proc)->c.interrupted = 1; else mt_interrupted = 1; } while(0)

#define CLR_INTRPT(proc) do { if (proc) (proc)->c.interrupted = 0; else mt_interrupted = 0; } while(0)

#define ENTER_CS(proc) do {                  	\
		         if (proc)           	\
		           (proc)->c.in_cs = 1;	\
		         else                	\
		           mt_in_cs = 1;     	\
		       } while (0)

#define LEAVE_CS(proc) do {                                         	\
		         if (proc) {                                	\
		           (proc)->c.in_cs = 0;                       	\
		           if ((proc)->c.pending) {                   	\
		             (proc)->c.pending = 0;                   	\
		             setitimer (ITIMER_VIRTUAL, &mt, NULL); 	\
		             context_timeout ();                    	\
		           }                                        	\
		         }                                          	\
		         else {                                     	\
		           mt_in_cs = 0;                            	\
		           if (mt_pending) {                        	\
		             mt_pending = 0;                        	\
		             setitimer (ITIMER_VIRTUAL, &mt, NULL); 	\
		             context_timeout ();                    	\
		           }                                        	\
		         }                                          	\
		       } while(0)

#define MAKE_PENDING(proc)  do {                     	\
			      if (proc)              	\
			        (proc)->c.pending = 1; 	\
			      else                   	\
			        mt_pending = 1;      	\
			    } while (0)

/*------------------------------------------------------------------------
 *
 *  Handle timer interrupt
 *
 *------------------------------------------------------------------------
 */
static void interrupt_handler (int sig)
{
#if defined(__hppa) && defined (__hpux) || \
    defined(__sparc__) && defined(__svr4__)
  /* running a sucky os */
  signal (SIGVTALRM, interrupt_handler);
#endif
  if (IN_CS (current_process))
    MAKE_PENDING(current_process);
  else if (INTERRUPTED (current_process)) {
    CLR_INTRPT (current_process);
    setitimer (ITIMER_VIRTUAL, &mt, NULL);
    context_timeout ();
  }
  else 
    SET_INTRPT (current_process);
}

/*------------------------------------------------------------------------
 *
 * Initialize timer data structure
 *
 *------------------------------------------------------------------------
 */
static void context_internal_init (void)
{
  static int first = 1;
  if (first && !unfair) {
    first = 0;
    mt.it_interval.tv_sec = 0;
    mt.it_interval.tv_usec = 0;
    mt.it_value.tv_sec = SLICE_SEC;
    mt.it_value.tv_usec = SLICE_USEC;
    signal (SIGVTALRM, interrupt_handler);
    setitimer (ITIMER_VIRTUAL, &mt, NULL);
  }
}
#endif


/*------------------------------------------------------------------------
 *  
 *  context_unfair --
 *
 *      Return back to unfair scheduling
 *
 *------------------------------------------------------------------------
 */
void context_unfair (void)
{
#ifdef FAIR
  struct itimerval t;
  unfair = 1;
  t.it_interval.tv_sec = 0;  t.it_interval.tv_usec =0;
  t.it_value.tv_sec = 0; t.it_value.tv_usec = 0;
  setitimer (ITIMER_VIRTUAL, &t, NULL);	/* disable timer interrupts */
#endif
}

/*------------------------------------------------------------------------
 *  
 *  context_fair --
 *
 *      Return back to unfair scheduling
 *
 *------------------------------------------------------------------------
 */
void context_fair (void)
{
#ifdef FAIR
  unfair = 0;
  mt.it_interval.tv_sec = 0;
  mt.it_interval.tv_usec = 0;
  mt.it_value.tv_sec = SLICE_SEC;
  mt.it_value.tv_usec = SLICE_USEC;
  signal (SIGVTALRM, interrupt_handler);
  setitimer (ITIMER_VIRTUAL, &mt, NULL);
#endif
}


/*------------------------------------------------------------------------
 *
 * Enable timer interrupts: must be called with interrupts disabled.
 *
 *------------------------------------------------------------------------
 */
#ifdef FAIR
void context_enable (void)
{
  LEAVE_CS (current_process);
}
#endif


/*------------------------------------------------------------------------
 *
 * Disable timer interrupts: must be called with interrupts enabled.
 *
 *------------------------------------------------------------------------
 */
#ifdef FAIR
void context_disable (void)
{
  ENTER_CS (current_process);
}
#endif

 
/*------------------------------------------------------------------------
 *
 * Called with interrupts disabled. Enables interrupts on termination.
 *
 *------------------------------------------------------------------------
 */
void context_switch (process_t *p)
{
  if (!current_process || !_setjmp (current_process->c.buf)) {
    current_process = p;
    _longjmp (p->c.buf,1);
  }
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
  context_enable ();
}

/*------------------------------------------------------------------------
 *
 *  Called with interrupts disabled
 *  cleaup routine
 *
 *------------------------------------------------------------------------
 */
void context_cleanup (void)
{
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
}

/*------------------------------------------------------------------------
 *
 * Called with interrupts disabled;
 * Enables interrupts on exit.
 *
 *------------------------------------------------------------------------
 */
static void context_stub (void)
{
#ifdef FAIR
  context_internal_init ();
#endif
  if (terminated_process) {
    context_destroy (terminated_process);
    terminated_process = NULL;
  }
  context_enable ();
  (*current_process->c.start)();
  context_disable ();
  terminated_process = current_process;
  /*
   * It would be nice to delete the process here, but we can't do that
   * because we're deleting the stack we're executing on! Some other
   * process must free our stack.
   */
  context_switch (context_select ());
}


/*------------------------------------------------------------------------
 *
 *  Called on exit; must be called with interrupts disabled
 *
 *------------------------------------------------------------------------
 */
void context_exit (void)
{
  terminated_process = current_process;
  context_switch (context_select ());
}

/*
 * Crazy Ubuntu jmpbuf encoder/decoder functions
 */

int
DecodeJMPBUF(int j) 
{
   int retVal;

   asm ("mov %1,%%edx; ror $0x9,%%edx; xor %%gs:0x18,%%edx; mov %%edx,%0;"
      :"=r" (retVal)  /* output */           
      :"r" (j)        /* input */        
      :"%edx"         /* clobbered register */ 
   );

   return retVal;
}

int
EncodeJMPBUF(int j) 
{
   int retVal;

   asm ("mov %1,%%edx; xor %%gs:0x18,%%edx; rol $0x9,%%edx; mov %%edx,%0;"
      :"=r" (retVal)  /* output */           
      :"r" (j)        /* input */        
      :"%edx"         /* clobbered register */ 
   );

   return retVal;
}

#if defined(__x86_64__)
unsigned long long EncodeJBRHEL(unsigned long long j)
{
  unsigned long long ret;

  asm ("mov %1,%%rdx; xor %%fs:0x30,%%rdx; rol $0x11,%%rdx; mov %%rdx,%0;"
       :"=r" (ret) /* output */
       :"r" (j) /* input */
       :"%rdx" /* clobbered */
       );

  return ret;
}

unsigned long long DecodeJBRHEL(unsigned long long j)
{
  unsigned long long ret;

  asm ("mov %1,%%rdx; ror $0x11,%%rdx; xor %%fs:0x30,%%rdx; mov %%rdx,%0;"
       :"=r" (ret) /* output */
       :"r" (j) /* input */
       :"%rdx" /* clobbered */
       );

  return ret;
}
#endif

/*
 * Non-portable code is in this function
 */
void context_init (process_t *p, void (*f)(void))
{
  void *stack;
  int n;

  int i;

  p->c.start = f;
  stack = p->c.stack;
  n = p->c.sz;

  _setjmp (p->c.buf);

#if 0
  printf ("%llx context_init, %llx stack\n", (unsigned long long)context_init, 
	  (unsigned long long)&stack);

  for (i=0; i < 8; i++) {
    printf ("%d = %llx (decoded %llx)\n", i, p->c.buf[0].__jmpbuf[i],
	    DecodeJBRHEL(p->c.buf[0].__jmpbuf[i]));
  }
#endif

#ifdef FAIR
  p->c.in_cs = 0;
  p->c.pending = 0;
  p->c.interrupted = 0;
#endif

#if defined(__sparc__) && !defined(__svr4__)

#define INIT_SP(p) (int)((double*)(p)->c.stack + (p)->c.sz/sizeof(double)-11)
#define CURR_SP(p) (p)->c.buf[2]

  /* Need 12 more doubles to save register windows. */
  p->c.buf[3] = p->c.buf[4] = (int)context_stub;
  p->c.buf[2] = (int)((double*)stack + n/sizeof(double)-11);

#elif defined(__sparc__)

#define INIT_SP(p) (int)((double*)(p)->c.stack + (p)->c.sz/sizeof(double)-11)
#define CURR_SP(p) (p)->c.buf[1]

  /* Need 12 more doubles to save register windows. */
  p->c.buf[2] = (int)context_stub;
  p->c.buf[1] = (int)((double*)stack + n/sizeof(double)-11);

#elif defined(__NetBSD__) && defined(__i386__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz-4)
#define CURR_SP(p) (p)->c.buf[2]

  p->c.buf[0] = (int)context_stub;
  p->c.buf[2] = (int)((char*)stack+n-4);

#elif defined(__NetBSD__) && defined(__m68k__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[2]

  p->c.buf[5] = (int)context_stub;
  p->c.buf[2] = (int)((char*)stack+n-4);

#elif defined(__linux__) 

#if defined(__i386__)

/* This works with Ubuntu 12.04 */
#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) DecodeJMPBUF((p)->c.buf[0].__jmpbuf[4])
#define SET_CURR_SP(p,v) (p)->c.buf[0].__jmpbuf[4] = EncodeJMPBUF(v)

  p->c.buf[0].__jmpbuf[5] = EncodeJMPBUF((int)context_stub);
  SET_CURR_SP(p,((int)((char*)stack+n-4)));


/* This works with RedHat 7.1 */
#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0].__jmpbuf[4]

  p->c.buf[0].__jmpbuf[5] = (int)context_stub;
  p->c.buf[0].__jmpbuf[4] = (int)((char*)stack+n-4);

#elif defined(__x86_64__)

#define INIT_SP(p) (unsigned long long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) DecodeJBRHEL((p)->c.buf[0].__jmpbuf[6])
#define SET_CURR_SP(p,v) (p)->c.buf[0].__jmpbuf[6] = EncodeJBRHEL((unsigned long long)v)

  p->c.buf[0].__jmpbuf[7] = EncodeJBRHEL((unsigned long long)context_stub);
  SET_CURR_SP(p,((unsigned long long)((char*)stack+n-8)));

#else

#error Unknown linux arch

#endif


/* 
   This used to work with Linux, version unknown... 
   AFAIK there is no way to distinguish between the lines
   below and above using ifdefs... argh!

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0].__sp

  p->c.buf[0].__pc = (__ptr_t)context_stub;
  p->c.buf[0].__sp = (__ptr_t)((char*)stack+n-4);
*/

#elif defined(__FreeBSD__) && defined(__i386__)

#define INIT_SP(p) (int)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0]._jb[2]

  p->c.buf[0]._jb[0] = (long)context_stub;
  p->c.buf[0]._jb[2] = (long)((char*)(stack+n-4));

#elif defined(__FreeBSD__)  && defined (__amd64__)

#define INIT_SP(p) (long long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[0]._jb[3]
#define SET_CURR_SP(p,v)  ((p)->c.buf[0]._jb[3] = (v)),((p)->c.buf[0]._jb[2] = 16+(v))

  p->c.buf[0]._jb[0] = (long)context_stub;
  SET_CURR_SP(p,(long)((char*)(stack+n-24)));

#if 0
  p->c.buf[0]._jb[2] = (long)((char*)(stack+n-8));
  p->c.buf[0]._jb[3] = (long)((char*)(stack+n-24));
#endif

#elif defined (__alpha)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[34]

  /* entry point needs adjustment */
  p->c.buf[30] = (long)context_stub+8;
  p->c.buf[34] = (long)stack+n-8;

#elif defined (mips) && defined(__sgi)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (p)->c.buf[JB_SP]

  p->c.buf[JB_PC] = (long)context_stub;
  p->c.buf[JB_SP] = (long)stack+n-8;

#elif defined(__hppa) && defined (__hpux)

#define INIT_SP(p) (long)((char*)(p)->c.stack)
#define CURR_SP(p) ((unsigned long*)&(p)->c.buf)[1]

  /* two stack frames, stack grows up */
  ((unsigned long*)&p->c.buf)[1] = (unsigned long)stack + 128; 

  /* function pointers point to a deref table */
  ((unsigned long*)&p->c.buf)[44] = 
    *((unsigned long*)((unsigned long)context_stub & ~3));

#elif defined(__ppc__) && defined (__APPLE__)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) ((unsigned long*)&(p)->c.buf)[0]

#if 0
#define PC_OFFSET 3
#define SP_OFFSET 9
#endif

#error FIXME

#elif defined(__i386__) && defined(__APPLE__)

#define INIT_SP(p) (unsigned long)((unsigned long)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) (unsigned long*)p->c.buf[9]

  p->c.buf[3] = (unsigned long)context_stub;
  p->c.buf[8] = (unsigned long)stack + n - 8;
  p->c.buf[9] = (unsigned long)stack + n - 24;
/*  ((unsigned long*)(&p->c.buf))[21] = (unsigned long)context_stub;
  ((unsigned long*)&(p)->c.buf)[0] = (unsigned long)stack+n-16;
*/

#elif defined(__x86_64__) && defined (__APPLE__)

#define INIT_SP(p) (long)((char*)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) ((unsigned long*)&(p)->c.buf)[2]

#define LOBITS(x)  (0xffffffff & (x))
#define HIBITS(x)  (0xffffffff & ((x)>>32))

  p->c.buf[14] = LOBITS((unsigned long long)context_stub);
  p->c.buf[15] = HIBITS((unsigned long long)context_stub);

  p->c.buf[4] = LOBITS((unsigned long long)stack + n - 40);
  p->c.buf[5] = HIBITS((unsigned long long)stack + n - 40);
  p->c.buf[6] = LOBITS((unsigned long)stack + n - 8);
  p->c.buf[7] = HIBITS((unsigned long)stack + n - 8);
  
#elif defined(PC_OFFSET) && defined(SP_OFFSET)

#define Max(a,b) ((a) > (b) ? (a) : (b))

#define INIT_SP(p) (unsigned long)((unsigned long)(p)->c.stack + (p)->c.sz)
#define CURR_SP(p) ((unsigned long*)&p->c.buf)[SP_OFFSET]

  ((unsigned long*)&p->c.buf)[PC_OFFSET] = (unsigned long)context_stub;
#if !defined (STACK_DIR_UP)
  ((unsigned long*)&p->c.buf)[SP_OFFSET] = (unsigned long)stack+n-Max(sizeof(long),sizeof(double));
#else
  ((unsigned long*)&p->c.buf)[SP_OFFSET] = (unsigned long)stack+Max(sizeof(long),sizeof(double));
#endif

#else
#error Unknown machine/OS combination
#endif
}


#ifndef SET_CURR_SP

#define SET_CURR_SP(p,v)  (CURR_SP(p) = v)

#endif

/*------------------------------------------------------------------------
 *
 *  Save context to file
 *
 *------------------------------------------------------------------------
 */
void context_write (FILE *fp, process_t *p)
{
  int i;
  int n;
  unsigned long *l;
  unsigned char *curr_sp, *init_sp;

  l = (unsigned long *)&p->c.buf;
  for (i=0; i < sizeof(p->c.buf)/sizeof(unsigned long); i++) {
    fprintf (fp, "%lu\n", *l);
    l++;
  }
 /* save stack */
  init_sp = (unsigned char *)INIT_SP(p);
  curr_sp = (unsigned char *)CURR_SP(p);

  if (init_sp > curr_sp) {
    curr_sp = init_sp;
    init_sp = (unsigned char *)CURR_SP(p);
  }

  i = (curr_sp - init_sp);
  fprintf (fp, "%d\n", i);

  /*
   * This is broken. if init_sp == curr_sp, do we save or not???
   * (machine-dependent)
   *
   */
  while (init_sp < curr_sp) {
    i = *init_sp;
    fprintf (fp, "%d\n", i);
    init_sp++;
  }
#ifdef FAIR
  fprintf (fp, "%d\n%d\n%d\n", p->c.in_cs, p->c.pending, p->c.interrupted);
#endif
}


/*------------------------------------------------------------------------
 *
 *  Read Context
 *
 *------------------------------------------------------------------------
 */
void context_read (FILE *fp, process_t *p)
{
  int i;
  int n;
  unsigned long *l;
  unsigned char *init_sp, *curr_sp;

  l = (unsigned long *)&p->c.buf;
  for (i=0; i < sizeof(p->c.buf)/sizeof(unsigned long); i++) {
    fscanf (fp, "%lu", l++);
  }
  init_sp = (unsigned char *)INIT_SP(p);
  curr_sp = (unsigned char *)CURR_SP(p);

  fscanf (fp, "%d", &i);

  /* XXX: assumes stack grows downward */
  curr_sp = init_sp - i;

  /* replace CURR_SP with curr_sp */
  SET_CURR_SP(p,curr_sp);

  if (init_sp > curr_sp) {
    curr_sp = init_sp;
    init_sp = (unsigned char *)CURR_SP(p);
  }

  /* this is broken too... */
  while (init_sp < curr_sp) {
    fscanf (fp, "%d", &i);
    *init_sp = i;
    init_sp++;
  }
#ifdef FAIR
  fscanf (fp, "%d%d%d\n", &p->c.in_cs, &p->c.pending, &p->c.interrupted);
#endif
}
