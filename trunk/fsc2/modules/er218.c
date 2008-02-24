/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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
#include "er218.conf"


#define ANGLE_TO_STEPS( x )    irnd( 8 * ( x ) )
#define STEPS_TO_ANGLE( x )    ( 0.125 * ( x ) ) 

#define USEC_PER_STEP  25000L    /* time (in us) for a step of the motor */

#define STROBE_WAIT    1000L     /* time to wait in strobe or latch operation */


#define INIT_CMD       0x19
#define INIT_DATA      0xFFF
#define DATA_CMD       0x1A
#define STORE_CMD      0x1D
#define STORE_DATA     0x007
#define UP_CMD         0x1D
#define DOWN_CMD       0x1C

#define SSTROBE        0x80;
#define ADDRESS_LATCH  0x10
#define DATA_LATCH     0x20

#define GO_UP    0
#define GO_DOWN  1

#define BACKSLASH_ANGLE 12.0


int  er218_init_hook(       void );
int  er218_test_hook(       void );
int  er218_exp_hook(        void );
int  er218_end_of_exp_hook( void );
void er218_exit_hook(       void );


Var_T * goniometer_name(                 Var_T * v );
Var_T * goniometer_backslash_correction( Var_T * v );
Var_T * goniometer_angle(                Var_T * v );
Var_T * goniometer_increment_angle(      Var_T * v );
Var_T * goniometer_reset(                Var_T * v );
Var_T * goniometer_set_zero_angle(       Var_T * v );


static double er218_read_config( void );
static void er218_write_config( double angle );
static void er218_init_cmd( void );
static void er218_go_cmd( int          dir,
                          unsigned int data );
static void er218_toggle( unsigned char bit );
static void er218_strobe_wait( unsigned int duration );
static void er218_position_wait( unsigned int steps );
static void er218_set_dio( int           ch,
                           unsigned char data );


struct ER218 {
	double angle;
	bool is_angle;
    unsigned char dio1_data;
    unsigned char dio2_data;
    int dio1_channel;
    int dio2_channel;
    char *dio_out_func;
    char *dio_mode_func;
    bool backslash_correction;
    int last_dir;
};


