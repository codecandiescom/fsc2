/*
  $Id$
*/

/****************************************************************/
/* This is the lexer for the EXPERIMENTS section of an EDL file */
/****************************************************************/


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
	int c = fgetc( expin );                                \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"


int explex( void );
static int exp_get_channel_name( void );

extern Token_Val exp_val;


/* locally used global variables */

int Exp_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:
ON_STOP		^[ \t]*ON_STOP:


INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PH          PH(ASE(SEQ(UENCE)?)?)?_?([0-9])?

CHT         ((CH1)|(CH2)|(CH3)|(CH4)|(AUX)|(MATH1)|(MATH2)|(MATH3)|(REF1)|(REF2)|(REF3)|(REF4)|(LIN))

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*




		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( exptext + expleng - 1 ) = '\0';
				T_free( Fname );
				Fname = get_string_copy( exptext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( exptext + expleng - 1 ) = '\0';
				Lc = atol( exptext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, "%s\n", exptext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				exptext = strchr( exptext, '\x03' );
				eprint( FATAL, "%s\n", exptext + 2 );
				THROW( EXCEPTION );
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Exp_Next_Section = DEVICES_SECTION;
				return 0;
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Exp_Next_Section = ASSIGNMENTS_SECTION;
				return 0;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Exp_Next_Section = VARIABLES_SECTION;
				return 0;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Exp_Next_Section = PHASES_SECTION;
				return 0;
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Exp_Next_Section = PREPARATIONS_SECTION;
				return 0;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Exp_Next_Section = EXPERIMENT_SECTION;
				return 0;
			}

{ON_STOP}   return ON_STOP_TOK;

"WHILE"     return WHILE_TOK;
"BREAK"     return BREAK_TOK;
"NEXT"      return CONT_TOK;
"IF"        return IF_TOK;
"ELSE"      return ELSE_TOK;
"REPEAT"    return REPEAT_TOK;
"FOR"       return FOR_TOK;
"FOREVER"   return FOREVER_TOK;
"{"         return '{';          /* block start delimiter */
"}"         return '}';          /* block end delimiter */

{INT}       {
				exp_val.lval = T_atol( exptext );
                return E_INT_TOKEN;
            }

{FLOAT}     {
                exp_val.dval = T_atof( exptext );
                return E_FLOAT_TOKEN;
            }

{STR}       {
				exptext[ expleng - 1 ] = '\0';
				exp_val.sptr = exptext + 1;
				return E_STR_TOKEN;
			}

{P}			{
				exp_val.lval = p_num( exptext );
				return E_INT_TOKEN;
			}

			/* combinations of pulse and property, e.g. `P3.LEN' */

{P}"."{F}   {
				exp_val.vptr = p_get( exptext, P_FUNC );
				return E_VAR_REF;
            }

{P}"."{S}   {
				exp_val.lval = p_num( exptext );
				return E_PPOS;
            }

{P}"."{L}   {
				exp_val.lval = p_num( exptext );
				return E_PLEN;
            }

{P}"."{DS}  {
				exp_val.lval = p_num( exptext );
				return E_PDPOS;
            }

{P}"."{DL}  {
				exp_val.lval = p_num( exptext );
				return E_PDLEN;
            }


{CHT}       return exp_get_channel_name( );


{IDENT}     {
				int acc;

				exp_val.vptr = func_get( exptext, &acc );
				if ( exp_val.vptr != NULL )
				{
					if ( acc == ACCESS_PREP )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								"in the EXPERIMENT section.\n",
								Fname, Lc, exptext );
						THROW( EXCEPTION );
					}
					return E_FUNC_TOKEN;
				}

				exp_val.vptr = vars_get( exptext );
				if ( exp_val.vptr == NULL )
				{
					eprint( FATAL, "%s:%ld: Variable `%s' has not been "
							"declared.\n", Fname, Lc, exptext );
					 THROW( EXCEPTION );
				}

				return E_VAR_TOKEN;
			}

"=="        return E_EQ;      /* equal */
"!="        return E_NE;      /* unequal */
"<"         return E_LT;      /* less than */              
"<="        return E_LE;      /* less than or equal */     
">"         return E_GT;      /* greater than */           
">="        return E_GE;      /* greater than or equal */  
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
":"         return ':';       /* condition separator in for loops */
"!"         return E_NOT;     /* logical negation operator */
"&"         return E_AND;     /* logical and operator */
"|"         return E_OR;      /* logical or operator */
"~"         return E_XOR;     /* logical exclusive or (xor) operator */
"+="        return E_PLSA;
"-="        return E_MINA;
"*="        return E_MULA;
"/="        return E_DIVA;
"%="        return E_MODA;
"^="        return E_EXPA;


			/* handling of end of statement character */
";"			return ';';

{WS}        /* skip white space */

"\x4ntesla" return E_NT_TOKEN;
"\x4utesla" return E_UT_TOKEN;
"\x4mtesla" return E_MT_TOKEN;
"\x4tesla"  return E_T_TOKEN;

"\x4nunit"  return E_NU_TOKEN;
"\x4uunit"  return E_UU_TOKEN;
"\x4munit"  return E_MU_TOKEN;
"\x4kunit"  return E_KU_TOKEN;
"\x4megunit" return E_MEG_TOKEN;

			/* handling of invalid input */
.           {
				eprint( FATAL, "%s:%ld: Invalid input in EXPERIMENT section: "
						"`%s'.\n", Fname, Lc, exptext );
				THROW( EXCEPTION );
			}

<<EOF>>	    {
				Exp_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int experiment_parser( FILE *in )
{
	if ( compilation.sections[ EXPERIMENT_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of EXPERIMENTS section "
		        "label.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
	compilation.sections[ EXPERIMENT_SECTION ] = SET;

	store_exp( in );
	exp_test_run( );

	if ( Exp_Next_Section != NO_SECTION )
	{
		eprint( FATAL, "%s:%ld: EXPERIMENT section has to be the very last "
				"section.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	return Exp_Next_Section;
}


static int exp_get_channel_name( void )
{
	Var *func, *v;
	int access;


	if ( ( func = func_get_long( "digitizer_get_channel_number",
								 &access, SET ) ) != NULL )
	{
		vars_push( STR_VAR, exptext );
		v = func_call( func );
		if ( v !=NULL && v->val.lval != UNDEFINED )
			 return E_VAR_REF;
	}

	eprint( FATAL, "%s:%ld: Token `%s' can't be used, no digitizer module "
			"loaded or module does not know how to interpret the token.\n",
			Fname, Lc, exptext );
	THROW( EXCEPTION );
}
