/*
  $Id$
*/



#include "fsc2.h"



#define DEVICE_NAME "TDS754A"      /* compare entry in /etc/gpib.conf ! */

#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  13         /* number of channel names */

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




typedef struct _W {
	long num;
	bool is_used;
	double start;
	double width;
	bool is_width;
	long num_points;
	struct _W * next;
} WINDOW;


typedef struct
{
	int device;

	double timebase;
	bool is_timebase;

	double num_avg;
	bool is_num_avg;

	WINDOW *w;
	int num_windows;
	bool is_equal_width;      // all windows have equal width -> tracking
	                          // cursors can be used without further checking
	bool gated_state;         // Gated measurements ?

	int trigger_channel;
	bool is_trigger_channel;

	long rec_len;
	double trig_pos;

	double cursor_pos;        // current position of cursor 1

	int meas_source;          // currently selected measurements source channel
	int data_source;          // currently selected data source channel

	bool channels_in_use[ MAX_CHANNELS ];
} TDS754A;



/* declaration of exported functions */

int tds754a_init_hook( void );
int tds754a_test_hook( void );
int tds754a_end_of_test_hook( void );
int tds754a_exp_hook( void );
int tds754a_end_of_exp_hook( void );
void tds754a_exit_hook( void );


Var *digitizer_define_window( Var *v );
Var *digitizer_timebase( Var *v );
Var *digitizer_num_averages( Var *v );
Var *digitizer_get_channel_number( Var *v );
Var *digitizer_trigger_channel( Var *v );


/* declaration of internally used functions */

bool tds754a_init( const char *name );
double tds754a_get_timebase( void );
bool tds754a_set_timebase( double timebase);
bool tds754a_get_record_length( long *ret );
bool tds754a_get_trigger_pos( double *ret );
long tds754a_get_num_avg( void );
bool tds754a_set_num_avg( long num_avg );
int tds754a_get_acq_mode(void);
bool tds754a_get_cursor_distance( double *cd );
bool tds754a_set_trigger_channel( const char *name );
int tds754a_get_trigger_channel( void );
void tds754a_gpib_failure( void );
const char *tds754a_ptime( double time );
void tds754a_delete_windows( void );
void tds754a_do_pre_exp_checks( void );
void tds754a_set_meas_window( WINDOW *w );
void tds754a_set_window( WINDOW *w );
bool tds754a_clear_SESR( void );
void tds754a_finished( void );
bool tds754a_set_cursor( int cur_num, double pos );
bool tds754a_set_track_cursors( bool flag );
bool tds754a_set_gated_meas( bool flag );
bool tds754a_display_channel( int channel );
bool tds754a_start_aquisition( void );
double tds754a_get_area( int channel, WINDOW *w );
bool tds754a_get_curve( int channel, WINDOW *w, double **data, long *length );





#ifdef TDS754A_MAIN

TDS754A tds754a;
const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4",
								 "AUX", "LIN" };

#else

extern TDS754A tds754a;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};

