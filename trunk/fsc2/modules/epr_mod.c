/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#include "epr_mod.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


EPR_MOD epr_mod        = { NULL, NULL, 0 },
        epr_mod_stored = { NULL, NULL, 0 };


/*---------------------------------*
 * Init hook, reads the state file
 *---------------------------------*/

int epr_mod_init_hook( void )
{
	epr_mod_read_state( );
    return 1;
}


/*-----------------------------------------------*
 * Test hook, makes a copy of the original state
 *-----------------------------------------------*/

int epr_mod_test_hook( void )
{
    epr_mod_copy_state( &epr_mod_stored, &epr_mod );
    return 1;
}


/*---------------------------------------------------*
 * Experiment hook, reestablishes the original state
 *---------------------------------------------------*/

int epr_mod_exp_hook( void )
{
    epr_mod_copy_state( &epr_mod, &epr_mod_stored );
    return 1;
}


/*--------------------------------*
 * Exit hook, releases all memory
 *--------------------------------*/

int epr_mod_exit_hook( void )
{
    epr_mod_clear( &epr_mod_stored );
    epr_mod_clear( &epr_mod);
    return 1;
}


/*-----------------------------------*
 * Function returns the modules name
 *-----------------------------------*/

Var_T *
epr_modulation_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------*
 * Function for calculating or setting the field/voltage
 * ratio for a certain frequency for a resonator
 *-------------------------------------------------------*/

