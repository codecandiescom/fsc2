/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


/* Include the header file for the library for the card */

#include <witio_48.h>


/* Include configuration information for the device */

#include "witio_48.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/* Exported functions */

int witio_48_init_hook(       void );
int witio_48_test_hook(       void );
int witio_48_exp_hook(        void );
int witio_48_end_of_exp_hook( void );
void witio_48_exit_hook(      void );


Var_T *dio_name(        Var_T * v );
Var_T *dio_reserve_dio( Var_T * v );
Var_T *dio_mode(        Var_T * v );
Var_T *dio_value(       Var_T * v );


#define NUMBER_OF_DIOS   2


static long translate_channel( long channel );

static void check_ret( int ret_val );


struct WITIO_48 {
	bool is_open;
	WITIO_48_MODE mode[ NUMBER_OF_DIOS ];
	char *reserved_by[ NUMBER_OF_DIOS ];
};


static struct WITIO_48 witio_48, witio_48_saved;


/*------------------------------------------------------*
 * Function that gets called when the module is loaded.
 *------------------------------------------------------*/

int witio_48_init_hook( void )
{
	int i;


	witio_48.is_open = SET;

	for ( i = 0; i < NUMBER_OF_DIOS; i++ )
	{
		witio_48.mode[ i ] = WITIO_48_MODE_1x24;
		witio_48.reserved_by[ i ] = NULL;
	}

	return 1;
}


/*--------------------------------------------------------*
 * Function that gets called at the start of the test run
 *--------------------------------------------------------*/

int witio_48_test_hook( void )
{
	int i;


	for ( i = 0; i < NUMBER_OF_DIOS; i++ )
	{
		witio_48_saved.mode[ i ] = witio_48.mode[ i ];
		if ( witio_48.reserved_by[ i ] )
			witio_48_saved.reserved_by[ i ] =
										 T_strdup( witio_48.reserved_by[ i ] );
	}

	return 1;
}


/*---------------------------------------------------------*
 * Function that gets called at the start of an experiment
 *---------------------------------------------------------*/

int witio_48_exp_hook( void )
{
	int i;


	TRY
	{
		for ( i = 0; i < NUMBER_OF_DIOS; i++ )
		{
			check_ret( witio_48_set_mode( ( WITIO_48_DIO ) i,
										  witio_48.mode[ i ] ) );

			if ( i == 0 )
				witio_48.is_open = SET;

			if ( witio_48.reserved_by[ i ] )
				witio_48.reserved_by[ i ] =
									CHAR_P T_free( witio_48.reserved_by[ i ] );
			if ( witio_48_saved.reserved_by[ i ] )
				witio_48.reserved_by[ i ] =
								   T_strdup( witio_48_saved.reserved_by[ i ] );
		}
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( witio_48.is_open )
		{
			witio_48_close( );
			witio_48.is_open = UNSET;
		}

		RETHROW( );
	}

	return 1;
}


/*-------------------------------------------------------*
 * Function that gets called at the end of an experiment
 *-------------------------------------------------------*/

int witio_48_end_of_exp_hook( void )
{
	if ( witio_48.is_open )
	{
		witio_48_close( );
		witio_48.is_open = UNSET;
	}
	return 1;
}


/*--------------------------------------------------------------*
 * Function that gets called just before the module is unloaded
 *--------------------------------------------------------------*/

