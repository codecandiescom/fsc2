/*
  $Id$
*/



#include "fsc2.h"



#define DEVICE_NAME "TDS754A"      /* compare entry in /etc/gpib.conf ! */



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
bool tds754a_clear_SESR( void );
void tds754a_finished( void );
bool tds754a_set_cursor( int cur_num, double pos );
bool tds754a_set_track_cursors( bool flag );
bool tds754a_set_gated_meas( bool flag );



typedef struct _W {
	long num;
	bool is_used;
	double start;
	double width;
	bool is_width;
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
	int trigger_channel;
	bool is_trigger_channel;

	long rec_len;
	double trig_pos;

} TDS754A;


#define TDS_POINTS_PER_DIV 50

#define MAX_CHANNELS  13         /* number of channel names */

#define TDS754A_CH1    0
#define TDS754A_CH2    1
#define TDS754A_CH3    2
#define TDS754A_CH4    3
#define TDS754A_AUX    4
#define TDS754A_MATH1  5
#define TDS754A_MATH2  6
#define TDS754A_MATH3  7
#define TDS754A_REF1   8
#define TDS754A_REF2   9
#define TDS754A_REF3  10
#define TDS754A_REF4  11
#define TDS754A_LIN   12         /* Line In (for triggger only) */



#ifdef TDS754A_MAIN

TDS754A tds754a;
const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4", "AUX",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4", "LIN" };

#else

extern TDS754A tds754a;
extern const char *Channel_Names[ ];

#endif


enum {
	SAMPLE,
	AVERAGE
};

