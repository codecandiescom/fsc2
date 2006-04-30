/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#if ! defined RB_PULSER_J_HEADER
#define RB_PULSER_J_HEADER


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "rb_pulser_j.conf"


/* Include the Rulbus header file */

#include <rulbus.h>


/* Uncomment the following line to have the pulser run in test mode, i.e.
   instead of outputting pulses have a line printed to stderr for each
   call for setting a deleay or clock card. */

#if 0
#define RB_PULSER_J_TEST
#endif


/* Defines for the number of delay and clock cards */

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

#define MAX_MW_PULSES     3


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


/* The delay of the "initial delay" card may never be zero since the pulse
   created by it triggers the detection delay card. A minimum delay (in
   Ticks) must be set here). */

#define INIT_DELAY_MINIMUM_DELAY_TICKS    2   /* typically 20 ns */


typedef struct RB_Pulser_J RB_Pulser_J_T;
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
	Ticks old_delay;
	double intr_delay;
	Rulbus_Delay_Card_T *prev;
	Rulbus_Delay_Card_T *next;
	int num_next;
	bool is_active;
	bool was_active;
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

	bool is_pos;             /* flags that are set when the corresponding */
	bool is_len;             /* property has been set */
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
};


struct RB_Pulser_J {
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


extern RB_Pulser_J_T rb_pulser_j;


/* Functions defined in rb_pulser_j.c */

int rb_pulser_j_init_hook(        void );
int rb_pulser_j_test_hook(        void );
int rb_pulser_j_end_of_test_hook( void );
int rb_pulser_j_exp_hook(         void );
int rb_pulser_j_end_of_exp_hook(  void );
void rb_pulser_j_exit_hook(       void );

Var_T *pulser_name(                Var_T * /* v */ );
Var_T *pulser_show_pulses(         Var_T * /* v */ );
Var_T *pulser_dump_pulses(         Var_T * /* v */ );
Var_T *pulser_state(               Var_T * /* v */ );
Var_T *pulser_update(              Var_T * /* v */ );
Var_T *pulser_shift(               Var_T * /* v */ );
Var_T *pulser_increment(           Var_T * /* v */ );
Var_T *pulser_reset(               Var_T * /* v */ );
Var_T *pulser_pulse_reset(         Var_T * /* v */ );
Var_T *pulser_pulse_minimum_specs( Var_T * /* v */ );

void rb_pulser_j_cleanup( void );

/* Functions defined in rb_pulser_j_gen.c */

bool rb_pulser_j_store_timebase( double /* timebase */ );

bool rb_pulser_j_set_function_delay( int    /* function */,
									 double /* delay    */ );

bool rb_pulser_j_set_trigger_mode( int /* mode */ );

bool rb_pulser_j_set_trig_in_slope( int /* slope */ );

bool rb_pulser_j_set_repeat_time( double /* rep_time */ );


/* Functions defined in rb_pulser_j_pulse.c */

bool rb_pulser_j_new_pulse( long /* pnum */ );

bool rb_pulser_j_set_pulse_function( long /* pnum     */,
									 int  /* function */ );

bool rb_pulser_j_set_pulse_position( long   /* pnum   */,
									 double /* p_time */ );

bool rb_pulser_j_set_pulse_length( long   /* pnum   */,
								   double /* p_time */ );

bool rb_pulser_j_set_pulse_position_change( long   /* pnum   */,
											double /* p_time */ );

bool rb_pulser_j_set_pulse_length_change( long   /* pnum   */,
										  double /* p_time */ );

bool rb_pulser_j_get_pulse_function( long  /* pnum     */,
									 int * /* function */ );

bool rb_pulser_j_get_pulse_position( long     /* pnum   */,
									 double * /* p_time */ );

bool rb_pulser_j_get_pulse_length( long     /* pnum   */,
								   double * /* p_time */ );

bool rb_pulser_j_get_pulse_position_change( long     /* pnum   */,
											double * /* p_time */ );

bool rb_pulser_j_get_pulse_length_change( long     /* pnum   */,
										  double * /* p_time */ );

bool rb_pulser_j_change_pulse_position( long   /* pnum   */,
										double /* p_time */ );

bool rb_pulser_j_change_pulse_length( long   /* pnum   */,
									  double /* p_time */ );

bool rb_pulser_j_change_pulse_position_change( long   /* pnum   */,
											   double /* p_time */ );

bool rb_pulser_j_change_pulse_length_change( long   /* pnum   */,
											 double /* p_time */ );


/* Functions defined in rb_pulser_j_init.c */

void rb_pulser_j_init_setup( void );


/* Functions defined in rb_pulser_j_run.c */

void rb_pulser_j_do_update( void );

void rb_pulser_j_update_pulses( bool /* flag */ );

void rb_pulser_j_full_reset( void );


/* Functions defined in rb_pulser_j_util.c */

int rb_pulser_j_start_compare( const void * /* A */,
							   const void * /* B */ );

Ticks rb_pulser_j_double2ticks( double /* p_time */ );

double rb_pulser_j_ticks2double( Ticks /* ticks */ );

Pulse_T *rb_pulser_j_get_pulse( long /* pnum */ );

const char *rb_pulser_j_ptime( double /* p_time */ );

const char *rb_pulser_j_pticks( Ticks /* ticks */ );

void rb_pulser_j_show_pulses( void );

void rb_pulser_j_dump_pulses( void );

void rb_pulser_j_write_pulses( FILE * /* fp */ );


/* Functions defined in rb_pulser_j_config.l */

void rb_pulser_j_read_configuration( void );


/* Functions defined in rb_pulser_j_ll.c */

void rb_pulser_j_init( void );

void rb_pulser_j_exit( void );

void rb_pulser_j_run( bool /* state */ );

void rb_pulser_j_delay_card_state( int  /* handle */,
								   bool /* state  */ );

void rb_pulser_j_delay_card_delay( int           /* handle */,
								   unsigned long /* delay  */ );


#endif /* ! RB_PULSER_J_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
