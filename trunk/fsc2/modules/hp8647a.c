/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


#define DEVICE_NAME "HP8647A"      /* compare entry in /etc/gpib.conf ! */

#define MIN_FREQ  2.5e5            /* 250 kHz  */
#define MAX_FREQ  1.0e9            /* 1000 MHz */
#define MIN_ATTEN 10.0             /* +10 db   */
#define MAX_ATTEN -136.0           /* -136 db  */

#define DEFAULT_TABLE_FILE "hp8647a.table"     /* libdir will be prepended ! */



/* declaration of exported functions */

int hp8647a_init_hook( void );
int hp8647a_test_hook( void );
int hp8647a_end_of_test_hook( void );
int hp8647a_exp_hook( void );
int hp8647a_end_of_exp_hook( void );
void hp8647a_exit_hook( void );


Var *synthesizer_set_frequency( Var *v );
Var *synthesizer_set_step_frequency( Var *v );
Var *synthesizer_get_frequency( Var *v );
Var *synthesizer_sweep_up( Var *v );
Var *synthesizer_sweep_down( Var *v );
Var *synthesizer_reset_frequency( Var *v );
Var *synthesizer_use_table( Var *v );



typedef struct {
	double freq;
	double att;
} ATT_TABLE_ENTRY;

typedef struct
{
	int device;

	double start_freq;
	bool SF;               /* start frequency needs setting ? */
	double freq_step;
	bool FS;               /* frequency steps needs setting ? */

	char *table_file;      /* name of attenuation table file */
	bool use_table;
	ATT_TABLE_ENTRY *att_table;

	double freq;
} HP8647A;


static HP8647A hp8647a;

static bool hp8647a_init( const char *name );
static double hp8647a_set_frequency( double freq );
static double hp8647a_get_frequency( void );
static bool hp8647a_read_table( FILE *fp );
FILE *hp8647a_find_table( char *name );
FILE *hp8647a_open_table( char *name );


