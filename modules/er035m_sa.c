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
#include "gpib.h"


/* Include configuration information for the device */

#include "er035m_sa.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0   /* returned as current fireld in test run */
#define ER035M_TEST_RES   0.001

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* Since the device mis-behaves quite often and field measurements aren't
   essential in all cases we allow to control what happens if the device
   does something unexpected or even staps communicationg:

   a) abort the progranm (default)
   b) continue, but never again try to talk to device
   c) continue, and on further requests make another attempt to talk to it

   This can be set via the gaussmeter_keep_going_on_error() function.
*/

#define STOP_ON_BUG      0
#define CONTINUE_ON_BUG  1
#define RETRY_ON_BUG     2


/* exported functions and symbols */

int er035m_sa_init_hook(       void );
int er035m_sa_test_hook(       void );
int er035m_sa_exp_hook(        void );
int er035m_sa_end_of_exp_hook( void );

Var_T * gaussmeter_name( Var_T * v );
Var_T * gaussmeter_field( Var_T * v );
Var_T * gaussmeter_keep_going_on_field_error( Var_T * v );
Var_T * gaussmeter_keep_going_on_error( Var_T * v );
Var_T * measure_field( Var_T * v );
Var_T * gaussmeter_resolution( Var_T * v );
Var_T * gaussmeter_probe_orientation( Var_T * v );
Var_T * gaussmeter_command( Var_T * v );
Var_T * gaussmeter_upper_search_limit( Var_T * v );
Var_T * gaussmeter_lower_search_limit( Var_T * v );


/* internally used functions */

static double er035m_sa_get_field( void );
static int er035m_sa_get_resolution( void );
static void er035m_sa_set_resolution( int res_index );
static long er035m_sa_get_upper_search_limit( void );
static long er035m_sa_get_lower_search_limit( void );
static void er035m_sa_set_upper_search_limit( long ul );
static void er035m_sa_set_lower_search_limit( long ll );
static void er035m_sa_command( const char * cmd );
static void er035m_sa_talk( const char * cmd,
                            char *       reply,
                            long *       length );
static void er035m_sa_failure( void );


struct NMR {
    int state;
    int device;
    const char *name;
    double field;
    int resolution;
    int probe_orientation;
    int probe_type;
    long upper_search_limit;
    long lower_search_limit;

    bool keep_going_on_bad_field;

	int              bug_behaviour;
	bool             device_is_dead;
};


static struct NMR nmr, nmr_stored;
static double res_list[ 3 ] = { 0.1, 0.01, 0.001 };

enum {
    UNDEF_RES = -1,
    LOW_RES,
    MEDIUM_RES,
    HIGH_RES
};


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US          100000    /* 100 ms, used in calls of usleep() */
#define ER035M_SA_WAIT 200000    /* this means 200 ms for usleep() */

/* If the field is unstable the gausmeter might never get to the state where
   the field value is valid with the requested resolution even though the lock
   state is achieved. 'ER035M_SA_MAX_RETRIES' tells how many times we retry in
   this case. With a value of 100 and the current setting of 'ER035M_SA_WAIT'
   of 200 ms it will take at least 20 s before this will happen.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_SA_MAX_RETRIES 100


#define PROBE_TYPE_F0 0          /* S-band probe */
#define PROBE_TYPE_F1 1          /* X-band probe */


long lower_search_limits[ 2 ] = {  450,  1450 };
long upper_search_limits[ 2 ] = { 2400, 20000 };


enum {
       ER035M_SA_UNKNOWN = -1,
       ER035M_SA_LOCKED = 0,
       ER035M_SA_SU_ACTIVE,
       ER035M_SA_SD_ACTIVE,
       ER035M_SA_OU_ACTIVE,
       ER035M_SA_OD_ACTIVE,
       ER035M_SA_SEARCH_AT_LIMIT
};



