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


#if ! defined TEGAM2714A_P_HEADER
#define TEGAM2714A_P_HEADER

#include "fsc2_module.h"


/* Include configuration information for the device */

#include "tegam2714a_p.conf"


#if DEFAULT_WAVEFORM_NUMBER < 0 || DEFAULT_WAVEFRORM_NUMER > 99
#error "DEFAULT_WAVEFORM_NUMBER in configuration file invalid, not between 0 and 99."
#endif

#if EMPTY_WAVEFORM_NUMBER < 0 || EMPTY_WAVEFORM_NUMBER > 99
#error "EMPTY_WAVEFORM_NUMBER in configuration file invalid, not between 0 and 99."
#endif

#if DEFAULT_WAVEFORM_NUMBER == EMPTY_WAVEFORM_NUMBER
#error "DEFAULT_WAVEFORM_NUMBER and EMPTY_WAVEFORM_NUMBER are set to identical values in configuration file."
#endif


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd


#define MIN_TIMEBASE           5.0e-8   /* minimum pulser time base: 50 ns */
#define MAX_TIMEBASE           0.1      /* maximum pulser time base: 0.1 s */


#define MAX_CHANNELS           100


#define MAX_AMPLITUDE          10.2


#define MAX_PULSER_BITS        131036L  /* maximum number of bits in channel */
#define MIN_PULSER_BITS        32L


#define START ( ( bool ) 1 )
#define STOP  ( ( bool ) 0 )


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )


typedef struct Pulse_Params Pulse_Params_T;
typedef struct Pulse Pulse_T;
typedef struct Function Function_T;
typedef struct TEGAM2714A_P TEGAM2714A_P_T;



struct Function {
    int self;
    const char *name;           /* name of function */
    bool is_used;               /* set if the function has been declared in
                                   the ASSIGNMENTS section */
    int channel;                /* channel to use for the waveform */
    bool is_channel;            /* set if channel for waveform has been set */

    int num_pulses;             /* number of pulses assigned to the function */
    int num_active_pulses;      /* number of pulses currenty in use */
    Pulse_T **pulses;           /* list of pulse pointers */

    Ticks max_seq_len;          /* maximum length of the pulse sequence */

    Ticks delay;                /* delay for the function/pod combination */
    bool is_delay;

    bool is_inverted;           /* if set polarity is inverted */

    double high_level;          /* high and low voltage levels of the pod(s) */
    double low_level;           /* associated with the function */

    bool is_high_level;         /* set when high or low volatge level was */
    bool is_low_level;          /* specified for the function */
};


struct Pulse {
    long num;                /* number of the pulse (automatically created
                                pulses have negative, normal pulses
                                positive numbers */
    bool is_active;          /* set if the pulse is really used */
    bool has_been_active;    /* used to find useless pulses */

    Pulse_T *next;
    Pulse_T *prev;

    Function_T *function;    /* function the pulse is associated with */

    Ticks pos;               /* current position, length, position change */
    Ticks len;               /* and length change of pulse (in units of the */
    Ticks dpos;              /* pulsers time base) */
    Ticks dlen;

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
};


struct TEGAM2714A_P {
    int device;              /* GPIB number of the device */

    bool is_needed;

    double timebase;         /* time base of the digitizer */
    bool is_timebase;

    Pulse_T *pulses;

    Ticks max_seq_len;       /* maximum length of all pulse sequences */
    bool is_max_seq_len;
    Ticks requested_max_seq_len;

    Function_T function;

    bool is_running;         /* set if the pulser is in run mode */

    FILE *show_file;
    FILE *dump_file;

    bool do_show_pulses;
    bool do_dump_pulses;

    char *old_arena;
    char *new_arena;
};


extern TEGAM2714A_P_T tegam2714a_p;



int tegam2714a_p_init_hook(        void );
int tegam2714a_p_test_hook(        void );
int tegam2714a_p_end_of_test_hook( void );
int tegam2714a_p_exp_hook(         void );
int tegam2714a_p_end_of_exp_hook(  void );
void tegam2714a_p_exit_hook(       void );


