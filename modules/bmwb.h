/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#if ! defined BMWB_HEADER_
#define BMWB_HEADER_

/* #define BMWB_TEST */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "fsc2_config.h"
#include <medriver.h>
#include "bmwb_rsc.h"


/* Read minimum and maximum frequencies (in GHz) of X- and Q-band bridge */

#include "x_bmwb.conf"
#undef DEVICE_NAME
#undef DEVICE_TYPE
#include "q_bmwb.conf"
#undef DEVICE_NAME
#undef DEVICE_TYPE


#define BMWB_X_BAND_STATE_FILE    "x_bmwb.state"
#define BMWB_Q_BAND_STATE_FILE    "q_bmwb.state"

#define X_BAND            0
#define Q_BAND            1
#define TYPE_FAIL        -1

#define MODE_STANDBY      0
#define MODE_TUNE         1
#define MODE_OPERATE      2


/* Minimum and maximum attenuation and attenuation deemed to be safe
   for tune mode (when switching to tune mode attenuation automatically
   gets increased to the value if lower before) */

#define MIN_ATTENUATION              0
#define MAX_ATTENUATION             60
#define SAFE_TUNE_MODE_ATTENUATION  20


/* Minimum and maximum expected detector current signal for X- and Q-band
   bridge (measured values times 2/3, taking voltage devider into account) */

#define DC_SIGNAL_MIN_X_BAND      0.0
#define DC_SIGNAL_MAX_X_BAND      0.24
#define DC_SIGNAL_SLOPE_X_BAND    2.4665
#define DC_SIGNAL_OFFSET_X_BAND  -0.1372

#define DC_SIGNAL_MIN_Q_BAND     -0.08
#define DC_SIGNAL_MAX_Q_BAND      9.5
#define DC_SIGNAL_SLOPE_Q_BAND    1.7954
#define DC_SIGNAL_OFFSET_Q_BAND   0.0467


/* Minimum and maximum expected AFC signal for X- and Q-band bridge (measured
   values times 2/3, taking voltage devider into account) */

#define AFC_SIGNAL_MIN_X_BAND  -0.155
#define AFC_SIGNAL_MAX_X_BAND   0.155

#define AFC_SIGNAL_MIN_Q_BAND  -8.67
#define AFC_SIGNAL_MAX_Q_BAND   8.67


/* Minimum and maximum voltage to be expected for "uncalibrated" signal
   (X-band only) */

#define UNCALIBRATED_MIN -0.5
#define UNCALIBRATED_MAX  2.5


/* Minimum and maximum voltage to be expected for "unlocked" signal
   (X-band only) */

#define UNLOCKED_MIN  1.0
#define UNLOCKED_MAX  3.0


/* Minimum and maximum x- and y-voltages masured in tune mode */

#define X_BAND_MIN_TUNE_X_VOLTS   -1.0
#define X_BAND_MAX_TUNE_X_VOLTS    4.0

#define X_BAND_MIN_TUNE_Y_VOLTS   -1.0
#define X_BAND_MAX_TUNE_Y_VOLTS    4.0

#define Q_BAND_MIN_TUNE_X_VOLTS   -1.0
#define Q_BAND_MAX_TUNE_X_VOLTS    4.0

#define Q_BAND_MIN_TUNE_Y_VOLTS   -1.0
#define Q_BAND_MAX_TUNE_Y_VOLTS    4.0


/* Frequency (in Hz) of the sawtooth (x-) voltage in tune mode */

#define X_BAND_TUNE_FREQ         1600
#define Q_BAND_TUNE_FREQ         1600


/* Conversion factors for microwave frequency given as a value in the
   interval [0,1] to the voltage to be output at the Meilhaus card */

#define X_BAND_FREQ_FACTOR     8.67
#define Q_BAND_FREQ_FACTOR     9.87


/* Conversion factor for microwave signal phase given as a value in the
   interval [0,1] to the voltage to be output at the Meilhaus card */

#define X_BAND_PHASE_FACTOR   -8.67
#define Q_BAND_PHASE_FACTOR  -10.00


/* Conversion factor for microwave bias given as a value in the interval
   [0,1] to the voltage to be output at the Meilhaus card */

#define X_BAND_BIAS_FACTOR    -8.67
#define Q_BAND_BIAS_FACTOR   -10.00


/* Conversion factor between microwave frequency (in GHz) and resonance
   field (in G) */

#define FAC              356.97577


/* Constants for buttons beside sliders and counters */

#define COARSE_DECREASE   -2
#define FINE_DECREASE     -1
#define FINE_INCREASE      1
#define COARSE_INCREASE    2


