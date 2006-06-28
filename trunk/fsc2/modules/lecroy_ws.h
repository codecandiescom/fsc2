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


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "lecroy_ws.conf"


#define LECROY_WS_UNDEF   -1
#define LECROY_WS_CH1      0
#define LECROY_WS_CH2      1
#if ( defined WS424 || defined WS434 || defined WS454 )
#define LECROY_WS_CH3      2
#define LECROY_WS_CH4      3
#define LECROY_WS_CH_MAX   LECROY_WS_CH4
#else
#define LECROY_WS_CH_MAX   LECROY_WS_CH2
#endif
#define LECROY_WS_MATH     4
#define LECROY_WS_Z1       5
#define LECROY_WS_Z2       6
#define LECROY_WS_Z3       7
#define LECROY_WS_Z4       8
#define LECROY_WS_M1       9
#define LECROY_WS_M2      10
#define LECROY_WS_M3      11
#define LECROY_WS_M4      12
#define LECROY_WS_LIN     13
#define LECROY_WS_EXT     14
#define LECROY_WS_EXT10   15


#define GENERAL_TO_LECROY_WS 0
#define LECROY_WS_TO_GENERAL 1


#define LECROY_WS_CPL_INVALID       -1   /* Input coupling for data channel */
#define LECROY_WS_CPL_AC_1_MOHM      0   /* (don't change the sequence!) */
#define LECROY_WS_CPL_DC_1_MOHM      1
#define LECROY_WS_CPL_DC_50_OHM      2
#define LECROY_WS_CPL_GND            3

#define LECROY_WS_TRG_AC             0
#define LECROY_WS_TRG_DC             1
#define LECROY_WS_TRG_LF_REJ         2
#define LECROY_WS_TRG_HF_REJ         3

#define LECROY_WS_TRG_CPL_AC         0    /* Trigger couplings */
#define LECROY_WS_TRG_CPL_DC         1    /* (don't change the sequence!) */
#define LECROY_WS_TRG_CPL_LF_REJ     2
#define LECROY_WS_TRG_CPL_HF_REJ     3
#define LECROY_WS_TRG_CPL_HF         4

#define LECROY_WS_TRG_MODE_AUTO      0    /* Trigger modes */
#define LECROY_WS_TRG_MODE_NORMAL    1    /* (don't change the sequence!) */
#define LECROY_WS_TRG_MODE_SINGLE    2
#define LECROY_WS_TRG_MODE_STOP      3

#define LECROY_WS_BWL_OFF            0    /* Bandwidth limiter settings */
#define LECROY_WS_BWL_ON             1
#if ( defined WS432 || defined WS434 || defined WS452 || defined WS454 )
#define LECROY_WS_BWL_200MHZ         2
#endif



/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must be reasonable ! */

#define LECROY_WS_TEST_TB_INDEX     5           /* 50/20 ns per division */
#define LECROY_WS_TEST_MS_INDEX     ( lecroy_ws.num_mem_sizes - 1 )
#define LECROY_WS_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY_WS_TEST_SENSITIVITY  2.5         /* 2.5 V per division */
#define LECROY_WS_TEST_OFFSET       0.0
#define LECROY_WS_TEST_COUPLING     LECROY_WS_CPL_DC_1_MOHM
#define LECROY_WS_TEST_NUM_AVG      10
#define LECROY_WS_TEST_TRIG_DELAY   0           /* no pre- or posttrigger */
#define LECROY_WS_TEST_TRIG_SOURCE  LECROY_WS_CH1
#define LECROY_WS_TEST_TRIG_MODE    LECROY_WS_TRG_MODE_NORMAL
#define LECROY_WS_TEST_TRIG_LEVEL   0.0
#define LECROY_WS_TEST_TRIG_SLOPE   POSITIVE
#define LECROY_WS_TEST_TRIG_COUP    LECROY_WS_TRG_CPL_DC
#define LECROY_WS_TEST_TRIG_MODE    LECROY_WS_TRG_MODE_NORMAL
#define LECROY_WS_TEST_BWL          LECROY_WS_BWL_OFF /* bandwidth limiter */


/* Flag from the INR (internal state change) register indicating that an
   acquisition is finished */

#define LECROY_WS_SIGNAL_ACQ      ( 1 <<  0 )


/* Total number of channels and maximum number of channels that can be
   displayed at once */

#define LECROY_WS_MAX_CHANNELS       13
#define LECROY_WS_MAX_USED_CHANNELS   8


/* Maximum and minimum sensitivity */

#define LECROY_WS_MAX_SENS           1.0e-3     /* 1 mV / div */
#define LECROY_WS_MIN_SENS_50OHM     2.0        /* 2.0 V / div */
#define LECROY_WS_MIN_SENS_1MOHM     10.0       /* 10.0 V / div */


/* The trigger delay can be set with a resolution of 1/10 of the timebase */

#define LECROY_WS_TRIG_DELAY_RESOLUTION 0.1


