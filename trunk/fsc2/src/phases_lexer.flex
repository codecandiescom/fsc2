/*
  $Id$
*/

/***********************************************************/
/* This is the lexer for the PHASES section of an EDL file */
/***********************************************************/


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
	int c = fgetc( phasesin );                             \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "phases_parser.h"

int phaseslex( void );
extern void phasesparse( void );


/* locally used global variables */

int Phases_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ASS         ^[ \t]*ASS(IGNMENT)?S?:
DEF         ^[ \t]*DEF(AULT)?S?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

PS          ^[ \t]*P(HASE)?_?S(EQ(UENCE)?)?_?[0-9]{0,2}
AS          ^[ \t]*A(CQ(UISITION)?)?_?S(EQ(UENCE)?)?_?[XY]?

WS          [\n=,:. ]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( phasestext + phasesleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( phasestext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( phasestext + phasesleng - 1 ) = '\0';
				Lc = atol( phasestext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, "%s", phasestext + 2 );
				return 0;
			}

			/* handling of ASSIGNMENTS: label */
{ASS}		{
				Phases_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of DEFAULTS: label */
{DEF}		{
				Phases_Next_Section = DEFAULTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: label */
{VAR}		{
				Phases_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: label */
{PHAS}		{
				Phases_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: label */
{PREP}		{
				Phases_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: label */
{EXP}		{
				Phases_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

            /* handling of PHASE_SEQUENCE with (optional) sequence number
			   (if sequence number is left out 1 is used as the default) */
{PS}        {
				char *cp = phasestext + phasesleng - 1;

				if ( *cp == '_' )
				    THROW( INVALID_INPUT_EXCEPTION );

				/* determine number of phase sequence (0 to ...)*/

				phaseslval.lval = 0;
				if ( isdigit( *cp ) )
				{
					while ( isdigit( *cp ) )
						cp--;
					phaseslval.lval = atol( ++cp ) - 1;
				}
				return PS_TOKEN;
			}

			/* handling of ACQUISITION_SEQUENCE */
{AS}        {
				char *cp = phasestext + phasesleng - 1;

				if ( *cp == '_' )
				    THROW( INVALID_INPUT_EXCEPTION );

				/* determine type of acquisition sequence (X or Y) */

				phaseslval.lval = 0;
				if ( tolower( *cp ) == 'Y')
					phaseslval.lval = 1;

				return AS_TOKEN;
			}

			/* handling of phase cycle identifiers */
"x"         {
				phaseslval.lval = PHASE_PLUS_X;
				return P_TOKEN;
			}
"+x"        {
				phaseslval.lval = PHASE_PLUS_X;
				return P_TOKEN;
			}
"-x"        {
				phaseslval.lval = PHASE_MINUS_X;
				return P_TOKEN;
			}
"y"         {
				phaseslval.lval = PHASE_PLUS_Y;
				return P_TOKEN;
			}
"+y"        {
				phaseslval.lval = PHASE_PLUS_Y;
				return P_TOKEN;
			}
"-y"        {
				phaseslval.lval = PHASE_MINUS_Y;
				return P_TOKEN;
			}

			/* handling of acquisition cycle identifiers */
"+"        {
				phaseslval.lval = ACQ_PLUS;
				return A_TOKEN;
		   }
"-"        {
				phaseslval.lval = ACQ_MINUS;
				return A_TOKEN;
		   }

{WS}        /* skip prettifying characters */

";"         return ';';              /* end of statement character */

			/* handling of invalid input */
.           THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>	    {
				Phases_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int phases_parser( FILE *in )
{
	if ( compilation.sections[ PHASES_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of PHASES section label.\n",
				Fname, Lc );
		return FAIL;
	}
	compilation.sections[ PHASES_SECTION ] = SET;

	Phases_Next_Section = OK;

	phases_init( );
	phasesin = in;

	TRY
	{
		phasesparse( );
		phases_end( );
	}
	CATCH( PHASES_EXCEPTION )
		return FAIL;
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in PHASES section: `%s'\n",
				Fname, Lc, phasestext );
		return FAIL;
	}

	return Phases_Next_Section;
}
