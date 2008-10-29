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


/*-----------------------------------*
 * Tries to find a resonator by name
 *-----------------------------------*/

Resonator_T *
epr_mod_find( Var_T * v )
{
	size_t i;


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
			break;

	if ( i == epr_mod.count )
	{
		print( FATAL, "Resonator entry with name '%s' doesn't exist.\n",
               v->val.sptr );
		THROW( EXCEPTION );
	}

	return epr_mod.resonators + i;
}


/*-------------------------------------------------*
 * Tries to find a frequency entry for a resonator
 *-------------------------------------------------*/


FREQ_ENTRY_T *
epr_mod_find_fe( Resonator_T * res,
                 double        freq )
{
    size_t i;


    if ( freq <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative frequency.\n" );
        THROW( EXCEPTION );
    }

    for ( i = 0; i < res->count; i++ )
        if (    fabs( freq - res->fe[ i ].freq ) / freq
                                            < 1.0e-2 * EPR_MOD_MIN_DEVIATION
             || fabs( freq - res->fe[ i ].freq ) / res->fe[ i ].freq
                                            < 1.0e-2 * EPR_MOD_MIN_DEVIATION )
            return res->fe + i;

    return NULL;
}


/*-------------------------------*
 * Save resonator data to a file
 *-------------------------------*/

void
epr_mod_save( void )
{
    FILE *fp;
    size_t i;
    size_t j;


    fsc2_assert( epr_mod.state_file != NULL );

    raise_permissions( );

    if ( ( fp = fopen( epr_mod.state_file, "w" ) ) == NULL )
    {
		lower_permissions( );
        print( FATAL, "Failed to open state file '%s' for writing.\n",
               epr_mod.state_file );
        THROW( EXCEPTION );
    }

    fprintf( fp, "# --- Do *not* edit this file, it gets created "
             "automatically ---\n" );

    for ( i = 0; i < epr_mod.count; i++ )
    {
		Resonator_T *res = epr_mod.resonators + i;

        fprintf( fp, "\nresonator: \"%s\"\n"
				 "  interpolate: %s\n"
				 "  extrapolate: %s\n",
				 res->name, res->interpolate ? "yes" : "no",
				 res->extrapolate ? "yes" : "no" );
		for ( j = 0; j < res->count; j++ )
		{
			fprintf( fp, "  freq: %f kHz\n"
					 "    ratio: %f G/V\n",
					 0.001 * res->fe[ j ].freq, res->fe[ j ].ratio );
			if ( res->fe[ j ].is_phase )
				fprintf( fp, "    phase: %f\n", res->fe[ j ].phase );
		}
    }

	fclose( fp );
	lower_permissions( );
}


/*----------------------------------------------------*
 * Deallocates all memory used for a device structure
 *----------------------------------------------------*/

void
epr_mod_clear( EPR_MOD * em )
{
    size_t i;


    if ( em->state_file )
        em->state_file = T_free( em->state_file );

    if ( em->resonators == NULL )
        return;

    for ( i = 0; i < em->count; i++ )
    {
        if ( em->resonators[ i ].name )
            T_free( em->resonators[ i ].name );
        if ( em->resonators[ i ].fe )
            T_free( em->resonators[ i ].fe );
    }

    em->resonators = T_free( em->resonators );
    em->count = 0;
}


/*-------------------------------------------*
 * Makes a deep copy of the device structure
 *-------------------------------------------*/

void
epr_mod_copy_state( EPR_MOD * to,
                    EPR_MOD * from )
{
	size_t i;


    epr_mod_clear( to );

    if ( from->state_file )
        to->state_file = T_strdup( from->state_file );

    if ( from->count == 0 )
        return;

    to->resonators = T_malloc( from->count * sizeof *to->resonators );
    memcpy( to->resonators, from->resonators,
            from->count * sizeof *to->resonators );

    /* This may look redundant since the values get reset in the next
       step but it's necessary to allow deallocation of memory if in
       the next step an allocation fails */

    for ( i = 0; i < from->count; i++ )
    {
        to->resonators[ i ].name = NULL;
        to->resonators[ i ].fe = NULL;
        to->resonators[ i ].count = 0;
    }

    to->count = from->count;

    for ( i = 0; i < from->count; i++ )
    {
        to->resonators[ i ].name = T_strdup( from->resonators[ i ].name );
        if ( from->resonators[ i ].count == 0 )
            continue;
        to->resonators[ i ].fe = T_malloc(    from->resonators[ i ].count
                                            * sizeof *to->resonators[ i ].fe );
        memcpy( to->resonators[ i ].fe, from->resonators[ i ].fe,
                from->resonators[ i ].count * sizeof * to->resonators[ i ].fe );
        to->resonators[ i ].count = from->resonators[ i ].count;
    }
}


/*------------------------------------------------------------*
 * Function for calculating the paramaters of a least square
 * fit for the frequency/voltage ratio as proportional to
 * the inverse of of the frequency (with an offset as another
 * factor). The function also sorts the list of frequency
 * entries in ascending order of frequency.
 *------------------------------------------------------------*/

void
epr_mod_recalc( Resonator_T * res )
{
    double ifsum = 0.0,
           if2sum = 0.0,
           rsum  = 0.0,
           r2sum = 0.0,
           rifsum = 0.0;
    size_t i;


    fsc2_assert( res->count > 2 );

    /* Sort the frequency entries by ascending freuencies */

    qsort( res->fe, res->count, sizeof *res->fe, epr_mod_comp );

    /* Calculate the least square fit parameters and the square of the
       regresion coefficient under the assumption that the field/voltage
       ratio is proportional to the inverse of tge frequency */

    for ( i = 0; i < res->count; i++ )
    {
        ifsum  += 1.0 / res->fe[ i ].freq;
        if2sum += 1.0 / ( res->fe[ i ].freq * res->fe[ i ].freq );
        rsum   += res->fe[ i ].ratio;
        r2sum  += res->fe[ i ].ratio * res->fe[ i ].ratio;
        rifsum += res->fe[ i ].ratio / res->fe[ i ].freq;
    }

    res->offset  =   ( if2sum * rsum - ifsum * rifsum )
                  / ( res->count * if2sum - ifsum * ifsum );

    res->slope  =   ( res->count * rifsum - ifsum * rsum )
                  / ( res->count * if2sum - ifsum * ifsum );

    res->r2 =  ( rifsum - ifsum * rsum / res->count );
    res->r2 *=   res->r2
               / (   ( if2sum - ifsum * ifsum / res->count )
                   * ( r2sum  - rsum  * rsum  / res->count ) );
}


/*----------------------------------------------------------*
 * Comparison function for frequency entries of a resonator
 *----------------------------------------------------------*/

int
epr_mod_comp( const void * a,
              const void * b )
{
    if ( ( ( FREQ_ENTRY_T * ) a )->freq < ( ( FREQ_ENTRY_T * ) b )->freq )
        return -1;
    if ( ( ( FREQ_ENTRY_T * ) a )->freq > ( ( FREQ_ENTRY_T * ) b )->freq )
        return 1;
    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
