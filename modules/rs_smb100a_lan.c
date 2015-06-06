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


#include "rs_smb100a.h"


static void rs_smb100a_initial_mod_setup( void );

static char ** rs_smb100a_get_options( void );

#if defined WITH_PULSE_MODULATION
static bool rs_smb100a_get_pulse_state( void );

static bool rs_smb100a_get_pulse_trig_slope( void );

static double rs_smb100a_get_pulse_width( void );

static double rs_smb100a_get_pulse_delay( void );

static double rs_smb100a_get_double_pulse_delay( void );
#endif

static void rs_smb100a_comm_failure( void );

static bool rs_smb100a_talk( const char * cmd,
                             char *       reply,
                             size_t *     length );


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

bool
rs_smb100a_init( const char * name )
{
	if ( vxi11_open( name, NETWORK_ADDRESS, VXI11_NAME,
                     false, false, 100000 ) == FAILURE )
        return false;

	rs_smb100a.device = 0;

	vxi11_set_timeout( READ, READ_TIMEOUT );
	vxi11_set_timeout( WRITE, WRITE_TIMEOUT );

	vxi11_device_clear( );

//	vxi11_lock_out( true );

    /* Check if the device reports the same model the module was compiled
       for and, if the module was compiled with support for pulse modulation,
       if the corresponding option is present. */

    char ** opts = rs_smb100a_get_options( );

#if defined B101
    if ( strcmp( opts[ 0 ], "SMB-B101" ) )
        print( SEVERE, "Module was compiled for model SMB-B101 but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#elif defined B102
    if ( strcmp( opts[ 0 ], "SMB-B102" ) )
        print( SEVERE, "Module was compiled for model SMB-B102 but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#elif defined B103
    if ( strcmp( opts[ 0 ], "SMB-B103" ) )
        print( SEVERE, "Module was compiled for model SMB-B103 but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#elif defined B106
    if ( strcmp( opts[ 0 ], "SMB-B106" ) )
        print( SEVERE, "Module was compiled for model SMB-B106 but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#elif defined B112
    if ( strcmp( opts[ 0 ], "SMB-B112" ) )
        print( SEVERE, "Module was compiled for model SMB-B112 but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#elif defined B112L
    if ( strcmp( opts[ 0 ], "SMB-B112L" ) )
        print( SEVERE, "Module was compiled for model SMB-B112L but the "
               "device claims to be of type %s.\n", opts[ 0 ] );
#endif

#if defined WITH_PULSE_MODULATION
#if ! defined B112 && ! defined B112L
    const char * pm = "SMB-K22";
#else
    const char * pm = "SMB-K21";
#endif

    bool found = false;
    for ( size_t i = 0; opts[ i ]; i++ )
        if ( ! strcmp( opts[ i ], pm ) )
        {
            found = true;
            break;
        }

    if ( ! found )
        print( SEVERE, "Module was compiled with support for pulse "
               "modulattion, but option %s isn't installed.\n", pm );
#endif

    for ( size_t i = 0; opts[ i ]; i++ )
        T_free( opts[ i ] );
    T_free( opts );

    /* Set up default settings */

    rs_smb100a_command( "CORR:STAT OFF\n" );
    rs_smb100a_command( "FREQ:MODE CW\n" );
    rs_smb100a_command( "FREQ:ERAN OFF\n" );
    rs_smb100a_command( "FREQ:OFFS 0\n" );
    rs_smb100a_command( "POW:MODE CW\n" );
    rs_smb100a_command( "POW:OFFS 0\n" );
    rs_smb100a_command( "POW:ALC ON\n" );
    rs_smb100a_command( "OUTP:AMOD AUTO\n" );
#if defined WITH_PULSE_MODULATION
    rs_smb100a_command( "PULM:SOUR INT\n" );
    rs_smb100a_command( "PULM:POL NORM\n" );
#endif

    /* Figure out the current frequency if it's not going to be set */

    if ( ! rs_smb100a.freq_is_set )
        rs_smb100a.freq = rs_smb100a_get_frequency( );
    else
        rs_smb100a_set_frequency( rs_smb100a.freq );

    /* Set or get the current attenuation */

    double att;
    if ( rs_smb100a.attenuation_is_set )
    {
        if ( rs_smb100a.use_table )
        {
            att =   rs_smb100a.attenuation
                  + rs_smb100a_get_att_from_table( rs_smb100a.freq )
                  - rs_smb100a.att_at_ref_freq;
            if ( att < MAX_ATTEN )
            {
                print( SEVERE, "Attenuation range is too low, using "
                       "%.1f dBm instead of %.1f dBm.\n", MAX_ATTEN, att );
                att = MAX_ATTEN;
            }
            if ( att > rs_smb100a.min_attenuation )
            {
                print( SEVERE, "Attenuation range is too high, using "
                       "%.1f dBm instead of %.1f dBm.\n",
                       rs_smb100a.min_attenuation, att );
                att = rs_smb100a.min_attenuation;
            }
        }
        else
            att = rs_smb100a.attenuation;

        rs_smb100a_set_attenuation( att );
    }
    else
        rs_smb100a.attenuation = rs_smb100a_get_attenuation( );

    /* Now we set the modulation type if it has been set, otherwise ask the
       synthesizer for its currents setting */

    if ( rs_smb100a.mod_type_is_set )
    {
        rs_smb100a_check_mod_ampl( rs_smb100a.freq );
        rs_smb100a_set_mod_type( rs_smb100a.mod_type );
    }
    else
        rs_smb100a_initial_mod_setup( );

    /* Set source and amplitude for each modulation type as far as it's set */

    for ( size_t i = 0; i < NUM_MOD_TYPES - 1; i++ )
    {
        if ( rs_smb100a.mod_source_is_set[ i ] )
            rs_smb100a_set_mod_source( i, rs_smb100a.mod_source[ i ],
                                     rs_smb100a.mod_freq[ i ] );
        else
        {
            rs_smb100a.mod_source[ i ] =
                       rs_smb100a_get_mod_source( i, rs_smb100a.mod_freq + i );
            rs_smb100a.mod_source_is_set[ i ] = true;
        }

        if ( rs_smb100a.mod_ampl_is_set[ i ] )
            rs_smb100a_set_mod_ampl( i, rs_smb100a.mod_ampl[ i ] );
        else
        {
            rs_smb100a.mod_ampl[ i ] = rs_smb100a_get_mod_ampl( i );
            rs_smb100a.mod_ampl_is_set[ i ] = true;
        }
    }

#if defined WITH_PULSE_MODULATION

    if ( rs_smb100a.pulse_trig_slope_is_set )
        rs_smb100a_set_pulse_trig_slope( rs_smb100a.pulse_trig_slope );
    else
        rs_smb100a.pulse_trig_slope = rs_smb100a_get_pulse_trig_slope( );

    /* If double pulse mode is going to be switched on make sure the distance
       between both the pulses is going to be larger than the pulse width */

    if (    rs_smb100a.double_pulse_mode_is_set
         && rs_smb100a.double_pulse_mode == true )
    {
        if ( ! rs_smb100a.pulse_width_is_set )
            rs_smb100a.pulse_width = rs_smb100a_get_pulse_width( );

        if ( ! rs_smb100a.double_pulse_delay_is_set )
            rs_smb100a.double_pulse_delay =
                                          rs_smb100a_get_double_pulse_delay( );

        if ( lrnd( ( rs_smb100a.double_pulse_delay - rs_smb100a.pulse_width )
                   / MIN_PULSE_WIDTH ) <= 0 )
        {
            print( FATAL, "Setting for pulse width is too short for double "
                   "pulse delay.\n" );
            THROW( EXCEPTION );
        }
    }

    /* Switch off double pulse mode per default and only switch it on again
       later on if it has been explicitely requested */

    rs_smb100a_set_double_pulse_mode( false );

    if ( rs_smb100a.pulse_width_is_set )
        rs_smb100a_set_pulse_width( rs_smb100a.pulse_width );
    else
    {
        rs_smb100a.pulse_width = rs_smb100a_get_pulse_width( );
        rs_smb100a.pulse_width_is_set = true;
    }

    if ( rs_smb100a.double_pulse_delay_is_set )
        rs_smb100a_set_double_pulse_delay( rs_smb100a.double_pulse_delay );
    else
    {
        rs_smb100a.double_pulse_delay = rs_smb100a_get_double_pulse_delay( );
        rs_smb100a.double_pulse_mode_is_set = true;
    }

    /* Switch off double pulse mode per default unless it has been explicitely
       requested */

    if ( rs_smb100a.double_pulse_mode_is_set )
        rs_smb100a_set_double_pulse_mode( rs_smb100a.double_pulse_mode );
    else
    {
        rs_smb100a.double_pulse_mode = false;
        rs_smb100a.double_pulse_mode_is_set = false;
    }

    if ( rs_smb100a.pulse_delay_is_set )
        rs_smb100a_set_pulse_delay( rs_smb100a.pulse_delay );
    else
        rs_smb100a.pulse_delay = rs_smb100a_get_pulse_delay( );

    if ( rs_smb100a.pulse_mode_state_is_set )
        rs_smb100a_set_pulse_state( rs_smb100a.pulse_mode_state );
    else
        rs_smb100a.pulse_mode_state = rs_smb100a_get_pulse_state( );
#endif

    rs_smb100a_set_output_state( rs_smb100a.state );

    return OK;
}


/*--------------------------------------------------------------*
 * Function figures out which types of modulation are currently
 * switched on. If none or only one is on nothing is changed,
 * but if two or more modulations are on all are switched off.
 * Afterwards the bandwidth for all types of modulation is
 * switched to STANDARD.
 *--------------------------------------------------------------*/

static void
rs_smb100a_initial_mod_setup( void )
{
    unsigned flags = 0;
    const char *types[ ] = { "FM", "AM", "PM" };
    char cmd[ 100 ];
    char buffer[ 100 ];
    size_t length;
    unsigned int i;


    for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
    {
        length = sizeof buffer;
        sprintf( cmd, "%s:STAT?\n", types[ i ] );
        rs_smb100a_talk( cmd, buffer, &length );

        if ( buffer[ 0 ] == '1' )
            flags |= 1 << i;
    }

    switch ( flags )
    {
        case 0 :
            rs_smb100a.mod_type_is_set = false;
            rs_smb100a.mod_type = MOD_TYPE_OFF;
            break;

        case 1 :
            rs_smb100a.mod_type_is_set = true;
            rs_smb100a.mod_type = MOD_TYPE_FM;
            rs_smb100a_check_mod_ampl( rs_smb100a.freq );
            break;

        case 2 :
            rs_smb100a.mod_type_is_set = true;
            rs_smb100a.mod_type = MOD_TYPE_AM;
            break;

        case 4 :
            rs_smb100a.mod_type_is_set = true;
            rs_smb100a.mod_type = MOD_TYPE_PM;
            rs_smb100a_check_mod_ampl( rs_smb100a.freq );
            break;

        default :
            for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
            {
                if ( ! ( flags & ( 1 << i ) ) )
                    continue;
                sprintf( cmd, "%s:STAT OFF", types[ i ] );
                rs_smb100a_talk( cmd, buffer, &length );
            }
            rs_smb100a.mod_type_is_set = false;
            rs_smb100a.mod_type = MOD_TYPE_OFF;
    }

    for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
    {
        if ( i == MOD_TYPE_AM )
            continue;
        sprintf( buffer, "%s:BAND STAN\n", types[ i ] );
        rs_smb100a_command( buffer );
    }
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_finished( void )
{
	if ( rs_smb100a.device != -1 )
	{
		vxi11_device_clear( );
		rs_smb100a_command( ":CDIS;:RUN\n" );
		vxi11_close( );
		rs_smb100a.device = -1;
	}
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static
char **
rs_smb100a_get_options( void )
{
    char buf[ 1000 ];
    size_t length = sizeof buf;

    rs_smb100a_talk( "*OPT?\n", buf, &length );

    if ( ! *buf || *buf == '\n' )
        rs_smb100a_comm_failure( );

    buf[ length - 1 ] = '\0';

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

bool
rs_smb100a_set_output_state( bool state )
{
    char cmd[ 100 ];


    sprintf( cmd, "OUTP %s\n", state ? "ON" : "OFF" );
    rs_smb100a_command( cmd );

    return state;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

bool
rs_smb100a_get_output_state( void )
{
    char buffer[ 10 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "OUTP?\n", buffer, &length );
    return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_set_frequency( double freq )
{
    char cmd[ 100 ];
    size_t length = sizeof cmd;


    fsc2_assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

    /* We have to wait here for the GPIB command for setting the frequency
       to be finished since otherwise sometimes pulses created afterwards
       were missing */

    if ( rs_smb100a.freq_change_delay < 1.0e-3 )
    {
        sprintf( cmd, "FREQ:CW %.0f;*WAI;*OPC?\n", freq );
        rs_smb100a_talk( cmd, cmd, &length );
    }
    else
    {
        sprintf( cmd, "FREQ:CW %.0f\n", freq );
        rs_smb100a_command( cmd );
        fsc2_usleep( lrnd( 1.0e6 * rs_smb100a.freq_change_delay ), false );
    }

    return freq;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_get_frequency( void )
{
    char buffer[ 100 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "FREQ:CW?\n", buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_set_attenuation( double att )
{
    char cmd[ 100 ];


    fsc2_assert( att >= MAX_ATTEN && att <= rs_smb100a.min_attenuation );

    sprintf( cmd, "POW %6.1f\n", att );
    rs_smb100a_command( cmd );

    return att;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_get_attenuation( void )
{
    char buffer[ 100 ];
    size_t length = sizeof buffer;

    rs_smb100a_talk( "POW?\n", buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int
rs_smb100a_set_mod_type( int type )
{
    switch ( type )
    {
        case MOD_TYPE_FM :
            rs_smb100a_command( "AM:STAT OFF;PM:STAT OFF;FM:STAT ON\n" );
            break;

        case MOD_TYPE_AM :
            rs_smb100a_command( "FM:STAT OFF;PM:STAT OFF;AM:STAT ON\n" );
            break;

        case MOD_TYPE_PM :
            rs_smb100a_command( "AM:STAT OFF;FM:STAT OFF;PM STAT ON\n" );
            break;

        case MOD_TYPE_OFF :
            rs_smb100a_command( "AM:STAT OFF;FM:STAT OFF;PM STAT OFF\n" );
            break;

        default :                         /* should never happen... */
            fsc2_impossible( );
    }

    return type;
}


/*--------------------------------------------------------------*
 * Function returns a number between 0 and (NUM_MOD_TYPES - 2)
 * indicating the modulation type that is currently switched on
 * or -1 if none is switched on.
 *--------------------------------------------------------------*/

int
rs_smb100a_get_mod_type( void )
{
    const char *types[ ] = { "FM", "AM", "PM" };
    char cmd[ 100 ];
    char buffer[ 100 ];
    size_t length;
    int i;


    for ( i = 0; i < NUM_MOD_TYPES - 1; i++ )
    {
        length = sizeof buffer;
        sprintf( cmd, "%s:STAT?\n", types[ i ] );
        rs_smb100a_talk( cmd, buffer, &length );

        if ( buffer[ 0 ] == '1' )
            return i;
    }

    return UNDEFINED;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int
 rs_smb100a_set_mod_source( int    type,
                          int    source,
                          double freq )
{
    const char *types[ ] = { "FM", "AM", "PM" };
    char cmd[ 100 ];


    fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );
    fsc2_assert( source >= 0 && source < NUM_MOD_SOURCES );

    switch( source )
    {
        case MOD_SOURCE_AC :
            sprintf( cmd, "%s:SOUR EXT;COUP AC\n", types[ type ] );
            break;

        case MOD_SOURCE_DC :
            sprintf( cmd, "%s:SOUR EXT;COUP DC\n", types[ type ] );
            break;

        case MOD_SOURCE_INT :
            sprintf( cmd, "%s:SOUR INT;FREQ %f\n", types[ type ], freq );
            break;

        default :                         /* this can never happen... */
            fsc2_impossible( );
    }

    rs_smb100a_command( cmd );

    return source;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

int
rs_smb100a_get_mod_source( int      type,
                           double * freq )
{
    const char *types[ ] = { "FM", "AM", "PM" };
    char cmd[ 100 ];
    char buffer[ 100 ];
    size_t length;
    int source;


    fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

    sprintf( cmd, "%s:SOUR?\n", types[ type ] );
    length = sizeof buffer;
    rs_smb100a_talk( cmd, buffer, &length );

    length = sizeof buffer;
    if ( ! strncmp( buffer, "INT", 3 ) )
    {
        source = MOD_SOURCE_INT;
        sprintf( cmd, "%s:INT:FREQ?\n", types[ type ] );
        rs_smb100a_talk( cmd, buffer, &length );
        buffer[ length - 1 ] = '\0';
        *freq = T_atod( buffer );
    }
    else if ( ! strncmp( buffer, "EXT", 3 ) )
    {
        sprintf( cmd, "%s:EXT:COUP?\n", types[ type ] );
        rs_smb100a_talk( cmd, buffer, &length );
        source = buffer[ 0 ] == 'A' ? MOD_SOURCE_AC : MOD_SOURCE_DC;
    }
    else
    {
        print( FATAL, "Device returned unexpected data.\n" );
        THROW( EXCEPTION );
    }

    return source;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_set_mod_ampl( int    type,
                         double ampl )
{
    char cmd[ 100 ];


    fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

    switch ( rs_smb100a.mod_type )
    {
        case MOD_TYPE_FM :
            sprintf( cmd, "FM %fHz\n", ampl );
            break;

        case MOD_TYPE_AM :
            sprintf( cmd, "AM %fPCT\n", ampl );
            break;

        case MOD_TYPE_PM :
            sprintf( cmd, "PM %fRAD\n", ampl );
            break;

        default :                         /* this can never happen... */
            fsc2_impossible( );
    }

    rs_smb100a_command( cmd );

    return ampl;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

double
rs_smb100a_get_mod_ampl( int type )
{
    const char *cmds[ ] = { "FM?\n", "AM?\n", "PM?\n" };
    char buffer[ 100 ];
    size_t length = sizeof buffer;


    fsc2_assert( type >= 0 && type < NUM_MOD_TYPES );

    rs_smb100a_talk( cmds[ type ], buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

#if defined WITH_PULSE_MODULATION

void
rs_smb100a_set_pulse_state( bool state )
{
    char cmd[ 100 ] = "PULM:STAT ";


    strcat( cmd, state ? "ON\n" : "OFF\n" );
    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static bool
rs_smb100a_get_pulse_state( void )
{
    char buffer[ 20 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "PULM:STAT?\n", buffer, &length );
    return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_set_double_pulse_mode( bool state )
{
    char cmd[ 100 ] = "PULS:DOUB:STAT ";


    strcat( cmd, state ? "ON\n" : "OFF\n" );
    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_set_pulse_trig_slope( bool state )
{
    char cmd[ 100 ] = ":TRIG:PULS:SLOP ";


    if ( state == SLOPE_RAISE )
        strcat( cmd, "POS\n" );
    else
        strcat( cmd, "NEG\n" );

    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static bool
rs_smb100a_get_pulse_trig_slope( void )
{
    char buffer[ 20 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( ":TRIG:PULS:SLOP?\n", buffer, &length );
    return buffer[ 0 ] == 'P' ? SLOPE_RAISE : SLOPE_FALL;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_set_pulse_width( double width )
{
    char cmd[ 100 ];


    sprintf( cmd, "PULS:WIDT %s\n", rs_smb100a_pretty_print( width ) );
    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static double
rs_smb100a_get_pulse_width( void )
{
    char buffer[ 100 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "PULS:WIDT?\n", buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_set_pulse_delay( double delay )
{
    char cmd[ 100 ];


    sprintf( cmd, "PULS:DEL %s\n", rs_smb100a_pretty_print( delay ) );
    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static double
rs_smb100a_get_pulse_delay( void )
{
    char buffer[ 100 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "PULS:DEL?\n", buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_smb100a_set_double_pulse_delay( double delay )
{
    char cmd[ 100 ];


    sprintf( cmd, "PULS:DOUB:DEL %s\n", rs_smb100a_pretty_print( delay ) );
    rs_smb100a_command( cmd );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static double
rs_smb100a_get_double_pulse_delay( void )
{
    char buffer[ 100 ];
    size_t length = sizeof buffer;


    rs_smb100a_talk( "PULS:DOUB:DEL?\n", buffer, &length );
    buffer[ length - 1 ] = '\0';
    return T_atod( buffer );
}


#endif /* WITH_PULSE_MODULATION */


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
rs_smb100a_comm_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
rs_smb100a_command( const char * cmd )
{
	size_t len = strlen( cmd );


	if ( vxi11_write( cmd, &len, false ) != SUCCESS )
		rs_smb100a_comm_failure( );

//	fsc2_usleep( 4000, false );

	return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
rs_smb100a_talk( const char * cmd,
                 char *       reply,
                 size_t *     length )
{
	size_t len = strlen( cmd );

	if ( vxi11_write( cmd, &len, false ) != SUCCESS )
		rs_smb100a_comm_failure( );

	fsc2_usleep( 4000, false );

	if ( vxi11_read( reply, length, false ) != SUCCESS )
		rs_smb100a_comm_failure( );

	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
