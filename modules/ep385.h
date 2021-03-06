/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once
#if ! defined EP385_HEADER
#define EP385_HEADER


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "ep385.conf"


/* Uncomment the next line for debugging of data sent via GPIB bus */

/*
#define EP385_GPIB_DEBUG 1
*/

/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd

#define FIXED_TIMEBASE         8.0e-9   /* fixed pulser time base: 8 ns */

#define MAX_CHANNELS            8       /* number of channels */

#define CHANNEL_OFFSET         14       /* channel numbers at the front panel
                                           range from 14 to 21 */


#define MAX_PULSER_BITS        32766    /* maximum number of bits in channel */

#define MAX_MEMORY_BLOCKS      16       /* maximum number of memory blocks */
#define BITS_PER_MEMORY_BLOCK  2048     /* length of memory block (in Ticks) */

#define MAX_REPEAT_TIMES       65535L
#define MIN_REPEAT_TIMES       10L
#define REPEAT_TICKS           12800L

#define MAX_PULSES_PER_CHANNEL   59

/* The repetition time can only be set in multiples of 102.4 us for this
   pulser and the shortest repetition time is 10 times this value while the
   longest possible one is the 65535 times it. The repetition time set this
   way is the time from the end of the pulse sequence until the start of the
   next sequence. To calculate the correct repetition rate you will have to
   add the time it takes for a pulse sequence, which, in turn is a multiple
   of 16.384 us (2048 * 8 ns), the time it takes to output a single memory
   block. */

#define REPEAT_RATE_UNITS      1.024e-4
#define MIN_REPEAT_RATE_MULT   10
#define MAX_REPEAT_RATE_MULT   65535



#define START ( ( bool ) 1 )
#define STOP  ( ( bool ) 0 )


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )

/* A pulse needs to be updated if either its activity state changed, its
   position changed or its length changed */

#define NEEDS_UPDATE( p ) (    ( ( p )->is_active ^ ( p )->was_active )  \
                            || (    ( p )->is_old_pos                    \
                                 && ( p )->old_pos != ( p )->pos )       \
                            || (    ( p )->is_old_len                    \
                                 && ( p )->old_len != ( p )->len ) )


/* typedefs of structures used in the module */

typedef struct EP385 EP385_T;
typedef struct Function Function_T;
typedef struct Channel Channel_T;
typedef struct Pulse Pulse_T;
typedef struct Phase_Setup Phase_Setup_T;
typedef struct Pulse_Params Pulse_Params_T;


struct Function {
    int self;                  /* the functions number */
    const char *name;          /* name of function */
    bool is_used;              /* set if the function has been declared in
                                  the ASSIGNMENTS section */
    bool is_needed;            /* set if the function has been assigned
                                  pulses */
    int num_channels;
    Channel_T *channel[ MAX_CHANNELS ];  /* channels assigned to function */

    int num_pulses;            /* number of pulses assigned to the function */
    Pulse_T **pulses;          /* list of pulse pointers */

    Pulse_T ***pl;             /* array of currently active pulse lists */
    Pulse_T ***pm;             /* (channel x phases) pulse list matrix */

    Phase_Setup_T *phase_setup;
    int next_phase;
    int pc_len;                /* length of the phase cycle */

    Ticks delay;               /* delay for the function/channel combination */
    bool is_delay;

    long max_seq_len;          /* maximum length of the pulse sequence */

    bool uses_auto_shape_pulses;
    Ticks left_shape_padding;
    Ticks right_shape_padding;
    Ticks min_left_shape_padding;
    Ticks min_right_shape_padding;

    bool uses_auto_twt_pulses;
    Ticks left_twt_padding;
    Ticks right_twt_padding;
    Ticks min_left_twt_padding;
    Ticks min_right_twt_padding;

    long max_duty_warning;   /* number of times TWT duty cycle was exceeded */
};


struct Pulse_Params {
    Ticks pos;
    Ticks len;
    Pulse_T *pulse;
};


struct Channel {
    int self;
    Function_T *function;
    bool needs_update;
    int num_pulses;
    int num_active_pulses;
    int old_num_active_pulses;
    Pulse_Params_T *pulse_params;
    Pulse_Params_T *old_pulse_params;
};


struct Pulse {
    long num;                /* (positive) number of the pulse */

    bool is_active;          /* set if the pulse is really used */
    bool was_active;
    bool has_been_active;    /* used to find useless pulses */

    Pulse_T *next;
    Pulse_T *prev;

    Function_T *function;    /* function the pulse is associated with */

    Ticks pos;               /* current position, length, position change */
    Ticks len;               /* and length change of pulse (in units of the */
    Ticks dpos;              /* pulsers time base) */
    Ticks dlen;

