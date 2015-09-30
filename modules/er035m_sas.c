/*
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


#include "fsc2_module.h"
#include "serial.h"


/* Include configuration information for the device */

#include "er035m_sas.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0    /* returned as current field in test run */
#define ER035M_TEST_RES   0.001

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* exported functions and symbols */

int er035m_sas_init_hook(       void );
int er035m_sas_test_hook(       void );
int er035m_sas_exp_hook(        void );
int er035m_sas_end_of_exp_hook( void );

Var_T * gaussmeter_name( Var_T * v );
Var_T * gaussmeter_field( Var_T * v );
Var_T * gaussmeter_keep_going_on_field_error( Var_T * v );
Var_T * gaussmeter_resolution( Var_T * v );
Var_T * gaussmeter_probe_orientation( Var_T * v );
Var_T * measure_field( Var_T * v );
Var_T * gaussmeter_command( Var_T * v );
Var_T * gaussmeter_upper_search_limit( Var_T * v );
Var_T * gaussmeter_lower_search_limit( Var_T * v );


/* Internally used functions */

static double er035m_sas_get_field( void );
static int er035m_sas_get_resolution( void );
static void er035m_sas_set_resolution( int res_index );
static long er035m_sas_get_upper_search_limit( void );
static long er035m_sas_get_lower_search_limit( void );
static void er035m_sas_set_upper_search_limit( long ul );
static void er035m_sas_set_lower_search_limit( long ll );
static bool er035m_sas_open( void );
static bool er035m_sas_close( void );
static void er035m_sas_talk( const char * cmd,
							 char       * buffer,
							 size_t     * length );
static void er035m_sas_command( const char * buf );
static bool er035m_sas_write( const char * cmd );
static bool er035m_sas_read( char *   buf,
                             size_t * len );
static bool er035m_sas_comm( int type,
                             ... );
static void er035m_sas_comm_fail( void );


/* Structure for the device's state */

static struct {
    int              state;      /* current state of the gaussmeter */
    double           field;      /* last measured field */
    int              resolution; /* set to either RES_VERY_LOW = 0.1 G,
                                    RES_LOW = 0.01 G or RES_HIGH = 0.001 G */
    int              sn;
    struct termios * tio;        /* serial port terminal interface structure */
    char             prompt;     /* prompt character send on each reply */
    int              probe_orientation;
    int              probe_type;
    long             upper_search_limit;
    long             lower_search_limit;

    bool keep_going_on_bad_field;
} nmr, nmr_stored;

static const char * er035m_sas_eol = "\r\n";

static double res_list[ 3 ] = { 0.1, 0.01, 0.001 };

enum {
    UNDEF_RES = -1,
    LOW_RES,
    MEDIUM_RES,
    HIGH_RES
};


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define ER035M_SAS_WAIT 200000    /* this means 200 ms for fsc2_usleep() */


/* If the field is too unstable the gausmeter might never get to the state
   where the field value is valid with the requested resolution even though
   the lock state is achieved. 'ER035M_SAS_MAX_RETRIES' determines the
   maximum number of retries before we give up. With a value of 100 and the
   current setting for 'ER035M_SAS_WAIT' of 200 ms it will take at least
   20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_SAS_MAX_RETRIES 100
#define FAIL_RETRIES           5

#define PROBE_TYPE_F0 0          /* S-band probe */
#define PROBE_TYPE_F1 1          /* X-band probe */


static long lower_search_limits[ 2 ] = {  450,  1450 };
static long upper_search_limits[ 2 ] = { 2400, 20000 };


enum {
       ER035M_SAS_UNKNOWN = -1,
       ER035M_SAS_LOCKED = 0,
       ER035M_SAS_SU_ACTIVE,
       ER035M_SAS_SD_ACTIVE,
       ER035M_SAS_OU_ACTIVE,
       ER035M_SAS_OD_ACTIVE,
       ER035M_SAS_SEARCH_AT_LIMIT
};


enum {
       SERIAL_INIT,
       SERIAL_READ,
       SERIAL_WRITE,
       SERIAL_EXIT
};


