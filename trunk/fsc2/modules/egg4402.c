/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/



#include "fsc2.h"
#include "gpib_if.h"


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "EGG4402"


/* declaration of exported functions */

int egg4402_init_hook( void );
int egg4402_exp_hook( void );
int egg4402_end_of_exp_hook( void );

Var *boxcar_name( Var *v );
Var *boxcar_curve_length( Var *v );
Var *boxcar_get_curve( Var *v );


/* Locally used functions */

static bool egg4402_init( const char *name );
static void egg4402_failure( void );
static void egg4402_query( char *buffer, long *length );



typedef struct
{
	int device;
	bool fatal_error;  /* set on communication errors to avoid long delays on
						  exit due to communication with non-reacting device */
} EGG4402;


static EGG4402 egg4402;


/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int egg4402_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	egg4402.device = -1;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int egg4402_exp_hook( void )
{
	/* Nothing to be done yet in a test run */

	if ( TEST_RUN )
		return 1;

	/* Initialize the box-car */

	egg4402.fatal_error = UNSET;

	if ( ! egg4402_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int egg4402_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( egg4402.device >= 0 && ! egg4402.fatal_error )
		gpib_local( egg4402.device );

	egg4402.fatal_error = UNSET;
	egg4402.device = -1;

	return 1;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *boxcar_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

Var *boxcar_curve_length( Var *v )
{
	char buffer[ 100 ];
	long length = 100;
	long num_points;
	long inter;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, 128 );

		if ( gpib_write( egg4402.device, "CL\n", 3 ) == FAILURE )
			egg4402_failure( );
		egg4402_query( buffer, &length );

		buffer[ length - 1 ] = '\0';
		return vars_push( INT_VAR, T_atol( buffer ) );
	}

	if ( ! TEST_RUN && I_am == PARENT )
	{
		eprint( FATAL, SET, "%s: Number of points can only be set in "
				"EXPERIMENT section.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		num_points = v->val.lval;
	else
	{
		eprint( WARN, SET, "%s: Floating point number used as numer of "
				"points in function %s().\n", DEVICE_NAME, Cur_Func );
		num_points = lround( v->val.dval );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "Superfluous parameter in call of function %s().\n",
				DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( num_points < 32 || num_points > 4096 )
	{
		eprint( FATAL, SET, "%s: Invalid number of points in function %s().\n"
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION );
	}

	inter = num_points;
	while ( inter != 1 )
	{
		inter /= 2;
		if ( inter != 1 && inter & 1 )
		{
			eprint( FATAL, SET, "%s: Invalid number of points (%ld) "
					"in function %s().\n", DEVICE_NAME, num_points, Cur_Func );
			THROW( EXCEPTION );
		}
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, num_points );

	sprintf( buffer, "CL %ld\n", num_points );
	if ( gpib_write( egg4402.device, buffer, strlen( buffer ) ) == FAILURE )
		egg4402_failure( );

	return vars_push( INT_VAR, num_points );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

Var *boxcar_get_curve( Var *v )
{
	unsigned char *buffer;
	double *ret_buffer;
	char cmd[ 100 ];
	long length;
	long curve_type;
	long curve_number;
	long max_points;
	long first, last;
	long num_points;
	long i;
	unsigned char *cc, *cn;
	Var *cl;
	double tmos[ 7 ] = { 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0 };
	int new_timo, old_timo = -1;
	double max_time;
	bool size_dynamic = UNSET;


	if ( ! TEST_RUN && I_am == PARENT )
	{
		eprint( FATAL, SET, "%s: Curves can only be fetched in the "
				"EXPERIMENT section.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	/* If the first variable is a number, the second also must be a number.
	   The first number must be 0 or 1 for live curve or memory curve, the
	   second must be the curve number */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			curve_type = v->val.lval;
		else
		{
			eprint( WARN, SET, "%s: Floating point number used as curve "
					"type in function %s().\n", DEVICE_NAME, Cur_Func );
			curve_type = lround( v->val.dval );
		}

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			eprint( FATAL, SET, "%s: Missing curve number in function %s():\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION );
		}

		v = vars_pop( v );
	}
	else                             /* first argument is a string */
	{
		curve_type = -1;

		if ( ! strncmp( v->val.sptr, "LC", 2 ) )
		{
			curve_type = 0;
			curve_number = T_atol( v->val.sptr + 2 );
		}

		if ( ! strncmp( v->val.sptr, "LIVECURVE", 9 ) )
		{
			curve_type = 0;
			curve_number = T_atol( v->val.sptr + 9 );
		}

		if ( ! strncmp( v->val.sptr, "LIVE_CURVE", 10 ) )
		{
			curve_type = 0;
			curve_number = T_atol( v->val.sptr + 10 );
		}

		if ( ! strncmp( v->val.sptr, "MC", 2 ) )
		{
			curve_type = 1;
			curve_number = T_atol( v->val.sptr + 2 );
		}

		if ( ! strncmp( v->val.sptr, "MEMORYCURVE", 11 ) )
		{
			curve_type = 1;
			curve_number = T_atol( v->val.sptr + 11 );
		}

		if ( ! strncmp( v->val.sptr, "MEMORY_CURVE", 12 ) )
		{
			curve_type = 1;
			curve_number = T_atol( v->val.sptr + 12 );
		}

		if ( curve_type == -1 )
		{
			eprint( FATAL, SET, "%s: Invalid 1. argument (`%s') in "
					"function %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION );
		}

		v = vars_pop( v );
	}

	/* Check validity of curve type and number argument */

	if ( curve_type == 0 && ( curve_number < 1 || curve_number > 2 ) )
	{
		eprint( FATAL, SET, "%s: Invalid live curve number %d in function "
				"%s().\n", DEVICE_NAME, curve_number, Cur_Func );
			THROW( EXCEPTION );
	}

	if ( curve_type == 1 && ( curve_number < 1 || curve_number > 3 ) )
	{
		eprint( FATAL, SET, "%s: Invalid memory curve number %d in "
				"function %s().\n", DEVICE_NAME, curve_number, Cur_Func );
			THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		max_points = 4096;
	else
	{
		cl = boxcar_curve_length( NULL );
		max_points = cl->val.lval;
		vars_pop( cl );
	}

	if ( v == NULL )
	{
		first = 0;
		last = max_points - 1;
		size_dynamic = SET;
	}
	else
	{
		vars_check( v, INT_VAR | FLOAT_VAR );
		if ( v->type == INT_VAR )
			first = v->val.lval - 1;
		else
		{
			eprint( WARN, SET, "%s: Floating point number used as first "
				"point in function %s().\n", DEVICE_NAME, Cur_Func );
			first = lround( v->val.dval ) - 1;
		}

		if ( first < 0 || first >= max_points )
		{
			eprint( FATAL, SET, "%s: Invalid value (%ld) for first point of "
					"curve in function %s().\n", DEVICE_NAME, first + 1,
					Cur_Func);
			THROW( EXCEPTION );
		}

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			last = max_points - 1;
			size_dynamic = SET;
		}
		else
		{
			if ( v->type == INT_VAR )
				last = v->val.lval - 1;
			else
			{
				eprint( WARN, SET, "%s: Floating point number used as last "
						"point in function %s().\n", DEVICE_NAME, Cur_Func );
				last = lround( v->val.dval ) - 1;
			}

			if ( last < 0 || last >= max_points )
			{
				eprint( FATAL, SET, "%s: Invalid value (%ld) for last "
						"point in function %s().\n", DEVICE_NAME, last + 1,
						Cur_Func );
				THROW( EXCEPTION );
			}
		}

		if ( first >= last )
		{
			eprint( FATAL, SET, "%s: Last point not larger than first "
					"point in function %s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION );
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous parameter in call of"
				"function %s().\n", DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	num_points = last - first + 1;

	if ( TEST_RUN )
	{
		
		buffer = T_calloc( num_points, sizeof( double ) );
		cl = vars_push( FLOAT_TRANS_ARR, buffer, last - first + 1 );
		if ( size_dynamic )
			cl->flags |= IS_DYNAMIC;
		T_free( buffer );
		return cl;
	}

	/* Get a buffer large enough to hold all data */
	/* !!!!!! Is the size we need for the buffer correct ? */

	length = num_points * 15 + 1;
	buffer = T_malloc( length );

	/* Set transfer type to ASCII float */

	if ( gpib_write( egg4402.device, "NT 1\n", 5 ) == FAILURE )
		egg4402_failure( );

	/* Set the curve to use for the transfer */

	sprintf( cmd, "CRV %ld %ld\n", curve_type, curve_number );
	if ( gpib_write( egg4402.device, cmd, strlen( cmd ) ) == FAILURE )
		egg4402_failure( );
	
	/* Tell the boxcar to send points */

	sprintf( cmd, "DC %ld %ld\n", first, num_points );
	if ( gpib_write( egg4402.device, cmd, strlen( cmd ) ) == FAILURE )
		egg4402_failure( );

	/* Try to read the data - on failure we still have to free the buffer */

	TRY
	{
		/* Set a timeout value that we have at least 20 ms per point - this
		   seems to be the upper limit needed. Don't set timout to less than
		   1s. Reset timeout when finished. */

		max_time = 0.02 * num_points;
		new_timo = 0;
		while ( tmos[ new_timo ] < max_time )
			new_timo++;
		new_timo += 11;
		gpib_timeout( egg4402.device, new_timo );

#if ! defined GPIB_NONE )
#if defined GPIB_JTT
		old_timo = ( int ) gpib_count;
#else
		old_timo = ( int ) ibcnt;
#endif
#endif

		egg4402_query( buffer, &length );
		gpib_timeout( egg4402.device, old_timo );
		TRY_SUCCESS;
	}
	CATCH( USER_BREAK_EXCEPTION )
	{
		if ( old_timo > 0 )
			gpib_timeout( egg4402.device, old_timo );
		T_free( buffer );
		PASSTHROU( );
	}
	CATCH( EXCEPTION )
	{
		T_free( buffer );
		PASSTHROU( );
	}

	/* Get a buffer for the data in binary form and convert the ASCII data */

	buffer[ length - 1 ] = '\0';
	ret_buffer = T_malloc( num_points * sizeof( double ) );

	for ( i = 0, cc = buffer; i < num_points; cc = cn, i++ )
	{
		cn = strchr( cc, '\r' );
		if ( cn == NULL )
		{
			T_free( buffer );
			eprint( FATAL, SET, "%s: Received unexpected data in function "
					"%s().\n", DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION );
		}
		*cn = '\0';
		cn += 2;                  /* the boxcar uses "\r\n" as separator */

		ret_buffer[ i ] = T_atof( cc );
	}

	T_free( buffer );

	cl = vars_push( FLOAT_TRANS_ARR, ret_buffer, last - first + 1 );
	T_free( ret_buffer );
	return cl;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static bool egg4402_init( const char *name )
{
	fsc2_assert( egg4402.device < 0 );

	if ( gpib_init_device( name, &egg4402.device ) == FAILURE )
        return FAIL;

	return OK;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static void egg4402_failure( void )
{
	eprint( FATAL, UNSET, "%s: Communication with boxcar failed.\n",
			DEVICE_NAME );
	egg4402.fatal_error = SET;
	THROW( EXCEPTION );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

static void egg4402_query( char *buffer, long *length )
{
	unsigned char stb;
	long dummy = 1;
	

	do
	{
		usleep( 100000 );
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION );
		if ( gpib_serial_poll( egg4402.device, &stb ) == FAILURE )
			egg4402_failure( );
	} while ( ! ( stb & 0x80 ) && ! ( stb & 1 ) );

	if ( gpib_read( egg4402.device, &stb, &dummy ) == FAILURE ||
		 gpib_read( egg4402.device, buffer, length ) == FAILURE )
		egg4402_failure( );
}
