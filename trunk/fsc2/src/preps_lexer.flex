/*
  $Id$
*/


/*****************************************************************/
/* This is the lexer for the PREPARATIONS section of an EDL file */
/*****************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-sensitive nounput

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
#include "preps_parser.h"


int prepslex( void );
static int preps_get_channel_name( void );

extern void prepsparse( void );

/* locally used global variables */

int Preps_Next_Section;


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

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

MW          M(ICRO)?_?W(AVE)?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?:?
DET         DET(ECTION)?:?
DET_GATE    DET(ECTION)?_?G(ATE)?:?
RF          R(ADIO)?_?F(REQ(UENCY)?)?:?
RF_GATE     R(ADIO)?_?F(REQ(UENCY)?)?_?G(ATE)?:?
OI          O(THER)?(_?1)?:?
OII         O(THER)?_?2:?
OIII        O(THER)?_?3:?
OIV         O(THER)?_?4:?

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PC          P(HA(SE)?)?_?C(YC(LE)?)?

PS          P(HASE)?_?S(EQ(UENCE)?)?(_?[0-9]{1,2})?

CHT         "CH1"|"CH2"|"CH3"|"CH4"|"AUX"|"MATH1"|"MATH2"|"MATH3"|"REF1"|"REF2"|"REF3"|"REF4"

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( prepstext + prepsleng - 1 ) = '\0';
				if ( Fname != NULL )
					T_free( Fname );
				Fname = get_string_copy( prepstext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( prepstext + prepsleng - 1 ) = '\0';
				Lc = atol( prepstext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, "%s", prepstext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				prepstext = strchr( prepstext, '\x03' );
				eprint( FATAL, "%s", prepstext + 2 );
				THROW( EXCEPTION );
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Preps_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Preps_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Preps_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Preps_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Preps_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

{INT}       {
				prepslval.lval = T_atol( prepstext );
                return INT_TOKEN;
            }

{FLOAT}     {
                prepslval.dval = T_atof( prepstext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
			   the print() function */
{STR}       {
				prepstext[ prepsleng - 1 ] = '\0';
				prepslval.sptr = prepstext + 1;
				return STR_TOKEN;
			}

			/* all pulse related keywords... */

{F}         return F_TOK;
{S}			return S_TOK;
{L}			return L_TOK;
{DS}        return DS_TOK;
{DL}        return DL_TOK;
{PC}        return PC_TOK;

			/* combinations of pulse and property, e.g. `P3.LEN' */

{P}"."{F}   {
				prepslval.vptr = p_get( prepstext, P_FUNC );
				return VAR_REF;
            }

{P}"."{S}   {
				prepslval.vptr = p_get( prepstext, P_POS );
				return VAR_REF;
            }

{P}"."{L}   {
				prepslval.vptr = p_get( prepstext, P_LEN );
				return VAR_REF;
            }

{P}"."{DS}  {
				prepslval.vptr = p_get( prepstext, P_DPOS );
				return VAR_REF;
            }

{P}"."{DL}  {
				prepslval.vptr = p_get( prepstext, P_DLEN );
				return VAR_REF;
            }

{P}"."{PC}  {
				prepslval.vptr = p_get( prepstext, P_PHASE );
				return VAR_REF;
            }

{P}(":")    {
			    Cur_Pulse = p_new( p_num( prepstext ) );
				return P_TOK;
			}

{PS}		{   /* Phase sequence */
				long val = 0;
				char *cp = prepstext + prepsleng - 1;

				/* determine number of phase sequence (0 to ...)*/

				if ( isdigit( *cp ) )
				{
					while ( isdigit( *cp ) )
						cp--;
					val = T_atol( ++cp );
				}

				prepslval.vptr = vars_push( INT_VAR, val );
				return VAR_REF;
			}

{MW}        {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_MW );
				return VAR_REF;
			}
{TWT}       {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_TWT );
				return VAR_REF;
			}
{TWT_GATE}  {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_TWT_GATE );
				return VAR_REF;
			}
{DET}       {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_DET );
				return VAR_REF;
			}
{DET_GATE}  {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_DET_GATE );
				return VAR_REF;
			}
{RF}        {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_RF );
				return VAR_REF;
			}
{RF_GATE}   {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_RF_GATE );
				return VAR_REF;
			}

{OI}        {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_OTHER_1 );
				return VAR_REF;
			}

{OII}        {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_OTHER_2 );
				return VAR_REF;
			}

{OIII}      {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_OTHER_3 );
				return VAR_REF;
			}

{OIV}       {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_OTHER_4 );
				return VAR_REF;
			}


CHT         return preps_get_channel_name( );

			/* handling of function, variable and array identifiers */
{IDENT}     {
				int acc;

				/* first check if the identifier is a function name */

				prepslval.vptr = func_get( prepstext, &acc );
				if ( prepslval.vptr != NULL )
				{
					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can only be "
								"used in the EXPERIMENT section.\n",
								 Fname, Lc, prepstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* if it's not a function it should be a variable */

				if ( ( prepslval.vptr = vars_get( prepstext ) ) == NULL )
				{
					eprint( FATAL, "%s:%ld: Variable `%s' has not been "
							"declared.\n", Fname, Lc, prepstext );
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

            /* quasi-assignment operator for pulse properties */

":"         return ':';

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
				eprint( FATAL, "%s:%ld: Invalid input in PREPARATIONS "
						"section: `%s'\n", Fname, Lc, prepstext );
				THROW( EXCEPTION );
			}

<<EOF>>	    {
				Preps_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int preparations_parser( FILE *in )
{
	static bool is_restart = UNSET;


	if ( compilation.sections[ PREPARATIONS_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of PREPARATIONS section "
		        "label.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	compilation.sections[ PREPARATIONS_SECTION ] = SET;

	Cur_Pulse = -1;
	prepsin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    prepsrestart( prepsin );
	else
		 is_restart = SET;

	prepsparse( );

	return Preps_Next_Section;
}


static int preps_get_channel_name( void )
{
	Var *func, *v;
	int access;


	if ( ( func = func_get_long( "digitizer_get_channel_number",
								 &access, SET ) ) != NULL )
	{
		vars_push( STR_VAR, prepstext );
		v = func_call( func );
		if ( v !=NULL && v->val.lval != UNDEFINED )
			 return E_VAR_REF;
	}

	eprint( FATAL, "%s:%ld: Token `%s' can't be used, no digitizer module "
			"loaded or module does not know how to interpret the token.\n",
			Fname, Lc, prepstext );
	THROW( EXCEPTION );
}
