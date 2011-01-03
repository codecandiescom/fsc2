/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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
static int check_unique( void );


BMWB bmwb;


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
main( int     argc,
	  char ** argv )
{
    int do_signal;
    int i;
    char *app_name;
    int bad_arg = 0;
    int type;


    bmwb.EUID = geteuid( );
    bmwb.EGID = getegid( );

    lower_permissions( );

    pthread_mutex_init( &bmwb.mutex, NULL );
    *bmwb.error_msg = '\0';
    bmwb.a_is_active = 0;
    bmwb.c_is_active = 0;
    bmwb.type = TYPE_FAIL;

    /* Check the application name to see if the user explicitely wants
       to use the X-band or Q-band bridge, overruling the built-in
       automatic determination of its type. */

    if ( ( app_name = strrchr( argv[ 0 ], '/' ) ) == NULL )
        app_name = argv[ 0 ];
    else
        app_name++;

    if ( ! strcmp( app_name, "xbmwb" ) )
        bmwb.type = X_BAND;
    else if ( ! strcmp( app_name, "qbmwb" ) )
        bmwb.type = Q_BAND;

    /* Check command line arguments. '-X' or '-Q' allow to select the
       bridge type and by '-S' the invoking process may tell that it
       wants to get a signal on success (SIGUSR1) or failure (SIGUSR2) */

    for ( i = 1; i < argc; i++ )
        if ( ! strcmp( argv[ i ], "-X" ) )
        {
            if ( bmwb.type == Q_BAND )
                bad_arg = 1;
            else
                bmwb.type = X_BAND;
        }
        else if ( ! strcmp( argv[ i ], "-Q" ) )
        {
            if ( bmwb.type == X_BAND )
                bad_arg = 1;
            else
                bmwb.type = Q_BAND;
        }
        else if ( ! strcmp( argv[ i ], "-S" ) )
            do_signal = 1;
        else
        {
            bad_arg = 1;
            break;
        }

    if ( bad_arg )
    {
        if ( strcmp( app_name, "qbmwb" ) && strcmp( app_name, "xbmwb" ) )
            fprintf( stderr, "Usage: %s [ OPTION... ]\n\n"
                     "  -X Connect to X-band bridge\n"
                     "  -Q Connect to Q-band bridge\n"
                     "  -S Send signal after initialization\n", app_name );
        else
            fprintf( stderr, "Usage: %s [ OPTION ]\n\n"
                     "  -S Send signal after initialization\n", app_name );
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
	}

    /* Check if another instance of the program is already running */

    if ( check_unique( ) )
    {
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
	}

    /* Initialize graphics */

    if ( ! fl_initialize( &argc, argv, "bmwb", NULL, 0 ) )
	{
		fprintf( stderr, "Failed to intialize graphics.\n" );
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
	}

    /* Initialize the Meilhaus card */

	if ( meilhaus_init( ) )
	{
        fprintf( stderr, "%s\n", bmwb.error_msg );
		fl_finish( );
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
		return EXIT_FAILURE;
	}

    /* Try to determine type of bridge */

    if ( ( type = get_bridge_type( ) ) == TYPE_FAIL )
    {
        fprintf( stderr, "Can't determine bridge type: %s\n", bmwb.error_msg );
        meilhaus_finish( );
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
    }

    /* Check for inconsistencies */

    if ( bmwb.type != TYPE_FAIL && bmwb.type != type )
        fprintf( stderr, "Warning: Requested bridge type differs from "
                 "detected type (requested was %s, detected is %s).\n",
                 bmwb.type == X_BAND ? "X-band" : "Q-band",
                 type == X_BAND ? "X-band" : "Q-band" );
    else
        bmwb.type = type;

    /* Get the last settings of the bridge from  state file */

	load_state( );

	graphics_init( );

    /* Open a socket we're listening on and which might be used to send
       commands for controling the bridge */

    if ( bmwb_open_sock( ) )
    {
        fprintf( stderr, "%s\n", bmwb.error_msg );
        meilhaus_finish( );
        if ( do_signal )
            kill( getppid( ), SIGUSR2 );
        return EXIT_FAILURE;
    }

    /* Signal a process we might be invoked from that initialization is
       finished */

    if ( do_signal )
        kill( getppid( ), SIGUSR1 );

    /* All intialization done go into the graphics loop */

	fl_do_forms( );

	return 0;
}