#define IRIS_UP            1
#define IRIS_DOWN         -1
#define IRIS_STOP          0


#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif


typedef struct {
    uid_t             EUID;           /* user and group ID the program got */
    gid_t             EGID;           /* started with */
    pthread_t         a_thread;       /* threads for dealing with connections */
    int               a_is_active;
    pthread_t         c_thread;
    int               c_is_active;
    pthread_mutex_t   mutex;
	int               type;
	int               mode;
	double            freq;
	double            min_freq;
	double            max_freq;
	int               attenuation;
	double            signal_phase;
	double            bias;
	FD_bmwb_rsc     * rsc;
	char              error_msg[ 100 + ME_ERROR_MSG_MAX_COUNT ];
} BMWB;

extern BMWB bmwb;


/* Number associated with the different DIs and DOs */

#define DIO_A   0         /* used for AFC state and type of bridge detection */
#define DIO_B   1         /* mostly used for bridge mode */
#define DIO_C   2         /* used for microwave attenuation */
#define DIO_D   3         /* used for iris control */


/* Define subdevice IDs for analog outputs */

#define FREQUENCY_AO            5         /* used for microwave frequency */
#define BIAS_AO                 6         /* used for microwave bias */
#define SIGNAL_PHASE_AO         7         /* used for signal phase */


/* Define subdevice ID for analog input and the different channels */

#define AI                      4         /* subdevice ID for analog input */

#define DETECTOR_CURRENT_AI     0         /* detector current */
#define AFC_SIGNAL_AI           1         /* AFC signal */
#define UNLOCKED_SIGNAL_AI      2         /* unlocked signal */
#define UNCALIBRATED_SIGNAL_AI  3         /* uncalibrated signal */
#define TUNE_MODE_X_SIGNAL_AI   4         /* tune mode x signal */
#define TUNE_MODE_Y_SIGNAL_AI   6         /* tune mode y signal */
#define OVERHEAT_SIGNAL_AI      8         /* overheat signal */


#define AFC_STATE_BIT       0x01
#define BRIDGE_TYPE_BIT     0x02

#define MODE_BITS           0x0F
#define MODE_STANDBY_BITS   0x0B
#define MODE_TUNE_BITS      0x0D
#define MODE_OPERATE_BITS   0x01

#define IRIS_UP_BIT         0x01
#define IRIS_DOWN_BIT       0x02

#define X_BAND_ATT_BITS     0x40
#define Q_BAND_ATT_BITS     0xF0



void error_handling( void );
int set_mw_freq( double val );
int set_mw_attenuation( int val );
int set_signal_phase( double val );
int set_mw_bias( double val );
int set_iris( int state );
int set_mode( int mode );
void save_state( void );

int measure_dc_signal( double * /* val */ );
int measure_afc_signal( double * /* val */ );
int measure_tune_mode( double * /* data */,
                       size_t   /* size */ );
int measure_unlocked_signal( double * /* val */ );
int measure_uncalibrated_signal( double * /* val */ );
int measure_afc_state( int * /* state */ );

void graphics_init( void );
int update_display( XEvent * /* xev  */,
                    void   * /* data */ );
void display_mode_pic( int /* mode */ );
void lock_objects( int /* state */ );

int meilhaus_init( void );
int meilhaus_finish( void );
int meilhaus_ai_single( int      /* channel */,
                        double   /* min     */,
                        double   /* max     */,
                        double * /* val     */ );
int meilhaus_ai_get_curves( int      /* x_channel */,
                            double * /* x_data    */,
                            double   /* x_min     */,
                            double   /* x_max     */,
                            int      /* y_channel */,
                            double * /* y_data    */,
                            double   /* y_min     */,
                            double   /* y_max     */,
                            size_t   /* len       */,
                            double   /* freq      */ );
int meilhaus_ao( int    /* ao  */,
                 double /* val */ );
int meilhaus_dio_in( int             /* dio */,
                     unsigned char * /* val */ );
int meilhaus_dio_out_state( int             /* dio */,
                            unsigned char * /* val */ );
int meilhaus_dio_out( int           /* dio */,
                      unsigned char /* val */ );


int bmwb_open_sock( void );

void raise_permissions( void );
void lower_permissions( void );
FILE * bmwb_fopen( const char * /* path */,
				   const char * /* mode */ );
double d_max( double /* a */,
			  double /* b */ );
double d_min( double /* a */,
			  double /* b */ );
int irnd( double /* d */ );
char * get_string( const char * /* fmt */,
				   ... );
const char * slash( const char * /* path */ );


#endif /* ! defined BMWB_HEADER_*/

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
