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

#if ! defined RS690_HEADER
#define RS690_HEADER


#include "fsc2_module.h"
#include "gpib.h"
#include <float.h>


/* Include configuration information for the device */

#include "rs690.conf"


/* Uncomment the next line for debugging of data sent via GPIB bus */

/*
#define RS690_GPIB_DEBUG 1
*/


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd

#define NUM_FIXED_TIMEBASES    3        /* number of fixed time bases */

#define TIMEBASE_4_NS          0
#define TIMEBASE_8_NS          1
#define TIMEBASE_16_NS         2

#define MAX_CHANNELS           ( NUM_HSM_CARDS * 64 )  /* number of channels */

#define TS_MAX_WORDS           1024   /* maximum number of words in TS mode */

#define MAX_LOOP_REPETITIONS   4095

#define MAX_TICKS_PER_ENTRY    491520L

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


/* typedefs of structures needed in the module */

typedef struct RS690 RS690_T;
typedef struct Function Function_T;
typedef struct Channel Channel_T;
typedef struct Pulse Pulse_T;
typedef struct Phase_Setup Phase_Setup_T;
typedef struct Pulse_Params Pulse_Params_T;
typedef struct FS FS_T;
typedef struct FS_Table FS_Table_T;


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
    int pc_len;                 /* length of the phase cycle */

    Ticks delay;               /* delay for the function/channel combination */
    bool is_delay;

    bool is_inverted;          /* if set polarity is inverted */

    Ticks max_seq_len;

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
    int field;
    int bit;
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

    Phs_Seq_T *pc;           /* the pulse sequence to be used for the pulse
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

    bool left_shape_warning;
    Pulse_T *sp;             /* for normal pulses reference to related shape
                                pulse (if such exists), for shape pulses
                                reference to pulse it is associated with */

    bool left_twt_warning;
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


struct FS {
    unsigned short fields[ 4 * NUM_HSM_CARDS ];   /* data for all channels */
    Ticks pos;                                    /* position of the slice */
    Ticks len;                                    /* length of the slice */
    bool is_composite;
    FS_T *next;
};


struct FS_Table {
    int table_loops_1;
    int middle_loops;
    int table_loops_2;
};


struct RS690 {
    int device;              /* GPIB number of the device */

    Pulse_T *pulses;
    Phase_Setup_T phs[ 2 ];

    double timebase;         /* time base of the digitizer */
    bool is_timebase;
    int timebase_type;
    bool timebase_mode;      /* INTERNAL (default) or EXTERNAL */
    int timebase_level;
    bool is_timebase_level;

    int trig_in_mode;        /* EXTERNAL or INTERNAL */
    bool is_trig_in_mode;

    int trig_in_slope;
    bool is_trig_in_slope;

    int trig_in_level_type;
    bool is_trig_in_level_type;

    Ticks repeat_time;       /* only in INTERNAL mode */
    bool is_repeat_time;

    bool keep_all;           /* keep even unused pulses ? */

    Ticks neg_delay;
    bool is_neg_delay;       /* if any of the functions has a negative delay */

    Function_T function[ PULSER_CHANNEL_NUM_FUNC ];
    Channel_T channel[ MAX_CHANNELS ];

    int needed_channels;     /* number of channels that are going to be needed
                                in the experiment */

    int num_pulses;

    bool needs_update;       /* set if pulse properties have been changed in
                                test run or experiment */
    bool is_running;         /* set if the pulser is in run mode */
    bool has_been_running;

    Ticks max_seq_len;

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

    bool auto_twt_pulses;
    long left_twt_warning;

    Ticks minimum_twt_pulse_distance;
    bool is_minimum_twt_pulse_distance;
    long twt_distance_warning;

    int last_used_field;

    unsigned short default_fields[ 4 * NUM_HSM_CARDS ];

    FS_T *new_fs;
    int new_fs_count;
    FS_T *last_new_fs;
    FS_T *old_fs;
    int old_fs_count;
    FS_T *last_old_fs;

    FS_Table_T new_table,
               old_table;
};


extern RS690_T rs690;


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int rs690_init_hook(        void );
int rs690_test_hook(        void );
int rs690_end_of_test_hook( void );
int rs690_exp_hook(         void );
int rs690_end_of_exp_hook(  void );
void rs690_exit_hook(       void );


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


/* Here follow the functions from rs690_gen.c */

bool rs690_store_timebase( double /* timebase */ );

