/* -*-C-*-
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/***********************************************************/
/* This is the lexer for the PHASES section of an EDL file */
/***********************************************************/


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap noinput nounput never-interactive

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                         \
{                                                                 \
    char c;                                                       \
    ssize_t count;                                                \
    while (    ( count = read( fileno( phasesin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                   \
        /* empty */ ;                                             \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );      \
}


#include "fsc2.h"
#include "phases_parser.h"

#if defined FLEX_NEEDS_BISON_DECLARATIONS
extern void phasesparse( void );
#endif


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int phasesget_lineno( void );
FILE * phasesget_in( void );
FILE * phasesget_out( void );
int phasesget_leng( void );
char * phasesget_text( void );
void phasesset_lineno( int );
void phasesset_in( FILE * );
void phasesset_out( FILE * );
int phasesget_debug( void );
void phasesset_debug( int );
int phaseslex_destroy( void );
#endif


/* locally used global variables */

int Phases_next_section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n
INCL        \x7.*\n

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

PA          "+"?[aA]
MA          "-"[aA]
PB          "+"?[bB]
MB          "-"[bB]
PU          "+"
MU          "-"

WS          [\n=,:. ]+


        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


            /* Handling of file name lines */
{FILE}      {
                *( phasestext + phasesleng - 1 ) = '\0';
                EDL.Fname = T_free( EDL.Fname );
                EDL.Fname = T_strdup( phasestext + 2 );
            }

            /* Handling of line number lines */
{LNUM}      {
                *( phasestext + phasesleng - 1 ) = '\0';
                EDL.Lc = atol( phasestext + 2 );
            }

            /* Handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, UNSET, "%s\n", phasestext + 2 );
                return 0;
            }

            /* Handling of new file included via #INCLUDE */
{INCL}      {
                phasestext[ strlen( phasestext ) - 1 ] = '\0';
                get_edl_file( phasestext + 1 );
            }

            /* Handling of DEVICES: labels */
{DEV}       {
                Phases_next_section = DEVICES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of ASSIGNMENTS: label */
{ASS}       {
                Phases_next_section = ASSIGNMENTS_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of VARIABLES: label */
{VAR}       {
                Phases_next_section = VARIABLES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of PHASES: label */
{PHAS}      {
                Phases_next_section = PHASES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of PREPARATIONS: label */
{PREP}      {
                Phases_next_section = PREPARATIONS_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of EXPERIMENT: label */
{EXP}       {
                Phases_next_section = EXPERIMENT_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of PHASE_SEQUENCE with (optional) sequence number
               (if sequence number is left out 0 is used as the default) */
{PS}        {
                char *cp = phasestext + phasesleng - 1;

                /* Determine number of phase sequence (0 to ...)*/

                phaseslval.lval = 0;
                if ( isdigit( ( unsigned char ) *cp ) )
                {
                    while ( isdigit( ( unsigned char ) *cp ) )
                        cp--;
                    phaseslval.lval = T_atol( ++cp );
                }
                return PS_TOKEN;
            }

            /* Handling of ACQUISITION_SEQUENCE */
{AS}        {
                char *cp = phasestext + phasesleng - 1;

                if ( *cp == '_' )
                    THROW( INVALID_INPUT_EXCEPTION );

                /* Determine type of acquisition sequence (A or B) */

                phaseslval.lval = 0;
                if ( *cp == '2')
                    phaseslval.lval = 1;

                return AS_TOKEN;
            }

            /* Handling of phase cycle identifiers */
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

            /* Handling of acquisition cycle identifiers */
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

{WS}        /* Skip prettifying characters */

";"         return ';';              /* end of statement character */

            /* Handling of invalid input */
.           THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>     {
                Phases_next_section = NO_SECTION;
                return 0;
            }


        /*----------------------*/
%%      /*     End of Rules     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

int
phases_parser( FILE * in )
{
    static bool is_restart = UNSET;


    if ( EDL.compilation.sections[ PHASES_SECTION ] )
    {
        print( FATAL, "Multiple instances of PHASES section label.\n" );
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
        print( FATAL, "Invalid input near '%s'.\n", phasestext );
        THROW( EXCEPTION );
    }

    return Phases_next_section;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
