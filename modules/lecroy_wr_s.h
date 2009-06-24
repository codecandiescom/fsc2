/*
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


#if ! defined LECROY_WR_S_HEADER
#define LECROY_WR_HEADER_S


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "lecroy_wr_s.conf"


#define LECROY_WR_UNDEF   -1
#define LECROY_WR_CH1      0
#define LECROY_WR_CH2      1
#define LECROY_WR_CH3      2
#define LECROY_WR_CH4      3
#define LECROY_WR_TA       4
#define LECROY_WR_TB       5
#define LECROY_WR_TC       6
#define LECROY_WR_TD       7
#define LECROY_WR_M1       8
#define LECROY_WR_M2       9
#define LECROY_WR_M3      10
#define LECROY_WR_M4      11
#define LECROY_WR_LIN     12
#define LECROY_WR_EXT     13
#define LECROY_WR_EXT10   14


#define GENERAL_TO_LECROY_WR 0
#define LECROY_WR_TO_GENERAL 1


#define LECROY_WR_CPL_INVALID       -1   /* Input coupling for data channel */
#define LECROY_WR_CPL_AC_1_MOHM      0   /* (don't change the sequence!) */
#define LECROY_WR_CPL_DC_1_MOHM      1
#define LECROY_WR_CPL_DC_50_OHM      2
#define LECROY_WR_CPL_GND            3

#define LECROY_WR_TRG_AC             0
#define LECROY_WR_TRG_DC             1
#define LECROY_WR_TRG_LF_REJ         2
#define LECROY_WR_TRG_HF_REJ         3

#define LECROY_WR_TRG_CPL_AC         0    /* Trigger couplings */
#define LECROY_WR_TRG_CPL_DC         1    /* (don't change the sequence!) */
#define LECROY_WR_TRG_CPL_LF_REJ     2
#define LECROY_WR_TRG_CPL_HF_REJ     3
#define LECROY_WR_TRG_CPL_HF         4

#define LECROY_WR_TRG_MODE_AUTO      0    /* Trigger modes */
#define LECROY_WR_TRG_MODE_NORMAL    1    /* (don't change the sequence!) */
#define LECROY_WR_TRG_MODE_SINGLE    2
#define LECROY_WR_TRG_MODE_STOP      3

#define LECROY_WR_BWL_OFF            0    /* Bandwidth limiter settings */
#define LECROY_WR_BWL_ON             1
#define LECROY_WR_BWL_200MHZ         2



/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must be reasonable ! */

#define LECROY_WR_TEST_TB_INDEX     5           /* 50/20 ns per division */
#define LECROY_WR_TEST_MS_INDEX     ( lecroy_wr.num_mem_sizes - 1 )
#define LECROY_WR_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY_WR_TEST_SENSITIVITY  2.5         /* 2.5 V per division */
#define LECROY_WR_TEST_OFFSET       0.0
#define LECROY_WR_TEST_COUPLING     LECROY_WR_CPL_DC_1_MOHM
#define LECROY_WR_TEST_NUM_AVG      10
#define LECROY_WR_TEST_TRIG_DELAY   0           /* no pre- or posttrigger */
#define LECROY_WR_TEST_TRIG_SOURCE  LECROY_WR_CH1
#define LECROY_WR_TEST_TRIG_MODE    LECROY_WR_TRG_MODE_NORMAL
#define LECROY_WR_TEST_TRIG_LEVEL   0.0
#define LECROY_WR_TEST_TRIG_SLOPE   POSITIVE
#define LECROY_WR_TEST_TRIG_COUP    LECROY_WR_TRG_CPL_DC
#define LECROY_WR_TEST_TRIG_MODE    LECROY_WR_TRG_MODE_NORMAL
#define LECROY_WR_TEST_BWL          LECROY_WR_BWL_OFF /* bandwidth limiter */


/* Flags from the INR (internal state change) register indicating that an
   acquisition or a waveform processing has finished */

#define LECROY_WR_SIGNAL_ACQ      ( 1 <<  0 )
#define LECROY_WR_PROC_DONE_TA    ( 1 <<  8 )
#define LECROY_WR_PROC_DONE_TB    ( 1 <<  9 )
#define LECROY_WR_PROC_DONE_TC    ( 1 << 10 )
#define LECROY_WR_PROC_DONE_TD    ( 1 << 11 )
#define LECROY_WR_PROC_DONE( ch ) ( 1 << ( 8 + ch - LECROY_WR_TA ) )



