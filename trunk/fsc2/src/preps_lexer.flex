/*****************************************************************/
/* This is the lexer for the PREPARATIONS section of an EDL file */
/*****************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive nounput

%{

/* We have to make the lexer read the input byte by byte since it might be
   called from within another lexer and wants to use the same input - otherwise
   it would also read in input into its internal buffer belonging to the
   calling lexer  */


#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( prepsin );                              \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
/* #include "preps.h" */
#include "preps_parser.h"


int prepslex( void );
extern void prepsparse( void );


/* locally used global variables */

int Preps_Next_Section;


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

ESTR        \x5.*\x3\n.*\n
STR         \x5.*\x6

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*

UNREC       [^\n \t;,\(\)\=\+\-\*\/\[\]\{\}\%\^]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( prepstext + prepsleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( prepstext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( prepstext + prepsleng - 1 ) = '\0';
				Lc = atol( prepstext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );
{ESTR}		{
				prepstext = strchr( prepstext, '\x03' );
				THROW( CLEANER_EXCEPTION );
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Preps_Next_Section = ASSIGNMENTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of DEFAULTS: labels */
{DEF}		{
				Preps_Next_Section = DEFAULTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Preps_Next_Section = VARIABLES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Preps_Next_Section = PHASES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Preps_Next_Section = EXPERIMENT_SECTION;
				return( SECTION_LABEL );
			}

{INT}       {
				prepslval.lval = atol( prepstext );
                return( INT_TOKEN );
            }

{FLOAT}     {
                prepslval.dval = atof( prepstext );
                return( FLOAT_TOKEN );
            }

{STR}       {
				prepstext[ strlen( prepstext ) - 1 ] = '\0';
				prepslval.sptr = prepstext + 1;
				return( STR_TOKEN );
			}

{IDENT}     {
				int acc;

				/* special treatent for calls of print() function */

				if ( ! strcmp( prepstext, "print" ) )
				    return( PRINT_TOK );

				prepslval.vptr = func_get( prepstext, &acc );
				if ( prepslval.vptr != NULL )
				{
					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								"in PREPARATIONS section.\n",
								 Fname, Lc, prepstext );
						THROW( SYNTAX_ERROR_EXCEPTION );
					}
					return( FUNC_TOKEN );
				}

				if ( ( prepslval.vptr = vars_get( prepstext ) )
				     == NULL )
					 THROW( ACCESS_NONEXISTING_VARIABLE );

				return( VAR_TOKEN );
			}

"=="        return( EQ );
"<"         return( LT );
"<="        return( LE );
">"         return( GT );
">="        return( GE );
"="         return( '=' );
"["         return( '[' );
"]"         return( ']' );
","         return( ',' );
"{"         return( '{' );
"}"         return( '}' );

"("         return( '(' );
")"         return( ')' );
"+"         return( '+' );
"-"         return( '-' );
"*"         return( '*' );
"/"         return( '/' );
"%"         return( '%' );
"^"         return( '^' );

			/* handling of end of statement character */
";"			return( ';' );

{WS}        /* skip white space */

			/* handling of invalid input */
{UNREC}     {
				eprint( FATAL, "%s:%ld: Invalid input in PREPARATIONS section:"
						" `%s'\n", Fname, Lc, prepstext );
				return( 0 );
			}

<<EOF>>	    {
				Preps_Next_Section = NO_SECTION;
				return( 0 );
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int preparations_parser( FILE *in )
{
	if ( compilation.sections[ PREPARATIONS_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of PREPARATIONS section "
		        "label.\n", Fname, Lc );
		return( FAIL );
	}
	compilation.sections[ PREPARATIONS_SECTION ] = SET;

	Preps_Next_Section = OK;

	prepsin = in;

	TRY
		prepsparse( );
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in PREPARATIONS section: `%s'\n",
				Fname, Lc, prepstext );
		return( FAIL );
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", prepstext + 2 );
		return( FAIL );
	}
	CATCH( ACCESS_NONEXISTING_VARIABLE )
	{
		eprint( FATAL, "%s:%ld: Variable `%s' has never been declared.\n",
				Fname, Lc, prepstext );
		return( FAIL );
	}

	return( Preps_Next_Section );
}