/***************************************************/
/*                                                 */
/*                  hook functions                 */
/*                                                 */
/***************************************************/


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sa_init_hook( void )
{
    Need_GPIB = true;
    nmr.name = DEVICE_NAME;

    nmr.state = ER035M_SA_UNKNOWN;
    nmr.resolution = UNDEF_RES;
    nmr.device = -1;

    nmr.keep_going_on_bad_field = false;

	nmr.bug_behaviour = STOP_ON_BUG;
	nmr.device_is_dead = false;

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sa_test_hook( void )
{
    nmr_stored = nmr;
    nmr.lower_search_limit = lower_search_limits[ PROBE_TYPE_F0 ];
    nmr.upper_search_limit = upper_search_limits[ PROBE_TYPE_F1 ];
    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sa_exp_hook( void )
{
    nmr = nmr_stored;

    fsc2_assert( nmr.device < 0 );

    nmr.keep_going_on_bad_field = false;

	nmr.bug_behaviour = STOP_ON_BUG;
	nmr.device_is_dead = false;

    if ( gpib_init_device( nmr.name, &nmr.device ) == FAILURE )
    {
        nmr.device = -1;
        er035m_sa_failure( );
    }

    fsc2_usleep( ER035M_SA_WAIT, false );

    /* Send a "Selected Device Clear" - otherwise the gaussmeter does not
       work ... */

    gpib_clear_device( nmr.device );
    fsc2_usleep( ER035M_SA_WAIT, false );

    /* Switch display on */

    er035m_sa_command( "ED\r"  );

    /* Find out the curent resolution, and if necessary, change it to the
       value requested by the user */

    int cur_res = er035m_sa_get_resolution( );

    if ( nmr.resolution == UNDEF_RES )
        nmr.resolution = cur_res;
    else if ( nmr.resolution != cur_res )
        er035m_sa_set_resolution( nmr.resolution );

    /* Ask gaussmeter to send status byte and test if it does */

    nmr.state = ER035M_SA_UNKNOWN;

 try_again:

    stop_on_user_request( );

    char buffer[ 30 ];
    long length = sizeof buffer;
    er035m_sa_talk( "PS\r", buffer, &length );

    if ( length < 3 )
    {
        print( FATAL, "Undocumented data received.\n" );
        THROW( EXCEPTION );
    }

    /* Now check that the status byte says that the device is OK, where OK
       means that modulation is on and the gaussmeter is either in locked
       state or is actively searching to achieve the lock (if it's just in
       TRANS L-H or H-L state check again) */

    for ( char * bp = buffer + 2; *bp; bp++ )
        switch ( *bp )
        {
            case '0' :      /* Probe F0 (S-band) is connected */
                nmr.probe_type = PROBE_TYPE_F0;
                break;

            case '1' :      /* Probe F1 (X-band) is connected */
                nmr.probe_type = PROBE_TYPE_F1;
                break;

            case '2' :      /* No probe connected -> error */
                print( FATAL, "No field probe connected to gaussmeter.\n" );
                THROW( EXCEPTION );

            case '3' :      /* Error temperature -> error */
                print( FATAL, "Temperature error from gaussmeter.\n" );
                THROW( EXCEPTION );

            case '4' :      /* TRANS L-H or H-L -> test again */
            case '5' :
                fsc2_usleep( 500000, true );
                goto try_again;

            case '6' :      /* MOD OFF -> error (should never happen */
                print( FATAL, "Modulation of gaussmeter is switched "
                       "off.\n" );
                THROW( EXCEPTION );

            case '7' :      /* MOD POS -> OK */
                nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
                break;

            case '8' :      /* MOD NEG -> OK */
                nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
                break;

            case '9' :      /* System in lock -> very good;-) */
                nmr.state = ER035M_SA_LOCKED;
                break;

            case 'A' :                   /* FIELD ? -> error */
                print( FATAL, "Gaussmeter can't find the field "
                       "(\"FIELD?\").\n" );
                THROW( EXCEPTION );

            case 'B' :                   /* SU active -> OK */
                nmr.state = ER035M_SA_SU_ACTIVE;
                break;

            case 'C' :                   /* SD active -> OK */
                nmr.state = ER035M_SA_SD_ACTIVE;
                break;

            case 'D' :                   /* OU active -> OK */
                nmr.state = ER035M_SA_OU_ACTIVE;
                break;

            case 'E' :                   /* OD active -> OK */
                nmr.state = ER035M_SA_OD_ACTIVE;
                break;

            case 'F' :      /* Search active but just at search limit -> OK */
                nmr.state = ER035M_SA_SEARCH_AT_LIMIT;
                break;
        }

    /* If the gaussmeter is already locked just get the field value, other-
       wise try to achieve locked state before asking for the field */

    if ( nmr.state == ER035M_SA_LOCKED )
        nmr.field = er035m_sa_get_field( );
    else
    {
        Var_T * v = measure_field( NULL );
        nmr.field = v->val.dval;
        vars_pop( v );
    }

    /* Find out the current settings of the search limits */

    nmr.upper_search_limit = er035m_sa_get_upper_search_limit( );
    nmr.lower_search_limit = er035m_sa_get_lower_search_limit( );

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_sa_end_of_exp_hook( void )
{
    if ( nmr.device >= 0 )
        gpib_local( nmr.device );

    nmr.device = -1;

    return 1;
}


/*************************************************/
/*                                               */
/*              exported functions               */
/*                                               */
/*************************************************/


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
gaussmeter_keep_going_on_error( Var_T * v )
{
	if ( ! v )
		return vars_push( INT_VAR, nmr.bug_behaviour );

	long behaviour;

	if ( v->type == STR_VAR )
	{
		if ( ! strcasecmp( v->val.sptr, "STOP" ) )
			behaviour = STOP_ON_BUG;
		else if ( ! strcasecmp( v->val.sptr, "CONTINUE" ) )
			behaviour = CONTINUE_ON_BUG;
		else if ( ! strcasecmp( v->val.sptr, "RETRY" ) )
			behaviour = RETRY_ON_BUG;
		else
		{
			print( FATAL, "Invalid argument \"%s\".\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
	{
		behaviour = get_strict_long( v, "behaviour on errors" );
		if (    behaviour != STOP_ON_BUG
			 && behaviour != CONTINUE_ON_BUG
			 && behaviour != RETRY_ON_BUG )
		{
			print( FATAL, "Invalid argument: %ld.\n", behaviour );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

    if ( behaviour != CONTINUE_ON_BUG )
        nmr.device_is_dead = false;

	return vars_push( INT_VAR, ( long ) ( nmr.bug_behaviour = behaviour ) );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
gaussmeter_field( Var_T * v )
{
    return measure_field( v );
}


/*----------------------------------------------------------------*
 * Function tries to get the gaussmeter into the locked state
 * and returns the current field value in a variable.
 *----------------------------------------------------------------*/

Var_T *
measure_field( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, ER035M_TEST_FIELD );

	if ( nmr.device_is_dead )
		return vars_push( FLOAT_VAR, -1.0 );

    /* If gaussmeter is in oscillator up/down state or the state is unknown
       (i.e. it's standing somewhere but not searching) search for field.
       Starting with searching down is just as probable the wrong decision
       as searching up... */

    volatile double field;

    TRY
    {
        if (    nmr.state == ER035M_SA_OU_ACTIVE
             || nmr.state == ER035M_SA_OD_ACTIVE
             || nmr.state == ER035M_SA_UNKNOWN )
            er035m_sa_command( "SD\r" );

        /* Wait for gaussmeter to go into lock state (or FAILURE) */

        while ( nmr.state != ER035M_SA_LOCKED )
        {
            /* All this can take a long time, allow user to abort */

            stop_on_user_request( );

            /* Get status byte and check if lock was achieved */

            char buffer[ 30 ];
            long length = sizeof buffer;
            er035m_sa_talk( "PS\r", buffer, &length );

            if ( length < 3 )
            {
                print( FATAL, "Undocumented data received.\n" );
                THROW( EXCEPTION );
            }

            /* Check all bytes sent except the first two */

            for ( char * bp = buffer + 2; *bp; bp++ )
                switch ( *bp )
                {
                    case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
                        break;

                    case '7' : case '8' :  /* MOD POS or NEG -> just go on */
                        break;

                    case '9' :      /* System in lock -> very good... */
                        nmr.state = ER035M_SA_LOCKED;
                        break;

                    case 'A' :      /* FIELD ? -> error */
                        if ( nmr.keep_going_on_bad_field )
                        {
                            print( SEVERE, "Gaussmeter not able to lock onto "
                                   "field.\n" );
                            return vars_push( FLOAT_VAR, -1.0 );
                        }

                        print( FATAL, "Gaussmeter not able to lock onto "
                               "field.\n" );
                        THROW( EXCEPTION );

                    case 'B' :      /* SU active -> OK */
                        nmr.state = ER035M_SA_SU_ACTIVE;
                        break;

                    case 'C' :      /* SD active */
                        nmr.state = ER035M_SA_SD_ACTIVE;
                        break;

                    case 'D' :   /* OU active -> error (should never happen) */
                        nmr.state = ER035M_SA_OU_ACTIVE;
                        print( FATAL, "Gaussmeter has an unidentifiable "
                               "problem.\n" );
                        THROW( EXCEPTION );

                    case 'E' :   /* OD active -> error (should never happen) */
                        nmr.state = ER035M_SA_OD_ACTIVE;
                        print( FATAL, "Gaussmeter has an unidentifiable "
                               "problem.\n" );
                        THROW( EXCEPTION );

                    case 'F' :   /* Search active but at a search limit -> OK*/
                        nmr.state = ER035M_SA_SEARCH_AT_LIMIT;
                        break;
                }
        }

        /* Finally  get current field value */

        field = er035m_sa_get_field( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( nmr.bug_behaviour != STOP_ON_BUG )
        {
            if ( nmr.bug_behaviour == CONTINUE_ON_BUG )
                nmr.device_is_dead = true;
            return vars_push( FLOAT_VAR, -1.0 );
        }

        RETHROW;
    }

    return vars_push( FLOAT_VAR, field );
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

	if ( nmr.device_is_dead )
		return vars_push( FLOAT_VAR, -1.0 );

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
    {
        TRY
        {
            er035m_sa_set_resolution( nmr.resolution );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( nmr.bug_behaviour != STOP_ON_BUG )
			{
				if ( nmr.bug_behaviour == CONTINUE_ON_BUG )
					nmr.device_is_dead = true;
				return vars_push( FLOAT_VAR, -1.0 );
			}

			RETHROW;
		}
    }

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
        if ( nmr.device_is_dead )
            return vars_push( INT_VAR, -1 );

        char * volatile cmd = NULL;

        TRY
        {
            cmd = T_strdup( v->val.sptr );
            translate_escape_sequences( cmd );
            er035m_sa_command( cmd );
            cmd = T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            cmd = T_free( cmd );

			if ( nmr.bug_behaviour != STOP_ON_BUG )
			{
				if ( nmr.bug_behaviour == CONTINUE_ON_BUG )
					nmr.device_is_dead = true;
				return vars_push( FLOAT_VAR, -1.0 );
			}

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

	if ( nmr.device_is_dead )
		return vars_push( FLOAT_VAR, -1.0 );

    volatile long int ul = lrnd( get_double( v, "upper search limit" ) );

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
    {
        TRY
        {
            er035m_sa_set_upper_search_limit( ul );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( nmr.bug_behaviour != STOP_ON_BUG )
			{
				if ( nmr.bug_behaviour == CONTINUE_ON_BUG )
					nmr.device_is_dead = true;
				return vars_push( FLOAT_VAR, -1.0 );
			}

			RETHROW;
		}
    }

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

	if ( nmr.device_is_dead )
		return vars_push( FLOAT_VAR, -1.0 );

    volatile long ll = lrnd( get_double( v, "lower search limit" ) );

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
    {
        TRY
        {
            er035m_sa_set_lower_search_limit( ll );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( nmr.bug_behaviour != STOP_ON_BUG )
			{
				if ( nmr.bug_behaviour == CONTINUE_ON_BUG )
					nmr.device_is_dead = true;
				return vars_push( FLOAT_VAR, -1.0 );
			}

			RETHROW;
		}
    }

    nmr.lower_search_limit = ll;

    return vars_push( FLOAT_VAR, ( double ) ll );
}


/******************************************************/
/*                                                    */
/*            internally used functions               */
/*                                                    */
/******************************************************/


/*------------------------------------------------------------------------*
 * er035m_sa_get_field() returns the field value from the gaussmeter - it
 * will return the settled value but will report a failure if gaussmeter
 * isn't in lock state. Another reason for a failure is a field that is
 * too unstable to achieve the requested resolution eventhough the
 * gaussmeter is already in lock state.
 * Take care: If the gaussmeter isn't already in the lock state call
 *            the function find_field() instead.
 *------------------------------------------------------------------------*/

static
double
er035m_sa_get_field( void )
{
    /* Repeatedly ask for field value until it's correct up to LSD -
       give up after 'ER035M_SA_MAX_RETRIES' retries to avoid getting
       into an infinite loop if the field is too unstable */

    long tries = ER035M_SA_MAX_RETRIES;
    char * state_flag;
    char buffer[ 50 ];

    do
    {
        stop_on_user_request( );

        /* Ask gaussmeter to send the current field and read result */

        long length = sizeof buffer;
        er035m_sa_talk( "PF\r", buffer, &length );
        if ( length < 3 )
            er035m_sa_failure( );

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

            print( FATAL, "Gaussmeter can't lockto on the field.\n" );
            THROW( EXCEPTION );
        }

    } while ( *state_flag != '0' && tries-- > 0 );

    /* If the maximum number of retries was exceeded give up */

    if ( tries <= 0 )
    {
        if ( nmr.keep_going_on_bad_field )
        {
            print( SEVERE, "Field is too unstable to be measured with the "
                   "requested resolution of %0f mG.\n",
                   1.0e3 * res_list[ nmr.resolution ] );
            return nmr.field = -1.0;
        }

        print( FATAL, "Field is too unstable to be measured with the "
               "requested resolution of %0f mG.\n",
               1.0e3 * res_list[ nmr.resolution ] );
        THROW( EXCEPTION );
    }

    /* Evaluate the field value part of the string */

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
er035m_sa_get_resolution( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;

    er035m_sa_talk( "RS\r", buffer, &length );

    if ( length >= 3 )
    {
        switch ( buffer[ 2 ] )
        {
            case '1' :
                return LOW_RES;

            case '2' :
                return MEDIUM_RES;

            case '3' :
                return HIGH_RES;
        }
    }

    print( FATAL, "Undocumented data received.\n" );
    THROW( EXCEPTION );

    return UNDEF_RES;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static
void
er035m_sa_set_resolution( int res_index )
{
    char buf[ 5 ];

    sprintf( buf, "RS%1d\r", res_index + 1 );
    er035m_sa_command( buf );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
long
er035m_sa_get_upper_search_limit( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;

    er035m_sa_talk( "UL\r", buffer, &length );
    return T_atol( buffer );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
long
er035m_sa_get_lower_search_limit( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;

    er035m_sa_talk( "LL\r", buffer, &length );
    return T_atol( buffer );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sa_set_upper_search_limit( long ul )
{
    char cmd[ 40 ];

    sprintf( cmd, "UL%ld\r", ul );
    er035m_sa_command( cmd );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sa_set_lower_search_limit( long ll )
{
    char cmd[ 40 ];

    sprintf( cmd, "LL%ld\r", ll );
    er035m_sa_command( cmd );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sa_command( const char * cmd )
{
    if ( gpib_write( nmr.device, cmd, strlen( cmd ) ) == FAILURE )
        er035m_sa_failure( );
    fsc2_usleep( ER035M_SA_WAIT, false );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static
void
er035m_sa_talk( char const * cmd,
                char       * reply,
                long       * length )
{
    er035m_sa_command( cmd );

    *length -= 1;
    if ( gpib_read( nmr.device, reply, length ) == FAILURE )
        er035m_sa_failure( );
    reply[ *length ] = '\0';

    /* Remove trailing line-feeds and carriage-returns */

    while (    *length
            && isspace( ( int ) reply[ *length - 1 ] ) )
        reply[ --*length ] = '\0';
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

static
void
er035m_sa_failure( void )
{
    print( FATAL, "Can't access the gaussmeter.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
