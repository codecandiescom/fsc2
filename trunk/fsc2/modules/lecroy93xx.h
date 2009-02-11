/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#if ! defined LECROY93XX_HEADER
#define LECROY93XX_HEADER


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "lecroy93xx.conf"


#define LECROY93XX_UNDEF   -1
#define LECROY93XX_CH1      0
#define LECROY93XX_CH2      1
#define LECROY93XX_CH3      2
#define LECROY93XX_CH4      3
#define LECROY93XX_TA       4
#define LECROY93XX_TB       5
#define LECROY93XX_TC       6
#define LECROY93XX_TD       7
#define LECROY93XX_M1       8
#define LECROY93XX_M2       9
#define LECROY93XX_M3      10
#define LECROY93XX_M4      11
#define LECROY93XX_LIN     12
#define LECROY93XX_EXT     13
#define LECROY93XX_EXT10   14


#define GENERAL_TO_LECROY93XX 0
#define LECROY93XX_TO_GENERAL 1


#define LECROY93XX_CPL_INVALID       -1   /* Input coupling for data channel */
#define LECROY93XX_CPL_AC_1_MOHM      0   /* (don't change the sequence!) */
#define LECROY93XX_CPL_DC_1_MOHM      1
#define LECROY93XX_CPL_DC_50_OHM      2
#define LECROY93XX_CPL_GND            3

#define LECROY93XX_TRG_AC             0
#define LECROY93XX_TRG_DC             1
#define LECROY93XX_TRG_LF_REJ         2
#define LECROY93XX_TRG_HF_REJ         3

#define LECROY93XX_TRG_CPL_AC         0    /* Trigger couplings */
#define LECROY93XX_TRG_CPL_DC         1    /* (don't change the sequence!) */
#define LECROY93XX_TRG_CPL_LF_REJ     2
#define LECROY93XX_TRG_CPL_HF_REJ     3
#define LECROY93XX_TRG_CPL_HF         4

#define LECROY93XX_TRG_MODE_AUTO      0    /* Trigger modes */
#define LECROY93XX_TRG_MODE_NORMAL    1    /* (don't change the sequence!) */
#define LECROY93XX_TRG_MODE_SINGLE    2
#define LECROY93XX_TRG_MODE_STOP      3

#define LECROY93XX_BWL_OFF            0    /* Bandwidth limiter settings */
#define LECROY93XX_BWL_ON             1
#define LECROY93XX_BWL_200MHZ         2


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must be reasonable ! */

#define LECROY93XX_TEST_TB_INDEX     5           /* 50/20 ns per division */
#define LECROY93XX_TEST_MS_INDEX     ( lecroy93xx.num_mem_sizes - 1 )
#define LECROY93XX_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY93XX_TEST_SENSITIVITY  1.0         /* 1 V per division */
#define LECROY93XX_TEST_OFFSET       0.0
#define LECROY93XX_TEST_COUPLING     LECROY93XX_CPL_AC_1_MOHM
#define LECROY93XX_TEST_NUM_AVG      10
#define LECROY93XX_TEST_TRIG_DELAY   0           /* no pre- or posttrigger */
#define LECROY93XX_TEST_TRIG_SOURCE  LECROY93XX_CH1
#define LECROY93XX_TEST_TRIG_MODE    LECROY93XX_TRG_MODE_NORMAL
#define LECROY93XX_TEST_TRIG_LEVEL   0.0
#define LECROY93XX_TEST_TRIG_SLOPE   POSITIVE
#define LECROY93XX_TEST_TRIG_COUP    LECROY93XX_TRG_CPL_DC
#define LECROY93XX_TEST_TRIG_MODE    LECROY93XX_TRG_MODE_NORMAL
#define LECROY93XX_TEST_BWL          LECROY93XX_BWL_OFF


/* Flags from the INR (internal state change) register indicating that an
   acquisition or a waveform processing has finished */

