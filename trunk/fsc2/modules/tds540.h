/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "tds540.conf"


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define TDS540_TEST_REC_LEN      500
#define TDS540_TEST_TIME_BASE    0.1
#define TDS540_TEST_SENSITIVITY  0.01
#define TDS540_TEST_NUM_AVG      16
#define TDS540_TEST_TRIG_POS     0.1
#define TDS540_TEST_TRIG_CHANNEL 0



#define TDS540_UNDEF -1
#define TDS540_CH1    0
#define TDS540_CH2    1
#define TDS540_CH3    2
#define TDS540_CH4    3
#define TDS540_MATH1  4
#define TDS540_MATH2  5
#define TDS540_MATH3  6
#define TDS540_REF1   7
#define TDS540_REF2   8
#define TDS540_REF3   9
#define TDS540_REF4  10
#define TDS540_AUX   11         /* Auxiliary (for triggger only) */
#define TDS540_LIN   12         /* Line In (for triggger only) */
#define MAX_CHANNELS 13


#define NUM_NORMAL_CHANNELS       ( TDS540_CH4 + 1 )
#define NUM_DISPLAYABLE_CHANNELS  ( TDS540_REF4 + 1 )
#define MAX_SIMULTANEOUS_CHANNELS 4

#define TDS540_POINTS_PER_DIV 50


/* Structure for description of a 'window' on the digitizer, made up from the
   area between a pair of cursors */

typedef struct _W {
	long num;                   /* number of window                          */
	double start;               /* start of window (in time units)			 */
	double width;               /* width of window (in time units)			 */
	long start_num;				/* first point of window                     */
	long end_num;				/* last point of window                      */
	bool is_start;              /* flag, set if start of window has been set */
	bool is_width;              /* flag, set if width of window has been set */
	long num_points;            /* number of data points between the cursors */
	struct _W *next;            /* pointer to next window structure			 */
	struct _W *prev;            /* pointer to previous window structure      */
} WINDOW;


typedef struct
{
	int device;

	bool is_reacting;

	double timebase;
	bool is_timebase;

	double sens[ NUM_NORMAL_CHANNELS ];
	double is_sens[ NUM_NORMAL_CHANNELS ];

	long num_avg;
	bool is_num_avg;

	WINDOW *w;                /* start element of list of windows */
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

	int meas_source;          /* channel selected as measurements source */
	int data_source;          /* channel selected as data source */

	bool channel_is_on[ NUM_DISPLAYABLE_CHANNELS ];
	bool channels_in_use[ NUM_DISPLAYABLE_CHANNELS ];

	bool lock_state;          /* set if keyboard is locked */
} TDS540;


enum {
	SAMPLE,
	AVERAGE
};


enum {
	GENERAL_TO_TDS540,
	TDS540_TO_GENERAL
};


#ifdef TDS540_MAIN

	TDS540 tds540;
	const char *Channel_Names[ MAX_CHANNELS ] = {
											 "CH1", "CH2", "CH3", "CH4",
											 "MATH1", "MATH2", "MATH3", "REF1",
											 "REF2", "REF3", "REF4",
											 "AUX", "LINE" };

	/* This array must be set to the available record lengths of the digitizer
	   and must always end with a 0 */

	static long record_lengths[ ] = { 500, 1000, 2500, 5000, 15000, 50000, 0 };

	/* List of all allowed time base values (in seconds) */

	static double tb[ ] = {                     500.0e-12,
							  1.0e-9,   2.0e-9,   5.0e-9,
							 10.0e-9,  20.0e-9,  50.0e-9,
							100.0e-9, 200.0e-9, 400.0e-9,
							  1.0e-6,   2.0e-6,   5.0e-6,
							 10.0e-6,  20.0e-6,  50.0e-6,
							100.0e-6, 200.0e-6, 500.0e-6,
							  1.0e-3,   2.0e-3,   5.0e-3,
							 10.0e-3,  20.0e-3,  50.0e-3,
							100.0e-3, 200.0e-3, 500.0e-3,
							  1.0,      2.0,      5.0,
							 10.0 };

	#define TB_ENTRIES ( sizeof tb / sizeof tb[ 0 ] )


	/* Maximum and minimum sensitivity settings (in V) of the measurement
	   channels*/

	static double max_sens = 1e-3,
    	          min_sens = 10.0;

