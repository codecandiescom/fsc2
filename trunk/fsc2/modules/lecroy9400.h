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

#include "lecroy9400.conf"


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define LECROY9400_TEST_REC_LEN      32000
#define LECROY9400_TEST_TB_ENTRY     19        /* i.e. 100 ms */
#define LECROY9400_TEST_SENSITIVITY  0.01
#define LECROY9400_TEST_NUM_AVG      10
#define LECROY9400_TEST_TRIG_POS     0.1
#define LECROY9400_TEST_TRIG_CHANNEL 0


#define MAX_CHANNELS       9         /* number of channel names */

#define LECROY9400_UNDEF   DIGITIZER_CHANNEL_INVALID
#define LECROY9400_CH1     DIGITIZER_CHANNEL_CH1
#define LECROY9400_CH2     DIGITIZER_CHANNEL_CH2
#define LECROY9400_MEM_C   DIGITIZER_CHANNEL_MEM_C
#define LECROY9400_MEM_D   DIGITIZER_CHANNEL_MEM_D
#define LECROY9400_FUNC_E  DIGITIZER_CHANNEL_FUNC_E
#define LECROY9400_FUNC_F  DIGITIZER_CHANNEL_FUNC_F
#define LECROY9400_LIN     DIGITIZER_CHANNEL_LINE
#define LECROY9400_EXT     DIGITIZER_CHANNEL_EXT
#define LECROY9400_EXT10   DIGITIZER_CHANNEL_EXT10

#define LECROY9400_MIN_TIMEBASE 5.0e-8
#define LECROY9400_MAX_TIMEBASE 0.2
#define LECROY9400_MIN_TB_INDEX 0
#define LECROY9400_MAX_TB_INDEX 20


#define LECROY9400_MAX_SENS     5.0e-3
#define LECROY9400_MIN_SENS_1M  12.5
#define LECROY9400_MIN_SENS_50  2.5


#define INVALID_COUPL     -1         /* Input coupling for data channel */
#define AC_1_MOHM          0
#define DC_1_MOHM          1
#define DC_50_OHM          2


#define MAX_OFFSET         8.0       /* Offset for data channel */
#define MIN_OFFSET        -8.0


#define TRG_CPL_AC         0         /* Trigger coupling */
#define TRG_CPL_DC         1
#define TRG_CPL_LF_REJ     2
#define TRG_CPL_HF_REJ     3

#define MAX_DESC_LEN       160


/* Structure for description of a `window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct W_ {
	long num;                   /* number of window                          */
	bool is_used;               /* flag, set when window has been used       */
	double start;               /* start of window (in time units)           */
	double width;               /* width of window (in time units)           */
	long start_num;				/* first point of window                     */
	long end_num;				/* last point of window                      */
	bool is_start;              /* flag, set if start of window has been set */
	bool is_width;              /* flag, set if width of window has been set */
	long num_points;            /* number of data points between the cursors */
	struct W_ *next;            /* pointer to next window structure          */
	struct W_ *prev;            /* pointer to previous window structure      */
} WINDOW;


typedef struct
{
	int device;

	bool is_reacting;
	int data_size;              /* # of bytes transfered for data points */

	bool is_displayed[ MAX_CHANNELS ];

	double timebase;
	int tb_index;
	bool is_timebase;

	unsigned char wv_desc[ LECROY9400_FUNC_F + 1 ][ MAX_DESC_LEN ];

	double sens[ MAX_CHANNELS ];
	double is_sens[ MAX_CHANNELS ];

	int coupl[ MAX_CHANNELS ];
	double is_coupl[ MAX_CHANNELS ];

	double offset[ MAX_CHANNELS ];
	double is_offset[ MAX_CHANNELS ];

	int coupling[ MAX_CHANNELS ];
	double is_coupling[ MAX_CHANNELS ];

	int trigger_channel;
	bool is_trigger_channel;

	double trigger_level;
	bool is_trigger_level;

	int trigger_slope;
	bool is_trigger_slope;

	int trigger_coupling;
	bool is_trigger_coupling;

	double trig_pos;
	bool is_trig_pos;

	long num_avg[ MAX_CHANNELS ];
	bool is_num_avg[ MAX_CHANNELS ];

	WINDOW *w;               /* start element of list of windows             */
	int num_windows;

	double cursor_pos;     /* current position of cursor 1                   */

	int meas_source;       /* currently selected measurements source channel */
	int data_source;       /* currently selected data source channel         */

	bool channels_in_use[ MAX_CHANNELS ];

	bool lock_state;       /* set if keyboard is locked */
} LECROY9400;



