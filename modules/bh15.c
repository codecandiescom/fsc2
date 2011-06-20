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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "bh15.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define BH15_TEST_FIELD   0.0


int bh15_init_hook(       void );
int bh15_exp_hook(        void );
int bh15_end_of_exp_hook( void );

Var_T * gaussmeter_name(       Var_T * v );
Var_T * gaussmeter_field(      Var_T * v );
Var_T * find_field(            Var_T * v );
Var_T * gaussmeter_resolution( Var_T * v );
Var_T * gaussmeter_wait(       Var_T * v );
Var_T * gaussmeter_command(    Var_T * v );


static double bh15_get_field( void );

static bool bh15_command( const char * cmd );


static struct {
    int state;
    int device;
    const char *name;
    double field;
    double resolution;
} bh15;

bool is_gaussmeter = UNSET;         /* tested by magnet power supply driver */


enum {
    BH15_UNKNOWN = 0,
    BH15_FAR_OFF,
    BH15_STILL_OFF,
    BH15_LOCKED
};

#define BH15_MAX_TRIES   20


/*****************************************************/
/*                                                   */
/*                  hook functions                   */
/*                                                   */
/*****************************************************/


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
bh15_init_hook( void )
{
    Need_GPIB = SET;
    bh15.name = DEVICE_NAME;

    is_gaussmeter = SET;

    bh15.state = BH15_UNKNOWN;
    bh15.device = -1;

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
bh15_exp_hook( void )
{
    char buffer[ 20 ];
    long len;
    int tries = 0;


    fsc2_assert( bh15.device < 0 );

    if ( gpib_init_device( bh15.name, &bh15.device ) == FAILURE )
    {
        bh15.device = -1;
        print( FATAL, "Can't initialize Bruker BH15 field controller: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    /* Send a "Selected Device Clear" - I don't know yet if this is really
       needed */

    gpib_clear_device( bh15.device );

    /* Set Mode 5 */

    bh15_command( "MO 5\r" );

    /* Set it into run mode */

    bh15_command( "RU\r" );

    sleep( 5 );                /* unfortunately, it seems to need this... */

    do
    {
        stop_on_user_request( );

        fsc2_usleep( 100000, UNSET );

        bh15_command( "LE\r" );

        len = sizeof buffer;
        if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
        {
            print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
            THROW( EXCEPTION );
        }
    } while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );

    bh15.state = BH15_FAR_OFF;

    /* We are always going to achieve a resolution of 50 mG ... */

    bh15.resolution = 0.05;
    bh15_get_field( );

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
bh15_end_of_exp_hook( void )
{
    if ( bh15.device >= 0 )
        gpib_local( bh15.device );

    bh15.state = BH15_UNKNOWN;
    bh15.device = -1;

    return 1;
}


/***************************************************/
/*                                                 */
/*              exported functions                 */
/*                                                 */
/***************************************************/


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
gaussmeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
gaussmeter_field( Var_T * v  UNUSED_ARG )
{
    return vars_push( FLOAT_VAR, bh15_get_field( ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
find_field( Var_T * v  UNUSED_ARG )
{
    return vars_push( FLOAT_VAR, bh15_get_field( ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
gaussmeter_resolution( Var_T * v  UNUSED_ARG )
{
    return vars_push( FLOAT_VAR, bh15.resolution );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
gaussmeter_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            bh15_command( cmd );
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


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
gaussmeter_wait( Var_T * v  UNUSED_ARG )
{
    fsc2_usleep( 100000, UNSET );
    return vars_push( INT_VAR, 1L );
}


/******************************************************/
/*                                                    */
/*            internally used functions               */
/*                                                    */
/******************************************************/


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static double
bh15_get_field( void )
{
    char buffer[ 20 ];
    long len;
    int tries = 0;
    char *val;
    char *endptr;


    if ( FSC2_MODE == TEST )
        return BH15_TEST_FIELD;

    do
    {
        stop_on_user_request( );

        fsc2_usleep( 100000, UNSET );

        bh15_command( "LE\r" );

        len = sizeof buffer;
        if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
        {
            print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
            THROW( EXCEPTION );
        }
    } while ( ++tries < BH15_MAX_TRIES && strchr( buffer, '1' ) != NULL );


    tries = 0;
    bh15.state = BH15_FAR_OFF;

    do
    {
        stop_on_user_request( );

        fsc2_usleep( 100000, UNSET );

        bh15_command( "FV\r" );

        len = sizeof buffer;
        if ( gpib_read( bh15.device, buffer, &len ) == FAILURE )
        {
            print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
            THROW( EXCEPTION );
        }

        /* Try to find the qualifier */

        val = buffer;
        while ( *val && ! isdigit( ( unsigned char ) *val ) )
            val++;

        if ( *val == '\0' )    /* no qualifier found ? */
        {
            print( FATAL, "Invalid data returned by Bruker BH15 field "
                   "controller.\n" );
            THROW( EXCEPTION );
        }

        switch ( *val )
        {
            case '0' :                             /* correct within 50 mG */
                bh15.state = BH15_LOCKED;
                break;

            case '1' :                             /* correct within 1 G */
                if ( bh15.state == BH15_FAR_OFF )
                {
                    bh15.state = BH15_STILL_OFF;
                    tries = 0;
                }
                break;

            case '2' :                             /* error larger than 1 G */
                bh15.state = BH15_FAR_OFF;
                tries = 0;
                break;

            case '3' :                             /* BH15 not in RUN mode */
                print( FATAL, "Bruker BH15 field controller dropped out of "
                       "run mode.\n" );
                THROW( EXCEPTION );
                break;

            default :
                print( FATAL, "Invalid data returned by Bruker BH15 field "
                       "controller.\n" );
                THROW( EXCEPTION );
        }

    } while ( bh15.state != BH15_LOCKED && ++tries < BH15_MAX_TRIES );

    if ( bh15.state != BH15_LOCKED )
    {
        print( FATAL, "Bruker BH15 field controller can't find the field.\n" );
        THROW( EXCEPTION );
    }

    /* Try to locate the start of the field value */

    val++;
    while ( *val && ! isdigit( ( unsigned char ) *val )
            && *val != '+' && *val != '-' )
        val++;

    if ( *val == '\0' )    /* no field value found ? */
    {
        print( FATAL, "Invalid data returned by Bruker BH15 field "
               "controller.\n" );
        THROW( EXCEPTION );
    }

    /* Convert the string and check for errors */

    bh15.field = strtod( val, &endptr );
    if ( endptr == val || errno == ERANGE )
    {
        print( FATAL, "Invalid data returned by Bruker BH15 field "
               "controller.\n" );
        THROW( EXCEPTION );
    }

    return bh15.field;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
bh15_command( const char * cmd )
{
    if ( gpib_write( bh15.device, cmd, strlen( cmd ) ) == FAILURE )
    {
        print( FATAL, "Can't access the Bruker BH15 field controller.\n" );
        THROW( EXCEPTION );
    }

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
