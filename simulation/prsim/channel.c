// Functions that are useful for channels in prsim
#include "prs.h"
#include "misc.h"
#include "array.h"
#include "channel.h"
#include <assert.h>
#include "math.h"
#include <string.h>

#define MAX_NAME_LENGTH 300

extern void signal_handler (int sig);

#define INTERRUPT signal_handler(0)
//#define INTERRUPT exit(1)

typedef enum {
	VAL_FROM_SIM_NEUTRAL, VAL_FROM_SIM_ERROR, VAL_FROM_SIM_VALID
} ValFromSimType;

//-----------------------------------------------------------------------------
// Called by the outside world...


//-----------------------------------------------------------------------------
// Internal stuff

// Returns whether or not it was successful
//static int channel_getValueFromInjectFile(PrsChannel *, unsigned int *);

static int channel_getValueFromFile (int *, FILE *, int , char *, int *);
	
static ValFromSimType channel_getValueFromSim (Prs *, PrsChannel *, unsigned int *);

// Get a value from the simulator and dump it to a file
static void channel_dumpValue (Prs *, PrsChannel * );

// Get value from simulator and from file and compare thme.
static void channel_expectValue (Prs *, PrsChannel *);

// Will lower any rail of this channel that is not already F
static void channel_makeNeutral(Prs *, PrsChannel *);

// Will raise the appropriate data rails to drive this value on the channel
static void channel_driveValue(Prs *, PrsChannel *, unsigned int);

// Util function to print out all of the channels.
static void channel_printChannels(Prs *, struct Channel *);

// Called when a channel's enable is lowered.  Take appropriate actions for
// inject, expect, dump channls.
static void channel_enableLowered (Prs *, PrsChannel *);

// Called when a channel's enable is raised.  Take appropriate actions for
// inject / expect / dump channels.
static void channel_enableRaised (Prs *, PrsChannel *);

// Lower any rails that is X.
static void channel_resetXRails (Prs *, PrsChannel *);

static void channel_logValue (Prs *, PrsChannel *);

// Used to convert between PRS_VAL_BLAH and a character for printfs.
char valString[] = { '1', '0', 'X' };

// Utility functions for moving between decimal values in files and
// representations are easier to deal with internally.

// Input is a decimal value.  Fill up array bin with 1's and 0's.  bin[0] is
// value of bit0.  Zero-pad for n bits.
static void convert_dec2base2array(unsigned int , int *, int );

// Same as above but does base4	
static void convert_dec2base4array(unsigned int , int *, int );

static void convert_baseNarray2dec(int *, int , unsigned int *, int );

//-----------------------------------------------------------------------------
// Add a channel name to the hChannels hashtable in Prs
static void insert_channel(char *s, struct Hashtable *H, PrsChannel *chan) {
  hash_bucket_t *b;
	
  b = hash_add (H, s);
  chan->b = b;
  b->v = (void *)chan;
}

//-----------------------------------------------------------------------------
// get a channel from the hashtable.
static PrsChannel *get_channel(char *s, struct Hashtable *H) {
	PrsChannel *chan;
	hash_bucket_t *b;
	
	b = hash_lookup (H, s);
	if (!b) {
		return NULL;
	} else {
	  chan = (PrsChannel *)b->v;
	  return chan;
	}
}


