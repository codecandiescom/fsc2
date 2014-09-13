/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if ! defined LECROY9410_HEADER
#define LECROY9410_HEADER

#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "lecroy9410.conf"


#define LECROY9410_IDENT_STRING     "LECROY,9410_,"

#define LECROY9410_INVALID_COUPL     -1   /* Input coupling for data channel */
#define LECROY9410_AC_1_MOHM          0   /* (don't change the sequence!) */
#define LECROY9410_DC_1_MOHM          1
#define LECROY9410_DC_50_OHM          2
#define LECROY9410_GND                3

#define LECROY9410_TRG_AC             0
#define LECROY9410_TRG_DC             1
#define LECROY9410_TRG_LF_REJ         2
#define LECROY9410_TRG_HF_REJ         3

#define LECROY9410_TRG_CPL_AC         0  /* Trigger couplings */
#define LECROY9410_TRG_CPL_DC         1  /* (don't change the sequence!) */
#define LECROY9410_TRG_CPL_LF_REJ     2
#define LECROY9410_TRG_CPL_HF_REJ     3

#define LECROY9410_TRG_MODE_AUTO      0  /* Trigger modes */
#define LECROY9410_TRG_MODE_NORMAL    1  /* (don't change the sequence!) */
#define LECROY9410_TRG_MODE_SINGLE    2
#define LECROY9410_TRG_MODE_SEQUENCE  3
#define LECROY9410_TRG_MODE_WRAP      4



/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define LECROY9410_TEST_TIMEBASE     5.0e-8      /* 50 ns per division*/
#define LECROY9410_TEST_TB_INDEX     5
#define LECROY9410_TEST_ILVD_MODE    UNSET       /* interleave mode (RIS/SS) */
#define LECROY9410_TEST_SENSITIVITY  2.5         /* 2.5 V per division */
#define LECROY9410_TEST_OFFSET       0.0
#define LECROY9410_TEST_NUM_AVG      10
#define LECROY9410_TEST_TRIG_DELAY   1.0e-8      /* 10 ns pretrigger */
#define LECROY9410_TEST_TRIG_CHANNEL 0           /* trigger on CH1 */
#define LECROY9410_TEST_TRIG_LEVEL   0.0
#define LECROY9410_TEST_TRIG_SLOPE   POSITIVE
#define LECROY9410_TEST_TRIG_COUP    LECROY9410_TRG_CPL_AC
#define LECROY9410_TEST_TRIG_MODE    LECROY9410_TRG_MODE_NORMAL
#define LECROY9410_TEST_REC_LEN      1000
#define LECROY9410_TEST_BWL          UNSET       /* bandwidth limiter */


#define LECROY9410_UNDEF   -1
#define LECROY9410_CH1      0
#define LECROY9410_CH2      1
#define LECROY9410_EXP_A    4
#define LECROY9410_EXP_B    5
#define LECROY9410_MEM_C    6
#define LECROY9410_MEM_D    7
#define LECROY9410_FUNC_E   8
#define LECROY9410_FUNC_F   9
#define LECROY9410_LIN     10
#define LECROY9410_EXT     11
#define LECROY9410_EXT10   12


/* Measurement channel with the highest number */

#define LECROY9410_CH_MAX   LECROY9410_CH2


#define GENERAL_TO_LECROY9410 0
#define LECROY9410_TO_GENERAL 1


/* Highest numbered channel (including function and memory channels) and
   maximum number of channels that can be displayed at once */

#define LECROY9410_MAX_CHANNELS       10
#define LECROY9410_MAX_USED_CHANNELS   4


/* Maximum and minimum time base setting [s/div] - please note that the
   time resolution depends in a somewhat complicated way on the time
   base and possibly the mode (RIS or SINGLE SHOT), see the HORI_RES
   structure below */

#define LECROY9410_MIN_TIMEBASE 1.0e-9
#define LECROY9410_MAX_TIMEBASE 5.0e3


/* Maximum and minimum sensitivity [V/div] for CH1 and CH2 */

#define LECROY9410_MAX_SENS     5.0e-3     /* 5 mV */
#define LECROY9410_MIN_SENS     2.5        /* 2.5 V */


/* The trigger delay can be set with a resolution of 1/50 of the timebase */

#define LECROY9410_TRIG_DELAY_RESOLUTION   0.02


/* Maximum number of averages */

#if defined WP01
#define LECROY9410_MAX_AVERAGES    1000000
#else
#define LECROY9410_MAX_AVERAGES    1000
#endif


