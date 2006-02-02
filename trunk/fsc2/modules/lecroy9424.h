/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined LECROY9424_HEADER
#define LECROY9424_HEADER

#include "fsc2_module.h"


/* Include configuration information for the device */

#include "lecroy9424.conf"


#define INVALID_COUPL     -1         /* Input coupling for data channel */
#define AC_1_MOHM          0         /* (don't change the sequence!) */
#define DC_1_MOHM          1
#define DC_50_OHM          2
#define GND                3

#define TRG_AC             0
#define TRG_DC             1
#define TRG_LF_REJ         2
#define TRG_HF_REJ         3
#define TRG_HF             4

#define TRG_CPL_AC         0         /* Trigger couplings */
#define TRG_CPL_DC         1         /* (don't change the sequence!) */
#define TRG_CPL_LF_REJ     2
#define TRG_CPL_HF_REJ     3
#define TRG_CPL_HF         4

#define TRG_MODE_AUTO      0         /* Trigger modes */
#define TRG_MODE_NORMAL    1         /* (don't change the sequence!) */
#define TRG_MODE_SINGLE    2
#define TRG_MODE_SEQUENCE  3
#define TRG_MODE_WRAP      4



/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define LECROY9424_TEST_TIMEBASE     5.0e-5      /* 50 us per division*/
#define LECROY9424_TEST_TB_INDEX     14
#define LECROY9424_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY9424_TEST_SENSITIVITY  2.5         /* 2.5 V per division */
#define LECROY9424_TEST_OFFSET       0.0
#define LECROY9424_TEST_NUM_AVG      10
#define LECROY9424_TEST_TRIG_DELAY   1.0e-2      /* 10 ms pretrigger */
#define LECROY9424_TEST_TRIG_CHANNEL 0           /* trigger on CH1 */
#define LECROY9424_TEST_TRIG_LEVEL   0.0
#define LECROY9424_TEST_TRIG_SLOPE   POSITIVE
#define LECROY9424_TEST_TRIG_COUP    TRG_CPL_AC
#define LECROY9424_TEST_TRIG_MODE    TRG_MODE_NORMAL
#define LECROY9424_TEST_REC_LEN      1000
#define LECROY9424_TEST_BWL          UNSET       /* bandwidth limiter */


#define LECROY9424_UNDEF   -1
#define LECROY9424_CH1      0
#define LECROY9424_CH2      1
#define LECROY9424_CH3      2
#define LECROY9424_CH4      3
#define LECROY9424_EXP_A    4
#define LECROY9424_EXP_B    5
#define LECROY9424_MEM_C    6
#define LECROY9424_MEM_D    7
#define LECROY9424_FUNC_E   8
#define LECROY9424_FUNC_F   9
#define LECROY9424_LIN     10

/* Measurement channel with the highest number */

#define LECROY9424_CH_MAX   LECROY9424_CH4


/* Channel 4 can be used as trigger with this model */

#define LCROY94_TMPL_CH4_AS_TRG


#define GENERAL_TO_LECROY9424 0
#define LECROY9424_TO_GENERAL 1


/* Total number of channels and maximum number of channels that can be
   diesplayed at once */

#define LECROY9424_MAX_CHANNELS       11         /* number of channel names */
#define LECROY9424_MAX_USED_CHANNELS   4


/* Maximum and minimum time base setting [s/div] - please note that the
   time resolution depends in a somewhat complivcated way on the time
   base and possibly the mode (RIS or SINGLE SHOT), see the HORI_RES
   structure below */

#define LECROY9424_MIN_TIMEBASE 1.0e-9
#define LECROY9424_MAX_TIMEBASE 5.0e3


/* Maximum and minimum sensitivity [V/div] for CH1 and CH2 */

#define LECROY9424_MAX_SENS     5.0e-3     /* 5 mV */
#define LECROY9424_MIN_SENS     2.5        /* 2.5 V */


/* Maximum number of averages */

#define LECROY9424_MAX_AVERAGES    1000


/* Maximum factors or values for trigger levels */

#define LECROY9424_TRG_MAX_LEVEL_CH_FAC 5.0

#define UNDEFINED_REC_LEN  -1


/* Constants for the INR register */

#define INR_FF_DONE        ( 1U << 11 )
#define INR_FE_DONE        ( 1U << 10 )
#define INR_MD_DONE        ( 1U <<  9 )
#define INR_MC_DONE        ( 1U <<  8 )
#define INR_SIG_DONE       ( 1U <<  0 )


