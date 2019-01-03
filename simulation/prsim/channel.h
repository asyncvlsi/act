#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include "prs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prs_channel;
typedef struct prs_channel PrsChannel;

#define CHINFO(x) ((struct prs_node_extra *)(x)->chinfo)
#define SPACE(x)  ((struct prs_node_extra *)(x)->chinfo)->morespace

// begin CLINT

struct prs_node_extra {
  unsigned int hasChans;
  void *inVector;
  void *morespace;
};

typedef enum {
	CHAN_e1ofN,
	CHAN_eMx1of2,
	CHAN_eMx1of4,
	CHAN_eDx1of2
} ChannelType;

struct prs_channel {
  hash_bucket_t *b;		/* bucket pointer */

	// Channel type
	ChannelType type;

	// Injectfile, dumpfile, and expectfile pointers
	FILE *fpInject, *fpDump, *fpExpect;
	// File names
	char *sInject, *sDump, *sExpect;
	// File line numbers
	int linenumInject, linenumDump, linenumExpect;
	
	// Whether we are doing these things for this channel
	unsigned int isInject:1, isDump:1, isExpect:1;

	// Whether to loop for any of these commands
	unsigned int loopInject:1, loopExpect:1;

	// For every channel that we're watching (dump or expect), it's okay if
	// initially the channel's enable is low but the channel is neutral.
	// Therefore, we keep track of this is the enable's first transition or
	// not...
	unsigned int isFirstEnableTransition:1;

	// Channel name
	char *name;

	// Channel's enable
	PrsNode *enable;

	// Channel's data rails
	PrsNode **dataRails;
	int numDataRails;

	// Size of the channel
	// For e1ofN, this is N.
	// for eMx1of2, eMx1of4, this is M
	int size;

};
// end CLINT

struct Channel {
  // Hashtable of channel names -> channel structs
  struct Hashtable *hChannels;

  // The Reset, required for channel stuff
  PrsNode *reset;
};

struct prs_node_extra *prs_node_extra_init (void);

void channel_checkpoint (struct Channel *C, FILE *fp);
void channel_restore (struct Channel *C, FILE *fp);

  /* Create a new channel */
void create_channel(Prs *, struct Channel *, char *, int , char *);

  /* Drive this channel with values from a file. */
void channel_injectfile(Prs *, struct Channel *, char *, char *, int );

  /* Check that values on this channel match those in a file */
void channel_expectfile(Prs *, struct Channel *, char *, char *, int );

  /* Put this channel's values into a file */ 
void channel_dumpfile(Prs *,  struct Channel *, char *, char *);

  /* Called by prsim whenever an enable switches.  Go through all of
     the channels and call appropriate functions for any channels
     associated with this enable. */
void channel_enableSwitched(Prs *, struct Channel *, PrsNode *);

void channel_resetSwitched(Prs *, struct Channel *);


#ifdef __cplusplus
}
#endif



#endif /* __CHANNEL_H__ */

