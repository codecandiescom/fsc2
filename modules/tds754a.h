/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#if ! defined TDS754A_HEADER
#define TDS754A_HEADER


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "tds754a.conf"


#define TDS754A_UNDEF  -1
#define TDS754A_CH1     0
#define TDS754A_CH2     1
#define TDS754A_CH3     2
#define TDS754A_CH4     3
#define TDS754A_MATH1   4
#define TDS754A_MATH2   5
#define TDS754A_MATH3   6
#define TDS754A_REF1    7
#define TDS754A_REF2    8
#define TDS754A_REF3    9
#define TDS754A_REF4   10
#define TDS754A_AUX    11         /* Auxiliary (for trigger only) */
#define TDS754A_LIN    12         /* Line In (for triggger only) */
#define MAX_CHANNELS   13

#define NUM_NORMAL_CHANNELS       ( TDS754A_CH4 + 1 )
#define NUM_MEAS_CHANNELS         ( TDS754A_MATH3 + 1 )
#define NUM_DISPLAYABLE_CHANNELS  ( TDS754A_REF4 + 1 )
#define MAX_SIMULTANEOUS_CHANNELS 4

#define TDS754A_POINTS_PER_DIV    50

#define CHECK_SENS_IMPEDANCE   /* Not all sensitivites are allowed for all
                                  input impedances, needs additonal checks */

/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define TDS754A_TEST_REC_LEN      500
#define TDS754A_TEST_TIME_BASE    0.1
#define TDS754A_TEST_SENSITIVITY  0.01
#define TDS754A_TEST_NUM_AVG      16
#define TDS754A_TEST_TRIG_POS     0.1
#define TDS754A_TEST_TRIG_CHANNEL TDS754A_CH1


/* Structure for description of a 'window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct Window Window_T;
typedef struct TDS754A TDS754A_T;


struct Window {
    long num;                   /* number of window                          */
    double start;               /* start of window (in time units)           */
    double width;               /* width of window (in time units)           */
    long start_num;             /* first point of window                     */
    long end_num;               /* last point of window                      */
    bool is_start;              /* flag, set if start of window has been set */
    bool is_width;              /* flag, set if width of window has been set */
    long num_points;            /* number of data points between the cursors */
    Window_T *next;             /* pointer to next window structure          */
    Window_T *prev;             /* pointer to previous window structure      */
};


struct TDS754A {
    int device;

    bool is_reacting;

    double timebase;
    bool is_timebase;

    double sens[ NUM_NORMAL_CHANNELS ];
    double is_sens[ NUM_NORMAL_CHANNELS ];

    long num_avg;
    bool is_num_avg;

    Window_T *w;              /* start element of list of windows */
    bool is_equal_width;      /* all windows have equal width -> tracking
                                 cursors can be used without further checks */
    bool gated_state;         /* Gated measurements ? */
    bool snap_state;

    int trigger_channel;
    bool is_trigger_channel;

    long rec_len;
    bool is_rec_len;

    double trig_pos;
    bool is_trig_pos;

    double cursor_pos;        /* current position of cursor 1 */

    int meas_source;          /* channel selected as measurement source */
    int data_source;          /* channel selected as data source */

    bool channel_is_on[ NUM_DISPLAYABLE_CHANNELS ];
    bool channels_in_use[ NUM_DISPLAYABLE_CHANNELS ];

    bool lock_state;          /* set if keyboard is locked */

    bool windows_are_checked;
};


enum {
    SAMPLE,
    AVERAGE
};

enum {
    GENERAL_TO_TDS754A,
    TDS754A_TO_GENERAL
};


#ifdef TDS754A_MAIN

TDS754A_T tds754a;

/* This array must be set to the available record lengths of the digitizer
   and must always end with a 0 */

static long record_lengths[ ] = { 500, 1000, 2500, 5000, 15000, 50000, 0 };

/* List of all possible time base values (in seconds) */

static double tb[ ] = {                     500.0e-12,
                          1.0e-9,   2.0e-9,   5.0e-9,
                         12.5e-9,  25.0e-9,  50.0e-9,
                        100.0e-9, 200.0e-9, 500.0e-9,
                          1.0e-6,   2.0e-6,   5.0e-6,
                         10.0e-6,  20.0e-6,  50.0e-6,
                        100.0e-6, 200.0e-6, 500.0e-6,
                          1.0e-3,   2.0e-3,   5.0e-3,
                         10.0e-3,  20.0e-3,  50.0e-3,
                        100.0e-3, 200.0e-3, 500.0e-3,
                          1.0,      2.0,      5.0,
                         10.0 };

/* Maximum and minimum sensitivity settings (in V) for the measurement
   channels.
   Take care: The minimum sensitivity of 10 V only works with 1 M Ohm input
   impedance, while for 50 Ohm the minimum sensitivity is only 1V.
   Unfortunately, this can only be tested after the digitizer is online. */

double max_sens    = 1e-3,
       min_sens_50 = 1.0,
       min_sens    = 10.0;


const char *TDS754A_Channel_Names[ MAX_CHANNELS ] =
                                          { "CH1", "CH2", "CH3", "CH4",
                                            "MATH1", "MATH2", "MATH3", "REF1",
                                            "REF2", "REF3", "REF4",
                                            "AUX", "LINE" };
#else

extern TDS754A_T tds754a;
extern const char *TDS754A_Channel_Names[ MAX_CHANNELS ];
extern double max_sens, min_sens_50, min_sens;

#endif

#define User_Channel_Names TDS754A_Channel_Names


