/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
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

extern Token_Val exp_val;


/* Locally used global variables */

int Exp_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*ASS(IGNMENTS?)?:
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

CHT         ((CH[1-4])|(AUX[12]?)|(MATH[1-3])|(REF[1-4])|(LIN(E)?)|(MEM_[CD])|(FUNC_[EF])|(EXT(10)?))

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*
FUNC        {IDENT}"#"[0-9]+



		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* Handling of file name lines */
{FILE}      {
				*( exptext + expleng - 1 ) = '\0';
				T_free( Fname );
				Fname = T_strdup( exptext + 2 );
			}

			/* Handling of line number lines */
{LNUM}		{
				*( exptext + expleng - 1 ) = '\0';
				Lc = atol( exptext + 2 );
			}

			/* Handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, UNSET, "%s\n", exptext + 2 );
				THROW( EXCEPTION )
			}

{ESTR}		{
				exptext = strchr( exptext, '\x03' );
				eprint( FATAL, UNSET, "%s\n", exptext + 2 );
				THROW( EXCEPTION )
			}

			/* Handling of DEVICES: labels */
{DEV}		{
				Exp_Next_Section = DEVICES_SECTION;
				return 0;
			}

			/* Handling of ASSIGNMENTS: labels */
{ASS}		{
				Exp_Next_Section = ASSIGNMENTS_SECTION;
				return 0;
			}

			/* Handling of VARIABLES: labels */
{VAR}		{
				Exp_Next_Section = VARIABLES_SECTION;
				return 0;
			}

			/* Handling of PHASES: labels */
{PHAS}		{
				Exp_Next_Section = PHASES_SECTION;
				return 0;
			}

			/* Handling of PREPARATIONS: labels */
{PREP}		{
				Exp_Next_Section = PREPARATIONS_SECTION;
				return 0;
			}

			/* Handling of EXPERIMENT: labels */
{EXP}		{
				Exp_Next_Section = EXPERIMENT_SECTION;
				return 0;
			}

{ON_STOP}   return ON_STOP_TOK;

"WHILE"     return WHILE_TOK;
"UNTIL"     return UNTIL_TOK;
"BREAK"     return BREAK_TOK;
"NEXT"      return NEXT_TOK;
"IF"        return IF_TOK;
"ELSE"      return ELSE_TOK;
"UNLESS"    return UNLESS_TOK;
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

			/* Combinations of pulse and property, e.g. `P3.LEN' */

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


{CHT}       {
                exp_val.vptr = get_digitizer_channel_number( exptext );
				return E_VAR_REF;
            }

{FUNC}      {
				int acc;

                if ( ! strcmp( strchr( exptext, '#' ), "#1" ) )
                    *strchr( exptext, '#' ) = '\0';

				exp_val.vptr = func_get( exptext, &acc );
				if ( exp_val.vptr != NULL )
				{
					if ( acc == ACCESS_PREP )
					{
						eprint( FATAL, SET, "Function %s() can't be used in "
								"the EXPERIMENT section.\n", exptext );
						THROW( EXCEPTION )
					}

					if ( ! strcmp( exptext, "end" ) && On_Stop_Pos >= 0 )
					{
						eprint( FATAL, SET, "Function %s() can not be called "
								"after the ON_STOP label.\n", exptext );
						THROW( EXCEPTION )
					}

					return E_FUNC_TOKEN;
				}
                else
                {
                    eprint( FATAL, SET, "Function `%s' is not loaded.\n",
							exptext );
					THROW( EXCEPTION )
                }
            }

{IDENT}     {
				int acc;

				exp_val.vptr = func_get( exptext, &acc );
				if ( exp_val.vptr != NULL )
				{
					if ( acc == ACCESS_PREP )
					{
						eprint( FATAL, SET, "Function `%s' can't be used in "
								"the EXPERIMENT section.\n", exptext );
						THROW( EXCEPTION )
					}

					if ( ! strcmp( exptext, "end" ) && On_Stop_Pos >= 0 )
					{
						eprint( FATAL, SET, "Function %s() can not be called "
								"after the ON_STOP label.\n", exptext );
						THROW( EXCEPTION )
					}

					return E_FUNC_TOKEN;
				}

				exp_val.vptr = vars_get( exptext );
				if ( exp_val.vptr == NULL )
				{
					eprint( FATAL, SET, "Variable or function `%s' has not "
								   "been declared or loaded.\n", exptext );
					 THROW( EXCEPTION )
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


			/* Handling of end of statement character */
";"			return ';';

{WS}        /* Skip white space */

"\x4ntesla" return E_NT_TOKEN;
"\x4utesla" return E_UT_TOKEN;
"\x4mtesla" return E_MT_TOKEN;
"\x4tesla"  return E_T_TOKEN;
"\x4ktesla" return E_KT_TOKEN;
"\x4Mtesla" return E_MGT_TOKEN;

"\x4nunit"  return E_NU_TOKEN;
"\x4uunit"  return E_UU_TOKEN;
"\x4munit"  return E_MU_TOKEN;
"\x4kunit"  return E_KU_TOKEN;
"\x4megunit" return E_MEG_TOKEN;

			/* Handling of invalid input */
.           {
				eprint( FATAL, SET, "Invalid input in EXPERIMENT section: "
						"`%s'.\n", exptext );
				THROW( EXCEPTION )
			}

<<EOF>>	    {
				Exp_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


/*-------------------------------------------*/
/*-------------------------------------------*/

int experiment_parser( FILE *in )
{
	if ( compilation.sections[ EXPERIMENT_SECTION ] )
	{
		eprint( FATAL, SET, "Multiple instances of EXPERIMENT section "
		        "label.\n" );
		THROW( EXCEPTION )
	}
	compilation.sections[ EXPERIMENT_SECTION ] = SET;

	prg_length = 0;
	store_exp( in );
	exp_test_run( );

	if ( Exp_Next_Section != NO_SECTION )
	{
		eprint( FATAL, SET, "EXPERIMENT section has to be the very last "
				"section.\n" );
		THROW( EXCEPTION )
	}

	return Exp_Next_Section;
}