/**************************************************/
/*                                                */
/*                hook functions                  */
/*                                                */
/**************************************************/


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sas_init_hook( void )
{
    /* Claim the serial port (throws exception on failure) */

    nmr.sn = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );
	nmr.tio = NULL;

    nmr.state      = ER035M_SAS_UNKNOWN;
    nmr.resolution = UNDEF_RES;
    nmr.prompt     = '\0';

    nmr.keep_going_on_bad_field = false;

    return 1;
}

/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sas_test_hook( void )
{
    nmr_stored = nmr;
    nmr.lower_search_limit = lower_search_limits[ PROBE_TYPE_F0 ];
    nmr.upper_search_limit = upper_search_limits[ PROBE_TYPE_F1 ];
    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sas_exp_hook( void )
{
    nmr = nmr_stored;

    nmr.keep_going_on_bad_field = false;

	/* Open connection to device and switch it to remore mode */

    if ( ! er035m_sas_open( ) )
    {
        print( FATAL, "Failed to open serial port connecttion to "
			   "gaussmeter.\n" );
        THROW( EXCEPTION );
    }

    /* Switch the display on */

    er035m_sas_command( "ED" );

    /* Find out the current resolution, and if necessary, change it to the
       value requested by the user during preparation phase */

    int cur_res = er035m_sas_get_resolution( );

    if ( nmr.resolution == UNDEF_RES )
        nmr.resolution = cur_res;
    else if ( nmr.resolution != cur_res )
        er035m_sas_set_resolution( nmr.resolution );

    /* Ask gaussmeter to send its status byte and test if it does - sometimes
	   the fucking thing does not answer (i.e. it just seems to send the
	   prompt character and nothing else) so in this case we give it
	   another chance (or even two, see FAIL_RETRIES above) */

    nmr.state = ER035M_SAS_UNKNOWN;

    char buffer[ 50 ];
	size_t length;

 try_again:

	length = sizeof buffer;
	er035m_sas_talk( "PS", buffer, &length );

    /* Get full state of device and check that the status byte says that it's
	   OK, where OK means that modulation is on and the gaussmeter is either
	   in locked state or is actively searching to achieve the lock (if it's
	   just in TRANS L-H or H-L state check again) */

	for ( char * bp = buffer; *bp; bp++ )
        switch ( *bp )
        {
            case '0' :     /* F0 (S-band) probe is connected */
nmr.probe_type = PROBE_TYPE_F1; /* this is a workaround for a device error!!! */
                break;

            case '1' :     /* F1 (X-band) probe is connected */
                nmr.probe_type = PROBE_TYPE_F1;
                break;

            case '2' :      /* No probe connected -> error */
                print( FATAL, "No field probe connected.\n" );
                THROW( EXCEPTION );

            case '3' :      /* Error temperature -> error */
                print( FATAL, "Temperature error.\n" );
                THROW( EXCEPTION );

            case '4' :      /* TRANS L-H or H-L -> test again */
            case '5' :
                fsc2_usleep( 500000, SET );
                goto try_again;

            case '6' :      /* MOD OFF -> error (should never happen) */
                print( FATAL, "Modulation is switched off.\n" );
                THROW( EXCEPTION );

            case '7' :      /* MOD POS -> OK */
                nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
                break;

            case '8' :      /* MOD NEG -> OK */
                nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
                break;

            case '9' :      /* System in lock -> very good... */
                nmr.state = ER035M_SAS_LOCKED;
                break;

            case 'A' :      /* FIELD ? -> error */
                print( FATAL, "Gaussmeter can't find the field "
                       "(\"FIELD?\").\n" );
                THROW( EXCEPTION );

            case 'B' :      /* SU active -> OK */
                nmr.state = ER035M_SAS_SU_ACTIVE;
                break;

            case 'C' :      /* SD active -> OK */
                nmr.state = ER035M_SAS_SD_ACTIVE;
                break;

            case 'D' :      /* OU active -> OK */
                nmr.state = ER035M_SAS_OU_ACTIVE;
                break;

            case 'E' :      /* OD active -> OK */
                nmr.state = ER035M_SAS_OD_ACTIVE;
                break;

            case 'F' :      /* Search active but just at search limit -> OK */
                nmr.state = ER035M_SAS_SEARCH_AT_LIMIT;
                break;

            default :
                print( FATAL, "Undocumented data received.\n" );
                THROW( EXCEPTION );
        }

    /* If the gaussmeter is already locked just get the field value, other-
       wise first try to achieve locked state */

    if ( nmr.state == ER035M_SAS_LOCKED )
        nmr.field = er035m_sas_get_field( );
	else
    {
        Var_T * v = measure_field( NULL );
        nmr.field = v->val.dval;
        vars_pop( v );
    }

    /* Find out the current settings of the search limits */

    nmr.upper_search_limit = er035m_sas_get_upper_search_limit( );
    nmr.lower_search_limit = er035m_sas_get_lower_search_limit( );

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sas_end_of_exp_hook( void )
{
	if ( nmr.tio )
		er035m_sas_close( );
    return 1;
}


/************************************************/
/*                                              */
/*              exported functions              */
/*                                              */
/************************************************/


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
gaussmeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 * When called the module won't abort the EDL script if the
 * gauss-meter loses the lock and can't reestablish it when
 * trying to measure the field. Instead gaussmeter_field()
 * will then stop attempting to measure the field and return
 * -1.
 *----------------------------------------------------------------*/

Var_T *
gaussmeter_keep_going_on_field_error( Var_T * v  UNUSED_ARG )
{
    nmr.keep_going_on_bad_field = true;
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
gaussmeter_field( Var_T * v )
{
    return measure_field( v );
}


/*----------------------------------------------------------------*
 * find_field() tries to get the gaussmeter into the locked state
 * and returns the current field value in a variable.
 *----------------------------------------------------------------*/

Var_T *
measure_field( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, ER035M_TEST_FIELD );

    /* If gaussmeter is in oscillator up/down state or the state is unknown
       (i.e. it's standing somewhere and not searching at all) search for the
       field. Starting with searching down is just as probable the wrong
       decision as searching up... */

    if (    nmr.state == ER035M_SAS_OU_ACTIVE
		 || nmr.state == ER035M_SAS_OD_ACTIVE
         || nmr.state == ER035M_SAS_UNKNOWN )
        er035m_sas_command( "SD" );

    /* Wait for gaussmeter to go into lock state (or FAIL) */

    while ( nmr.state != ER035M_SAS_LOCKED )
    {
        /* All this can take a long time, allow user to abort */

        stop_on_user_request( );

        /* Get status byte and check if lock was achieved - sometimes the
           device doesn't seem to answer (i.e. it just seems to send the
           prompt character and nothing else). In this case give it another
		   chance (or even two, see FAIL_RETRIES above). */

		char buffer[ 30 ];
		size_t length;
		long retries = FAIL_RETRIES;

		do
		{
			length = sizeof buffer;
			er035m_sas_talk( "PS", buffer, &length );
		} while ( length == 0 && --retries > 0 );

		if ( retries <= 0 )
			er035m_sas_comm_fail( );

        for ( char * bp = buffer; *bp; bp++ )
            switch ( *bp++ )
            {
                case '0' : case '1' :  // probe indicator data -> OK
                    break;

                case '2' :             // no probe -> error
                    print( FATAL, "No field probe connected to the NMR "
                           "gaussmeter.\n" );
                    THROW( EXCEPTION );

                case '4' : case '5' :  // TRANS L-H or H-L -> test again
                    break;

                case '7' : case '8' :  // MOD POS or NEG -> just go on
                    break;

                case '9' :             // finally locked -> very good...
                    nmr.state = ER035M_SAS_LOCKED;
                    break;

                case 'A' :             // FIELD ? -> error
                    if ( nmr.keep_going_on_bad_field )
                    {
                        print( SEVERE, "Gaussmeter not able to lock onto "
                               "field.\n" );
                        return vars_push( FLOAT_VAR, -1.0 );
                    }

                    print( FATAL, "Gaussmeter has an unidentifiable "
                           "problem.\n" );
                    THROW( EXCEPTION );

                case 'B' :             // SU active -> OK
                    nmr.state = ER035M_SAS_SU_ACTIVE;
                    break;

                case 'C' :             // SD active
                    nmr.state = ER035M_SAS_SD_ACTIVE;
                    break;

                case 'D' :           // OU active -> error (should never happen)
                    nmr.state = ER035M_SAS_OU_ACTIVE;
                    print( FATAL, "Gaussmeter has an unidentifiable "
                           "problem.\n" );
                    THROW( EXCEPTION );

                case 'E' :           // OD active -> error (should never happen)
                    nmr.state = ER035M_SAS_OD_ACTIVE;
                    print( FATAL, "Gaussmeter has an unidentifiable "
                           "problem.\n" );
                    THROW( EXCEPTION );

                case 'F' :         // Search active but at a search limit -> OK
                    nmr.state = ER035M_SAS_SEARCH_AT_LIMIT;
                    break;

                default :
                    print( FATAL, "Undocumented data received from the NMR "
                           "gaussmeter.\n" );
                    THROW( EXCEPTION );
            }
    }

    /* Finally  get current field value */

    return vars_push( FLOAT_VAR, er035m_sas_get_field( ) );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_resolution( Var_T * v )
{
    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && nmr.resolution == UNDEF_RES )
            no_query_possible( );

        if ( FSC2_MODE == TEST && nmr.resolution == UNDEF_RES )
            return vars_push( FLOAT_VAR, ER035M_TEST_RES );

        return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
    }

    double res = get_double( v, "resolution" );

    too_many_arguments( v );

    if ( res <= 0 )
    {
        print( FATAL, "Invalid resolution of %.0f mG.\n", 1.0e3 * res );
        THROW( EXCEPTION );
    }

    /* Try to match the requested resolution, if necessary use the nearest
       possible value */

    int res_index = UNDEF_RES;
    for ( int i = 0; i < 2; i++ )
        if ( res <= res_list[ i ] && res >= res_list[ i + 1 ] )
        {
            res_index = i +
                 ( ( res / res_list[ i ] > res_list[ i + 1 ] / res ) ? 0 : 1 );
            break;
        }

    if ( res_index == UNDEF_RES )
    {
        if ( res >= res_list[ LOW_RES ] )
            res_index = LOW_RES;
        if ( res <= res_list[ HIGH_RES ] )
            res_index = HIGH_RES;
    }

    if ( fabs( res_list[ res_index ] - res ) > 1.0e-2 * res_list[ res_index ] )
        print( WARN, "Can't set resolution to %.0f mG, using %.0f mG "
               "instead.\n", 1.0e3 * res, 1.0e3 * res_list[ res_index ] );

    fsc2_assert( res_index >= LOW_RES && res_index <= HIGH_RES );

    nmr.resolution = res_index;

    if ( FSC2_MODE == EXPERIMENT )
        er035m_sas_set_resolution( nmr.resolution );

    return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
}


/*--------------------------------------------------------*
 * While the manual claims something different neither setting nor querying
 * the modulation (and thereby the probe orientation) using the "MO" command
 * does really seem to work. The only way to figure out the modulation in
 * a reliable way seems to be to look at the status information using "PS".
 *--------------------------------------------------------*/

Var_T *
gaussmeter_probe_orientation( Var_T * v )
{
	too_many_arguments( v );

	if ( FSC2_MODE == PREPARATION )
		no_query_possible( );

	if ( FSC2_MODE == TEST )
		return vars_push( INT_VAR, 1L );

	return vars_push( INT_VAR, ( long ) nmr.probe_orientation );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
gaussmeter_command( Var_T * v )
{
    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
		char * volatile cmd = NULL;

        TRY
        {
			cmd = T_strdup( v->val.sptr );
			translate_escape_sequences( cmd );
			er035m_sas_command( cmd );
			cmd = T_free( cmd );
			TRY_SUCCESS;
		}
        OTHERWISE
        {
            cmd = T_free( cmd );
            RETHROW;
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_upper_search_limit( Var_T * v )
{
    if ( v == NULL )
        return vars_push( FLOAT_VAR, ( double ) nmr.upper_search_limit );

    long ul = lrnd( ceil( get_double( v, "upper search limit" ) ) );

    if ( ul > upper_search_limits[ FSC2_MODE == TEST ?
                                   PROBE_TYPE_F1 : nmr.probe_type ] )
    {
        print( SEVERE, "Requested upper search limit too high, changing to "
               "%ld G.\n",
               upper_search_limits[ FSC2_MODE == TEST ?
                                    PROBE_TYPE_F1 : nmr.probe_type ] );
        ul = upper_search_limits[ FSC2_MODE == TEST ?
                                  PROBE_TYPE_F1 : nmr.probe_type ];
    }

    if ( ul <= nmr.lower_search_limit )
    {
        print( FATAL, "Requested upper search limit isn't higher than the "
               "currently active lower search limit.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        er035m_sas_set_upper_search_limit( ul );
    nmr.upper_search_limit = ul;

    return vars_push( FLOAT_VAR, ( double ) ul );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_lower_search_limit( Var_T * v )
{
    if ( v == NULL )
        return vars_push( FLOAT_VAR, ( double ) nmr.lower_search_limit );

    long ll = lrnd( floor( get_double( v, "lower search limit" ) ) );

    if ( ll < lower_search_limits[ FSC2_MODE == TEST ?
                                   PROBE_TYPE_F0 : nmr.probe_type ] )
    {
        print( SEVERE, "Requested lower search limit too low, changing to "
               "%ld G.\n",
               lower_search_limits[ FSC2_MODE == TEST ?
                                    PROBE_TYPE_F0 : nmr.probe_type ] );
        ll = lower_search_limits[ FSC2_MODE == TEST ?
                                  PROBE_TYPE_F0 : nmr.probe_type ];
    }

    if ( ll >= nmr.upper_search_limit )
    {
        print( FATAL, "Requested lower search limit isn't lower than the "
               "currently active upper search limit.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        er035m_sas_set_lower_search_limit( ll );
    nmr.lower_search_limit = ll;

    return vars_push( FLOAT_VAR, ( double ) ll );
}


/****************************************************/
/*                                                  */
/*            internally used functions             */
/*                                                  */
/****************************************************/


/*-------------------------------------------------------------------------*
 * er035m_sas_get_field() returns the field value from the gaussmeter - it
 * will return the settled value but will report a failure if gaussmeter
 * isn't in lock state. Another reason for a failure is a field that is
 * too unstable to achieve the requested resolution even though the
 * gaussmeter is already in lock state.
 * Take care: If the gaussmeter isn't already in the lock state call
 *            the function find_field() instead.
 *-----------------------------------------------------------------------*/

static
double
er035m_sas_get_field( void )
{
    /* Repeat asking for the field value until it's correct up to the LSD -
       it will give up after 'ER035M_SAS_MAX_RETRIES' retries to avoid
       getting into an infinite loop when the field is too unstable */

    long tries = ER035M_SAS_MAX_RETRIES;
    char *state_flag;
	char buffer[ 50 ];

    do
    {
        /* Ask gaussmeter to send the current field and state - sometimes
           the fucking thing does not answer (i.e. it just seems to send the
           prompt character and nothing else) so in this case we give it
           another chance (or even more, see FAIL_RETRIES above) */

		size_t length = sizeof buffer;
		er035m_sas_talk( "PF", buffer, &length );

        /* Check flag showing the state */

        state_flag = strrchr( buffer, ',' );

        if ( ! state_flag )
        {
            print( FATAL, "Undocumented data received.\n" );
            THROW( EXCEPTION );
        }

        /* Report error if gaussmeter isn't in lock state */

        if ( *++state_flag >= '3' )
        {
            if ( nmr.keep_going_on_bad_field )
            {
                print( SEVERE, "Gaussmeter not able to lock onto field.\n" );
                return -1.0;
            }

            print( FATAL, "Gaussmeter can't get lock on the field.\n" );
            THROW( EXCEPTION );
        }
    } while ( *state_flag != '0' && tries-- > 0 );

    /* If the maximum number of retries was exceeded we've got to give up */

    if ( tries < 0 )
    {
        if ( nmr.keep_going_on_bad_field )
        {
            print( SEVERE, "Field is too unstable to be measured with the "
                   "requested resolution of %0f mG.\n",
                   1.0e3 * res_list[ nmr.resolution ] );
            return nmr.field = -1.0;
        }

        print( FATAL, "Field is too unstable to be measured with the requested "
               "resolution of %.0f mG.\n", 1.0e3 * res_list[ nmr.resolution ] );
        THROW( EXCEPTION );
    }

    /* Finally interpret the field value string */

    *( state_flag - 1 ) = '\0';

    char *bp = buffer;
    int sign = 1;
    while ( *bp && ! isdigit( ( int ) *bp ) )
        if ( * bp++ == '-' )
            sign = -1;

    if ( ! *bp || sscanf( bp, "%lf", &nmr.field ) != 1 )
    {
        print( FATAL, "Undocumented data received.\n" );
        THROW( EXCEPTION );
    }

    return nmr.field *= sign;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static
int
er035m_sas_get_resolution( void )
{
    char buffer[ 20 ];
    size_t length = sizeof buffer;

	er035m_sas_talk( "RS", buffer, &length );

    if ( length == 1 )
        switch ( *buffer )
        {
            case '1' :
                return LOW_RES;

            case '2' :
                return MEDIUM_RES;

            case '3' :
                return HIGH_RES;
        }

    print( FATAL, "Undocumented data received.\n" );
    THROW( EXCEPTION );

    return UNDEF_RES;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static
void
er035m_sas_set_resolution( int res_index )
{
    char cmd[ 4 ];

    sprintf( cmd, "RS%1d", res_index + 1 );
	er035m_sas_command( cmd );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
long
er035m_sas_get_upper_search_limit( void )
{
    char buffer[ 20 ];
    size_t length = sizeof buffer;

	er035m_sas_talk( "UL", buffer, &length );

	long ul;
	if ( sscanf( buffer, "%ld", &ul ) != 1 )
	{
		print( FATAL, "Undocumented data received.\n" );
		THROW( EXCEPTION );
	}

    return ul;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
long
er035m_sas_get_lower_search_limit( void )
{
    char buffer[ 20 ];
    size_t length = sizeof buffer;

	er035m_sas_talk( "LL", buffer, &length );

	long ll;
	if ( sscanf( buffer, "%ld", &ll ) != 1 )
	{
		print( FATAL, "Undocumented data received.\n" );
		THROW( EXCEPTION );
	}

	return ll;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sas_set_upper_search_limit( long ul )
{
    char cmd[ 40 ];

    snprintf( cmd, 40, "UL%ld", ul );
	er035m_sas_command( cmd );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sas_set_lower_search_limit( long ll )
{
    char cmd[ 40 ];

    snprintf( cmd, 40, "LL%ld", ll );
	er035m_sas_command( cmd );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
bool
er035m_sas_open( void )
{
    return er035m_sas_comm( SERIAL_INIT );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
bool
er035m_sas_close( void )
{
    return er035m_sas_comm( SERIAL_EXIT );
}



/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
void
er035m_sas_talk( const char * cmd,
				 char       * buffer,
				 size_t     * length )
{
    if ( cmd == NULL || *cmd == '\0' )
	{
		print( FATAL, "Internal error: empty command.\n" );
		THROW( EXCEPTION );
	}

	long retries = FAIL_RETRIES;
    while ( retries-- > 0 )
    {
        stop_on_user_request( );

		size_t len = *length;
		if (    er035m_sas_write( cmd ) != OK
			 || er035m_sas_read( buffer, &len ) != OK )
			er035m_sas_comm_fail( );

        if ( len > 0 )
		{
			*length = len;
			break;
		}
    }

	if ( retries <= 0 )
		er035m_sas_comm_fail( );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
void
er035m_sas_command( const char * cmd )
{
    if ( cmd == NULL || *cmd == '\0' )
        return;

	if ( er035m_sas_write( cmd ) != OK )
		er035m_sas_comm_fail( );

	/* Read in prompt char sent as reply */

	char rd_buf[ 2 ];
	size_t length = sizeof rd_buf;
	if ( er035m_sas_read( rd_buf, &length ) != OK )
		er035m_sas_comm_fail( );
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
bool
er035m_sas_write( const char * buf )
{
    static char *wrbuf = NULL;
    static long wrlen = 0;
    bool res;

    if ( buf == NULL || *buf == '\0' )
        return OK;
    wrlen = strlen( buf );

    if ( er035m_sas_eol != NULL && strlen( er035m_sas_eol ) > 0 )
    {
        wrlen += strlen( er035m_sas_eol );

		wrbuf = T_malloc( wrlen + 1 );

        strcpy( wrbuf, buf );
        strcat( wrbuf, er035m_sas_eol );

        res = er035m_sas_comm( SERIAL_WRITE, wrbuf );
        T_free( wrbuf );
    }
    else
        res = er035m_sas_comm( SERIAL_WRITE, buf );

	return res;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
bool
er035m_sas_read( char *   buf,
                 size_t * len )
{
    if ( ! buf || ! *len )
        return OK;

    *len -= 1;
    if ( ! er035m_sas_comm( SERIAL_READ, buf, len ) )
        return FAIL;

    /* If the prompt character sent by the device with each reply isn't
	   known yet take it to be the very first byte read (default is '*'
	   but who knows if this got changed by some unlucky coincidence...) */

    if ( nmr.prompt == '\0' )
        nmr.prompt = buf[ 0 ];

    /* Make buffer end with '\0' */

    buf[ *len ] = '\0';

	// Remove anything that looks like an end-of-line characters

    char * ptr;
    while (    ( ptr = strchr( buf, '\r' ) )
            || ( ptr = strchr( buf, '\n' ) ) )
    {
        *ptr = '\0';
        *len = ptr - buf;
    }

    /* Remove leading prompt characters if there are any */

    for ( ptr = buf; *len > 0 && *ptr == nmr.prompt; ptr++, *len -= 1 )
        /* empty */ ;

	if ( ptr != buf )
		memmove( buf, ptr, *len + 1 );

	/* Remove trailing prompt chars */

	while ( *len > 0 && buf[ *len - 1 ] == nmr.prompt )
		buf[ *--len ] = '\0';

    return OK;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
bool
er035m_sas_comm( int type,
                 ... )
{
    va_list ap;
    char *buf;
    ssize_t len;
    size_t *lptr;

    switch ( type )
    {
        case SERIAL_INIT :
            /* Open the serial port for reading and writing. */

            if ( ( nmr.tio = fsc2_serial_open( nmr.sn, O_RDWR ) ) == NULL )
                return FAIL;

            /* Use 8-N-1, allow flow control, ignore modem lines, enable
               reading and set the baud rate. */

            nmr.tio->c_cflag &= ~ ( PARENB | CSTOPB | CSIZE );

            nmr.tio->c_cflag |= CS8 | CLOCAL | CREAD;
            cfsetispeed( nmr.tio, SERIAL_BAUDRATE );
            cfsetospeed( nmr.tio, SERIAL_BAUDRATE );

            nmr.tio->c_iflag = IGNBRK;
            nmr.tio->c_oflag = 0;
            nmr.tio->c_lflag = 0;
            fsc2_tcflush( nmr.sn, TCIOFLUSH );
            fsc2_tcsetattr( nmr.sn, TCSANOW, nmr.tio );

			/* Switch to remote mode (makes also sure a command with no
			   reply except for the promptcharacter is sent first, so we
			   can safely catch that character) */

			fsc2_usleep( ER035M_SAS_WAIT, UNSET );
			er035m_sas_command( "REM" );

            break;

        case SERIAL_EXIT :
            er035m_sas_write( "LOC" );
            fsc2_serial_close( nmr.sn );
            break;

        case SERIAL_WRITE :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            va_end( ap );

            len = strlen( buf );
            if ( fsc2_serial_write( nmr.sn, buf, len, 0, SET ) != len )
            {
                if ( len == 0 )
                    stop_on_user_request( );
                return FAIL;
            }

            /* The device gets out of sync when we're not waiting after each
               write... */

            fsc2_usleep( ER035M_SAS_WAIT, UNSET );
            break;

        case SERIAL_READ :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            lptr = va_arg( ap, size_t * );
            va_end( ap );

            /* Try to read from the gaussmeter, give it up to 2 seconds time
               to respond */

            if ( ( len = fsc2_serial_read( nmr.sn, buf, *lptr,
										   nmr.prompt ? &nmr.prompt : NULL,
										   ( nmr.prompt ? 10 : 1 )
										   * ER035M_SAS_WAIT, UNSET ) ) <= 0 )
            {
                if ( len == 0 )
                    stop_on_user_request( );

                *lptr = 0;
                return FAIL;
            }

            /* The most significant bit of each byte the gaussmeter sends is
               irrelevant and may reflect the parity setting at the device,
               get rid of it... */

            *lptr = len;
            for ( len = 0; len < ( ssize_t ) *lptr; len++ )
                buf[ len ] &= 0x7f;
            break;

        default :
            print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }

    return OK;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
void
er035m_sas_comm_fail( void )
{
    print( FATAL, "Can't access the gaussmeter.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 * tab-width: 4
 * indent-tabs-mode: nil
 */