static struct ER218 er218,
                    er218_saved;


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
er218_init_hook( void )
{
    int dev_num;
    Var_T *func_ptr;
    Var_T *v;
    char *func;
    int acc;


	er218.angle = er218_read_config( );

    er218.dio1_channel = CHANNEL_CH3;
    er218.dio2_channel = CHANNEL_CH4;

    er218.backslash_correction = UNSET;
    er218.last_dir = -1;

    if ( ( dev_num = exists_device( DIO_MODULE ) ) == 0 )
    {
        print( FATAL, "Can't find the module '%s' - it must be listed before "
               "this module.\n", DIO_MODULE );
        THROW( EXCEPTION );
    }

    /* Get a lock on the DIO used to control the goniometer */

    if ( dev_num == 1 )
        func = T_strdup( "dio_reserve_dio" );
    else
        func = get_string( "dio_reserve_dio#%d", dev_num );

    if (    ! func_exists( func )
         || ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        T_free( func );
        print( FATAL, "Function for reserving the DIO is missing from "
               "module '%s'.\n", DIO_MODULE );
        THROW( EXCEPTION );
    }

    T_free( func );
    vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

    v = func_call( func_ptr );

    if ( v->val.lval != 1 )
    {
        print( FATAL, "Can't reserve the DIO using module '%s'.\n",
               DIO_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( v );

    /* Get the name for the function for setting a value at the DIO and
       check if it exists */

    if ( dev_num == 1 )
        er218.dio_out_func = T_strdup( "dio_value" );
    else
        er218.dio_out_func = get_string( "dio_value#%d", dev_num );

    if ( ! func_exists( er218.dio_out_func ) )
    {
        er218.dio_out_func = CHAR_P T_free( er218.dio_out_func );
        print( FATAL, "Function for setting a DIO value is missing from module "
               "'%s'.\n", DIO_MODULE ); 
        THROW( EXCEPTION );
    }

    /* Get the name for the function for setting the mode of the DIO and
       check if it exists */

    if ( dev_num == 1 )
        er218.dio_mode_func = T_strdup( "dio_mode" );
    else
        er218.dio_mode_func = get_string( "dio_mode#%d", dev_num );

    if ( ! func_exists( er218.dio_mode_func ) )
    {
        er218.dio_mode_func = CHAR_P T_free( er218.dio_mode_func );
        print( FATAL, "Function for setting the DIOs mode is missing from "
               "module '%s'.\n", DIO_MODULE ); 
        THROW( EXCEPTION );
    }

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
er218_test_hook( void )
{
    Var_T *func_ptr;
    int acc;


    er218_saved = er218;

    er218.angle = er218_read_config( );

    /* Call function that brings the DIO into 8 bit mode */

    if ( ( func_ptr = func_get( er218.dio_mode_func, &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
    vars_push( INT_VAR, ( long ) 8 );
    vars_pop( func_call( func_ptr ) );

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
er218_exp_hook( void )
{
    Var_T *func_ptr;
    int acc;


    er218 = er218_saved;

    er218.angle = er218_read_config( );
    er218.is_angle = SET;

    /* Bring the DIO into 8 bit mode */

    if ( ( func_ptr = func_get( er218.dio_mode_func, &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
    vars_push( INT_VAR, ( long ) 8 );
    vars_pop( func_call( func_ptr ) );

    if ( er218.backslash_correction )
    {
        er218_go_cmd( GO_DOWN, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
        er218_go_cmd( GO_UP, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
    }

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
er218_end_of_exp_hook( void )
{
    if ( er218.is_angle )
        er218_write_config( er218.angle );

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

void
er218_exit_hook( void )
{
    if ( er218.dio_out_func )
        er218.dio_out_func = CHAR_P T_free( er218.dio_out_func );

    if ( er218.dio_mode_func )
        er218.dio_mode_func = CHAR_P T_free( er218.dio_mode_func );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_backslash_correction( Var_T * v )
{
    bool bc;

    if ( v != NULL )
    {
        bc = get_boolean( v );
        too_many_arguments( v );

        if (    bc != er218.backslash_correction
             && bc
             && ! er218.last_dir == GO_UP )
        {
            er218_go_cmd( GO_DOWN, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
            er218_go_cmd( GO_UP, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
        }

        er218.backslash_correction = bc;
    }

    return vars_push( INT_VAR, ( long ) er218.backslash_correction );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_angle( Var_T * v )
{
    double angle;
    int new_angle;
    int old_angle = ANGLE_TO_STEPS( er218.angle );
    int diff;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, er218.angle );

    angle = get_double( v, "goniometer angle" );

    too_many_arguments( v );

    while ( angle >= 360.0 )
        angle -= 360.0;
    while ( angle < 0.0 )
        angle += 360.0;

    new_angle = ANGLE_TO_STEPS( angle );

    if ( fabs( new_angle - 8 * angle ) > 0.05 )
        print( WARN, "Angle must be multiple of %.3f degree, adjusting to "
               "%.3f degrees.\n",
               STEPS_TO_ANGLE( 1 ), STEPS_TO_ANGLE( new_angle ) );

    diff = new_angle - old_angle;

    if ( diff == 0 )
        return vars_push( FLOAT_VAR, er218.angle );

    if ( ! er218.backslash_correction )
    {
        if ( diff < - ANGLE_TO_STEPS( 180.0 ) )
            diff += ANGLE_TO_STEPS( 360.0 );

        if ( diff > ANGLE_TO_STEPS( 180.0 ) )
            diff -= ANGLE_TO_STEPS( 360.0 );

        er218_go_cmd( diff > 0 ? GO_UP : GO_DOWN, abs( diff ) );
    }
    else
    {
        if ( diff < 0 )
            diff += ANGLE_TO_STEPS( 360.0 );

        if ( diff <= ANGLE_TO_STEPS( 180.0 + 2 * BACKSLASH_ANGLE ) )
            er218_go_cmd( GO_UP, diff );
        else
        {
            diff =   ANGLE_TO_STEPS( 360.0 ) - diff
                   + ANGLE_TO_STEPS( BACKSLASH_ANGLE );
            er218_go_cmd( GO_DOWN, diff );
            er218_go_cmd( GO_UP, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
        }
    }

    return vars_push( FLOAT_VAR, er218.angle = STEPS_TO_ANGLE( new_angle ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_increment_angle( Var_T * v )
{
    double delta;

    if ( v == NULL )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    delta = get_double( v, "increment angle" );

    if ( fabs( ANGLE_TO_STEPS( delta ) - 8 * delta ) > 0.05 )
        print( WARN, "Increment angle must be multiple of %.3f dregree, "
               "adjusting to %.3f degrees.\n",
               STEPS_TO_ANGLE( 1 ), STEPS_TO_ANGLE( ANGLE_TO_STEPS( delta ) ) );

    return goniometer_angle(
                     vars_push( FLOAT_VAR,
                                  er218.angle
                                + STEPS_TO_ANGLE( ANGLE_TO_STEPS( delta ) ) ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_reset( Var_T * v  UNUSED_ARG )
{
	er218_init_cmd( );
	er218.angle = 0.0;
	er218.is_angle = SET;

    er218_init_cmd( );

    if ( er218.backslash_correction )
    {
        er218_go_cmd( GO_DOWN, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
        er218_go_cmd( GO_UP, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
    }

    return vars_push( FLOAT_VAR, 0.0 );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
goniometer_set_zero_angle( Var_T * v  UNUSED_ARG )
{
	er218.angle = 0.0;
	er218.is_angle = SET;

    return vars_push( FLOAT_VAR, 0.0 );
}


/*----------------------------------------------------------*
 * Function reads in the last set angle from the state file
 *----------------------------------------------------------*/

static double
er218_read_config( void )
{
    char *fn;
    const char *dn;
    FILE *fp;
    int c;
	double angle;
	int num_angles = 0;
    bool in_comment = UNSET;
    bool found_end = UNSET;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), ER218_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open state file '%s'.\n", fn );
        T_free( fn );
        THROW( EXCEPTION );
    }

	do
    {
        c = fsc2_fgetc( fp );

        switch ( c )
        {
            case EOF :
                found_end = SET;
                break;

            case '#' :
                in_comment = SET;
                break;

            case '\n' :
                in_comment = UNSET;
                break;

            default :
                if ( in_comment || isspace( c ) )
                    break;
                fsc2_fseek( fp, -1, SEEK_CUR );

				if ( fsc2_fscanf( fp, "%lf", &angle ) != 1 )
				{
					print( FATAL, "Invalid state file '%s'.\n", fn );
                    T_free( fn );
                    fsc2_fclose( fp );
                    THROW( EXCEPTION );
                }

				num_angles++;
				break;
        }
    } while ( ! found_end );

    fsc2_fclose( fp );

	if ( num_angles != 1 )
	{
		print( FATAL, "Invalid state file '%s'.\n", fn );
		T_free( fn );
		THROW( EXCEPTION );
	}

	T_free( fn );

	return angle;
}


/*------------------------------------------------------------*
 * Function writes out the state file with the last set angle
 *------------------------------------------------------------*/

static void
er218_write_config( double angle )
{

    char *fn;
    const char *dn;
    FILE *fp;


    dn = fsc2_config_dir( );
    fn = get_string( "%s%s%s", dn, slash( dn ), ER218_STATE_FILE );

    if ( ( fp = fsc2_fopen( fn, "w" ) ) == NULL )
    {
        print( SEVERE, "Can't store state data in '%s', can't open file "
               "for writing.\n", fn );
		T_free( fn );
        THROW( EXCEPTION );
    }

	T_free( fn );

    fsc2_fprintf( fp, "# --- Do *not* edit this file, it gets created "
                  "automatically ---\n\n"
                  "# This is the last angle set at the Bruker ER218 GP1 "
				  "goniometer\n\n"
				  "%.3f\n", angle );

	fsc2_fclose( fp );
}


/*----------------------------------------------*
 * Sends the command to go to the zero position
 *----------------------------------------------*/

static void
er218_init_cmd( void )
{
    er218.dio1_data = INIT_CMD;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );
    er218_toggle( ADDRESS_LATCH );

    er218.dio1_data = INIT_DATA & 0xFF;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );

    er218.dio2_data = INIT_DATA >> 8;
    er218_set_dio( er218.dio2_channel, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218_position_wait( ANGLE_TO_STEPS( 360.0 ) );
}


/*----------------------------------------------------------*
 * Sends the command to go up or down by a certain angle
 * ('dir' is the direction to move to, 'data' is the number
 * of steps of the motor required to reach the new angle)
 *----------------------------------------------------------*/

static void
er218_go_cmd( int          dir,
              unsigned int data )
{
    er218.dio1_data = DATA_CMD;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );
    er218_toggle( ADDRESS_LATCH );

    er218.dio1_data = ~ data & 0xFF;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );

    er218.dio2_data = ( ~ data >> 8 ) & 0x0F;
    er218_set_dio( er218.dio2_channel, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218.dio1_data = DATA_CMD;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );
    er218_toggle( ADDRESS_LATCH );

    er218.dio1_data = STORE_DATA & 0xFF;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );

    er218.dio2_data = ( STORE_DATA >> 8 ) & 0x0F;
    er218_set_dio( er218.dio2_channel, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218.dio1_data = dir == GO_UP ? UP_CMD : DOWN_CMD;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );
    er218_toggle( ADDRESS_LATCH );

    er218.dio1_data = STORE_DATA & 0xFF;
    er218_set_dio( er218.dio1_channel, er218.dio1_data );

    er218_set_dio( er218.dio2_channel, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218.last_dir = dir;

    er218_position_wait( data );
}


/*--------------------------------------------------*
 * Function toggles a strobe or latch line by first
 * setting it to high, then to low.
 *--------------------------------------------------*/

static void
er218_toggle( unsigned char bit )
{
    er218_set_dio( er218.dio2_channel, er218.dio2_data | bit );
    er218_strobe_wait( STROBE_WAIT );
    er218_set_dio( er218.dio2_channel, er218.dio2_data );
    er218_strobe_wait( STROBE_WAIT );
}


/*------------------------------------------------------*
 * Function to wait for the time a strobe or latch line
 * has to be kept in the same state - expects the time
 * in milli-seconds
 *------------------------------------------------------*/

static void
er218_strobe_wait( unsigned int duration )
{
    if ( FSC2_MODE == EXPERIMENT )
        fsc2_usleep( duration, UNSET );
}


/*-------------------------------------------------------*
 * Function to wait until the goniometer has reached its
 * requested positions - exects the number of steps of
 * the motor to be done
 *-------------------------------------------------------*/

static void
er218_position_wait( unsigned int steps )
{
    if ( FSC2_MODE == EXPERIMENT )
        fsc2_usleep( steps * USEC_PER_STEP, UNSET );
}


/*--------------------------------------*
 * Sends a data byte to one of the DIOs
 *--------------------------------------*/

static void
er218_set_dio( int           ch,
               unsigned char data )
{
    Var_T *func_ptr;
    int acc;


    if ( ( func_ptr = func_get( er218.dio_out_func, &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
    vars_push( INT_VAR, ( long ) ch );
    vars_push( INT_VAR, ( long ) data );
    vars_pop( func_call( func_ptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
