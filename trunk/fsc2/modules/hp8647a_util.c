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


#include "hp8647a.h"



/*----------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the table file */
/* and sets the table_file entry in the device structure to the name of */
/* the table file. Otherwise an exception is thrown. In any case the    */
/* memory used for the file name passed to the function is deallocated. */
/*----------------------------------------------------------------------*/

FILE *hp8647a_find_table( char **name )
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

	if ( ( tfp = hp8647a_open_table( *name ) ) != NULL )
	{
		hp8647a.table_file = T_strdup( *name );
		return tfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( *name, '/' ) != NULL )
	{
		eprint( FATAL, UNSET, "%s: Table file `%s' not found.\n",
				DEVICE_NAME, *name );
		THROW( EXCEPTION );
	}

	/* Last chance: The table file is in the library directory... */

	new_name = get_string( "%s%s%s", libdir, slash( libdir ), *name );
	T_free( *name );
	*name = new_name;

	if ( ( tfp = hp8647a_open_table( *name ) ) == NULL )
	{
		eprint( FATAL, UNSET, "%s: Table file `%s' not found, neither in the "
				"current dirctory nor in `%s'.\n", DEVICE_NAME,
				strip_path( *name ), libdir );
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

FILE *hp8647a_open_table( char *name )
{
	FILE *tfp;


	if ( access( name, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		eprint( FATAL, UNSET, "%s: No read permission for table file `%s'.\n",
				DEVICE_NAME, name );
		T_free( name );
		THROW( EXCEPTION )
	}

	if ( ( tfp = fopen( name, "r" ) ) == NULL )
	{
		eprint( FATAL, UNSET, "%s: Can't open table file `%s'.\n",
				DEVICE_NAME, name );
		T_free( name );
		THROW( EXCEPTION );
	}

	return tfp;
}


/*-----------------------------------------------*/
/* Calculates from the table the attenuation for */
/* a given frequency by interpolation            */
/*-----------------------------------------------*/

double hp8647a_get_att_from_table( double freq )
{
	long i_low, i_high, i_cur;
	double att;


	/* First check that the frequency is covered by the table, if it isn't
	   print out a warning and return attenuation for the smallest or
	   highest frequency */

	if ( freq < hp8647a.min_table_freq )
	{
		eprint( WARN, SET, "%s: Frequency of %f MHz not covered by table, "
				"interpolating.\n", DEVICE_NAME, freq * 1.0e-6 );
		return hp8647a.att_table[ 0 ].att;
	}

	if ( freq > hp8647a.max_table_freq )
	{
		eprint( WARN, SET, "%s: Frequency of %f MHz not covered by table, "
				"interpolating.\n", DEVICE_NAME, freq * 1.0e-6 );
		return hp8647a.att_table[ hp8647a.att_table_len - 1 ].att;
	}

	/* Find the indices of the two entries in the table (that has been sorted
	   in ascending order of frequencies) bracketing the frequency. Since in
	   most cases the frequencies in the table are equally spaced, we first
	   try to find the entry by interpolating. If this doesn't succeeds the
	   frequencies can't be uniformly spaced and we used simple bisecting.*/

	i_low = 0;
	i_high = hp8647a.att_table_len - 1;

	if ( freq == hp8647a.att_table[ i_low ].freq )
		return hp8647a.att_table[ i_low ].att;
	if ( freq == hp8647a.att_table[ i_high ].freq )
		return hp8647a.att_table[ i_high ].att;

	i_cur = floor( ( i_high - i_low ) *
				   ( freq - hp8647a.att_table[ i_low ].freq ) /
				   (   hp8647a.att_table[ i_high ].freq 
					 - hp8647a.att_table[ i_low ].freq ) ) + i_low;

	if ( freq > hp8647a.att_table[ i_cur ].freq )
	{
		i_low = i_cur;
		if ( freq < hp8647a.att_table[ i_cur + 1 ].freq )
			i_high = i_cur + 1;
	}
	else
		i_high = i_cur;

	while ( i_high - i_low > 1 )
	{
		if ( freq > hp8647a.att_table[ i_cur ].freq )
			i_low = i_cur;
		else
			i_high = i_cur;
		i_cur = ( i_high + i_low ) >> 1;
	}

	/* Now do a linear interpolation for the attenuation between the bracketing
	   frequencies we have data for */

	att =  ( hp8647a.att_table[ i_high ].att - hp8647a.att_table[ i_low ].att )
		 * ( freq - hp8647a.att_table[ i_low ].freq )
		 / (   hp8647a.att_table[ i_high ].freq
			 - hp8647a.att_table[ i_low ].freq )
		 + hp8647a.att_table[ i_low ].att;

	/* Reduce value to the resolution the attenuation can be set with */

	return ATT_RESOLUTION * lround( att / ATT_RESOLUTION );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

double hp8647a_get_att( double freq )
{
	double att;

	if ( ! hp8647a.use_table )
		return hp8647a.attenuation;

	att =   hp8647a.attenuation + hp8647a_get_att_from_table( freq )
		  - hp8647a.att_at_ref_freq;

	if ( att < MAX_ATTEN )
	{
		if ( ! TEST_RUN && I_am == PARENT )
			eprint( SEVERE, UNSET, "%s: Attenuation dynamic range is "
					"insufficient (f = %g MHz), using %f db instead of %f "
					"db.\n", DEVICE_NAME, freq * 1.0e-6, MAX_ATTEN, att );
		else
			eprint( SEVERE, SET, "%s: Attenuation dynamic range is "
					"insufficient (f = %g MHz) , using %f db instead of "
					"%f db.\n", DEVICE_NAME, freq * 1.0e-6, MAX_ATTEN, att );
		att = MAX_ATTEN;
	}
	if ( att > hp8647a.min_attenuation )
	{
		if ( ! TEST_RUN && I_am == PARENT )
			eprint( SEVERE, UNSET, "%s: Attenuation dynamic range is "
					"insufficient (f = %g MHz), using %f db instead of %f "
					"db.\n", DEVICE_NAME, freq * 1.0e-6,
					hp8647a.min_attenuation, att );
		else
			eprint( SEVERE, SET, "%s: Attenuation dynamic range is "
					"insufficient (f = %g MHz) , using %f db instead of "
					"%f db.\n", DEVICE_NAME, freq * 1.0e-6,
					hp8647a.min_attenuation, att );
		att = hp8647a.min_attenuation;
	}

	return att;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int hp8647a_set_mod_param( Var *v, double *dres, int *ires )
{
	const char *type[ ] =   { "FM", "AM", "PHASE", "OFF" },
		       *source[ ] = { "EXT AC", "AC", "EXT DC", "DC",
							  "INT 1kHz", "INT 1 kHz", "INT 1", "1kHz",
							  "1 kHz", "1", "INT 400Hz", "INT 400 Hz",
							  "INT 400", "400Hz", "400 Hz", "400" };


	/* If the variable is an integer of floating value this means an
	   amplitude setting */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			eprint( WARN, SET, "%s: Integer value used as modulation "
					"amplitude.\n", DEVICE_NAME );
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
			*ires = MOD_TYPE_PHASE;
			return 2;

		case 3 :
			*ires = MOD_TYPE_OFF;
			return 2;
	}

	switch ( is_in( v->val.sptr, source, 16 ) )
	{
		case 0 : case 1 :
			*ires = MOD_SOURCE_AC;
			return 3;

		case 2 : case 3 :
			*ires = MOD_SOURCE_DC;
			return 3;

		case 4 : case 5 : case 6 : case 7 : case 8 : case 9 :
			*ires = MOD_SOURCE_1k;
			return 3;

		case 10 : case 11 : case 12 : case 13 : case 14 : case 15 :
			*ires = MOD_SOURCE_400;
			return 3;
	}

	eprint( FATAL, SET, "%s: Invalid parameter \"%s\" in call of "
			"function `synthesizer_modulation'.\n", DEVICE_NAME, v->val.sptr );
	THROW( EXCEPTION );

	return -1;               /* we're never going to get here... */
}