/* declaration of exported functions */

int lecroy9400_init_hook( void );
int lecroy9400_exp_hook( void );
int lecroy9400_end_of_exp_hook( void );
void lecroy9400_exit_hook( void );


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

int lecroy9400_get_tb_index( double timebase );
const char *lecroy9400_ptime( double p_time );
void lecroy9400_delete_windows( void );
void lecroy9400_do_pre_exp_checks( void );

bool lecroy9400_init( const char *name );
double lecroy9400_get_timebase( void );
bool lecroy9400_set_timebase( double timebase);
bool lecroy9400_set_trigger_pos( double pos );
bool lecroy9400_get_trigger_pos( double *ret );
long lecroy9400_get_num_avg( void );
bool lecroy9400_set_num_avg( long num_avg );
int lecroy9400_get_acq_mode(void);
bool lecroy9400_get_cursor_position( int cur_no, double *cp );
bool lecroy9400_get_cursor_distance( double *cd );
bool lecroy9400_set_trigger_channel( const char *name );
int lecroy9400_get_trigger_channel( void );
void lecroy9400_gpib_failure( void );
bool lecroy9400_clear_SESR( void );
void lecroy9400_finished( void );
bool lecroy9400_set_cursor( int cur_num, double pos );
bool lecroy9400_set_snap( bool flag );
bool lecroy9400_display_channel( int channel );
bool lecroy9400_set_sens( int channel, double val );
double lecroy9400_get_sens( int channel );
bool lecroy9400_start_aquisition( void );
double lecroy9400_get_area( int channel, WINDOW *w, bool use_cursor );
bool lecroy9400_get_curve( int channel, WINDOW *w, double **data, long *length,
					   bool use_cursor );
double lecroy9400_get_amplitude( int channel, WINDOW *w, bool use_cursor );
void lecroy9400_free_running( void );
bool lecroy9400_lock_state( bool lock );


#ifdef LECROY9400_MAIN

LECROY9400 lecroy9400;

const char *Channel_Names[ 9 ] = { "CH1", "CH2", "MEM_C", "MEM_D", "FUNC_E",
								   "FUNC_F", "LINE", "EXT", "EXT10" };
/* List of all timebases (in s/div) - currently only timebases that can be
   used in single shot mode are supported (i.e. neither random interleaved
   sampling nor roll mode) */

double tb[ 21 ] = {                      50.0e-9,
					100.0e-9, 200.0e-9, 500.0e-9,
					  1.0e-6,   2.0e-6,   5.0e-6,
					 10.0e-9,  20.0e-9,  50.0e-9,
					100.0e-6, 200.0e-6, 500.0e-6,
					  1.0e-3,   2.0e-3,   5.0e-3,
					 10.0e-3,  20.0e-3,  50.0e-3,
					100.0e-3, 200.0e-3 };

/* List of the corresponding sample rates, i.e. the time/point */

double sr[ 21 ] = {						 10.0e-9,
					 10.0e.9,  10.0e.9,  10.0e.9,
					 10.0e.9,  10.0e.9,  10.0e.9,
					 10.0e.9,  10.0e.9,  20.0e-9,
					 40.0e-9,  80.0e-9, 200.0e-9,
					400.0e-9, 800.0e-9,   2.0e-6,
					  4.0e-6,   8.0e-6,  20.0e-6,
					 40.0e-6,  80.0e-6 };

/* List of points per division */

int ppd[ 21 ] = { 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 2500,
				  2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500,
				  2500, 2500, 2500 };

/* List of number of averages that can be done using the WP01 Waveform
   Processing option */

static long na[ 16 ] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000,
						 20000, 50000, 100000, 200000, 500000, 1000000 };

#else

extern LECROY9400 lecroy9400;

extern const char *Channel_Names[ 9 ];

external double tb[ 21 ];
external double sr[ 21 ];
external int ppd[ 21 ];
external int long na[ 16 ];

#endif


enum {
	SAMPLE,
	AVERAGE
};


#define TB_ENTRIES ( sizeof tb / sizeof tb[ 0 ] )
#define NA_ENTRIES ( sizeof na / sizeof na[ 0 ] )