/* Declaration of exported functions */

int tds754a_init_hook(        void );
int tds754a_test_hook(        void );
int tds754a_end_of_test_hook( void );
int tds754a_exp_hook(         void );
int tds754a_end_of_exp_hook(  void );
void tds754a_exit_hook(       void );


Var_T * digitizer_name(               Var_T * /* v */ );
Var_T * digitizer_define_window(      Var_T * /* v */ );
Var_T * digitizer_change_window(      Var_T * /* v */ );
Var_T * digitizer_window_position(    Var_T * /* v */ );
Var_T * digitizer_window_width(       Var_T * /* v */ );
Var_T * digitizer_display_channel(    Var_T * /* v */ );
Var_T * digitizer_timebase(           Var_T * /* v */ );
Var_T * digitizer_time_per_point(     Var_T * /* v */ );
Var_T * digitizer_sensitivity(        Var_T * /* v */ );
Var_T * digitizer_num_averages(       Var_T * /* v */ );
Var_T * digitizer_record_length(      Var_T * /* v */ );
Var_T * digitizer_trigger_position(   Var_T * /* v */ );
Var_T * digitizer_trigger_delay(      Var_T * /* v */ );
Var_T * digitizer_meas_channel_ok(    Var_T * /* v */ );
Var_T * digitizer_trigger_channel(    Var_T * /* v */ );
Var_T * digitizer_start_acquisition(  Var_T * /* v */ );
Var_T * digitizer_get_area(           Var_T * /* v */ );
Var_T * digitizer_get_area_fast(      Var_T * /* v */ );
Var_T * digitizer_get_curve(          Var_T * /* v */ );
Var_T * digitizer_get_curve_fast(     Var_T * /* v */ );
Var_T * digitizer_get_amplitude(      Var_T * /* v */ );
Var_T * digitizer_get_amplitude_fast( Var_T * /* v */ );
Var_T * digitizer_run(                Var_T * /* v */ );
Var_T * digitizer_lock_keyboard(      Var_T * /* v */ );
Var_T * digitizer_copy_curve(         Var_T * /* v */ );
Var_T * digitizer_command(            Var_T * /* v */ );


/* Declaration of internally used functions */

const char * tds754a_ptime( double /* p_time */ );

void tds754a_delete_windows( TDS754A_T * /* s */ );

void tds754a_do_pre_exp_checks( void );

void tds754a_window_checks( Window_T * /* w */ );

void tds754a_set_tracking( Window_T * /* w */ );

void tds754a_set_meas_window( Window_T * /* w */ );

void tds754a_set_curve_window( Window_T * /* w */ );

void tds754a_set_window( Window_T * /* w */ );

long tds754a_translate_channel( int  /* dir     */,
                                long /* channel */,
                                bool /* flag    */  );

void tds754a_store_state( TDS754A_T * /* dest */,
                          TDS754A_T * /* src  */  );

void tds754a_state_check( double /* timebase */,
                          long   /* rec_len  */,
                          double /* trig_pos */  );

Window_T * tds754a_get_window_by_number( long /* win_number */ );

bool tds754a_init( const char * /* name */ );

double tds754a_get_timebase( void );

void tds754a_set_timebase( double /* timebase */ );

void tds754a_set_record_length( long /* num_points */ );

long tds754a_get_record_length( void );

void tds754a_set_trigger_pos( double /* pos */ );

double tds754a_get_trigger_pos( void );

long tds754a_get_num_avg( void );

void tds754a_set_num_avg( long /* num_avg */ );

int tds754a_get_acq_mode( void );

double tds754a_get_cursor_position( int /* cur_no */ );

double tds754a_get_cursor_distance( void );

void tds754a_set_trigger_channel( int /* channel */ );

int tds754a_get_trigger_channel( void );

void tds754a_gpib_failure( void );

void tds754a_clear_SESR( void );

void tds754a_finished( void );

void tds754a_set_cursor( int    /* cur_num */,
                         double /* pos     */  );

void tds754a_set_track_cursors( bool /* flag */ );

void tds754a_set_gated_meas( bool /* flag */ );

void tds754a_set_snap( bool /* flag */ );

bool tds754a_display_channel_state( int /* channel */ );

void tds754a_display_channel( int  /* channel */,
                              bool /* on_flag */  );

double tds754a_get_sens( int /* channel */ );

void tds754a_set_sens( int    /* channel */,
                       double /* val     */  );

void tds754a_start_acquisition( void );

double tds754a_get_area( int        /* channel    */,
                         Window_T * /* w          */,
                         bool       /* use_cursor */  );

void tds754a_get_curve( int        /* channel    */,
                        Window_T * /* w          */,
                        double **  /* data       */,
                        long *     /* length     */,
                        bool       /* use_cursor */  );

double tds754a_get_amplitude( int        /* channel    */,
                              Window_T * /* w          */,
                              bool       /* use_cursor */  );

void tds754a_free_running( void );

void tds754a_lock_state( bool /* lock */ );

void tds754a_copy_curve( int /* src  */,
                         int /* dest */  );

bool tds754a_command( const char * /* cmd */ );


/* Unfortunately, the digitizer sometimes seems to have problems returning
   data when there's not a short delay between the write and read command.
   I found empirically that waiting 2 ms seems to get rid of the problem. */

#define gpib_read( a, b, c ) \
        ( fsc2_usleep( 2000, UNSET ), gpib_read( ( a ), ( b ), ( c ) ) )


#endif /* ! TDS754A_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
