/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


/**********************************************/
/* This is the main lexer for EDL input files */
/**********************************************/


%option noyywrap noinput nounput never-interactive

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                        \
{                                                                \
    char c;                                                      \
    ssize_t count;                                               \
    while (    ( count = read( fileno( splitin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                  \
        /* empty */ ;                                            \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );     \
}


#include "fsc2.h"


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int splitget_lineno( void );
FILE * splitget_in( void );
FILE * splitget_out( void );
int splitget_leng( void );
char * splitget_text( void );
void splitset_lineno( int );
void splitset_in( FILE * );
void splitset_out( FILE * );
int splitget_debug( void );
void splitset_debug( int );
int splitlex_destroy( void );
#endif


/* local functions */

static bool section_parser( int section );


%}

FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[\t ]*DEV(ICE)?S?:
ASS         ^[\t ]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[\t ]*VAR(IABLE)?S?:
PHAS        ^[\t ]*PHA(SES)?:
PREP        ^[\t ]*PREP(ARATION)?S?:
EXP         ^[\t ]*EXP(ERIMENT)?:


        /*---------------*/
%%      /*     Rules     */
        /*---------------*/

            /* handling of file name lines */
{FILE}      {
                *( splittext + splitleng - 1 ) = '\0';
                EDL.Fname = T_free( EDL.Fname );
                EDL.Fname = T_strdup( splittext + 2 );
            }

            /* handling of line number lines */
{LNUM}      {
                *( splittext + splitleng - 1 ) = '\0';
                EDL.Lc = atol( splittext + 2 );
            }

            /* handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, UNSET, "%s", splittext + 2 );
                return FAIL;
            }

            /* handling of DEVICES: label */
{DEV}       {
                if ( ! section_parser( DEVICES_SECTION ) )
                    return FAIL;
            }

            /* handling of ASSIGNMENTS: label */
{ASS}       {
                if ( ! section_parser( ASSIGNMENTS_SECTION ) )
                    return FAIL;
            }

            /* handling of VARIABLES: label */
{VAR}       {
                if ( ! section_parser( VARIABLES_SECTION ) )
                    return FAIL;
            }

            /* handling of PHASES: label */
{PHAS}      {
                if ( ! section_parser( PHASES_SECTION ) )
                    return FAIL;
            }

            /* handling of PREPARATIONS: label */
{PREP}      {
                if ( ! section_parser( PREPARATIONS_SECTION ) )
                    return FAIL;
            }

            /* handling of EXPERIMENT: label */
{EXP}       {
                if ( ! section_parser( EXPERIMENT_SECTION ) )
                    return FAIL;
            }

            /* handling of unknown label */
.+":"       {
                print( FATAL, "Unknown label: '%s'.\n", splittext );
                return FAIL;
            }

[\t \n]+    /* dump white space */

            /* handling of invalid input */
.           {
                print( FATAL, "Missing section label at start of file.\n" );
                return FAIL;
            }

<<EOF>>     return OK;


        /*----------------------*/
%%      /*     End of rules     */
        /*----------------------*/


/* The following is for some versions of flex that undefine YY_FLUSH_BUFFER
   immediately after the rules section */

#if ! defined YY_FLUSH_BUFFER
#define YY_FLUSH_BUFFER split_flush_buffer( YY_CURRENT_BUFFER )
#endif


/*-------------------------------------------*
 *-------------------------------------------*/

bool
scan_main( const char * name,
           FILE *       fp )
{
    bool split_error;


    fsc2_assert( fp != NULL );

    /* Forget everything about previous tests */

    clean_up( );

    /* Parse the function and the device name data base */

    if ( ! functions_init( ) || ! device_list_parse( ) )
        return FAIL;

    /* Setting the program length to -1 means that no EXPERIMENT section has
       been found yet. */

    EDL.prg_length = -1;

    /* Make the lexer read its input from the output of fsc2_clean */

    Fsc2_Internals.fsc2_clean_died = UNSET;
    if ( ( splitin = filter_edl( name, fp ) ) == NULL )
    {
        Fsc2_Internals.fsc2_clean_died = SET;
        return FAIL;
    }

    /* The next rather simple looking line is were the predigested EDL file,
       i.e. what we get back from fsc2_clean, is read in and tested for
       syntactical correctness. The code of the EXPERIMENT section is stored
       in memory when the splitlex() function returns successfully. */

    split_error = ( bool ) splitlex( );

    fclose( splitin );

    while ( ! Fsc2_Internals.fsc2_clean_died )
        fsc2_usleep( 10000, UNSET );

    if ( split_error == OK && ! Fsc2_Internals.fsc2_clean_status_ok )
    {
        print( FATAL, "Parsing the EDL script '%s' failed due to some "
               "internal problems.\n", name );
        split_error = FAIL;
    }

    /* In case there was an error and the input file wasn't read until the
       end it's sometimes necessary to flush the lexer's input buffer */

    YY_FLUSH_BUFFER;

    if ( split_error != OK && Fsc2_Internals.cmdline_flags & ICONIFIED_RUN )
    {
        Fsc2_Internals.cmdline_flags &= ~ ICONIFIED_RUN;
        fl_raise_form( GUI.main_form->fsc2 );
        XMapWindow( fl_get_display( ), GUI.main_form->fsc2->window );
    }

    return split_error;
}


/*--------------------------------------------------------------------*
 * This routine is the central distributor that starts the different
 * parsers for all of the sections in the EDL file. It is called once
 * from splitlex(), which just finds the very first section label and
 * passes it to this routine. Here the appropriate lexers are called,
 * which return the next label if they finish successfully. This new
 * label is used to call the next lexer. On failure errors are caught
 * (if the lexer didn't do it by itself) and the failure is signaled
 * back to splitlex(). On success we only return to splitlex() on end
 * of the input file(s).
 * ->
 *    * number (label) of first section found by splitlex()
 * <-
 *    * flag for success (OK) or failure (FAIL)
 *--------------------------------------------------------------------*/

static bool
section_parser( int section )
{
    CLOBBER_PROTECT( section );

    TRY
    {
        do
        {
            /* First run the appropriate lexer/parser combination */

            switch ( section )
            {
                case DEVICES_SECTION :
                    section = devices_parser( splitin );
                    break;

                case ASSIGNMENTS_SECTION :
                    section = assignments_parser( splitin );
                    break;

                case VARIABLES_SECTION :
                    section = variables_parser( splitin );
                    break;

                case PHASES_SECTION :
                    section = phases_parser( splitin );
                    break;

                case PREPARATIONS_SECTION :
                    section = preparations_parser( splitin );
                    break;

                case EXPERIMENT_SECTION :
                    section = experiment_parser( splitin );
                    break;

                default :              /* this should never happen, but ... */
                    fsc2_impossible( );
                    break;
            }
        } while ( section != NO_SECTION );

        TRY_SUCCESS;
        return OK;
    }
    CATCH( MISSING_SEMICOLON_EXCEPTION )
    {
        print( FATAL, "Missing semicolon before (or on) this line.\n" );
        return FAIL;
    }

    return FAIL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
