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


#if ! defined HFS9000_HEADER
#define HFS9000_HEADER


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "hfs9000.conf"


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd

#define MIN_TIMEBASE           1.6e-9   /* minimum pulser time base: 1.6 ns */
#define MAX_TIMEBASE           2.0e-5   /* maximum pulser time base: 20 us */


#define MIN_CHANNEL           1

#if NUM_CHANNEL_CARDS == 1
#define MAX_CHANNEL            4
#define IS_NORMAL_CHANNEL( x ) ( ( x ) > 1 && ( x ) <= 4 )
#endif
#if NUM_CHANNEL_CARDS == 2
#define MAX_CHANNEL            8
#define IS_NORMAL_CHANNEL( x ) ( ( x ) > 1 && ( x ) <= 8 )
#endif
#if NUM_CHANNEL_CARDS == 3
#define MAX_CHANNEL            12
#define IS_NORMAL_CHANNEL( x ) ( ( x ) > 0 && ( x ) <= 12 )
#endif

#define CHANNEL_LETTER( x )   ( 'A' + ( ( x ) - 1 ) / 4 )
#define CHANNEL_NUMBER( x )   ( '1' + ( ( x ) - 1 ) % 4 )


#define MIN_POD_HIGH_VOLTAGE  -1.5
#define MAX_POD_HIGH_VOLTAGE   5.5

#define MIN_POD_LOW_VOLTAGE   -2.0
#define MAX_POD_LOW_VOLTAGE    5.0

#define MAX_POD_VOLTAGE_SWING   5.5
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.01

#define MAX_TRIG_IN_LEVEL       4.7
#define MIN_TRIG_IN_LEVEL      -4.7

#define MAX_PULSER_BITS       65536     /* maximum number of bits in channel */


/* Trigger Out is treated as a kind of channel with the following
   restrictions: The levels of this channel are fixed and it can't be
   inverted, there can be only one pulse and the length of the pulse is
   fixed to HFS9000_TRIG_OUT_PULSE_LEN. */

#define HFS9000_TRIG_OUT            0
#define HFS9000_TRIG_OUT_PULSE_LEN  2.0e-8  /* roughly 20 ns */


#define START ( ( bool ) 1 )
#define STOP  ( ( bool ) 0 )


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )

/* A pulse needs to be updated if either its activity state changed, its
   position changed or its length changed */

#define NEEDS_UPDATE( p ) ( ( ( p )->is_active ^ ( p )->was_active ) || \
                            ( ( p )->is_old_pos &&                      \
                              ( p )->old_pos != ( p )->pos )         || \
                            ( ( p )->is_old_len &&                      \
                              ( p )->old_len != ( p )->len ) )


/* typedefs of structures needed in the module */


typedef struct HFS9000 HFS9000_T;
typedef struct Function Function_T;
typedef struct Channel Channel_T;
typedef struct Pulse Pulse_T;


struct Function {
    int self;                  /* the functions number */
    const char *name;
    bool is_used;              /* set if the function has been declared in
                                  the ASSIGNMENTS section */
    bool is_needed;            /* set if the function has been assigned
                                  pulses */
    Channel_T *channel;        /* channel assigned to function */

    int num_pulses;            /* number of pulses assigned to the function */
    int num_active_pulses;     /* number of pulses currenty in use */
    Pulse_T **pulses;          /* list of pulse pointers */

    long max_seq_len;          /* maximum length of the pulse sequence */

    bool is_inverted;          /* if set polarity is inverted */

    Ticks delay;               /* delay for the function/channel combination */
    bool is_delay;

    double high_level;         /* high and low voltage levels for the pod(s) */
    double low_level;          /* associated with the fucntion */

    bool is_high_level;
    bool is_low_level;
};


struct Channel {
    int self;
    Function_T *function;
    bool needs_update;
    char *old_d;
    char *new_d;
    bool state;
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

    Channel_T *channel;

    bool needs_update;       /* set if the pulses properties have been changed
                                in test run or experiment */
};


struct HFS9000 {
    int device;              /* GPIB number of the device */

    bool is_needed;
    bool in_setup;

    Pulse_T *pulses;

    double timebase;         /* time base of the digitizer */
    bool is_timebase;

    int trig_in_mode;        /* EXTERNAL or INTERNAL */
    int trig_in_slope;       /* only in EXTERNAL mode */
    double trig_in_level;    /* only in EXTERNAL mode */

    bool keep_all;           /* keep even unused pulses ? */

    bool is_trig_in_mode;
    bool is_trig_in_slope;
    bool is_trig_in_level;

    Ticks neg_delay;
    bool is_neg_delay;       /* if any of the functions has a negative delay */

    long max_seq_len;        /* maximum length of all pulse sequences */
    bool is_max_seq_len;

    Function_T function[ PULSER_CHANNEL_NUM_FUNC ];
    Channel_T channel[ MAX_CHANNEL + 1 ];   /* zero is for TRIGGER_OUT ! */

    int needed_channels;     /* number of channels that are going to be needed
                                in the experiment */

    bool needs_update;       /* set if pulse properties have been changed in
                                test run or experiment */
    bool is_running;         /* set if the pulser is in run mode */
    bool has_been_running;

    Ticks mem_size;          /* size of the complete sequence, i.e. including
                                the memory needed for padding */

    bool stop_on_update;     /* if not set the pulser does not get stopped
                                while doing an update */
    FILE *dump_file;
    FILE *show_file;
};


