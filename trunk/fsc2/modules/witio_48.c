/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#include "fsc2_module.h"


/* Include the header file for the library for the card */

#include <witio_48.h>


/* Include configuration information for the device */

#include "witio_48.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/* Exported functions */

int witio_48_init_hook( void );
int witio_48_test_hook( void );
int witio_48_exp_hook( void );
int witio_48_end_of_exp_hook( void );
void witio_48_exit_hook( void );


Var *dio_name( Var *v );
Var *dio_mode( Var *v );
Var *dio_value( Var *v );


static long translate_channel( long channel );
static void check_ret( int ret_val );


typedef struct {
	WITIO_48_MODE mode[ 2 ];
} WITIO_48;

static WITIO_48 witio_48, witio_48_saved;


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int witio_48_init_hook( void )
{
	int i;

	for ( i = 0; i < 2; i++ )
		witio_48.mode[ i ] = WITIO_48_MODE_1x24;

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int witio_48_test_hook( void )
{
	witio_48_saved = witio_48;
	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int witio_48_exp_hook( void )
{
	int i;


	witio_48 = witio_48_saved;

	for ( i = 0; i < 2; i++ )
		check_ret( witio_48_set_mode( ( WITIO_48_DIO ) i,
									  witio_48.mode[ i ] ) );

	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int witio_48_end_of_exp_hook( void )
{
	witio_48_close( );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void witio_48_exit_hook( void )
{
	/* This shouldn't be necessary, I just want to make 100% sure that
	   the device file for the board is really closed */

	witio_48_close( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *dio_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *dio_mode( Var *v )
{
	long dio;
	long mode;


	if ( v == NULL )
	{
		print( FATAL, "Missing DIO argument" );
		THROW( EXCEPTION );
	}

	dio = get_strict_long( v, "DIO number" ) - 1;

	if ( dio < 0 || dio > 1 )
	{
		print( FATAL, "Invalid DIO number %ld, must be 1 or 2 "
			   "(or DIO_1 or DIO_2).\n", dio + 1 );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			check_ret( witio_48_get_mode( ( WITIO_48_DIO ) dio,
										  &witio_48.mode[ dio ] ) );
		return vars_push( INT_VAR, ( long ) witio_48.mode[ dio ] );
	}

	if ( v->type == STR_VAR )
	{
		if ( ! strcmp( v->val.sptr, "3x8" ) )
			 mode = ( long ) WITIO_48_MODE_3x8;
		else if ( ! strcmp( v->val.sptr, "2x12" ) )
			 mode = ( long ) WITIO_48_MODE_2x12;
		else if ( ! strcmp( v->val.sptr, "1x24" ) )
			 mode = ( long ) WITIO_48_MODE_1x24;
		else if ( ! strcmp( v->val.sptr, "16_8" ) )
			 mode = ( long ) WITIO_48_MODE_16_8;
		else
		{
			print( FATAL, "Invalid mode type '%s', valid are \"3x8\", "
				   "\"2x12\", \"1x24\" or \"16_8\" (or numbers between 0 "
				   "and 3).\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}
	else
		mode = get_strict_long( v, "DIO mode" );

	too_many_arguments( v );

	if ( mode < WITIO_48_MODE_3x8 || mode > WITIO_48_MODE_16_8 )
	{
		print( FATAL, "Invalid mode type %ld, valid values are between 0 and "
			   "3 (or \"3x8\", \"2x12\", \"1x24\" or \"16_8\").\n" );
		THROW( EXCEPTION );
	}

	witio_48.mode[ dio ] = ( WITIO_48_MODE ) mode;

	if ( FSC2_MODE == EXPERIMENT )
		check_ret( witio_48_set_mode( ( WITIO_48_DIO ) dio,
									  witio_48.mode[ dio ] ) );

	return vars_push( INT_VAR, mode );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *dio_value( Var *v )
{
	long dio;
	long ch;
	long val;
	unsigned long uval = 0;


	if ( v == NULL )
	{
		print( FATAL, "Missing DIO argument" );
		THROW( EXCEPTION );
	}

	dio = get_strict_long( v, "DIO number" ) - 1;

	if ( dio < 0 || dio > 1 )
	{
		print( FATAL, "Invalid DIO number %ld, must be 'DIO_1' or 'DIO_2' "
			   "(or 1 or 2).\n", dio + 1 );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing channel argument" );
		THROW( EXCEPTION );
	}

	ch = translate_channel( get_strict_long( v, "channel number" ) );

	if ( ( witio_48.mode[ dio ] == WITIO_48_MODE_1x24 &&
		   ch > WITIO_48_CHANNEL_0 ) ||
		 ( ( witio_48.mode[ dio ] == WITIO_48_MODE_2x12 ||
			 witio_48.mode[ dio ] == WITIO_48_MODE_16_8 ) &&
		   ch > WITIO_48_CHANNEL_1 ) )
	{
		print( FATAL, "Invalid channel argument 'CH%ld' for current I/O mode "
			   "of selected DIO.\n", ch + 1 );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			check_ret( witio_48_dio_in( ( WITIO_48_DIO ) dio,
										( WITIO_48_CHANNEL ) ch, &uval ) );
		return vars_push( INT_VAR, ( long ) uval );
	}

	val = get_strict_long( v, "value to output" );

	too_many_arguments( v );

	if ( val < 0 )
	{
		print( FATAL, "Invalid negative output value.\n" );
		THROW( EXCEPTION );
	}

	uval = ( unsigned long ) val;

	if ( ( witio_48.mode[ dio ] == WITIO_48_MODE_1x24 &&
		   uval >= 1UL << 24 ) ||
		 ( witio_48.mode[ dio ] == WITIO_48_MODE_16_8 &&
		   ch == WITIO_48_CHANNEL_0 && uval >= 1UL << 16 ) ||
		 ( witio_48.mode[ dio ] == WITIO_48_MODE_2x12 &&
		   uval >= 1UL << 12 ) ||
		 ( ( witio_48.mode[ dio ] == WITIO_48_MODE_3x8 ||
			 ( witio_48.mode[ dio ] == WITIO_48_MODE_16_8
			   && ch == WITIO_48_CHANNEL_1 ) ) && uval > 1UL << 8 ) )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, "Value of %ld to be output is too large for the "
				   "current I/O mode of the DIO.\n", val );
			THROW( EXCEPTION );
		}
		else
		{
			if ( witio_48.mode[ dio ] == WITIO_48_MODE_1x24 )
				uval &= 0xFFFFFFUL;
			else if ( witio_48.mode[ dio ] == WITIO_48_MODE_16_8 &&
					  ch == WITIO_48_CHANNEL_0 )
				uval &= 0xFFFFUL;
			else if ( witio_48.mode[ dio ] == WITIO_48_MODE_2x12 )
				uval &= 0xFFFUL;
			else
				uval &= 0xFFUL;

			print( SEVERE, "Value of %ld to be output is too large for the "
				   "current I/O mode of the DIO, truncating it to %lu.\n",
				   val, uval );
		}
	}

	if ( FSC2_MODE == EXPERIMENT )
		check_ret( witio_48_dio_out( ( WITIO_48_DIO ) dio,
									 ( WITIO_48_CHANNEL ) ch, uval ) );

	return vars_push( INT_VAR, ( long ) uval );
}

/*--------------------------------------------------------------*/
/* The function is used to translate back and forth between the */
/* channel numbers the way the user specifies them in the EDL   */
/* program and the channel numbers as specified in the header   */
/* file of the device library.                                  */
/*--------------------------------------------------------------*/

static long translate_channel( long channel )
{
	switch ( channel )
	{
		case CHANNEL_CH1 :
			return ( long ) WITIO_48_CHANNEL_0;

		case CHANNEL_CH2 :
			return ( long ) WITIO_48_CHANNEL_1;

		case CHANNEL_CH3 :
			return ( long ) WITIO_48_CHANNEL_2;

		default :
			print( FATAL, "Invalid channel channel, DIO has no channel "
				   "named %s, use either 'CH1', 'CH2' or 'CH3'.\n",
				   Channel_Names[ channel ] );
			THROW( EXCEPTION );
	}

	return -1;        /* we can't end up here */
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void check_ret( int ret_val )
{
	if ( ret_val == WITIO_48_OK )
		return;

	print( FATAL, "%s.\n", witio_48_error_message );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
