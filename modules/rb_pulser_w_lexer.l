/* -*-C-*-
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noyy_top_state noinput nounput never-interactive

%{

#include "rb_pulser_w.h"


static void set_cur_delay( int card_no );
static void set_cur_clock( int clock_no );
static void set_name( char * name );


static long rb_pulser_w_Lc,
            rb_pulser_w_Comm_Lc;
static int cur_delay = INVALID_DELAY_NUMBER;
static int cur_clock = INVALID_CLOCK_NUMBER;


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int rb_pulser_w_get_lineno( void );
FILE * rb_pulser_w_get_in( void );
FILE * rb_pulser_w_get_out( void );
int rb_pulser_w_get_leng( void );
char * rb_pulser_w_get_text( void );
void rb_pulser_w_set_lineno( int );
void rb_pulser_w_set_in( FILE * );
void rb_pulser_w_set_out( FILE * );
int rb_pulser_w_get_debug( void );
void rb_pulser_w_set_debug( int );
int rb_pulser_w_lex_destroy( void );
#endif

%}

REM1     ^[\t ]*("//"|"#").*\n
REM2     [\t ]*("//"|"#").*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ,;:]*\n
WS       [\t ,;:]+
TWS      [\t ,;:]+\n
NAME     \"[^"]*\"

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ and bash style comment spanning a whole line */

{REM1}      rb_pulser_w_Lc++;

            /* handling of C++/bash style comment not spanning a whole line */

{REM2}      rb_pulser_w_Lc++;

            /* handling of C style comment */