/* Maximum factors or values for trigger levels */

#define LECROY9410_TRG_MAX_LEVEL_CH_FAC 5.0
#define LECROY9410_TRG_MAX_LEVEL_EXT    2.0
#define LECROY9410_TRG_MAX_LEVEL_EXT10  10.0

#define LECROY9410_UNDEF_REC_LEN  -1


/* Constants for the INR register */

#define LECROY9410_INR_FF_DONE        ( 1U << 11 )
#define LECROY9410_INR_FE_DONE        ( 1U << 10 )
#define LECROY9410_INR_MD_DONE        ( 1U <<  9 )
#define LECROY9410_INR_MC_DONE        ( 1U <<  8 )
#define LECROY9410_INR_SIG_DONE       ( 1U <<  0 )


/* Structure for description of a 'window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct Window Window_T;
typedef struct LECROY9410 LECROY9410_T;
typedef struct HORI_RES HORI_RES_T;



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


struct LECROY9410 {
    int device;

    unsigned int INR;

    bool is_displayed[ LECROY9410_MAX_CHANNELS ];

    int num_used_channels;

    double timebase;
    int tb_index;
    bool is_timebase;

    bool interleaved;
    bool is_interleaved;

    double sens[ LECROY9410_MAX_CHANNELS ];
    bool is_sens[ LECROY9410_MAX_CHANNELS ];

    double offset[ LECROY9410_MAX_CHANNELS ];
    bool is_offset[ LECROY9410_MAX_CHANNELS ];

    bool bandwidth_limiter;
    bool is_bandwidth_limiter;

    int coupling[ LECROY9410_MAX_CHANNELS ];
    bool is_coupling[ LECROY9410_MAX_CHANNELS ];

    int trigger_channel;
    bool is_trigger_channel;

    double trigger_level[ LECROY9410_MAX_CHANNELS ];
    bool is_trigger_level[ LECROY9410_MAX_CHANNELS ];

    int trigger_slope[ LECROY9410_MAX_CHANNELS ];
    bool is_trigger_slope[ LECROY9410_MAX_CHANNELS ];

    int trigger_coupling[ LECROY9410_MAX_CHANNELS ];
    bool is_trigger_coupling[ LECROY9410_MAX_CHANNELS ];

    double trigger_delay;
    bool is_trigger_delay;

    int trigger_mode;
    bool is_trigger_mode;

    bool is_avg_setup[ LECROY9410_MAX_CHANNELS ];
    long source_ch[ LECROY9410_MAX_CHANNELS ];
    long rec_len[ LECROY9410_MAX_CHANNELS ];
    long num_avg[ LECROY9410_MAX_CHANNELS ];

    Window_T *w;           /* start element of list of windows */
    int num_windows;

    bool channels_in_use[ LECROY9410_MAX_CHANNELS ];
};


struct HORI_RES {
    double tdiv;         /* time base setting */
    double ris_tp;       /* time resolution (RIS mode) */
    long ris_rl;         /* record length (RIS mode) */
    double ss_tp;        /* time resolution (SINGLE SHOT mode) */
    long ss_rl;          /* record length (SINGLE SHOT mode) */
};


#if defined LECROY9410_MAIN_