/* Maximum factors or values for trigger levels */

#define LECROY_WS_TRG_MAX_LEVEL_CH_FAC 5.0    /* 5 times sensitivity */
#define LECROY_WS_TRG_MAX_LEVEL_EXT    0.5    /* 500 mV */
#define LECROY_WS_TRG_MAX_LEVEL_EXT10  5.0    /* 5 V */



/* Define the maximum number of sweeps that can be averaged
*/

#define LECROY_WS_MAX_AVERAGES     1000000


/* Definition of the shortest possible record length (i.e. the number of
   samples for an aquisition) - this is identical for all models */

#define LECROY_WS_MIN_MEMORY_SIZE    500


/* Model dependend minimum timebase */

#if ( defined WS422 || defined WS424 )
#define LECROY_WS_MIN_TIME_BASE      1.0e-9
#endif


#if ( defined WS432 || defined WS434 )
#define LECROY_WS_MIN_TIME_BASE      5.0e-10
#endif


#if ( defined WS452 || defined WS454 )
#define LECROY_WS_MIN_TIME_BASE      2.0e-10
#endif



typedef struct Window Window_T;
typedef struct LECROY_WS LECROY_WS_T;
typedef struct HORI_RES HORI_RES_T;


/* Structure for description of a 'window' on the digitizer, made up from
   the area between the pair of cursors */

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


/* Structure for the time resolution ("time per point") and number of samples
   ("points per division") for SS and RIS mode (we need one such structure
   for each memory size and timebase setting) */


struct HORI_RES {
	double tpp;         /* time resolution (SINGLE SHOT mode) */
	long   ppd;         /* points per division (SINGLE SHOT mode) */
	double tpp_ris;     /* time resolution (RIS mode)  */
	long   ppd_ris;     /* points per division (RIS mode) */
};


struct LECROY_WS {
	int device;

	unsigned int INR;

	bool is_displayed[ LECROY_WS_MAX_CHANNELS ];

	int num_used_channels;

	double timebase;
	int tb_index;           /* index into 'tbas' for current timebase */
	bool is_timebase;

	bool interleaved;       /* set if in RIS mode, unset in SINGLE SHOT mode */
	bool is_interleaved;

	long num_mem_sizes;      /* number of memory size settings */
	long *mem_sizes;         /* allowed memory size settings */

	long mem_size;
	int ms_index;           /* index into 'mem_sizes' for current setting */
	bool is_mem_size;

	double sens[ LECROY_WS_MAX_CHANNELS ];
	bool is_sens[ LECROY_WS_MAX_CHANNELS ];

	double offset[ LECROY_WS_MAX_CHANNELS ];
	bool is_offset[ LECROY_WS_MAX_CHANNELS ];

	int bandwidth_limiter[ LECROY_WS_MAX_CHANNELS ];
	bool is_bandwidth_limiter[ LECROY_WS_MAX_CHANNELS ];

	int coupling[ LECROY_WS_MAX_CHANNELS ];
	bool is_coupling[ LECROY_WS_MAX_CHANNELS ];
	bool need_high_imp[ LECROY_WS_MAX_CHANNELS ];

	int trigger_source;
	bool is_trigger_source;

	double trigger_level[ LECROY_WS_MAX_CHANNELS ];
	bool is_trigger_level[ LECROY_WS_MAX_CHANNELS ];

	bool trigger_slope[ LECROY_WS_MAX_CHANNELS ];
	bool is_trigger_slope[ LECROY_WS_MAX_CHANNELS ];

	int trigger_coupling[ LECROY_WS_MAX_CHANNELS ];
	bool is_trigger_coupling[ LECROY_WS_MAX_CHANNELS ];

	double trigger_delay;
	bool is_trigger_delay;

	int trigger_mode;
	bool is_trigger_mode;

	bool is_avg_setup[ LECROY_WS_MAX_CHANNELS ];
	long source_ch[ LECROY_WS_MAX_CHANNELS ];
	long num_avg[ LECROY_WS_MAX_CHANNELS ];

	Window_T *w;           /* start element of list of windows               */
	int num_windows;

	bool channels_in_use[ LECROY_WS_MAX_CHANNELS ];

	long num_tbas;           /* number of timebase settings */
	double *tbas;            /* allowed timebase settings */

	HORI_RES_T **hres;       /* table of points per division and time reso-
								lutions for the different record lengths
								and timebases */
	HORI_RES_T *cur_hres;    /* pointer into table to current settings */
};


extern LECROY_WS_T lecroy_ws;
extern const char *LECROY_WS_Channel_Names[ 16 ];
extern bool lecroy_ws_IN_SETUP;


enum {
	SAMPLE,
	AVERAGE
};


/* declaration of exported functions */

int lecroy_ws_init_hook(       void );
int lecroy_ws_test_hook(       void );
int lecroy_ws_exp_hook(        void );
int lecroy_ws_end_of_exp_hook( void );
void lecroy_ws_exit_hook(      void );


