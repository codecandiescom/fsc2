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


#pragma once
#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "lecroy9400.conf"


#define MAX_CHANNELS       9
#define MAX_USED_CHANNELS  4

#define LECROY9400_UNDEF   -1

#define LECROY9400_CH1      0
#define LECROY9400_CH2      1
#define LECROY9400_MEM_C    2
#define LECROY9400_MEM_D    3
#define LECROY9400_FUNC_E   4
#define LECROY9400_FUNC_F   5
#define LECROY9400_LIN      6
#define LECROY9400_EXT      7
#define LECROY9400_EXT10    8

#define GENERAL_TO_LECROY9400 0
#define LECROY9400_TO_GENERAL 1

#define LECROY9400_MIN_TIMEBASE 5.0e-8
#define LECROY9400_MAX_TIMEBASE 0.2
#define LECROY9400_MIN_TB_INDEX 0
#define LECROY9400_MAX_TB_INDEX 20

#define LECROY9400_MAX_SENS     5.0e-3
#define LECROY9400_MIN_SENS_1M  12.5
#define LECROY9400_MIN_SENS_50  2.5

#define INVALID_COUPL     -1         /* Input coupling for data channel */
#define AC_1_MOHM          0
#define DC_1_MOHM          1
#define DC_50_OHM          2
#define GND                3

#define MAX_OFFSET         8.0       /* Offset for data channel */
#define MIN_OFFSET        -8.0

#define TRG_MODE_AUTO      0         /* Trigger modes */
#define TRG_MODE_NORMAL    1         /* (don't change the sequence!) */
#define TRG_MODE_SINGLE    2
#define TRG_MODE_SEQ       3

#define TRG_CPL_AC         0         /* Trigger coupling */
#define TRG_CPL_DC         1
#define TRG_CPL_LF_REJ     2
#define TRG_CPL_HF_REJ     3

#define TRG_SLOPE_POS      0
#define TRG_SLOPE_NEG      1
#define TRG_SLOPE_POS_NEG  2

#define UNDEFINED_REC_LEN  1

#define MAX_DESC_LEN     160         /* amount of memory that needs to be
                                        allocated for a curve descriptor */


/* Here values are defined that get returned by the driver in the test run
   when the digitizer can't be accessed - these values must really be
   reasonable ! */

#define LECROY9400_TEST_TB_ENTRY       4        /* i.e. 1 us */
#define LECROY9400_TEST_COUPLING       AC_1_MOHM
#define LECROY9400_TEST_SENSITIVITY    0.01
#define LECROY9400_TEST_BWL            0
#define LECROY9400_TEST_NUM_AVG        10
#define LECROY9400_TEST_TRIG_DELAY     0.0
#define LECROY9400_TEST_TRIG_CHANNEL   LECROY9400_CH1
#define LECROY9400_TEST_TRIG_LEVEL     0.0
#define LECROY9400_TEST_TRIG_MODE      TRG_MODE_NORMAL
#define LECROY9400_TEST_TRIG_COUPLING  TRG_CPL_AC
#define LECROY9400_TEST_TRIG_SLOPE     TRG_SLOPE_POS


/* Structure for description of a 'window' on the digitizer, made up from the
   area between the pair of cursors */

typedef struct Window Window_T;
typedef struct LECROY9400 LECROY9400_T;


struct Window {
    long num;                   /* number of window                          */
    double start;               /* start of window (in time units)           */
    double width;               /* width of window (in time units)           */
    long start_num;             /* first point of window                     */
    long end_num;               /* last point of window                      */
    bool is_start;              /* flag, set if start of window has been set */
    bool is_width;              /* flag, set if width of window has been set */
    long num_points;            /* number of data points between the cursors */
    Window_T *next;             /* pointer to next window structure          */
    Window_T *prev;             /* pointer to previous window structure      */
};


struct LECROY9400 {
    int device;

    bool is_reacting;
    int data_size;                  /* # of bytes transfered for data points */

    bool is_displayed[ MAX_CHANNELS ];

    int num_used_channels;

    double timebase;
    int tb_index;
    bool is_timebase;

    unsigned char wv_desc[ MAX_CHANNELS ][ MAX_DESC_LEN ];

    double sens[ MAX_CHANNELS ];
    double is_sens[ MAX_CHANNELS ];

    int coupl[ MAX_CHANNELS ];
    double is_coupl[ MAX_CHANNELS ];

    double offset[ MAX_CHANNELS ];
    double is_offset[ MAX_CHANNELS ];

    bool bandwidth_limiter;
    bool is_bandwidth_limiter;

    int trigger_channel;
    bool is_trigger_channel;

    int trigger_mode;
    bool is_trigger_mode;

    double trigger_level;
    bool is_trigger_level;

    int trigger_slope;
    bool is_trigger_slope;

    int trigger_coupling;
    bool is_trigger_coupling;

    double trigger_delay;
    bool is_trigger_delay;

    long source_ch[ MAX_CHANNELS ];

    long rec_len[ MAX_CHANNELS ];

    long num_avg[ MAX_CHANNELS ];
    bool is_num_avg[ MAX_CHANNELS ];

    bool is_reject[ MAX_CHANNELS ];

    Window_T *w;           /* start element of list of windows               */
    int num_windows;

    double cursor_pos;     /* current position of cursor 1                   */