HORI_RES_T hres[ 36 ] = { { 2.0e-9, 2.5e-10,    80,   -1.0,    -1 },
                          { 5.0e-9, 2.5e-10,   200,   -1.0,    -1 },
                          { 1.0e-8, 2.5e-10,   400,   -1.0,    -1 },
                          { 2.0e-8, 2.5e-10,   800,   -1.0,    -1 },
                          { 5.0e-8, 2.5e-10,  2000, 1.0e-8,    50 },
                          { 1.0e-7, 2.5e-10,  4000, 1.0e-8,   100 },
                          { 2.0e-7, 2.5e-10,  8000, 1.0e-8,   200 },
                          { 5.0e-7, 5.0e-10, 10000, 1.0e-8,   500 },
                          { 1.0e-6, 1.0e-00, 10000, 1.0e-8,  1000 },
                          { 2.0e-6, 2.5e-09,  8000, 1.0e-8,  2000 },
                          { 5.0e-6, 5.0e-09, 10000, 1.0e-8,  5000 },
                          { 1.0e-5,    -1.0,    -1, 1.0e-8, 10000 },
                          { 2.0e-5,    -1.0,    -1, 2.5e-8,  8000 },
                          { 5.0e-5,    -1.0,    -1, 5.0e-8, 10000 },
                          { 1.0e-4,    -1.0,    -1, 1.0e-7, 10000 },
                          { 2.0e-4,    -1.0,    -1, 2.5e-7,  8000 },
                          { 5.0e-4,    -1.0,    -1, 5.0e-7, 10000 },
                          { 1.0e-3,    -1.0,    -1, 1.0e-6, 10000 },
                          { 2.0e-3,    -1.0,    -1, 2.5e-6,  8000 },
                          { 5.0e-3,    -1.0,    -1, 5.0e-6, 10000 },
                          { 1.0e-2,    -1.0,    -1, 1.0e-5, 10000 },
                          { 2.0e-2,    -1.0,    -1, 2.5e-5,  8000 },
                          { 5.0e-2,    -1.0,    -1, 5.0e-5, 10000 },
                          { 1.0e-1,    -1.0,    -1, 1.0e-4, 10000 },
                          { 2.0e-1,    -1.0,    -1, 2.5e-4,  8000 },
                          { 5.0e-1,    -1.0,    -1, 5.0e-4, 10000 },
                          {    1.0,    -1.0,    -1, 1.0e-3, 10000 },
                          {    2.0,    -1.0,    -1, 2.5e-3,  8000 },
                          {    5.0,    -1.0,    -1, 5.0e-3, 10000 },
                          {  1.0e1,    -1.0,    -1, 1.0e-2, 10000 },
                          {  2.0e1,    -1.0,    -1, 2.5e-2,  8000 },
                          {  5.0e1,    -1.0,    -1, 5.0e-2, 10000 },
                          {  1.0e2,    -1.0,    -1, 1.0e-1, 10000 },
                          {  2.0e2,    -1.0,    -1, 2.5e-1,  8000 },
                          {  5.0e2,    -1.0,    -1, 5.0e-1, 10000 },
                          {  1.0e3,    -1.0,    -1, 1.0e-0, 10000 } };

/* List of fixed sensivity settings where the range of the offset changes */

double fixed_sens[ 8 ] = { 5.0e-3, 1.0e-2, 2.0e-2, 5.0e-2,
                           0.1, 0.2, 0.5, 1.0 };

/* List of offset factors for the different sensitivity ranges */

int offset_factor[ 8 ] = { 48, 24, 12, 12, 12, 12, 12, 10 };

#else
extern HORI_RES_T hres[ 36 ];
extern double fixed_sens[ 8 ];
extern int offset_factor[ 8 ];
#endif

extern long rl[ 10 ];
extern LECROY9410_T lecroy9410;
extern const char *LECROY9410_Channel_Names[ 13 ];


enum {
    SAMPLE,
    AVERAGE
};


/* declaration of exported functions */

int lecroy9410_init_hook(       void );
int lecroy9410_test_hook(       void );
int lecroy9410_exp_hook(        void );
int lecroy9410_end_of_exp_hook( void );
void lecroy9410_exit_hook(      void );


Var_T * digitizer_name(               Var_T * /* v */ );
Var_T * digitizer_define_window(      Var_T * /* v */ );
Var_T * digitizer_change_window(      Var_T * /* v */ );
Var_T * digitizer_window_position(    Var_T * /* v */ );
Var_T * digitizer_window_width(       Var_T * /* v */ );
Var_T * digitizer_timebase(           Var_T * /* v */ );
Var_T * digitizer_interleave_mode(    Var_T * /* v */ );
Var_T * digitizer_time_per_point(     Var_T * /* v */ );
Var_T * digitizer_sensitivity(        Var_T * /* v */ );
Var_T * digitizer_coupling(           Var_T * /* v */ );
Var_T * digitizer_offset(             Var_T * /* v */ );
Var_T * digitizer_bandwidth_limiter(  Var_T * /* v */ );
Var_T * digitizer_trigger_channel(    Var_T * /* v */ );
Var_T * digitizer_trigger_level(      Var_T * /* v */ );
Var_T * digitizer_trigger_slope(      Var_T * /* v */ );
Var_T * digitizer_trigger_coupling(   Var_T * /* v */ );
Var_T * digitizer_trigger_mode(       Var_T * /* v */ );
Var_T * digitizer_trigger_delay(      Var_T * /* v */ );
Var_T * digitizer_averaging(          Var_T * /* v */ );
Var_T * digitizer_num_averages(       Var_T * /* v */ );
Var_T * digitizer_record_length(      Var_T * /* v */ );
Var_T * digitizer_trigger_position(   Var_T * /* v */ );
Var_T * digitizer_meas_channel_ok(    Var_T * /* v */ );
Var_T * digitizer_start_acquisition(  Var_T * /* v */ );
Var_T * digitizer_get_curve(          Var_T * /* v */ );
Var_T * digitizer_get_curve_fast(     Var_T * /* v */ );
Var_T * digitizer_get_area(           Var_T * /* v */ );
Var_T * digitizer_get_area_fast(      Var_T * /* v */ );
Var_T * digitizer_get_amplitude(      Var_T * /* v */ );
Var_T * digitizer_get_amplitude_fast( Var_T * /* v */ );
Var_T * digitizer_copy_curve(         Var_T * /* v */ );
Var_T * digitizer_command(            Var_T * /* v */ );


