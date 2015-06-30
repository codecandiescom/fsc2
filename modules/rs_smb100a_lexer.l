/* -*-C-*-
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noyy_top_state noinput nounput never-interactive

%{

#include "rs_smb100a.h"


#define IS_FREQ     1
#define IS_ATT      2
#define TABLE_INIT -1
#define TABLE_END   0
#define TABLE_CHUNK_SIZE 128

static int rs_smb100a_get_unit( void );
static void rs_smb100a_new_table_entry( int    type,
                                        double val );
static void finalize_table( bool is_freq,
                            bool is_att );
 static int rs_smb100a_comp( const void * a,
                             const void * b );

static long rs_smb100a_Lc,
            rs_smb100a_Comm_Lc;
static double unit_fac = 1.0;

static long cur_table_len;

/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int rs_smb100a_get_lineno( void );
FILE * rs_smb100a_get_in( void );
FILE * rs_smb100a_get_out( void );
int rs_smb100a_get_leng( void );
char * rs_smb100a_get_text( void );
void rs_smb100a_set_lineno( int );
void rs_smb100a_set_in( FILE * );
void rs_smb100a_set_out( FILE * );
int rs_smb100a_get_debug( void );
void rs_smb100a_set_debug( int );
int rs_smb100a_lex_destroy( void );
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
{REM1}      rs_smb100a_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      rs_smb100a_Lc++;

            /* handling of C style comment */
{REM3}      {
                rs_smb100a_Comm_Lc = rs_smb100a_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          rs_smb100a_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     THROW( EOF_IN_COMMENT_EXCEPTION );

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                rs_smb100a_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT );

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      rs_smb100a_Lc++;

            /* dumps trailing white space */
{TWS}       rs_smb100a_Lc++;

            /* dump all other forms of white */
{WS}

            /* count lines */
\n          rs_smb100a_Lc++;

