/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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


#if ! defined RB_PULSER_HEADER
#define RB_PULSER_HEADER


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "rb_pulser.conf"


/* Include the Rulbus header file */

#include <rulbus.h>


#define NUM_DELAY_CARDS  10

#ifndef FIXED_TIMEBASE
#define NUM_CLOCK_CARDS   2
#else
#define NUM_CLOCK_CARDS   1
#endif


/* Define symbolic names for all of the cards the pulser is made of */

#define DELAY_CARD        0
#define CLOCK_CARD        1

#define ERT_CLOCK         0
#define TB_CLOCK          1

#define ERT_DELAY         0
#define INIT_DELAY        1
#define MW_DELAY_0        2
#define MW_DELAY_1        3
#define MW_DELAY_2        4
#define MW_DELAY_3        5
#define MW_DELAY_4        6
#define RF_DELAY          7
#define DET_DELAY_0       8
#define DET_DELAY_1       9

#define INVALID_DELAY_NUMBER -1
#define INVALID_CLOCK_NUMBER -1


/* Minimum precision (relative to the timebase) for pulse positions and
   lengths etc. - when the user requests a position or length that can't
   be set with at least this relative precision a warning is printed */

#define PRECISION         0.01


/* Definitions for dealing with pulse positions and lengths in units of
   the frequency of the clock card driving the delay card */

#define Ticks       long         /* times in units of clock cards frequency */
#define Ticks_max   l_max
#define Ticks_min   l_min
#define Ticks_rnd   lrnd
#define MAX_TICKS   RULBUS_RB8514_DELAY_CARD_MAX


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )


#define STOP        0
#define START       1


typedef struct RB_Pulser RB_Pulser_T;
typedef struct Rulbus_Clock_Card Rulbus_Clock_Card_T;
typedef struct Rulbus_Delay_Card Rulbus_Delay_Card_T;
typedef struct Function Function_T;
typedef struct Pulse Pulse_T;


struct Rulbus_Clock_Card {
	char *name;
	int handle;
	int freq;
};


struct Rulbus_Delay_Card {
	char *name;
	int handle;
	Ticks delay;
	double intr_delay;
	Rulbus_Delay_Card_T *prev;
	Rulbus_Delay_Card_T *next;
	int num_next;
	bool is_active;
	bool was_active;
	bool needs_update;
};


struct Function {
	int self;                   /* the functions number */
	const char *name;           /* name of function */
	bool is_used;
	bool is_declared;

	int num_pulses;             /* number of pulses assigned to the function */
	int num_active_pulses;
	Pulse_T **pulses;           /* list of pulse pointers */

	double last_pulse_len;

	double delay;
	bool is_delay;

	Rulbus_Delay_Card_T *delay_card;
};


struct Pulse {
	long num;                /* number of the pulse (automatically created
								pulses have negative, normal pulses
								positive numbers */
	bool is_active;          /* set if the pulse is really used */
	bool was_active;
	bool has_been_active;    /* used to find useless pulses */

	Pulse_T *next;
	Pulse_T *prev;

	Function_T *function;    /* function the pulse is associated with */

	double pos;              /* current position, length, position change */
	Ticks len;               /* and length change of pulse (in units of the */
	double dpos;             /* pulsers time base) */
	Ticks dlen;

	bool is_function;        /* flags that are set when the corresponding */
	bool is_pos;             /* property has been set */
	bool is_len;
	bool is_dpos;
	bool is_dlen;

	double initial_pos;      /* position, length, position change and length */
	Ticks initial_len;       /* change at the start of the experiment */
	double initial_dpos;
	Ticks initial_dlen;

	bool initial_is_pos;     /* property has initially been set */
	bool initial_is_len;
	bool initial_is_dpos;
	bool initial_is_dlen;

	bool needs_update;       /* set if the pulses properties have been
								changed in test run or experiment */
};


struct RB_Pulser {
	bool is_needed;

	char *config_file;

	Rulbus_Clock_Card_T clock_card[ NUM_CLOCK_CARDS ];
	Rulbus_Delay_Card_T delay_card[ NUM_DELAY_CARDS ];

	bool exists_synthesizer;

	Pulse_T *pulses;

	double timebase;         /* time base of pulse clock */
	int tb_index;            /* to be used as argument for clock card */
	bool is_timebase;

	int trig_in_mode;        /* EXTERNAL or INTERNAL */
	int trig_in_slope;       /* only in EXTERNAL mode */

	double rep_time;
	bool is_rep_time;
	int rep_time_index;      /* to be used as argument for clock card */
	Ticks rep_time_ticks;    /* only in INTERNAL mode, used for ERT card */