#define LECROY93XX_SIGNAL_ACQ      ( 1 <<  0 )
#define LECROY93XX_PROC_DONE_TA    ( 1 <<  8 )
#define LECROY93XX_PROC_DONE_TB    ( 1 <<  9 )
#define LECROY93XX_PROC_DONE_TC    ( 1 << 10 )
#define LECROY93XX_PROC_DONE_TD    ( 1 << 11 )
#define LECROY93XX_PROC_DONE( ch ) ( 1 << ( 8 + ch - LECROY93XX_TA ) )



/* Total number of channels */

#define LECROY93XX_MAX_CHANNELS       12
#define LECROY93XX_TOTAL_CHANNELS     15


/* Include a file with model dependend settings (it needs to be included
 * here since it relies on some of the above definitions */

#include "lecroy93xx_models.h"


/* Some typedefs used below */

typedef struct Window Window_T;
typedef struct LECROY93XX LECROY93XX_T;
typedef struct HORI_RES HORI_RES_T;


/* Structure for description of a 'window' on the digitizer, made up from
   the area between the pair of cursors */

struct Window {
    long num;                   /* number of window                          */
    double start;               /* start of window (in time units)           */
    double width;               /* width of window (in time units)           */
    long start_num;             /* first point of window                     */
    long end_num;               /* last point of window                      */
    long num_points;            /* number of data points between the cursors */
    Window_T *next;             /* pointer to next window structure          */
    Window_T *prev;             /* pointer to previous window structure      */
};


/* Structure for the time resolution ("time per point") and number of samples
   ("points per division") for SS and RIS mode (we need one such structure
   for each memory size and timebase setting) */

struct HORI_RES {
    double tpp;         /* time resolution (SS mode) */
    long   cl;          /* number of points of curve (SS mode) */
    double tpp_ris;     /* time resolution (RIS mode)  */
    long   cl_ris;      /* number of points of curve (RIS mode) */
};


struct LECROY93XX {
    int device;

    unsigned int INR;

    bool is_displayed[ LECROY93XX_MAX_CHANNELS ];

    int num_used_channels;

    double timebase;
    int tb_index;           /* index into 'tbas' for current timebase */
    bool is_timebase;

    bool interleaved;       /* set if in RIS mode, unset in SS mode */
    bool is_interleaved;

    long num_mem_sizes;      /* number of memory size settings */
    long *mem_sizes;         /* allowed memory size settings */

    long mem_size;
    int ms_index;           /* index into 'mem_sizes' for current setting */
    bool is_mem_size;

    double sens[ LECROY93XX_MAX_CHANNELS ];
    bool is_sens[ LECROY93XX_MAX_CHANNELS ];

    double offset[ LECROY93XX_MAX_CHANNELS ];
    bool is_offset[ LECROY93XX_MAX_CHANNELS ];

    int bandwidth_limiter;
    bool is_bandwidth_limiter;

    int coupling[ LECROY93XX_MAX_CHANNELS ];
    bool is_coupling[ LECROY93XX_MAX_CHANNELS ];

    int trigger_source;
    bool is_trigger_source;

    double trigger_level[ LECROY93XX_TOTAL_CHANNELS ];
    bool is_trigger_level[ LECROY93XX_TOTAL_CHANNELS ];

    bool trigger_slope[ LECROY93XX_TOTAL_CHANNELS ];
    bool is_trigger_slope[ LECROY93XX_TOTAL_CHANNELS ];

    int trigger_coupling[ LECROY93XX_TOTAL_CHANNELS ];
    bool is_trigger_coupling[ LECROY93XX_TOTAL_CHANNELS ];

    double trigger_delay;
    bool is_trigger_delay;

    int trigger_mode;
    bool is_trigger_mode;

    bool is_avg_setup[ LECROY93XX_MAX_CHANNELS ];
    long source_ch[ LECROY93XX_MAX_CHANNELS ];
    long num_avg[ LECROY93XX_MAX_CHANNELS ];

    Window_T *w;           /* start element of list of windows               */
    int num_windows;