//-----------------------------------------------------------------------------
// Creates a new channel.  We need to do the following:
// - Make sure that the channel type is legal
// - Update the channels array for the channel's enable
// TODO: check that nobody has created a channel by this name yet...
void create_channel(Prs *P, struct Channel *C, char *sChannelType, int size, char *sName) {
	PrsChannel *chan;
	char *sEnableName;
	char sRailName[MAX_NAME_LENGTH];
	PrsNode *rail;
	PrsNode *enable;
	int i, b;
	char *sChannelName;
	PrsNode *reset;

	// If this is the first channel created, then we need to make Reset a
	// breakpoint.
	if (C->reset == NULL) {
		reset = prs_node (P, "Reset");
		if (reset == NULL) {
			printf("Could not find node \"Reset\"!!!!\n");
			INTERRUPT;
		} else {
		        C->reset = reset;
			// Make reset a breakpoint
			reset->bp = 1;
		}
	}
		

	MALLOC(sChannelName, char, MAX_NAME_LENGTH);
	MALLOC(chan, PrsChannel, 1);

	sprintf(sChannelName, "%s", sName);

	// Make sure that the channel type is okay and do type-specific stuff
	if (strcmp(sChannelType, "e1ofN") == 0) {
		DBG("* Creating e1ofN channel\n");
		chan->type = CHAN_e1ofN;

		// Store an array of data rails
		MALLOC(chan->dataRails, PrsNode *, size);
		
		for (i = 0; i < size; i++) {
			sprintf(sRailName, "%s.d[%d]", sChannelName, i);
			rail = prs_node (P, sRailName);
			if (!rail) {
			  if (size == 1) {
			    sprintf (sRailName, "%s.d", sChannelName);
			    rail = prs_node (P, sRailName);
			  }
			  if (!rail) {
			    printf("ERROR: Could not find data rail %s", sRailName);
			    FREE(chan->dataRails);
			    return;
			  }
			}
			chan->dataRails[i] = rail;
		}
		chan->numDataRails = size;


	} else if (strcmp(sChannelType, "eMx1of2") == 0) {
		DBG("* Creating eMx1of2 channel\n");
		chan->type = CHAN_eMx1of2;
		chan->numDataRails = 2*size;

		// Store an array of data rails
		MALLOC(chan->dataRails, PrsNode *, size*2);
		for (b = 0; b < size; b++) {
			for (i = 0; i < 2; i++) {
				sprintf(sRailName, "%s.b[%d].d[%d]", sChannelName, b, i);
				rail = prs_node (P, sRailName);
				if (!rail) {
					printf("ERROR: Could not find data rail %s", sRailName);
					FREE(chan->dataRails);
					return;
				}
				chan->dataRails[b*2+i] = rail;
			}
		}
	} else if (strcmp(sChannelType, "eDx1of2") == 0) {
		DBG("* Creating eDx1of2 channel\n");
		chan->type = CHAN_eDx1of2;
		chan->numDataRails = 2*size;

		// Store an array of data rails
		MALLOC(chan->dataRails, PrsNode *, size*2);
		for (b = 0; b < size; b++) {
			for (i = 0; i < 2; i++) {
				sprintf(sRailName, "%s.d[%d].d[%d]", sChannelName, b, i);
				rail = prs_node (P, sRailName);
				if (!rail) {
					printf("ERROR: Could not find data rail %s", sRailName);
					FREE(chan->dataRails);
					return;
				}
				chan->dataRails[b*2+i] = rail;
			}
		}
	} else if (strcmp(sChannelType, "eMx1of4") == 0) {
		DBG("* Creating eMx1of4 channel\n");
		chan->type = CHAN_eMx1of4;
		chan->numDataRails = 4*size;

		// Store an array of data rails
		MALLOC(chan->dataRails, PrsNode *, size*4);
		for (b = 0; b < size; b++) {
			for (i = 0; i < 4; i++) {
				sprintf(sRailName, "%s.b[%d].d[%d]", sChannelName, b, i);
				rail = prs_node (P, sRailName);
				if (!rail) {
					printf("ERROR: Could not find data rail %s", sRailName);
					FREE(chan->dataRails);
					return;
				}
				chan->dataRails[b*4+i] = rail;
			}
		}

	} else {
		printf("ERROR: Don't know anything about channel type %s\n", sChannelType);
		return;
	}

	// Do some stuff that is common to all channel types!
	chan->size = size;
	chan->name = sChannelName;
	chan->fpInject = chan->fpDump = chan->fpExpect = NULL;
	chan->isInject = chan->isDump = chan->isExpect = 0;
	chan->isFirstEnableTransition = 1;

	assert (!chan->fpInject);	

	// Assign the enable
	MALLOC(sEnableName, char, MAX_NAME_LENGTH);
	sprintf(sEnableName, "%s.e", sChannelName);

	//DBG("Looking for enable %s...\n", sEnableName);

	enable = prs_node (P, sEnableName);
	if (!enable) {
		printf ("ERROR: Could not find enable \"%s\" for channel %s\n", sEnableName, sChannelName);
		return;
	}

	chan->enable = enable;
	CHINFO(enable)->hasChans = 1;
	enable->bp = 1;

	// Also need to add this channel to the channel hash
	insert_channel(chan->name, C->hChannels, chan);

	printf("Created channel %s.\n", chan->name);

}