	bool is_trig_in_mode;
	bool is_trig_in_slope;

	double neg_delay;
	bool is_neg_delay;       /* if any of the functions has a negative delay */

	Function_T function[ PULSER_CHANNEL_NUM_FUNC ];

	bool needs_update;       /* set if pulse properties have been changed in
								test run or experiment */
	bool is_running;         /* set if the pulser is in run mode */

	char *synth_state;
	char *synth_pulse_state;
	char *synth_pulse_width;
	char *synth_pulse_delay;
	char *synth_trig_slope;

	FILE *show_file;
	FILE *dump_file;

	bool do_show_pulses;
	bool do_dump_pulses;
};


extern RB_Pulser_T rb_pulser;


/* Functions defined in rb_pulser.c */

int rb_pulser_init_hook( void );
int rb_pulser_test_hook( void );
int rb_pulser_end_of_test_hook( void );
int rb_pulser_exp_hook( void );
int rb_pulser_end_of_exp_hook( void );
void rb_pulser_exit_hook( void );

Var_T *pulser_name( Var_T *v );
Var_T *pulser_show_pulses( Var_T *v );
Var_T *pulser_dump_pulses( Var_T *v );
Var_T *pulser_state( Var_T *v );
Var_T *pulser_update( Var_T *v );
Var_T *pulser_shift( Var_T *v );
Var_T *pulser_increment( Var_T *v );
Var_T *pulser_reset( Var_T *v );
Var_T *pulser_pulse_reset( Var_T *v );
Var_T *pulser_pulse_minimum_specs( Var_T *v );
void rb_pulser_cleanup( void );


/* Functions defined in rb_pulser_gen.c */

bool rb_pulser_store_timebase( double timebase );
bool rb_pulser_set_function_delay( int function, double delay );
bool rb_pulser_set_trigger_mode( int mode );
bool rb_pulser_set_trig_in_slope( int slope );
bool rb_pulser_set_repeat_time( double rep_time );


/* Functions defined in rb_pulser_pulse.c */

bool rb_pulser_new_pulse( long pnum );
bool rb_pulser_set_pulse_function( long pnum, int function );
bool rb_pulser_set_pulse_position( long pnum, double p_time );
bool rb_pulser_set_pulse_length( long pnum, double p_time );
bool rb_pulser_set_pulse_position_change( long pnum, double p_time );
bool rb_pulser_set_pulse_length_change( long pnum, double p_time );
bool rb_pulser_get_pulse_function( long pnum, int *function );
bool rb_pulser_get_pulse_position( long pnum, double *p_time );
bool rb_pulser_get_pulse_length( long pnum, double *p_time );
bool rb_pulser_get_pulse_position_change( long pnum, double *p_time );
bool rb_pulser_get_pulse_length_change( long pnum, double *p_time );
bool rb_pulser_change_pulse_position( long pnum, double p_time );
bool rb_pulser_change_pulse_length( long pnum, double p_time );
bool rb_pulser_change_pulse_position_change( long pnum, double p_time );
bool rb_pulser_change_pulse_length_change( long pnum, double p_time );


/* Functions defined in rb_pulser_init.c */

void rb_pulser_init_setup( void );


/* Functions defined in rb_pulser_run.c */

bool rb_pulser_do_update( void );
bool rb_pulser_update_pulses( bool flag );
void rb_pulser_function_init( void );
void rb_pulser_init_delay( void );
void rb_pulser_delay_card_setup( void );
void rb_pulser_full_reset( void );
void rb_pulser_seq_length_check( void );


/* Functions defined in rb_pulser_util.c */

int rb_pulser_start_compare( const void *A, const void *B );
Ticks rb_pulser_double2ticks( double p_time );
double rb_pulser_ticks2double( Ticks ticks );
Pulse_T *rb_pulser_get_pulse( long pnum );
const char *rb_pulser_ptime( double p_time );
const char *rb_pulser_pticks( Ticks ticks );
void rb_pulser_show_pulses( void );
void rb_pulser_dump_pulses( void );
void rb_pulser_write_pulses( FILE *fp );


/* Functions defined in rb_pulser_config.l */

void rb_pulser_read_configuration( void );


/* Functions defined in rb_pulser_ll.c */

void rb_pulser_init( void );
void rb_pulser_exit( void );
void rb_pulser_run( bool state );
void rb_pulser_delay_card_state( int handle, bool state );
void rb_pulser_delay_card_delay( int handle, unsigned long delay );


#endif /* ! RB_PULSER_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
