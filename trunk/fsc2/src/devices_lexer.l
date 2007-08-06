/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


/************************************************************/
/* This is the lexer for the DEVICES section of an EDL file */
/************************************************************/


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap nounput never-interactive

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which isn't to be handled here. */

#define YY_INPUT( buf, result, max_size )                          \
{                                                                  \
    char c;                                                        \
    ssize_t count;                                                 \
    while (    ( count = read( fileno( devicesin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                    \
        /* empty */ ;                                              \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );       \
}


#include "fsc2.h"
#include "devices_parser.h"


extern void devicesparse( void );


/* We need these to avoid unnecessary warnings from the flex-generated
   C code when newer versions of flex are used */

int devicesget_lineno( void );
FILE * devicesget_in( void );
FILE * devicesget_out( void );
int devicesget_leng( void );
char * devicesget_text( void );
void devicesset_lineno( int );
void devicesset_in( FILE * );
void devicesset_out( FILE * );
int devicesget_debug( void );
void devicesset_debug( int );
int deviceslex_destroy( void );


/* locally used global variables */

int Devices_next_section;

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

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

IDENT       [A-Za-z0-9][A-Za-z_0-9]*

WS          [\n\t ]+



        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


            /* handling of file name lines */
{FILE}      {
                *( devicestext + devicesleng - 1 ) = '\0';
                EDL.Fname = CHAR_P T_free( EDL.Fname );
                EDL.Fname = T_strdup( devicestext + 2 );
            }

            /* handling of line number lines */
{LNUM}      {
                *( devicestext + devicesleng - 1 ) = '\0';
                EDL.Lc = atol( devicestext + 2 );
            }

            /* handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, UNSET, "%s\n", devicestext + 2 );
                THROW( EXCEPTION );
            }

{ESTR}      {
                devicestext = strchr( devicestext, '\x03' );
                eprint( FATAL, UNSET, "%s\n", devicestext + 2 );
                THROW( EXCEPTION );
            }

            /* handling of DEVICES: labels */
{DEV}       {
                Devices_next_section = DEVICES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of ASSIGNMENTS: labels */
{ASS}       {
                Devices_next_section = ASSIGNMENTS_SECTION;
                return SECTION_LABEL;
            }

            /* handling of VARIABLES: labels */
{VAR}       {
                Devices_next_section = VARIABLES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of PHASES: labels */
{PHAS}      {
                Devices_next_section = PHASES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of PREPARATIONS: labels */
{PREP}      {
                Devices_next_section = PREPARATIONS_SECTION;
                return SECTION_LABEL;
            }

            /* handling of EXPERIMENT: labels */
{EXP}       {
                Devices_next_section = EXPERIMENT_SECTION;
                return SECTION_LABEL;
            }

{IDENT}     {
                deviceslval.sptr = devicestext;
                return DEV_TOKEN;
            }

;           return ';';

{WS}        /* skip prettifying characters */

            /* handling of invalid input (i.e. everything else) */
.           {
                print( FATAL, "Invalid input in DEVICES section: '%s'.\n",
                       devicestext );
                THROW( EXCEPTION );
            }

<<EOF>>     {
                Devices_next_section = NO_SECTION;
                return 0;
            }


        /*----------------------*/
%%      /*     End of Rules     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

int devices_parser( FILE * in )
{
    int i;


    /* Make sure there is only one DEVICES section and that it is the very
       first section */

    if ( EDL.compilation.sections[ DEVICES_SECTION ] )
    {
        print( FATAL, "Multiple instances of DEVICES section label.\n" );
        THROW( EXCEPTION );
    }

    for ( i = ASSIGNMENTS_SECTION; i <= EXPERIMENT_SECTION; i++ )
    {
        if ( EDL.compilation.sections[ i ] == UNSET )
            continue;

        print( FATAL, "Sorry, DEVICES section must be the very first "
               "section.\n" );
        THROW( EXCEPTION );
    }

    EDL.compilation.sections[ DEVICES_SECTION ] = SET;

    devicesin = in;

    devicesparse( );
    load_all_drivers( );

    return Devices_next_section;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
