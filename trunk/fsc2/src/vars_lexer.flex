/*
  $Id$
*/


/**************************************************************/
/* This is the lexer for the VARIABLES section of an EDL file */
/**************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive nounput

%{

/* We have to make the lexer read the input byte by byte since it might
   be called from within another lexer and wants to use the same input -
   otherwise it would also read in input into its internal buffer belonging
   to the calling lexer  */

#define YY_INPUT( buf, result, max_size )                          \
        {                                                          \
	        int c = fgetc( variablesin );                          \
	        result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
        }



#include "fsc2.h"
#include "vars_parser.h"


int variableslex( void );
extern void variablesparse( void );

#ifdef DEBUG
void print_all_vars( void );
#endif

/* locally used global variables */

int Vars_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*ASS(IGNMENT)?S?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?S?:

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

IDENT       [A-Za-z]+[A-Za-z0-9_]*

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
ML          M(AX(IMUM)?)?_?L(EN(GTH)?)?

WS          [\n \t]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( variablestext + variablesleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( variablestext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( variablestext + variablesleng - 1 ) = '\0';
				Lc = atol( variablestext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );

{ESTR}		{
				variablestext = strchr( variablestext, '\x03' );
				THROW( CLEANER_EXCEPTION );
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Vars_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of ASSIGNMENTS: label */
{ASS}		{
				Vars_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: label */
{VAR}		{
				Vars_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: label */
{PHAS}		{
				Vars_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: label */
{PREP}		{
				Vars_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: label */
{EXP}		{
				Vars_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

			/* all needed pulse related keywords... */

{P}?"."{F}  {
				variableslval.vptr
				           = pulse_get_by_addr( n2p( variablestext ), P_FUNC );
				return VAR_REF;
            }

{P}?"."{S}  {
				variableslval.vptr
				            = pulse_get_by_addr( n2p( variablestext ), P_POS );
				return VAR_REF;
            }

{P}?"."{L}  {
				variableslval.vptr
				            = pulse_get_by_addr( n2p( variablestext ), P_LEN );
				return VAR_REF;
            }

{P}?"."{DS} {
				variableslval.vptr
				           = pulse_get_by_addr( n2p( variablestext ), P_DPOS );
				return VAR_REF;
            }

{P}?"."{DL} {
				variableslval.vptr
				           = pulse_get_by_addr( n2p( variablestext ), P_DLEN );
				return VAR_REF;
            }

{P}?"."{ML} {
				variableslval.vptr
				         = pulse_get_by_addr( n2p( variablestext ), P_MAXLEN );
				return VAR_REF;
            }

			/* handling of integer numbers */
{INT}       {
				variableslval.lval = atol( variablestext );
                return INT_TOKEN;
            }

			/* handling of floating point numbers */
{FLOAT}     {
                variableslval.dval = atof( variablestext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				variablestext[ strlen( variablestext ) - 1 ] = '\0';
				variableslval.sptr = variablestext + 1;
				return STR_TOKEN;
			}

			/* handling of function, variable and array identifiers */
{IDENT}     {
				int acc;

				/* first check if the identifier is a function name */

				variableslval.vptr = func_get( variablestext, &acc );
				if ( variableslval.vptr != NULL )
				{
					/* if it's a function check that the function can be used
					   in the current context */

					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								 "in VARIABLES section.\n",
								 Fname, Lc, variablestext );
						THROW( FUNCTION_EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* if it's not a function it's got to be a variable */

				if ( ( variableslval.vptr = vars_get( variablestext ) )
				     == NULL )
			         variableslval.vptr = vars_new( variablestext );
				return VAR_TOKEN;
			}

			/* stuff used with functions, arrays and math */

"=="        return EQ;        /* equal */
"<"         return LT;        /* less than */
"<="        return LE;        /* less than or equal */
">"         return GT;        /* greater than */
">="        return GE;        /* greater than or equal */
"="         return '=';       /* assignment operator */
"["         return '[';       /* start of array indices */
"]"         return ']';       /* end of array indices */
","         return ',';       /* list separator */
"("         return '(';       /* start of function argument list */
")"         return ')';       /* end of function argument list */
"{"         return '{';       /* start of initialisation data list */
"}"         return '}';       /* end of initialisation data list */
"+"         return '+';       /* addition operator */
"-"         return '-';       /* subtraction operator or unary minus */
"*"         return '*';       /* multiplication operator */
"/"         return '/';       /* division operator */
"%"         return '%';       /* modulo operator */
"^"         return '^';       /* exponentiation operator */

			/* handling of end of statement character */
";"			return ';';

{WS}        /* skip white space */

"\x4nsec"   return NS_TOKEN;
"\x4usec"   return US_TOKEN;
"\x4msec"   return MS_TOKEN;
"\x4sec"    return S_TOKEN;

			/* handling of invalid input */
.           THROW( INVALID_INPUT_EXCEPTION );

			/* handling of end of file */
<<EOF>>	    {
				Vars_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


/*---------------------------------------------------------------*/
/* variables_parser() first checks that it wasn't called before, */
/* initializes a few variables and then starts the lexer/parser  */
/* combination. If this works out ok, either the number of the   */
/* next section or, on end of file, NO_SECTION is stored in the  */
/* global variable `Vars_Next_Section', which is returned to the */
/* calling section_parser(). Otherwise, some special exceptions  */
/* are caught here (resulting in FAIL return state) or, for the  */
/* more comman exceptions, are caught in section_parser() (in    */
/* this case, an OK state is returned).                          */
/*---------------------------------------------------------------*/

int variables_parser( FILE *in )
{
	/* don't allow more than one VARIABLES section */

	if ( compilation.sections[ VARIABLES_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of VARIABLES section "
		        "label.\n", Fname, Lc );
		return FAIL;
	}
	compilation.sections[ VARIABLES_SECTION ] = SET;

	Vars_Next_Section = OK;    /* until now... (allows section_parser()
								  to catch several common errors) */
	variablesin = in;

	TRY
		variablesparse( );
	CATCH( MULTIPLE_VARIABLE_DEFINITION_EXCEPTION )
		return FAIL;
	CATCH( UNKNOWN_FUNCTION_EXCEPTION )
		return FAIL;
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in VARIABLES section: `%s'\n",
				 Fname, Lc, variablestext );
		return FAIL;
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", variablestext + 2 );
		return FAIL;
	}
	CATCH( FUNCTION_EXCEPTION )
		return FAIL;

#ifdef DEBUG
	print_all_vars( );
#endif

	return Vars_Next_Section;
}


void print_all_vars( void )
{
	Var *v = var_list;
	long i;

	while ( v != NULL )
	{
		if ( ! ( v->flags & NEW_VARIABLE ) )
		{
			switch ( v->type )
			{	
				case INT_VAR :
					printf( "%s = %ld\n", v->name, v->val.lval );
					break;

				case FLOAT_VAR :
					printf( "%s = %f\n", v->name, v->val.dval );
					break;

				case INT_ARR :
					if ( need_alloc( v ) )
					    break;
					for ( i = 0; i < v->len; ++i )
						printf( "%s[%ld] = %ld\n", v->name, i,
								v->val.lpnt[ i ] );
					break;

				case FLOAT_ARR :
					for ( i = 0; i < v->len; ++i )
						printf( "%s[%ld] = %f\n", v->name, i,
								v->val.dpnt[ i ] );
					break;

				default :             /* this never should happen... */
					assert ( 1 == 0 );
			}
		}

		v = v->next;
	}
}