    Phs_Seq_T *pc;          /* the pulse sequence to be used for the pulse
                                (or NULL if none is to be used) */
    bool is_function;        /* flags that are set when the corresponding */
    bool is_pos;             /* property has been set */
    bool is_len;
    bool is_dpos;
    bool is_dlen;

    Ticks initial_pos;       /* position, length, position change and length */
    Ticks initial_len;       /* change at the start of the experiment */
    Ticks initial_dpos;
    Ticks initial_dlen;

    bool initial_is_pos;     /* property has initially been set */
    bool initial_is_len;
    bool initial_is_dpos;
    bool initial_is_dlen;

    Ticks old_pos;           /* position and length of pulse before a change */
    Ticks old_len;           /* is applied */

    bool is_old_pos;
    bool is_old_len;

    bool needs_update;       /* set if the pulses properties have been changed
                                in test run or experiment */

    bool left_shape_warning; /* stores if for pulse the left or right shape */
    bool right_shape_warning;/* padding couldn't be set correctly */
    Pulse_T *sp;             /* for normal pulses reference to related shape
                                pulse (if such exists), for shape pulses
                                reference to pulse it is associated with */

    bool left_twt_warning;   /* stores if for pulse the left or right TWT */
    bool right_twt_warning;  /* padding couldn't be set correctly */
    Pulse_T *tp;             /* for normal pulses reference to related TWT
                                pulse (if such exists), for TWT pulses
                                reference to pulse it is associated with */
};


