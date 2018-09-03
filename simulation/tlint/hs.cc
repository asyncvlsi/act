/*
  (c) Rajit Manohar
  Date: Jan 2017

   Based on the Python hspice-1.01 module. Copyright information below.

// (c) Janez Puhan
// Date: 18.5.2009
// HSpice binary file import module
// Modifications:
// 1. All vector names are converted to lowercase.
// 2. In vector names 'v(*' is converted to '*'. 
// 3. No longer try to close a file after failed fopen (caused a crash). 
// Author: Arpad Buermen

*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "misc.h"
#include "array.h"
#include "atrace.h"

#define BLOCKHEADERSIZE					4

#define numOfVariablesPosition			0
#define numOfProbesPosition				4
#define numOfSweepsPosition				8
#define numOfSweepsEndPosition			12
#define postStartPosition1				16
#define postStartPosition2				20
#define postString11					"9007"
#define postString12					"9601"
#define postString21					"2001"
#define numOfPostCharacters				4
#define dateStartPosition				88
#define dateEndPosition					112
#define titleStartPosition				24
#define sweepSizePosition1				176
#define sweepSizePosition2				187
#define vectorDescriptionStartPosition	256

#define frequency						2
#define complex_var						1
#define real_var						0

/*
  Perform endian swap on array of numbers. Arguments:
   block    ... pointer to array of numbers
   size     ... size of the array
   itemSize ... size of one number in the array in bytes
*/
static void do_swap(char *block, int size, int itemSize)
{
  int i;
  for(i = 0; i < size; i++) {
    int j;
    for(j = 0; j < itemSize / 2; j++) {
      char tmp = block[j];
      block[j] = block[itemSize - j - 1];
      block[itemSize - j - 1] = tmp;
    }
    block = block + itemSize;
  }
}


/*
// Read block header. Returns:
//   -1 ... block header corrupted
//    0 ... endian swap not performed
//    1 ... endian swap performed
// Arguments:
//   f           ... pointer to file for reading
//   fileName    ... name of the file
//   blockHeader ... array of four integers consisting block header
//   size        ... size of items in block
*/
static int readBlockHeader(FILE *f, const char *fileName, int *blockHeader, 
			   int size)
{
  int swap;
  int num;

  Assert (sizeof (int) == 4, "Fix this please");

  /* read the header */
  num = fread(blockHeader, sizeof(int), BLOCKHEADERSIZE, f);

  if(num != BLOCKHEADERSIZE)  {
    /* did not read the header */
    return -1;
  }

  /* Block header check and swap */
  if (blockHeader[0] == 0x00000004 && blockHeader[2] == 0x00000004) {
    swap = 0;
  }
  else if (blockHeader[0] == 0x04000000 && blockHeader[2] == 0x04000000) {
    swap = 1;
  }
  else {
    fatal_error ("Invalid block header");
    return -1;
  }
  if(swap == 1) {
    do_swap((char *)blockHeader, BLOCKHEADERSIZE, sizeof(int));
  }
  blockHeader[0] = blockHeader[BLOCKHEADERSIZE - 1] / size; /* XXX: this can't be right */
  return swap;
}


/*
// Read block data. Returns:
//   0 ... reading performed normally
//   1 ... reading failed
// Arguments:
//   f           ... pointer to file for reading
//   fileName    ... name of the file
//   ptr         ... pointer to reserved space for data
//   offset      ... pointer to reserved space size,
//                   increased for current block size
//   itemSize    ... size of one item in block
//   numOfItems  ... number of items in block
//   swap        ... perform endian swap flag
*/
static int readBlockData(FILE *f, const char *fileName, void *ptr,
		  int *offset, int itemSize, int numOfItems, int swap)
{
  int num = fread(ptr, itemSize, numOfItems, f);

  if(num != numOfItems) {
    fatal_error ("reading trace file %s: could not read block data", fileName);
    return 1;
  }

  *offset = *offset + numOfItems;
  if(swap > 0) do_swap((char *)ptr, numOfItems, itemSize); /* endianness */
  return 0;
}