/*-------------------------------------------*
 *-------------------------------------------*/

void
error_handling( void )
{
    if (    ! pthread_equal( bmwb.c_thread, pthread_self( ) )
         && ! pthread_equal( bmwb.a_thread, pthread_self( ) ) )
    {
        fprintf( stderr, "%s\n", bmwb.error_msg );
        *bmwb.error_msg = '\0';
    }
}


/*---------------------------------------------------------*
 * Function for determining the bridge type (X- or Q-band)
 *---------------------------------------------------------*/

static int
get_bridge_type( void )
{
#if ! defined BMWB_TEST
    unsigned char v;


    /* Get the value from the DIO that receives the bit for determining
       the bridge type */

    if ( meilhaus_dio_in( DIO_A, &v ) )
        return TYPE_FAIL;

    /* If BRIDGE_TYPE_BIT is set this is a Q-band bridge, otherwise a
       X-band bridge */

    bmwb.type = ( v & BRIDGE_TYPE_BIT ) ? Q_BAND : X_BAND;

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


/*------------------------------------------------*
 * Function for setting a new microwave frequency
 *------------------------------------------------*/

int
set_mw_freq( double val )
{
    double volts;


    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_mw_freq()." );
        return 1;
    }

    volts = val * ( bmwb.type == X_BAND ?
                    X_BAND_FREQ_FACTOR : Q_BAND_FREQ_FACTOR );

    if ( meilhaus_ao( FREQUENCY_AO, volts ) )
        return 1;

	bmwb.freq = val;

    return 0;
}


/*--------------------------------------------------*
 * Function for setting a new microwave attenuation
 *--------------------------------------------------*/

int
set_mw_attenuation( int val )
{
    unsigned char byte;


    /* Allowed range is 0 to 60 dB */

    if ( val < MIN_ATTENUATION || val > MAX_ATTENUATION )
    {
        sprintf( bmwb.error_msg, "Invalid argument for "
                 "set_mw_attenuation()." );
        return 1;
    }

    /* The bits to set the attenuation are two decimal coded nibbles,
       one for powers of 10 and one for the remainder of the attenuation
       in dB */

    byte = ( val / 10 ) << 8 | ( val % 10 );

    if ( meilhaus_dio_out( DIO_C, byte ) )
        return 1;

	bmwb.attenuation = val;

    return 0;
}


/*-----------------------------------------*
 * Function for setting a new signal phase
 *-----------------------------------------*/

int
set_signal_phase( double val )
{
    double volts;


    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_signal_phase()." );
        return 1;
    }

    volts = val * ( bmwb.type == X_BAND ?
                    X_BAND_PHASE_FACTOR : Q_BAND_PHASE_FACTOR );

    if ( meilhaus_ao( SIGNAL_PHASE_AO, volts ) )
        return 1;

	bmwb.signal_phase = val;

    return 0;
}


/*-------------------------------------------------*
 * Function for setting a new microwave bias power 
 *-------------------------------------------------*/

int
set_mw_bias( double val )
{
    double volts;


    if ( val < 0.0 || val > 1.0 )
    {
        sprintf( bmwb.error_msg, "Invalid argument for set_mw_bias()." );
        return 1;
    }

    volts = val * ( bmwb.type == X_BAND ?
                    X_BAND_BIAS_FACTOR : Q_BAND_BIAS_FACTOR );

    if ( meilhaus_ao( BIAS_AO, volts ) )
        return 1;

	bmwb.bias = val;

    return 0;
}


/*------------------------------------------------------*
 * Function for controlling the iris motor, argument is
 * 0 to stop the motor, 1 to go up and -1 to go down.
 *------------------------------------------------------*/

