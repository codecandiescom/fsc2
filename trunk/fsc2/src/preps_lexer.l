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

extern void prepsparse( void );

/* Locally used global variables */

int Preps_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

MW          M(ICRO)?_?W(AVE)?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?
DET         DET(ECTION)?
DET_GATE    DET(ECTION)?_?G(ATE)?
DEF         DEF(ENSE)?
RF          R(ADIO)?_?F(REQ(UENCY)?)?
RF_GATE     R(ADIO)?_?F(REQ(UENCY)?)?_?G(ATE)?
PSH         P(ULSE)?_?SH(APE)?
OI          O(THER)?(_?1)?
OII         O(THER)?_?2
OIII        O(THER)?_?3
OIV         O(THER)?_?4

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}(#{INT})?

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PC          PH(ASE)?_?C(YCLE)?

PS          PH(ASE)?_?S(EQ(UENCE)?)?(_?[0-9])?

CHT         ((CH[1-4])|(AUX[12]?)|(MATH[1-3])|(REF[1-4])|(LIN(E)?)|(MEM_[CD])|(FUNC_[EF])|(EXT(10)?))


CTRL        (WHILE)|(UNTIL)|(BREAK)|(NEXT)|(IF)|(ELSE)|(REPEAT)|(FOR)

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*
FUNC        {IDENT}"#"[0-9]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* Handling of file name lines */
{FILE}      {
				*( prepstext + prepsleng - 1 ) = '\0';
				T_free( Fname );
				Fname = T_strdup( prepstext + 2 );
			}

			/* Handling of line number lines */
{LNUM}		{
				*( prepstext + prepsleng - 1 ) = '\0';
				Lc = atol( prepstext + 2 );
			}

			/* Handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, UNSET, "%s\n", prepstext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				prepstext = strchr( prepstext, '\x03' );
				eprint( FATAL, UNSET, "%s\n", prepstext + 2 );
				THROW( EXCEPTION );
			}

			/* Handling of DEVICES: labels */
{DEV}		{
				Preps_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of ASSIGNMENTS: labels */
{ASS}		{
				Preps_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of VARIABLES: labels */
{VAR}		{
				Preps_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of PHASES: labels */
{PHAS}		{
				Preps_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of EXPERIMENT: labels */
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

            /* Handling of string constants (to be used as format strings in
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

			/* Combinations of pulse and property, e.g. `P3.LEN' */

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
				char *t;

				Cur_Pulser = 0;
				if ( ( t = strrchr( prepstext, '#' ) ) != NULL )
				{
					if ( ( Cur_Pulser = T_atol( t + 1 ) - 1 ) >= Num_Pulsers )
					{
						eprint( FATAL, SET, "There is no pulser #%ld\n",
								Cur_Pulser + 1 );
						THROW( EXCEPTION );
					}

					*t = '\0';
				}

			    Cur_Pulse = p_new( p_num( prepstext ) );
				return P_TOK;
			}

{PS}		{   /* Phase sequence */
				long val = 0;
				char *cp = prepstext + prepsleng - 1;

				/* determine number of phase sequence */

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
{DEF}       {
				prepslval.vptr = vars_push( INT_VAR, PULSER_CHANNEL_DEFENSE );
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

{PSH}       {
				prepslval.vptr = vars_push( INT_VAR,
											PULSER_CHANNEL_PULSE_SHAPE );
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


{CHT}       {
                prepslval.vptr = get_digitizer_channel_number( prepstext );
                return VAR_REF;
            }

{CTRL}      {
				eprint( FATAL, SET, "Flow control structures can't be used "
						"in the PREPARATIONS section.\n" );
				THROW( EXCEPTION );
			}

			/* Handling of function, variable and array identifiers */
{FUNC}      {
				char *t;
				int acc;

				t = strrchr( prepstext, '#' );
                if ( t != NULL && ! strcmp( t, "#1" ) )
                    *t = '\0';

				prepslval.vptr = func_get( prepstext, &acc );
				if ( prepslval.vptr != NULL )
				{
					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, SET, "Function `%s' can only be used "
								"in the EXPERIMENT section.\n", prepstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}
                else
                {
                    eprint( FATAL, SET, "Function `%s' is not loaded.\n",
							prepstext );
					THROW( EXCEPTION );
                }
            }

{IDENT}     {
				int acc;

				/* First check if the identifier is a function name */

				prepslval.vptr = func_get( prepstext, &acc );
				if ( prepslval.vptr != NULL )
				{
					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, SET, "Function `%s' can only be used "
								"in the EXPERIMENT section.\n", prepstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* If it's not a function it should be a variable */

				if ( ( prepslval.vptr = vars_get( prepstext ) ) == NULL )
				{
					eprint( FATAL, SET, "Variable or function `%s' has not "
						    "been declared or loaded.\n", prepstext );
					THROW( EXCEPTION );
				}

				return VAR_TOKEN;
			}

			/* Stuff used with functions, arrays and math */

"=="        return EQ;        /* equal */
"!="        return NE;        /* unequal */
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
"+="        return PLSA;
"-="        return MINA;
"*="        return MULA;
"/="        return DIVA;
"%="        return MODA;
"^="        return EXPA;

            /* Quasi-assignment operator for pulse properties */

":"         return ':';

			/* Handling of end of statement character */
";"			return ';';

{WS}        /* Skip white space */

"\x4ntesla" return NT_TOKEN;
"\x4utesla" return UT_TOKEN;
"\x4mtesla" return MT_TOKEN;
"\x4tesla"  return T_TOKEN;
"\x4ktesla" return KT_TOKEN;
"\x4Mtesla" return MGT_TOKEN;

"\x4nunit"  return NU_TOKEN;
"\x4uunit"  return UU_TOKEN;
"\x4munit"  return MU_TOKEN;
"\x4kunit"  return KU_TOKEN;
"\x4megunit" return MEG_TOKEN;

			/* Handling of invalid input */
.           {
				eprint( FATAL, SET, "Invalid input in PREPARATIONS "
						"section: `%s'.\n", prepstext );
				THROW( EXCEPTION );
			}

<<EOF>>	    {
				Preps_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


/*-------------------------------------------*/
/*-------------------------------------------*/

int preparations_parser( FILE *in )
{
	static bool is_restart = UNSET;


	if ( compilation.sections[ PREPARATIONS_SECTION ] )
	{
		eprint( FATAL, SET, "Multiple instances of PREPARATIONS section "
		        "label.\n" );
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
