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


#include "fsc2_module.h"
#include "er218.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ANGLE_TO_STEPS( x )    irnd( 8 * ( x ) )
#define STEPS_TO_ANGLE( x )    ( 0.125 * ( x ) ) 

#define USEC_PER_STEP  25000L   /* time (in us) for a step of the motor */

#define STROBE_WAIT     1000L   /* time to wait in strobe or latch operation */


#define INIT_CMD       0x19
#define INIT_DATA      0xFFF
#define DATA_CMD       0x1A
#define STORE_CMD      0x1B
#define STORE_DATA     0x007
#define UP_CMD         0x1D
#define DOWN_CMD       0x1C

#define SSTROBE        0x80
#define CMD_LATCH      0x10
#define DATA_LATCH     0x20

#define GO_UP    0
#define GO_DOWN  1

#define BACKSLASH_ANGLE 12.0


int  er218_init_hook(       void );
int  er218_test_hook(       void );
int  er218_exp_hook(        void );
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
static void er218_set_dio( int           dio,
                           unsigned char data );


struct ER218 {
	double angle;
	bool is_angle;
    unsigned char dio1_data;
    unsigned char dio2_data;
    char *dio_out_func[ 2 ];
    char *dio_mode_func[ 2];
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
    const char *dio_names[ ] = { DIO_MODULE_1, DIO_MODULE_2 };
    int dio_num[ 2 ];
    Var_T *func_ptr;
    Var_T *v;
    char *func;
    int acc;
    int i,
        j;


	er218.angle = er218_read_config( );

    er218.backslash_correction = UNSET;
    er218.last_dir = -1;