/*******************************************/
/*   the hook functions...                 */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int hp8647a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	hp8647a.device = -1;
	hp8647a.freq = -1.0;         /* mark as unset yet */
	hp8647a.SF = UNSET;
	hp8647a.freq_step = 0.0;
	hp8647a.FS = SET;

	hp8647a.table_file = NULL;
	hp8647a.use_table = UNSET;
	hp8647a.att_table = NULL;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_test_hook( void )
{
	if ( ! hp8647a.SF )
	{
		eprint( FATAL, "%s: Start frequency has not been set.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}
	hp8647a.freq = hp8647a.start_freq;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int hp8647a_end_of_test_hook( void )
{
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int hp8647a_exp_hook( void )
{
	if ( ! hp8647a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed.", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int hp8647a_end_of_exp_hook( void )
{
//	hp8647a_finished( );

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void hp8647a_exit_hook( void )
{
	if ( hp8647a.table_file )
		T_free( hp8647a.table_file );
	if ( hp8647a.att_table )
		T_free( hp8647a.att_table );
}




Var *synthesizer_set_frequency( Var *v )
{
	double freq;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as frequency in call "
				"of function `synthesizer_set_frequency'.", Fname, Lc,
				DEVICE_NAME );

	freq = VALUE( v );

	if ( freq < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative frequency in call of "
				"function `synthesizer_set_frequency'.",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( freq < MIN_FREQ || freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency (%f Hz) not within valid range "
				"of %f kHz to %g Mhz.", Fname, Lc, DEVICE_NAME,
				1.0e-3 * MIN_FREQ, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	
	if ( TEST_RUN )                      /* In test run of experiment */
		hp8647a.freq = freq;
	else if ( I_am == PARENT )           /* in PREPARATIONS section */
	{
		hp8647a.start_freq = freq;
		hp8647a.SF = SET;
	}
	else
		hp8647a.freq = hp8647a_set_frequency( freq );

	return vars_push( FLOAT_VAR, freq );
}


Var *synthesizer_set_step_frequency( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer variable used as step frequency in "
				"call of function `synthesizer_set_step_frequency'.", Fname,
				Lc, DEVICE_NAME );

	hp8647a.freq_step = VALUE( v );

	return vars_push( FLOAT_VAR, hp8647a.freq_step );
}


/*
  This function may only be called in the EXPERIMENT section!
*/


Var *synthesizer_get_frequency( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, hp8647a_get_frequency( ) );
}


/*
  This function may only get called in the EXPERIMENT section!
*/

Var *synthesizer_sweep_up( Var *v )
{
	double new_freq;


	v = v;

	if ( hp8647a.freq_step == 0.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Step frequency hasn't been set.",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	new_freq = hp8647a.freq + hp8647a.freq_step;

	if ( new_freq < MIN_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting below lower limit of "
				"%f kHz.", Fname, Lc, DEVICE_NAME, 1.0e-3 * MIN_FREQ );
		THROW( EXCEPTION );
	}

	if ( new_freq > MAX_FREQ )
	{
		eprint( FATAL, "%s:%ld: %s: Frequency is getting above upper limit of "
				"%f MHz.", Fname, Lc, DEVICE_NAME, 1.0e-6 * MAX_FREQ );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		hp8647a.freq = new_freq;
	else
		hp8647a.freq = hp8647a_set_frequency( new_freq );

	return vars_push( FLOAT_VAR, hp8647a.freq );
}


/*
  This function may only get called in the EXPERIMENT section!
*/

Var *synthesizer_sweep_down( Var *v )
{
	Var *nv;


	v = v;
	hp8647a.freq_step *= -1.0;
	nv = synthesizer_sweep_down( NULL );
	hp8647a.freq_step *= -1.0;
	return nv;
}


/*
  This function may only get called in the EXPERIMENT section!
*/

Var *synthesizer_reset_frequency( Var *v )
{
	v = v;

	if ( TEST_RUN )
		hp8647a.freq = hp8647a.start_freq;
	else
		hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );

	return vars_push( FLOAT_VAR, hp8647a.freq );
}



Var *synthesizer_use_table( Var *v )
{
	FILE *tfp;
	char *tfname;


	/* Try to figure out the name of the table file - if no argument is given
	   use the default table file, otherwise use the user supplied file name */

	if ( v == NULL )
	{
		hp8647a.table_file = get_string( strlen( libdir ) +
										 strlen( DEFAULT_TABLE_FILE ) + 2 );
		strcpy( hp8647a.table_file, libdir );
		if ( libdir[ strlen( libdir ) - 1 ] != '/' )
			strcat( hp8647a.table_file, "/" );
		strcat( hp8647a.table_file, DEFAULT_TABLE_FILE );

		if ( ( tfp = hp8647a_open_table( hp8647a.table_file ) ) == NULL )
		{
			eprint( FATAL, "%s:%ld: %s: Default table file `%s' not found.",
					Fname, Lc, DEVICE_NAME, hp8647a.table_file );
			T_free( hp8647a.table_file );
			THROW( EXCEPTION );
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		tfname = get_string_copy( v->val.sptr );
		v = vars_pop( v );

		if ( v != NULL )
		{
			eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of "
					"function `synthesizer_use_table'.",
					Fname, Lc, DEVICE_NAME );
			while ( ( v = vars_pop( v ) ) != NULL )
				;
		}

		tfp = hp8647a_find_table( tfname );
	}

	hp8647a_read_table( tfp );
	T_free( hp8647a.table_file );
	hp8647a.table_file = NULL;
	return vars_push( INT_VAR, 1 );
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


static bool hp8647a_init( const char *name )
{
	if ( gpib_init_device( name, &hp8647a.device ) == FAILURE )
        return FAIL;

	/* If the start frequency needs to be set do it now, otherwise get the
	   frequency set at the synthesier and store it */

	if ( hp8647a.SF )
		hp8647a.freq = hp8647a_set_frequency( hp8647a.start_freq );
	else
		hp8647a.start_freq = hp8647a.freq = hp8647a_get_frequency( );

	return OK;
}


static double hp8647a_set_frequency( double freq )
{
	char cmd[ 100 ] = "FREQ:CW ";


	assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd + strlen( cmd ), "%f", freq );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the synthesizer.",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return freq;
}


static double hp8647a_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;

	if ( gpib_write( hp8647a.device, "FREQ:CW?", 8 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the synthesizer.",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}


	return T_atof( buffer );
}


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
	   eprint( FATAL, "%s:%ld: %s: Table file `%s' not found.",
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
				"current dirctory nor in `%s'.", Fname, Lc,
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
					"`%s'.", Fname, Lc, DEVICE_NAME, name );
		THROW( EXCEPTION )
	}

	if ( ( tfp = fopen( hp8647a.table_file, "r" ) ) == NULL )
	{
		T_free( name );
		eprint( FATAL, "%s:%ld: %s: Can't open table file `%s'.", Fname, Lc,
				DEVICE_NAME, name );
		THROW( EXCEPTION );
	}

	return tfp;
}


static bool hp8647a_read_table( FILE *fp )
{
	fclose( fp );
	return OK;
}
