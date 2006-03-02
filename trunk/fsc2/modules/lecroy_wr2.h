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

#include "lecroy_wr2.conf"


#define LECROY_WR2_UNDEF   -1
#define LECROY_WR2_CH1      0
#define LECROY_WR2_CH2      1
#if ! ( defined LT262 || defined LT372 || defined LT584 )
#define LECROY_WR2_CH3      2
#define LECROY_WR2_CH4      3
#define LECORY_WR2_CH_MAX   LECROY_WR2_CH4
#else
#define LECORY_WR2_CH_MAX   LECROY_WR2_CH2
#endif
#define LECROY_WR2_TA       4
#define LECROY_WR2_TB       5
#define LECROY_WR2_TC       6
#define LECROY_WR2_TD       7
#define LECROY_WR2_M1       8
#define LECROY_WR2_M2       9
#define LECROY_WR2_M3      10
#define LECROY_WR2_M4      11
#define LECROY_WR2_LIN     12
#define LECROY_WR2_EXT     13
#define LECROY_WR2_EXT10   14


#define GENERAL_TO_LECROY_WR2 0
#define LECROY_WR2_TO_GENERAL 1


#define LECROY_WR2_INVALID_COUPL     -1   /* Input coupling for data channel */
#define LECROY_WR2_AC_1_MOHM          0   /* (don't change the sequence!) */
#define LECROY_WR2_DC_1_MOHM          1
#define LECROY_WR2_DC_50_OHM          2
#define LECROY_WR2_GND                3

#define TRG_AC             0
#define TRG_DC             1
#define TRG_LF_REJ         2
#define TRG_HF_REJ         3

#define TRG_CPL_AC         0         /* Trigger couplings */
#define TRG_CPL_DC         1         /* (don't change the sequence!) */
#define TRG_CPL_LF_REJ     2
#define TRG_CPL_HF_REJ     3
#define TRG_CPL_HF         4

#define TRG_MODE_AUTO      0         /* Trigger modes */
#define TRG_MODE_NORMAL    1         /* (don't change the sequence!) */
#define TRG_MODE_SINGLE    2
#define TRG_MODE_STOP      3


#define LECROY_WR2_BWL_OFF            0       /* Bandwidth limiter settings */
#define LECROY_WR2_BWL_ON             1
#define LECROY_WR2_BWL_200MHZ         2



/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must be reasonable ! */

#define LECROY_WR2_TEST_TB_INDEX     5           /* 50 ns per division*/
#define LECROY_WR2_TEST_MS_INDEX     ( lecroy_wr2.num_mem_sizes - 1 )
#define LECROY_WR2_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY_WR2_TEST_SENSITIVITY  2.5         /* 2.5 V per division */
#define LECROY_WR2_TEST_OFFSET       0.0
#define LECROY_WR2_TEST_COUPLING     LECROY_WR2_DC_1_MOHM
#define LECROY_WR2_TEST_NUM_AVG      10
#define LECROY_WR2_TEST_TRIG_DELAY   1.0e-8      /* 10 ns pretrigger */
#define LECROY_WR2_TEST_TRIG_SOURCE  LECROY_WR2_CH1
#define LECROY_WR2_TEST_TRIG_MODE    TRG_MODE_NORMAL
#define LECROY_WR2_TEST_TRIG_LEVEL   0.0
#define LECROY_WR2_TEST_TRIG_SLOPE   POSITIVE
#define LECROY_WR2_TEST_TRIG_COUP    TRG_CPL_DC
#define LECROY_WR2_TEST_TRIG_MODE    TRG_MODE_NORMAL
#define LECROY_WR2_TEST_BWL          LECROY_WR2_BWL_OFF /* bandwidth limiter */


/* Flags from the INR (internal state change) register indicating that an
   acquisition or a waveform processing has finished */

#define LECROY_WR2_SIGNAL_ACQ      ( 1 <  0 )
#define LECROY_WR2_PROC_DONE_TA    ( 1 <  8 )
#define LECROY_WR2_PROC_DONE_TB    ( 1 <  9 )
#define LECROY_WR2_PROC_DONE_TC    ( 1 < 10 )
#define LECROY_WR2_PROC_DONE_TD    ( 1 < 11 )
#define LECROY_WR2_PROC_DONE( ch ) ( 1 < ( 8 + ch - LECROY_WR2_CH1 ) )



/* Total number of channels and maximum number of channels that can be
   displayed at once */

#define LECROY_WR2_MAX_CHANNELS       12
#define LECROY_WR2_MAX_USED_CHANNELS   8