int
set_iris( int state )
{
    unsigned char v;


    if ( meilhaus_dio_out_state( DIO_D, &v ) )
        return 1;

    v &= ~ ( IRIS_UP_BIT | IRIS_DOWN_BIT );
                
    switch ( state )
    {
        case IRIS_STOP :
            break;

        case IRIS_UP :
            v |= IRIS_UP_BIT;
            break;

        case IRIS_DOWN :
            v |= IRIS_DOWN_BIT;
            break;

        default :
            sprintf( bmwb.error_msg, "Invalid argument for set_iris()." );
            return 1;
    }

    if ( meilhaus_dio_out( DIO_D, v ) )
        return 1;

    return 0;
}


/*--------------------------------------*
 * Function for switching to a new mode 
 *--------------------------------------*/

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
            /* Security feature: when switching to tune mode make sure
               that the attenuation isn't too high */

            if ( bmwb.attenuation < SAFE_TUNE_MODE_ATTENUATION )
            {
                if ( set_mw_attenuation( SAFE_TUNE_MODE_ATTENUATION ) )
                    return 1;
                fl_set_counter_value( bmwb.rsc->attenuation_counter,
                                      bmwb.attenuation );
            }

            v |= MODE_TUNE_BITS;
            break;

        case MODE_OPERATE :
            v |= MODE_OPERATE_BITS;
            break;

        default :
            sprintf( bmwb.error_msg, "Invalid argument for set_mode()." );
            return 1;
    }

    if ( meilhaus_dio_out( DIO_B, v ) )
        return 1;

	bmwb.mode = mode;

    return 0;
}


/*-------------------------------------------------------*
 * Loads the last stored state of the bridge from a file
 *-------------------------------------------------------*/

static void
load_state( void )
{
	FILE *fp = NULL;
	char *fn;


    /* Make sure the bridge is in a safe state (STANDBY and max. attenuation) */

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

    raise_permissions( );

    if (    ! fn
		 || ( fp = bmwb_fopen( fn, "r" ) ) == NULL
		 || fscanf( fp, "%lf %lf %lf", &bmwb.freq, &bmwb.signal_phase,
					&bmwb.bias ) != 3
		 || ! ( bmwb.freq >= 0.0 || bmwb.freq <= 1.0 )
		 || ! ( bmwb.signal_phase >= 0.0 || bmwb.signal_phase <= 1.0 )
		 || ! ( bmwb.bias >= 0.0 || bmwb.bias <= 1.0 ) )
	{
		bmwb.freq         = 0.0;
		bmwb.signal_phase = 0.0;
		bmwb.bias         = 0.0;
    }

	if ( fp )
		fclose( fp );

    lower_permissions( );

	if ( fn )
		free( fn );
}


/*-------------------------------------------------*
 * Safes the current state of the bridge to a file
 *-------------------------------------------------*/

void
save_state( void )
{
	char *fn;
	FILE *fp;


    raise_permissions( );

	if ( bmwb.type == X_BAND )
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_X_BAND_STATE_FILE );
	else
		fn = get_string( "%s%s%s", libdir, slash( libdir ),
						 BMWB_Q_BAND_STATE_FILE );

    if ( fn && ( fp = bmwb_fopen( fn, "w" ) ) )
	{
		fprintf( fp, "%f %f %f\n", bmwb.freq, bmwb.signal_phase,
				 bmwb.bias );
		fclose( fp );
	}

    lower_permissions( );

	if ( fn )
		free( fn );

}


/*-------------------------------------------*
 * Tests if there's another instance of the program already running by
 * trying to connect to the socket it's then supposed to listen on.
 *-------------------------------------------*/

static int
check_unique( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;


    /* Try to get a socket (but first make sure the name isn't too long) */

    if (    sizeof P_tmpdir + 8 > sizeof serv_addr.sun_path
         || ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
	{
		fprintf( stderr, "Can't connect to microwave bridge.\n" );
        return 1;
	}

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, P_tmpdir "/bmwb.uds" );

	/* Try to connect to the server */

    if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
                  sizeof serv_addr ) != -1 )
    {
        fprintf( stderr, "Another instance of the program seems already to be "
                 "running.\n" );
        return 1;
    }

    shutdown( sock_fd, SHUT_RDWR );
    close( sock_fd );
    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
