/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#if ! defined DG2020_F_HEADER
#define DG2020_F_HEADER


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "dg2020_f.conf"


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd

#define MIN_TIMEBASE            5.0e-9  /* minimum pulser time base: 5 ns */
#define MAX_TIMEBASE            0.1     /* maximum pulser time base: 0.1 s */

#define MAX_CHANNELS            36      /* number of internal channels */


# define MIN_POD_HIGH_VOLTAGE  -2.0     /* for P3420 (Variable Output Pod) */
# define MAX_POD_HIGH_VOLTAGE   7.0

# define MIN_POD_LOW_VOLTAGE   -3.0
# define MAX_POD_LOW_VOLTAGE    6.0

#define MAX_POD_VOLTAGE_SWING   9.0
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.1

#define MAX_TRIG_IN_LEVEL       5.0
#define MIN_TRIG_IN_LEVEL      -5.0



#define MIN_BLOCK_SIZE     64     /* minimum length of a block */
#define MAX_BLOCK_REPEATS  65536  /* maximum of repeats of block in sequence */
#define MAX_PULSER_BITS    65536  /* maximum number of bits in channel */


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

typedef struct PHS PHS_T;
typedef struct Function Function_T;
typedef struct Pod Pod_T;
typedef struct Channel Channel_T;
typedef struct Pulse Pulse_T;
typedef struct Block Block_T;
typedef struct DG2020 DG2020_T;


struct PHS {                   /* needed in phase setup */
    int var[ 4 ][ 2 ];
    bool is_var[ 4 ][ 2 ];
};


struct Function {
    int self;                  /* the functions number */
    const char *name;

    bool is_used;              /* set if the function has been declared in
                                  the ASSIGNMENTS section */
    bool is_needed;            /* set if the function has been assigned
                                  pulses */

    Pod_T *pod;                /* points to the pod assigned to the function */
    Pod_T *pod2;               /* points to the second pod assigned to the
                                  function (phase functions only) */
    PHS_T phs;                 /* phase functions only: how to translate
                                  phases to pod outputs */
    bool is_phs;

    int num_channels;          /* number of channels assigned to function */
    int num_needed_channels;   /* number of channels really needed */
    Channel_T *channel[ MAX_CHANNELS ];

    int num_pulses;            /* number of pulses assigned to the function */
    int num_active_pulses;     /* number of pulses currently in use */
    Pulse_T **pulses;          /* list of pulse pointers */

    bool needs_phases;         /* set if phase cycling is needed */
    int next_phase;

    Function_T *phase_func;    /* for phase functions here is stored which
                                  function it's going to take care of while
                                  for normal functions it's a pointer to the
                                  phase function responsible for it. */
    Ticks psd;                 /* delay due to phase switches (only needed */
    bool is_psd;               /* for the phase functions) */

    long max_seq_len;          /* maximum length of the pulse sequence */

    bool is_inverted;          /* set if polarity is inverted */

    Ticks delay;               /* delay for the function/pod combination */
    bool is_delay;

    double high_level;         /* high and low voltage levels for the pod(s) */
    double low_level;          /* associated with the fucntion */

    bool is_high_level;
    bool is_low_level;

};


struct Pod {
    int self;
    Function_T *function;
};


struct Channel {
    int self;
    Function_T *function;
};


struct Pulse {
    long num;                /* number of the pulse (pulses used for realize
                                phase cycling have negative, normal pulses
                                positive numbers */
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

    Channel_T *channel;      /* channel the pulse belongs to - only needed
                                for phase pulses */

    Pulse_T *for_pulse;      /* only for phase cycling pulses: the pulse the
                                phase cycling pulse is used for */

    bool needs_update;       /* set if the pulses properties have been changed
                                in test run or experiment */
};


struct Block {
    bool is_used;
    char blk_name[ 9 ];
    Ticks start;
    long repeat;
};


struct DG2020 {
    int device;              /* GPIB number of the device */

    bool is_needed;
    bool in_setup;

    Pulse_T *pulses;
    PHS_T phs[ 2 ];

    double timebase;         /* time base of the digitizer */
    bool is_timebase;

    long phase_numbers[ 2 ];

    int trig_in_mode;        /* EXTERNAL or INTERNAL */
    int trig_in_slope;       /* only in EXTERNAL mode */
    double trig_in_level;    /* only in EXTERNAL mode */
    int trig_in_impedance;   /* only in EXTERNAL mode */
    Ticks repeat_time;       /* only in INTERNAL mode */

    bool keep_all;           /* keep even unused pulses ? */

    bool is_trig_in_mode;
    bool is_trig_in_slope;
    bool is_trig_in_level;
    bool is_trig_in_impedance;
    bool is_repeat_time;

    Ticks neg_delay;
    bool is_neg_delay;       /* if any of the functions has a negative delay */

    long max_seq_len;        /* maximum length of all pulse sequences */
    bool is_max_seq_len;

