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

#include "hjs_attenuator.h"


#define TABLE_INIT        0
#define TABLE_ENTRY       1
#define TABLE_END         2
#define TABLE_TYPE_DB     3
#define TABLE_TYPE_STEP   4
#define TABLE_CHUNK_SIZE 32


static void hjs_attenuator_new_table_entry( int    type,
                                            double val );

static int hjs_attenuator_comp( const void * a,
                                const void * b );

static long hjs_attenuator_Lc,
            hjs_attenuator_Comm_Lc;

static size_t cur_table_len;

/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int hjs_attenuator_get_lineno( void );
FILE * hjs_attenuator_get_in( void );
FILE * hjs_attenuator_get_out( void );
int hjs_attenuator_get_leng( void );
char * hjs_attenuator_get_text( void );
void hjs_attenuator_set_lineno( int );
void hjs_attenuator_set_in( FILE * );
void hjs_attenuator_set_out( FILE * );
int hjs_attenuator_get_debug( void );
void hjs_attenuator_set_debug( int );
int hjs_attenuator_lex_destroy( void );
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

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    [+-]?((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
D        ([+-]?{INT})|{FLOAT}
UNIT     "db"|"dB"


%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      hjs_attenuator_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      hjs_attenuator_Lc++;

            /* handling of C style comment */
{REM3}      {
                hjs_attenuator_Comm_Lc = hjs_attenuator_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          hjs_attenuator_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     THROW( EOF_IN_COMMENT_EXCEPTION );

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                hjs_attenuator_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT );

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      hjs_attenuator_Lc++;

            /* dumps trailing white space */
{TWS}       hjs_attenuator_Lc++;

            /* dump all other forms of white space as well as units */
{WS}
{UNIT}

            /* count lines */
\n          hjs_attenuator_Lc++;

{D}         return 1;

<<EOF>>     return 0;

.           THROW( INVALID_INPUT_EXCEPTION );


        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
hjs_attenuator_read_calibration( FILE * fp )
{
    static bool is_restart = UNSET;


    if ( ( hjs_attenuator_in = fp ) == NULL )
    {
        print( FATAL, "Internal error while reading calibration file.\n" );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        hjs_attenuator_restart( hjs_attenuator_in );
    else
        is_restart = SET;

    hjs_attenuator_Lc = 1;

    TRY
    {
        hjs_attenuator_new_table_entry( TABLE_INIT, 0.0 );
        while( hjs_attenuator_lex( ) != 0 )
            hjs_attenuator_new_table_entry( TABLE_ENTRY,
                                            T_atod( hjs_attenuator_text ) );
        hjs_attenuator_new_table_entry( TABLE_END, 0.0 );
        TRY_SUCCESS;
    }
    CATCH( EOF_IN_COMMENT_EXCEPTION )
    {
        print( FATAL, "Unexpected end of calibration file '%s' in comment "
               "starting at line %ld.\n", hjs_attenuator.calib_file,
               hjs_attenuator_Comm_Lc );
        THROW( EXCEPTION );
    }
    CATCH( DANGLING_END_OF_COMMENT )
    {
        print( FATAL, "End of comment found in calibration file '%s' at "
               "line %ld.\n", hjs_attenuator.calib_file, hjs_attenuator_Lc );
        THROW( EXCEPTION );
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid entry in calibration file '%s' at line %ld.\n",
               hjs_attenuator.calib_file, hjs_attenuator_Lc );
        THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW( );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
hjs_attenuator_new_table_entry( int    type,
                                double val )
{
    static int last_type;
    size_t i;


    /* Initialize everything when type is TABLE_INIT */

    if ( type == TABLE_INIT )
    {
        last_type = TABLE_INIT;
        hjs_attenuator.att_table = T_malloc( TABLE_CHUNK_SIZE
                                          * sizeof *hjs_attenuator.att_table );
        cur_table_len = TABLE_CHUNK_SIZE;
        return;
    }

    /* If type is TABLE_END this is the final check after all data have
       been read */

    if ( type == TABLE_END )
    {
        /* Check that we're not expecting another attenuation or frequency
           while the end of the file is already reached */

        if ( last_type != TABLE_TYPE_STEP )
        {
            print( FATAL, "More attenuations than steps in calibration "
                   "file '%s'.\n", hjs_attenuator.calib_file );
            THROW( EXCEPTION );
        }

        /* Check that there are at least two entries in the table */

        if ( hjs_attenuator.att_table_len < 2 )
        {
            print( FATAL, "Calibration file '%s' contains less than 2 "
                   "entries.\n", hjs_attenuator.calib_file );
            THROW( EXCEPTION );
        }

        /* Cut back table size to the amount of memory really needed and sort
           the table in ascending order with respect to the frequencies, then
           set lowest and highest frequency in structure for synthesizer */

        hjs_attenuator.att_table = realloc( hjs_attenuator.att_table,
                                            hjs_attenuator.att_table_len
                                          * sizeof *hjs_attenuator.att_table );
        cur_table_len = hjs_attenuator.att_table_len;
        qsort( hjs_attenuator.att_table, hjs_attenuator.att_table_len,
               sizeof *hjs_attenuator.att_table, hjs_attenuator_comp );

        /* Finally check if the stepper motor postitions drop monotonuosly
           with increasing attenuation - otherwise something very fishy is
           going on... */

        for ( i = 0; i < hjs_attenuator.att_table_len - 2; i++ )
            if ( hjs_attenuator.att_table[ i ].step <=
                 hjs_attenuator.att_table[ i + 1 ].step )
            {
                print( FATAL, "Stepper motor position aren't dropping "
                       "monotonously in calibration file '%s'.\n",
                       hjs_attenuator.calib_file );
                THROW( EXCEPTION );
            }

        hjs_attenuator.min_table_att = hjs_attenuator.att_table[ 0 ].att;
        hjs_attenuator.max_table_att =
            hjs_attenuator.att_table[ hjs_attenuator.att_table_len - 1 ].att;
        return;
    }

    if ( last_type == TABLE_INIT || last_type == TABLE_TYPE_STEP )
    {
        /* If necessary extend the length of the table */

        if ( cur_table_len <= hjs_attenuator.att_table_len )
        {
            cur_table_len += TABLE_CHUNK_SIZE;
            hjs_attenuator.att_table = T_realloc( hjs_attenuator.att_table,
                                                  cur_table_len
                                          * sizeof *hjs_attenuator.att_table );
        }

        hjs_attenuator.att_table[ hjs_attenuator.att_table_len ].att = val;
        last_type = TABLE_TYPE_DB;
    }
    else
    {
        hjs_attenuator.att_table[ hjs_attenuator.att_table_len++ ].step = val;
        last_type = TABLE_TYPE_STEP;
    }
}


/*---------------------------------------------------------------*
 * Comparison function sorting of the table entries in ascending
 * order of attenuations.
 *---------------------------------------------------------------*/

static int
hjs_attenuator_comp( const void * a,
                     const void * b )
{
    Att_Table_Entry_T *A = ( Att_Table_Entry_T * ) a,
                      *B = ( Att_Table_Entry_T * ) b;


    if ( A->att == B->att )
    {
        print( FATAL, "Attenuation of %s dB appears twice in calibration file "
                      "'%s'\n", A->att, hjs_attenuator.calib_file );
        THROW( EXCEPTION );
    }

    return A->att < B->att ? -1 : 1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
