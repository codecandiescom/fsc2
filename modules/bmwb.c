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


#include "bmwb.h"


static int get_bridge_type( void );
static void load_state( void );


BMWB bmwb;


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
main( int     argc,
	  char ** argv )
{
    bmwb.EUID = geteuid( );
    bmwb.EGID = getegid( );

    lower_permissions( );

    pthread_mutex_init( &bmwb.mutex, NULL );
    *bmwb.error_msg = '\0';
    bmwb.is_locked = 0;

    if ( ! fl_initialize( &argc, argv, "bmwb", NULL, 0 ) )
	{
		fprintf( stderr, "Failed to intialize graphics.\n" );
        if ( argc > 1 && ! strcmp( argv[ 1 ], "-S" ) )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
	}

	if ( meilhaus_init( ) )
	{
        fprintf( stderr, "%s\n", bmwb.error_msg );
		fl_finish( );
        if ( argc > 1 && ! strcmp( argv[ 1 ], "-S" ) )
            kill( getppid( ), SIGUSR2 );
		return EXIT_FAILURE;
	}

    if ( ( bmwb.type = get_bridge_type( ) ) == TYPE_FAIL )
    {
        fprintf( stderr, "Can't determine bridge type: %s", bmwb.error_msg );
        meilhaus_finish( );
        if ( argc > 1 && ! strcmp( argv[ 1 ], "-S" ) )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
    }

    pthread_mutex_lock( &bmwb.mutex );

    if ( bmwb_open_sock( ) )
    {
        fprintf( stderr, "%s", bmwb.error_msg );
        meilhaus_finish( );
        if ( argc > 1 && ! strcmp( argv[ 1 ], "-S" ) )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
    }


	load_state( );

	graphics_init( );

    if ( argc > 1 && ! strcmp( argv[ 1 ], "-S" ) )
        kill( getppid( ), SIGUSR1 );

    pthread_mutex_unlock( &bmwb.mutex );

	fl_do_forms( );

	return 0;
}


void
error_handling( void )
{
    if ( ! pthread_equal( bmwb.tid, pthread_self( ) ) )
    {
        fprintf( stderr, "%s\n", bmwb.error_msg );
        *bmwb.error_msg = '\0';
    }
}


/*----------------------------*
 * Determines the bridge type
 *----------------------------*/

static int
get_bridge_type( void )
{
#if ! defined BMWB_TEST
    unsigned char v;

    if ( meilhaus_dio_in( DIO_A, &v ) )
        return TYPE_FAIL;

    bmwb.type = v & BRIDGE_TYPE_BIT ? Q_BAND : X_BAND;

    /* Set the upper bits of the DIO to the ones needed for setting the
       attenuation, which depend on the bridge type */

    if ( meilhaus_dio_out( DIO_B, bmwb.type == X_BAND ?
                           X_BAND_ATT_BITS : Q_BAND_ATT_BITS ) )
        return TYPE_FAIL;

    return bmwb.mode;
#else
    return X_BAND;
#endif
}


/*--------------------------------*
 * Sets a new microwave frequency
 *--------------------------------*/

int
set_mw_freq( double val )
{
    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_mw_freq().\n" );
        return 1;
    }

    if ( meilhaus_ao( AO_0, val ) )
        return 1;

	bmwb.freq = val;

    return 0;
}


/*----------------------------------*
 * Sets a new microwave attenuation
 *----------------------------------*/

int
set_mw_attenuation( int val )
{
    unsigned char byte;


    if ( val < MIN_ATTENUATION || val > MAX_ATTENUATION )
    {
        sprintf( bmwb.error_msg, "Invalid argument for "
                 "set_mw_attenuation().\n" );
        return 1;
    }

    byte = (val / 10 ) << 8 | ( val % 10 );

    if ( meilhaus_dio_out( DIO_C, byte ) )
        return 1;

	bmwb.attenuation = val;

    return 0;
}


/*-------------------------*
 * Sets a new signal phase
 *-------------------------*/

int
set_signal_phase( double val )
{
    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_signal_phase().\n" );
        return 1;
    }

    if ( meilhaus_ao( AO_2, - val ) )
        return 1;

	bmwb.signal_phase = val;

    return 0;
}


/*---------------------------------*
 * Sets a new microwave bias power 
 *---------------------------------*/

int
set_mw_bias( double val )
{
    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_mw_bias().\n" );
        return 1;
    }

    if ( meilhaus_ao( AO_1, - val ) )
        return 1;

	bmwb.bias = val;

    return 0;
}