extern HFS9000_T hfs9000;


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int hfs9000_init_hook(        void );
int hfs9000_test_hook(        void );
int hfs9000_end_of_test_hook( void );
int hfs9000_exp_hook(         void );
int hfs9000_end_of_exp_hook(  void );
void hfs9000_exit_hook(       void );


Var_T *pulser_name(                   Var_T * /* v */ );
Var_T *pulser_show_pulses(            Var_T * /* v */ );
Var_T *pulser_dump_pulses(            Var_T * /* v */ );
Var_T *pulser_keep_all_pulses(        Var_T * /* v */ );
Var_T *pulser_maximum_pattern_length( Var_T * /* v */ );
Var_T *pulser_state(                  Var_T * /* v */ );
Var_T *pulser_channel_state(          Var_T * /* v */ );
Var_T *pulser_update(                 Var_T * /* v */ );
Var_T *pulser_shift(                  Var_T * /* v */ );
Var_T *pulser_increment(              Var_T * /* v */ );
Var_T *pulser_reset(                  Var_T * /* v */ );
Var_T *pulser_pulse_reset(            Var_T * /* v */ );
Var_T *pulser_lock_keyboard(          Var_T * /* v */ );
Var_T *pulser_stop_on_update(         Var_T * /* v */ );
Var_T *pulser_command(                Var_T * /* v */ );


/* Here follow the functions from hfs9000_gen.c */

bool hfs9000_store_timebase( double timebase );
bool hfs9000_assign_channel_to_function( int function, long channel );
bool hfs9000_invert_function( int function );
bool hfs9000_set_function_delay( int function, double delay );
bool hfs9000_set_function_high_level( int function, double voltage );
bool hfs9000_set_function_low_level( int function, double voltage );
bool hfs9000_set_trigger_mode( int mode );
bool hfs9000_set_trig_in_level( double voltage );
bool hfs9000_set_trig_in_slope( int slope );
bool hfs9000_set_max_seq_len( double seq_len );
bool hfs9000_keep_all( void );


/* These are the functions from hfs9000_pulse.c */

bool hfs9000_new_pulse( long pnum );

bool hfs9000_set_pulse_function( long /* pnum     */,
                                 int  /* function */ );

bool hfs9000_set_pulse_position( long   /* pnum   */,
                                 double /* p_time */ );

bool hfs9000_set_pulse_length( long   /* pnum   */,
                               double /* p_time */ );

bool hfs9000_set_pulse_position_change( long   /* pnum   */,
                                        double /* p_time */ );

bool hfs9000_set_pulse_length_change( long   /* pnum   */,
                                      double /* p_time */ );

bool hfs9000_get_pulse_function( long  /* pnum     */,
                                 int * /* function */ );

bool hfs9000_get_pulse_position( long     /* pnum   */,
                                 double * /* p_time */ );

bool hfs9000_get_pulse_length( long     /* pnum   */,
                               double * /* p_time */ );

bool hfs9000_get_pulse_position_change( long     /* pnum   */,
                                        double * /* p_time */ );

bool hfs9000_get_pulse_length_change( long     /* pnum   */,
                                      double * /* p_time */ );

bool hfs9000_change_pulse_position( long   /* pnum   */,
                                    double /* p_time */ );

bool hfs9000_change_pulse_length( long   /* pnum   */,
                                  double /* tp_ime */ );

bool hfs9000_change_pulse_position_change( long   /* pnum   */,
                                           double /* p_time */ );

bool hfs9000_change_pulse_length_change( long   /* pnum   */,
                                         double /* p_time */ );


/* Functions from hfs9000_init.c */

void hfs9000_init_setup( void );

void hfs9000_set_pulses( Function_T * /* f */ );


/* Functions from hfs9000_util.c */

Ticks hfs9000_double2ticks( double /* p_time */ );

double hfs9000_ticks2double( Ticks /* ticks */ );

void hfs9000_check_pod_level_diff( double /* high */,
                                   double /* low  */ );

Pulse_T *hfs9000_get_pulse( long /* pnum */ );

const char *hfs9000_ptime( double /* p_time */ );

const char *hfs9000_pticks( Ticks /* ticks */ );

int hfs9000_start_compare( const void * /* A */,
                           const void * /* B */ );

Ticks hfs9000_get_max_seq_len( void );

void hfs9000_set( char * /* arena  */,
                  Ticks  /* start  */,
                  Ticks  /* len    */,
                  Ticks  /* offset */ );

int hfs9000_diff( char *  /* old_p  */,
                  char *  /* new_p  */,
                  Ticks * /* start  */,
                  Ticks * /* length */ );

void hfs9000_dump_channels( FILE * /* fp */ );


/* Functions fron hfs9000_run.c */

bool hfs9000_do_update( void );

void hfs9000_do_checks( Function_T * /* f */ );

void hfs9000_set_pulses( Function_T * /* f */ );

void hfs9000_full_reset( void );


/* Functions from hfs9000_gpib.c */

bool hfs9000_init( const char * /* name */ );

bool hfs9000_set_constant( int   /* channel */,
                           Ticks /* start   */,
                           Ticks /* length  */,
                           int   /* state   */ );

bool hfs9000_set_trig_out_pulse( void );

bool hfs9000_run( bool /* flag */ );

bool hfs9000_get_channel_state( int /* channel */ );

bool hfs9000_set_channel_state( int  /* channel */,
                                bool /* flag    */ );

bool hfs9000_command( const char * /* cmd */ );

long hfs9000_ch_to_num( long /* channel */ );

bool hfs9000_operation_complete( void );


#endif  /* ! HFS9000_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