/*
// Read block trailer. Returns:
//   0 ... reading performed normally

// Arguments:
//   f           ... pointer to file for reading
//   fileName    ... name of the file
//   swap        ... perform endian swap flag
//   header      ... block size from header
*/
static int readBlockTrailer(FILE *f, const char *fileName, int swap, int header)
{
  int trailer, num;
  num = fread(&trailer, sizeof(int), 1, f);
  if(num != 1) {
    fatal_error ("reading trace file %s: could not read trailer", fileName);
  }

  if(swap > 0) do_swap((char *)(&trailer), 1, sizeof(int));

  /* Block header and trailer match check. */
  if(header != trailer) {
    fatal_error ("reading trace file %s: inconsistent trailer");
  }
  return 0;
}

/*
// Read one file header block. Returns:
//   -1 ... this was the last block
//    0 ... there is at least one more block left
//    1 ... error occured during reading the block
// Arguments:
//   f         ... pointer to file for reading
//   debugMode ... debug messages flag
//   fileName  ... name of the file
//   buf       ... pointer to header buffer,
//                 enlarged (reallocated) for current block
//   bufOffset ... pointer to buffer size, increased for current block
// size
*/
static int readHeaderBlock(FILE *f, const char *fileName, char **buf,
					int *bufOffset)
{
  int error, blockHeader[BLOCKHEADERSIZE], swap;

  /* Get size of file header block */
  swap = readBlockHeader(f, fileName, blockHeader, sizeof(char));
  if(swap < 0) {
    /* error */
    return 1;
  }

  /* Allocate space for buffer. */
  REALLOC (*buf, char, (*bufOffset + blockHeader[0] + 1));

  /* read data */
  error = readBlockData(f, fileName, *buf + *bufOffset, bufOffset,
			sizeof(char), blockHeader[0], swap /* XXX: was 0 */);
  if(error == 1) return 1;	/* Error. */
  (*buf)[*bufOffset] = 0;

  /* Read trailer of file header block. */
  error = readBlockTrailer(f, fileName, swap, blockHeader[BLOCKHEADERSIZE - 1]);
  if(error == 1) return 1;	/* Error. */

  if(strstr(*buf, "$&%#")) return -1;	/* End of block. */

  return 0;	/* There is more. */
}

#if 0
// Get sweep infornation from file header block. Returns:
//   0 ... performed normally
//   1 ... error occurred
// Arguments:
//   debugMode   ... debug messages flag
//   sweep       ... acquired sweep parameter name, new reference created
//   buf         ... header string
//   sweepSize   ... acquired number of sweep points
//   sweepValues ... sweep points array, new reference created
//   faSweep     ... pointer to fast access structure for sweep array
int getSweepInfo(int debugMode, PyObject **sweep, char *buf, int *sweepSize,
				 PyObject **sweepValues, struct FastArray *faSweep)
{
	char *sweepName = NULL;
	npy_intp dims;
	sweepName = strtok(NULL, " \t\n");	// Get sweep parameter name.
	if(sweepName == NULL)
	{
		if(debugMode)
			fprintf(debugFile, "HSpiceRead: failed to extract sweep name.\n");
		return 1;
	}
	*sweep = PyUnicode_FromString(sweepName);
	if(*sweep == NULL)
	{
		if(debugMode)
			fprintf(debugFile, "HSpiceRead: failed to create sweep name string.\n");
		return 1;
	}

	// Get number of sweep points.
	if(strncmp(&buf[postStartPosition2], postString21, numOfPostCharacters) != 0)
		*sweepSize = atoi(&buf[sweepSizePosition1]);
	else *sweepSize = atoi(&buf[sweepSizePosition2]);

	// Create array for sweep parameter values.
	dims=*sweepSize;
	*sweepValues = PyArray_SimpleNew(1, &dims, PyArray_DOUBLE);
	if(*sweepValues == NULL)
	{
		if(debugMode) fprintf(debugFile, "HSpiceRead: failed to create array.\n");
		return 1;
	}

	// Prepare fast access structure.
	faSweep->data = ((PyArrayObject *)(*sweepValues))->data;
	faSweep->pos = ((PyArrayObject *)(*sweepValues))->data;
	faSweep->stride =
		((PyArrayObject *)(*sweepValues))->strides[((PyArrayObject *)(*sweepValues))->nd -
		1];
	faSweep->length = PyArray_Size(*sweepValues);

	return 0;
}
#endif