    Function_T function[ PULSER_CHANNEL_NUM_FUNC ];
    Pod_T pod[ NUM_PODS ];
    Channel_T channel[ MAX_CHANNELS ];

    int needed_channels;     /* number of channels that are going to be needed
                                for the experiment */

    bool needs_update;       /* set if pulse properties have been changed in
                                test run or experiment */
    bool is_running;         /* set if the pulser is in run mode */

    Block_T block[ 2 ];      /* blocks needed for padding */

    Ticks mem_size;          /* size of the complete sequence, i.e. including
                                the memory needed for padding */
    bool is_grace_period;
    Ticks grace_period;

    FILE *show_file;
    FILE *dump_file;
};


extern DG2020_T dg2020;


/* Here are all the directy exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int dg2020_f_init_hook(        void );
int dg2020_f_test_hook(        void );
int dg2020_f_end_of_test_hook( void );
int dg2020_f_exp_hook(         void );
int dg2020_f_end_of_exp_hook(  void );
void dg2020_f_exit_hook(       void );


Var_T *pulser_name(                   Var_T * /* v */ );
Var_T *pulser_show_pulses(            Var_T * /* v */ );
Var_T *pulser_dump_pulses(            Var_T * /* v */ );
Var_T *pulser_phase_switch_delay(     Var_T * /* v */ );
Var_T *pulser_grace_period(           Var_T * /* v */ );
Var_T *pulser_keep_all_pulses(        Var_T * /* v */ );
Var_T *pulser_maximum_pattern_length( Var_T * /* v */ );
Var_T *pulser_state(                  Var_T * /* v */ );
Var_T *pulser_channel_state(          Var_T * /* v */ );
Var_T *pulser_update(                 Var_T * /* v */ );
Var_T *pulser_shift(                  Var_T * /* v */ );
Var_T *pulser_increment(              Var_T * /* v */ );
Var_T *pulser_next_phase(             Var_T * /* v */ );
Var_T *pulser_reset(                  Var_T * /* v */ );
Var_T *pulser_phase_reset(            Var_T * /* v */ );
Var_T *pulser_pulse_reset(            Var_T * /* v */ );
Var_T *pulser_lock_keyboard(          Var_T * /* v */ );
Var_T *pulser_command(                Var_T * /* v */ );


/* Here follow the functions from dg2020_gen_f.c */

bool dg2020_store_timebase( double /* timebase */ );

bool dg2020_assign_function( int  /* function */,
                             long /* pod      */  );

bool dg2020_assign_channel_to_function( int  /* function */, 
                                        long /* channel  */  );

bool dg2020_invert_function( int /* function */ );

bool dg2020_set_function_delay( int    /* function */,
                                double /* delay    */  );

bool dg2020_set_function_high_level( int    /* function */,
                                     double /* voltage  */  );

bool dg2020_set_function_low_level( int    /* function */,
                                    double /* voltage  */  );

bool dg2020_set_trigger_mode( int /* mode */ );

bool dg2020_set_trig_in_level( double /* voltage */ );

bool dg2020_set_trig_in_slope( int /* slope */ );

bool dg2020_set_trig_in_impedance( int /* state */ );

bool dg2020_set_repeat_time( double /* rep_time */ );

bool dg2020_set_max_seq_len( double /* seq_len */ );

bool dg2020_set_phase_reference( int /* phase    */,
                                 int /* function */  );

bool dg2020_phase_setup_prep( int  /* func */,
                              int  /* type */,
                              int  /* pod  */,
                              long /* val  */  );

bool dg2020_phase_setup( int /* func */ );

bool dg2020_phase_setup_finalize( int   /* func  */,
                                  PHS_T /* p_phs */  );

bool dg2020_set_phase_switch_delay( int    /* func     */,
                                    double /* del_time */  );

bool dg2020_set_grace_period( double /* gp_time */ );

bool dg2020_keep_all( void );


/* These are the functions from dg2020_pulse_f.c */

bool dg2020_new_pulse( long /* pnum */ );

bool dg2020_set_pulse_function( long /* pnum     */,
                                int  /* function */  );

bool dg2020_set_pulse_position( long   /* pnum   */,
                                double /* p_time */  );

bool dg2020_set_pulse_length( long   /* pnum   */,
                              double /* p_time */  );

bool dg2020_set_pulse_position_change( long   /* pnum   */,
                                       double /* p_time */  );

bool dg2020_set_pulse_length_change( long   /* pnum   */,
                                     double /* p_time */  );

bool dg2020_set_pulse_phase_cycle( long /* pnum  */,
                                   long /* cycle */  );


bool dg2020_get_pulse_function( long  /* pnum     */,
                                int * /* function */  );

bool dg2020_get_pulse_position( long     /* pnum   */,
                                double * /* p_time */  );

bool dg2020_get_pulse_length( long     /* pnum   */,
                              double * /* p_time */  );