bool rs690_store_timebase_level( int /* level_type */ );

bool rs690_assign_channel_to_function( int  /* function */,
                                       long /* channel  */  );

bool rs690_set_function_delay( int    /* function */,
                               double /* delay    */  );

bool rs690_set_trigger_mode( int /* mode */ );

bool rs690_set_trig_in_slope( int /* slope */ );

bool rs690_set_trig_in_level_type( double /* type */ );

bool rs690_set_repeat_time( double /* rep_time */ );

bool rs690_invert_function( int /* function */ );

bool rs690_set_phase_reference( int /* phase    */,
                                int /* function */  );

bool rs690_phase_setup_prep( int  /* func    */,
                             int  /* type    */,
                             int  /* dummy   */,
                             long /* channel */  );

bool rs690_phase_setup( int /* func */ );

bool rs690_keep_all( void );


/* These are the functions from rs690_pulse.c */

bool rs690_new_pulse( long /* pnum */ );

bool rs690_set_pulse_function( long /* pnum     */,
                               int  /* function */  );

bool rs690_set_pulse_position( long   /* pnum   */,
                               double /* p_time */  );

bool rs690_set_pulse_length( long   /* pnum   */,
                             double /* p_time */  );

bool rs690_set_pulse_position_change( long   /* pnum   */,
                                      double /* p_time */  );

bool rs690_set_pulse_length_change( long   /* pnum   */,
                                    double /* p_time */  );

bool rs690_set_pulse_phase_cycle( long /* pnum  */,
                                  long /* cycle */  );

bool rs690_get_pulse_function( long  /* pnum     */,
                               int * /* function */  );

bool rs690_get_pulse_position( long     /* pnum   */,
                               double * /* p_time */  );

bool rs690_get_pulse_length( long     /* pnum   */,
                             double * /* p_time */  );

bool rs690_get_pulse_position_change( long     /* pnum   */,
                                      double * /* p_time */  );

bool rs690_get_pulse_length_change( long     /* pnum   */,
                                    double * /* p_time */  );

bool rs690_get_pulse_phase_cycle( long   /* pnum  */,
                                  long * /* cycle */  );

bool rs690_change_pulse_position( long   /* pnum   */,
                                  double /* p_time */  );

bool rs690_change_pulse_length( long   /* pnum   */,
                                double /* tp_ime */  );

bool rs690_change_pulse_position_change( long   /* pnum   */,
                                         double /* p_time */  );

bool rs690_change_pulse_length_change( long   /* pnum   */,
                                       double /* p_time */  );


/* Functions from rs690_init.c */

void rs690_init_setup( void );


/* Functions from rs690_util.c */

Ticks rs690_double2ticks( double /* p_time */ );

double rs690_ticks2double( Ticks /* ticks */ );

Pulse_T *rs690_get_pulse( long /* pnum */ );

const char * rs690_ptime( double /* p_time */ );

const char * rs690_pticks( Ticks /* ticks */ );

int rs690_pulse_compare( const void * /* A */,
                         const void * /* B */  );

void rs690_show_pulses( void );

void rs690_dump_pulses( void );

void rs690_dump_channels( FILE * /* fp */ );

void rs690_duty_check( void );

Ticks rs690_calc_max_length( Function_T * /* f */ );

char * rs690_num_2_channel( int /* num */ );

bool rs690_set_max_seq_len( double /* seq_len */ );


/* Functions fron rs690_run.c */

bool rs690_do_update( void );

void rs690_do_checks( Function_T * /* f */ );

void rs690_set_pulses( Function_T * /* f */ );

void rs690_full_reset( void );

Pulse_T * rs690_delete_pulse( Pulse_T * /* p    */,
                              bool      /* warn */  );

void rs690_shape_padding_check_1( Channel_T * /* ch */ );

void rs690_shape_padding_check_2( void );

void rs690_twt_padding_check( Channel_T * /* ch */ );

void rs690_seq_length_check( void );

void rs690_channel_setup( bool /* flag */ );

void rs690_cleanup_fs( void );


/* Functions from rs690_gpib.c */

bool rs690_init( const char * /* name */ );

bool rs690_run( bool /* state */ );

bool rs690_lock_state( bool /* lock */ );

bool rs690_set_channels( void );

bool rs690_command( const char * /* cmd */ );

long rs690_ch_to_num( long /* channel */ );


#endif /* ! RS690_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
