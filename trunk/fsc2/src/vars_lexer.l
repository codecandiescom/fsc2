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
static Var *vars_get_channel_name( void );

extern void varsparse( void );


/* Locally used global variables */

int Vars_Next_Section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?S?:

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

CHT         ((CH1)|(CH2)|(CH3)|(CH4)|(AUX)|(MATH1)|(MATH2)|(MATH3)|(REF1)|(REF2)|(REF3)|(REF4)|(LIN))

IDENT       [A-Za-z]+[A-Za-z0-9_]*
FUNC        {IDENT}"#"[0-9]+

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?

CTRL        (WHILE)|(UNTIL)|(BREAK)|(NEXT)|(IF)|(ELSE)|(REPEAT)|(FOR)

WS          [\n \t]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* Handling of file name lines */
{FILE}      {
				*( varstext + varsleng - 1 ) = '\0';
				T_free( Fname );
				Fname = T_strdup( varstext + 2 );
			}

			/* Handling of line number lines */
{LNUM}		{
				*( varstext + varsleng - 1 ) = '\0';
				Lc = atol( varstext + 2 );
			}

			/* Handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, UNSET, "%s\n", varstext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				varstext = strchr( varstext, '\x03' );
				eprint( FATAL, UNSET, "%s\n", varstext + 2 );
				THROW( EXCEPTION );
			}

			/* Handling of DEVICES: labels */
{DEV}		{
				Vars_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of ASSIGNMENTS: label */
{ASS}		{
				Vars_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of VARIABLES: label */
{VAR}		{
				Vars_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of PHASES: label */
{PHAS}		{
				Vars_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of PREPARATIONS: label */
{PREP}		{
				Vars_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* Handling of EXPERIMENT: label */
{EXP}		{
				Vars_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

			/* All needed pulse related keywords... */

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

{CHT}       {
                varslval.vptr = vars_get_channel_name( );
				return VAR_REF;
            }

			/* Handling of integer and floating point numbers */
{INT}       {
				varslval.lval = T_atol( varstext );
                return INT_TOKEN;
            }

{FLOAT}     {
                varslval.dval = T_atof( varstext );
                return FLOAT_TOKEN;
            }

            /* Handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				varstext[ strlen( varstext ) - 1 ] = '\0';
				varslval.sptr = varstext + 1;
				return STR_TOKEN;
			}

{CTRL}      {
				eprint( FATAL, SET, "Flow control structures can't be used "
						"in the VARIABLES section.\n" );
				THROW( EXCEPTION );
			}

			/* Handling of function, variable and array identifiers */
{FUNC}      {
				int acc;

                if ( ! strcmp( strchr( varstext, '#' ), "#1" ) )
                    *strchr( varstext, '#' ) = '\0';

				varslval.vptr = func_get( varstext, &acc );
				if ( varslval.vptr != NULL )
				{
					/* If it's a function check that the function can be used
					   in the current context */

					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, SET, "Function `%s' can only be used "
								"in the EXPERIMENT section.\n", varstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}
                else
                {
                    eprint( FATAL, SET, "Function `%s' is not loaded.\n",
							varstext );
					THROW( EXCEPTION );
                }
            }

{IDENT}     {
				int acc;

				/* First check if the identifier is a function name */

				varslval.vptr = func_get( varstext, &acc );
				if ( varslval.vptr != NULL )
				{
					/* If it's a function check that the function can be used
					   in the current context */

					if ( acc == ACCESS_EXP )
					{
						eprint( FATAL, SET, "Function `%s' can only be used "
								"in the EXPERIMENT section.\n", varstext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* If it's not a function it's got to be a variable */

				if ( ( varslval.vptr = vars_get( varstext ) )
				     == NULL )
			         varslval.vptr = vars_new( varstext );
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
				eprint( FATAL, SET, "Invalid input in VARIABLES section: "
						"`%s'.\n", varstext );
				THROW( EXCEPTION );
			}

			/* Handling of end of file */
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


	/* Don't allow more than one VARIABLES section */

	if ( compilation.sections[ VARIABLES_SECTION ] )
	{
		eprint( FATAL, SET, "Multiple instances of VARIABLES section "
				"label.\n" );
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

	vars_sort( );

	return Vars_Next_Section;
}


/*-------------------------------------------*/
/*-------------------------------------------*/

static Var *vars_get_channel_name( void )
{
	Var *func, *v;
	int access;


	if ( ( func = func_get_long( "digitizer_get_channel_number",
								 &access, UNSET ) ) != NULL )
	{
		vars_push( STR_VAR, varstext );
		v = func_call( func );
		if ( v != NULL && v->val.lval != UNDEFINED )
			 return v;
	}

	eprint( FATAL, SET, "Token `%s' can't be used, no digitizer module "
			"loaded or module does not know how to interpret the token.\n",
			varstext );
	THROW( EXCEPTION );
}
