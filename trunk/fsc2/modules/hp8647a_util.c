/*
  $Id$
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
		new_name = get_string(   strlen( getenv( "HOME" ) )
							   + strlen( *name ) + 1 );
		strcpy( new_name, getenv( "HOME" ) );
		if ( ( *name )[ 1 ] != '/' )
			strcat( new_name, "/" );
		strcat( new_name, *name + 1 );
		T_free( *name );
		*name = new_name;
	}

	/* Now try to open the file - set table_file entry in the device structure
	   to the name and return the file pointer */

	if ( ( tfp = hp8647a_open_table( *name ) ) != NULL )
	{
		hp8647a.table_file = get_string_copy( *name );
		return tfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( *name, '/' ) != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Table file `%s' not found.\n",
				Fname, Lc, *name );
		THROW( EXCEPTION );
	}

	/* Last chance: The table file is in the library directory... */

	new_name = get_string( strlen( libdir ) + strlen( *name ) );
	strcpy( new_name, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( new_name, "/" );
	strcat( new_name, *name );
	T_free( *name );
	*name = new_name;

	if ( ( tfp = hp8647a_open_table( *name ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Table file `%s' not found, neither in the "
				"current dirctory nor in `%s'.\n", Fname, Lc,
				strrchr( *name, '/' ) + 1, libdir );
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


	if ( access( hp8647a.table_file, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		T_free( name );
		eprint( FATAL, "%s:%ld: %s: No read permission for table file "
					"`%s'.\n", Fname, Lc, DEVICE_NAME, name );
		THROW( EXCEPTION )
	}

	if ( ( tfp = fopen( hp8647a.table_file, "r" ) ) == NULL )
	{
		T_free( name );
		eprint( FATAL, "%s:%ld: %s: Can't open table file `%s'.\n", Fname, Lc,
				DEVICE_NAME, name );
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
		eprint( WARN, "%s:%ld: %s: Frequency of %f MHz not covered by table, "
				"interpolating.\n", Fname, Lc, DEVICE_NAME, freq * 1.0e-6 );
		return hp8647a.att_table[ 0 ].att;
	}

	if ( freq > hp8647a.max_table_freq )
	{
		eprint( WARN, "%s:%ld: %s: Frequency of %f MHz not covered by table, "
				"interpolating.\n", Fname, Lc, DEVICE_NAME, freq * 1.0e-6 );
		return hp8647a.att_table[ hp8647a.att_table_len - 1 ].att;
	}

	/* Find the indices of the two entries in the table (that has been sorted
	   in ascending order of frequencies) bracketing the frequency. Since in
	   most cases the frequencies in the table are equally spaced, we first
	   try to find the entry by interpolating. If this doesn't succeeds the
	   frequencies can't be uniformly spaced and we used simple bisecting.*/

	i_low = 0;
	i_high = hp8647a.att_table_len - 1;

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

	att =   hp8647a.attenuation - hp8647a_get_att_from_table( freq )
		  + hp8647a.att_at_ref_freq;

	if ( att < MAX_ATTEN )
	{
		if ( ! TEST_RUN && I_am == PARENT )
			eprint( SEVERE, "%s: Attenuation dynamic range is insufficient "
					"(f = %g MHz), using %f dB instead of %f dB.\n",
					DEVICE_NAME, freq * 1.0e-6, MAX_ATTEN, att );
		else
			eprint( SEVERE, "%s:%ld: %s: Attenuation dynamic range is "
					"insufficient (f = %g MHz) , using %f dB instead of "
					"%f dB.\n", Fname, Lc, DEVICE_NAME, freq * 1.0e-6,
					MAX_ATTEN, att );
		att = MAX_ATTEN;
	}
	if ( att > MIN_ATTEN )
	{
		if ( ! TEST_RUN && I_am == PARENT )
			eprint( SEVERE, "%s: Attenuation dynamic range is insufficient "
					"(f = %g MHz), using %f dB instead of %f dB.\n",
					DEVICE_NAME, freq * 1.0e-6, MIN_ATTEN, att );
		else
			eprint( SEVERE, "%s:%ld: %s: Attenuation dynamic range is "
					"insufficient (f = %g MHz) , using %f dB instead of "
					"%f dB.\n", Fname, Lc, DEVICE_NAME, freq * 1.0e-6,
					MIN_ATTEN, att );
		att = MIN_ATTEN;
	}

	return att;
}



int hp8647a_set_mod_param( Var *v, double *dres, int *ires )
{
	const char *type[ ] =   { "FM", "AM", "PHASE" },
		       *source[ ] = { "EXT AC", "AC", "EXT DC", "DC",
							  "INT 1kHz", "INT 1 kHz", "INT 1", "1kHz",
							  "1 kHz", "1", "INT 400Hz", "INT 400 Hz",
							  "INT 400", "400Hz", "400 Hz", "400" };


	/* If the variable is an integer of floating value this means a amplitude
	   setting */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			eprint( WARN, "%s:%ld: %s: Integer value used as modulation "
					"amplitude.\n", Fname, Lc, DEVICE_NAME );
		*dres = VALUE( v );
		return 1;
	}

	/* Otherwise its got to be a string */

	vars_check( v, STR_VAR );

	switch ( is_in( v->val.sptr, type, 3 ) )
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

	eprint( FATAL, "%s:%ld: %s: Invalid parameter \"%s\" in call of "
			"function `synthesizer_modulation'.\n", Fname, Lc, DEVICE_NAME,
			v->val.sptr );
	THROW( EXCEPTION );

	return -1;               /* we're never going to get here... */
}