struct Phase_Setup {
    bool is_defined;
    bool is_set[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
    bool is_needed[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
    Channel_T *channels[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
    Function_T *function;
};


struct EP385 {
    int device;              /* GPIB number of the device */

    bool is_needed;

    Pulse_T *pulses;         /* list of pulses */
    Phase_Setup_T phs[ 2 ];  /* phase settings */

    double timebase;         /* time base of the digitizer */
    bool is_timebase;
    int timebase_mode;      /* INTERNAL (default) or EXTERNAL */

    int trig_in_mode;        /* EXTERNAL or INTERNAL */
    bool is_trig_in_mode;

    bool is_repeat_time;     /* is a repeat time set? */
    Ticks repeat_time;       /* (only in INTERNAL mode) */

    bool keep_all;           /* keep even unused pulses ? */

    bool is_neg_delay;       /* has any of the functions a negative delay? */
    Ticks neg_delay;         /* amount of negative delay */

    Function_T function[ PULSER_CHANNEL_NUM_FUNC ];
    Channel_T channel[ MAX_CHANNELS ];

    int needed_channels;     /* number of channels that are going to be needed
                                in the experiment */

    bool in_setup;

    bool needs_update;       /* set if pulse properties have been changed in
                                test run or experiment */
    bool is_running;         /* set if the pulser is in run mode */
    bool has_been_running;

    Ticks shape_2_defense;
    bool is_shape_2_defense;
    long shape_2_defense_too_near;

    Ticks defense_2_shape;
    bool is_defense_2_shape;
    long defense_2_shape_too_near;

    bool is_confirmation;

    FILE *show_file;
    FILE *dump_file;

    bool do_show_pulses;
    bool do_dump_pulses;

    bool auto_shape_pulses;
    long left_shape_warning;
    long right_shape_warning;

    bool auto_twt_pulses;
    long left_twt_warning;
    long right_twt_warning;

    Ticks minimum_twt_pulse_distance;
    bool is_minimum_twt_pulse_distance;
    long twt_distance_warning;
};


extern EP385_T  ep385;


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int ep385_init_hook(        void );
int ep385_test_hook(        void );
int ep385_end_of_test_hook( void );
int ep385_exp_hook(         void );
int ep385_end_of_exp_hook(  void );
void ep385_exit_hook(       void );


Var_T * pulser_name(                              Var_T * /* v */ );
Var_T * pulser_automatic_shape_pulses(            Var_T * /* v */ );
Var_T * pulser_automatic_twt_pulses(              Var_T * /* v */ );
Var_T * pulser_show_pulses(                       Var_T * /* v */ );
Var_T * pulser_dump_pulses(                       Var_T * /* v */ );
Var_T * pulser_shape_to_defense_minimum_distance( Var_T * /* v */ );
Var_T * pulser_defense_to_shape_minimum_distance( Var_T * /* v */ );
Var_T * pulser_minimum_twt_pulse_distance(        Var_T * /* v */ );
Var_T * pulser_state(                             Var_T * /* v */ );
Var_T * pulser_channel_state(                     Var_T * /* v */ );
Var_T * pulser_update(                            Var_T * /* v */ );
Var_T * pulser_shift(                             Var_T * /* v */ );
Var_T * pulser_increment(                         Var_T * /* v */ );
Var_T * pulser_reset(                             Var_T * /* v */ );
Var_T * pulser_pulse_reset(                       Var_T * /* v */ );
Var_T * pulser_next_phase(                        Var_T * /* v */ );
Var_T * pulser_phase_reset(                       Var_T * /* v */ );
Var_T * pulser_lock_keyboard(                     Var_T * /* v */ );
Var_T * pulser_command(                           Var_T * /* v */ );
Var_T * pulser_maximum_pattern_length(            Var_T * /* v */ );


/* Here follow the functions from ep385_gen.c */

bool ep385_store_timebase( double /* timebase */ );

bool ep385_assign_channel_to_function( int  /* function */,
                                       long /* channel  */  );

bool ep385_set_function_delay( int    /* function */,
                               double /* delay    */  );

bool ep385_set_trigger_mode( int /* mode */ );

bool ep385_set_repeat_time( double /* rep_time */ );

bool ep385_set_phase_reference( int /* phase    */,
                                int /* function */  );

bool ep385_phase_setup_prep( int  /* func    */,
                             int  /* type    */,
                             int  /* dummy   */,
                             long /* channel */  );

bool ep385_phase_setup( int /* func */ );

bool ep385_keep_all( void );


/* These are the functions from ep385_pulse.c */

bool ep385_new_pulse( long /* pnum */ );

bool ep385_set_pulse_function( long /* pnum     */,
                               int  /* function */  );

bool ep385_set_pulse_position( long   /* pnum   */,
                               double /* p_time */  );

bool ep385_set_pulse_length( long   /* pnum   */,
                             double /* p_time */  );

bool ep385_set_pulse_position_change( long   /* pnum   */,
                                      double /* p_time */  );

bool ep385_set_pulse_length_change( long   /* pnum   */,
                                    double /* p_time */  );

bool ep385_set_pulse_phase_cycle( long /* pnum  */,
                                  long /* cycle */  );

bool ep385_get_pulse_function( long  /* pnum     */,
                               int * /* function */  );

bool ep385_get_pulse_position( long     /* pnum   */,
                               double * /* p_time */  );

bool ep385_get_pulse_length( long     /* pnum   */,
                             double * /* p_time */  );

bool ep385_get_pulse_position_change( long     /* pnum   */,
                                      double * /* p_time */  );

bool ep385_get_pulse_length_change( long     /* pnum   */,
                                    double * /* p_time */  );

bool ep385_get_pulse_phase_cycle( long   /* pnum  */,
                                  long * /* cycle */  );

bool ep385_change_pulse_position( long   /* pnum   */,
                                  double /* p_time */  );

bool ep385_change_pulse_length( long   /* pnum   */,
                                double /* tp_ime */  );

bool ep385_change_pulse_position_change( long   /* pnum   */,
                                         double /* p_time */  );

bool ep385_change_pulse_length_change( long   /* pnum   */,
                                       double /* p_time */  );


/* Functions from ep385_init.c */

void ep385_init_setup( void );


/* Functions from ep385_util.c */

void ep385_timebase_check( void );

Ticks ep385_double2ticks( double /* p_time */ );

Ticks ep385_double2ticks_simple( double /* p_time */ );

double ep385_ticks2double( Ticks /* ticks */ );

Pulse_T *ep385_get_pulse( long /* pnum */ );

const char *ep385_ptime( double /* p_time */ );

const char *ep385_pticks( Ticks /* ticks */ );

int ep385_pulse_compare( const void * /* A */,
                         const void * /* B */  );

void ep385_show_pulses( void );

void ep385_dump_pulses( void );

void ep385_dump_channels( FILE * /* fp */ );

void ep385_duty_check( void );

Ticks ep385_calc_max_length( Function_T * /* f */ );

bool ep385_set_max_seq_len( double /* seq_len */ );


/* Functions fron ep385_run.c */

bool ep385_do_update( void );

void ep385_do_checks( Function_T * /* f */ );

void ep385_set_pulses( Function_T * /* f */ );

void ep385_full_reset( void );

Pulse_T * ep385_delete_pulse( Pulse_T * /* p    */,
                              bool      /* warn */  );

void ep385_shape_padding_check_1( Channel_T * /* ch */ );

void ep385_shape_padding_check_2( void );

void ep385_twt_padding_check( Channel_T * /* ch */ );


/* Functions from ep385_gpib.c */

bool ep385_init( const char * /* name */ );

bool ep385_run( bool /* state */ );

bool ep385_set_channels( void );

bool ep385_command( const char * /* cmd */ );


#endif /* ! EP385_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