/*
// Read one data block. Returns:
//   -1 ... this was the last block
//    0 ... there is at least one more block left
//    1 ... error occured during reading the block
// Arguments:
//   f             ... pointer to file for reading
//   debugMode     ... debug messages flag
//   fileName      ... name of the file
//   rawData       ... pointer to data array,
//                     enlarged (reallocated) for current block
//   rawDataOffset ... pointer to data array size, increased for
// current block size
*/
int readDataBlock(FILE *f, const char *fileName, float **rawData,
		  int *rawDataOffset)
{
  int error, blockHeader[BLOCKHEADERSIZE], swap;

  /* Get size of raw data block. */
  swap = readBlockHeader(f, fileName, blockHeader, sizeof(float));
  if(swap < 0) return 1;	/* Error. */

  /* Allocate space for raw data. */
  if (!*rawData) {
    MALLOC (*rawData, float, blockHeader[0]);
  }
  else {
    REALLOC (*rawData, float, *rawDataOffset + blockHeader[0]);
  }

  /* Read raw data block. */
  error = readBlockData(f, fileName, *rawData + *rawDataOffset,
			rawDataOffset, sizeof(float), blockHeader[0], swap);

  if(error == 1) return 1;	/* Error.*/

  /* Read trailer of file header block. */
  error = readBlockTrailer(f, fileName, swap, blockHeader[BLOCKHEADERSIZE - 1]);
  if(error == 1) return 1;	/* Error.*/

  if((*rawData)[*rawDataOffset - 1] > 9e29) return -1;	/* End of
							   block. */

  error = getc (f);
  ungetc(error, f);

  if (feof (f)) 
    return -1;

  return 0;	/* There is more. */
}

/*
// Convert one table to an atrace file
// Arguments:
//   A              ... atrace output file
//   f              ... pointer to file for reading
//   fileName       ... name of the file
//   numOfVariables ... number of variables in table
//   type           ... type of variables with exeption of scale 
//   numOfVectors   ... number of variables and probes in table
//   scale          ... scale name
//   names           ... array of vector names
*/
void convTable(atrace *A, FILE *f, const char *fileName,
	      int numOfVariables, int type, int numOfVectors,
	       char *scale, name_t **names)
{
  int i, j, num, offset = 0, numOfColumns = numOfVectors;
  float *rawDataPos, *rawData = NULL;
  float prevtm;

  /* Read raw data blocks. */

  /*printf ("Raw data blocks begin at: %lu\n", ftell(f));*/

  do {
    num = readDataBlock(f, fileName, &rawData, &offset);
  } while (num == 0);

  if(num > 0) {
    fatal_error ("Could not read data from trace file %s", fileName);
  }

  /* Increase number of columns if variables with exeption of scale
     are complex. */
  if(type == complex_var) {
    fatal_error ("Can't handle complex variables");
  }

  rawDataPos = rawData;
  num = (offset - 1) / numOfColumns;	/* Number of rows. */

  printf ("   steps: %d\n", num);

  prevtm = -1;
  for(i = 0; i < num; i++) {
    float tm;

    tm = *rawDataPos;

    if ((i > 0) && (tm < prevtm)) {
      warning("[step=%d] Current time: %g; previous time: %g; time moving backward?", i, (double)tm, (double)prevtm);
    }

    rawDataPos++;
    for(j = 0; j < numOfVectors-1; j++) {
      if (names[j]) {
	if (i > 0 && (fabs(names[j]->v-*rawDataPos) >= 1e-6)) {
	  atrace_signal_change (A, names[j], tm, *rawDataPos);
	}
      }
      rawDataPos++;
    }
    prevtm = tm;
  }
  //printf ("left = %d\n", (int) ((unsigned long
  //long)sizeof(float)*offset - (unsigned long long)rawDataPos  +
  //(unsigned long long)rawData));
  free (rawData);
  rawData = NULL;
}

