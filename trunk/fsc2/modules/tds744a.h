/*
  $Id$
*/



#include "fsc2.h"



#define DEVICE_NAME "TDS744A"      /* compare entry in /etc/gpib.conf ! */

#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  13         /* number of channel names */

#define TDS744A_UNDEF -1
#define TDS744A_CH1    0
#define TDS744A_CH2    1
#define TDS744A_CH3    2
#define TDS744A_CH4    3
#define TDS744A_MATH1  4
#define TDS744A_MATH2  5
#define TDS744A_MATH3  6
#define TDS744A_REF1   7
#define TDS744A_REF2   8
#define TDS744A_REF3   9
#define TDS744A_REF4  10
#define TDS744A_AUX   11         /* Auxiliary (for triggger only) */
#define TDS744A_LIN   12         /* Line In (for triggger only) */



/* Structure for description of a `window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct _W {
	long num;                   /* number of window                          */
	bool is_used;               /* flag, set when window has been used		 */
	double start;               /* start of window (in time units)			 */
	double width;               /* width of window (in time units)			 */
	long start_num;				/* first point of window                     */
	long end_num;				/* last point of window                      */
	bool is_width;              /* flag, set if width of window has been set */
	long num_points;            /* number of data points between the cursors */
	struct _W *next;            /* pointer to next window structure			 */
	struct _W *prev;            /* pointer to previous window structure      */
} WINDOW;


typedef struct
{
	int device;

	double timebase;
	bool is_timebase;

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
	double trig_pos;

	double cursor_pos;        // current position of cursor 1

	int meas_source;          // currently selected measurements source channel
	int data_source;          // currently selected data source channel

	bool channels_in_use[ MAX_CHANNELS ];
	double channel_sens[ MAX_CHANNELS ];

} TDS744A;



/* declaration of exported functions */

int tds744a_init_hook( void );
int tds744a_exp_hook( void );
int tds744a_end_of_exp_hook( void );
void tds744a_exit_hook( void );


Var *digitizer_define_window( Var *v );
Var *digitizer_timebase( Var *v );
Var *digitizer_num_averages( Var *v );
Var *digitizer_get_channel_number( Var *v );
Var *digitizer_trigger_channel( Var *v );
Var *digitizer_start_acquisition( Var *v );
Var *digitizer_get_area( Var *v );
Var *digitizer_get_curve( Var *v );
Var *digitizer_get_amplitude( Var *v );


/* declaration of internally used functions */

const char *tds744a_ptime( double time );
void tds744a_delete_windows( void );
void tds744a_do_pre_exp_checks( void );
void tds744a_set_meas_window( WINDOW *w );
void tds744a_set_curve_window( WINDOW *w );
void tds744a_set_window( WINDOW *w );

bool tds744a_init( const char *name );
double tds744a_get_timebase( void );
bool tds744a_set_timebase( double timebase);
bool tds744a_get_record_length( long *ret );
bool tds744a_get_trigger_pos( double *ret );
long tds744a_get_num_avg( void );
bool tds744a_set_num_avg( long num_avg );
int tds744a_get_acq_mode(void);
bool tds744a_get_cursor_position( int cur_no, double *cp );
bool tds744a_get_cursor_distance( double *cd );
bool tds744a_set_trigger_channel( const char *name );
int tds744a_get_trigger_channel( void );
void tds744a_gpib_failure( void );
bool tds744a_clear_SESR( void );
void tds744a_finished( void );
bool tds744a_set_cursor( int cur_num, double pos );
bool tds744a_set_track_cursors( bool flag );
bool tds744a_set_gated_meas( bool flag );
bool tds744a_set_snap( bool flag );
bool tds744a_display_channel( int channel );
double tds744a_get_sens( int channel );
bool tds744a_start_aquisition( void );
double tds744a_get_area( int channel, WINDOW *w, bool use_cursors );
bool tds744a_get_curve( int channel, WINDOW *w, double **data, long *length,
						bool use_cursor );
double tds744a_get_amplitude( int channel, WINDOW *w, bool use_cursors );



#ifdef TDS744A_MAIN

TDS744A tds744a;
const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4",
								 "AUX", "LINE" };

#else

extern TDS744A tds744a;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};