Var_T * pulser_name(                   Var_T * /* v */ );
Var_T * pulser_show_pulses(            Var_T * /* v */ );
Var_T * pulser_dump_pulses(            Var_T * /* v */ );
Var_T * pulser_state(                  Var_T * /* v */ );
Var_T * pulser_maximum_pattern_length( Var_T * /* v */ );
Var_T * pulser_update(                 Var_T * /* v */ );
Var_T * pulser_shift(                  Var_T * /* v */ );
Var_T * pulser_increment(              Var_T * /* v */ );
Var_T * pulser_reset(                  Var_T * /* v */ );
Var_T * pulser_pulse_reset(            Var_T * /* v */ );


bool tegam2714a_p_store_timebase( double /* timebase */ );

bool tegam2714a_p_assign_channel_to_function( int  /* function */,
                                              long /* channel  */  );

bool tegam2714a_p_invert_function( int /* function */ );

bool tegam2714a_p_set_function_delay( int    /* function */,
                                      double /* delay    */  );

bool tegam2714a_p_set_function_high_level( int    /* function */,
                                           double /* voltage  */  );

bool tegam2714a_p_set_function_low_level( int    /* function */,
                                          double /* voltage  */  );

bool tegam2714a_p_set_trigger_mode( int /* mode */ );

bool tegam2714a_p_set_max_seq_len( double /* seq_len */ );


bool tegam2714a_p_new_pulse( long /* pnum */ );

bool tegam2714a_p_set_pulse_function( long /* pnum     */,
                                      int  /* function */  );

bool tegam2714a_p_set_pulse_position( long   /* pnum   */,
                                      double /* p_time */  );

bool tegam2714a_p_set_pulse_length( long   /* pnum   */,
                                    double /* p_time */  );

bool tegam2714a_p_set_pulse_position_change( long   /* pnum   */,
                                             double /* p_time */  );

bool tegam2714a_p_set_pulse_length_change( long   /* pnum   */,
                                           double /* p_time */  );

bool tegam2714a_p_get_pulse_function( long  /* pnum     */,
                                      int * /* function */  );

bool tegam2714a_p_get_pulse_position( long     /* pnum   */,
                                      double * /* p_time */  );

bool tegam2714a_p_get_pulse_length( long     /* pnum   */,
                                    double * /* p_time */  );

bool tegam2714a_p_get_pulse_position_change( long     /* pnum   */,
                                             double * /* p_time */  );

bool tegam2714a_p_get_pulse_length_change( long     /* pnum   */,
                                           double * /* p_time */  );

bool tegam2714a_p_change_pulse_position( long   /* pnum   */,
                                         double /* p_time */  );

bool tegam2714a_p_change_pulse_length( long   /* pnum   */,
                                       double /* p_time */  );

bool tegam2714a_p_change_pulse_position_change( long   /* pnum   */,
                                                double /* p_time */  );

bool tegam2714a_p_change_pulse_length_change( long   /* pnum   */,
                                              double /* p_time */  );


void tegam2714a_p_init_setup( void );

void tegam2714a_p_do_update( void );

void tegam2714a_p_do_checks( bool /* in_setup */ );

void tegam2714a_p_pulse_setup( void );

void tegam2714a_p_init( const char * /* name */ );

void tegam2714a_p_run( bool /* state */ );

void tegam2714a_p_set_amplitude( double ampl );

void  tegam2714a_p_set_constant( Ticks /* start  */,
                                 Ticks /* length */,
                                 int   /* state  */  );

void tegam2714a_p_check_levels( double /* high */,
                                double /* low  */  );

const char * tegam2714a_p_ptime( double /* p_time */ );

const char * tegam2714a_p_pticks( Ticks /* ticks */ );

Ticks tegam2714a_p_double2ticks( double /* p_time */ );

double tegam2714a_p_ticks2double( Ticks /* ticks */ );

int tegam2714a_p_num_len( long /* num */ );

Pulse_T * tegam2714a_p_get_pulse( long /* pnum */ );

int tegam2714a_p_start_compare( const void * /* A */,
                                const void * /* B */  );

void tegam2714a_p_set( char * /* arena  */,
                       Ticks  /* start  */,
                       Ticks  /* len    */,
                       Ticks  /* offset */  );

int tegam2714a_p_diff( char *  /* old_p  */,
                       char *  /* new_p  */,
                       Ticks * /* start  */,
                       Ticks * /* length */  );

void tegam2714a_p_show_pulses( void );

void tegam2714a_p_dump_pulses( void );

void tegam2714a_p_write_pulses( FILE * /* fp */ );


#endif /* ! TEGAM2714A_P_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
