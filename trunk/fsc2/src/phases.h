/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined PHASES_HEADER
#define PHASES_HEADER


#include "fsc2.h"

typedef struct Phs_Seq Phs_Seq_T;
typedef struct Acq_Seq Acq_Seq_T;
typedef struct PA_Seq PA_Seq_T;


struct Phs_Seq {
    long num;
    int *sequence;                   /* array of phase types */
    int len;                         /* length of array of phase types */
    Phs_Seq_T *next;
};


struct Acq_Seq {
    bool defined;                    /* is the acquisition sequence defined? */
    int *sequence;                   /* array of acquisition types */
    int len;                         /* length of array of acquisition types */
};


struct PA_Seq {
    Phs_Seq_T *phs_seq;
    Acq_Seq_T acq_seq[ 2 ];
};


void phases_clear( void );

void acq_seq_start( long /* acq_num  */,
                    long /* acq_type */  );

void acq_seq_cont(  long /* acq_type */ );

Phs_Seq_T * phase_seq_start( long /* phase_seq_num */ );

void phases_add_phase( Phs_Seq_T * /* p          */,
                       int         /* phase_type */  );

void acq_miss_list( void );

void phase_miss_list( Phs_Seq_T * /* p */ );

void phases_end( void );


#endif  /* ! PHASES_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
