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


#if ! defined RB_PULSER_W_HEADER
#define RB_PULSER_W_HEADER


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "rb_pulser_w.conf"


/* Include the Rulbus header file */

#include <rulbus.h>


/* Uncomment the following line to have the pulser run in test mode, i.e.
   instead of outputting pulses have a line printed to stderr for each
   call for setting a deleay or clock card. */

#if 0
#define RB_PULSER_W_TEST
#endif


/* Defines for the number of delay and clock cards */

#define NUM_DELAY_CARDS  15

#if defined FIXED_TIMEBASE
#define NUM_CLOCK_CARDS   1
#else
#define NUM_CLOCK_CARDS   3
#endif


/* Define symbolic names for all of the cards the pulser is made of */

#define DELAY_CARD        0
#define CLOCK_CARD        1

#define ERT_CLOCK         0
#define TB_CLOCK_1        1
#define TB_CLOCK_2        2

#define ERT_DELAY         0
#define MW_DELAY_0        1      /* delay before 1st MW pulse */
#define MW_DELAY_1        2      /* creates the first MW pulse */
#define MW_DELAY_2        3      /* delay between 1st and 2nd MW pulse */
#define MW_DELAY_3        4      /* creates 2nd MW pulse */
#define MW_DELAY_4        5      /* delay between 2nd and 3rd MW pulse */
#define MW_DELAY_5        6      /* creates 3rd MW pulse */
#define PHASE_DELAY_0     7
#define PHASE_DELAY_1     8
#define PHASE_DELAY_2     9
#define DEFENSE_DELAY    10
#define RF_DELAY         11
#define LASER_DELAY_0    12
#define LASER_DELAY_1    13
#define DET_DELAY        14

#define INVALID_DELAY_NUMBER -1
#define INVALID_CLOCK_NUMBER -1

#define MAX_MW_PULSES         3


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
#define Ticks_ceil(  x )  Ticks_rnd( ceil(  x ) )
#define Ticks_floor( x )  Ticks_rnd( floor( x ) )


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )


#define STOP        0
#define START       1


/* The delay of the "initial delay" card may never be zero since the pulse
   created by it triggers the detection delay card. A minimum delay (in
   Ticks) must be set here). */

#define INIT_DELAY_MINIMUM_DELAY_TICKS    2   /* typically 20 ns */


typedef struct RB_Pulser_W RB_Pulser_W_T;
typedef struct Rulbus_Clock_Card Rulbus_Clock_Card_T;
typedef struct Rulbus_Delay_Card Rulbus_Delay_Card_T;
typedef struct Function Function_T;
typedef struct Pulse Pulse_T;
typedef struct PHS PHS_T;


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

    int old_num_active_pulses;  /* only used for RF pulses */
    double old_delay;           /* only used for RF pulses */

    Pulse_T **pulses;           /* list of pulse pointers */

    double last_pulse_len;

    double delay;
    bool is_delay;

    Rulbus_Delay_Card_T *delay_card;
};


struct Pulse {
    long num;                /* number of the pulse (automatically created
                                pulses have negative wile normal pulses have
                                positive numbers */
    bool is_active;          /* set if the pulse is really used */
    bool was_active;
    bool has_been_active;    /* used to find useless pulses */

    Pulse_T *next;
    Pulse_T *prev;

    Function_T *function;    /* function the pulse is associated with */

    Phs_Seq_T *pc;           /* the pulse sequence to be used for the pulse
                                (or NULL if none is to be used) */

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


struct RB_Pulser_W {
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
    char *synth_double_mode;
    char *synth_double_delay;

    bool needs_phases;       /* set if phase cycling (of microwave pulses)
                                is needed */
    int cur_phase;           /* Index of current phase in phase cycle */
    int pc_len;              /* length of the phase cycle */

    bool is_pulse_2_defense;
    double pulse_2_defense;  /* minimum delay between end of last microwave
                                pulse and end of defense pulse */
    bool defense_pulse_mode; /* tells if defense pulses gets set */
    bool is_psd;
    double psd;              /* minimum delay between start of phase pulse and
                                start of microwave pulse */
    bool is_grace_period;
    double grace_period;     /* minimum delay between end of microwave pulse
                                and start of next phase pulse */
    FILE *show_file;
    FILE *dump_file;