#else

	extern TDS540 tds540;
	extern const char *Channel_Names[ ];

#endif

#define User_Channel_Names Channel_Names


/* Declaration of exported functions */

int tds540_init_hook( void );
int tds540_test_hook( void );
int tds540_end_of_test_hook( void );
int tds540_exp_hook( void );
int tds540_end_of_exp_hook( void );
void tds540_exit_hook( void );


Var *digitizer_name( Var *v );
Var *digitizer_define_window( Var *v );
Var *digitizer_change_window( Var *v );
Var *digitizer_window_position( Var *v );
Var *digitizer_window_width( Var *v );
Var *digitizer_display_channel( Var *v );
Var *digitizer_timebase( Var *v );
Var *digitizer_time_per_point( Var *v );
Var *digitizer_sensitivity( Var *v );
Var *digitizer_num_averages( Var *v );
Var *digitizer_record_length( Var *v );
Var *digitizer_trigger_position( Var *v );
Var *digitizer_meas_channel_ok( Var *v );
Var *digitizer_trigger_channel( Var *v );
Var *digitizer_start_acquisition( Var *v );
Var *digitizer_get_area( Var *v );
Var *digitizer_get_area_fast( Var *v );
Var *digitizer_get_curve( Var *v );
Var *digitizer_get_curve_fast( Var *v );
Var *digitizer_get_amplitude( Var *v );
Var *digitizer_get_amplitude_fast( Var *v );
Var *digitizer_run( Var *v );
Var *digitizer_lock_keyboard( Var *v );


/* Declaration of internally used functions */

const char *tds540_ptime( double p_time );
void tds540_delete_windows( TDS540 *s );
void tds540_do_pre_exp_checks( void );
void tds540_window_checks( WINDOW *w );
void tds540_set_tracking( WINDOW *w );
void tds540_set_meas_window( WINDOW *w );
void tds540_set_curve_window( WINDOW *w );
void tds540_set_window( WINDOW *w );
long tds540_translate_channel( int dir, long channel );
void tds540_store_state( TDS540 *dest, TDS540 *src );
void tds540_state_check( double timebase, long rec_len, double trig_pos );

bool tds540_init( const char *name );
double tds540_get_timebase( void );
void tds540_set_timebase( double timebase);
void tds540_set_record_length( long num_points );
long tds540_get_record_length( void );
void tds540_set_trigger_pos( double pos );
double tds540_get_trigger_pos( void );
long tds540_get_num_avg( void );
void tds540_set_num_avg( long num_avg );
int tds540_get_acq_mode( void );
double tds540_get_cursor_position( int cur_no );
double tds540_get_cursor_distance( void );
void tds540_set_trigger_channel( int channel );
int tds540_get_trigger_channel( void );
void tds540_gpib_failure( void );
void tds540_clear_SESR( void );
void tds540_finished( void );
void tds540_set_cursor( int cur_num, double pos );
void tds540_set_track_cursors( bool flag );
void tds540_set_gated_meas( bool flag );
void tds540_set_snap( bool flag );
bool tds540_display_channel_state( int channel );
void tds540_display_channel( int channel, bool on_flag );
double tds540_get_sens( int channel );
void tds540_set_sens( int channel, double val );
void tds540_start_acquisition( void );
double tds540_get_area( int channel, WINDOW *w, bool use_cursor );
void tds540_get_curve( int channel, WINDOW *w, double **data, long *length,
					   bool use_cursor );
double tds540_get_amplitude( int channel, WINDOW *w, bool use_cursor );
void tds540_free_running( void );
void tds540_lock_state( bool lock );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
