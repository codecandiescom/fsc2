/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
