/*
  $Id$
*/

#if ! defined PHASES_HEADER
#define PHASES_HEADER


#include "fsc2.h"


typedef struct _PhS_ {
	long num;
	int *sequence;                   /* array of phase types */
	int len;                         /* length of array of phase types */
	struct _PhS_ *next;
} Phase_Sequence;


typedef struct {
	bool defined;                    /* is the acquisition sequence defined? */
	int *sequence;                   /* array of acquisition types */
	int len;                         /* length of array of acquisition types */
} Acquisition_Sequence;



void phases_clear( void );
void acq_seq_start( long acq_num, long acq_type );
void acq_seq_cont(  long acq_type );
Phase_Sequence * phase_seq_start( long phase_seq_num );
void phases_add_phase( Phase_Sequence *p, int phase_type );
void acq_miss_list( void );
void phase_miss_list( Phase_Sequence *p );
void phases_end( void );


#endif  /* ! PHASES_HEADER */
