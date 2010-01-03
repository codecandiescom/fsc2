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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "lakeshore330.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define SAMPLE_CHANNEL_A       0
#define SAMPLE_CHANNEL_B       1
#define DEFAULT_SAMPLE_CHANNEL SAMPLE_CHANNEL_B

#define LOCK_STATE_LOCAL       0
#define LOCK_STATE_REMOTE      1
#define LOCK_STATE_REMOTE_LLO  2

int lakeshore330_init_hook(       void );
int lakeshore330_exp_hook(        void );
int lakeshore330_end_of_exp_hook( void );


Var_T * temp_contr_name(           Var_T *v );
Var_T * temp_contr_temperature(    Var_T *v );
Var_T * temp_contr_sample_channel( Var_T *v );
Var_T * temp_contr_lock_keyboard(  Var_T *v );
Var_T * temp_contr_command(        Var_T *v );


static bool lakeshore330_init( const char * name );
static double lakeshore330_sens_data( long channel );
static long lakeshore330_set_sample_channel( long channel );
static long lakeshore330_get_sample_channel( void );
static void lakeshore330_lock( int state );
static bool lakeshore330_command( const char * cmd );
static bool lakeshore330_talk( const char * cmd,
                               char *       reply,
                               long *       length );

static void lakeshore330_gpib_failure( void );


static struct {
    int device;
    int lock_state;
    int channel;
} lakeshore330;



/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore330_init_hook( void )
{
    Need_GPIB = SET;

    lakeshore330.device     = -1;
    lakeshore330.lock_state = LOCK_STATE_REMOTE_LLO;
    lakeshore330.channel    = DEFAULT_SAMPLE_CHANNEL;

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore330_exp_hook( void )
{
    if ( ! lakeshore330_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed.\n" );
        THROW( EXCEPTION );
    }
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore330_end_of_exp_hook( void )
{
    lakeshore330_lock( 0 );
    return 1;
}


/**************************************/
/*                                    */
/*          EDL functions             */
/*                                    */
/**************************************/

/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------*
 * Returns temperature reading from controller
 *---------------------------------------------*/

Var_T *
temp_contr_temperature( Var_T * v  UNUSED_ARG )
{
    long channel;


    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, 123.45 );

    if ( v == NULL )
        return vars_push( FLOAT_VAR,
                          lakeshore330_sens_data( lakeshore330.channel ) );

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        channel = get_long( v, "channel number" ) - 1;

        if ( channel != SAMPLE_CHANNEL_A && channel != SAMPLE_CHANNEL_B )
        {
            print( FATAL, "Invalid sample channel number (%ld).\n", channel );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if (    ( *v->val.sptr != 'A' && *v->val.sptr != 'B' )
             || strlen( v->val.sptr ) != 1 )
        {
            print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
            THROW( EXCEPTION );
        }

        channel = ( long ) ( *v->val.sptr - 'A' );
    }

    return vars_push( FLOAT_VAR, lakeshore330_sens_data( channel ) );
}


/*-----------------------------------------------------------------*
 * Sets or returns sample channel (function returns either 1 or 2
 * for channel A or B and accepts 1 or 2 or the strings "A" or "B"
 * as input arguments).
 *-----------------------------------------------------------------*/

Var_T *
temp_contr_sample_channel( Var_T * v )
{
    long channel;


    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) lakeshore330.channel );

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        channel = get_long( v, "channel number" ) - 1;

        if ( channel != SAMPLE_CHANNEL_A && channel != SAMPLE_CHANNEL_B )
        {
            print( FATAL, "Invalid sample channel number (%ld).\n", channel );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if (    ( *v->val.sptr != 'A' && *v->val.sptr != 'B' )
             || strlen( v->val.sptr ) != 1 )
        {
            print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
            THROW( EXCEPTION );
        }
        channel = ( long ) ( *v->val.sptr - 'A' );
    }

    if ( FSC2_MODE == TEST )
    {
        lakeshore330.channel = channel;
        return vars_push( INT_VAR, channel + 1 );
    }

    return vars_push( INT_VAR,
                     ( long ) lakeshore330_set_sample_channel( channel ) + 1 );
}


