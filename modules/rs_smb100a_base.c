/* -*-C-*-
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#include <stdio.h>
#include "rs_smb100a.h"
#include "vxi11_user.h"


static void rs_connect( void );
static void rs_disconnect( void );
static void check_model_and_options( void );
static char ** get_options( void );
static void comm_failure( void );


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_init( rs_smb100a_T * rs_cur )
{
	rs = rs_cur;

	rs_connect( );

	outp_init( );
	freq_init( );
	pow_init( );
    am_init( );
    fm_init( );
	pm_init( );
    lfo_init( );
    inp_init( );
    list_init( );
    pulm_init( );
	mod_init( );
    table_init( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_cleanup( void )
{
    table_cleanup( );
	list_cleanup( );
	rs_disconnect( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
rs_connect( void )
{
	rs->is_connected = false;

	if ( FSC2_MODE != EXPERIMENT )
    {
        rs->use_binary = false;
		return;
    }

	if ( vxi11_open( DEVICE_NAME, NETWORK_ADDRESS, VXI11_NAME,
                     false, false, 100000 ) == FAILURE )
	{
		print( FATAL, "Failed to connect to device.\n" );
		THROW( EXCEPTION );
	}

	rs->is_connected = true;

	vxi11_set_timeout( READ, READ_TIMEOUT );
	vxi11_set_timeout( WRITE, WRITE_TIMEOUT );

	vxi11_device_clear( );

//	vxi11_lock_out( true );

    // Tell the device to send all data in ASCII


    // If BINARY_TRANSFER is defined and we're (hopefully) on a machine
    // that uses IEEE 754 set the endianess the device expects data in
    // to our endianess.

#if defined BINARY_TRANSFER
    if ( sizeof( double ) != 8 )
    {
        rs->use_binary = false;
        rs_write( "FORM ASC" );
    }
    else
    {
        rs->use_binary = true;

        rs_write( "FORM PACK" );

        int tst = 1;
        if ( * ( unsigned char * ) &tst  == 1 )
            rs_write( "FORM:BORD NORM" );
        else
            rs_write( "FORM:BORD SWAP" );
    }
#else
    rs->use_binary = false;
        rs_write( "FORM ASC" );
#endif

    check_model_and_options( );
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
rs_disconnect( void )
{
	if ( FSC2_MODE != EXPERIMENT || ! rs->is_connected )
		return;

	vxi11_device_clear( );
	vxi11_close( );
	rs->is_connected = false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_write( char const * data )
{
	size_t len = strlen( data );

	if ( vxi11_write( data, &len, false ) != SUCCESS )
		comm_failure( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_write_n( char const * data,
            size_t       length )
{
	if ( vxi11_write( data, &length, false ) != SUCCESS )
		comm_failure( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

size_t
rs_talk( char const * cmd,
		 char       * reply,
		 size_t       length )
{
	rs_write( cmd );

	if ( vxi11_read( reply, &length, false ) != SUCCESS )
		comm_failure( );

	if ( reply[ length - 1 ] == '\n' )
		reply[ --length ] = '\0';

    return length;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

size_t
rs_talk_bin( char const * cmd,
             char       * reply,
             size_t       length )
{
	rs_write( cmd );

    size_t rec = length;
	if ( vxi11_read( reply, &rec, false ) != SUCCESS )
		comm_failure( );

    return length;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
wait_opc( double max_timeout )
{
	vxi11_set_timeout( READ, lrnd( 1000000 * max_timeout ) );
    if ( ! query_bool( "*OPC?" ) )
        bad_data( );
    vxi11_set_timeout( READ, READ_TIMEOUT );
}    


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
query_bool( char const * cmd )
{
	char reply[ 10 ];

	if ( rs_talk( cmd, reply, sizeof reply ) == 1 )
	{
		if ( reply[ 0 ] == '0' )
			return false;
		else if ( reply[ 0 ] == '1' )
			return true;
	}

	return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
query_int( char const * cmd )
{
	char reply[ 20 ];
	rs_talk( cmd, reply, sizeof reply );

    errno = 0;
    char * ep;
    long res = strtol( reply, &ep, 10 );

    if (    ep == reply
         || *ep
         || errno == ERANGE
         || res > INT_MAX
		 || res < INT_MIN )
		bad_data( );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
query_double( const char * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    errno = 0;
    char * ep;
    double res = strtod( reply, &ep );

    if (    ep == reply
         || *ep
         || errno == ERANGE )
		 bad_data( );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum ALC_State
query_alc_state( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "0" ) )
        return ALC_STATE_OFF;
    else if ( ! strcmp( reply, "1" ) )
        return ALC_STATE_ON;
    else if ( ! strcmp( reply, "AUTO" ) )
        return ALC_STATE_AUTO;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Coupling
query_coupling( char const * cmd )
{
	char reply[ 20 ];
	rs_talk( cmd, reply, sizeof reply );

	if ( ! strcmp( reply, "AC" ) )
        return COUPLING_AC;
	else if ( ! strcmp( reply, "DC" ) )
		return COUPLING_DC;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Source
query_source( char const * cmd )
{
	char reply[ 20 ];
	rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "INT" ) )
        return SOURCE_INT;
    else if ( ! strcmp( reply, "EXT" ) )
        return SOURCE_EXT;
    else if ( ! strcmp( reply, "INT,EXT" ) )
        return SOURCE_INT_EXT;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Power_Mode
query_pow_mode( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "NORM" ) )
        return POWER_MODE_NORMAL;
    else if ( ! strcmp( reply, "LOWN" ) )
        return POWER_MODE_LOW_NOISE;
    else if ( ! strcmp( reply, "LOWD" ) )
        return POWER_MODE_LOW_DISTORTION;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Off_Mode
query_off_mode( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "UNCH" ) )
        return OFF_MODE_UNCHANGED;
    else if ( ! strcmp( reply, "FATT" ) )
        return OFF_MODE_FATT;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Mod_Mode
query_mod_mode( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "NORM" ) )
        return MOD_MODE_NORMAL;
    else if ( !strcmp( reply, "LNO" ) )
        return MOD_MODE_LOW_NOISE;
    else if ( ! strcmp( reply, "HDEV" ) )
        return MOD_MODE_HIGH_DEVIATION;

    return bad_data( );
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
query_imp( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( !strcmp( reply, "LOW" ) )
        return IMPEDANCE_LOW;
    else if ( ! strcmp( reply, "G50" ) )
        return IMPEDANCE_G50;
    else if ( ! strcmp( reply, "G600" ) )
        return IMPEDANCE_G600;
    else if ( ! strcmp( reply, "G1K" ) )
        return IMPEDANCE_G1K;
    else if ( ! strcmp( reply, "G10K" ) )
        return IMPEDANCE_G10K;
    else if ( ! strcmp( reply, "HIGH" ) )
        return IMPEDANCE_HIGH;

    return bad_data( );
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Slope
query_slope( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "POS" ) )
        return SLOPE_POSITIVE;
    else if ( ! strcmp( reply, "NEG" ) )
        return SLOPE_NEGATIVE;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Polarity
query_pol( char const * cmd )
{
	char reply[ 20 ];
    rs_talk( cmd, reply, sizeof reply );

    if ( ! strcmp( reply, "NORM" ) )
        return POLARITY_NORMAL;
    else if ( ! strcmp( reply, "INV" ) )
        return POLARITY_INVERTED;

    return bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double *
query_list( char const * cmd,
            int          list_length  )
{
    char * reply = NULL;
    double * res = NULL;
    CLOBBER_PROTECT( reply );

    if ( rs->use_binary )
    {
        TRY
        {
            size_t len = 8 * list_length;
            reply = T_malloc( 7 + len );
            len = rs_talk_bin( cmd, reply, 7 + len );

            if (    len < 2
                 || reply[ 0 ] != '#'
                 || ! isdigit( ( int ) reply[ 1 ] ) )
                bad_data( );

            size_t cnt = reply[ 1 ] - '0';

            if ( len < 2 + cnt )
                bad_data( );

            int total = 0;
            for ( size_t i = 0; i < cnt; i++ )
            {
                if ( ! isdigit( ( int ) reply[ 2 + i ] ) )
                    bad_data( );
                total = total * 10 + reply[ 2 + i ] - '0';
            }

            if (    2 + cnt + total != len
                 || total != 8 * list_length )
                bad_data( );

            res = T_malloc( total );
            memcpy( res, reply + 2 + cnt, total );

            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( reply );
            RETHROW;
        }

        T_free( reply );
        return res;
    }

    reply = T_malloc( 15 * list_length );

    TRY
    {
        rs_talk( cmd, reply, 15 * list_length );

        res = T_malloc( list_length * sizeof *res );
        char * p = reply;
        int cnt = 0;

        while ( ( p = strtok( p, "," ) ) )
        {
            char * ep;
            double r = strtod( p, &ep );
            if ( ep == p || *ep || errno == ERANGE )
                bad_data( );

            res[ cnt++ ] = r;
            p = NULL;
        }

        if ( cnt != list_length )
            bad_data( );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( res );
        T_free( reply );
        RETHROW;
    }

    T_free( reply );
    return res;
}


/*----------------------------------------------------*
 * Method for finding out which model we're talking to
 * and what the installed options are.
 *----------------------------------------------------*/