    bool do_show_pulses;
    bool do_dump_pulses;
};


extern RB_Pulser_W_T rb_pulser_w;


/* Functions defined in rb_pulser_w.c */

int rb_pulser_w_init_hook(        void );
int rb_pulser_w_test_hook(        void );
int rb_pulser_w_end_of_test_hook( void );
int rb_pulser_w_exp_hook(         void );
int rb_pulser_w_end_of_exp_hook(  void );
void rb_pulser_w_exit_hook(       void );

Var_T * pulser_name(                     Var_T * /* v */ );
Var_T * pulser_show_pulses(              Var_T * /* v */ );
Var_T * pulser_dump_pulses(              Var_T * /* v */ );
Var_T * pulser_phase_switch_delay(       Var_T * /* v */ );
Var_T * pulser_grace_period(             Var_T * /* v */ );
Var_T * pulser_minimum_defense_distance( Var_T * /* v */ );
Var_T * pulser_defense_pulse_mode(       Var_T * /* v */ );
Var_T * pulser_state(                    Var_T * /* v */ );
Var_T * pulser_update(                   Var_T * /* v */ );
Var_T * pulser_shift(                    Var_T * /* v */ );
Var_T * pulser_increment(                Var_T * /* v */ );
Var_T * pulser_next_phase(               Var_T * /* v */ );
Var_T * pulser_phase_reset(              Var_T * /* v */ );
Var_T * pulser_reset(                    Var_T * /* v */ );
Var_T * pulser_pulse_reset(              Var_T * /* v */ );
Var_T * pulser_pulse_minimum_specs(      Var_T * /* v */ );

void rb_pulser_w_cleanup( void );

/* Functions defined in rb_pulser_w_gen.c */

bool rb_pulser_w_store_timebase( double /* timebase */ );

bool rb_pulser_w_set_function_delay( int    /* function */,
                                     double /* delay    */  );

bool rb_pulser_w_set_trigger_mode( int /* mode */ );

bool rb_pulser_w_set_trig_in_slope( int /* slope */ );

bool rb_pulser_w_set_repeat_time( double /* rep_time */ );

bool rb_pulser_w_set_phase_switch_delay( int    /* dummy    */,
                                         double /* del_time */  );

bool rb_pulser_w_set_grace_period( double /* gp_time */ );


/* Functions defined in rb_pulser_w_pulse.c */

bool rb_pulser_w_new_pulse( long /* pnum */ );

bool rb_pulser_w_set_pulse_function( long /* pnum     */,
                                     int  /* function */  );

bool rb_pulser_w_set_pulse_position( long   /* pnum   */,
                                     double /* p_time */  );

bool rb_pulser_w_set_pulse_length( long   /* pnum   */,
                                   double /* p_time */  );

bool rb_pulser_w_set_pulse_position_change( long   /* pnum   */,
                                            double /* p_time */  );

bool rb_pulser_w_set_pulse_length_change( long   /* pnum   */,
                                          double /* p_time */  );

bool rb_pulser_w_set_pulse_phase_cycle( long /* pnum  */,
                                        long /* cycle */  );

bool rb_pulser_w_get_pulse_function( long  /* pnum     */,
                                     int * /* function */  );

bool rb_pulser_w_get_pulse_position( long     /* pnum   */,
                                     double * /* p_time */  );

bool rb_pulser_w_get_pulse_length( long     /* pnum   */,
                                   double * /* p_time */  );

bool rb_pulser_w_get_pulse_position_change( long     /* pnum   */,
                                            double * /* p_time */  );

bool rb_pulser_w_get_pulse_length_change( long     /* pnum   */,
                                          double * /* p_time */  );

bool rb_pulser_w_get_pulse_phase_cycle( long   /* pnum  */,
                                        long * /* cycle */  );

bool rb_pulser_w_change_pulse_position( long   /* pnum   */,
                                        double /* p_time */  );

bool rb_pulser_w_change_pulse_length( long   /* pnum   */,
                                      double /* p_time */  );

bool rb_pulser_w_change_pulse_position_change( long   /* pnum   */,
                                               double /* p_time */  );

bool rb_pulser_w_change_pulse_length_change( long   /* pnum   */,
                                             double /* p_time */  );


/* Functions defined in rb_pulser_w_init.c */

void rb_pulser_w_init_setup( void );


/* Functions defined in rb_pulser_w_run.c */

void rb_pulser_w_do_update( void );

void rb_pulser_w_update_pulses( bool /* flag */ );

void rb_pulser_w_full_reset( void );


/* Functions defined in rb_pulser_w_util.c */

int rb_pulser_w_start_compare( const void * /* A */,
                               const void * /* B */  );

Ticks rb_pulser_w_double2ticks( double /* p_time */ );

double rb_pulser_w_ticks2double( Ticks /* ticks */ );

Pulse_T * rb_pulser_w_get_pulse( long /* pnum */ );

const char * rb_pulser_w_ptime( double /* p_time */ );

const char * rb_pulser_w_pticks( Ticks /* ticks */ );

void rb_pulser_w_start_show_pulses( void );

void rb_pulser_w_open_dump_file( void );

void rb_pulser_w_write_pulses( FILE * /* fp */ );

double rb_pulser_mw_min_specs( Pulse_T * /* p */ );

double rb_pulser_rf_min_specs( Pulse_T * /* p */ );

double rb_pulser_laser_min_specs( Pulse_T * /* p */ );

double rb_pulser_det_min_specs( Pulse_T * /* p */ );


/* Functions defined in rb_pulser_w_config.l */

void rb_pulser_w_read_configuration( void );


/* Functions defined in rb_pulser_w_ll.c */

void rb_pulser_w_init( void );

void rb_pulser_w_exit( void );

void rb_pulser_w_run( bool /* state */ );

void rb_pulser_w_delay_card_state( Rulbus_Delay_Card_T * /* card  */,
                                   bool                  /* state */  );

void rb_pulser_w_delay_card_delay( Rulbus_Delay_Card_T * /* card  */,
                                   unsigned long         /* delay */,
                                   bool                  /* force */ );

void rb_pulser_w_set_phase( Rulbus_Delay_Card_T * /* card  */,
                            int                   /* phase */  );


#endif /* ! RB_PULSER_W_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