//-----------------------------------------------------------------------------
// Use this file to drive values on a channel
void channel_injectfile(Prs *P, struct Channel *C, char *sChanName, char *sFileName, int isLoop) {
	PrsChannel *chan;
	unsigned int val;
	int i;

	// First make sure that this channel exists!
	chan = get_channel(sChanName, C->hChannels);
	if (chan == NULL) {
		printf("ERROR: Could not find channel %s.\n", sChanName);
		return;
	} else {
		DBG("Got channel named %s.\n", chan->name);
	}

	if (chan->fpInject) {
		printf("WARNING: Injecting file for %s when that channel already has an injectfile open (%s).\n", sChanName, chan->sInject);
	}

	chan->fpInject = fopen(sFileName, "r");
	MALLOC(chan->sInject, char, MAX_NAME_LENGTH);	
	strcpy(chan->sInject, sFileName);
	if (chan->fpInject == NULL) {
		printf("ERROR: Could not open '%s' for reading\n", sFileName);
		chan->fpInject = NULL;
		return;
	}

	chan->isInject = 1;
	chan->linenumInject = 0;
	if (isLoop) {
		chan->loopInject = 1;
	} else {
		chan->loopInject = 0;
	}
}

//-----------------------------------------------------------------------------
// Check values on this channel against those in this file.
void channel_expectfile(Prs *P, struct Channel *C, char *sChanName, char *sFileName, int isLoop) {
	PrsChannel *chan;
	unsigned int val;
	int i;

	// First make sure that this channel exists!
	chan = get_channel(sChanName, C->hChannels);
	if (chan == NULL) {
		printf("ERROR: Could not find channel %s.\n", sChanName);
		return;
	} else {
		DBG("Got channel named %s.\n", chan->name);
	}

	if (chan->fpExpect) {
		printf("WARNING: expectfile for %s when that channel already has an expectfile open (%s).\n", sChanName, chan->sExpect);
	}

	chan->fpExpect = fopen(sFileName, "r");
	MALLOC(chan->sExpect, char, MAX_NAME_LENGTH);	
	strcpy(chan->sExpect, sFileName);
	if (chan->fpExpect == NULL) {
		printf("ERROR: Could not open '%s' for reading\n", sFileName);
		chan->fpExpect = NULL;
		return;
	}

	chan->isExpect = 1;
	chan->linenumExpect = 0;
	if (isLoop) {
		chan->loopExpect = 1;
	} else {
		chan->loopExpect = 0;
	}

	if (chan->enable->val == PRS_VAL_T) {
		printf("WARNING: Enable was already high when you did expectfile.  You may miss the first value.  You should apply expectfile before reset.\n");
	}
}

//-----------------------------------------------------------------------------
// Put values from this channel into a file.
void channel_dumpfile(Prs *P, struct Channel *C, char *sChanName, char *sFileName) {
	PrsChannel *chan;
	unsigned int val;
	int i;

	// First make sure that this channel exists!
	chan = get_channel(sChanName, C->hChannels);
	if (chan == NULL) {
		printf("ERROR: Could not find channel %s.\n", sChanName);
		return;
	} else {
		DBG("Got channel named %s.\n", chan->name);
	}

	if (chan->fpDump) {
		printf("WARNING: dumpfile for %s when that channel already has a dumpfile open (%s).\n", sChanName, chan->sDump);
	}

	chan->fpDump = fopen(sFileName, "w");
	MALLOC(chan->sDump, char, MAX_NAME_LENGTH);	
	strcpy(chan->sDump, sFileName);
	if (chan->fpDump == NULL) {
		printf("ERROR: Could not open '%s' for reading\n", sFileName);
		chan->fpDump = NULL;
		return;
	}

	chan->isDump = 1;
	chan->linenumDump = 0;

	if (chan->enable->val == PRS_VAL_T) {
		printf("WARNING: Enable was already high when you did dumpfile.  You may miss the first value.  You should apply dumpfile before reset.\n");
	}
}


