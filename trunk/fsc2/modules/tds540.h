/*
  $Id$
*/



#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "TDS540"     /* compare entry in /etc/gpib.conf ! */

#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  13         /* number of channel names */

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
	double is_sens[ TDS540_CH4 - TDS540_CH1 + 1 ];

	double num_avg;
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
} TDS540;



/* declaration of exported functions */

int tds540_init_hook( void );
int tds540_exp_hook( void );
int tds540_end_of_exp_hook( void );
void tds540_exit_hook( void );


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

const char *tds540_ptime( double time );
void tds540_delete_windows( void );
void tds540_do_pre_exp_checks( void );
void tds540_set_meas_window( WINDOW *w );
void tds540_set_curve_window( WINDOW *w );
void tds540_set_window( WINDOW *w );

bool tds540_init( const char *name );
double tds540_get_timebase( void );
bool tds540_set_timebase( double timebase);
bool tds540_set_record_length( long num_points );
bool tds540_get_record_length( long *ret );
bool tds540_set_trigger_pos( double pos );
bool tds540_get_trigger_pos( double *ret );
long tds540_get_num_avg( void );
bool tds540_set_num_avg( long num_avg );
int tds540_get_acq_mode(void);
bool tds540_get_cursor_position( int cur_no, double *cp );
bool tds540_get_cursor_distance( double *cd );
bool tds540_set_trigger_channel( const char *name );
int tds540_get_trigger_channel( void );
void tds540_gpib_failure( void );
bool tds540_clear_SESR( void );
void tds540_finished( void );
bool tds540_set_cursor( int cur_num, double pos );
bool tds540_set_track_cursors( bool flag );
bool tds540_set_gated_meas( bool flag );
bool tds540_set_snap( bool flag );
bool tds540_display_channel( int channel );
bool tds540_set_sens( int channel, double val );
double tds540_get_sens( int channel );
bool tds540_start_aquisition( void );
double tds540_get_area( int channel, WINDOW *w, bool use_cursors );
bool tds540_get_curve( int channel, WINDOW *w, double **data, long *length,
					   bool use_cursor );
double tds540_get_amplitude( int channel, WINDOW *w, bool use_cursors );
bool tds540_lock_state( bool lock );



#ifdef TDS540_MAIN

TDS540 tds540;
const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4",
								 "AUX", "LINE" };

#else

extern TDS540 tds540;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};