    bool channels_in_use[ LECROY93XX_MAX_CHANNELS ];

    long num_tbas;           /* number of timebase settings */
    double *tbas;            /* allowed timebase settings */

    HORI_RES_T **hres;       /* table of points per division and time reso-
                                lutions for the different record lengths
                                and timebases */
    HORI_RES_T *cur_hres;    /* pointer into table to current settings */
};


extern LECROY93XX_T lecroy93xx;
extern const char *LECROY93XX_Channel_Names[ 15 ];
extern bool lecroy93xx_IN_SETUP;


enum {
    SAMPLE,
    AVERAGE
};


#if defined LECROY93XX_MAIN_
#if LECROY93XX_CH_MAX >= LECROY93XX_CH4
int trg_channels[ 7 ] = { LECROY93XX_CH1,
                          LECROY93XX_CH2,
                          LECROY93XX_CH3,
                          LECROY93XX_CH4,
                          LECROY93XX_LIN,
                          LECROY93XX_EXT,
                          LECROY93XX_EXT10
                        };
#else
int trg_channels[ 5 ] = { LECROY93XX_CH1,
                          LECROY93XX_CH2,
                          LECROY93XX_LIN,
                          LECROY93XX_EXT,
                          LECROY93XX_EXT10
                        };
#endif
#else
#if LECROY93XX_CH_MAX >= LECROY93XX_CH4
extern int trg_channels[ 7 ];
#else
extern int trg_channels[ 5 ];
#endif
#endif

/* declaration of exported functions */

int lecroy93xx_init_hook(       void );
int lecroy93xx_test_hook(       void );
int lecroy93xx_exp_hook(        void );
int lecroy93xx_end_of_exp_hook( void );
void lecroy93xx_exit_hook(      void );


Var_T * digitizer_name(              Var_T * /* v */ );
Var_T * digitizer_define_window(     Var_T * /* v */ );
Var_T * digitizer_change_window(     Var_T * /* v */ );
Var_T * digitizer_window_position(   Var_T * /* v */ );
Var_T * digitizer_window_width(      Var_T * /* v */ );
Var_T * digitizer_timebase(          Var_T * /* v */ );
Var_T * digitizer_interleave_mode(   Var_T * /* v */ );
Var_T * digitizer_memory_size(       Var_T * /* v */ );
Var_T * digitizer_record_length(     Var_T * /* v */ );
Var_T * digitizer_time_per_point(    Var_T * /* v */ );
Var_T * digitizer_sensitivity(       Var_T * /* v */ );
Var_T * digitizer_offset(            Var_T * /* v */ );
Var_T * digitizer_coupling(          Var_T * /* v */ );
Var_T * digitizer_bandwidth_limiter( Var_T * /* v */ );
Var_T * digitizer_trigger_channel(   Var_T * /* v */ );
Var_T * digitizer_trigger_level(     Var_T * /* v */ );
Var_T * digitizer_trigger_slope(     Var_T * /* v */ );
Var_T * digitizer_trigger_coupling(  Var_T * /* v */ );
Var_T * digitizer_trigger_mode(      Var_T * /* v */ );
Var_T * digitizer_trigger_delay(     Var_T * /* v */ );
Var_T * digitizer_averaging(         Var_T * /* v */ );
Var_T * digitizer_num_averages(      Var_T * /* v */ );
Var_T * digitizer_trigger_position(  Var_T * /* v */ );
Var_T * digitizer_meas_channel_ok(   Var_T * /* v */ );
Var_T * digitizer_start_acquisition( Var_T * /* v */ );
Var_T * digitizer_get_curve(         Var_T * /* v */ );
Var_T * digitizer_get_area(          Var_T * /* v */ );
Var_T * digitizer_get_amplitude(     Var_T * /* v */ );
Var_T * digitizer_copy_curve(        Var_T * /* v */ );
Var_T * digitizer_command(           Var_T * /* v */ );


/* declaration of internally used functions */