//-----------------------------------------------------------------------------
// If reset is high, then skip.
// O/w...
// Go through all of the channels from the hChannels and find the ones that
// have this node as their enable.
void channel_enableSwitched(Prs *P, struct Channel *C, PrsNode *enable) {
  int i;
  PrsChannel *chan;
  struct Hashtable *h;
  hash_bucket_t *b;
  PrsNode *reset;

  reset = C->reset;
  if (reset == NULL) {
    printf("Something is really hosed...  Reset should not be NULL...\n");
  }
  if (reset->val == PRS_VAL_F) {
    h = C->hChannels;
    for (i = 0; i < h->size; i++) {
      for (b = h->head[i]; b; b = b->next) {
	chan = (PrsChannel *)b->v;
	if (chan->enable == enable) {
	  DBG ("* Enable for channel %s switched to %c!\n", chan->name, valString[chan->enable->val]);
	  if (chan->enable->val == PRS_VAL_F) {
	    channel_enableLowered (P, chan);
	  } else if (chan->enable->val == PRS_VAL_T) {
	    channel_enableRaised (P, chan);
	  }
	  // Nothing to do if enable is X

	  // If this is the first transition, then this variable has to become
	  // false (o/w this is just a vacuous firing...)
	  chan->isFirstEnableTransition = 0;
	}
      }
    }
  } else {
    // Reset is false, ignoring change...
    DBG("* Reset is not false, so not doing anything...\n");
  }
}

//-----------------------------------------------------------------------------
// If reset went high, then neutralize all of the input channels
// If reset went low, then pretend that all of the enables just switched
void channel_resetSwitched(Prs *P, struct Channel *C) {
  PrsNode *reset;
  int i;
  PrsChannel *chan;
  struct Hashtable *h;
  hash_bucket_t *b;

  reset = C->reset;
  if (reset == NULL) {
    printf("Something is really hosed...  Reset should not be NULL...\n");
  }
  if (reset->val == PRS_VAL_T) {
    DBG("*** Reset := 1, time to neutralize!!!!\n");
  } else if (reset->val == PRS_VAL_F) {
    DBG("*** Reset := 0, time to look at enables!!!!!\n");
  }
  h = C->hChannels;
  // Get a pointer to each channel
  for (i = 0; i < h->size; i++) {
    for (b = h->head[i]; b; b = b->next) {
      chan = (PrsChannel *)b->v;
      DBG("*** chan = %s\n", chan->name);

      if (reset->val == PRS_VAL_T) {
	if (chan->isInject) {
	  // Neutralize this channel!
	  channel_makeNeutral (P, chan);
	}
      } else if (reset->val == PRS_VAL_F) {
	if (chan->enable->val == PRS_VAL_F) {
	  channel_enableLowered (P, chan);
	} else if (chan->enable->val == PRS_VAL_T) {
	  channel_enableRaised (P, chan);
	}
      }
    }
  }
}
			



/*


	INTERNAL STUFF


*/


//-----------------------------------------------------------------------------
// Action depends on whether this file is inject, expect, or dump
static void channel_enableRaised(Prs *P, PrsChannel *chan) {
	 int val;
	 DBG("* in channel_enableRaised %s\n", chan->name);
	if (chan->isInject) {
		DBG("* Channel %s is an injectfile channel\n", chan->name);
		if (!channel_getValueFromFile (&chan->linenumInject, chan->fpInject, chan->loopInject, chan->sInject, &val)) {
			DBG("Got NULL from inject file...\n");
		} else {
			channel_driveValue (P, chan, val);
		}
		// Have to do a quick check here to reset any data rails that need it.
		channel_resetXRails (P, chan);
	}
	if (chan->isExpect || chan->isDump) {
		// do nothing

	}
}


//-----------------------------------------------------------------------------
// Action depends on whether this file is inject, expect, or dump
static void channel_enableLowered(Prs *P, PrsChannel *chan) {
	if (chan->isInject) {
		channel_makeNeutral (P, chan);
	}
	if (chan->isExpect) {
		channel_expectValue (P, chan);
	}
	if (chan->isDump) {
		channel_dumpValue (P, chan);
	}
}

