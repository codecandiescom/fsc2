/*
  $Id$
*/

/****************************************************************/
/* This is the lexer for the ASSIGNMENTS section of an EDL file */
/****************************************************************/


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
	int c = fgetc( assignin );                             \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "assign_parser.h"


int assignlex( void );
extern void assignparse( void );


/* locally used global variables */

int Assign_Next_Section;

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

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
ML          M(AX(IMUM)?)?_?L(EN(GTH)?)?

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

IDENT       [A-Za-z]+[A-Za-z0-9_]*

INT         [+-]?[0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

MW          M(ICRO)?_?W(AVE)?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?:?
DET         DET(ECTION)?:?
DET_GATE    DET(ECTION)?_?G(ATE)?:?
RF          R(ADIO)?_?F(REQUENCY)?:?
RF_GATE     R(ADIO)?_?F(REQUENCY)?_?G(ATE)?:?
PX          PH(ASE)?_?X:?
PY          PH(ASE)?_?Y:?

DEL         ((D)|(DEL)|(DELAY)):?
POD         P(OD)?
CH          C(H(ANNEL)?)?
INV         I(NV(ERT(ED)?)?)?

WS          [\n=: ]+



		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( assigntext + assignleng - 1 ) = '\0';
				if ( Fname != NULL )
					T_free( Fname );
				Fname = get_string_copy( assigntext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( assigntext + assignleng - 1 ) = '\0';
				Lc = atol( assigntext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, "%s", assigntext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				assigntext = strchr( assigntext, '\x03' );
				eprint( FATAL, "%s", assigntext + 2 );
				THROW( EXCEPTION );
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Assign_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Assign_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Assign_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Assign_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Assign_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Assign_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

{MW}        return MW_TOKEN;

{TWT}       return TWT_TOKEN;

{TWT_GATE}  return TWT_GATE_TOKEN;

{DET}       return DET_TOKEN;

{DET_GATE}  return DET_GATE_TOKEN;

{RF}        return RF_TOKEN;

{RF_GATE}   return RF_GATE_TOKEN;

{PX}        return PHX_TOKEN;

{PY}        return PHY_TOKEN;

{POD}       return POD_TOKEN;

{DEL}       return DEL_TOKEN;

{CH}        return CH_TOKEN;

{INV}       return INV_TOKEN;

"\x4nsec"   return NS_TOKEN;
"\x4usec"   return US_TOKEN;
"\x4msec"   return MS_TOKEN;
"\x4sec"    return S_TOKEN;

			/* all needed pulse related keywords... */

			/* combinations of pulse and property, e.g. `P3.LEN' */

{P}?"."{F}  {
				assignlval.vptr = pulse_get_prop( assigntext, P_FUNC );
				return VAR_REF;
            }

{P}?"."{S}  {
				assignlval.vptr = pulse_get_prop( assigntext, P_POS );
				return VAR_REF;
            }

{P}?"."{L}  {
				assignlval.vptr = pulse_get_prop( assigntext, P_LEN );
				return VAR_REF;
            }

{P}?"."{DS} {
				assignlval.vptr = pulse_get_prop( assigntext, P_DPOS );
				return VAR_REF;
            }

{P}?"."{DL} {
				assignlval.vptr = pulse_get_prop( assigntext, P_DLEN );
				return VAR_REF;
            }

{P}?"."{ML} {
				assignlval.vptr = pulse_get_prop( assigntext, P_MAXLEN );
				return VAR_REF;
            }

{INT}       {
            	assignlval.lval = atol( assigntext );
                return INT_TOKEN;
            }

{FLOAT}     {
            	assignlval.dval = atof( assigntext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				assigntext[ strlen( assigntext ) - 1 ] = '\0';
				assignlval.sptr = assigntext + 1;
				return STR_TOKEN;
			}

			/* handling of function, variable and array identifiers */
{IDENT}     {
				int acc;

				/* first check if the identifier is a function name */

				assignlval.vptr = func_get( assigntext, &acc );
				if ( assignlval.vptr != NULL )
				{
					/* if it's a function check that the function can be used
					   in the current context */

					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								 "in ASSIGN section.\n",
								 Fname, Lc, assigntext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* if it's not a function it should be a variable */

				if ( ( assignlval.vptr = vars_get( assigntext ) ) == NULL )
				{
					eprint(	FATAL, "%s:%ld: Variable `%s' has not been "
							"declared.\n", Fname, Lc, assigntext );
					 THROW( EXCEPTION );
				}

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

{WS}        /* skip prettifying characters */

";"         return ';';               /* end of statement character */

			/* handling of invalid input (i.e. everything else) */
.           {
				eprint( FATAL, "%s:%ld: Invalid input in ASSIGNMENTS section: "
						"`%s'\n", Fname, Lc, assigntext );
				THROW( EXCEPTION );
			}

<<EOF>>	    {
				Assign_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int assignments_parser( FILE *in )
{
	static bool is_restart = UNSET;


	if ( compilation.sections[ ASSIGNMENTS_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of ASSIGNMENTS section "
		        "label.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	compilation.sections[ ASSIGNMENTS_SECTION ] = SET;

	assignin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    assignrestart( assignin );
	else
		 is_restart = SET;

	assignparse( );
	assign_end( );

	return Assign_Next_Section;
}