bool dg2020_get_pulse_position_change( long     /* pnum   */,
                                       double * /* p_time */  );

bool dg2020_get_pulse_length_change( long     /* pnum   */,
                                     double * /* p_time */  );

bool dg2020_get_pulse_phase_cycle( long   /* pnum  */,
                                   long * /* cycle */  );


bool dg2020_change_pulse_position( long   /* pnum   */,
                                   double /* p_time */  );

bool dg2020_change_pulse_length( long   /* pnum   */,
                                 double /* p_time */  );

bool dg2020_change_pulse_position_change( long   /* pnum   */,
                                          double /* p_time */  );

bool dg2020_change_pulse_length_change( long   /* pnum   */,
                                        double /* p_time */  );


/* Here come the functions from dg2020_util_f.c */

Ticks dg2020_double2ticks( double /* p_time */ );

double dg2020_ticks2double( Ticks /* ticks */ );

void dg2020_check_pod_level_diff( double /* high */,
                                  double /* low  */  );

Pulse_T *dg2020_get_pulse( long /* pnum */ );

const char *dg2020_ptime( double /* p_time */ );

const char *dg2020_pticks( Ticks /* ticks */ );

Channel_T *dg2020_get_next_free_channel( void );

int dg2020_start_compare( const void * /* A */,
                          const void * /* B */  );

bool dg2020_find_phase_pulse( Pulse_T *   /* p   */,
                              Pulse_T *** /* pl  */,
                              int *       /* num */  );

int dg2020_get_phase_pulse_list( Function_T * /* f       */,
                                 Channel_T *  /* channel */,
                                 Pulse_T ***  /* list    */  );

Ticks dg2020_get_max_seq_len( void );

void dg2020_calc_padding( void );

bool dg2020_prep_cmd( char ** /* cmd     */,
                      int     /* channel */,
                      Ticks   /* address */,
                      Ticks   /* length  */  );

void dg2020_set( char * /* arena  */,
                 Ticks  /* start  */,
                 Ticks  /* len    */,
                 Ticks  /* offset */  );

int dg2020_diff( char *  /* old_p  */,
                 char *  /* new_p  */,
                 Ticks * /* start  */,
                 Ticks * /* length */  );

void dg2020_dump_channels( FILE * /* fp */ );


/* Functions from dg2020_init_f.c */

void dg2020_init_setup( void );


/* Functions from dg2020_run_f.c */

bool dg2020_do_update( void );

bool dg2020_reorganize_pulses( bool /* flag */ );

void dg2020_do_checks( Function_T * /* f */ );

void dg2020_full_reset( void );

Pulse_T *dg2020_delete_pulse( Pulse_T * /* p */ );

void dg2020_reorganize_phases( Function_T * /* f    */,
                               bool         /* flag */  );

void dg2020_recalc_phase_pulse( Function_T * /* f       */,
                                Pulse_T *    /* phase_p */,
                                Pulse_T *    /* p       */,
                                int          /* nth     */,
                                bool         /* flag    */  );

void dg2020_finalize_phase_pulses( int /* func */ );

void dg2020_set_pulses( Function_T * /* f */ );

void dg2020_set_phase_pulses( Function_T * /* f */ );

void dg2020_commit( Function_T * /* f    */,
                    bool         /* flag */  );

void dg2020_commit_phases( Function_T * /* f    */,
                           bool         /* flag */  );

void dg2020_clear_padding_block( Function_T * /* f */ );


/* Finally the functions from dg2020_gpib_f.c */

bool dg2020_init( const char * /* name */ );

bool dg2020_run( bool /* flag */ );

bool dg2020_set_timebase( double /* timebase */ );

bool dg2020_set_memory_size( long /* mem_size */ );

bool dg2020_channel_assign(  int /* channel */,
                             int /* pod     */  );

bool dg2020_update_data( void );

bool dg2020_make_blocks( int       /* num_blocks */,
                         Block_T * /* block      */  );

bool dg2020_make_seq( int       /* num_blocks */,
                      Block_T * /* block      */  );

bool pulser_set_channel( int    /* channel */,
                         Ticks  /* address */,
                         Ticks  /* length  */,
                         char * /* pattern */  );

bool dg2020_set_constant( int   /* channel */,
                          Ticks /* address */,
                          Ticks /* length  */,
                          int   /* state   */  );

bool dg2020_set_pod_high_level( int    /* pod     */,
                                double /* voltage */  );

bool dg2020_set_pod_low_level( int    /* pod     */,
                               double /* voltage */  );

bool dg2020_set_trigger_in_level( double /* voltage */ );

bool dg2020_set_trigger_in_slope( int /* slope */ );

bool dg2020_set_trigger_in_impedance( int /* state */ );

void dg2020_gpib_failure( void );

bool dg2020_lock_state( bool /* lock */ );

bool dg2020_command( const char * /* cmd */ );

long dg2020_ch_to_num( long /* channel */ );


#endif /* ! DG2020_F_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