//-----------------------------------------------------------------------------
// Lower any data rails that are at X.
static void channel_resetXRails (Prs *P, PrsChannel *chan) {
	int i;
	DBG("* Resetting any rail that is X...\n");
	for (i = 0; i < chan->numDataRails; i++) {
		if ((chan->dataRails[i]->val == PRS_VAL_X) && !(chan->dataRails[i]->queue)) {
			DBG("* Lowering rail %s.d[%d].\n", chan->name, i);
			prs_set_node (P, chan->dataRails[i], PRS_VAL_F);
					
		}
	}
}


//-----------------------------------------------------------------------------
// Read a value from the inject file.
// If we're doing a loop-injectfile, start over when done with the file.
/*
static int channel_getValueFromInjectFile(PrsChannel *chan, unsigned int *val) {
	int result;

	chan->linenumInject++;
	// Now we need to initialize this channel, based on the value from the file.
	result = fscanf(chan->fpInject, "%u", val);
	if (result == 1) {
		//DBG("Got result %u from file\n", *val);
		return 1;
	} else if (result == EOF) {
		if (chan->loopInject) {
			rewind(chan->fpInject);
			return channel_getValueFromInjectFile(chan, val);
		} else {
			printf("Out of values for %s.\n", chan->sInject);
			return 0;
		}
	} else if (result == 0) {
		printf("ERORR: Problem reading file %s (line %d), aborting injectfile", chan->sInject, chan->linenumInject);
		fclose(chan->fpInject);
		chan->fpInject = NULL;
		chan->sInject = NULL;
		return 0;
	} else {
		DBG("Not sure wtf is going on here... result of fscanf = %d (line #%d)\n", result, chan->linenumInject);
		return 0;
	}
}
*/

//-----------------------------------------------------------------------------
// Read a value from the inject file.
// If we're doing a loop-injectfile, start over when done with the file.
static int channel_getValueFromFile
	(int *linenum, FILE *fp, int loop, char *sName, int *val) {
	int result;

	(*linenum)++;

	result = fscanf(fp, "%d", val);
	if (result == 1) {
		return 1;
	} else if (result == EOF) {
		if (loop) {
			rewind(fp);
			return channel_getValueFromFile(linenum, fp, loop, sName, val);
		} else {
			printf("Out of values for %s.\n", sName);
			return 0;
		}
	} else if (result == 0) {
		printf("ERORR: Problem reading file %s (line %d), aborting", sName, *linenum);
		fclose(fp);
		//chan->fpInject = NULL;
		//chan->sInject = NULL;
		return 0;
	} else {
		DBG("Not sure wtf is going on here... result of fscanf = %d (line #%d)\n", result, *linenum);
		return 0;
	}
}



//-----------------------------------------------------------------------------
// Lowers all of the data rails for a channel
static void channel_makeNeutral(Prs *P, PrsChannel *chan) {
	int i;
	DBG("* Making channel %s neutral.\n", chan->name);
	for (i = 0; i < chan->numDataRails; i++) {
		if (chan->dataRails[i]->val != PRS_VAL_F)	 {
		  prs_set_node (P, chan->dataRails[i], PRS_VAL_F);
		}
	}

}