Var_T *
epr_modulation_ratio( Var_T * v )
{
	Resonator_T *res;
	double freq;
    double ratio;
    FREQ_ENTRY_T *fe;


	res = epr_mod_find( v );
	v = vars_pop( v );

	if ( v == NULL )
	{
		print( FATAL, "Missing frequency.\n" );
		THROW( EXCEPTION );
	}

	freq = get_double( v, "modulation frequency" );
	v = vars_pop( v );

    if ( freq <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative frequency.\n" );
		THROW( EXCEPTION );
    }

    if ( ( fe = epr_mod_find_fe( res, freq ) ) != NULL )
    {
        if ( v == NULL )
        {
            if (    res->interpolate
                 && res->count > 2
                    && res->r2 >= EPR_MOD_MIN_REGRESSION )
                return vars_push( FLOAT_VAR, res->slope / freq + res->offset );
            else
                return vars_push( FLOAT_VAR, fe->ratio );
        }

        ratio = get_double( v, "field/voltage ratio" );

        too_many_arguments( v );

        fe->ratio = ratio;

        if ( res->count > 2 && ( res->interpolate || res->extrapolate ) )
            epr_mod_recalc( res );
        else
            qsort( res->fe, res->count, sizeof *res->fe, epr_mod_comp );

        return vars_push( FLOAT_VAR, fe->ratio );
    }

    if ( v == NULL )
    {
        if ( freq < res->fe[ 0 ].freq || freq > res->fe[ res->count - 1 ].freq )
        {
            if ( ! res->extrapolate )
            {
                print( FATAL, "Frequency not within known range and no "
                       "permission to extrapolate.\n" );
                THROW( EXCEPTION );
            }
        }
        else if ( ! res->interpolate )
        {
            print( FATAL, "No permission to interpolate.\n" );
            THROW( EXCEPTION );
        }

        if ( res->count < 3 )
        {
            print( FATAL, "Not enough frequencies known to do inter- or "
                   "extrapolation.\n" );
            THROW( EXCEPTION );
        }

        if ( res->r2 < EPR_MOD_MIN_REGRESSION )
        {
            print( FATAL, "Correlation coefficient to small for doing "
                   "inter- or extrapolation.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR, res->slope / freq + res->offset );
    }

    ratio = get_double( v, "field/voltage ratio" );

    too_many_arguments( v );

    res->fe = T_realloc( res->fe, ( res->count + 1 ) * sizeof *res->fe );
    res->fe[ res->count ].freq = freq;
    res->fe[ res->count ].ratio = ratio;
    res->fe[ res->count++ ].is_phase = UNSET;

    if ( res->count > 2 )
        epr_mod_recalc( res );
    else
        qsort( res->fe, res->count, sizeof *res->fe, epr_mod_comp );

    return vars_push( FLOAT_VAR, ratio );
}


/*-----------------------------------------------*
 * Function for calculating or setting the phase
 * ratio for a certain frequency for a resonator
 *-----------------------------------------------*/

Var_T *
epr_modulation_phase( Var_T * v )
{
	Resonator_T *res;
	double freq;
    double phase;
    FREQ_ENTRY_T *fe;


	res = epr_mod_find( v );
	v = vars_pop( v );

	if ( v == NULL )
	{
		print( FATAL, "Missing frequency.\n" );
		THROW( EXCEPTION );
	}

	freq = get_double( v, "modulation frequency" );
	v = vars_pop( v );

    if ( ! ( fe = epr_mod_find_fe( res, freq ) ) )
    {
        print( FATAL, "Frequency doesn't match any entry.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        if ( ! fe->is_phase )
        {
            print( FATAL, "No phase known for this frequency.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR, fe->phase );
    }

    phase = get_double( v, "modulation phase" );

    too_many_arguments( v );

    while ( phase > 360.0 )
        phase -= 360.0;
    while ( phase < -360.0 )
        phase += 360.0;

    fe->phase = phase;
    fe->is_phase = SET;

    return vars_push( FLOAT_VAR, fe->phase );
}


/*----------------------------------------------*
 * Function to query if for a certain frequency
 * a modulation phase is set
 *----------------------------------------------*/

Var_T *
epr_modulation_has_phase( Var_T * v )
{
	Resonator_T *res;
	double freq;
    FREQ_ENTRY_T *fe;


	res = epr_mod_find( v );
	v = vars_pop( v );

	if ( v == NULL )
	{
		print( FATAL, "Missing frequency.\n" );
		THROW( EXCEPTION );
	}

	freq = get_double( v, "modulation frequency" );

    return vars_push( INT_VAR,
                      ( long ) (    ( fe = epr_mod_find_fe( res, freq ) )
                                 && fe->is_phase ) );
}

       
/*---------------------------------------------*
 * Function for creating a new resonator entry
 *---------------------------------------------*/

Var_T *
epr_modulation_add_resonator( Var_T * v )
{
	size_t i;
	Resonator_T *res = NULL;


	if ( v == NULL )
	{
		print( FATAL, "Missing resonator name.\n" );
		THROW( EXCEPTION );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Argument must be a string.\n" );
		THROW( EXCEPTION );
	}

    if ( *v->val.sptr == '\0' )
    {
        print( FATAL, "Invalid resonator name.\n" );
        THROW( EXCEPTION );
    }

	for ( i = 0; i < epr_mod.count; i++ )
		if ( ! strcmp( epr_mod.resonators[ i ].name, v->val.sptr ) )
		{
			res = epr_mod.resonators + i;
			break;
		}

	if ( res != NULL )
	{
		print( FATAL, "Resonator entry with name '%s' already exists.\n",
               v->val.sptr );
		THROW( EXCEPTION );
	}

	TRY
	{
		res = T_malloc( sizeof *res );
		res->name = T_strdup( v->val.sptr );
        res->interpolate = UNSET;
        res->extrapolate = UNSET;
        res->fe = NULL;
		res->count = 0;

		epr_mod.resonators =
			realloc( epr_mod.resonators,
					 ( epr_mod.count + 1 ) * sizeof *epr_mod.resonators );
		epr_mod.resonators[ epr_mod.count++ ] = *res;
		TRY_SUCCESS;
	}
	CATCH ( OUT_OF_MEMORY_EXCEPTION )
	{
		if ( res )
        {
            if ( res->name )
                T_free( res->name );
			T_free( res );
        }
		RETHROW( );
	}

	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------*
 * Function for deleting a complete resonator entry
 *--------------------------------------------------*/

Var_T *
epr_modulation_delete_resonator( Var_T * v )
{
	Resonator_T *res = epr_mod_find( v );
	size_t i;


	for ( i = 0; i < epr_mod.count; i++ )
		if ( epr_mod.resonators + i == res )
			break;

	fsc2_assert( i < epr_mod.count );

	T_free( res->fe );
	T_free( res->name );

	if ( i < epr_mod.count - 1 )
		memmove( epr_mod.resonators + i, epr_mod.resonators + i + 1,
				 ( epr_mod.count - i - 1 ) * sizeof *epr_mod.resonators );

    if ( --epr_mod.count > 0 )
        epr_mod.resonators = 
            T_realloc( epr_mod.resonators,
                       epr_mod.count * sizeof *epr_mod.resonators );
    else
        epr_mod.resonators = T_free( epr_mod.resonators );

	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------*
 * Function returns the number of resonator entries
 *--------------------------------------------------*/

Var_T *
epr_modulation_resonator_count( Var_T * v  UNUSED_ARG )
{
    return vars_push( INT_VAR, ( long ) epr_mod.count );
}


/*--------------------------------------------*
 * Function to set or query a resonators name
 *--------------------------------------------*/

Var_T *
epr_modulation_resonator_name( Var_T * v )
{
    long idx;
    char *name;


    if ( v == NULL )
    {
        print( FATAL, "Missing resonator index.\n" );
        THROW( EXCEPTION );
    }

    idx = get_long( v, "resonator index" ) - 1;

    if ( idx < 0 || idx >= ( long ) epr_mod.count )
    {
        print( FATAL, "Invalid resonator index." );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( STR_VAR, epr_mod.resonators[ idx ].name );

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Invalid argument, resonator name must be a string "
               "variable.\n" );
        THROW( EXCEPTION );
    }

    if ( *v->val.sptr == '\0' )
    {
        print( FATAL, "Invalid resonator name.\n" );
        THROW( EXCEPTION );
    }

    name = T_strdup( v->val.sptr );

    return vars_push ( STR_VAR,
                       epr_mod.resonators[ idx ].name = name );
}


/*------------------------------------------------------------------*
 * Function to set or query if interpolation is set for a resonator
 *------------------------------------------------------------------*/

Var_T *
epr_modulation_resonator_interpolate( Var_T * v )
{
	Resonator_T *res = epr_mod_find( v );


    if ( ( v = vars_pop( v ) ) != NULL )
    {
        res->interpolate = get_boolean( v );
        too_many_arguments( v );
    }

    return vars_push( INT_VAR, ( long ) res->interpolate );
}


/*----------------------------------------------------------------*
 * Function to query if interpolation can be done for a resonator
 *----------------------------------------------------------------*/

Var_T *
epr_modulation_resonator_can_interpolate( Var_T * v  UNUSED_ARG )
{
	Resonator_T *res = epr_mod_find( v );


    return vars_push( INT_VAR,
                      ( long ) (    res->interpolate
                                 && res->count > 2
                                 && res->r2 >= EPR_MOD_MIN_REGRESSION ) );
}    


/*------------------------------------------------------------------*
 * Function to set or query if extrapolation is set for a resonator
 *------------------------------------------------------------------*/

Var_T *
epr_modulation_resonator_extrapolate( Var_T * v )
{
	Resonator_T *res = epr_mod_find( v );


    if ( ( v = vars_pop( v ) ) != NULL )
    {
        res->extrapolate = get_boolean( v );
        too_many_arguments( v );
    }

    return vars_push( INT_VAR, ( long ) res->extrapolate );
}


/*----------------------------------------------------------------*
 * Function to query if extrapolation can be done for a resonator
 *----------------------------------------------------------------*/

Var_T *
epr_modulation_resonator_can_extrapolate( Var_T * v  UNUSED_ARG )
{
	Resonator_T *res = epr_mod_find( v );


    return vars_push( INT_VAR,
                      ( long ) (    res->extrapolate
                                 && res->count > 2
                                 && res->r2 >= EPR_MOD_MIN_REGRESSION ) );
}    


/*---------------------------------------------------------*
 * Function for query of number of entries for a resonator
 *---------------------------------------------------------*/

Var_T *
epr_modulation_resonator_frequencies( Var_T * v )
{
	Resonator_T *res = epr_mod_find( v );
    double *freq = NULL;
    size_t i;


    if ( res->count > 0 )
    {
        freq = T_malloc( res->count * sizeof *freq );
        for ( i = 0; i < res->count; i++ )
            freq[ i ] = res->fe[ i ].freq;
    }

    return vars_push( FLOAT_ARR, freq, res->count );
}


/*----------------------------------------------------*
 * Function to be called for storing changed settings
 *----------------------------------------------------*/

Var_T *
epr_modulation_store( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != TEST )
    {
        epr_mod_save( );

        if ( FSC2_MODE != PREPARATION )
            epr_mod_copy_state( &epr_mod_stored, &epr_mod );
    }

    return vars_push( INT_VAR, 1 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