    int meas_source;       /* currently selected measurements source channel */
    int data_source;       /* currently selected data source channel         */

    bool channels_in_use[ MAX_CHANNELS ];

    bool lock_state;       /* set if keyboard is locked */
};



extern LECROY9400_T lecroy9400;

extern const char *LECROY9400_Channel_Names[ 9 ];

extern double sset[ 10 ];
extern double tb[ 21 ];
extern double tpp[ 21 ];
extern double sr[ 21 ];
extern int ppd[ 21 ];
extern long na[ 16 ];
extern long cl[ 10 ];
extern long ml[ 21 ];

extern bool lecroy9400_IN_SETUP;


enum {
    SAMPLE,
    AVERAGE
};


/* declaration of exported functions */

int lecroy9400_init_hook(       void );
int lecroy9400_test_hook(       void );
int lecroy9400_exp_hook(        void );
int lecroy9400_end_of_exp_hook( void );
void lecroy9400_exit_hook(      void );


Var_T * digitizer_name(              Var_T * /* v */ );
Var_T * digitizer_define_window(     Var_T * /* v */ );
Var_T * digitizer_timebase(          Var_T * /* v */ );
Var_T * digitizer_time_per_point(    Var_T * /* v */ );
Var_T * digitizer_sensitivity(       Var_T * /* v */ );
Var_T * digitizer_coupling(          Var_T * /* v */ );
Var_T * digitizer_bandwidth_limiter( Var_T * /* v */ );
Var_T * digitizer_averaging(         Var_T * /* v */ );
Var_T * digitizer_num_averages(      Var_T * /* v */ );
Var_T * digitizer_record_length(     Var_T * /* v */ );
Var_T * digitizer_trigger_delay(     Var_T * /* v */ );
Var_T * digitizer_trigger_mode(      Var_T * /* v */ );
Var_T * digitizer_trigger_coupling(  Var_T * /* v */ );
Var_T * digitizer_meas_channel_ok(   Var_T * /* v */ );
Var_T * digitizer_trigger_channel(   Var_T * /* v */ );
Var_T * digitizer_trigger_level(     Var_T * /* v */ );
Var_T * digitizer_trigger_slope(     Var_T * /* v */ );
Var_T * digitizer_start_acquisition( Var_T * /* v */ );
Var_T * digitizer_get_curve(         Var_T * /* v */ );
Var_T * digitizer_get_curve_fast(    Var_T * /* v */ );
Var_T * digitizer_command(           Var_T * /* v */ );


/* declaration of internally used functions */

int lecroy9400_get_tb_index( double /* timebase */ );

const char * lecroy9400_ptime( double /* p_time */ );

void lecroy9400_delete_windows( LECROY9400_T * /* s */ );

double lecroy9400_trigger_delay_check( void );

void lecroy9400_do_pre_exp_checks( void );

long lecroy9400_translate_channel( int  /* dir    */,
                                   long /* channel */,
                                   bool /* flag    */  );

void lecroy9400_store_state( LECROY9400_T * /* src  */,
                             LECROY9400_T * /* dest */  );

bool lecroy9400_init( const char * /* name */ );

double lecroy9400_get_timebase( void );

bool lecroy9400_set_timebase( double /* timebase */ );

int lecroy9400_get_trigger_source( void );

bool lecroy9400_set_trigger_source( int /* channel */ );

int lecroy9400_get_trigger_mode( void );

bool lecroy9400_set_trigger_mode( int /* mode */ );

double lecroy9400_get_trigger_level( void );

bool lecroy9400_set_trigger_level( double /* level */ );

int lecroy9400_get_trigger_slope( void );

bool lecroy9400_set_trigger_slope( int /* slope */ );

int lecroy9400_get_trigger_coupling( void );

bool lecroy9400_set_trigger_coupling( int /* cpl */ );

double lecroy9400_get_sens( int /* channel */ );

bool lecroy9400_set_sens( int    /* channel */,
                          double /* sens    */  );

double lecroy9400_get_offset( int /* channel */ );

bool lecroy9400_set_offset( int    /* channel */,
                            double /* offset */  );

int lecroy9400_get_coupling( int /* channel */ );

bool lecroy9400_set_coupling( int /* channel */,
                              int /* type    */  );

int lecroy9400_get_bandwidth_limiter( void );

void lecroy9400_set_bandwidth_limiter( bool /* state */ );

bool lecroy9400_is_displayed( int /* channel */ );

bool lecroy9400_display( int /* channel */,
                         int /* on_off  */  );

long lecroy9400_get_num_avg( int /* channel */ );

bool lecroy9400_get_desc( int  /* channel */,
                          bool /* do_stop */ );

double lecroy9400_get_trigger_delay( void );

void lecroy9400_set_trigger_delay( double /* delay */ );

void lecroy9400_set_up_averaging( long /* channel */,
                                  long /* source  */,
                                  long /* num_avg */,
                                  bool /* reject  */,
                                  long /* rec_len */  );

void lecroy9400_finished( void );

void lecroy9400_start_acquisition( void );

void lecroy9400_get_curve( int        /* ch         */,
                           Window_T * /* w          */,
                           double **  /* array      */,
                           long *     /* length     */,
                           bool       /* use_cursor */  );

bool lecroy9400_command( const char * /* cmd */ );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
