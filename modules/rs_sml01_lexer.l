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

#include "rs_sml01.h"


#define IS_FREQ 1
#define IS_ATT  2
#define TABLE_INIT -1
#define TABLE_END   0
#define TABLE_CHUNK_SIZE 128

static int rs_sml01_get_unit( void );

static void rs_sml01_new_table_entry( int    type,
                                      double val );

static int rs_sml01_comp( const void * a,
                          const void * b );

static long rs_sml01_Lc,
            rs_sml01_Comm_Lc;
static double unit_fac = 1.0;

static long cur_table_len;

/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int rs_sml01_get_lineno( void );
FILE * rs_sml01_get_in( void );
FILE * rs_sml01_get_out( void );
int rs_sml01_get_leng( void );
char * rs_sml01_get_text( void );
void rs_sml01_set_lineno( int );
void rs_sml01_set_in( FILE * );
void rs_sml01_set_out( FILE * );
int rs_sml01_get_debug( void );
void rs_sml01_set_debug( int );
int rs_sml01_lex_destroy( void );
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
D        {INT}|{FLOAT}
IU       {INT}[\n\t ]*((("k"|"M"|"G")*"Hz")|"db"|"dB")
FU       {FLOAT}[\n\t ]*((("k"|"M"|"G")*"Hz")|"db"|"dB")
DU       {IU}|{FU}


%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      rs_sml01_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      rs_sml01_Lc++;

            /* handling of C style comment */
{REM3}      {
                rs_sml01_Comm_Lc = rs_sml01_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          rs_sml01_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     THROW( EOF_IN_COMMENT_EXCEPTION );

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                rs_sml01_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT );

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      rs_sml01_Lc++;

            /* dumps trailing white space */
{TWS}       rs_sml01_Lc++;

            /* dump all other forms of white */
{WS}

            /* count lines */
\n          rs_sml01_Lc++;

{DU}        return rs_sml01_get_unit( );

{D}         {
                unit_fac = 1.0;
                return IS_FREQ | IS_ATT;
            }

<<EOF>>     return 0;

.           THROW( INVALID_INPUT_EXCEPTION );


        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
