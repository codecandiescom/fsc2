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

/***********************************************************/
/* This is the lexer for the PHASES section of an EDL file */
/***********************************************************/


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
	int c = fgetc( phasesin );                             \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "phases_parser.h"

int phaseslex( void );
extern void phasesparse( void );


/* locally used global variables */

int Phases_Next_Section;


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

PS          ^[ \t]*PH(ASE)?_?S(EQ(UENCE)?)?(_?[0-9]+)?
AS          ^[ \t]*A(CQ(UISITION)?)?_?S(EQ(UENCE)?)?(_?[12])?

PX          "+"?[xX]
MX          "-"[xX]
PY          "+"?[yY]
MY          "-"[yY]
CW          [cC][wW]

PA          "+"?[aA]
MA          "-"[aA]
PB			"+"?[bB]
MB			"-"[bB]
PU          "+"
MU          "-"

WS          [\n=,:. ]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( phasestext + phasesleng - 1 ) = '\0';
				T_free( EDL.Fname );
				EDL.Fname = T_strdup( phasestext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( phasestext + phasesleng - 1 ) = '\0';
				EDL.Lc = atol( phasestext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				eprint( FATAL, UNSET, "%s\n", phasestext + 2 );
				return 0;
			}

			/* handling of DEVICES: labels */
{DEV}		{
				Phases_Next_Section = DEVICES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of ASSIGNMENTS: label */
{ASS}		{
				Phases_Next_Section = ASSIGNMENTS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of VARIABLES: label */
{VAR}		{
				Phases_Next_Section = VARIABLES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PHASES: label */
{PHAS}		{
				Phases_Next_Section = PHASES_SECTION;
				return SECTION_LABEL;
			}

			/* handling of PREPARATIONS: label */
{PREP}		{
				Phases_Next_Section = PREPARATIONS_SECTION;
				return SECTION_LABEL;
			}

			/* handling of EXPERIMENT: label */
{EXP}		{
				Phases_Next_Section = EXPERIMENT_SECTION;
				return SECTION_LABEL;
			}

            /* handling of PHASE_SEQUENCE with (optional) sequence number
			   (if sequence number is left out 0 is used as the default) */
{PS}        {
				char *cp = phasestext + phasesleng - 1;

				/* determine number of phase sequence (0 to ...)*/

				phaseslval.lval = 0;
				if ( isdigit( *cp ) )
				{
					while ( isdigit( *cp ) )
						cp--;
					phaseslval.lval = T_atol( ++cp );
				}
				return PS_TOKEN;
			}

			/* handling of ACQUISITION_SEQUENCE */
{AS}        {
				char *cp = phasestext + phasesleng - 1;

				if ( *cp == '_' )
				    THROW( INVALID_INPUT_EXCEPTION );

				/* determine type of acquisition sequence (A or B) */

				phaseslval.lval = 0;
				if ( *cp == '2')
					phaseslval.lval = 1;

				return AS_TOKEN;
			}

			/* handling of phase cycle identifiers */
{PX}        {
				phaseslval.lval = PHASE_PLUS_X;
				return P_TOKEN;
			}
{MX}        {
				phaseslval.lval = PHASE_MINUS_X;
				return P_TOKEN;
			}
{PY}        {
				phaseslval.lval = PHASE_PLUS_Y;
				return P_TOKEN;
			}
{MY}        {
				phaseslval.lval = PHASE_MINUS_Y;
				return P_TOKEN;
			}

{CW}        {
				phaseslval.lval = PHASE_CW;
				return P_TOKEN;
			}

			/* handling of acquisition cycle identifiers */
{PA}       {
				phaseslval.lval = ACQ_PLUS_A;
				return A_TOKEN;
		   }
{MA}       {
				phaseslval.lval = ACQ_MINUS_A;
				return A_TOKEN;
		   }

{PB}       {
				phaseslval.lval = ACQ_PLUS_B;
				return A_TOKEN;
		   }
{MB}       {
				phaseslval.lval = ACQ_MINUS_B;
				return A_TOKEN;
		   }

{PU}       {
				phaseslval.lval = ACQ_PLUS_U;
				return A_TOKEN;
		   }
{MU}       {
				phaseslval.lval = ACQ_MINUS_U;
				return A_TOKEN;
		   }

{WS}        /* skip prettifying characters */

";"         return ';';              /* end of statement character */

			/* handling of invalid input */
.           THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>	    {
				Phases_Next_Section = NO_SECTION;
				return 0;
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int phases_parser( FILE *in )
{
	static bool is_restart = UNSET;


	if ( EDL.compilation.sections[ PHASES_SECTION ] )
	{
		eprint( FATAL, SET, "Multiple instances of PHASES section label.\n" );
		THROW( EXCEPTION );
	}
	EDL.compilation.sections[ PHASES_SECTION ] = SET;

	phasesin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    phasesrestart( phasesin );
	else
		 is_restart = SET;

	TRY
	{
		phasesparse( );
		phases_end( );             /* do some preliminary consistency checks */
		TRY_SUCCESS;
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, SET, "Invalid input in PHASES section: `%s'.\n",
				phasestext );
		THROW( EXCEPTION );
	}
	OTHERWISE
		RETHROW( );

	return Phases_Next_Section;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
