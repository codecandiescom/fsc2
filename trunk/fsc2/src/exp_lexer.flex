/************************************************************************/
/* This is the primary lexer for the EXPERIMENTS section of an EDL file */
/************************************************************************/


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
	int c = fgetc( prim_expin );                           \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
/* #include "prim_exp.h" */
#include "prim_exp_parser.h"


int prim_explex( void );
extern void prim_expparse( void );


/* locally used global variables */

int Prim_Exp_Next_Section;


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
				*( prim_exptext + prim_expleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( prim_exptext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( prim_exptext + prim_expleng - 1 ) = '\0';
				Lc = atol( prim_exptext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Prim_Exp_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of DEFAULTS: labels */
{DEF}		{
				Prim_Exp_Next_Section = DEFAULTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Prim_Exp_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Prim_Exp_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Prim_Exp_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Prim_Exp_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

{INT}       {
				prim_explval.lval = atol( prim_exptext );
                return INT_TOKEN;
            }

{FLOAT}     {
                prim_explval.dval = atof( prim_exptext );
                return FLOAT_TOKEN;
            }

{IDENT}     {
				int acc;

				prim_explval.vptr = func_get( prim_exptext, &acc );
				if ( prim_explval.vptr != NULL )
				{
					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								"in EXPERIMENT section.\n",
								Fname, Lc, prim_exptext );
						THROW( SYNTAX_ERROR_EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				if ( ( prim_explval.vptr = vars_get( prim_exptext ) )
				     == NULL )
					 THROW( ACCESS_NONEXISTING_VARIABLE );

				return VAR_TOKEN;
			}

"="         return '=';
"["         return '[';
"]"         return ']';
","         return ',';
"{"         return '{';
"}"         return '}';
				  	  
"("         return '(';
")"         return ')';
"+"         return '+';
"-"         return '-';
"*"         return '*';
"/"         return '/';
"%"         return '%';
"^"         return '^';

			/* handling of end of statement character */
";"			return ';';

{WS}        /* skip white space */

			/* handling of invalid input */
{UNREC}     THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>	    {
				Prim_Exp_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int primary_experiment_parser( FILE *in )
{
	if ( compilation.sections[ EXPERIMENT_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of EXPERIMENTS section "
		        "label.\n", Fname, Lc );
		return FAIL;
	}
	compilation.sections[ EXPERIMENT_SECTION ] = SET;

	Prim_Exp_Next_Section = OK;

	prim_expin = in;

	TRY
		prim_expparse( );
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in EXPERIMENT section: `%s'\n",
				Fname, Lc, prim_exptext );
		return FAIL;
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", prim_exptext + 2 );
		return FAIL;
	}
	CATCH( ACCESS_NONEXISTING_VARIABLE )
	{
		eprint( FATAL, "%s:%ld: Variable `%s' has never been declared.\n",
				Fname, Lc, prim_exptext );
		return FAIL;
	}

	return Prim_Exp_Next_Section;
}
