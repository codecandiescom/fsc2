/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


#include "hp8672a.h"



/*----------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the table file */
/* and sets the table_file entry in the device structure to the name of */
/* the table file. Otherwise an exception is thrown. In any case the    */
/* memory used for the file name passed to the function is deallocated. */
/*----------------------------------------------------------------------*/

FILE *hp8672a_find_table( char **name )
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

	if ( ( tfp = hp8672a_open_table( *name ) ) != NULL )
	{
		hp8672a.table_file = T_strdup( *name );
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

	if ( ( tfp = hp8672a_open_table( *name ) ) == NULL )
	{
		print( FATAL, "Table file '%s' not found in either the current "
			   "directory or in '%s'.\n", strip_path( *name ), libdir );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*------------------------------------------------------------------*/
/* Tries to open the file with the given name and returns the file  */
/* pointer, returns NULL if file does not exist returns, or throws  */
/* an exception if the file can't be read (either because of prob-  */
/* lems with the permissions or other, unknoen reasons). If opening */
/* the file fails with an exception memory used for its name is     */
/* deallocated.                                                     */
/*------------------------------------------------------------------*/

FILE *hp8672a_open_table( char *name )
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


/*-----------------------------------------------*/
/* Calculates from the table the attenuation for */
/* a given frequency by interpolation            */
/*-----------------------------------------------*/

double hp8672a_get_att_from_table( double freq )
{
	long i_low, i_high, i_cur;
	double att;


	/* First check that the frequency is covered by the table, if it isn't
	   print out a warning and return attenuation for the smallest or
	   highest frequency */

	if ( freq < hp8672a.min_table_freq )
	{
		print( WARN, "Frequency of %f MHz not covered by table, "
			   "interpolating.\n", freq * 1.0e-6 );
		return hp8672a.att_table[ 0 ].att;
	}

	if ( freq > hp8672a.max_table_freq )
	{
		print( WARN, "Frequency of %f MHz not covered by table, "
			   "interpolating.\n", freq * 1.0e-6 );
		return hp8672a.att_table[ hp8672a.att_table_len - 1 ].att;
	}

	/* Find the indices of the two entries in the table (that has been sorted
	   in ascending order of frequencies) bracketing the frequency. Since in
	   most cases the frequencies in the table are equally spaced, we first
	   try to find the entry by interpolating. If this doesn't succeeds the
	   frequencies can't be uniformly spaced and we used simple bisecting.*/

	i_low = 0;
	i_high = hp8672a.att_table_len - 1;

	if ( freq == hp8672a.att_table[ i_low ].freq )
		return hp8672a.att_table[ i_low ].att;
	if ( freq == hp8672a.att_table[ i_high ].freq )
		return hp8672a.att_table[ i_high ].att;

	i_cur = lrnd( floor( ( i_high - i_low ) *
						 ( freq - hp8672a.att_table[ i_low ].freq ) /
						 ( hp8672a.att_table[ i_high ].freq
						   - hp8672a.att_table[ i_low ].freq ) ) ) + i_low;

	if ( freq > hp8672a.att_table[ i_cur ].freq )
	{
		i_low = i_cur;
		if ( freq < hp8672a.att_table[ i_cur + 1 ].freq )
			i_high = i_cur + 1;
	}
	else
		i_high = i_cur;

	while ( i_high - i_low > 1 )
	{
		if ( freq > hp8672a.att_table[ i_cur ].freq )
			i_low = i_cur;
		else
			i_high = i_cur;
		i_cur = ( i_high + i_low ) >> 1;
	}

	/* Now do a linear interpolation for the attenuation between the bracketing
	   frequencies we have data for */

	att =  ( hp8672a.att_table[ i_high ].att - hp8672a.att_table[ i_low ].att )
		 * ( freq - hp8672a.att_table[ i_low ].freq )
		 / (   hp8672a.att_table[ i_high ].freq
			 - hp8672a.att_table[ i_low ].freq )
		 + hp8672a.att_table[ i_low ].att;

	/* Reduce value to the resolution the attenuation can be set with */

	return ATT_RESOLUTION * lrnd( att / ATT_RESOLUTION );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

double hp8672a_get_att( double freq )
{
	double att;

	if ( ! hp8672a.use_table )
		return hp8672a.attenuation;

	att =   hp8672a.attenuation + hp8672a_get_att_from_table( freq )
		  - hp8672a.att_at_ref_freq;

	if ( att < MAX_ATTEN )
	{
		print( SEVERE, "Attenuation dynamic range is insufficient (f = "
			   "%g MHz), using %f db instead of %f db.\n",
			   freq * 1.0e-6, MAX_ATTEN, att );
		att = MAX_ATTEN;
	}
	if ( att > hp8672a.min_attenuation )
	{
		print( SEVERE, "Attenuation dynamic range is insufficient (f = "
			   "%g MHz), using %f db instead of %f db.\n",
			   freq * 1.0e-6, hp8672a.min_attenuation, att );
		att = hp8672a.min_attenuation;
	}

	return att;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int hp8672a_set_mod_param( Var *v, double *dres, int *ires )
{
	const char *type[ ] =   { "FM", "AM", "OFF" };


	/* If the variable is an integer or floating value this means an
	   amplitude setting */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		*dres = VALUE( v );
		return 1;
	}

	/* Otherwise it got to be a string */

	vars_check( v, STR_VAR );

	switch ( is_in( v->val.sptr, type, NUM_MOD_TYPES ) )
	{
		case 0 :
			*ires = MOD_TYPE_FM;
			return 2;

		case 1 :
			*ires = MOD_TYPE_AM;
			return 2;

		case 2 :
			*ires = MOD_TYPE_OFF;
			return 2;
	}

	print( FATAL, "Invalid parameter \"%s\".\n", v->val.sptr );
	THROW( EXCEPTION );

	return -1;               /* we're never going to get here... */
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int hp8672_mod_ampl_check( double ampl )
{
	int i;


	if ( ! hp8672a.mod_type_is_set )
	{
		print( FATAL, "Can't set modulation amplitude as long as "
			   "modulation type hasn't been set.\n" );
		THROW( EXCEPTION );
	}

	if ( hp8672a.mod_type == MOD_TYPE_OFF )
	{
		print( FATAL, "Can't set modulation amplitude as long as modulation "
			   "is switched off.\n" );
		THROW( EXCEPTION );
	}

	if ( ampl <= 0 )
	{
		print( FATAL, "Invalid modulation amplitude (%lf).\n", ampl );
		THROW( EXCEPTION );
	}

	if ( hp8672a.mod_type == MOD_TYPE_AM )
	{
		if ( fabs( ampl - 30.0 ) >= 0.5 && fabs( ampl - 100.0 ) >= 0.5 )
		{
			print( FATAL, "Amplitude modulation range can only be set to "
				   "either 30 % or 100 % but not %lf %.\n", ampl );
			THROW( EXCEPTION );
		}

		if ( fabs( ampl - 30.0 ) < 0.5 )
			return 0;
		return 1;
	}

	for ( i = 0; i < FM_AMPL_SETTINGS; i++ )
		if ( fabs( ampl - fm_ampl[ i ] ) / fm_ampl[ i ] <= 1.0e2 )
			return i;

	print( FATAL, "Invalid frequency modulation amplitude of %lf MHz.\n",
		   ampl * 1e-6 );
	THROW( EXCEPTION );

	return -1;                   /* Can never be reached */
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