Var_T *digitizer_name(              Var_T * /* v */ );
Var_T *digitizer_define_window(     Var_T * /* v */ );
Var_T *digitizer_change_window(     Var_T * /* v */ );
Var_T *digitizer_window_position(   Var_T * /* v */ );
Var_T *digitizer_window_width(      Var_T * /* v */ );
Var_T *digitizer_timebase(          Var_T * /* v */ );
Var_T *digitizer_interleave_mode(   Var_T * /* v */ );
Var_T *digitizer_memory_size(       Var_T * /* v */ );
Var_T *digitizer_record_length(     Var_T * /* v */ );
Var_T *digitizer_time_per_point(    Var_T * /* v */ );
Var_T *digitizer_sensitivity(       Var_T * /* v */ );
Var_T *digitizer_offset(            Var_T * /* v */ );
Var_T *digitizer_coupling(          Var_T * /* v */ );
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
Var_T *digitizer_copy_curve(        Var_T * /* v */ );
Var_T *digitizer_command(           Var_T * /* v */ );


/* declaration of internally used functions */

bool lecroy_ws_init( const char * /* name */ );

double lecroy_ws_get_timebase( void );

bool lecroy_ws_set_timebase( double /* timebase */ );

bool lecroy_ws_get_interleaved( void );

bool lecroy_ws_set_interleaved( bool /* state */ );

long lecroy_ws_get_memory_size( void );

bool lecroy_ws_set_memory_size( long /* mem_size */ );

double lecroy_ws_get_sens( int /* channel */ );

bool lecroy_ws_set_sens( int    /* channel */,
						 double /* sens    */ );

double lecroy_ws_get_offset( int /* channel */ );

bool lecroy_ws_set_offset( int    /* channel */,
						   double /* offset  */ );

int lecroy_ws_get_coupling( int /* channel */ );

bool lecroy_ws_set_coupling( int /* channel */,
							 int /* type    */ );

int lecroy_ws_get_bandwidth_limiter( int /* channel */ );

bool lecroy_ws_set_bandwidth_limiter( int /* channel */,
									  int /* bwl     */ );

int lecroy_ws_get_trigger_source( void );

bool lecroy_ws_set_trigger_source( int /* channel */ );

double lecroy_ws_get_trigger_level( int /* channel */ );

bool lecroy_ws_set_trigger_level( int    /* channel */,
								  double /* level   */ );

bool lecroy_ws_get_trigger_slope( int /* channel */ );

bool lecroy_ws_set_trigger_slope( int  /* channel */,
								  bool /* slope   */ );

int lecroy_ws_get_trigger_coupling( int /* channel */ );

int lecroy_ws_set_trigger_coupling( int /* channel */,
									int /* cpl     */ );

int lecroy_ws_get_trigger_mode( void );

int lecroy_ws_set_trigger_mode( int /* mode */ );

double lecroy_ws_get_trigger_delay( void );

bool lecroy_ws_set_trigger_delay( double /* delay */ );

bool lecroy_ws_is_displayed( int /* channel */ );

bool lecroy_ws_display( int /* channel */,
						int /* on_off  */ );

long lecroy_ws_get_num_avg( int /* channel */ );

bool lecroy_ws_get_desc( int /* channel */ );

void lecroy_ws_set_up_averaging( long /* channel */,
								 long /* source  */,
								 long /* num_avg */,
								 long /* rec_len */ );

void lecroy_ws_finished( void );

void lecroy_ws_start_acquisition( void );

void lecroy_ws_get_curve( int        /* ch     */,
						  Window_T * /* w      */,
						  double **  /* array  */,
						  long *     /* length */ );

double lecroy_ws_get_area( int        /* ch */,
						   Window_T * /* w  */ );

double lecroy_ws_get_amplitude( int        /* ch */,
								Window_T * /* w  */ );

void lecroy_ws_copy_curve( long /* src  */,
						   long /* dest */ );

bool lecroy_ws_command( const char * /* cmd */ );

const char *lecroy_ws_ptime( double /* p_time */ );

double lecroy_ws_trigger_delay_check( void );

void lecroy_ws_delete_windows( LECROY_WS_T * /* s */ );

Window_T *lecroy_ws_get_window_by_number( long /* wid */ );

void lecroy_ws_all_windows_check( void );

void lecroy_ws_window_check( Window_T * /* w        */,
							 bool       /* show_num */ );

long lecroy_ws_curve_length( void );

double lecroy_ws_time_per_point( void );

long lecroy_ws_translate_channel( int  /* dir     */,
								  long /* channel */,
								  bool /* flag    */ );

void lecroy_ws_store_state( LECROY_WS_T * /* dest */,
							LECROY_WS_T * /* src  */ );


void lecroy_ws_numpoints_prep( void );

void lecroy_ws_tbas_prep( void );

void lecroy_ws_hori_res_prep( void );

void lecroy_ws_clean_up( void );

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