/* Structure for description of a 'window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct Window Window_T;
typedef struct LECROY9424 LECROY9424_T;
typedef struct HORI_RES HORI_RES_T;


struct Window {
	long num;                   /* number of window                          */
	double start;               /* start of window (in time units)           */
	double width;               /* width of window (in time units)           */
	long start_num;				/* first point of window                     */
	long end_num;				/* last point of window                      */
	long num_points;            /* number of data points between the cursors */
	Window_T *next;             /* pointer to next window structure          */
	Window_T *prev;             /* pointer to previous window structure      */
};


struct LECROY9424 {
	int device;

	unsigned int INR;

	bool is_displayed[ LECROY9424_MAX_CHANNELS ];

	int num_used_channels;

	double timebase;
	int tb_index;
	bool is_timebase;

	bool interleaved;
	bool is_interleaved;

	double sens[ LECROY9424_MAX_CHANNELS ];
	bool is_sens[ LECROY9424_MAX_CHANNELS ];

	double offset[ LECROY9424_MAX_CHANNELS ];
	bool is_offset[ LECROY9424_MAX_CHANNELS ];

	bool bandwidth_limiter;
	bool is_bandwidth_limiter;

	int coupling[ LECROY9424_MAX_CHANNELS ];
	bool is_coupling[ LECROY9424_MAX_CHANNELS ];

	int trigger_channel;
	bool is_trigger_channel;

	double trigger_level[ LECROY9424_MAX_CHANNELS ];
	bool is_trigger_level[ LECROY9424_MAX_CHANNELS ];

	int trigger_slope[ LECROY9424_MAX_CHANNELS ];
	bool is_trigger_slope[ LECROY9424_MAX_CHANNELS ];

	int trigger_coupling[ LECROY9424_MAX_CHANNELS ];
	bool is_trigger_coupling[ LECROY9424_MAX_CHANNELS ];

	double trigger_delay;
	bool is_trigger_delay;

	int trigger_mode;
	bool is_trigger_mode;

	bool is_avg_setup[ LECROY9424_MAX_CHANNELS ];
	long source_ch[ LECROY9424_MAX_CHANNELS ];
	long rec_len[ LECROY9424_MAX_CHANNELS ];
	long num_avg[ LECROY9424_MAX_CHANNELS ];

	Window_T *w;           /* start element of list of windows               */
	int num_windows;

	bool channels_in_use[ LECROY9424_MAX_CHANNELS ];
};


struct HORI_RES {
	double tdiv;         /* time base setting */
	double ris_tp;       /* time resolution (RIS mode) */
	long ris_rl;         /* record length (RIS mode) */
	double ss_tp;        /* time resolution (SINGLE SHOT mode) */
	long ss_rl;          /* record length (SINGLE SHOT mode) */
};


#if defined LECROY9424_MAIN_