/* Total number of channels */

#define LECROY_WR_MAX_CHANNELS       12
#define LECROY_WR_TOTAL_CHANNELS     15


/* Maximum and minimum sensitivity (in V/div) */

#define LECROY_WR_MAX_SENS           2.0e-3     /* 2 mV */
#define LECROY_WR_MIN_SENS           10.0       /* 10 V */


/* The trigger delay can be set with a resolution of 1/10 of the timebase */

#define LECROY_WR_TRIG_DELAY_RESOLUTION 0.1


/* Definition of the shortest possible record length (i.e. the number of
   samples for an aquisition) - this is identical for all models */

#define LECROY_WR_MIN_MEMORY_SIZE    500


/* Include a file with model dependend settings (it needs to be included
 * here since it relies on some of the above definitions. This only is to
 * be included after the configuration file for the device! */

#include "lecroy_wr_models.h"


/* Some typedefs used below */

typedef struct Window Window_T;
typedef struct LECROY_WR LECROY_WR_T;
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


struct LECROY_WR {
    int sn;
    struct termios *tio;    /* serial port terminal interface structure */

    unsigned int INR;

    bool is_displayed[ LECROY_WR_MAX_CHANNELS ];

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

    double sens[ LECROY_WR_MAX_CHANNELS ];
    bool is_sens[ LECROY_WR_MAX_CHANNELS ];

    double offset[ LECROY_WR_MAX_CHANNELS ];
    bool is_offset[ LECROY_WR_MAX_CHANNELS ];

    int bandwidth_limiter[ LECROY_WR_MAX_CHANNELS ];
    bool is_bandwidth_limiter[ LECROY_WR_MAX_CHANNELS ];

    int coupling[ LECROY_WR_MAX_CHANNELS ];
    bool is_coupling[ LECROY_WR_MAX_CHANNELS ];

    int trigger_source;
    bool is_trigger_source;

    double trigger_level[ LECROY_WR_TOTAL_CHANNELS ];
    bool is_trigger_level[ LECROY_WR_TOTAL_CHANNELS ];

    bool trigger_slope[ LECROY_WR_TOTAL_CHANNELS ];
    bool is_trigger_slope[ LECROY_WR_TOTAL_CHANNELS ];

    int trigger_coupling[ LECROY_WR_TOTAL_CHANNELS ];
    bool is_trigger_coupling[ LECROY_WR_TOTAL_CHANNELS ];

    double trigger_delay;
    bool is_trigger_delay;

    int trigger_mode;
    bool is_trigger_mode;

    bool is_avg_setup[ LECROY_WR_MAX_CHANNELS ];
    long source_ch[ LECROY_WR_MAX_CHANNELS ];
    long num_avg[ LECROY_WR_MAX_CHANNELS ];

    Window_T *w;           /* start element of list of windows               */
    int num_windows;

    bool channels_in_use[ LECROY_WR_MAX_CHANNELS ];

    long num_tbas;           /* number of timebase settings */
    double *tbas;            /* allowed timebase settings */

    HORI_RES_T **hres;       /* table of points per division and time reso-
                                lutions for the different record lengths
                                and timebases */
    HORI_RES_T *cur_hres;    /* pointer into table to current settings */
};


extern LECROY_WR_T lecroy_wr;
extern const char *LECROY_WR_Channel_Names[ 15 ];
extern bool lecroy_wr_IN_SETUP;


enum {
    SAMPLE,
    AVERAGE
};


#if defined LECROY_WR_S_MAIN_
#if defined LECROY_WR_CH3 && defined LECROY_WR_CH4
int trg_channels[ 7 ] = { LECROY_WR_CH1,
                          LECROY_WR_CH2,
                          LECROY_WR_CH3,
                          LECROY_WR_CH4,
                          LECROY_WR_LIN,
                          LECROY_WR_EXT,
                          LECROY_WR_EXT10
                        };
#else
int trg_channels[ 5 ] = { LECROY_WR_CH1,
                          LECROY_WR_CH2,
                          LECROY_WR_LIN,
                          LECROY_WR_EXT,
                          LECROY_WR_EXT10
                        };
#endif
#else
#if defined LECROY_WR_CH3 && defined LECROY_WR_CH4
extern int trg_channels[ 7 ];
#else
extern int trg_channels[ 5 ];
#endif
#endif

