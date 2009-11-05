/*
  Copyright (C) 1999-2009 Anton Savitsky / Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#if ! defined AG54830B_HEADER
#define AG54830B_HEADER

#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "ag54830b.conf"


#define AG54830B_TEST_REC_LEN        1000   /* in points */
#define AG54830B_TEST_NUM_AVG        16
#define AG54830B_TEST_TIME_BASE      0.1    /* in seconds*/
#define AG54830B_TEST_TIME_PER_POINT 1e-4   /* in seconds*/
#define AG54830B_TEST_SENSITIVITY    0.01   /* in Volts/div*/
#define AG54830B_TEST_CHANNEL_STATE  0      /* OFF*/
#define AG54830B_TEST_TRIG_POS       0.1    /* Pretrigger in Time Ranges*/

#define AG54830B_UNDEF -1
#define AG54830B_CH1    0          /* Normal channels     */
#define AG54830B_CH2    1
#define AG54830B_F1     2          /* Function channels */
#define AG54830B_F2     3
#define AG54830B_F3     4
#define AG54830B_F4     5
#define AG54830B_M1     6          /* Memory channels   */
#define AG54830B_M2     7
#define AG54830B_M3     8
#define AG54830B_M4     9
#define AG54830B_LIN    10         /* Line In (for triggger only) */
#define MAX_CHANNELS    11         /* number of channel names */

#define NUM_NORMAL_CHANNELS       ( AG54830B_CH2 + 1 )
#define NUM_MEAS_CHANNELS         ( AG54830B_F4  + 1 )
#define NUM_DISPLAYABLE_CHANNELS  ( AG54830B_M4  + 1 )


#define MIN_REC_LEN    16
#define MAX_REC_LEN    32768
#define MIN_NUM_AVG    1
#define MAX_NUM_AVG    4096
#define MIN_TIMEBASE   500.0e-12
#define MAX_TIMEBASE   20.0
#define MIN_SENS       5.0
#define MIN_SENS50     1.0
#define MAX_SENS       1.0e-3


/* Declaration of exported functions */

int ag54830b_init_hook(       void );
int ag54830b_exp_hook(        void );
int ag54830b_end_of_exp_hook( void );

Var_T * digitizer_name(              Var_T * v );
Var_T * digitizer_show_channel(      Var_T * v );
Var_T * digitizer_timebase(          Var_T * v );
Var_T * digitizer_time_per_point(    Var_T * v );
Var_T * digitizer_sensitivity(       Var_T * v );
Var_T * digitizer_num_averages(      Var_T * v );
Var_T * digitizer_record_length(     Var_T * v );
Var_T * digitizer_trigger_position(  Var_T * v );
Var_T * digitizer_start_acquisition( Var_T * v );
Var_T * digitizer_get_curve(         Var_T * v );
Var_T * digitizer_get_curve_fast(    Var_T * v );
Var_T * digitizer_run(               Var_T * v );
Var_T * digitizer_get_xorigin(       Var_T * v );
Var_T * digitizer_get_xincrement(    Var_T * v );
Var_T * digitizer_get_yorigin(       Var_T * v );
Var_T * digitizer_get_yincrement(    Var_T * v );
Var_T * digitizer_command(           Var_T * v );


/* Locally used functions */

bool   ag54830b_init( const char * name );
bool   ag54830b_command( const char * cmd );
bool   ag54830b_command_retry( const char * cmd );
bool   ag54830b_talk( const char * cmd,
					  char       * reply,
					  long       * length );
double ag54830b_get_timebase( void );
void   ag54830b_set_timebase( double timebase );
long   ag54830b_get_num_avg( void );
void   ag54830b_set_num_avg( long num_avg );
int    ag54830b_get_acq_mode( void );
void   ag54830b_set_record_length( long num_points );
long   ag54830b_get_record_length( void );
double ag54830b_get_sens( int channel );
void   ag54830b_set_sens( int    channel,
							 double sens );
long   ag54830b_translate_channel( int  dir,
								   long channel,
								   bool flag );
void   ag54830b_acq_completed( void );
void   ag54830b_get_curve( int       channel,
						   double ** data,
						   long    * length,
						   bool      get_scaling );
double ag54830b_get_xorigin( int channel );
double ag54830b_get_xincrement( int channel );
double ag54830b_get_yorigin( int channel );
double ag54830b_get_yincrement( int channel );
void   ag54830b_set_trigger_pos( double pos );
double ag54830b_get_trigger_pos( void );
bool   ag54830b_display_channel_state( int channel );


typedef struct {
	int    device;

	double timebase;
	bool   is_timebase;

	double time_per_point;
	bool   is_time_per_point;

	double sens[ NUM_NORMAL_CHANNELS ];
	double is_sens[ NUM_NORMAL_CHANNELS ];

	long   num_avg;
	bool   is_num_avg;

	long   rec_len;
	bool   is_rec_len;

	int    meas_source;          /* channel selected as measurements source */
	int    data_source;          /* channel selected as data source */

	double trig_pos;
	bool   is_trig_pos;

	bool   channel_is_on[ NUM_DISPLAYABLE_CHANNELS ];
	bool   channels_in_use[ NUM_DISPLAYABLE_CHANNELS ];

	bool   acquisition_is_running;
} AG54830B_T;

enum {
	GENERAL_TO_AG54830B,
	AG54830B_TO_GENERAL
};


#if defined AG53840B_MAIN

AG54830B_T ag54830b;

const char *AG54830B_Channel_Names[ MAX_CHANNELS  ] = {
											"CHAN1", "CHAN2", "FUNC1", "FUNC2",
								 			"FUNC3", "FUNC4", "WMEM1", "WMEM2",
								 			"WMEM3", "WMEM4", "LINE" };

#else

extern AG54830B_T ag54830b;

extern const char *AG54830B_Channel_Names[ MAX_CHANNELS  ];

#endif


#endif /* ! defined AG54830B_HEADER */


 /*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
