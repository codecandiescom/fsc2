/*
  $Id$
*/

/************************************************************/
/* This is the lexer for the DEVICES section of an EDL file */
/************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive nounput

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( devicesin );                            \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "devices_parser.h"


int deviceslex( void );
extern void devicesparse( void );


/* locally used global variables */

int Devices_Next_Section;

%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*ASS(IGNMENT)?S?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

IDENT       [A-Za-z0-9]+

WS          [\n=:; ]+



		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( devicestext + devicesleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( devicestext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( devicestext + devicesleng - 1 ) = '\0';
				Lc = atol( devicestext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );

{ESTR}		{
				devicestext = strchr( devicestext, '\x03' );
				THROW( CLEANER_EXCEPTION );
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Devices_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Devices_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Devices_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Devices_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Devices_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Devices_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

{IDENT}     return DEV_TOKEN;


{WS}        /* skip prettifying characters */

			/* handling of invalid input (i.e. everything else) */
.           THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>	    {
				Devices_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int devices_parser( FILE *in )
{
	if ( compilation.sections[ DEVICES_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of DEVICES section "
		        "label.\n", Fname, Lc );
		return FAIL;
	}
	compilation.sections[ DEVICES_SECTION ] = SET;

	Devices_Next_Section = OK;

	clear_device_list( );
	devicesin = in;

	TRY
	{
		device_list_parse( );	
		devicesparse( );
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in DEVICES section: "
				"`%s'\n", Fname, Lc, devicestext );
		clear_device_list( );
		return FAIL;
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", devicestext + 2 );
		clear_device_list( );
		return FAIL;
	}
	CATCH( DEVICES_EXCEPTION )
	{
		clear_device_list( );
		return( FAIL );
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
	{
		clear_device_list( );
		return( FAIL );
	}

	return Devices_Next_Section;
}