static
void
check_model_and_options( void )
{
#if defined B101
	char const * model_str = "SMB-B101";
	char const * pulse_mod_str = "SMB-K22";
#elif defined B102
	char const * model_str = "SMB-B102";
	char const * pulse_mod_str = "SMB-K22";
#elif defined B103
	char const * model_str = "SMB-B103";
	char const * pulse_mod_str = "SMB-K22";
#elif defined B106
	char const * model_str = "SMB-B106";
	char const * pulse_mod_str = "SMB-K22";
#elif defined B112
	char const * model_str = "SMB-B112";
	char const * pulse_mod_str = "SMB-K21";
#elif defined B112L
	char const * model_str = "SMB-B112L";
	char const * pulse_mod_str = "SMB-K21";
#endif

	char ** opts = get_options( );

    if ( strcmp( opts[ 0 ], model_str ) )
	{
        print( FATAL, "The module was compiled for model %s but the "
               "device reports to be of type %s.\n", model_str, opts[ 0 ] );
		THROW( EXCEPTION );
	}

#if defined WITH_PULSE_MODULATION
    bool found = false;
    for ( size_t i = 0; opts[ i ]; i++ )
        if ( ! strcmp( opts[ i ], pulse_mod_str ) )
        {
            found = true;
            break;
        }

    if ( ! found )
	{
        print( FATAL, "The module was compiled with support for pulse "
               "modulattion, but option %s isn't installed.\n", pulse_mod_str );
		THROW( EXCEPTION );
	}
#endif

#if defined WITH_PULSE_GENERATION
    found = false;
    for ( size_t i = 0; opts[ i ]; i++ )
        if ( ! strcmp( opts[ i ], "SMB-K23" ) )
        {
            found = true;
            break;
        }

    if ( ! found )
	{
        print( FATAL, "The module was compiled with support for pulse "
               "generation, but option SMB-K23 isn't installed.\n" );
		THROW( EXCEPTION );
	}
#endif

    rs->has_reverse_power_protection = true;
#if defined B112 || defined B112L
    rs->has_reverse_power_protection = false;
    for ( size_t i = 0; opts[ i ]; i++ )
        if ( ! strcmp( opts[ i ], "SMB-B30" ) )
        {
            rs->has_reverse_power_protection = true;
            break;
        }
#endif

    for ( size_t i = 0; opts[ i ]; i++ )
        T_free( opts[ i ] );
    T_free( opts );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static
char **
get_options( void )
{
    char buf[ 1000 ];

    rs_talk( "*OPT?", buf, sizeof buf );

    if ( ! *buf )
        comm_failure( );

    char *bp = buf;
    char **res = T_malloc( sizeof *res );
    *res = NULL;
    int cnt = 1;

    while ( ( bp = strtok( bp, ", " ) ) )
    {
        res = T_realloc( res, ++cnt * sizeof *res );
        res[ cnt - 2 ] = T_strdup( bp );
        res[ cnt - 1 ] = NULL;
        bp = NULL;
    }

    return res;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static
void
comm_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int
bad_data( void )
{
	print( FATAL, "Invalid data received from device.\n" );
	THROW( EXCEPTION );

	return 0;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

long
impedance_to_int( enum Impedance imp )
{
	switch ( imp )
	{
		case IMPEDANCE_LOW :
			return 10;

		case IMPEDANCE_G50 :
			return 50;

		case IMPEDANCE_G600 :
			return 600;

		case IMPEDANCE_G1K :
			return 1000;

		case IMPEDANCE_G10K :
			return 10000;

		case IMPEDANCE_HIGH :
			return 200000;
	}

	fsc2_impossible( );
	return -1;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

enum Impedance
int_to_impedance( long imp )
{
	switch ( imp )
	{
		case 10 :
			return IMPEDANCE_LOW;

		case 50 :
			return IMPEDANCE_G50;

		case 600 :
			return IMPEDANCE_G600;

		case 1000 :
			return IMPEDANCE_G1K;

		case 10000 :
			return IMPEDANCE_G10K;

		case 200000 :
			return IMPEDANCE_HIGH;
	}

	print( FATAL, "Invalid impedance value of %ld Ohm.\n", imp );
	THROW( EXCEPTION );
	return -1;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

char const *
impedance_to_name( enum Impedance imp )
{
	switch ( imp )
	{
		case IMPEDANCE_LOW :
			return "LOW";

		case IMPEDANCE_G50 :
			return "G50";

		case IMPEDANCE_G600 :
			return "G600";

		case IMPEDANCE_G1K :
			return "G1000";

		case IMPEDANCE_G10K :
			return "G10000";

		case IMPEDANCE_HIGH :
			return "HIGH";
	}

	fsc2_impossible( );
	return "";
}



/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
