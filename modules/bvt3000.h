/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#if ! defined BVT3000_HEADER
#define BVT3000_HEADER


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "bvt3000.conf"


#define STX   '\x02'
#define ETX   '\x03'
#define EOT   '\x04'
#define ENQ   '\x05'
#define ACK   '\x06'
#define NAK   '\x15'

#define SP1   0
#define SP2   1

#define AUTOMATIC_MODE  3
#define MANUAL_MODE     0

#define NORMAL_OPERATION    0
#define CONFIGURATION_MODE  2

#define TUNE_OFF            0
#define ADAPTIVE_TUNE       1
#define SELF_TUNE           2

#define HEATER_OK           0
#define HEATER_OVERHEATING  1

#define LN2_OK              0
#define LN2_NEEDS_REFILL    1
#define LN2_TANK_EMPTY      2


/* Defines for the BVT3000 interface status */

#define BVT3000_HEATER_ON                ( 1 <<  0 )
#define BVT3000_EVAPORATOR_CONNECTED     ( 1 <<  2 )
#define BVT3000_MISSING_GAS_FLOW         ( 1 <<  3 )
#define BVT3000_HEATER_OVERHEATING       ( 1 <<  4 )
#define BVT3000_EXCHANGER_CONNECTED      ( 1 <<  5 )
#define BVT3000_LN2_REFILL               ( 1 <<  6 )
#define BVT3000_LN2_EMPTY                ( 1 <<  7 )
#define BVT3000_LN2_HEATER_ON            ( 1 <<  8 )
#define BVT3000_BVBT3500_PRESENT         ( 1 << 10 )

/* Defines for bits in the Eurotherm 902S status word (SW) */

#define DATA_FORMAT_FLAG        0x0001   /* off for free, on for fixed format */
#define SENSOR_BREAK_FLAG       0x0002   /* set on sensor break */
#define KEYLOCK_FLAG            0x0004   /* set if keyboard is disabled */
#define ALARM2_STATE_FLAG       0x0100   /* set if ALARM2 is on */
#define ALARM1_STATE_FLAG       0x0400   /* set if ALARM1 is on */
#define ALARMS_STATE_FLAG       0x1000   /* ALARM1 or ALARM2 is on */
#define ACTIVE_SETPOINT_FLAG    0x2000   /* off for SP1, set for SP2 */
#define REMOTE_ACTIVE_FLAG      0x4000   /* set when in remote state */
#define MANUAL_MODE_FLAG        0x8000   /* set when manual, off in auto mode */


/* Defines for bits in the Eurotherm 902S extension status word (XS) */

#define SELF_TUNE_FLAG          0x01     /* set if in self tune mode */
#define ADAPTIVE_TUNE_FLAG      0x02     /* set if adaptuve tune is on */
#define ENABLE_BROADCAST_FLAG   0x04     /* broadcast enabled when on */
#define PID_CONTROL_FLAG        0x10     /* ? */
#define ACTIVE_PID_FLAG         0x20     /* off: PID1 active, on: PID2 active */


#define MIN_SETPOINT    73.0   /* lowest allowed value for setpoint, 73 K */
#define MAX_SETPOINT  1273.0   /* highest allowed value for setpoint, 1273 K */

#define MAX_PROPORTIONAL_BAND   999.9    /* according to experiments */
#define MAX_INTEGRAL_TIME      9999.0    /* according to experiments */
#define MAX_DERIVATIVE_TIME     999.9    /* according to experiments */
#define MAX_AT_TRIGGER_LEVEL    231.7    /* according to experiments */


#define TEST_STATE                 MANUAL_MODE
#define TEST_TUNE_STATE            ADAPTIVE_TUNE
#define TEST_TEMPERATURE          123.4
#define TEST_SETPOINT             123.4
#define TEST_HEATER_POWER_LIMIT   100.0
#define TEST_HEATER_POWER          50.0
#define TEST_FLOW_RATE           1600.0
#define TEST_PROPORTIONAL_BAND     36.0
#define TEST_INTEGRAL_TIME         17.0
#define TEST_DERIVATIVE_TIME        2.8
#define TEST_CUTBACK_LOW           67.9
#define TEST_CUTBACK_HIGH          56.2
#define TEST_AT_TRIGGER_LEVEL       4.6
#define TEST_DISPLAY_MIN            73.0
#define TEST_DISPLAY_MAX          1273.0
#define TEST_LN2_HEATER_POWER       35.0


/* Maximum time to wait for a reply (in us) */

#define SERIAL_WAIT  125000
#define ACK_WAIT     300000


typedef struct {
    int              state;
    double           setpoint;
    double           min_setpoint;
    double           max_setpoint;
    bool             heater_state;
    double           heater_power_limit;
    double           heater_power;
    double           flow_rate;
    int              tune_state;
    double           cb[ 2 ];
    double           max_cb[ 2 ];
    double           at_trigger;
    double           max_at_trigger;
    double           display_max;
    double           display_min;
    double           ln2_heater_power;
    bool             ln2_heater_state;
    bool             has_evaporator;
    bool             evaporator_needed;
    int              sn;
	bool             is_open;
    struct termios * tio;
} BVT3000;