/*-----------------------*
 * Sets a new lock phase 
 *-----------------------*/

int
set_lock_phase( double val )
{
    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_lock_phase().\n" );
        return 1;
    }

    if ( meilhaus_ao( AO_3, - val ) )
        return 1;

	bmwb.lock_phase = val;

    return 0;
}


/*-------------------------*
 * Controls the iris motor
 *-------------------------*/

int
set_iris( int state )
{
    unsigned char v;


    if ( bmwb.type == Q_BAND )
    {
        sprintf( bmwb.error_msg, "set_iris() can't be used with Q-band "
                 "bridge.\n" );
        return 1;
    }

    if ( meilhaus_dio_out_state( DIO_D, &v ) )
        return 1;

    v &= ~ ( IRIS_UP_BIT | IRIS_DOWN_BIT );
                
    switch ( state )
    {
        case 0 :
            break;

        case 1 :
            v |= IRIS_UP_BIT;
            break;

        case -1 :
            v |= IRIS_DOWN_BIT;
            break;

        default :
            sprintf( bmwb.error_msg, "Invalid argument for set_iris().\n" );
            return 1;
    }

    if ( meilhaus_dio_out( DIO_D, v ) )
        return 1;

    return 0;
}


/*------------------------*
 * Switches to a new mode 
 *------------------------*/

int
set_mode( int mode )
{
    unsigned char v;


    if ( meilhaus_dio_out_state( DIO_B, &v ) )
        return 1;

    v &= ~ MODE_BITS;

    switch ( mode )
    {
        case MODE_STANDBY :
            v |= MODE_STANDBY_BITS;
            break;

        case MODE_TUNE :
            v |= MODE_TUNE_BITS;
            break;

        case MODE_OPERATE :
            v |= MODE_OPERATE_BITS;
            break;

        default :
            sprintf( bmwb.error_msg, "Invalid argument for set_mode().\n" );
            return 1;
    }

    if ( meilhaus_dio_out( DIO_B, v ) )
        return 1;

	bmwb.mode = mode;

    return 0;
}


/*---------------------------------------------------------*
 * Reloads the last stored state of the bridge from a file
 *---------------------------------------------------------*/

static void
load_state( void )
{
	FILE *fp = NULL;
	char *fn;


	set_mode( MODE_STANDBY );
	set_mw_attenuation( MAX_ATTENUATION );

	if ( bmwb.type == X_BAND )
	{
		bmwb.min_freq = X_BAND_MIN_FREQ;
		bmwb.max_freq = X_BAND_MAX_FREQ;
	}
	else
	{
		bmwb.min_freq = Q_BAND_MIN_FREQ;
		bmwb.max_freq = Q_BAND_MAX_FREQ;
	}

	if ( bmwb.type == X_BAND )
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_X_BAND_STATE_FILE );
	else
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_Q_BAND_STATE_FILE );

    if (    ! fn
		 || ( fp = bmwb_fopen( fn, "r" ) ) == NULL
		 || fscanf( fp, "%lf %lf %lf %lf", &bmwb.freq, &bmwb.signal_phase,
					&bmwb.bias, &bmwb.lock_phase ) != 4
		 || ! ( bmwb.freq >= 0.0 || bmwb.freq <= 1.0 )
		 || ! ( bmwb.signal_phase >= 0.0 || bmwb.signal_phase <= 1.0 )
		 || ! ( bmwb.bias >= 0.0 || bmwb.bias <= 1.0 )
		 || ! ( bmwb.lock_phase >= 0.0 || bmwb.lock_phase <= 1.0 ) )
	{
		bmwb.freq         = 0.0;
		bmwb.signal_phase = 0.0;
		bmwb.bias         = 0.0;
		bmwb.lock_phase   = 0.0;
    }

	if ( fn )
		free( fn );

	if ( fp )
		fclose( fp );
}


/*-------------------------------------------------*
 * Safes the current state of the bridge to a file
 *-------------------------------------------------*/

void
save_state( void )
{
	char *fn;
	FILE *fp;


	if ( bmwb.type == X_BAND )
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_X_BAND_STATE_FILE );
	else
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_Q_BAND_STATE_FILE );

    if ( fn && ( fp = bmwb_fopen( fn, "r" ) ) != NULL )
	{
		fprintf( fp, "%f %f %f %f\n", bmwb.freq, bmwb.signal_phase,
				 bmwb.bias, bmwb.lock_phase );
		fclose( fp );
	}

	if ( fn )
		free( fn );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