    for ( i = 0; i < 2; i++ )
    {
        if ( ( dio_num[ i ] = exists_device( dio_names[ i ] ) ) == 0 )
        {
            print( FATAL, "Can't find the module '%s' - must be listed "
                   "before this module.\n", dio_names[ i ] );
            THROW( EXCEPTION );
        }

        /* Get a lock on the DIO */

        func = get_string( "dio_reserve_dio#%d", dio_num[ i ] );

        if (    ! func_exists( func )
             || ( func_ptr = func_get( func, &acc ) ) == NULL )
        {
            T_free( func );
            print( FATAL, "Function for reserving the DIO is missing from "
                   "module '%s'.\n", dio_names[ i ] );
            THROW( EXCEPTION );
        }

        T_free( func );
        vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

        v = func_call( func_ptr );

        if ( v->val.lval != 1 )
        {
            print( FATAL, "Can't reserve the DIO using module '%s'.\n",
                   dio_names[ i ] );
            THROW( EXCEPTION );
        }

        vars_pop( v );

        /* Get the name for the function for setting a value at the DIO and
           check if it exists */

        er218.dio_out_func[ i ] = get_string( "dio_value#%d", dio_num[ i ] );

        if ( ! func_exists( er218.dio_out_func[ i ] ) )
        {
            er218.dio_out_func[ i ] = T_free( er218.dio_out_func[ i ] );

            if ( i == 1 )
            {
                er218.dio_out_func[ 0 ] = T_free( er218.dio_out_func[ 0 ] );
                er218.dio_mode_func[ 0 ] = T_free( er218.dio_mode_func[ 0 ] );
            }
            print( FATAL, "Function for setting a DIO value is missing from "
                   "module '%s'.\n", dio_names[ i ] ); 
            THROW( EXCEPTION );
        }

        /* Get the name for the function for setting the mode of the DIO and
           check if it exists */

        er218.dio_mode_func[ i ] = get_string( "dio_mode#%d", dio_num[ i ] );

        if ( ! func_exists( er218.dio_mode_func[ i ] ) )
        {
            for ( j = 0; j <= i; j++ )
            {
                er218.dio_out_func[ j ] = T_free( er218.dio_out_func[ j ] );
                er218.dio_mode_func[ j ] = T_free( er218.dio_mode_func[ j ] );
            }
            print( FATAL, "Function for setting the DIOs mode is missing from "
                   "module '%s'.\n", dio_names[ i ] ); 
            THROW( EXCEPTION );
        }
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
    int i;


    er218_saved = er218;

    er218.angle = er218_read_config( );

    /* Call function that brings the DIO into 8 bit mode */

    for ( i = 0; i < 2; i++ )
    {
        if ( ( func_ptr = func_get( er218.dio_mode_func[ i ], &acc ) ) == NULL )
        {
            print( FATAL, "Internal error detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
        }

        vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
        vars_push( INT_VAR, 8L );
        vars_pop( func_call( func_ptr ) );
    }

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
er218_exp_hook( void )
{
    Var_T *func_ptr;
    int acc;
    int i;


    er218 = er218_saved;

    er218.angle = er218_read_config( );
    er218.is_angle = SET;

    /* Bring the DIOs into 8 bit mode */

    for ( i = 0; i < 2; i++ )
    {
        if ( ( func_ptr = func_get( er218.dio_mode_func[ i ], &acc ) ) == NULL )
        {
            print( FATAL, "Internal error detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
        }

        vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
        vars_push( INT_VAR, 8L );
        vars_pop( func_call( func_ptr ) );
    }

    if ( er218.backslash_correction )
    {
        er218_go_cmd( GO_DOWN, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
        er218_go_cmd( GO_UP, ANGLE_TO_STEPS( BACKSLASH_ANGLE ) );
    }

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

void
er218_exit_hook( void )
{
    int i;


    for ( i = 0; i < 2; i++ )
    {
        if ( er218.dio_out_func[ i ] )
            er218.dio_out_func[ i ] = T_free( er218.dio_out_func[ i ] );

        if ( er218.dio_mode_func[ i ] )
            er218.dio_mode_func[ i ] = T_free( er218.dio_mode_func[ i ] );
    }
}


/*------------------------*
 * Return the device name
 *------------------------*/

Var_T *
goniometer_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------*
 * Switch backslash correction on or off
 *---------------------------------------*/

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


/*------------------------------------*
 * Go to a certain angle (in degrees)
 *------------------------------------*/

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

	er218.angle = STEPS_TO_ANGLE( new_angle );
	if ( FSC2_MODE == EXPERIMENT )
    	er218_write_config( er218.angle );
    return vars_push( FLOAT_VAR, er218.angle );
}


/*-------------------------*
 * Move by a certain angle 
 *-------------------------*/

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


/*------------------------------------------------------*
 * Send reset command, making the device go to an angle
 * it considers to be the zero position.
 *------------------------------------------------------*/

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


/*----------------------------------------------------------------------*
 * Make the current angle the angle the module considers the zero angle
 *----------------------------------------------------------------------*/

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


/*--------------------------------------------------------------------*
 * Sends command to go to what the device considers the zero position
 *--------------------------------------------------------------------*/

static void
er218_init_cmd( void )
{
    er218.dio1_data = INIT_CMD;
    er218_set_dio( 0, er218.dio1_data );

    er218_toggle( CMD_LATCH );

    er218.dio1_data = INIT_DATA & 0xFF;
    er218_set_dio( 0, er218.dio1_data );

    er218.dio2_data = INIT_DATA >> 8;
    er218_set_dio( 1, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218_toggle( SSTROBE );

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
    /* Step 1: send the number of steps (value emust be sent inverted) */

    er218.dio1_data = DATA_CMD;
    er218_set_dio( 0, er218.dio1_data );

    er218_toggle( CMD_LATCH );

    er218.dio1_data = ~ data & 0xFF;
    er218_set_dio( 0, er218.dio1_data );

    er218.dio2_data = ~ data >> 8 & 0x0F;
    er218_set_dio( 1, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218_toggle( SSTROBE );

    /* Step 2: make the device accept the data */

    er218.dio1_data = STORE_CMD;
    er218_set_dio( 0, er218.dio1_data );

    er218_toggle( CMD_LATCH );

    er218.dio1_data = STORE_DATA & 0xFF;
    er218_set_dio( 0, er218.dio1_data );

    er218.dio2_data = STORE_DATA >> 8 & 0x0F;
    er218_set_dio( 1, er218.dio2_data );

    er218_toggle( DATA_LATCH );

    er218_toggle( SSTROBE );

    /* Step 3: tell the device to go up or down */

    er218.dio1_data = dir == GO_UP ? UP_CMD : DOWN_CMD;
    er218_set_dio( 0, er218.dio1_data );

    er218_toggle( CMD_LATCH );

    er218_toggle( SSTROBE );

    er218.last_dir = dir;

    er218_position_wait( data );
}


/*--------------------------------------------------*
 * Function toggles a strobe or latch line by first
 * setting it to high, then back to low.
 *--------------------------------------------------*/

static void
er218_toggle( unsigned char bit )
{
    er218_set_dio( 1, er218.dio2_data | bit );
    er218_strobe_wait( STROBE_WAIT );
    er218_set_dio( 1, er218.dio2_data );
}


/*----------------------------------------------------------------*
 * Function to wait for the time a strobe or latch line has to be
 * kept in the same state - expects the time in milli-seconds
 *----------------------------------------------------------------*/

static void
er218_strobe_wait( unsigned int duration )
{
    if ( FSC2_MODE == EXPERIMENT )
        fsc2_usleep( duration, UNSET );
}


/*-------------------------------------------------------*
 * Function to wait until the goniometer has reached its
 * requested positions - expects the number of steps of
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
er218_set_dio( int           dio,
               unsigned char data )
{
    Var_T *func_ptr;
    int acc;


    if ( ( func_ptr = func_get( er218.dio_out_func[ dio ], &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
    vars_push( INT_VAR, 0L );                  /* Push channel number */
    vars_push( INT_VAR, ( long ) data );       /* Push dat to be output */
    vars_pop( func_call( func_ptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
