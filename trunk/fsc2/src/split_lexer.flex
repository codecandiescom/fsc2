/*
   $Id$
*/


/**********************************************/
/* This is the main lexer for EDL input files */
/**********************************************/


%option noyywrap case-sensitive nounput

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( splitin );                              \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}


#include "fsc2.h"


/* local functions */

static int splitlex( void );
static bool section_parser( int section );


%}

FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[\t ]*DEV(ICE)?S?:
ASS         ^[\t ]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[\t ]*VAR(IABLE)?S?:
PHAS        ^[\t ]*PHA(SES)?:
PREP        ^[\t ]*PREP(ARATION)?S?:
EXP         ^[\t ]*EXP(ERIMENT)?:


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/

			/* handling of file name lines */
{FILE}      {
				*( splittext + splitleng - 1 ) = '\0';
				T_free( Fname );
				Fname = get_string_copy( splittext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( splittext + splitleng - 1 ) = '\0';
				Lc = atol( splittext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				printf( "%s", splittext + 2 );
				return FAIL;
			}

			/* handling of DEVICES: label */
{DEV}		{
				if ( ! section_parser( DEVICES_SECTION ) )
					return FAIL;
			}

			/* handling of ASSIGNMENTS: label */
{ASS}		{
				if ( ! section_parser( ASSIGNMENTS_SECTION ) )
					return FAIL;
			}

			/* handling of VARIABLES: label */
{VAR}		{
				if ( ! section_parser( VARIABLES_SECTION ) )
					return FAIL;
			}

			/* handling of PHASES: label */
{PHAS}      {
				if ( ! section_parser( PHASES_SECTION ) )
					return FAIL;
			}

			/* handling of PREPARATIONS: label */
{PREP}      {
				if ( ! section_parser( PREPARATIONS_SECTION ) )
					return FAIL;
			}

			/* handling of EXPERIMENT: label */
{EXP}       {
				if ( ! section_parser( EXPERIMENT_SECTION ) )
					return FAIL;
			}

			/* handling of unknown label */
.+":"       {
				eprint( FATAL, "%s:%ld: Unknown label: `%s'.\n",
						Fname, Lc, splittext );
				return FAIL;
			}

[\t \n]+    /* dump white space */

			/* handling of invalid input */
.   		{
				eprint( FATAL, "%s:%ld: Missing section label at start of "
							   "file.\n", Fname, Lc );
				return FAIL;
			}

<<EOF>>     return OK;


		/*----------------------*/
%%		/*     End of rules     */
		/*----------------------*/


bool scan_main( char *file )
{
	char *cmd;
	bool split_error;


	/* Forget everything about previous tests */

	clean_up( );

	/* Parse the function and the device name data base */

	if ( ! functions_init( ) || ! device_list_parse( ) )
		return FAIL;

	/* Check that the file name is reasonable */

	if ( file == NULL || *file == '\0' )
	{
		printf( "Missing input file.\n" );
		return FAIL;
	}

	/* Filter input through "fsc2_clean" (must be located in `libdir') */

	cmd = get_string( strlen( libdir ) + strlen( "/fsc2_clean " )
	                  + strlen( file ) );
	strcpy( cmd, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( cmd, "/" );
	strcat( cmd, "fsc2_clean " );
	strcat( cmd, file );

	signal( SIGCHLD, SIG_IGN );

	splitin = popen( cmd, "r" );

	/* Now try to parse the cleaned up contents */

	split_error = splitlex( );

	pclose( splitin );

	signal( SIGCHLD, main_sig_handler );

	T_free( cmd );

	return split_error;
}


/*--------------------------------------------------------------------*/
/* This routine is the central distributor for starting the different */
/* parsers for all of the sections in the EDL file. It is called once */
/* from splitlex(), which just finds the very first section label and */
/* passes it to this routine. Here the appropriate lexer is called,   */
/* which returns the next label if it finishes successfully. The new  */
/* label is used to call the next lexer. On failure, several common   */
/* errors are caught (if the lexer didn't do it by itself) and the    */
/* failure is signaled back to splitlex(). On success we only return  */
/* to splitlex() on end of the input file(s).                         */
/* ->                                                                 */
/*    * number (label) of first section found by splitlex()           */
/* <-                                                                 */
/*    * flag for success (OK) or failure (FAIL)                       */
/*--------------------------------------------------------------------*/

bool section_parser( int section )
{
	TRY 
	{
		do
		{
			/* First run the appropriate lexer/parser combination */

			switch ( section )
			{
				case DEVICES_SECTION :
					section = devices_parser( splitin );
					break;
	
				case ASSIGNMENTS_SECTION :
					section = assignments_parser( splitin );
					break;
	
				case VARIABLES_SECTION :
					section = variables_parser( splitin );
					break;
	
				case PHASES_SECTION :
					section = phases_parser( splitin );
					break;
	
				case PREPARATIONS_SECTION :
					section = preparations_parser( splitin );
					break;
	
				case EXPERIMENT_SECTION :
					section = experiment_parser( splitin );
					break;
	
				default :              /* this should never happen, but ... */
					assert( 1 == 0 );
			}
	
		} while ( section != NO_SECTION );

		/* Finally, we have to do all the checks that only can be done
		   after the EDL file has been completely parsed */

		TRY_SUCCESS;
		return OK;

	}
	CATCH( SYNTAX_ERROR_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Syntax error.\n", Fname, Lc ); 
		return FAIL;
	}
	CATCH( MISSING_SEMICOLON_EXCEPTION )
	{		
		eprint( FATAL, "%s:%ld: Missing semicolon before (or on) this line.\n",
				Fname, Lc );
		return FAIL;
	}
	OTHERWISE
		return FAIL;

	return FAIL;
}
