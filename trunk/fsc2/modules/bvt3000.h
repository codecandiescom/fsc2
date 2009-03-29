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

#define SP1   0
#define SP2   1

#define AUTOMATIC_MODE  0
#define MANUAL_MODE     1


#define MIN_SETPOINT    73.0   /* lowest allowed value for setpoint, 73 K */
#define MAX_SETPOINT  1273.0   /* highest allowed value for setpoint, 1273 K */


#define TEST_TEMPERATURE          123.4
#define TEST_SETPOINT             123.4
#define TEST_HEATER_POWER_LIMIT   100.0
#define TEST_HEATER_POWER          50.0
#define TEST_FLOW_RATE           1600.0

/* Maximum time to wait for a reply (in us) */

#define SERIAL_WAIT  125000
#define ACK_WAIT     300000


typedef struct {
	bool             is_open;
    double           setpoint;
    double           min_setpoint;
    double           max_setpoint;
    double           heater_power_limit;
    double           heater_power;
    double           flow_rate;
    struct termios * tio;
} BVT3000;


int bvt3000_init_hook(       void );
int bvt3000_exp_hook(        void );
int bvt3000_end_of_exp_hook( void );

Var_T * temp_contr_name(               Var_T * v );
Var_T * temp_contr_command(            Var_T * v );
Var_T * temp_contr_temperature(        Var_T * v );
Var_T * temp_contr_setpoint(           Var_T * v );
Var_T * temp_contr_heater_power(       Var_T * v );
Var_T * temp_contr_heater_power_limit( Var_T * v );
Var_T * temp_contr_flow_rate(          Var_T * v );
Var_T * temp_contr_lock_keyboard(      Var_T * v );

void bvt3000_init( void );
void bvt3000_set_flow_rate( unsigned int fr );
unsigned int bvt3000_get_flow_rate( void );
void bvt3000_set_heater_state( bool state );
bool bvt3000_get_heater_state( void );
unsigned char bvt300_get_port( int port );
unsigned int bvt3000_get_interface_status( void );
char * bvt3000_query( const char * cmd );
void bvt3000_send_command( const char * cmd );
bool bvt3000_check_ack( void );
size_t bvt3000_add_bcc( unsigned char * data );
bool bvt3000_check_bcc( unsigned char * data,
								 unsigned char   bcc );
void bvt3000_comm_fail( void );

bool eurotherm902s_check_sensor_break( void );
double eurotherm902s_get_temperature( void );
void eurotherm902s_set_mode( int mode );
int eurotherm902s_get_mode( void );
void eurotherm902s_set_active_setpoint( int sp );
int eurotherm902s_get_active_setpoint( void );
void eurotherm902s_set_setpoint( double temp );
double eurotherm902s_get_setpoint( void );
double eurotherm902s_get_working_setpoint( void );
void eurotherm902s_set_sw( unsigned int sw );
unsigned int eurotherm902s_get_sw( void );
void eurotherm902s_set_os( unsigned int os );
unsigned int eurotherm902s_get_os( void );
void eurotherm902s_set_xs( unsigned int xs );
unsigned int eurotherm902s_get_xs( void );
double eurotherm902s_get_min_setpoint( void );
double eurotherm902s_get_max_setpoint( void );
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
void eurotherm902s_lock_keyboard( bool lock );


#endif /* ! BVT3000_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
