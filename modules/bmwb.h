/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#if ! defined BMWB_HEADER_
#define BMWB_HEADER_

//#define BMWB_TEST 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

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


#define DEV_ID   0


#define BMWB_X_BAND_STATE_FILE    "bmwb_x_band.state"
#define BMWB_Q_BAND_STATE_FILE    "bmwb_q_band.state"

#define X_BAND            0
#define Q_BAND            1
#define TYPE_FAIL        -1

#define MODE_STANDBY      0
#define MODE_TUNE         1
#define MODE_OPERATE      2

#define MIN_ATTENUATION   0
#define MAX_ATTENUATION   60


/* Minimum and maximum expected detector current signal for X- and Q-band
   bridge (measured values times 2/3, taking voltage devider into account) */

#define DC_SIGNAL_MIN_X_BAND    0.0
#define DC_SIGNAL_MAX_X_BAND    0.24

#define DC_SIGNAL_MIN_Q_BAND   -0.08
#define DC_SIGNAL_MAX_Q_BAND    9.5

/* Minimum and maximum expected AFC signal for X- and Q-band bridge (measured
   values times 2/3, taking voltage devider into account) */

#define AFC_SIGNAL_MIN_X_BAND  -0.155
#define AFC_SIGNAL_MAX_X_BAND   0.155

#define AFC_SIGNAL_MIN_Q_BAND  -8.67
#define AFC_SIGNAL_MAX_Q_BAND   8.67

/* Minimum and maximum voltage to be expected for "uncalibrated" signal
   (only for X-band) */

#define UNCALIBRATED_MIN -0.5
#define UNCALIBRATED_MAX  2.5

/* Minimum and maximum voltage to be expected for "unlocked" signal
   (only for X-band) */

#define UNLOCKED_MIN  1.0
#define UNLOCKED_MAX  3.0


#define FAC              356.97577


#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif


typedef struct {
    uid_t             EUID;           /* user and group ID the program got */
    gid_t             EGID;           /* started with */
    pthread_t         tid;            /* thread for dealing with connections */
    pthread_mutex_t   mutex;
    int               is_locked;
	int               type;
	int               mode;
	double            freq;
	double            min_freq;
	double            max_freq;
	int               attenuation;
	double            signal_phase;
	double            bias;
	double            lock_phase;
	FD_bmwb_rsc     * rsc;
	char              error_msg[ 100 + ME_ERROR_MSG_MAX_COUNT ];
} BMWB;

extern BMWB bmwb;


#define DIO_A   0         /* used for AFc state and type of bridge detection */
#define DIO_B   1         /* mostly used for mode */
#define DIO_C   2         /* used for microwave attenuation */
#define DIO_D   3         /* used for iris control */

#define AI      4

#define AO_0    5         /* used for microwave frequency */
#define AO_1    6         /* used for microwave bias */
#define AO_2    7         /* used for signal phase */
#define AO_3    8         /* used for lock phase */


#define AI_CH0  0         /* used for detector current */
#define AI_CH1  1         /* used for afc signal */
#define AI_CH2  2         /* used for unlocked signal */
#define AI_CH3  3         /* used for uncalibrated signal */
#define AI_CH4  4         /* used (together with AI_CH5) */
#define AI_CH5  5         /* for tune mode x-signal */
#define AI_CH6  6         /* used (together with AI_CH7) */
#define AI_CH7  7         /* for tune mode y-signal */
#define AI_CH8  8         /* used for overheat signal */


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
int set_lock_phase( double val );
int set_iris( int state );
int set_mode( int mode );
void save_state( void );

int measure_dc_signal( double * val );
int measure_afc_signal( double * val );
size_t measure_tune_mode( double ** data );
int measure_unlocked_signal( double * val );
int measure_uncalibrated_signal( double * val );
int measure_afc_state( int * state );

void graphics_init( void );
int update_display( XEvent * xev   UNUSED_ARG ,
                    void   * data  UNUSED_ARG );
void display_mode_pic( int mode );
void lock_objects( int state );

int meilhaus_init( void );
int meilhaus_finish( void );
int meilhaus_ai_single( int      channel,
                        double   min,
                        double   max,
                        double * val );
int meilhaus_ao( int    ao,
                 double val );
int meilhaus_dio_in( int             dio,
                     unsigned char * val );
int meilhaus_dio_out_state( int             dio,
                            unsigned char * val );
int meilhaus_dio_out( int           dio,
                      unsigned char val );


int bmwb_open_sock( void );

void raise_permissions( void );
void lower_permissions( void );
FILE * bmwb_fopen( const char *,
				   const char * );
double d_max( double  a,
			  double  b );
double d_min( double  a,
			  double  b );
int irnd( double d );
char * get_string( const char * fmt,
				   ... );
const char * slash( const char * path );


#endif /* ! defined BMWB_HEADER_*/

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
