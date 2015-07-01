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


#include "rs_smb100a.h"
#include <unistd.h>


static char * complete_table_file_name( char const * name );


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
table_init( void )
{
    if ( FSC2_MODE != PREPARATION )
        return;

    rs->table.name = NULL;
    rs->table.table = NULL;
    rs->table.len = 0;
    rs->table.enabled = false;
    rs->table.ref_freq = 0;
    rs->table.att_at_ref_freq = 0;
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
table_cleanup( void )
{
    if ( FSC2_MODE != TEST )
        rs->table.table = T_free( rs->table.table );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
table_load_file( Var_T * v )
{
    /* Assemble the table file name, use default name if no file name
       has been given. */

    if ( ! v || ( v->type == STR_VAR && ! *v->val.sptr ) )
    {
#if ! defined DEFAULT_TABLE_NAME
        print( FATAL, "Module was not compiled with a default table name.\n" );
        THROW( EXCEPTION );
#endif

        if ( DEFAULT_TABLE_FILE[ 0 ] ==  '/' )
            rs->table.name = T_strdup( DEFAULT_TABLE_FILE );
        else
            rs->table.name = get_string( "%s%s%s", libdir, slash( libdir ),
                                         DEFAULT_TABLE_FILE );
    }
    else
    {
        if ( v->type != STR_VAR )
        {
            print( FATAL, "Expect a single string with the name of the table "
                   "file.\n" );
            THROW( EXCEPTION );
        }

        rs->table.name = complete_table_file_name( v->val.sptr );
        too_many_arguments( v );
    }

    // Try to open it

    FILE * tf;
    if ( ! ( tf = fopen( rs->table.name, "r" ) ) )
    {
        print( FATAL, "Failed to open table file '%s'.\n", rs->table.name );
        T_free( rs->table.name );
        THROW( EXCEPTION );
    }

    // Finally try to read it in an parse it

    TRY
    {
        rs_read_table( tf );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( tf );
        rs->table.name = T_free( rs->table.name );
        RETHROW;
    }

    fclose( tf );
    rs->table.name = T_free( rs->table.name );
    rs->table.enabled = true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
char *
complete_table_file_name( char const * name )
{
    char * n = NULL;

    if ( name[ 0 ] == '~' )
        n =  get_string( "%s%s%s", getenv( "HOME" ),
                         name[ 1 ] != '/' ? "/" : "", name + 1 );
    else
    {
        struct stat buf;
        if ( ! stat( name, &buf ) )
            n = T_strdup( name );
        else if ( errno == ENOENT && ! strchr( name, '/' ) )
            n = get_string( "%s%s%s", libdir, slash( libdir ), name );
    }

    if ( ! n )
    {
        print( FATAL, "Could not find any file matching '%s'.\n", name );
        THROW( EXCEPTION );
    }

    return n;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
table_ref_freq( void )
{
    return rs->table.ref_freq;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double
table_set_ref_freq( double freq )
{
    if ( freq <= 0 )
    {
        rs->table.enabled = 0;
        rs->table.att_at_ref_freq = 0;
        return rs->table.ref_freq = 0;
    }

    if ( ! rs->table.table )
    {
        print( FATAL, "Can't set attenuation reference frequency, no table "
               "has been loaded.\n" );
        THROW( EXCEPTION );
    }

    if ( freq < rs->table.min_freq || freq > rs->table.max_freq )
    {
        print( FATAL, "Attenuation reference frequency of %g MHz outside of "
               "range %g MHz to %g MHz covered by table.\n", freq * 1.0e-6,
               rs->table.min_freq * 1.0e-6, rs->table.max_freq * 1.0e-6 );
        THROW( EXCEPTION );
    }

    rs->table.enabled = true;
    rs->table.att_at_ref_freq = table_att_offset( freq );
    return rs->table.ref_freq = freq;
}

/*-----------------------------------------------*
 * Calculates attenuatiom offsey from the table
 * for a given frequency by interpolation.
 *-----------------------------------------------*/

double
table_att_offset( double freq )
{
    if ( ! rs->table.enabled )
        return 0;

    /* First check that the frequency is covered by the table, if it isn't
       print out a warning and return attenuation for the smallest or
       highest frequency */

    if ( freq < rs->freq.min_freq )
    {
        print( WARN, "Frequency of %f MHz below range covered by table, "
               "using attenuation for lowest frequency.\n", freq * 1.0e-6 );
        return rs->table.table[ 0 ].att - rs->table.att_at_ref_freq;
    }

    if ( freq > rs->table.max_freq )
    {
        print( WARN, "Frequency of %f MHz above range covered by table, "
               "using attenuation for highest frequency.\n", freq * 1.0e-6 );
        return   rs->table.table[ rs->table.len - 1 ].att
               - rs->table.att_at_ref_freq;
    }

    /* Find the indices of the two entries in the table (that has been sorted
       in ascending order of frequencies) bracketing the frequency. Since in
       most cases the frequencies in the table are equally spaced, we first
       try to find the entry by interpolating. If this doesn't succeeds the
       frequencies can't be uniformly spaced and we used simple bisecting.*/

    long i_low = 0;
    long i_high = rs->table.len - 1;

    if ( freq == rs->table.table[ i_low ].freq )
        return rs->table.table[ i_low ].att - rs->table.att_at_ref_freq;
    if ( freq == rs->table.table[ i_high ].freq )
        return rs->table.table[ i_high ].att - rs->table.att_at_ref_freq;

    long i_cur = lrnd( floor(   ( i_high - i_low )
                              * ( freq - rs->table.table[ i_low ].freq )
                              / (   rs->table.table[ i_high ].freq
                                  - rs->table.table[ i_low ].freq ) ) ) + i_low;

    if ( freq > rs->table.table[ i_cur ].freq )
    {
        i_low = i_cur;
        if ( freq < rs->table.table[ i_cur + 1 ].freq )
            i_high = i_cur + 1;
    }
    else
        i_high = i_cur;

    while ( i_high - i_low > 1 )
    {
        if ( freq > rs->table.table[ i_cur ].freq )
            i_low = i_cur;
        else
            i_high = i_cur;
        i_cur = ( i_high + i_low ) >> 1;
    }

    /* Now do a linear interpolation for the attenuation between the
       bracketing frequencies we have data for */

    return   (   rs->table.table[ i_high ].att
               - rs->table.table[ i_low ].att )
           * ( freq - rs->table.table[ i_low ].freq )
           / (   rs->table.table[ i_high ].freq
               - rs->table.table[ i_low ].freq )
           + rs->table.table[ i_low ].att
           - rs->table.att_at_ref_freq;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