{REM3}      {
                rb_pulser_w_Comm_Lc = rb_pulser_w_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */

\n          rb_pulser_w_Lc++;

            /* handling of EOF within a comment -> fatal error */

<<EOF>>     {
                print( FATAL, "End of configuration file '%s' detected within "
                              "a comment.\n", rb_pulser_w.config_file );
                THROW( EXCEPTION );
            }

            /* end of comment but not end of line */

{EREM1}     yy_pop_state( );

            /* end of comment and end of line */

{EREM2}     {
                rb_pulser_w_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     {
                print( FATAL, "End of comment indicator (\"*/\") without "
                       "previous start of comment in configuration file '%s' "
                       "at line %ld.\n", rb_pulser_w.config_file,
                       rb_pulser_w_Lc );
                THROW( EXCEPTION );
            }

            /* dump empty line (i.e. just containing tabs and spaces) */

{WLWS}      rb_pulser_w_Lc++;

            /* dumps trailing white space */

{TWS}       rb_pulser_w_Lc++;

            /* dump all other forms of white space as well as units */

{WS}

            /* count lines */

\n          rb_pulser_w_Lc++;

"ERT_DELAY_CARD"        set_cur_delay( ERT_DELAY );
"MW_DELAY_CARD_0"       set_cur_delay( MW_DELAY_0 );
"MW_DELAY_CARD_1"       set_cur_delay( MW_DELAY_1 );
"MW_DELAY_CARD_2"       set_cur_delay( MW_DELAY_2 );
"MW_DELAY_CARD_3"       set_cur_delay( MW_DELAY_3 );
"MW_DELAY_CARD_4"       set_cur_delay( MW_DELAY_4 );
"MW_DELAY_CARD_5"       set_cur_delay( MW_DELAY_5 );
"PHASE_DELAY_CARD_0"    set_cur_delay( PHASE_DELAY_0 );
"PHASE_DELAY_CARD_1"    set_cur_delay( PHASE_DELAY_1 );
"PHASE_DELAY_CARD_2"    set_cur_delay( PHASE_DELAY_2 );
"RF_DELAY_CARD"         set_cur_delay( RF_DELAY );
"LASER_DELAY_CARD_0"    set_cur_delay( LASER_DELAY_0 );
"LASER_DELAY_CARD_1"    set_cur_delay( LASER_DELAY_1 );
"DEFENSE_DELAY_CARD"    set_cur_delay( DEFENSE_DELAY );
"DET_DELAY_CARD"        set_cur_delay( DET_DELAY );
"ERT_CLOCK_CARD"        set_cur_clock( ERT_CLOCK );
"TB_CLOCK_CARD_1"       set_cur_clock( TB_CLOCK_1 );
"TB_CLOCK_CARD_2"       set_cur_clock( TB_CLOCK_2 );

{NAME}                  set_name( rb_pulser_w_text );

<<EOF>>     return 0;

.           {
                print( FATAL, "Invalid input in configuration file '%s' at "
                       "line %ld.\n", rb_pulser_w.config_file,
                       rb_pulser_w_Lc );
                THROW( EXCEPTION );
            }

        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
set_cur_delay( int card_no )
{
    if (    cur_delay != INVALID_DELAY_NUMBER
         || cur_clock != INVALID_CLOCK_NUMBER )
    {
        print( FATAL, "Invalid input in configuration file '%s' at "
               "line %ld.\n", rb_pulser_w.config_file, rb_pulser_w_Lc );
        THROW( EXCEPTION );
    }

    cur_delay = card_no;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
set_cur_clock( int clock_no )
{
    if (    cur_delay != INVALID_DELAY_NUMBER
         || cur_clock != INVALID_CLOCK_NUMBER )
    {
        print( FATAL, "Invalid input in configuration file '%s' at "
               "line %ld.\n", rb_pulser_w.config_file, rb_pulser_w_Lc );
        THROW( EXCEPTION );
    }

    cur_clock = clock_no;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
set_name( char * name )
{
    /* Make sure either a delay or a clock card has been selected for the
       name */

    if (    cur_delay == INVALID_DELAY_NUMBER
         && cur_clock == INVALID_CLOCK_NUMBER )
    {
        print( FATAL, "Invalid input in configuration file '%s' at "
               "line %ld.\n", rb_pulser_w.config_file, rb_pulser_w_Lc );
        THROW( EXCEPTION );
    }

    /* Don't accept names with an embedded line break */

    if ( strrchr( rb_pulser_w_text, '\n' ) != NULL )
    {
        print( FATAL, "Invalid input in configuration file '%s' at "
               "line %ld, card name contains a line break\n",
               rb_pulser_w.config_file, rb_pulser_w_Lc );
        THROW( EXCEPTION );
    }

    if ( cur_clock == INVALID_CLOCK_NUMBER )
    {
        if ( rb_pulser_w.delay_card[ cur_delay ].name != NULL )
        {
            print( FATAL, "Name of delay card defined twice in configuration "
                   "file '%s' at line %ld.\n",
                   rb_pulser_w.config_file, rb_pulser_w_Lc );
            THROW( EXCEPTION );
        }

        *( name + strlen( name ) - 1 ) = '\0';
        rb_pulser_w.delay_card[ cur_delay ].name = T_strdup( name + 1 );
        cur_delay = INVALID_DELAY_NUMBER;
    }
    else
    {
        fsc2_assert( cur_delay == INVALID_DELAY_NUMBER );

#if defined FIXED_TIMEBASE
        if ( cur_clock == TB_CLOCK_1 || cur_clock == TB_CLOCK_2 )
        {
            cur_clock = INVALID_CLOCK_NUMBER;
            return;
        }
#endif

        if ( rb_pulser_w.clock_card[ cur_clock ].name != NULL )
        {
            print( FATAL, "Name of clock card defined twice in configuration "
                   "file '%s' at line %ld.\n",
                   rb_pulser_w.config_file, rb_pulser_w_Lc );
            THROW( EXCEPTION );
        }

        *( name + strlen( name ) - 1 ) = '\0';
        rb_pulser_w.clock_card[ cur_clock ].name = T_strdup( name + 1 );
        cur_clock = INVALID_CLOCK_NUMBER;
    }
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rb_pulser_w_read_configuration( void )
{
    static bool is_restart = UNSET;
    size_t i;

    /* Try to open the configuration file */

    if ( access( rb_pulser_w.config_file, R_OK ) == -1 )
    {
        if ( errno == ENOENT )       /* file not found */
            print( FATAL, "Configuration file '%s' not found.\n",
                   rb_pulser_w.config_file );
        else
            print( FATAL, "No read permission for configuration file '%s'.\n",
                   rb_pulser_w.config_file );
        THROW( EXCEPTION );
    }

    if ( ( rb_pulser_w_in = fopen( rb_pulser_w.config_file, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open configuration file '%s'.\n",
               rb_pulser_w.config_file );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        rb_pulser_w_restart( rb_pulser_w_in );
    else
        is_restart = SET;

    /* Read and interpret the configuration file */

    cur_delay = INVALID_DELAY_NUMBER;
    cur_clock = INVALID_CLOCK_NUMBER;
    rb_pulser_w_Lc = 1;

    TRY
    {
        rb_pulser_w_lex( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( rb_pulser_w_in );
        rb_pulser_w_cleanup( );
        RETHROW( );
    }

    fclose( rb_pulser_w_in );

    /* Check that we didn't end up in the middle of a keyword-value pair */

    if (    cur_delay != INVALID_DELAY_NUMBER
         || cur_clock != INVALID_CLOCK_NUMBER )
    {
        print( FATAL, "Invalid input in configuration file '%s' at "
               "end of file.\n", rb_pulser_w.config_file );
        rb_pulser_w_cleanup( );
        THROW( EXCEPTION );
    }

    /* Check that all delay cards got names assigned to them */

    for ( i = 0; i < NUM_DELAY_CARDS; i++ )
        if ( rb_pulser_w.delay_card[ i ].name == NULL )
            break;

    if ( i < NUM_DELAY_CARDS )
    {
        print( FATAL, "Not all delay cards got assigned names in "
               "configuration file '%s'.\n", rb_pulser_w.config_file );
        rb_pulser_w_cleanup( );
        THROW( EXCEPTION );
    }

    /* Check that all clock cards got names assigned to them */

    for ( i = 0; i < NUM_CLOCK_CARDS; i++ )
        if ( rb_pulser_w.clock_card[ i ].name == NULL )
            break;

    if ( i < NUM_CLOCK_CARDS )
    {
        print( FATAL, "Not all clock cards got assigned names in "
               "configuration file '%s'.\n", rb_pulser_w.config_file );
        rb_pulser_w_cleanup( );
        THROW( EXCEPTION );
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
