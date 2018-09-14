/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/

/*
 *
 * Fix multi-terminal transistors in .ext file
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "misc.h"

#define MAXLINE 1024


/*------------------------------------------------------------------------
 *
 *  Usage message
 *
 *------------------------------------------------------------------------
 */
static
void usage (void)
{
  char *usage_str[] = {
    "Usage: extfix <file>",
    "",
    "Converts multi-terminal transistors into two terminal devices,",
    "one for each pair of terminals."
    "",
    NULL
  };
  int i = 0;

  while (usage_str[i]) {
    fprintf (stderr, "%s\n", usage_str[i]);
    i++;
  }
}

int exists_file (char *s)
{
  FILE *fp;
  if ((fp = fopen (s, "r"))) {
    fclose (fp);
    return 1;
  }
  return 0;
}

static int line_number;

static
int mustbe (char *l, char *match, int loc)
{
  int p = 0;

  for (; l[loc] && match[p]; loc++,p++)
    if (l[loc] != match[p])
      fatal_error ("Error on line %d in ext file; match to [%s] failed", line_number, match);
  if (match[p])
    fatal_error ("Error on line %d in ext file; match to [%s] failed", line_number, match);
  return loc;
}

static int skipspace (char *l, int loc)
{
  while (l[loc] && isspace (l[loc]))
    loc++;
  return loc;
}

static int skip_num (char *l, int loc)
{
  int flag = 0;
  loc = skipspace (l,loc);
  while (l[loc] && (isdigit (l[loc]) || l[loc] == '.' || l[loc] == '-')) {
    loc++;
    flag = 1;
  }
  if (!flag)
    fatal_error ("Missing number, line %d of ext file", line_number);
  return loc;
}

static int skip_str (char *l, int loc)
{
  int flag = 0;
  loc = skipspace (l,loc);
  while (l[loc] && !isspace(l[loc])) {
    loc++;
    flag = 1;
  }
  if (!flag)
    fatal_error ("Missing string, line %d of ext file", line_number);
  return loc;
}

static int skip_attrlist (char *l, int loc)
{
  loc--;
  do {
    loc++;
    loc = skip_str (l, loc);
    loc = skipspace (l, loc);
  } while (l[loc] == ',');
  return loc;
}
  
/*------------------------------------------------------------------------
 *
 *  Fixup .ext file
 *
 *------------------------------------------------------------------------
 */
static
void fix_ext_line (char *linein)
{
  int pos;
  int common;
  int start, end, nameend;
  char c;
  static char buf[MAXLINE];
  struct termlist {
    int pos1, pos2;
    int nameend;
    struct termlist *next;
  } *T, *t1, *t2;
  int type;
  int gatestart, gateend;
  static int flagerror = 0;

#define N_FET 1
#define P_FET 2

  /* init L structure the first time */
  if (strncmp (linein, "fet", 3) == 0) {
    linein[strlen(linein)-1] = '\0';
    pos = mustbe (linein, "fet",0);
    pos = skipspace(linein,pos);
    if (linein[pos] == 'n') {
      type = N_FET;
      pos = mustbe (linein, "nfet",pos);
    }
    else {
      type = P_FET;
      pos = mustbe (linein, "pfet", pos);
    }
    pos = skip_num(linein, pos);   pos = skip_num(linein, pos);
    pos = skip_num(linein, pos);   pos = skip_num(linein, pos);
    pos = skip_num(linein, pos);   pos = skip_num(linein, pos);

    /* substrate */
    pos = skip_str (linein, pos);
    
    /* gate */
    gatestart = pos;
    pos = skip_str (linein, pos);
    gateend = pos;
    pos = skip_num (linein, pos); pos = skip_num (linein, pos);

    /* list of terminals */
    T = NULL;
    t2 = NULL;

    common = pos;
    pos = skipspace(linein,pos);
    do {
      start = pos;
      pos = skip_str (linein, pos);
      nameend = pos;
      pos = skip_num (linein, pos);
      pos = skipspace (linein, pos);
      if (linein[pos] == '0')
	pos = skip_num (linein, pos);
      else
	pos = skip_attrlist (linein, pos);
      end = pos-1;
      pos = skipspace (linein, pos);

      MALLOC (t1, struct termlist, 1);
      t1->pos1 = start; t1->pos2 = end;
      t1->nameend = nameend;
      t1->next = NULL;
      if (t2 == NULL) {
	T = t1;
	t2 = T;
      }
      else {
	t2->next = t1;
	t2 = t1;
      }
    } while (linein[pos]);

    start = 1;
    for (t1 = T->next; t1; t1 = t1->next) {
      if (strncmp (linein+t1->pos1,linein+T->pos1,
		   T->nameend-T->pos1+1) != 0) {
	t2 = t1;
	start++;
      }
    }
    if (start > 2) {
      if (!flagerror) {
	fprintf (stderr, "WARNING: Multi terminal fet in output.\n");
	flagerror = 1;
      }
      c = linein[gateend]; linein[gateend] = '\0';
      fprintf (stderr, "WARNING: type=%s gate=%s, ", type == N_FET ? "nfet" : "pfet", 
	       linein+gatestart);
      linein[gateend] = c;
      fprintf (stderr, " terminals=");
      for (t1 = T; t1; t1 = t1->next) {
	c = linein[t1->nameend];
	linein[t1->nameend] = '\0';
	fprintf (stderr, "%s ", linein+t1->pos1);
	linein[t1->nameend] = c;
      }
      fprintf (stderr, "\n");
      printf ("%s\n", linein);
    }
    else {
      t1 = T;
      /* one liner */
      c = linein[common]; linein[common] = '\0';
      printf ("%s ", linein);
      linein[common] = c;
      /* print t1 t2 pair */
      c = linein[t1->pos2+1]; linein[t1->pos2+1] = '\0';
      printf ("%s ", linein+t1->pos1);
      linein[t1->pos2+1] = c;
      
      c = linein[t2->pos2+1]; linein[t2->pos2+1] = '\0';
      printf ("%s\n", linein+t2->pos1);
      linein[t2->pos2+1] = c;
    }
  }
  else
    printf ("%s", linein);
}

void
extfix (FILE *ext)
{
  static char buf[MAXLINE];
  
  /* munge ext file */
  buf[MAXLINE-1] = '\n';
  line_number = 1;
  while (fgets (buf, MAXLINE, ext)) {
    if (buf[MAXLINE-1] == '\0')
      fatal_error ("This needs to be fixed!");      /* FIXME */
    fix_ext_line (buf);
    line_number++;
  }
  fclose (ext);
}




/*------------------------------------------------------------------------
 *
 *  main program
 *
 *------------------------------------------------------------------------
 */
int main (int argc, char **argv)
{
  FILE *fp;
  char *F, *Filename;

  if (argc > 2) {
    usage();
    fatal_error ("Too many arguments");
  }
  if (argc == 2) {
    Filename = argv[1];
    if (exists_file (Filename))
      F = Filename;
    else {
      MALLOC (F, char, strlen(Filename)+5);
      strcpy (F, Filename);
      strcat (F, ".ext");
    }
    if (!(fp = fopen (F, "r")))
      fatal_error ("Unable to open file %s for reading.\n", F);
  }
  else
    fp = stdin;

  extfix (fp);
  return 0;
}
