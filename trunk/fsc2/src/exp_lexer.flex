/*
  $Id$
*/

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


int prim_explex( void );

extern Token_Val prim_exp_val;


/* locally used global variables */

int Prim_Exp_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

ASS         ^[ \t]*ASS(IGNMENT)?S?:
DEF         ^[ \t]*DEF(AULT)?S?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

CONT        CONT(INUE)?

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PH          PH(ASE(SEQ(UENCE)?)?)?_?{INT}?
ML          M(AX(IMUM)?)?_?L(EN(GTH)?)?
RP          R(EPL(ACEMENT)?)?_?P((ULSE)?S?)?

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*




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
{ESTR}		{
				prim_exptext = strchr( prim_exptext, '\x03' );
				THROW( CLEANER_EXCEPTION );
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Prim_Exp_Next_Section = ASSIGNMENTS_SECTION;
				return 0;
			}

			/* handling of DEFAULTS: labels */
{DEF}		{
				Prim_Exp_Next_Section = DEFAULTS_SECTION;
				return 0;
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Prim_Exp_Next_Section = VARIABLES_SECTION;
				return 0;
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Prim_Exp_Next_Section = PHASES_SECTION;
				return 0;
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Prim_Exp_Next_Section = PREPARATIONS_SECTION;
				return 0;
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Prim_Exp_Next_Section = EXPERIMENT_SECTION;
				return 0;
			}

"WHILE"     return WHILE_TOK;
"BREAK"     return BREAK_TOK;
{CONT}      return CONT_TOK;
"IF"        return IF_TOK;
"ELSE"      return ELSE_TOK;
"REPEAT"    return REPEAT_TOK;
"{"         return '{';          /* block start delimiter */
"}"         return '}';          /* block end delimiter */

{INT}       {
				prim_exp_val.lval = atol( prim_exptext );
                return E_INT_TOKEN;
            }

{FLOAT}     {
                prim_exp_val.dval = atof( prim_exptext );
                return E_FLOAT_TOKEN;
            }

{STR}       {
				prim_exptext[ prim_expleng - 1 ] = '\0';
				prim_exp_val.sptr = prim_exptext + 1;
				return E_STR_TOKEN;
			}

			/* combinations of pulse and property, e.g. `P3.LEN' */

{P}?"."{F}  {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_FUNC );
				return E_VAR_REF;
            }

{P}?"."{S}  {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_POS );
				return E_VAR_REF;
            }

{P}?"."{L}  {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_LEN );
				return E_VAR_REF;
            }

{P}?"."{DS} {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_DPOS );
				return E_VAR_REF;
            }

{P}?"."{DL} {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_DLEN );
				return E_VAR_REF;
            }

{P}?"."{ML} {
				prim_exp_val.vptr = pulse_get_by_addr( n2p( prim_exptext ),
													   P_MAXLEN );
				return E_VAR_REF;
            }

{IDENT}     {
				int acc;

				prim_exp_val.vptr = func_get( prim_exptext, &acc );
				if ( prim_exp_val.vptr != NULL )
				{
					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								"in EXPERIMENT section.\n",
								Fname, Lc, prim_exptext );
						THROW( SYNTAX_ERROR_EXCEPTION );
					}
					return E_FUNC_TOKEN;
				}

				if ( ( prim_exp_val.vptr = vars_get( prim_exptext ) )
				     == NULL )
					 THROW( ACCESS_NONEXISTING_VARIABLE );

				return E_VAR_TOKEN;
			}

"=="        return E_EQ;      /* equal */
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

			/* handling of end of statement character */
";"			return ';';

{WS}        /* skip white space */

			/* handling of invalid input */
.           THROW( INVALID_INPUT_EXCEPTION );

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

	TRY
		store_exp( in );
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
	CATCH( EXPERIMENT_EXCEPTION )
		return FAIL;

	if ( Prim_Exp_Next_Section != NO_SECTION )
	{
		eprint( FATAL, "%s:%ld: EXPERIMENT section has to be the very last "
				"section.\n", Fname, Lc );
		return( FAIL );
	}

	return Prim_Exp_Next_Section;
}
