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

FILE *hp8647a_find_table( char *name )
{
	FILE *tfp;
	char *new_name;


	/* Expand a leading tilde to the users home directory */

	if ( name[ 0 ] == '~' )
	{
		new_name = get_string( strlen( getenv( "HOME" ) ) + strlen( name ) );
		strcpy( new_name, getenv( "HOME" ) );
		if ( name[ 1 ] != '/' )
			strcat( new_name, "/" );
		strcat( new_name, name + 1 );
		T_free( name );
		name = new_name;
	}

	/* Now try to open the file - set table_file entry in the device structure
	   to the name and return the file pointer */

	if ( ( tfp = hp8647a_open_table( name ) ) != NULL )
	{
		hp8647a.table_file = name;
		return tfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( name, '/' ) != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Table file `%s' not found.\n",
				Fname, Lc, name );
		T_free( name );
		THROW( EXCEPTION );
	}

	/* Last chance: The table file is in the library directory... */

	new_name = get_string( strlen( libdir ) + strlen( name ) );
	strcpy( new_name, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( new_name, "/" );
	strcat( new_name, name );
	T_free( name );
	name = new_name;

	if ( ( tfp = hp8647a_open_table( name ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Table file `%s' not found, neither in the "
				"current dirctory nor in `%s'.\n", Fname, Lc,
				strrchr( name, '/' ) + 1, libdir );
		T_free( name );
		THROW( EXCEPTION );
	}

	hp8647a.table_file = name;
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
	   in ascending order of frequencies) between which the frequency is
	   laying. In most cases the frequencies in the table will be equally
	   spaced, so we use bisecting employing the mean slope of the current
	   interval for our guesses. This is probably the fasted method for the
	   most common case (value is found after first run through loop) but may
	   be slow in other cases. */

	i_low = 0;
	i_high = hp8647a.att_table_len - 1;
	i_cur = i_low;

	while ( i_high - i_low > 1 )
	{
		i_cur = lround( (   hp8647a.att_table[ i_high ].freq 
						  - hp8647a.att_table[ i_low ].freq )
						/ ( freq - hp8647a.att_table[ i_low ].freq ) ) + i_low;
		if ( freq > hp8647a.att_table[ i_cur ].freq )
			i_low = i_cur;
		else
		{
			i_high = i_cur;
			continue;
		}

		if ( freq < hp8647a.att_table[ i_cur + 1 ].freq )
			i_high = i_cur + 1;
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
