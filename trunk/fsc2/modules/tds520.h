/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#include "fsc2.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "tds520.conf"


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define TDS520_TEST_REC_LEN      500
#define TDS520_TEST_TIME_BASE    0.1
#define TDS520_TEST_SENSITIVITY  0.01
#define TDS520_TEST_NUM_AVG      16
#define TDS520_TEST_TRIG_POS     0.1
#define TDS520_TEST_TRIG_CHANNEL 0



#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  12         /* number of channel names */

#define TDS520_UNDEF -1
#define TDS520_CH1    0
#define TDS520_CH2    1
#define TDS520_MATH1  2
#define TDS520_MATH2  3
#define TDS520_MATH3  4
#define TDS520_REF1   5
#define TDS520_REF2   6
#define TDS520_REF3   7
#define TDS520_REF4   8
#define TDS520_AUX1   9         /* Auxiliary (for triggger only) */
#define TDS520_AUX2  10         /* Auxiliary (for triggger only) */
#define TDS520_LIN   11         /* Line In (for triggger only) */


/* Structure for description of a `window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct _W {
	long num;                   /* number of window                          */
	bool is_used;               /* flag, set when window has been used       */
	double start;               /* start of window (in time units)           */
	double width;               /* width of window (in time units)           */
	long start_num;				/* first point of window                     */
	long end_num;				/* last point of window                      */
	bool is_start;              /* flag, set if start of window has been set */
	bool is_width;              /* flag, set if width of window has been set */
	long num_points;            /* number of data points between the cursors */
	struct _W *next;            /* pointer to next window structure          */
	struct _W *prev;            /* pointer to previous window structure      */
} WINDOW;


typedef struct
{
	int device;

	bool is_reacting;

	double timebase;
	bool is_timebase;

	double sens[ MAX_CHANNELS ];
	double is_sens[ MAX_CHANNELS ];

	long num_avg;
	bool is_num_avg;

	WINDOW *w;               /* start element of list of windows             */
	int num_windows;

	int trigger_channel;
	bool is_trigger_channel;

	long rec_len;
	bool is_rec_len;

	double trig_pos;
	bool is_trig_pos;

	double cursor_pos;     /* current position of cursor 1                   */

	int meas_source;       /* currently selected measurements source channel */
	int data_source;       /* currently selected data source channel         */

	bool channels_in_use[ MAX_CHANNELS ];

	bool lock_state;       /* set if keyboard is locked */
} TDS520;



/* declaration of exported functions */

int tds520_init_hook( void );
int tds520_exp_hook( void );
int tds520_end_of_exp_hook( void );
void tds520_exit_hook( void );


Var *digitizer_name( Var *v );
Var *digitizer_define_window( Var *v );
Var *digitizer_timebase( Var *v );
Var *digitizer_sensitivity( Var *v );
Var *digitizer_num_averages( Var *v );
Var *digitizer_get_channel_number( Var *v );
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


/* declaration of internally used functions */

const char *tds520_ptime( double time );
void tds520_delete_windows( void );
void tds520_do_pre_exp_checks( void );

bool tds520_init( const char *name );
double tds520_get_timebase( void );
bool tds520_set_timebase( double timebase);
bool tds520_set_record_length( long num_points );
bool tds520_get_record_length( long *ret );
bool tds520_set_trigger_pos( double pos );
bool tds520_get_trigger_pos( double *ret );
long tds520_get_num_avg( void );
bool tds520_set_num_avg( long num_avg );
int tds520_get_acq_mode(void);
bool tds520_get_cursor_position( int cur_no, double *cp );
bool tds520_get_cursor_distance( double *cd );
bool tds520_set_trigger_channel( const char *name );
int tds520_get_trigger_channel( void );
void tds520_gpib_failure( void );
bool tds520_clear_SESR( void );
void tds520_finished( void );
bool tds520_set_cursor( int cur_num, double pos );
bool tds520_set_snap( bool flag );
bool tds520_display_channel( int channel );
bool tds520_set_sens( int channel, double val );
double tds520_get_sens( int channel );
bool tds520_start_aquisition( void );
double tds520_get_area( int channel, WINDOW *w, bool use_cursor );
bool tds520_get_curve( int channel, WINDOW *w, double **data, long *length,
					   bool use_cursor );
double tds520_get_amplitude( int channel, WINDOW *w, bool use_cursor );
void tds520_free_running( void );
bool tds520_lock_state( bool lock );


#ifdef TDS520_MAIN

TDS520 tds520;
const char *Channel_Names[ ] = { "CH1", "CH2", "MATH1", "MATH2", "MATH3",
								 "REF1", "REF2", "REF3", "REF4",
								 "AUX1", "AUX2", "LINE" };

#else

extern TDS520 tds520;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};