void witio_48_exit_hook( void )
{
	int i;


	/* Get rid of the strings that might have been used for locking */

	for ( i = 0; i < NUMBER_OF_DIOS; i++ )
	{
		if ( witio_48.reserved_by[ i ] )
			T_free( witio_48.reserved_by[ i ] );
		if ( witio_48_saved.reserved_by[ i ] )
			T_free( witio_48_saved.reserved_by[ i ] );
	}
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *dio_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------*
 * The module allows to get locks on the DIOs. This is done
 * so that another module can reserve a DIO for its own use
 * and the user can't accidentally mess around with the DIO.
 * To get a lock this function must be called with the DIO
 * number and a pass-phrase (usually the device name for the
 * module requesting the lock). If there's also a third
 * argument it must be boolean and if set the function will
 * look the DIO, otherwise it is unlocked.
 * The function never throws an exception, it just returns 1
 * on success (the requested DIO is now locked or unlocked)
 * or 0 if the lock state could not be changed. Trying to
 * lock twice with the same pass-phrase succeeds but is
 * basically a NOP.
 *-----------------------------------------------------------*/

Var_T *dio_reserve_dio( Var_T * v )
{
	long dio;
	bool lock_state = SET;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	/* Get and check the DIO number */

	dio = get_strict_long( v, "DIO number" ) - 1;

	if ( dio < 0 || dio >= NUMBER_OF_DIOS )
	{
		print( FATAL, "Invalid DIO number %ld, must be DIO1 or DIO2 (or 1 or "
			   "2).\n", dio + 1 );
		THROW( EXCEPTION );
	}

	/* If there's no further argument return a value indicating if the
	   DIO is locked */

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( INT_VAR, witio_48.reserved_by[ dio ] ? 1L : 0L );

	/* Get the pass-phrase */

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Second argument isn't a string.\n" );
		THROW( EXCEPTION );
	}

	/* The next, optional argument tells if the DIO is to be locked or
	   unlocked (if it's missing it defaults to locking the DIO) */

	if ( v->next != NULL )
	{
		lock_state = get_boolean( v->next );
		too_many_arguments( v->next );
	}

	/* Lock or unlock (after checking the pass-phrase) the DIO */

	if ( witio_48.reserved_by[ dio ] )
	{
		if ( lock_state == SET )
		{
			if ( ! strcmp( witio_48.reserved_by[ dio ], v->val.sptr ) )
				return vars_push( INT_VAR, 1L );
			else
				return vars_push( INT_VAR, 0L );
		}
		else
		{
			if ( ! strcmp( witio_48.reserved_by[ dio ], v->val.sptr ) )
			{
				witio_48.reserved_by[ dio ] =
								  CHAR_P T_free( witio_48.reserved_by[ dio ] );
				return vars_push( INT_VAR, 1L );
			}
			else
				return vars_push( INT_VAR, 0L );
		}
	}

	if ( ! lock_state )
		return vars_push( INT_VAR, 1L );

	witio_48.reserved_by[ dio ] = T_strdup( v->val.sptr );
	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------*
 * Function for querying or setting the mode of one of the DIOs.
 * Modes are either "3x8", "2x12", "1x24" or "16_8". If the DIO
 * has been reserved, the first argument must be the correct
 * pass-phrase agreed upon in the lock operation.
 *---------------------------------------------------------------*/

Var_T *dio_mode( Var_T * v )
{
	long dio;
	long mode;
	char *pass = NULL;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	/* If the first argument is a string we assume it's a pass-phrase */

	if ( v->type == STR_VAR )
	{
		pass = v->val.sptr;
		if ( v->next == NULL )
		{
			print( FATAL, "Missing argument(s).\n" );
			THROW( EXCEPTION );
		}
		v = v->next;
	}

	/* Get and check the DIO number */

	dio = get_strict_long( v, "DIO number" ) - 1;

	if ( dio < 0 || dio >= NUMBER_OF_DIOS )
	{
		print( FATAL, "Invalid DIO number %ld, must be DIO1 or DIO2 (or 1 or "
			   "2).\n", dio + 1 );
		THROW( EXCEPTION );
	}

	/* If the DIO is locked check the pass-phrase */

	if ( witio_48.reserved_by[ dio ] )
	{
		if ( pass == NULL )
		{
			print( FATAL, "DIO%ld is reserved, phase-phrase required.\n",
				   dio + 1 );
			THROW( EXCEPTION );
		}
		else if ( strcmp( witio_48.reserved_by[ dio ], pass ) )
		{
			print( FATAL, "DIO%ld is reserved, wrong phase-phrase.\n",
				   dio + 1 );
			THROW( EXCEPTION );
		}
	}

	/* If there's no mode argument return the currently set mode */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			check_ret( witio_48_get_mode( ( WITIO_48_DIO ) dio,
										  &witio_48.mode[ dio ] ) );
		return vars_push( INT_VAR, ( long ) witio_48.mode[ dio ] );
	}

	/* Get and check the mode argument */

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
			mode = -1;
	}
	else
		mode = get_strict_long( v, "DIO mode" );

	if ( mode < WITIO_48_MODE_3x8 || mode > WITIO_48_MODE_16_8 )
	{
		print( FATAL, "Invalid mode type '%s', valid are \"3x8\", "
			   "\"2x12\", \"1x24\" or \"16_8\" (or numbers between 0 "
			   "and 3).\n", v->val.sptr );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* Set the new mode */

	witio_48.mode[ dio ] = ( WITIO_48_MODE ) mode;

	if ( FSC2_MODE == EXPERIMENT )
		check_ret( witio_48_set_mode( ( WITIO_48_DIO ) dio,
									  witio_48.mode[ dio ] ) );

	return vars_push( INT_VAR, mode );
}


