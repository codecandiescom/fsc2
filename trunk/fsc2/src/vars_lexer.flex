/*
  $Id$
*/


/**************************************************************/
/* This is the lexer for the VARIABLES section of an EDL file */
/**************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-sensitive nounput

%{

/* We have to make the lexer read the input byte by byte since it might
   be called from within another lexer and wants to use the same input -
   otherwise it would also read in input into its internal buffer belonging
   to the calling lexer  */

#define YY_INPUT( buf, result, max_size )                          \
        {                                                          \
	        int c = fgetc( varsin );                               \
	        result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
        }



#include "fsc2.h"
#include "vars_parser.h"


int varslex( void );
extern void varsparse( void );

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

WS          [\n \t]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( varstext + varsleng - 1 ) = '\0';
				if ( Fname != NULL )
					T_free( Fname );
				Fname = get_string_copy( varstext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( varstext + varsleng - 1 ) = '\0';
				Lc = atol( varstext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, "%s", varstext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				varstext = strchr( varstext, '\x03' );
				eprint( FATAL, "%s", varstext + 2 );
				THROW( EXCEPTION );
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

{P}"."{F}   {
				varslval.vptr = p_get( varstext, P_FUNC );
				return VAR_REF;
            }

{P}"."{S}   {
				varslval.vptr = p_get( varstext, P_POS );
				return VAR_REF;
            }

{P}"."{L}   {
				varslval.vptr = p_get( varstext, P_LEN );
				return VAR_REF;
            }

{P}"."{DS}  {
				varslval.vptr = p_get( varstext, P_DPOS );
				return VAR_REF;
            }

{P}"."{DL}  {
				varslval.vptr = p_get( varstext, P_DLEN );
				return VAR_REF;
            }

			/* handling of integer numbers */
{INT}       {
				varslval.lval = atol( varstext );
                return INT_TOKEN;
            }

			/* handling of floating point numbers */
{FLOAT}     {
                varslval.dval = atof( varstext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				varstext[ strlen( varstext ) - 1 ] = '\0';
				varslval.sptr = varstext + 1;
				return STR_TOKEN;
			}

			/* handling of function, variable and array identifiers */
{IDENT}     {
				int acc;

				/* first check if the identifier is a function name */

				varslval.vptr = func_get( varstext, &acc );
				if ( varslval.vptr != NULL )
				{
					/* if it's a function check that the function can be used
					   in the current context */

					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can only be "
								"used in the EXPERIMENT section.\n",
								 Fname, Lc, varstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* if it's not a function it's got to be a variable */

				if ( ( varslval.vptr = vars_get( varstext ) )
				     == NULL )
			         varslval.vptr = vars_new( varstext );
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
"!"         return NOT;       /* logical negation operator */
"&"         return AND;       /* logical and operator */
"|"         return OR;        /* logical or operator */
"~"         return XOR;       /* logical exclusive or (xor) operator */

			/* handling of end of statement character */
";"			return ';';

{WS}        /* skip white space */

"\x4ntesla" return NT_TOKEN;
"\x4utesla" return UT_TOKEN;
"\x4mtesla" return MT_TOKEN;
"\x4tesla"  return T_TOKEN;

"\x4nunit"  return NU_TOKEN;
"\x4uunit"  return UU_TOKEN;
"\x4mvolt"  return MU_TOKEN;
"\x4kunit"  return KU_TOKEN;
"\x4megunit" return MEG_TOKEN;

			/* handling of invalid input */
.           {
				eprint( FATAL, "%s:%ld: Invalid input in VARIABLES section: "
						"`%s'\n", Fname, Lc, varstext );
				THROW( EXCEPTION );
			}

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
	static bool is_restart = UNSET;


	/* don't allow more than one VARIABLES section */

	if ( compilation.sections[ VARIABLES_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of VARIABLES section "
		        "label.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	compilation.sections[ VARIABLES_SECTION ] = SET;

	varsin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    varsrestart( varsin );
	else
		 is_restart = SET;

	varsparse( );


#ifdef DEBUG
/*
	print_all_vars( );
*/
#endif

	return Vars_Next_Section;
}


#ifdef DEBUG
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
#endif