int bvt3000_init_hook(       void );
int bvt3000_exp_hook(        void );
int bvt3000_end_of_exp_hook( void );


Var_T * temp_contr_name(                  Var_T * v );
Var_T * temp_contr_command(               Var_T * v );
Var_T * temp_contr_temperature(           Var_T * v );
Var_T * temp_contr_setpoint(              Var_T * v );
Var_T * temp_contr_heater_state(          Var_T * v );
Var_T * temp_contr_heater_power(          Var_T * v );
Var_T * temp_contr_heater_power_limit(    Var_T * v );
Var_T * temp_contr_check_heater(          Var_T * v );
Var_T * temp_contr_gas_flow(              Var_T * v );
Var_T * temp_contr_state(                 Var_T * v );
Var_T * temp_contr_tune_state(            Var_T * v );
Var_T * temp_contr_proportional_band(     Var_T * v );
Var_T * temp_contr_integral_time(         Var_T * v );
Var_T * temp_contr_derivative_time(       Var_T * v );
Var_T * temp_contr_cutbacks(              Var_T * v );
Var_T * temp_contr_adaptive_tune_trigger( Var_T * v );
Var_T * temp_contr_lock_keyboard(         Var_T * v );
Var_T * temp_contr_ln2_heater_state(      Var_T * v );
Var_T * temp_contr_ln2_heater_power(      Var_T * v );
Var_T * temp_contr_check_ln2_heater(      Var_T * v );


void bvt3000_init( void );
void bvt3000_set_flow_rate( unsigned int fr );
unsigned int bvt3000_get_flow_rate( void );
void bvt3000_set_heater_state( bool state );
bool bvt3000_get_heater_state( void );
int bvt3000_check_heater( void );
unsigned char bvt300_get_port( int port );
unsigned int bvt3000_get_interface_status( void );
void bvt3000_set_ln2_heater_state( bool state );
bool bvt3000_get_ln2_heater_state( void );
void bvt3000_set_ln2_heater_power( double p );
double bvt3000_get_ln2_heater_power( void );
int bvt3000_check_ln2_heater( void );
char * bvt3000_query( const char * cmd );
void bvt3000_send_command( const char * cmd );
bool bvt3000_check_ack( void );
size_t bvt3000_add_bcc( unsigned char * data );
bool bvt3000_check_bcc( unsigned char * data,
								 unsigned char   bcc );
void bvt3000_comm_fail( void );

void eurotherm902s_init( void );
bool eurotherm902s_check_sensor_break( void );
void eurotherm902s_set_operation_mode( int op_mode );
int eurotherm902s_get_operation_mode( void );
void eurotherm902s_set_mode( int mode );
int eurotherm902s_get_mode( void );
double eurotherm902s_get_temperature( void );
void eurotherm902s_set_active_setpoint( int sp );
int eurotherm902s_get_active_setpoint( void );
void eurotherm902s_set_setpoint( int    sp,
                                 double temp );
double eurotherm902s_get_setpoint( int sp );
double eurotherm902s_get_working_setpoint( void );
void eurotherm902s_set_sw( unsigned int sw );
unsigned int eurotherm902s_get_sw( void );
void eurotherm902s_set_os( unsigned int os );
unsigned int eurotherm902s_get_os( void );
void eurotherm902s_set_xs( unsigned int xs );
unsigned int eurotherm902s_get_xs( void );
bool eurotherm902s_get_alarm_state( void );
bool eurotherm902s_get_self_tune_state( void );
void eurotherm902s_set_adaptive_tune_trigger( double tr );
double eurotherm902s_get_adaptive_tune_trigger( void );
void eurotherm902s_set_self_tune_state( bool on_off );
bool eurotherm902s_get_adaptive_tune_state( void );
void eurotherm902s_set_adaptive_tune_state( bool on_off );
double eurotherm902s_get_min_setpoint( int sp );
double eurotherm902s_get_max_setpoint( int sp );
void eurotherm902s_set_heater_power_limit( double power );
double eurotherm902s_get_heater_power_limit( void );
void eurotherm902s_set_heater_power( double power );
double eurotherm902s_get_heater_power( void );
void eurotherm902s_set_proportional_band( double pb );
double eurotherm902s_get_proportional_band( void );
void eurotherm902s_set_integral_time( double t );
double eurotherm902s_get_integral_time( void );
void eurotherm902s_set_derivative_time( double t );
double eurotherm902s_get_derivative_time( void );
void eurotherm902s_set_cutback_high( double cb );
double eurotherm902s_get_cutback_high( void );
void eurotherm902s_set_cutback_low( double cb );
double eurotherm902s_get_cutback_low( void );
double eurotherm902s_get_display_maximum( void );
double eurotherm902s_get_display_minimum( void );
void eurotherm902s_lock_keyboard( bool lock );


#endif /* ! BVT3000_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