/*-----------------------------------------------------------------*
 * Function reads from or writes to one of the channels of one of
 * the DIOs. If the DIO has been reserved, the first argument must
 * be the correct pass-phrase agreed upon in the lock operation.
 *-----------------------------------------------------------------*/

Var_T *dio_value( Var_T * v )
{
	long dio;
	long ch;
	long val;
	unsigned long uval = 0;
	char *pass = NULL;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* If the first argument is a string we assume it's a pass-phrase */

	if ( v->type == STR_VAR )
	{
		pass = v->val.sptr;
		if ( v->next == NULL )
		{
			print( FATAL, "Missing arguments.\n" );
			THROW( EXCEPTION );
		}
		v = v->next;
	}

	/* Get and check the DIO number */

	dio = get_strict_long( v, "DIO number" ) - 1;

	if ( dio < 0 || dio >= NUMBER_OF_DIOS )
	{
		print( FATAL, "Invalid DIO number %ld, must be 'DIO1' or 'DIO2' "
			   "(or 1 or 2).\n", dio + 1 );
		THROW( EXCEPTION );
	}

	/* If the DIO is locked check the pass-phrase */

	if ( witio_48.reserved_by[ dio ] )
	{
		if ( pass == NULL )
		{
			print( FATAL, "DIO%ld is reserved, phase-phrase required.\n",
				   dio + 1 );
			THROW( EXCEPTION );
		}
		else if ( strcmp( witio_48.reserved_by[ dio ], pass ) )
		{
			print( FATAL, "DIO%ld is reserved, wrong phase-phrase.\n",
				   dio + 1 );
			THROW( EXCEPTION );
		}
	}

	/* Get and check the channel number */

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

	/* If there's no value to output read the input from the DIO and return
	   it to the user */

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			check_ret( witio_48_dio_in( ( WITIO_48_DIO ) dio,
										( WITIO_48_CHANNEL ) ch, &uval ) );
		return vars_push( INT_VAR, ( long ) uval );
	}

	/* Otherwise get the value and check if it's reasonable */

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

	/* Output the value */

	if ( FSC2_MODE == EXPERIMENT )
		check_ret( witio_48_dio_out( ( WITIO_48_DIO ) dio,
									 ( WITIO_48_CHANNEL ) ch, uval ) );

	return vars_push( INT_VAR, ( long ) uval );
}


/*--------------------------------------------------------------*
 * The function is used to translate back and forth between the
 * channel numbers the way the user specifies them in the EDL
 * program and the channel numbers as specified in the header
 * file of the device library.
 *--------------------------------------------------------------*/

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

	fsc2_assert( 1 == 0 );

	return -1;        /* we can't end up here */
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void check_ret( int ret_val )
{
	if ( ret_val == WITIO_48_OK )
		return;

	print( FATAL, "%s.\n", witio_48_strerror( ) );
	witio_48_close( );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