HORI_RES_T hres[ 39 ] = { { 1.0e-9, 1.0e-10,   100,   -1.0,    -1 },
					      { 2.0e-9, 1.0e-10,   200,   -1.0,    -1 },
					      { 5.0e-9, 1.0e-10,   500,   -1.0,    -1 },
					      { 1.0e-8, 1.0e-10,  1000,   -1.0,    -1 },
					      { 2.0e-8, 1.0e-10,  2000,   -1.0,    -1 },
					      { 5.0e-8, 1.0e-10,  5000, 1.0e-8,    50 },
					      { 1.0e-7, 1.0e-10, 10000, 1.0e-8,   100 },
					      { 2.0e-7, 1.0e-10, 20000, 1.0e-8,   200 },
					      { 5.0e-7, 1.0e-10, 50000, 1.0e-8,   500 },
					      { 1.0e-6, 2.5e-10, 40000, 1.0e-8,  1000 },
					      { 2.0e-6, 5.0e-10, 40000, 1.0e-8,  2000 },
					      { 5.0e-6, 1.0e-09, 50000, 1.0e-8,  5000 },
					      { 1.0e-5, 2.5e-09, 40000, 1.0e-8, 10000 },
					      { 2.0e-5, 5.0e-09, 40000, 1.0e-8, 20000 },
					      { 5.0e-5,    -1.0,    -1, 1.0e-8, 50000 },
					      { 1.0e-4,    -1.0,    -1, 2.5e-8, 40000 },
					      { 2.0e-4,    -1.0,    -1, 5.0e-8, 40000 },
					      { 5.0e-4,    -1.0,    -1, 1.0e-7, 50000 },
					      { 1.0e-3,    -1.0,    -1, 2.5e-7, 40000 },
					      { 2.0e-3,    -1.0,    -1, 5.0e-7, 40000 },
					      { 5.0e-3,    -1.0,    -1, 1.0e-6, 50000 },
					      { 1.0e-2,    -1.0,    -1, 2.5e-6, 40000 },
					      { 2.0e-2,    -1.0,    -1, 5.0e-6, 40000 },
					      { 5.0e-2,    -1.0,    -1, 1.0e-5, 50000 },
					      { 1.0e-1,    -1.0,    -1, 2.5e-5, 40000 },
					      { 2.0e-1,    -1.0,    -1, 5.0e-5, 40000 },
					      { 5.0e-1,    -1.0,    -1, 1.0e-4, 50000 },
					      {    1.0,    -1.0,    -1, 2.5e-4, 40000 },
					      {    2.0,    -1.0,    -1, 5.0e-4, 40000 },
					      {    5.0,    -1.0,    -1, 1.0e-3, 50000 },
					      {  1.0e1,    -1.0,    -1, 2.5e-3, 40000 },
					      {  2.0e1,    -1.0,    -1, 5.0e-3, 40000 },
					      {  5.0e1,    -1.0,    -1, 1.0e-2, 50000 },
					      {  1.0e2,    -1.0,    -1, 2.5e-2, 40000 },
					      {  2.0e2,    -1.0,    -1, 5.0e-2, 40000 },
					      {  5.0e2,    -1.0,    -1, 1.0e-1, 50000 },
					      {  1.0e3,    -1.0,    -1, 2.5e-1, 40000 },
					      {  2.0e3,    -1.0,    -1, 5.0e-1, 40000 },
					      {  5.0e3,    -1.0,    -1,    1.0, 50000 } };

/* List of fixed sensivity settings where the range of the offset changes */

double fixed_sens[ 8 ] = { 5.0e-3, 1.0e-2, 2.0e-2, 5.0e-2,
						   0.1, 0.2, 0.5, 1.0 };

/* List of offset factors for the different sensitivity ranges */

int offset_factor[ 8 ] = { 48, 24, 12, 12, 12, 12, 12, 10 };

#else
extern HORI_RES_T hres[ 39 ];
extern double fixed_sens[ 8 ];
extern int offset_factor[ 8 ];
#endif

extern long rl[ 10 ];
extern LECROY9424_T lecroy9424;
extern const char *LECROY9424_Channel_Names[ 13 ];


enum {
	SAMPLE,
	AVERAGE
};


/* declaration of exported functions */

int lecroy9424_init_hook(       void );
int lecroy9424_test_hook(       void );
int lecroy9424_exp_hook(        void );
int lecroy9424_end_of_exp_hook( void );
void lecroy9424_exit_hook(      void );


Var_T *digitizer_name(              Var_T * /* v */ );
Var_T *digitizer_define_window(     Var_T * /* v */ );
Var_T *digitizer_change_window(     Var_T * /* v */ );
Var_T *digitizer_window_position(   Var_T * /* v */ );
Var_T *digitizer_window_width(      Var_T * /* v */ );
Var_T *digitizer_timebase(          Var_T * /* v */ );
Var_T *digitizer_interleave_mode(   Var_T * /* v */ );
Var_T *digitizer_time_per_point(    Var_T * /* v */ );
Var_T *digitizer_sensitivity(       Var_T * /* v */ );
Var_T *digitizer_offset(            Var_T * /* v */ );
Var_T *digitizer_bandwidth_limiter( Var_T * /* v */ );
Var_T *digitizer_trigger_channel(   Var_T * /* v */ );
Var_T *digitizer_trigger_level(     Var_T * /* v */ );
Var_T *digitizer_trigger_slope(     Var_T * /* v */ );
Var_T *digitizer_trigger_coupling(  Var_T * /* v */ );
Var_T *digitizer_trigger_mode(      Var_T * /* v */ );
Var_T *digitizer_trigger_delay(     Var_T * /* v */ );
Var_T *digitizer_averaging(         Var_T * /* v */ );
Var_T *digitizer_num_averages(      Var_T * /* v */ );
Var_T *digitizer_record_length(     Var_T * /* v */ );
Var_T *digitizer_trigger_position(  Var_T * /* v */ );
Var_T *digitizer_meas_channel_ok(   Var_T * /* v */ );
Var_T *digitizer_start_acquisition( Var_T * /* v */ );
Var_T *digitizer_get_curve(         Var_T * /* v */ );
Var_T *digitizer_get_area(          Var_T * /* v */ );
Var_T *digitizer_get_amplitude(     Var_T * /* v */ );
Var_T *digitizer_run(               Var_T * /* v */ );
Var_T *digitizer_copy_curve(        Var_T * /* v */ );
Var_T *digitizer_command(           Var_T * /* v */ );


