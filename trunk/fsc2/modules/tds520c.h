/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "TDS520C"    /* compare entry in /etc/gpib.conf ! */

#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  12         /* number of channel names */

#define TDS520C_UNDEF -1
#define TDS520C_CH1    0
#define TDS520C_CH2    1
#define TDS520C_MATH1  2
#define TDS520C_MATH2  3
#define TDS520C_MATH3  4
#define TDS520C_REF1   5
#define TDS520C_REF2   6
#define TDS520C_REF3   7
#define TDS520C_REF4   8
#define TDS520C_AUX1   9         /* Auxiliary (for triggger only) */
#define TDS520C_AUX2  10         /* Auxiliary (for triggger only) */
#define TDS520C_LIN   11         /* Line In (for triggger only) */



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

	double num_avg;
	bool is_num_avg;

	WINDOW *w;               /* start element of list of windows             */
	int num_windows;
	bool is_equal_width;     /* all windows have equal width -> tracking     */
							 /* cursors can be used without further checking */
	bool gated_state;        /* Gated measurements ?                         */
	bool snap_state;

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
} TDS520C;



/* declaration of exported functions */

int tds520c_init_hook( void );
int tds520c_exp_hook( void );
int tds520c_end_of_exp_hook( void );
void tds520c_exit_hook( void );


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
Var *digitizer_lock_keyboard( Var *v );


/* declaration of internally used functions */

const char *tds520c_ptime( double time );
void tds520c_delete_windows( void );
void tds520c_do_pre_exp_checks( void );
void tds520c_set_meas_window( WINDOW *w );
void tds520c_set_curve_window( WINDOW *w );
void tds520c_set_window( WINDOW *w );

bool tds520c_init( const char *name );
double tds520c_get_timebase( void );
bool tds520c_set_timebase( double timebase);
bool tds520c_set_record_length( long num_points );
bool tds520c_get_record_length( long *ret );
bool tds520c_set_trigger_pos( double pos );
bool tds520c_get_trigger_pos( double *ret );
long tds520c_get_num_avg( void );
bool tds520c_set_num_avg( long num_avg );
int tds520c_get_acq_mode(void);
bool tds520c_get_cursor_position( int cur_no, double *cp );
bool tds520c_get_cursor_distance( double *cd );
bool tds520c_set_trigger_channel( const char *name );
int tds520c_get_trigger_channel( void );
void tds520c_gpib_failure( void );
bool tds520c_clear_SESR( void );
void tds520c_finished( void );
bool tds520c_set_cursor( int cur_num, double pos );
bool tds520c_set_track_cursors( bool flag );
bool tds520c_set_gated_meas( bool flag );
bool tds520c_set_snap( bool flag );
bool tds520c_display_channel( int channel );
bool tds520c_set_sens( int channel, double val );
double tds520c_get_sens( int channel );
bool tds520c_start_aquisition( void );
double tds520c_get_area( int channel, WINDOW *w, bool use_cursor );
bool tds520c_get_curve( int channel, WINDOW *w, double **data, long *length,
						bool use_cursor );
double tds520c_get_amplitude( int channel, WINDOW *w, bool use_cursor );
bool tds520c_lock_state( bool lock );


#ifdef TDS520C_MAIN

TDS520C tds520c;
const char *Channel_Names[ ] = { "CH1", "CH2", "MATH1", "MATH2", "MATH3",
								 "REF1", "REF2", "REF3", "REF4",
								 "AUX1", "AUX2", "LINE" };

#else

extern TDS520C tds520c;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};