bool lecroy93xx_init( const char * /* name */ );

double lecroy93xx_get_timebase( void );

bool lecroy93xx_set_timebase( double /* timebase */ );

bool lecroy93xx_get_interleaved( void );

bool lecroy93xx_set_interleaved( bool /* state */ );

long lecroy93xx_get_memory_size( void );

bool lecroy93xx_set_memory_size( long /* mem_size */ );

double lecroy93xx_get_sens( int /* channel */ );

bool lecroy93xx_set_sens( int    /* channel */,
                          double /* sens    */  );

double lecroy93xx_get_offset( int /* channel */ );

bool lecroy93xx_set_offset( int    /* channel */,
                            double /* offset  */  );

int lecroy93xx_get_coupling( int /* channel */ );

bool lecroy93xx_set_coupling( int /* channel */,
                              int /* type    */  );

int lecroy93xx_get_bandwidth_limiter( void );

bool lecroy93xx_set_bandwidth_limiter( int /* bwl */  );

int lecroy93xx_get_trigger_source( void );

bool lecroy93xx_set_trigger_source( int /* channel */ );

double lecroy93xx_get_trigger_level( int /* channel */ );

bool lecroy93xx_set_trigger_level( int    /* channel */,
                                   double /* level   */  );

bool lecroy93xx_get_trigger_slope( int /* channel */ );

bool lecroy93xx_set_trigger_slope( int  /* channel */,
                                   bool /* slope   */  );

int lecroy93xx_get_trigger_coupling( int /* channel */ );

int lecroy93xx_set_trigger_coupling( int /* channel */,
                                     int /* cpl     */  );

int lecroy93xx_get_trigger_mode( void );

int lecroy93xx_set_trigger_mode( int /* mode */ );

double lecroy93xx_get_trigger_delay( void );

bool lecroy93xx_set_trigger_delay( double /* delay */ );

bool lecroy93xx_is_displayed( int /* channel */ );

bool lecroy93xx_display( int /* channel */,
                         int /* on_off  */  );

long lecroy93xx_get_num_avg( int /* channel */ );

bool lecroy93xx_get_desc( int /* channel */ );

void lecroy93xx_set_up_averaging( long /* channel */,
                                  long /* source  */,
                                  long /* num_avg */,
                                  long /* rec_len */  );

void lecroy93xx_finished( void );

void lecroy93xx_start_acquisition( void );

void lecroy93xx_get_curve( int        /* ch     */,
                           Window_T * /* w      */,
                           double **  /* array  */,
                           long *     /* length */  );

double lecroy93xx_get_area( int        /* ch */,
                            Window_T * /* w  */  );

double lecroy93xx_get_amplitude( int        /* ch */,
                                 Window_T * /* w  */  );

void lecroy93xx_copy_curve( long /* src  */,
                            long /* dest */  );

bool lecroy93xx_command( const char * /* cmd */ );

void lecroy93xx_soe_checks( void );

void lecroy93xx_exit_cleanup( void );

const char * lecroy93xx_ptime( double /* p_time */ );

double lecroy93xx_trigger_delay_check( void );

void lecroy93xx_delete_windows( LECROY93XX_T * /* s */ );

Window_T * lecroy93xx_get_window_by_number( long /* wid */ );

void lecroy93xx_all_windows_check( void );

void lecroy93xx_window_check( Window_T * /* w        */,
                              bool       /* show_num */  );

long lecroy93xx_curve_length( void );

double lecroy93xx_time_per_point( void );

long lecroy93xx_translate_channel( int  /* dir     */,
                                   long /* channel */,
                                   bool /* flag    */  );

void lecroy93xx_store_state( LECROY93XX_T * /* dest */,
                             LECROY93XX_T * /* src  */  );


void lecroy93xx_numpoints_prep( void );

void lecroy93xx_tbas_prep( void );

void lecroy93xx_hori_res_prep( void );

void lecroy93xx_clean_up( void );


#endif /* ! defined LECROY93XX_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
