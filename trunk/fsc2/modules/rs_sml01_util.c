/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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


#include "rs_sml01.h"

extern size_t num_fm_mod_ranges;
extern struct MOD_RANGES fm_mod_ranges[ ];
extern size_t num_pm_mod_ranges;
extern struct MOD_RANGES pm_mod_ranges[ ];


/*----------------------------------------------------------------------*
 * If the function succeeds it returns a file pointer to the table file
 * and sets the table_file entry in the device structure to the name of
 * the table file. Otherwise an exception is thrown. In any case the
 * memory used for the file name passed to the function is deallocated.
 *----------------------------------------------------------------------*/

FILE *rs_sml01_find_table( char **name )
{
	FILE *tfp;
	char *new_name;


	/* Expand a leading tilde to the users home directory */

	if ( ( *name )[ 0 ] == '~' )
	{
		new_name = get_string( "%s%s%s", getenv( "HOME" ),
							   ( *name )[ 1 ] != '/' ? "/" : "", *name + 1 );
		T_free( *name );
		*name = new_name;
	}

	/* Now try to open the file - set table_file entry in the device structure
	   to the name and return the file pointer */

	if ( ( tfp = rs_sml01_open_table( *name ) ) != NULL )
	{
		rs_sml01.table_file = T_strdup( *name );
		return tfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( *name, '/' ) != NULL )
	{
		print( FATAL, "Table file '%s' not found.\n", *name );
		THROW( EXCEPTION );
	}

	/* Last chance: The table file is in the library directory... */

	new_name = get_string( "%s%s%s", libdir, slash( libdir ), *name );
	T_free( *name );
	*name = new_name;

	if ( ( tfp = rs_sml01_open_table( *name ) ) == NULL )
	{
		print( FATAL, "Table file '%s' not found in either the current "
			   "directory or in '%s'.\n", strip_path( *name ), libdir );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*------------------------------------------------------------------*
 * Tries to open the file with the given name and returns the file
 * pointer, returns NULL if file does not exist returns, or throws
 * an exception if the file can't be read (either because of prob-
 * lems with the permissions or other, unknoen reasons). If opening
 * the file fails with an exception memory used for its name is
 * deallocated.
 *------------------------------------------------------------------*/

FILE *rs_sml01_open_table( char *name )
{
	FILE *tfp;


	if ( access( name, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		print( FATAL, "No read permission for table file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	if ( ( tfp = fopen( name, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open table file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*-----------------------------------------------*
 * Calculates from the table the attenuation for
 * a given frequency by interpolation.
 *-----------------------------------------------*/

double rs_sml01_get_att_from_table( double freq )
{
	long i_low, i_high, i_cur;
	double att;


	/* First check that the frequency is covered by the table, if it isn't
	   print out a warning and return attenuation for the smallest or
	   highest frequency */

	if ( freq < rs_sml01.min_table_freq )
	{
		print( WARN, "Frequency of %f MHz not covered by table, "
			   "interpolating.\n", freq * 1.0e-6 );
		return rs_sml01.att_table[ 0 ].att;
	}

	if ( freq > rs_sml01.max_table_freq )
	{
		print( WARN, "Frequency of %f MHz not covered by table, "
			   "interpolating.\n", freq * 1.0e-6 );
		return rs_sml01.att_table[ rs_sml01.att_table_len - 1 ].att;
	}

	/* Find the indices of the two entries in the table (that has been sorted
	   in ascending order of frequencies) bracketing the frequency. Since in
	   most cases the frequencies in the table are equally spaced, we first
	   try to find the entry by interpolating. If this doesn't succeeds the
	   frequencies can't be uniformly spaced and we used simple bisecting.*/

	i_low = 0;
	i_high = rs_sml01.att_table_len - 1;

	if ( freq == rs_sml01.att_table[ i_low ].freq )
		return rs_sml01.att_table[ i_low ].att;
	if ( freq == rs_sml01.att_table[ i_high ].freq )
		return rs_sml01.att_table[ i_high ].att;

	i_cur = lrnd( floor( ( i_high - i_low ) *
						 ( freq - rs_sml01.att_table[ i_low ].freq ) /
						 ( rs_sml01.att_table[ i_high ].freq
						   - rs_sml01.att_table[ i_low ].freq ) ) ) + i_low;

	if ( freq > rs_sml01.att_table[ i_cur ].freq )
	{
		i_low = i_cur;
		if ( freq < rs_sml01.att_table[ i_cur + 1 ].freq )
			i_high = i_cur + 1;
	}
	else
		i_high = i_cur;

	while ( i_high - i_low > 1 )
	{
		if ( freq > rs_sml01.att_table[ i_cur ].freq )
			i_low = i_cur;
		else
			i_high = i_cur;
		i_cur = ( i_high + i_low ) >> 1;
	}

	/* Now do a linear interpolation for the attenuation between the bracketing
	   frequencies we have data for */

	att =   ( rs_sml01.att_table[ i_high ].att
			  - rs_sml01.att_table[ i_low ].att )
		  * ( freq - rs_sml01.att_table[ i_low ].freq )
		  / (   rs_sml01.att_table[ i_high ].freq
			  - rs_sml01.att_table[ i_low ].freq )
		  + rs_sml01.att_table[ i_low ].att;

	/* Reduce value to the resolution the attenuation can be set with */

	return ATT_RESOLUTION * lrnd( att / ATT_RESOLUTION );
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

double rs_sml01_get_att( double freq )
{
	double att;

	if ( ! rs_sml01.use_table )
		return rs_sml01.attenuation;

	att =   rs_sml01.attenuation + rs_sml01_get_att_from_table( freq )
		  - rs_sml01.att_at_ref_freq;

	if ( att < MAX_ATTEN )
	{
		print( SEVERE, "Attenuation dynamic range is insufficient (f = "
			   "%g MHz), using %f db instead of %f db.\n",
			   freq * 1.0e-6, MAX_ATTEN, att );
		att = MAX_ATTEN;
	}
	if ( att > rs_sml01.min_attenuation )
	{
		print( SEVERE, "Attenuation dynamic range is insufficient (f = "
			   "%g MHz), using %f db instead of %f db.\n",
			   freq * 1.0e-6, rs_sml01.min_attenuation, att );
		att = rs_sml01.min_attenuation;
	}

	return att;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

unsigned int rs_sml01_get_mod_param( Var_T **v, double *dres, int *ires )
{
	const char *type[ ] =   { "FM", "AM", "PHASE", "OFF" },
			   *source[ ] = { "EXT AC", "AC", "EXT DC", "DC", "INT" };



	/* If the variable is an integer of floating value this means an
	   amplitude setting */

	if ( (*v)->type & ( INT_VAR | FLOAT_VAR ) )
	{
		*dres = get_double( *v, "modulation amplitude" );
		return 0;
	}

	if ( (*v)->type !=  STR_VAR )
	{
		print( FATAL, "Invalid argument, expecting string argument.\n" );
		THROW( EXCEPTION );
	}

	switch ( is_in( (*v)->val.sptr, type, NUM_MOD_TYPES ) )
	{
		case 0 :
			*ires = MOD_TYPE_FM;
			return 1;

		case 1 :
			*ires = MOD_TYPE_AM;
			return 1;

		case 2 :
			*ires = MOD_TYPE_PM;
			return 1;

		case 3 :
			*ires = MOD_TYPE_OFF;
			return 1;
	}

	switch ( is_in( (*v)->val.sptr, source,
					sizeof source / sizeof source[ 0 ] ) )
	{
		case 0 : case 1 :
			*ires = MOD_SOURCE_AC;
			return 2;

		case 2 : case 3 :
			*ires = MOD_SOURCE_DC;
			return 2;

		case 4 :
			*ires = MOD_SOURCE_INT;
			if ( ( *v = vars_pop( *v ) ) == NULL || 
				 ! ( (*v)->type & ( INT_VAR | FLOAT_VAR ) ) )
			{
				print( FATAL, "Argument setting to internal modulation must "
					   "be immediately followed by the modulation "
					   "frequency.\n" );
				THROW( EXCEPTION );
			}

			*dres = get_double( *v, "modulation frequency" );
			return 2;
	}

	print( FATAL, "Invalid parameter \"%s\".\n", (*v)->val.sptr );
	THROW( EXCEPTION );

	return 4;               /* we're never going to get here... */
}


/*---------------------------------------------------------------------*
 * Function for checking that the modulation amplitude is still within
 * the allowed range for a certain frequency. If the modulation ampli-
 * tude is too large it is reduced to the maximum possible value and a
 * warning is printed out.
 *---------------------------------------------------------------------*/

void rs_sml01_check_mod_ampl( double freq )
{
	size_t i;


	/* Now checks required if modulation is off or if AM modulation is used */

	if ( rs_sml01.mod_type == MOD_TYPE_AM ||
		 rs_sml01.mod_type == MOD_TYPE_OFF )
		return;

	if ( rs_sml01.mod_type == MOD_TYPE_FM )
	{
		for ( i = 0; i < num_fm_mod_ranges; i++ )
			if ( freq <= fm_mod_ranges[ i ].upper_limit_freq )
				break;

		fsc2_assert( i < num_fm_mod_ranges );

		if ( rs_sml01.mod_ampl[ rs_sml01.mod_type ] >
			 								   fm_mod_ranges[ i ].upper_limit )
		{
			print( SEVERE, "Current FM modulation amplitude of %.1f kHz is "
				   "is too high for new RF frequency of %.4f MHz, reducing "
				   "it to %.1f kHz.\n",
				   rs_sml01.mod_ampl[ rs_sml01.mod_type ] * 1.0e-3,
				   freq * 1.0e-6,  fm_mod_ranges[ i ].upper_limit * 1.0e-3 );

			if ( FSC2_MODE == EXPERIMENT )
				rs_sml01_set_mod_ampl( rs_sml01.mod_type,
									  rs_sml01.mod_ampl[ rs_sml01.mod_type ] );

			rs_sml01.mod_ampl[ rs_sml01.mod_type ] =
												fm_mod_ranges[ i ].upper_limit;
		}
	}
	else
	{
		for ( i = 0; i < num_pm_mod_ranges; i++ )
			if ( freq <= pm_mod_ranges[ i ].upper_limit_freq )
				break;

		fsc2_assert( i < num_pm_mod_ranges );

		if ( rs_sml01.mod_ampl[ rs_sml01.mod_type ] >
			 								   pm_mod_ranges[ i ].upper_limit )
		{
			print( SEVERE, "Current phase modulation amplitude of %.2f rad is "
				   "is too high for new RF frequency of %.4f MHz, reducing "
				   "it to %.2f rad.\n",
				   rs_sml01.mod_ampl[ rs_sml01.mod_type ],
				   freq * 1.0e-6,  pm_mod_ranges[ i ].upper_limit );

			if ( FSC2_MODE == EXPERIMENT )
				rs_sml01_set_mod_ampl( rs_sml01.mod_type,
									  rs_sml01.mod_ampl[ rs_sml01.mod_type ] );

			rs_sml01.mod_ampl[ rs_sml01.mod_type ] =
												pm_mod_ranges[ i ].upper_limit;
		}
	}
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

#if defined WITH_PULSE_MODULATION

char *rs_sml01_pretty_print( double t )
{
	static char ts[ 30 ];

	if ( t >= 1.0 )
		sprintf( ts, "%.8f s", t );
	else if ( t >= 1.0e-3 )
		sprintf( ts, "%.5f ms", t * 1.0e3 );
	else if ( t >= 1.0e-6 )
		sprintf( ts, "%.2f us", t * 1.0e6);
	else
		sprintf( ts, "%d ns", irnd( t * 1.0e9 ) );

	return ts;
}

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