/* Maximum and minimum sensitivity [V/div]

#define LECROY_WR2_MAX_SENS     5.0e-3     /* 5 mV */
#define LECROY_WR2_MIN_SENS     2.5        /* 2.5 V */


/* Maximum factors or values for trigger levels */

#define LECROY_WR2_TRG_MAX_LEVEL_CH_FAC 5.0    /* 5 times sensitivity */
#define LECROY_WR2_TRG_MAX_LEVEL_EXT    0.5    /* 500 mV */
#define LECROY_WR2_TRG_MAX_LEVEL_EXT10  5.0    /* 5 V */



/* Definition of the shortest possible record length (i.e. te number of
   points for a single channel) - this seems to be identical for all models */

#define LECROY_WR2_MIN_MEM_SIZE    500


/* Model dependend values */

#if defined LT262
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH2
#define LECROY_WR2_MAX_SAMPLE_RATE  1000000000L    /* 1 GS */
#endif


#if defined LT264
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH4
#define LECROY_WR2_MAX_SAMPLE_RATE  1000000000L    /* 1 GS */
#endif


#if defined LT354
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH4
#define LECROY_WR2_MAX_SAMPLE_RATE  1000000000L    /* 1 GS */
#endif


#if defined LT372
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH2
#define LECROY_WR2_MAX_SAMPLE_RATE  2000000000L    /* 2 GS */
#endif


#if defined LT374
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH4
#define LECROY_WR2_MAX_SAMPLE_RATE  2000000000L    /* 2 GS */
#endif


#if defined LT584
#define LECROY_WR2_CH_MAX           LECROY_WR2_CH2
#define LECROY_WR2_MAX_SAMPLE_RATE  2000000000L    /* 2 GS */
#endif


/* Structure for description of a 'window' on the digitizer, made up from
   the area between the pair of cursors */

typedef struct Window Window_T;
typedef struct LECROY_WR2 LECROY_WR2_T;
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


struct HORI_RES {
	double tpp;         /* time resolution (SINGLE SHOT mode) */
	long   ppd;         /* points per division (SINGLE SHOT mode) */
	double tpp_ris;     /* time resolution (RIS mode)  */
	long   ppd_ris;     /* points per division (RIS mode) */
};


struct LECROY_WR2 {
	int device;

	unsigned int INR;

	bool is_displayed[ LECROY_WR2_MAX_CHANNELS ];

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

	double sens[ LECROY_WR2_MAX_CHANNELS ];
	bool is_sens[ LECROY_WR2_MAX_CHANNELS ];

	double offset[ LECROY_WR2_MAX_CHANNELS ];
	bool is_offset[ LECROY_WR2_MAX_CHANNELS ];

	int bandwidth_limiter[ LECROY_WR2_MAX_CHANNELS ];
	bool is_bandwidth_limiter[ LECROY_WR2_MAX_CHANNELS ];

	int coupling[ LECROY_WR2_MAX_CHANNELS ];
	bool is_coupling[ LECROY_WR2_MAX_CHANNELS ];

	int trigger_source;
	bool is_trigger_source;

	double trigger_level[ LECROY_WR2_MAX_CHANNELS ];
	bool is_trigger_level[ LECROY_WR2_MAX_CHANNELS ];

	bool trigger_slope[ LECROY_WR2_MAX_CHANNELS ];
	bool is_trigger_slope[ LECROY_WR2_MAX_CHANNELS ];

	int trigger_coupling[ LECROY_WR2_MAX_CHANNELS ];
	bool is_trigger_coupling[ LECROY_WR2_MAX_CHANNELS ];

	double trigger_delay;
	bool is_trigger_delay;

	int trigger_mode;
	bool is_trigger_mode;

	bool is_avg_setup[ LECROY_WR2_MAX_CHANNELS ];
	long source_ch[ LECROY_WR2_MAX_CHANNELS ];
	long num_avg[ LECROY_WR2_MAX_CHANNELS ];

	Window_T *w;           /* start element of list of windows               */
	int num_windows;

	bool channels_in_use[ LECROY_WR2_MAX_CHANNELS ];

	long num_tbas;           /* number of timebase settings */
	double *tbas;            /* allowed timebase settings */

	HORI_RES_T **hres;       /* table of points per division and time reso-
								lutions for the different record lengths
								and timebases */
	HORI_RES_T *cur_hres;    /* pointer into table to current settings */
};


extern LECROY_WR2_T lecroy_wr2;
extern const char *LECROY_WR2_Channel_Names[ 15 ];
extern bool lecroy_wr2_IN_SETUP;


