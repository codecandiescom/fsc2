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


#include "rb_pulser.h"


static void rb_pulser_init_print( FILE *fp );
static void rb_pulser_basic_pulse_check( void );
static void rb_pulser_basic_functions_check( void );
static void rb_pulser_rf_synth_init( void );


/*-------------------------------------------------------------------*/
/* Function does everything that needs to be done for checking and   */
/* completing the internal representation of the pulser at the start */
/* of a test run.                                                    */
/*-------------------------------------------------------------------*/

void rb_pulser_init_setup( void )
{
	rb_pulser_basic_pulse_check( );
	rb_pulser_basic_functions_check( );
	rb_pulser_rf_synth_init( );

	if ( rb_pulser.dump_file != NULL )
		rb_pulser_init_print( rb_pulser.dump_file );

	if ( rb_pulser.show_file != NULL )
		rb_pulser_init_print( rb_pulser.show_file );

	rb_pulser_update_pulses( SET );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static void rb_pulser_init_print( FILE *fp )
{
	int i;


	if ( fp == NULL )
		return;

	fprintf( fp, "TB: %g\nD: %ld\n===\n", rb_pulser.timebase,
			 Ticks_rnd( rb_pulser.neg_delay / rb_pulser.timebase ) );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( rb_pulser.function[ i ].is_used )
			fprintf( fp, "%s:  %ld\n", rb_pulser.function[ i ].name,
					 Ticks_rnd( rb_pulser.function[ i ].delay /
								rb_pulser.timebase ) );
}


/*-------------------------------------------------------------------------*
 * Function runs through all pulses and checks that at least:
 * 1. the pulse is associated with a function
 * 2. the start position is set
 * 3. the length is set
 *-------------------------------------------------------------------------*/

static void rb_pulser_basic_pulse_check( void )
{
	PULSE *p;


	for ( p = rb_pulser_Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			print( FATAL, "Pulse #%ld is not associated with a function.\n",
				   p->num );
			THROW( EXCEPTION );
		}

		/* Check start position and pulse length */

		if ( ! p->is_pos || ! p->is_len || p->len == 0 )
			p->is_active = UNSET;

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;
	}
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void rb_pulser_basic_functions_check( void )
{
	FUNCTION *f;
	int i;
	PULSE *cp;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser.function + i;

		if ( f->num_pulses == 0 )
		{
			if(  f->is_declared )
				print( WARN, "No pulses have been assigned to function "
					   "'%s'.\n", f->name );
			f->is_used = UNSET;
		}

		if ( ! f->is_used )
			continue;

		/* Assemble a list of all pulses assigned to the function */

		for ( f->num_pulses = 0, cp = rb_pulser_Pulses;
			  cp != NULL; cp = cp->next )
		{
			if ( cp->function != f )
				continue;

			f->num_pulses++;
			f->pulses = PULSE_PP T_realloc( f->pulses, f->num_pulses
													   * sizeof *f->pulses );
			f->pulses[ f->num_pulses - 1 ] = cp;
		}
	}
}


/*------------------------------------------------------------------------*
 * Find out about the names of synthesizer related functions if RF pulses
 * could be required (because there's at least a single pulse marked as
 * belonging to the RF function)
 *------------------------------------------------------------------------*/

static void rb_pulser_rf_synth_init( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_RF;
	int dev_num;
	char *func;
	Var *Func_ptr;
	int acc;


	if ( ! f->is_used || f->pulses == NULL )
		return;

	/* Try to find the functions for setting the pulse width and delay for
	   the synthesizer */

	if ( ( dev_num = exists_device( SYNTHESIZER_MODULE ) ) == 0 )
	{
		print( FATAL, "Can't find the module '%s' - it must be listed in the "
			   "DEVICES section to allow RF pulses.\n", SYNTHESIZER_MODULE );
		THROW( EXCEPTION );
	}

	if ( dev_num )
		func = T_strdup( SYNTHESIZER_PULSE_STATE );
	else
		func = get_string( SYNTHESIZER_PULSE_STATE "#%d", dev_num );
	
	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for switching pulse modulation on or off is "
			   "missing from the synthesizer module '%s'.\n",
			   SYNTHESIZER_MODULE );
		THROW( EXCEPTION );
	}

	rb_pulser.synth_pulse_state = func;

	if ( dev_num )
		func = T_strdup( SYNTHESIZER_PULSE_WIDTH );
	else
		func = get_string( SYNTHESIZER_PULSE_WIDTH "#%d", dev_num );
	
	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		rb_pulser.synth_pulse_state =
								  CHAR_P T_free( rb_pulser.synth_pulse_state );
		T_free( func );
		print( FATAL, "Function for setting pulse widths is missing from "
			   "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
		THROW( EXCEPTION );
	}

	rb_pulser.synth_pulse_width = func;
	
	if ( dev_num )
		func = T_strdup( SYNTHESIZER_PULSE_DELAY );
	else
		func = get_string( SYNTHESIZER_PULSE_DELAY "#%d", dev_num );
	
	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		rb_pulser.synth_pulse_state =
								  CHAR_P T_free( rb_pulser.synth_pulse_state );
		rb_pulser.synth_pulse_width =
								  CHAR_P T_free( rb_pulser.synth_pulse_width );
		T_free( func );
		print( FATAL, "Function for setting pulse delays is missing from "
			   "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
		THROW( EXCEPTION );
	}

	rb_pulser.synth_pulse_delay = func;

	if ( dev_num )
		func = T_strdup( SYNTHESIZER_TRIG_SLOPE );
	else
		func = get_string( SYNTHESIZER_TRIG_SLOPE "#%d", dev_num );
	
	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		rb_pulser.synth_pulse_state =
								  CHAR_P T_free( rb_pulser.synth_pulse_state );
		rb_pulser.synth_pulse_delay =
								  CHAR_P T_free( rb_pulser.synth_pulse_delay );
		rb_pulser.synth_pulse_width =
								  CHAR_P T_free( rb_pulser.synth_pulse_width );
		T_free( func );
		print( FATAL, "Function for setting the trigger slope is missing from "
			   "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
		THROW( EXCEPTION );
	}

	rb_pulser.synth_trig_slope = func;

}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