{DU}        return rs_smb100a_get_unit( );

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
rs_read_table( FILE       * fp )
{
    static bool is_restart = false;

    if ( ( rs_smb100a_in = fp ) == NULL )
    {
        print( FATAL, "Internal error while reading table file '%s'.\n" );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        rs_smb100a_restart( rs_smb100a_in );
    else
        is_restart = true;

    rs_smb100a_Lc = 1;

    TRY
    {

        rs_smb100a_new_table_entry( TABLE_INIT, 0.0 );

        int type;
        while( ( type = rs_smb100a_lex( ) ) != 0 )
            rs_smb100a_new_table_entry( type,
                                        T_atod( rs_smb100a_text ) * unit_fac );

        rs_smb100a_new_table_entry( TABLE_END, 0.0 );
        TRY_SUCCESS;
    }
    CATCH( EOF_IN_COMMENT_EXCEPTION )
    {
        print( FATAL, "Unexpected end of table file '%s' in comment starting "
               "at line %ld.\n", rs->table.name, rs_smb100a_Comm_Lc );
        THROW( EXCEPTION );
    }
    CATCH( DANGLING_END_OF_COMMENT )
    {
        print( FATAL, "End of comment found in table file '%s' at line %ld.\n",
               rs->table.name, rs_smb100a_Lc );
        THROW( EXCEPTION );
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid entry in table file '%s' at line %ld.\n",
               rs->table.name, rs_smb100a_Lc );
        THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static int
rs_smb100a_get_unit( void )
{
    bool done = false;
    int type = 0;

    for ( char * cp = rs_smb100a_text; ! done && *cp; cp++ )
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
                  done = true;
                  break;

             case 'M' :
                  *cp = '\0';
                  type = IS_FREQ;
                  unit_fac = 1.0e6;
                  done = true;
                  break;

             case 'G' :
                  *cp = '\0';
                  type = IS_FREQ;
                  unit_fac = 1.0e9;
                  done = true;
                  break;

             case 'd' :
                  *cp = '\0';
                  type = IS_ATT;
                  unit_fac = 1.0;
                  done = true;
                  break;
        }
    }

    return type;
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void
rs_smb100a_new_table_entry( int    type,
                            double val )
{
    static bool is_freq = false,
                is_att = false;

    /* Initialize everything when type is -1 */

    if ( type == TABLE_INIT )
    {
        is_freq = is_att = false;
        rs->table.table = T_malloc(   TABLE_CHUNK_SIZE
                                    * sizeof *rs->table.table );
        cur_table_len = TABLE_CHUNK_SIZE;
        return;
    }

    /* If type is 0 this is the final check after all data have been read */

    if ( type == TABLE_END )
    {
        finalize_table( is_freq, is_att );
        return;
    }

    /* Two frequencies or two attenuations in a row aren't allowed (at least
       when the first entry wasn't used up) */

    if ( type == IS_ATT && is_att )
    {
        print( FATAL, "Error while reading table file '%s': Two attenuation "
               "values in a row at line %ld.\n", rs->table.name, rs_smb100a_Lc );
        THROW( EXCEPTION );
    }

    if ( type == IS_FREQ && is_freq )
    {
        print( FATAL, "Error while reading table file '%s': Two frequency "
               "values in a row at line %ld.\n", rs->table.name, rs_smb100a_Lc );
        THROW( EXCEPTION );
    }

    /* If the type is indetermined either set it to the not yet set type
       or to frequency if none has been set yet - thus the default, when no
       units are given, is frequency first and then the attenuation */

    if ( type == ( IS_FREQ | IS_ATT ) )
    {
        if ( is_freq )
           type = IS_ATT;
        else
            type = IS_FREQ;
    }

    /* If necessary extend the length of the table */

    if ( cur_table_len <= rs->table.len )
    {
        cur_table_len += TABLE_CHUNK_SIZE;
        rs->table.table = T_realloc( rs->table.table,
                                     cur_table_len * sizeof *rs->table.table );
    }

    if ( type == IS_FREQ )
    {
        if ( val > rs->freq.max_freq || val < rs->freq.min_freq )
            print( WARN, "Frequency of %g MHz in table file '%s', line %ld, "
                   "is not within range of synthesizer (%f kHz - %f GHz).\n",
                   val * 1.0e-6, rs->table.name, rs_smb100a_Lc,
                   rs->freq.min_freq * 1.0e-3, rs->freq.max_freq * 1.0e-9 );
        rs->table.table[ rs->table.len ].freq = val;
        is_freq = true;
    }
    else                /* IS_ATT */
    {
        rs->table.table[ rs->table.len ].att = val;
        is_att = true;
    }

    /* If both frequency and attenuation have been set increment entry count
       and prepare for the next pair of data */

    if ( is_freq && is_att )
    {
        rs->table.len++;
        is_freq = is_att = false;
    }
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static
void
finalize_table( bool is_freq,
                bool is_att )
{
    /* Check that we're not expecting another attenuation or frequency
       while the end of the file is already reached */

    if ( is_freq ^ is_att )                /* one of them is still set ? */
    {
        print( FATAL, "Missing final %s in table file '%s'.\n",
               is_freq ? "attenuation" : "frequency", rs->table.name );
        THROW( EXCEPTION );
    }

    /* Check that there are at least two entries in the table */

    if ( rs->table.len < 2 )
    {
        print( FATAL, "Table file '%s' contains less than 2 entries.\n",
               rs->table.name );
        THROW( EXCEPTION );
    }

    /* Cut back table size to the amount of memory really needed and sort
       the table in ascending order with respect to the frequencies, then
       set lowest and highest frequency. */

    rs->table.table = T_realloc( rs->table.table,
                                 rs->table.len * sizeof *rs->table.table );

    qsort( rs->table.table, rs->table.len, sizeof *rs->table.table,
           rs_smb100a_comp );

    rs->table.min_freq = rs->table.table[ 0 ].freq;
    rs->table.max_freq = rs->table.table[ rs->table.len - 1 ].freq;
}


/*---------------------------------------------------------------*
 * Comparison function sorting of the table entries in ascending
 * order of frequencies.
 *---------------------------------------------------------------*/

static int
rs_smb100a_comp( void const * a,
         void const * b )
{
    Table_Entry_T * A = ( Table_Entry_T * ) a,
                  * B = ( Table_Entry_T * ) b;


    if ( A->freq == B->freq )
    {
        print( FATAL, "Frequency of %lf MHz appears twice in table file "
               "'%s'\n", 1.0e-6 * A->freq, rs->table.name );
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