rs_sml01_read_table( FILE * fp )
{
    static bool is_restart = UNSET;
    int type;


    if ( ( rs_sml01_in = fp ) == NULL )
    {
        print( FATAL, "Internal error while reading attenuation table.\n" );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        rs_sml01_restart( rs_sml01_in );
    else
        is_restart = SET;

    rs_sml01_Lc = 1;

    TRY
    {
        rs_sml01_new_table_entry( TABLE_INIT, 0.0 );
        while( ( type = rs_sml01_lex( ) ) != 0 )
            rs_sml01_new_table_entry( type,
                                        T_atod( rs_sml01_text ) * unit_fac );
        rs_sml01_new_table_entry( TABLE_END, 0.0 );
        TRY_SUCCESS;
    }
    CATCH( EOF_IN_COMMENT_EXCEPTION )
    {
        print( FATAL, "Unexpected end of table file '%s' in comment starting "
               "at line %ld.\n", rs_sml01.table_file, rs_sml01_Comm_Lc );
        THROW( EXCEPTION );
    }
    CATCH( DANGLING_END_OF_COMMENT )
    {
        print( FATAL, "End of comment found in table file '%s' at line %ld.\n",
               rs_sml01.table_file, rs_sml01_Lc );
        THROW( EXCEPTION );
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid entry in table file '%s' at line %ld.\n",
               rs_sml01.table_file, rs_sml01_Lc );
        THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW( );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static int
rs_sml01_get_unit( void )
{
    char *cp;
    bool done = UNSET;
    int type = 0;


    for ( cp = rs_sml01_text; ! done && *cp; cp++ )
    {
        switch ( *cp )
        {
             case ' ' : case '\t' :
                 *cp = '\0';
                 break;

             case 'k' :
                  *cp = '\0';
                  unit_fac = 1.0e3;
                  type = IS_FREQ;
                  done = SET;
                  break;

             case 'M' :
                  *cp = '\0';
                  type = IS_FREQ;
                  unit_fac = 1.0e6;
                  done = SET;
                  break;

             case 'G' :
                  *cp = '\0';
                  type = IS_FREQ;
                  unit_fac = 1.0e9;
                  done = SET;
                  break;

             case 'd' :
                  *cp = '\0';
                  type = IS_ATT;
                  unit_fac = 1.0;
                  done = SET;
                  break;
        }
    }

    return type;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
rs_sml01_new_table_entry( int    type,
                          double val )
{
    static bool is_freq = UNSET,
                is_att = UNSET;


    /* Initialize everything when type is -1 */

    if ( type == TABLE_INIT )
    {
        is_freq = is_att = UNSET;
        rs_sml01.att_table = T_malloc( TABLE_CHUNK_SIZE
                                       * sizeof *rs_sml01.att_table );
        cur_table_len = TABLE_CHUNK_SIZE;
        return;
    }

    /* If type is 0 this is the final check after all data have been read */

    if ( type == TABLE_END )
    {
        /* Check that we're not expecting another attenuation or frequency
           while the end of the file is already reached */

        if ( is_freq ^ is_att )                /* one of them is still set ? */
        {
            print( FATAL, "Missing final %s in table file '%s'.\n",
                   is_freq ? "attenuation" : "frequency",
                   rs_sml01.table_file );
            THROW( EXCEPTION );
        }

        /* Check that there are at least two entries in the table */

        if ( rs_sml01.att_table_len < 2 )
        {
            print( FATAL, "Table file '%s' contains less than 2 entries.\n",
                   rs_sml01.table_file );
            THROW( EXCEPTION );
        }

        /* Cut back table size to the amount of memory really needed and sort
           the table in ascending order with respect to the frequencies, then
           set lowest and highest frequency in structure for synthesizer */

        rs_sml01.att_table =T_realloc( rs_sml01.att_table,
                                         rs_sml01.att_table_len
                                       * sizeof *rs_sml01.att_table );
        cur_table_len = rs_sml01.att_table_len;
        qsort( rs_sml01.att_table, rs_sml01.att_table_len,
               sizeof ( Att_Table_Entry_T ), rs_sml01_comp );
        rs_sml01.min_table_freq = rs_sml01.att_table[ 0 ].freq;
        rs_sml01.max_table_freq =
                     rs_sml01.att_table[ rs_sml01.att_table_len - 1 ].freq;
        return;
    }

    /* Two frequencies or two attenuations in a row aren't allowed (at least
       when the first entry wasn't used up) */

    if ( type == IS_ATT && is_att )
    {
        print( FATAL, "Error while reading table file '%s': Two attenuation "
               "values in a row at line %ld.\n",
               rs_sml01.table_file, rs_sml01_Lc );
        THROW( EXCEPTION );
    }

    if ( type == IS_FREQ && is_freq )
    {
        print( FATAL, "Error while reading table file '%s': Two frequency "
               "values in a row at line %ld.\n",
               rs_sml01.table_file, rs_sml01_Lc );
        THROW( EXCEPTION );
    }

    /* If the type is indetermined either set it to the not yet set type
       or to frequency if none has been set yet - thus the default, when no
       units are given is frequency first, then the attenuation */

    if ( type == ( IS_FREQ | IS_ATT ) )
    {
        if ( is_freq )
           type = IS_ATT;
        else
            type = IS_FREQ;
    }

    /* If necessary extend the length of the table */

    if ( cur_table_len <= rs_sml01.att_table_len )
    {
        cur_table_len += TABLE_CHUNK_SIZE;
        rs_sml01.att_table = T_realloc( rs_sml01.att_table,
                                          cur_table_len
                                        * sizeof *rs_sml01.att_table );
    }

    if ( type == IS_FREQ )
    {
        if ( val < 0 )
        {
            print( FATAL, "Invalid negative RF frequency found in the table "
                   "file '%s' at line %ld.\n",
                   rs_sml01.table_file, rs_sml01_Lc );
            THROW( EXCEPTION );
        }

        if ( val > MAX_FREQ || val < MIN_FREQ )
            print( WARN, "Frequency of %g MHz in table file '%s', line %ld, "
                   "is not within range of synthesizer (%f kHz - %f MHz).\n",
                   val * 1.0e-6, rs_sml01.table_file, rs_sml01_Lc,
                   MIN_FREQ * 1.0e-3, MAX_FREQ * 1.0e-6 );
        rs_sml01.att_table[ rs_sml01.att_table_len ].freq = val;
        is_freq = SET;
    }
    else                /* IS_ATT */
    {
        if ( val > rs_sml01.min_attenuation - MAX_ATTEN )
        {
            print( FATAL, "Attenuation of %.1f dB in table file '%s', line "
                   "%ld, can't be achieved, maximum dynamic range is "
                   "%.1f dB.\n", val, rs_sml01.table_file, rs_sml01_Lc,
                   rs_sml01.min_attenuation - MAX_ATTEN );
            THROW( EXCEPTION );
        }
        rs_sml01.att_table[ rs_sml01.att_table_len ].att = val;
        is_att = SET;
    }

    /* If both frequency and attenuation have been set increment entry count
       and prepare for the next pair of data */

    if ( is_freq && is_att )
    {
        rs_sml01.att_table_len++;
        is_freq = is_att = UNSET;
    }
}


/*---------------------------------------------------------------*
 * Comparison function sorting of the table entries in ascending
 * order of frequencies.
 *---------------------------------------------------------------*/

static int
rs_sml01_comp( const void * a,
               const void * b )
{
    Att_Table_Entry_T *A = ( Att_Table_Entry_T * ) a,
                      *B = ( Att_Table_Entry_T * ) b;


    if ( A->freq == B->freq )
    {
        print( FATAL, "Frequency of %lf MHz appears twice in table file "
               "'%s'\n", 1.0e-6 * A->freq, rs_sml01.table_file );
        THROW( EXCEPTION );
    }

    return A->freq < B->freq ? -1 : 1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