//-----------------------------------------------------------------------------
// Raise the appropriate data rails to send a value on a channel
static void channel_driveValue(Prs *P, PrsChannel *chan, unsigned int val) {
	int i, b;
	int numRails;
	int *valArray;
	int raised;
	DBG("** in channel_driveValue()\n");
	switch (chan->type) {
		case CHAN_e1ofN:
			DBG("* e1ofN channel\n");
			numRails = chan->size;
			if (val > numRails) {
				printf("ERROR: Cannot assign value %d to channel %s, which is e1of%d.\n", val, chan->name, chan->size);
				INTERRUPT;
				return;
			}
			if (chan->dataRails[val]->val == PRS_VAL_T) {
				printf("Something fishy is going on...\n");
				INTERRUPT;
				return;
			}
			
			DBG("* Raising rail %s.d[%d].\n", chan->name, val);
			prs_set_node (P, chan->dataRails[val], PRS_VAL_T);
			break;


	        case CHAN_eDx1of2:
		case CHAN_eMx1of2:
			DBG("* eMx1of2 channel\n");
			MALLOC(valArray, int, chan->size);
			// Make sure val is small enough (< 2**chan_size)
			if (val >= pow(2,chan->size)) {
				printf("ERROR: Value %u is too big for channel %s (e%dx1of2)\n",
					val, chan->name, chan->size);
				exit(1);
			}
			// Convert to array of 1's and 0's
			convert_dec2base2array(val, valArray, chan->size);
			for (b = 0; b < chan->size; b++) {
				if (valArray[b] == 0) {
				  prs_set_node (P, chan->dataRails[b*2], PRS_VAL_T);
				} else if (valArray[b] == 1) {
				  prs_set_node (P, chan->dataRails[b*2+1], PRS_VAL_T);
				} else {
					printf("Looks like there's a bug in driveValue() for Mx1of2...\n");
					exit(1);
				}
			}
			FREE(valArray);
			break;


		case CHAN_eMx1of4:
			DBG("* eMx1of4 channel\n");
			MALLOC(valArray, int, chan->size);

			// Make sure val is small enough (< 4**chan_size)
			if (val >= pow(4,chan->size)) {
				printf("ERROR: Value %u is too big for channel %s (e%dx1of4)\n",
					val, chan->name, chan->size);
				exit(1);
			}
			// Convert to array of (0,1,2,3)...
			convert_dec2base4array(val, valArray, chan->size);
			for (b = 0; b < chan->size; b++) {
				raised = 0;
				for (i = 0; i < 4; i++) {
					if (valArray[b] == i) {
						if (!raised) {
						  prs_set_node (P, chan->dataRails[b*4+i], PRS_VAL_T);
							raised = 1;
						} else {
							printf("Looks like there's a bug in driveValue() for Mx1of4...\n");
						}
					}
				}
				if (!raised) {
					printf("Didn't raise any rails for %s.b[%d]...\n", chan->name, b);
					printf("valArray[%d] = %d.\n", b, valArray[b]);
				}
			}
			FREE(valArray);
			break;
		default:
			printf("ERROR: Channel %s has unknown type!\n", chan->name);
			exit(1);
			break;
	}
}


//-----------------------------------------------------------------------------
// Enable was lowered, get value and dump to file
static void channel_dumpValue (Prs *P, PrsChannel *chan ) {
	unsigned int chanVal;
	ValFromSimType result;

	// Get the value of the channel
	result = channel_getValueFromSim (P, chan, &chanVal);
	if (result == VAL_FROM_SIM_ERROR) {
		printf("ERROR: Problem getting value for channel %s from simulation!\n", chan->name);
		INTERRUPT;
		return;
	}
	// result == -1 means that the channel was neutral	
	else if (result == VAL_FROM_SIM_NEUTRAL) {
		if (!chan->isFirstEnableTransition) {
			printf("WARNING: %s.e is low, but channel %s is neutral.  If this is not reset, then you have problems...\n", chan->name, chan->name);
		}
		return;
	}

	fprintf(chan->fpDump, "%d\n", chanVal);
	fflush (chan->fpDump);
}


//-----------------------------------------------------------------------------
// Enable was lowered, check that the value on the channel matches that in the
// file.
static void channel_expectValue (Prs *P, PrsChannel *chan ) {
	unsigned int chanVal;
	int fileVal;
	ValFromSimType result;

	// Get the value of the channel
	result = channel_getValueFromSim (P, chan, &chanVal);
	if (result == VAL_FROM_SIM_ERROR) {
		printf("ERROR: Problem getting value for channel %s from simulation!\n", chan->name);
		INTERRUPT;
		return;
	}
	// result == -1 means that the channel was neutral	
	else if (result == VAL_FROM_SIM_NEUTRAL) {
		if (!chan->isFirstEnableTransition) {
			INTERRUPT;
			printf("WARNING: %s.e is low, but channel %s is neutral.  If this is not reset, then you have problems...\n", chan->name, chan->name);
		}
		return;
	}

	// Get the value from the file
	if (!channel_getValueFromFile(&chan->linenumExpect, chan->fpExpect, chan->loopExpect, chan->sExpect, &fileVal)) {
		printf("ERROR: Problem getting value for channel %s from file!\n", chan->name);
		INTERRUPT;
		return;
	}

	if (fileVal == -1) {
		DBG("Ignoring value for %s\n", chan->name);
	} else if (chanVal == fileVal) {
		DBG("Good!  Values match for %s!!!!\n", chan->name);
	} else {
		printf("ERROR: Mismatch in values for %s.  Prsim has %d but file says %d (line number %d).\n", chan->name, chanVal, fileVal, chan->linenumExpect);
		INTERRUPT;
		//exit(1);
	}
}