/* declaration of exported functions */

int lecroy_wr_s_init_hook(       void );
int lecroy_wr_s_test_hook(       void );
int lecroy_wr_s_exp_hook(        void );
int lecroy_wr_s_end_of_exp_hook( void );
void lecroy_wr_s_exit_hook(      void );


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

bool lecroy_wr_init( void );

double lecroy_wr_get_timebase( void );

bool lecroy_wr_set_timebase( double /* timebase */ );

bool lecroy_wr_get_interleaved( void );

bool lecroy_wr_set_interleaved( bool /* state */ );

long lecroy_wr_get_memory_size( void );

bool lecroy_wr_set_memory_size( long /* mem_size */ );

double lecroy_wr_get_sens( int /* channel */ );

bool lecroy_wr_set_sens( int    /* channel */,
                         double /* sens    */  );

double lecroy_wr_get_offset( int /* channel */ );

bool lecroy_wr_set_offset( int    /* channel */,
                           double /* offset  */  );

int lecroy_wr_get_coupling( int /* channel */ );

bool lecroy_wr_set_coupling( int /* channel */,
                             int /* type    */  );

int lecroy_wr_get_bandwidth_limiter( int /* channel */ );

bool lecroy_wr_set_bandwidth_limiter( int /* channel */,
                                      int /* bwl     */  );

int lecroy_wr_get_trigger_source( void );

bool lecroy_wr_set_trigger_source( int /* channel */ );

double lecroy_wr_get_trigger_level( int /* channel */ );

bool lecroy_wr_set_trigger_level( int    /* channel */,
                                  double /* level   */  );

bool lecroy_wr_get_trigger_slope( int /* channel */ );

bool lecroy_wr_set_trigger_slope( int  /* channel */,
                                  bool /* slope   */  );

int lecroy_wr_get_trigger_coupling( int /* channel */ );

int lecroy_wr_set_trigger_coupling( int /* channel */,
                                    int /* cpl     */  );

int lecroy_wr_get_trigger_mode( void );

int lecroy_wr_set_trigger_mode( int /* mode */ );

double lecroy_wr_get_trigger_delay( void );

bool lecroy_wr_set_trigger_delay( double /* delay */ );

bool lecroy_wr_is_displayed( int /* channel */ );

bool lecroy_wr_display( int /* channel */,
                        int /* on_off  */  );

long lecroy_wr_get_num_avg( int /* channel */ );

bool lecroy_wr_get_desc( int /* channel */ );

void lecroy_wr_set_up_averaging( long /* channel */,
                                 long /* source  */,
                                 long /* num_avg */,
                                 long /* rec_len */  );

void lecroy_wr_finished( void );

void lecroy_wr_start_acquisition( void );

void lecroy_wr_get_curve( int        /* ch     */,
                          Window_T * /* w      */,
                          double **  /* array  */,
                          long *     /* length */  );

double lecroy_wr_get_area( int        /* ch */,
                           Window_T * /* w  */  );

double lecroy_wr_get_amplitude( int        /* ch */,
                                Window_T * /* w  */  );

void lecroy_wr_copy_curve( long /* src  */,
                           long /* dest */  );

bool lecroy_wr_command( const char * /* cmd */ );

void lecroy_wr_soe_checks( void );

void lecroy_wr_exit_cleanup( void );

const char * lecroy_wr_ptime( double /* p_time */ );

double lecroy_wr_trigger_delay_check( void );

void lecroy_wr_delete_windows( LECROY_WR_T * /* s */ );

Window_T * lecroy_wr_get_window_by_number( long /* wid */ );

void lecroy_wr_all_windows_check( void );

void lecroy_wr_window_check( Window_T * /* w        */,
                             bool       /* show_num */  );

long lecroy_wr_curve_length( void );

double lecroy_wr_time_per_point( void );

long lecroy_wr_translate_channel( int  /* dir     */,
                                  long /* channel */,
                                  bool /* flag    */  );

void lecroy_wr_store_state( LECROY_WR_T * /* dest */,
                            LECROY_WR_T * /* src  */  );


void lecroy_wr_numpoints_prep( void );

void lecroy_wr_tbas_prep( void );

void lecroy_wr_hori_res_prep( void );

void lecroy_wr_clean_up( void );


#endif /* ! defined LECROY_WR_S_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