/*---------------------------------------------------------*
 * If called with a non-zero argument the keyboard becomes
 * unlocked during an experiment.
 *---------------------------------------------------------*/

Var_T *
temp_contr_lock_keyboard( Var_T * v )
{
    int lock;


    if ( v == NULL )
        lock = LOCK_STATE_REMOTE_LLO;
    else
    {
        lock = get_boolean( v ) ? LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
        too_many_arguments( v );

        if ( FSC2_MODE == EXPERIMENT )
            lakeshore330_lock( lock - 1 );
    }

    return vars_push( INT_VAR, lock == LOCK_STATE_REMOTE_LLO ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            lakeshore330_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW( );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static bool
lakeshore330_init( const char * name )
{
    char buf[ 20 ];
    long len = sizeof buf;


    /* Initialize GPIB communication with the temperature controller */

    if ( gpib_init_device( name, &lakeshore330.device ) == FAILURE )
        return FAIL;

    /* Set end of EOS character to '\n' */

    if (    gpib_write( lakeshore330.device, "TERM 2\r\n", 8 ) == FAILURE
         || gpib_write( lakeshore330.device, "END 0\n", 6 ) == FAILURE )
        return FAIL;

    if (    gpib_write( lakeshore330.device, "*STB?\n", 6 ) == FAILURE
         || gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
        return FAIL;

    /* Set mode to Kelvin */

    if ( gpib_write( lakeshore330.device, "SUNI K\n", 7 ) == FAILURE )
        return FAIL;

    /* Switch device to remote state with local lockout */

    if ( gpib_write( lakeshore330.device, "MODE 2\n", 7 ) == FAILURE )
        return FAIL;

    /* Get the currently used sensor */

    lakeshore330_get_sample_channel( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore330_sens_data( long channel )
{
    char buf[ 50 ];
    long len = sizeof buf;
    double temp;


    if ( channel != lakeshore330.channel )
        lakeshore330_set_sample_channel( channel );

    lakeshore330_talk( "SDAT?\n", buf, &len );

    if ( *buf != '-' && *buf != '+' && ! isdigit( ( unsigned char ) *buf ) )
    {
        print( FATAL, "Error reading temperature.\n" );
        THROW( EXCEPTION );
    }

    sscanf( buf, "%lf", &temp );
    return temp;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static long
lakeshore330_set_sample_channel( long channel )
{
    char buf[ 20 ];


    fsc2_assert( channel == SAMPLE_CHANNEL_A || channel == SAMPLE_CHANNEL_B );

    sprintf( buf, "SCHN %c\n", ( char ) ( channel + 'A' ) );
    lakeshore330_command( buf );
    fsc2_usleep( 500000, UNSET );
    return lakeshore330.channel = channel;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static long
lakeshore330_get_sample_channel( void )
{
    char buf[ 20 ];
    long len = sizeof buf;


    lakeshore330_talk( "SCHN?\n", buf, &len );
    if ( *buf != 'A' && *buf != 'B' )
        lakeshore330_gpib_failure( );

    return lakeshore330.channel = *buf - 'A';
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore330_lock( int state )
{
    char cmd[ 20 ];


    fsc2_assert( state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO );

    sprintf( cmd, "MODE %d\n", state );
    lakeshore330_command( cmd );
    lakeshore330.lock_state = state;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore330_command( const char * cmd )
{
    if ( gpib_write( lakeshore330.device, cmd, strlen( cmd ) ) == FAILURE )
        lakeshore330_gpib_failure( );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore330_talk( const char * cmd,
                   char *       reply,
                   long *       length )
{
    if (    gpib_write( lakeshore330.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( lakeshore330.device, reply, length ) == FAILURE )
        lakeshore330_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore330_gpib_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