//-----------------------------------------------------------------------------
// Put the value of the channel into a file.
// Returns 0 if there was a problem.
// Note that sometimes the enable will hard-reset to zero on reset.  In this
// case none of the rails will be high.  This is probably okay, but print a
// warning anyways and return -1;
static ValFromSimType channel_getValueFromSim (Prs *P, PrsChannel *chan, unsigned int *val) {
	int railval, i, b;
	int *valArray;
	int allNeutral, bitNeutral;
	railval = -1;
	// Whether none of the rails for the entire channel were high
	allNeutral = 1;
	// Whether there was a bit/quad that was valid
	bitNeutral = -1;
	

	switch (chan->type) {
		case CHAN_e1ofN:
			for (i = 0; i < chan->numDataRails; i++) {
				if (chan->dataRails[i]->val == PRS_VAL_T) {
					if (railval == -1) {
						railval = i;
						allNeutral = 0;
					} else {
						printf("ERROR: Multiple rails high for channel %s.\n", chan->name);
						INTERRUPT;
						return VAL_FROM_SIM_ERROR;
					}
				}
			}
			if (railval == -1) {
				// Don't interrupt or print an error msg yet -
				// This could happen during reset, in which
				// case this behavior is all right...

				//printf ("ERROR: %s.e is low but no data rails are high!\n", chan->name);
				//INTERRUPT;
				return VAL_FROM_SIM_NEUTRAL;
			} else {
				*val = (unsigned int)railval;
				allNeutral = 0;
				return VAL_FROM_SIM_VALID;
			}
			break;

		case CHAN_eMx1of2:
		case CHAN_eDx1of2:
			MALLOC(valArray, int, chan->size);
			for (i = 0; i < chan->size; i++) {
				if (chan->dataRails[i*2]->val == PRS_VAL_T) {
					valArray[i] = 0;
					allNeutral = 0;	
				} else if (chan->dataRails[i*2+1]->val == PRS_VAL_T) {
					valArray[i] = 1;
					allNeutral = 0;	
				} else {
					bitNeutral = i;
					//printf("ERROR: %s.e is low but neither rail in %s.b[%d] is high!\n", chan->name, i);
					// INTERRUPT;
					//return;
				}
			}
			if ((bitNeutral != -1) && (allNeutral ==0)) {
				printf("ERROR: Bit %d in %s was neutral, but other bits were valid!\n", bitNeutral, chan->name);
				exit(1);
			} else if (allNeutral == 1) {
				return VAL_FROM_SIM_NEUTRAL;
			}

			// Need to convert valArray into a real number
			{ unsigned int rv;
			convert_baseNarray2dec(valArray, chan->size, &rv, 2);
			railval = rv;
			}
			FREE(valArray);
			*val = railval;
			return VAL_FROM_SIM_VALID;
			break;

		case CHAN_eMx1of4:
			MALLOC(valArray, int, chan->size);
			for (b = 0; b < chan->size; b++) {
				valArray[b] = -1;
				for (i = 0; i < 4; i++) {
					if (chan->dataRails[b*4+i]->val == PRS_VAL_T) {
						if (valArray[b] == -1) {
							//DBG("valArray[%d] = %d\n", b, i);
							valArray[b] = i;
							allNeutral = 0;
						} else {
							printf("ERROR: Multiple rails high for channel %s.b[%d].\n", chan->name, b);
						}
					}
				}
				if (valArray[b] == -1) {
					bitNeutral = b;
					//printf("ERROR: %s.e is low but no rail in %s.b[%d] is high!\n", chan->name, b);
					// INTERRUPT;
					//return;
				}
			}

			if ((bitNeutral != -1) && (allNeutral ==0)) {
				printf("ERROR: Quad %d in %s was neutral, but other quads were valid!\n", bitNeutral, chan->name);
				exit(1);
			} else if (allNeutral == 1) {
				return VAL_FROM_SIM_NEUTRAL;
			}

			// Need to convert valArray into a real number
			{
			  unsigned int rv;
			  convert_baseNarray2dec(valArray, chan->size, &rv, 4);
			  railval = rv;
			}
			FREE(valArray);
			*val = railval;
			return VAL_FROM_SIM_VALID;
			break;
	}

}

