/*
  $Id$
*/

#if ! defined PHASES_HEADER
#define PHASES_HEADER


#include "fsc2.h"


typedef struct {
	bool defined;                       /* is this phase sequence defined? */
	int sequence[ MAX_PHASE_SEQ_LEN ];  /* array of phase types */
	int len;                            /* length of array of phase types */
} Phase_Sequence;


typedef struct {
	bool defined;                    /* is the acquisition sequence defined? */
	int sequence[ MAX_PHASE_SEQ_LEN ];  /* array of acquisition types */
	int len;                         /* length of array of acquisition types */
} Acquisition_Sequence;



void phases_clear( void );
void acq_seq_start( long acq_num, long acq_type );
void acq_seq_cont(  long acq_type );
long phase_seq_start( long phase_seq_num );
void phases_add_phase( long phase_seq_num, int phase_type );
void acq_miss_list( void );
void phase_miss_list( long phase_seq_num );
void phases_end( void );
bool check_phase_setup( void );


#endif  /* ! PHASES_HEADER */