/*
  Utility functions: name conversion.

  1. Remove v( ) for signal
  2. If trailing #, return NULL

*/
static char *name_convert (char *signal)
{
  char *s;
  int l;
  int pos, skip;

  
  l = strlen (signal);
  pos = l-1;

  if ((l > 2) && (signal[0] == 'v' || signal[0] == 'V') &&
      (signal[1] == '(')) {
    skip = 2;
  } 
  else {
    skip = 0;
  }

  if (skip == 2 && (signal[l-1] == ')')) {
    pos = l - 2;
  }

  s = (char *)malloc (l + 2);
  if (!s) {
    return NULL;
  }

  if (signal[skip] == '/') {
    skip++;
  }

  if (pos == l-1) {
    sprintf (s, "%s", signal+skip);
    l = strlen (s);
  }
  else {
    sprintf (s, "%s", signal+skip);
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

void csv_convert (FILE *fp, const char *output)
{
  atrace *A;
  char *buf;
  int sz = 10;
  int first = 1;
  A_DECL (name_t *, names);
  char *tok;

  MALLOC (buf, char, sz);
  buf[sz-1] = '\0';

  A_INIT (names);

  while (fgets (buf, sz, fp)) {
    /* resize buffer if we didn't get the entire line */
    while (buf[strlen(buf)-1] != '\n') {
      int newsz = sz*2;

      REALLOC (buf, char, newsz);
      buf[newsz-1] = '\0';
      Assert (fgets (buf+sz-1, sz+1, fp), "EOF without end of line?");
      sz = newsz;
    }
    if (first == 1) {
      /* get names */

      A = atrace_create (output, ATRACE_DELTA, 1e-9 /* 10ns */ , 1e-12 /* dt */);

      tok = strtok (buf, ",\n");

      while (tok) {
	char *nm, *tmp;
	int l;
	nm = Strdup (tok);
	
	l = strlen (nm);
	
	tok = strtok (NULL, ",\n");

	if (strlen (tok) != l) {
	  printf ("[%s] [%s]\n", nm, tok);
	  fatal_error ("Assumed that names were `name X' `name Y' pairs\n");
	}
	
	if (l < 2 || nm[l-2] != ' ' || nm[l-1] != 'X' || tok[l-2] != ' ' || tok[l-1] != 'Y') {
	  printf ("[%s] [%s]\n", nm, tok);
	  fatal_error ("Names must be [<foo> X] and [<foo> Y]\n");
	}
	nm[l-2] = '\0';
	tmp = name_convert (nm);
	free (nm);
	nm = tmp;

	A_NEW (names, name_t *);
	A_NEXT (names) = atrace_create_node (A, nm);
	A_INC (names);

	free (nm);
	tok = strtok (NULL, ",\n");
      }
    }
    else {
      int i;

      tok = strtok (buf, ",\n");

      i = 0;
      while (tok) {
	float t, v; 
	/* t,v pair */

	if (i >= A_LEN (names)) {
	  fatal_error ("Too much data on line %d\n", first);
	}

	t = atof (tok);
	tok = strtok (NULL, ",\n");
	if (!tok) {
	  fatal_error ("Missing data on line %d\n", first);
	}
	v = atof (tok);

	if (first < 2 || (fabs(names[i]->v - v) >= 1e-6)) {
	  atrace_signal_change (A, names[i], t, v);
	}
	tok = strtok (NULL, ",\n");
	i++;
      }
      if (i != A_LEN (names)) {
	fatal_error ("Missing data from line %d\n", first);
      }
    }
    first++;
  }

  free (buf);

  if (first > 1) {
    atrace_close (A);
  }
  exit (0);
}

void raw_convert (FILE *fp, const char *output)
{
  atrace *A;
  char *buf;
  int sz = 10240;
  int lineno = 0;
  A_DECL (name_t *, names);
  char *tok;
  int nvars;
  int i;
  double *vals;

  MALLOC (buf, char, sz);
  buf[sz-1] = '\0';

  A_INIT (names);

  buf[0] = '\0';
  buf[sz-1] = '\0';

  /* line 1 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  /* line 2 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  /* line 3 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  /* line 4 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  /* line 5 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  tok = strtok (buf, " ");
  tok = strtok (NULL, " ");
  tok = strtok (NULL, " ");
  Assert (tok, "Hmm");
  nvars = atoi (tok);

  /* line 6 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  /* line 7 */
  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");

  /* now read the variables */
  A = atrace_create (output, ATRACE_DELTA, 1e-9 /* 10ns */ , 1e-12 /* dt */);

  atrace_filter (A, 0.01, 0.01); /* 1% change, 10mV change */
  
  for (i=0; i < nvars; i++) {
    /* first line must be time */
    Assert (fgets (buf, sz, fp), "Hmm...");
    Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");

    tok = strtok (buf, " \t");
 
    Assert (atoi(tok) == i, "Hmm");
    tok = strtok (NULL, " \t");

    if (i == 0) {
      if (strcasecmp (tok, "time") != 0) {
	fatal_error ("Variable 0 should be time!\n");
      }
    }
    else {
      char *nm, *tmp;
      nm = Strdup (tok);
      tmp = name_convert (nm);
      A_NEW (names, name_t *);
      A_NEXT (names) = atrace_create_node (A, tmp);
      A_INC (names);
      FREE (nm);

      tok = strtok (NULL, " \t\n");

      if (strcmp (tok, "voltage") != 0) {
	fatal_error ("Only voltage trace files supported");
      }
    }
  }

  fgets (buf, sz, fp);
  Assert (buf[sz-1] == '\0' && buf[strlen (buf)-1] == '\n', "Hmm");
  if (strcmp (buf, "Binary:\n") != 0) {
    fatal_error ("Expecting `Binary:'");
  }
  /* now read the file! */

  MALLOC (vals, double, nvars);

  while (!feof (fp)) {
    int x;

    x = fread (vals, sizeof (double), nvars, fp);
    if (x == 0) break;
    if (x != nvars) {
      fatal_error ("File format error");
    }

    for (i=1; i < nvars; i++) {
      atrace_signal_change (A, names[i-1], vals[0], vals[i]);
    }
  }
  atrace_close (A);
  exit (0);
}

#ifdef HAVE_ZLIB

#include <zlib.h>

void zcsv_convert (const char *input, const char *output)
{
  atrace *A;
  char *buf;
  int sz = 10;
  int first = 1;
  A_DECL (name_t *, names);
  char *tok;
  gzFile gzf;
  
  gzf = gzopen (input, "rb");
  if (gzf == NULL) {
    fatal_error ("Could not open file `%s' for reading", input);
  }

  MALLOC (buf, char, sz);
  buf[sz-1] = '\0';

  A_INIT (names);
  
  while (gzgets (gzf, buf, sz)) {
    /* resize buffer if we didn't get the entire line */
    while (buf[strlen(buf)-1] != '\n') {
      int newsz = sz*2;

      REALLOC (buf, char, newsz);
      buf[newsz-1] = '\0';
      Assert (gzgets (gzf, buf+sz-1, sz+1), "EOF without end of line?");
      sz = newsz;
    }
    if (first == 1) {
      /* get names */

      A = atrace_create (output, ATRACE_DELTA, 1e-9 /* 10ns */ , 1e-12 /* dt */);

      tok = strtok (buf, ",\n");

      while (tok) {
	char *nm, *tmp;
	int l;
	nm = Strdup (tok);
	
	l = strlen (nm);
	
	tok = strtok (NULL, ",\n");

	if (strlen (tok) != l) {
	  printf ("[%s] [%s]\n", nm, tok);
	  fatal_error ("Assumed that names were `name X' `name Y' pairs\n");
	}
	
	if (l < 2 || nm[l-2] != ' ' || nm[l-1] != 'X' || tok[l-2] != ' ' || tok[l-1] != 'Y') {
	  printf ("[%s] [%s]\n", nm, tok);
	  fatal_error ("Names must be [<foo> X] and [<foo> Y]\n");
	}
	nm[l-2] = '\0';
	tmp = name_convert (nm);
	free (nm);
	nm = tmp;

	A_NEW (names, name_t *);
	A_NEXT (names) = atrace_create_node (A, nm);
	A_INC (names);

	free (nm);
	tok = strtok (NULL, ",\n");
      }
    }
    else {
      int i;

      tok = strtok (buf, ",\n");

      i = 0;
      while (tok) {
	float t, v; 
	/* t,v pair */

	if (i >= A_LEN (names)) {
	  fatal_error ("Too much data on line %d\n", first);
	}

	t = atof (tok);
	tok = strtok (NULL, ",\n");
	if (!tok) {
	  fatal_error ("Missing data on line %d\n", first);
	}
	v = atof (tok);

	if (first < 2 || (fabs(names[i]->v - v) >= 1e-6)) {
	  atrace_signal_change (A, names[i], t, v);
	}
	tok = strtok (NULL, ",\n");
	i++;
      }
      if (i != A_LEN (names)) {
	fatal_error ("Missing data from line %d\n", first);
      }
    }
    first++;
  }

  free (buf);

  if (first > 1) {
    atrace_close (A);
  }
  exit (0);
}

#endif


int main (int argc, char **argv) 
{
  const char *fileName;
  const char *outfile;
  FILE *f;
  
  name_t **names;

  char *buf = NULL;
  int offset = 0;
  
  int i, num;
  int numOfVectors, numOfVariables;

  char *token;
  int type;

  char *scale;

  atrace *A;

  int raw_fmt = 0;

  if (argc != 3 && argc != 4) {
    fatal_error ("Usage: %s [-r] <tracefile> <atrace file>\n", argv[0]);
  }
  if (argc == 4) {
    if (strcmp (argv[1], "-r") != 0) {
      fatal_error ("Usage: %s [-r] <tracefile> <atrace file>\n", argv[0]);
    }
    raw_fmt = 1;
    fileName = argv[2];
    outfile = argv[3];
  }
  else {
    fileName = argv[1];
    outfile = argv[2];
  }
  
  f = fopen(fileName, "rb");	/* open file */
  if(f == NULL) {
    fatal_error ("Could not open file %s for reading", fileName);
  }
  num = getc(f);
  ungetc(num, f);
  if(num == EOF) {
    fatal_error ("Trace file %s is empty", fileName);
  }

  if (raw_fmt == 1) {
    raw_convert (f, outfile);
  }
  else {
    if((num & 0xff) >= ' ') {
      /* ASCII file, try CSV conversion */
      csv_convert (f, outfile);
      fatal_error ("Trace file %s looks like an ASCII file", fileName);
    }
#ifdef HAVE_ZLIB
    else if ((num & 0xff) == 0x1f) {
      /* might be gzipped */
      num = getc (f);
      num = getc (f);
      if ((num & 0xff) == 0x8b) {
	fclose (f);
	zcsv_convert (fileName, outfile);
	exit (0);
      }
      fseek (f, 0, SEEK_SET);
    }
#endif
  }

  /* Read file */
  do {
    num = readHeaderBlock(f, fileName, &buf, &offset);
  } while(num == 0);

  if(num > 0) {
    fatal_error ("Error reading trace file!");
  }

  /* Check version of post format. */
  if(strncmp(&buf[postStartPosition1], postString11, numOfPostCharacters) != 0 &&
     strncmp(&buf[postStartPosition1], postString12, numOfPostCharacters) != 0 &&
     strncmp(&buf[postStartPosition2], postString21, numOfPostCharacters) != 0)
    {
      fatal_error ("Unknown post format!\n");
    }

  /* file contains creation date */
  buf[dateEndPosition] = 0;
  /*date = PyUnicode_FromString(&buf[dateStartPosition]);*/

  i = dateStartPosition - 1;
  while(buf[i] == ' ') i--;
  buf[i + 1] = 0;

  /*title = PyUnicode_FromString(&buf[titleStartPosition]);*/
  printf ("HSPICE trace file information\n");
  printf ("   Title: %s\n", &buf[titleStartPosition]);
  printf ("    Date: %s\n", &buf[dateStartPosition]);
  
  buf[numOfSweepsEndPosition] = 0;	/* Check number of sweep parameters. */
  num = atoi(&buf[numOfSweepsPosition]);
  if(num < 0 || num > 1) {
    fatal_error ("Only one-dimensional sweeps supported (num=%d)", num);
  }

  buf[numOfSweepsPosition] = 0;	/* Get number of vectors (variables
				   and probes) */
  numOfVectors = atoi(&buf[numOfProbesPosition]);

  printf ("    #vec: %d\n", numOfVectors);
  
  buf[numOfProbesPosition] = 0;
  numOfVariables = atoi(&buf[numOfVariablesPosition]);	/* Scale included. */
  numOfVectors = numOfVectors + numOfVariables;

  printf ("    #var: %d\n", numOfVariables);

  /* Get type of variables. Scale is always real. */
  token = strtok(&buf[vectorDescriptionStartPosition], " \t\n");
  type = atoi(token);

  if(type == frequency) {
    type = complex_var;
  }
  else {
    type = real_var;
  }

  /* skip the types for each entry */
  for(i = 0; i < numOfVectors; i++) {
    token = strtok(NULL, " \t\n");
  }

  if(token == NULL) {
    fatal_error ("Failed to extract independent variable names");
  }

  if (strcmp (token, "TIME") != 0) {
    warning ("Did not find TIME at the right location... trying new approach");
    while (token && strcmp (token, "TIME") != 0) {
      numOfVectors++;
      numOfVariables++;
      token = strtok(NULL, " \t\n");
    }
    if (!token) {
      fatal_error ("Could not find location of TIME in the trace file");
    }
    printf (" new #var: %d\n", numOfVariables);
  }

  scale = Strdup (token);

  A = atrace_create (outfile, ATRACE_DELTA, 1e-9 /* 10ns */ , 1e-12 /* dt */);
  
  MALLOC (names, name_t *, numOfVectors-1);
  for (i=0; i < numOfVectors-1; i++) {
    names[i] = NULL;
  }

  for(i = 0; i < numOfVectors - 1; i++)	{
    /* Get vector names. */
    char *tmp = strtok(NULL, " \t\n");
    if (!tmp) {
      fatal_error ("Error in reading names of vectors");
    }
    tmp = name_convert (tmp);

    if (strlen (tmp) > 2 && tmp[0] == 'i' && tmp[1] == '(') {
      /* skip currents */
      names[i] = NULL;
    }
    else {
      names[i] = atrace_create_node (A, tmp);
      atrace_mk_analog (names[i]);
    }
    free (tmp);
  }

  // Process vector names: make name lowercase, remove v( in front of name
  if(num == 1)	fatal_error ("Handle sweeps!");

  convTable(A, f, fileName, numOfVariables, type,
	    numOfVectors, scale, names);

  fclose(f);
  atrace_close (A);
  return 0;
}