//-----------------------------------------------------------------------------
static void channel_printChannels(Prs *P, struct Channel *C) {
	int i;
	PrsChannel *chan;
	struct Hashtable *h;
	hash_bucket_t *b;
	
	h = C->hChannels;
	for (i = 0; i < h->size; i++) {
		for (b = h->head[i]; b; b = b->next) {
			chan = (PrsChannel *)b->v;
			printf("Channel %s\n", chan->name);
		}
	}
}




//-----------------------------------------------------------------------------
// Utility functions for moving between decimal values in files and
// representations are easier to deal with internally.

// Input is a decimal value.  Fill up array bin with 1's and 0's.  bin[0] is
// value of bit0.  Zero-pad for n bits.
static void convert_dec2base2array(unsigned int dec, int *bin, int n) {
	int i;

	// Just get the lsb of dec and then shift right by one
	for (i = 0; i < n; i++) {
		bin[i] = dec & 1;
		dec = dec >> 1;
	}
}

static void convert_dec2base4array(unsigned int dec, int *quat, int n) {
	int i;

	// Just get the two least-sig bits of dec and then shift right by two
	for (i = 0; i < n; i++) {
		quat[i] = dec & 3;
		dec = dec >> 2;
	}
}

static void convert_baseNarray2dec(int *a, int length, unsigned int *dec, int base) {
	int i,j;
	int base2i;
	//DBG("Converting to base %d\n", base);
	*dec = 0;
	for (i = 0; i < length; i++) {
		//base2i = pow((double)base,(double)i);
		// Normal pow(double, double) was giving me some *really* fucked up results, so I'm just going to do this the hard way....).
		//base2i = 1;
		//for (j = 0; j < i; j++) {
			//base2i *= base;
		//}
		base2i = pow(base, i);
		//DBG("a[%d] = %d, base=%d, i=%d, base**i=%d\n", i, a[i], base, i, base2i);
		//*dec += (unsigned int) (a[i] * pow(base,i));
		*dec += (unsigned int) (a[i] * base2i);
		//DBG("dec = %u\n", *dec);
	}
}

struct prs_node_extra *prs_node_extra_init (void)
{
  struct prs_node_extra *pe;

  NEW (pe, struct prs_node_extra);
  pe->morespace = NULL;
  pe->hasChans = 0;
  pe->inVector = NULL;

  return pe;
}

void channel_checkpoint (struct Channel *C, FILE *fp)
{
  int i;
  hash_bucket_t *b;
  PrsChannel *pc;

  fprintf (fp, "%d\n", C->hChannels->n);
  for (i=0; i < C->hChannels->size; i++) {
    for (b = C->hChannels->head[i]; b; b = b->next) { 
      pc = (PrsChannel *)b->v;
      fprintf (fp, "%s ", b->key);
      if (pc->fpInject) {
	fprintf (fp, "%lu ", ftell (pc->fpInject));
      }
      if (pc->fpExpect) {
	fprintf (fp, "%lu ", ftell (pc->fpExpect));
      }
      fprintf (fp, "\n");
    }
  }
}

void channel_restore (struct Channel *C, FILE *fp)
{
  char buf[10240];
  int count;
  int i;
  PrsChannel *pc;
  unsigned long loc;

  if (fscanf (fp, "%d", &count) != 1) Assert (0, "Checkpoint read error");
  if (count != C->hChannels->n) {
    fatal_error ("Invalid number of channels in checkpoint");
  }
  for (i=0; i < count; i++) {
    if (fscanf (fp, "%s", buf) != 1) Assert (0, "Checkpoint read error");
    pc = get_channel (buf, C->hChannels);
    if (!pc) {
      fatal_error ("Channel %s doesn't exist", buf);
    }
    if (pc->fpInject) {
      if (fscanf (fp, "%lu", &loc) != 1) Assert (0, "Checkpoint read error");
      fseek (pc->fpInject, loc, SEEK_SET);
    }
    if (pc->fpExpect) {
      if (fscanf (fp, "%lu", &loc) != 1) Assert (0, "Checkpoint read error");
      fseek (pc->fpExpect, loc, SEEK_SET);
    }
  }
}