/* declaration of internally used functions */

bool lecroy9410_init( const char * /* name */ );

double lecroy9410_get_timebase( void );

void lecroy9410_set_timebase( double /* timebase */ );

bool lecroy9410_get_interleaved( void );

void lecroy9410_set_interleaved( bool /* state */ );

double lecroy9410_get_sens( int /* channel */ );

void lecroy9410_set_sens( int    /* channel */,
                          double /* sens    */  );

double lecroy9410_get_offset( int /* channel */ );

void lecroy9410_set_offset( int    /* channel */,
                            double /* offset  */  );

int lecroy9410_get_coupling( int /* channel */ );

void lecroy9410_set_coupling( int /* channel */,
                              int /* type    */  );

int lecroy9410_get_bandwidth_limiter( void );

void lecroy9410_set_bandwidth_limiter( bool /* state */ );

int lecroy9410_get_trigger_source( void );

void lecroy9410_set_trigger_source( int /* channel */ );

double lecroy9410_get_trigger_level( int /* channel */ );

void lecroy9410_set_trigger_level( int    /* channel */,
                                   double /* level   */  );

double lecroy9410_get_trigger_slope( int /* channel */ );

void lecroy9410_set_trigger_slope( int /* channel */,
                                   int /* slope   */  );

int lecroy9410_get_trigger_coupling( int /* channel */ );

void lecroy9410_set_trigger_coupling( int /* channel */,
                                      int /* cpl     */  );

int lecroy9410_get_trigger_mode( void );

void lecroy9410_set_trigger_mode( int /* mode */ );

double lecroy9410_get_trigger_delay( void );

void lecroy9410_set_trigger_delay( double /* delay */ );

long lecroy9410_get_probe_attenuation( int /* ch */ );

void lecroy9410_set_probe_attenuation( int /* ch   */,
                                       long /* att */  );

bool lecroy9410_is_displayed( int /* ch */ );

void lecroy9410_display( int /* ch     */,
                         int /* on_off */  );

void lecroy9410_set_up_averaging( long /* channel */,
                                  long /* source  */,
                                  long /* num_avg */,
                                  long /* rec_len */  );

void lecroy9410_finished( void );

void lecroy9410_start_acquisition( void );

void lecroy9410_get_curve( int        /* ch     */,
                           Window_T * /* w      */,
                           double **  /* array  */,
                           long *     /* length */  );

double lecroy9410_get_area( int        /* ch */,
                            Window_T * /* w */  );

double lecroy9410_get_amplitude( int        /* ch */,
                                 Window_T * /* w  */  );

void lecroy9410_copy_curve( long /* src  */,
                            long /* dest */  );

void lecroy9410_command( const char * /* cmd */ );

void lecroy9410_soe_checks( void );

void lecroy9410_exit_cleanup( void );

const char * lecroy9410_ptime( double /* p_time */ );

void lecroy9410_delete_windows( LECROY9410_T * /* s */ );

Window_T * lecroy9410_get_window_by_number( long /* wid */ );

double lecroy9410_trigger_delay_check( void );

void lecroy9410_all_windows_check( void );

void lecroy9410_window_check( Window_T * /* w        */,
                              bool       /* show_num */  );

long lecroy9410_find_length( void );

long lecroy9410_curve_length( void );

double lecroy9410_time_per_point( void );

long lecroy9410_translate_channel( int  /* dir     */,
                                   long /* channel */,
                                   bool /* flag    */  );

void lecroy9410_store_state( LECROY9410_T * /* dest */,
                             LECROY9410_T * /* src  */  );


#endif /* ! LECROY9410_HEADER */

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