enum {
	SAMPLE,
	AVERAGE
};


/* declaration of exported functions */

int lecroy_wr2_init_hook(       void );
int lecroy_wr2_test_hook(       void );
int lecroy_wr2_exp_hook(        void );
int lecroy_wr2_end_of_exp_hook( void );
void lecroy_wr2_exit_hook(      void );


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

bool lecroy_wr2_init( const char * /* name */ );

double lecroy_wr2_get_timebase( void );

bool lecroy_wr2_set_timebase( double /* timebase */ );

bool lecroy_wr2_get_interleaved( void );

bool lecroy_wr2_set_interleaved( bool /* state */ );

long lecroy_wr2_get_memory_size( void );

bool lecroy_wr2_set_memory_size( long /* mem_size */ );

double lecroy_wr2_get_sens( int /* channel */ );

bool lecroy_wr2_set_sens( int    /* channel */,
						  double /* sens    */ );

double lecroy_wr2_get_offset( int /* channel */ );

bool lecroy_wr2_set_offset( int    /* channel */,
							double /* offset  */ );

int lecroy_wr2_get_coupling( int /* channel */ );

bool lecroy_wr2_set_coupling( int /* channel */,
							  int /* type    */ );

int lecroy_wr2_get_bandwidth_limiter( int /* channel */ );

bool lecroy_wr2_set_bandwidth_limiter( int /* channel */,
									   int /* bwl     */ );

int lecroy_wr2_get_trigger_source( void );

bool lecroy_wr2_set_trigger_source( int /* channel */ );

double lecroy_wr2_get_trigger_level( int /* channel */ );

bool lecroy_wr2_set_trigger_level( int    /* channel */,
								   double /* level   */ );

bool lecroy_wr2_get_trigger_slope( int /* channel */ );

bool lecroy_wr2_set_trigger_slope( int  /* channel */,
								   bool /* slope   */ );

int lecroy_wr2_get_trigger_coupling( int /* channel */ );

int lecroy_wr2_set_trigger_coupling( int /* channel */,
									 int /* cpl     */ );

int lecroy_wr2_get_trigger_mode( void );

int lecroy_wr2_set_trigger_mode( int /* mode */ );

double lecroy_wr2_get_trigger_delay( void );

bool lecroy_wr2_set_trigger_delay( double /* delay */ );

bool lecroy_wr2_is_displayed( int /* channel */ );

bool lecroy_wr2_display( int /* channel */,
						 int /* on_off  */ );

long lecroy_wr2_get_num_avg( int /* channel */ );

bool lecroy_wr2_get_desc( int /* channel */ );

void lecroy_wr2_set_up_averaging( long /* channel */,
								  long /* source  */,
								  long /* num_avg */,
								  long /* rec_len */ );

void lecroy_wr2_finished( void );

void lecroy_wr2_start_acquisition( void );

void lecroy_wr2_get_curve( int        /* ch     */,
						   Window_T * /* w      */,
						   double **  /* array  */,
						   long *     /* length */ );

double lecroy_wr2_get_area( int        /* ch */,
							Window_T * /* w  */ );

double lecroy_wr2_get_amplitude( int        /* ch */,
								 Window_T * /* w  */ );

void lecroy_wr2_free_running( void );

void lecroy_wr2_copy_curve( long /* src  */,
							long /* dest */ );

bool lecroy_wr2_command( const char * /* cmd */ );

const char *lecroy_wr2_ptime( double /* p_time */ );

double lecroy_wr2_trigger_delay_check( void );

void lecroy_wr2_delete_windows( LECROY_WR2_T * /* s */ );

Window_T *lecroy_wr2_get_window_by_number( long /* wid */ );

void lecroy_wr2_all_windows_check( void );

void lecroy_wr2_window_check( Window_T * /* w        */,
							  bool       /* show_num */ );

void lecroy_wr2_length_check( long /* len */ );

long lecroy_wr2_curve_length( void );

double lecroy_wr2_time_per_point( void );

long lecroy_wr2_translate_channel( int  /* dir     */,
								   long /* channel */,
								   bool /* flag    */ );

void lecroy_wr2_store_state( LECROY_WR2_T * /* dest */,
							 LECROY_WR2_T * /* src  */ );


void lecroy_wr2_numpoints_prep( void );

void lecroy_wr2_tbas_prep( void );

void lecroy_wr2_hori_res_prep( void );

void lecroy_wr2_clean_up( void );

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
