/* -*-C-*-
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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
/* This is the lexer for the ASSIGNMENTS section of an EDL file */
/****************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-sensitive nounput

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

static int Assign_Next_Section;
static bool in_phs = UNSET;

/* locally used functions */

static void set_cur_pulser( void );

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

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

IDENT       [A-Za-z]+[A-Za-z0-9_]*

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

MW          M(ICRO)?_?W(AVE)?(#{INT})?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?(#{INT})?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?(#{INT})?:?
DET         DET(ECTION)?(#{INT})?:?
DET_GATE    DET(ECTION)?_?G(ATE)?(#{INT})?:?
DEF         DEF(ENSE)?(#{INT})?:?
RF          R(ADIO)?_?F(REQ(UENCY)?)?(#{INT})?:?
RF_GATE     R(ADIO)?_?F(REQ(UENCY)?)?_?G(ATE)?(#{INT})?:?
PSH         P(ULSE)?_?SH(APE(R)?)?(#{INT})?:?
OI          O(THER)?(_?1)?(#{INT})?:?
OII         O(THER)?_?2?(#{INT})?:?
OIII        O(THER)?_?3?(#{INT})?:?
OIV         O(THER)?_?4?(#{INT})?:?
PH1         PH(ASE)?(_?1)?(#{INT})?:?
PH2         PH(ASE)?_?2(#{INT})?:?

PHS1        PH(ASE)?_S(ETUP)?(_?1)?(#{INT})?:?
PHS2        PH(ASE)?_S(ETUP)?_?2(#{INT})?:?

PX			"+"?[xX]:?
PY			"+"?[yY]:?
PMX			"-"[yX]:?
PMY			"-"[yY]:?
PCW         [cC][wW]
POD1        P(OD)?_?1
POD2        P(OD)?_?2
ON          ON
OFF         OFF

DEL         ((DEL)|(DELAY)):?
POD         P(OD)?
CH          CH(ANNEL)?
INV         INV(ERT(ED)?)?
VH          V_?H(IGH)?
VL          V_?L(OW)?

TB          T(IME)?_?B(ASE)?(#{INT})?:?
TM          T(RIG(GER)?)?_?M(ODE)?(#{INT})?:?
MPL         MAX(IMUM)?_?PATT(ERN)?_LEN(GTH)(#{INT})?
KAP         KEEP_?ALL_?PULSES(#{INT})?:?

INTERN      INT(ERN(AL)?)?
EXTERN      EXT(ERN(AL)?)?
SL          SL(OPE)?
NEG         NEG(ATIVE)?
POS         POS(ITIVE)?
THRESH      LEV(EL)?
IMP         IMP(EDANCE)?
LOW         L(OW)?
HIGH        H(IGH)?
REPT        REP(EAT)?_?T(IME)?
REPF        REP(EAT)?_?F(REQ(UENCY)?)?

PSD1        PH(ASE)?_?S(W(ITCH)?)?_?D(EL(AY)?)?(_?1)?(#{INT})?:?
PSD2        PH(ASE)?_?S(W(ITCH)?)?_?D(EL(AY)?)?_?2(#{INT})?:?

GP          G(RACE)?_?P(ERIOD)?(#{INT})?:?

TO          TRIG(GER)?_?OUT

CTRL        (WHILE)|(BREAK)|(NEXT)|(IF)|(ELSE)|(REPEAT)|(FOR)

WS          [\n=: ]+



		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( assigntext + assignleng - 1 ) = '\0';
				T_free( EDL.Fname );
				EDL.Fname = T_strdup( assigntext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( assigntext + assignleng - 1 ) = '\0';
				EDL.Lc = atol( assigntext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, UNSET, "%s\n", assigntext + 2 );
				THROW( EXCEPTION );
			}

{ESTR}		{
				assigntext = strchr( assigntext, '\x03' );
				eprint( FATAL, UNSET, "%s\n", assigntext + 2 );
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

{PHS1}		{
			    set_cur_pulser( );
				in_phs = SET;
  				assignlval.lval = 0;
				return PHS_TOK;
			}
{PHS2}		{
			    set_cur_pulser( );
				in_phs = SET;
				assignlval.lval = 1;
				return PHS_TOK;
			}

{PX}		{
				assignlval.lval = PHASE_PLUS_X;
				return PXY_TOK;
			}
{PY}		{
				assignlval.lval = PHASE_PLUS_Y;
				return PXY_TOK;
			}
{PMX}		{
				assignlval.lval = PHASE_MINUS_X;
				return PXY_TOK;
			}
{PMY}		{
				assignlval.lval = PHASE_MINUS_Y;
				return PXY_TOK;
			}
{PCW}       {
				assignlval.lval = PHASE_CW;
				return PXY_TOK;
			}

{POD1}		return POD1_TOK;
{POD2}		return POD2_TOK;

{ON}        return ON_TOK;
{OFF}       return OFF_TOK;

{MW}        {
				set_cur_pulser( );
				return MW_TOKEN;
			}

{TWT}       {
				set_cur_pulser( );
				return TWT_TOKEN;
			}

{TWT_GATE}  {
				set_cur_pulser( );
				return TWT_GATE_TOKEN;
			}

{DET}       {
				set_cur_pulser( );
				return DET_TOKEN;
			}

{DET_GATE}  {
				set_cur_pulser( );
				return DET_GATE_TOKEN;
			}

{DEF}       {
				set_cur_pulser( );
				return DEF_TOKEN;
			}

{RF}        {
				set_cur_pulser( );
				return RF_TOKEN;
			}

{RF_GATE}   {
				set_cur_pulser( );
				return RF_GATE_TOKEN;
			}

{PSH}       {
				set_cur_pulser( );
				return PSH_TOKEN;
			}

{PH1}       {
				set_cur_pulser( );
				return PH1_TOKEN;
			}

{PH2}       {
				set_cur_pulser( );
				return PH2_TOKEN;
			}

{OI}        {
				set_cur_pulser( );
				return OI_TOKEN;
			}

{OII}       {
				set_cur_pulser( );
				return OII_TOKEN;
			}

{OIII}      {
				set_cur_pulser( );
				return OIII_TOKEN;
			}

{OIV}       {
				set_cur_pulser( );
				return OIV_TOKEN;
			}

{POD}       return POD_TOKEN;

{DEL}       return DEL_TOKEN;

{CH}        return CH_TOKEN;

{INV}       return INV_TOKEN;

{VH}        return VH_TOKEN;

{VL}        return VL_TOKEN;

            /* keywords related to time base and trigger mode */

{TB}		{
				set_cur_pulser( );
				return TB_TOKEN;
			}

{TM}		{
				set_cur_pulser( );
				return TM_TOKEN;
			}

{MPL}		{
				set_cur_pulser( );
				return MPL_TOKEN;
			}

{INTERN}    return INTERN_TOKEN;
{EXTERN}    return EXTERN_TOKEN;
{SL}        return SLOPE_TOKEN;
{NEG}       return NEG_TOKEN;
{POS}       return POS_TOKEN;
{THRESH}    return THRESH_TOKEN;
{IMP}       return IMP_TOKEN;
{KAP}       return KAP_TOKEN;
{LOW}       {
				assignlval.lval = LOW;
				return HL_TOKEN;
			}
{HIGH}      {
				assignlval.lval = HIGH;
				return HL_TOKEN;
			}
{REPT}      return REPT_TOKEN;
{REPF}      return REPF_TOKEN;

{GP}		{
				set_cur_pulser( );
				return GP_TOKEN;
			}

"\x4nunit"   return NU_TOKEN;
"\x4uunit"   return UU_TOKEN;
"\x4munit"   return MU_TOKEN;
"\x4kunit"   return KU_TOKEN;
"\x4megunit" return MEG_TOKEN;
"\x4ntesla"  return NT_TOKEN;
"\x4utesla"  return UT_TOKEN;
"\x4mtesla"  return MT_TOKEN;
"\x4tesla"   return T_TOKEN;
"\x4ktesla"  return MT_TOKEN;
"\x4Ktesla"  return MT_TOKEN;

			/* all pulse related keywords... */

			/* combinations of pulse and property, e.g. 'P3.LEN' */

{P}"."{F}   {
				assignlval.vptr = p_get( assigntext, P_FUNC );
				return VAR_REF;
            }

{P}"."{S}   {
				assignlval.vptr = p_get( assigntext, P_POS );
				return VAR_REF;
            }

{P}"."{L}   {
				assignlval.vptr = p_get( assigntext, P_LEN );
				return VAR_REF;
            }

{P}"."{DS}  {
				assignlval.vptr = p_get( assigntext, P_DPOS );
				return VAR_REF;
            }

{P}"."{DL}  {
				assignlval.vptr = p_get( assigntext, P_DLEN );
				return VAR_REF;
            }

{PSD1}		{
				assignlval.lval	= 0;
				return PSD_TOKEN;
			}

{PSD2}		{
				assignlval.lval	= 1;
				return PSD_TOKEN;
			}

{INT}       {
            	assignlval.lval = T_atol( assigntext );
                return INT_TOKEN;
            }

{FLOAT}     {
            	assignlval.dval = T_atod( assigntext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				assigntext[ strlen( assigntext ) - 1 ] = '\0';
				assignlval.sptr = assigntext + 1;
				return STR_TOKEN;
			}

{CTRL}      {
				print( FATAL, "Flow control structures can't be used in the "
					   "ASSIGNMENTS section.\n" );
				THROW( EXCEPTION );
			}

			/* This is here for the HFS9000 which has a special TRIGGER_OUT
			   channel that is assigned the channel number 0 */
{TO}        {
            	assignlval.lval = 0;
                return INT_TOKEN;
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

					if ( acc == ACCESS_EXP )
					{
						print( FATAL, "Function '%s' can only be used in the "
							   "EXPERIMENT section.\n", assigntext );
						THROW( EXCEPTION );
					}
					return FUNC_TOKEN;
				}

				/* if it's not a function it should be a variable */

				if ( ( assignlval.vptr = vars_get( assigntext ) ) == NULL )
				{
					print(	FATAL, "Variable or function '%s' has not been "
							"declared or loaded.\n", assigntext );
					 THROW( EXCEPTION );
				}

				return VAR_TOKEN;
			}

			/* stuff used with functions, arrays and math */

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

{WS}        /* skip prettifying characters */

";"         {
				in_phs = UNSET;
				return ';';
			}

			/* handling of invalid input (i.e. everything else) */
.           {
				print( FATAL, "Invalid input in ASSIGNMENTS section: '%s'.\n",
					   assigntext );
				THROW( EXCEPTION );
			}

<<EOF>>	    {
				Assign_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


/*-------------------------------------------*/
/*-------------------------------------------*/

int assignments_parser( FILE *in )
{
	static bool is_restart = UNSET;


	if ( EDL.compilation.sections[ ASSIGNMENTS_SECTION ] )
	{
		print( FATAL, "Multiple instances of ASSIGNMENTS section label.\n" );
		THROW( EXCEPTION );
	}
	EDL.compilation.sections[ ASSIGNMENTS_SECTION ] = SET;

	assignin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    assignrestart( assignin );
	else
		 is_restart = SET;

	assignparse( );

	return Assign_Next_Section;
}


/*-------------------------------------------*/
/*-------------------------------------------*/

static void set_cur_pulser( void )
{
	const char *t;


	if ( in_phs )
		return;

	Cur_Pulser = 0;
	if ( ( t = strrchr( assigntext, '#' ) ) == NULL )
		return;

	if ( ( Cur_Pulser = T_atol( t + 1 ) - 1 ) >= EDL.Num_Pulsers )
	{
		print( FATAL, "There is no pulser #%ld\n", Cur_Pulser + 1 );
		THROW( EXCEPTION );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