/* declaration of internally used functions */

bool lecroy9424_init( const char * /* name */ );

double lecroy9424_get_timebase( void );

bool lecroy9424_set_timebase( double /* timebase */ );

bool lecroy9424_get_interleaved( void );

bool lecroy9424_set_interleaved( bool /* state */ );

double lecroy9424_get_sens( int /* channel */ );

bool lecroy9424_set_sens( int    /* channel */,
						  double /* sens    */ );

double lecroy9424_get_offset( int /* channel */ );

bool lecroy9424_set_offset( int    /* channel */,
							double /* offset  */ );

int lecroy9424_get_coupling( int /* channel */ );

bool lecroy9424_set_coupling( int /* channel */,
							  int /* type    */ );

int lecroy9424_get_bandwidth_limiter( void );

bool lecroy9424_set_bandwidth_limiter( bool /* state */ );

int lecroy9424_get_trigger_source( void );

bool lecroy9424_set_trigger_source( int /* channel */ );

double lecroy9424_get_trigger_level( int /* channel */ );

bool lecroy9424_set_trigger_level( int    /* channel */,
								   double /* level   */ );

double lecroy9424_get_trigger_slope( int /* channel */ );


bool lecroy9424_set_trigger_slope( int /* channel */,
								   int /* slope   */ );

int lecroy9424_get_trigger_coupling( int /* channel */ );

int lecroy9424_set_trigger_coupling( int /* channel */,
									 int /* cpl     */ );

int lecroy9424_get_trigger_mode( void );

int lecroy9424_set_trigger_mode( int /* mode */ );

double lecroy9424_get_trigger_delay( void );

bool lecroy9424_set_trigger_delay( double /* delay */ );

bool lecroy9424_is_displayed( int /* ch */ );

bool lecroy9424_display( int /* ch     */,
						 int /* on_off */ );

long lecroy9424_get_num_avg( int /* channel */ );

bool lecroy9424_get_desc( int /* channel */ );

void lecroy9424_set_up_averaging( long /* channel */,
								  long /* source  */,
								  long /* num_avg */,
								  long /* rec_len */ );

void lecroy9424_finished( void );

void lecroy9424_start_acquisition( void );


void lecroy9424_get_curve( int        /* ch     */,
						   Window_T * /* w      */,
						   double **  /* array  */,
						   long *     /* length */ );

double lecroy9424_get_area( int        /* ch */,
							Window_T * /* w  */ );

double lecroy9424_get_amplitude( int        /* ch */,
								 Window_T * /* w  */ );

void lecroy9424_free_running( void );

void lecroy9424_copy_curve( long /* src  */,
							long /* dest */ );

bool lecroy9424_command( const char * /* cmd */ );


const char *lecroy9424_ptime( double /* p_time */ );

void lecroy9424_delete_windows( LECROY9424_T * /* s */ );

Window_T *lecroy9424_get_window_by_number( long /* wid */ );

double lecroy9424_trigger_delay_check( void );

void lecroy9424_all_windows_check( void );

void lecroy9424_window_check( Window_T * /* w        */,
							  bool       /* show_num */ );

void lecroy9424_length_check( long /* len */ );

long lecroy9424_find_length( void );

long lecroy9424_curve_length( void );

double lecroy9424_time_per_point( void );

long lecroy9424_translate_channel( int  /* dir     */,
								   long /* channel */,
								   bool /* flag    */ );

void lecroy9424_store_state( LECROY9424_T * /* dest */,
							 LECROY9424_T * /* src  */ );


#endif /* ! LECROY9424_HEADER */

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
