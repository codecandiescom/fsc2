/*
  $Id$
*/


#include "hp8647a.h"


/*--------------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the table file and */
/* sets the table_file entry in the device structure to the name of the     */
/* table file. Otherwise an exception is thrown. In every case the memory   */
/* used for the file name passed to the function is deallocated.            */
/*--------------------------------------------------------------------------*/

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
