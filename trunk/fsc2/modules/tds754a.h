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



#define DEVICE_NAME "TDS754A"      /* compare entry in /etc/gpib.conf ! */

#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  13         /* number of channel names */

#define TDS754A_UNDEF -1
#define TDS754A_CH1    0
#define TDS754A_CH2    1
#define TDS754A_CH3    2
#define TDS754A_CH4    3
#define TDS754A_MATH1  4
#define TDS754A_MATH2  5
#define TDS754A_MATH3  6
#define TDS754A_REF1   7
#define TDS754A_REF2   8
#define TDS754A_REF3   9
#define TDS754A_REF4  10
#define TDS754A_AUX   11         /* Auxiliary (for triggger only) */
#define TDS754A_LIN   12         /* Line In (for triggger only) */



/* Structure for description of a `window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct _W {
	long num;                   /* number of window                          */
	bool is_used;               /* flag, set when window has been used		 */
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

	double sens[ MAX_CHANNELS ];
	double is_sens[ MAX_CHANNELS ];

	long num_avg;
	bool is_num_avg;

	WINDOW *w;                // start element of list of windows
	int num_windows;
	bool is_equal_width;      // all windows have equal width -> tracking
	                          // cursors can be used without further checking
	bool gated_state;         // Gated measurements ?
	bool snap_state;

	int trigger_channel;
	bool is_trigger_channel;

	long rec_len;
	bool is_rec_len;

	double trig_pos;
	bool is_trig_pos;

	double cursor_pos;        // current position of cursor 1

	int meas_source;          // currently selected measurements source channel
	int data_source;          // currently selected data source channel

	bool channels_in_use[ MAX_CHANNELS ];

	bool lock_state;          // set if keyboard is locked
} TDS754A;



/* declaration of exported functions */

int tds754a_init_hook( void );
int tds754a_exp_hook( void );
int tds754a_end_of_exp_hook( void );
void tds754a_exit_hook( void );


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

const char *tds754a_ptime( double time );
void tds754a_delete_windows( void );
void tds754a_do_pre_exp_checks( void );
void tds754a_set_meas_window( WINDOW *w );
void tds754a_set_curve_window( WINDOW *w );
void tds754a_set_window( WINDOW *w );

bool tds754a_init( const char *name );
double tds754a_get_timebase( void );
bool tds754a_set_timebase( double timebase);
bool tds754a_set_record_length( long num_points );
bool tds754a_get_record_length( long *ret );
bool tds754a_set_trigger_pos( double pos );
bool tds754a_get_trigger_pos( double *ret );
long tds754a_get_num_avg( void );
bool tds754a_set_num_avg( long num_avg );
int tds754a_get_acq_mode(void);
bool tds754a_get_cursor_position( int cur_no, double *cp );
bool tds754a_get_cursor_distance( double *cd );
bool tds754a_set_trigger_channel( const char *name );
int tds754a_get_trigger_channel( void );
void tds754a_gpib_failure( void );
bool tds754a_clear_SESR( void );
void tds754a_finished( void );
bool tds754a_set_cursor( int cur_num, double pos );
bool tds754a_set_track_cursors( bool flag );
bool tds754a_set_gated_meas( bool flag );
bool tds754a_set_snap( bool flag );
bool tds754a_display_channel( int channel );
double tds754a_get_sens( int channel );
bool tds754a_set_sens( int channel, double val );
bool tds754a_start_aquisition( void );
double tds754a_get_area( int channel, WINDOW *w, bool use_cursor );
bool tds754a_get_curve( int channel, WINDOW *w, double **data, long *length,
						bool use_cursor );
double tds754a_get_amplitude( int channel, WINDOW *w, bool use_cursor );
void tds754a_free_running( void );
bool tds754a_lock_state( bool lock );



#ifdef TDS754A_MAIN

TDS754A tds754a;
const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4",
								 "AUX", "LINE" };

#else

extern TDS754A tds754a;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};
