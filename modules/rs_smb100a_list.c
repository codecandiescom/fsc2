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
#include "vxi11_user.h"


static void list_send( char const   * type,
                       double const * data,
                       long           length );


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_init( void )
{
	rs->list.processing_list = false;
    rs->list.len             = 0;
    rs->list.max_len         = 2000;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->list.default_name = "fsc2_def_list";
		rs->list.name = NULL;
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_cleanup( void )
{
	rs->list.name = T_free( rs->list.name );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_select( char const * name )
{
    if ( name && *name )
        list_check_list_name( name );
    else
        name = rs->list.default_name;

    // In test mode we simply have to accept the name

    if ( FSC2_MODE != EXPERIMENT )
    {
        rs->list.len = 101;
        T_free( rs->list.name );
        rs->list.name = T_strdup( name );
		return;
    }

    // Check if the list exists and has any points, otherwise give up

	char * cmd = NULL;
	CLOBBER_PROTECT( cmd );

	TRY
	{
		cmd = T_malloc( 12 + strlen( name ) );
		sprintf( cmd, "LIST:SEL \"%s\"", name );
		rs_write( cmd );

		char reply[ 50 ];
		rs_talk( "LIST:FREQ:POIN?", reply, 50 );

        int list_len = query_int( reply );
		if ( list_len < 1 )
		{
            sprintf( cmd, "LIST:DEL \"%s\"", name );
			rs_write( cmd );

            rs->list.len = 0;
			print( FATAL, "List '%s' does not exist or is empty.\n", name );
			THROW( EXCEPTION );
		}

        rs->list.len  = list_len;
		rs->list.name = T_free( rs->list.name );
		rs->list.name = T_strdup( name );

		T_free( cmd );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( cmd );
		RETHROW;
	}
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_setup_A( double const * freqs,
			  double const * pows,
			  long           len,
			  char   const * name )
{
    // Check arguments

    if ( name && *name )
        list_check_list_name( name );
    else
        name = rs->list.default_name;

    if ( ! len )
	{
		print( FATAL, "Empty list of frequencies.\n" );
		THROW( EXCEPTION );
	}

	if ( len > rs->list.max_len )
	{
		print( FATAL, "List is too long, maximum number of points in list "
               "is %d.\n", rs->list.max_len );
		THROW( EXCEPTION );
	}

    // Check all frequencies and powers and store them

	double * freq_list = NULL;
	double * pow_list  = NULL;

	CLOBBER_PROTECT( freq_list );
	CLOBBER_PROTECT( pow_list );

	TRY
	{
		freq_list = T_malloc( len * sizeof *freq_list );
		pow_list  = T_malloc( len * sizeof *pow_list );

		for ( long i = 0; i < len; i++ )
		{
			freq_list[ i ] = freq_check_frequency( freqs[ i ] );
            double p = pows[ i ] - table_att_offset( freq_list[ i ] );
			pow_list[ i ]  = pow_check_power( p, freq_list[ i ] );
		}

        rs->list.len = len;

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( pow_list );
		T_free( freq_list );
		RETHROW;
	}

	if ( FSC2_MODE != EXPERIMENT )
	{
        rs->list.len = len;
		T_free( pow_list );
		T_free( freq_list );
		return;
	}

	char * cmd = NULL;
	CLOBBER_PROTECT( cmd );

	TRY
	{
        // If necessary sselect new list name

        if ( ! rs->list.name || strcmp( rs->list.name, name ) )
        {
            cmd = get_string( "LIST:SEL \"%s\"", name );
            rs_write( cmd );
        }

        rs->list.name = T_free( rs->list.name );
        rs->list.name = T_strdup( name );

        list_send( "FREQ", freq_list, len );
        list_send( "POW", pow_list, len );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( cmd );
		T_free( freq_list );
		T_free( pow_list );
		RETHROW;
	}

	T_free( cmd );
	T_free( freq_list );
	T_free( pow_list );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_setup_B( double const * freqs,
			  double         pow,
			  long           len,
			  char   const * name )
{
	double * pows = T_malloc( len * sizeof *pows );
	CLOBBER_PROTECT( pows );

	for ( long i = 0; i < len; i++ )
		pows[ i ] = pow;

	TRY
	{
		list_setup_A( freqs, pows, len, name );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( pows );
		RETHROW;
	}
	
	T_free( pows );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_setup_C( double const * freqs,
			  long           len,
			  char   const * name )
{
	list_setup_B( freqs, rs->pow.req_pow, len, name );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
list_send( char const   * type,
           double const * data,
           long           length )
{
    char * cmd = 0;
    CLOBBER_PROTECT( cmd );

    TRY
    {
        if ( rs->use_binary )
        {
            int cnt = floor( log10( 8 * length ) ) + 1;
            size_t total = 8 + strlen( type ) + cnt + 8 * length;

            cmd = T_malloc( total );
            memcpy( cmd + sprintf( cmd, "LIST:%s #%d%ld", type,
                                   cnt, 8 * length ), data, 8 * length );
            rs_write_n( cmd, total );
        }
        else
        {
            cmd = T_malloc( 7 + strlen( type ) + 15 * length );
            char * np = cmd + sprintf( cmd, "LIST:%s ", type );

            for ( long i = 0; i < length; i++ )
                np += sprintf( np, "%.2f,", data[ i ] );
            *--np = '\0';

            rs_write( cmd );
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( cmd );
        RETHROW;
    }

    T_free( cmd );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
list_length( void )
{
    return rs->list.len;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_start( bool relearn_list )
{
    if ( ! rs->list.name )
	{
		print( FATAL, "No list has been selected.\n" );
		THROW( EXCEPTION );
	}

    if ( rs->list.processing_list )
        return;

	if ( FSC2_MODE != EXPERIMENT )
	{
		if ( ! outp_state( ) )
			outp_set_state( true );
		return;
	}

    // Before anyhing can be done with the list output must be on

    if ( ! outp_state( ) )
        outp_set_state( true );

    rs_write( "LIST:MODE STEP" );
	rs_write( "LIST:TRIG:SOUR EXT" );

    if ( relearn_list )
    {
        rs_write( "LIST:LEAR" );
        wait_opc( 10 );
    }

    freq_list_mode( true );

    if ( ! relearn_list )
        wait_opc( 10 );

    rs->list.processing_list = true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_stop( bool keep_rf_on )
{
    if ( ! rs->list.processing_list )
        return;

    rs->list.processing_list = false;

    if ( ! keep_rf_on )
        outp_set_state( false );

    freq_list_mode( false );

	if ( FSC2_MODE == EXPERIMENT )
		rs_write( "LIST:RES" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
list_index( void )
{
    if ( ! rs->list.processing_list )
        return -1;

	if ( FSC2_MODE != EXPERIMENT )
		return 0;

    return query_int( "LIST:INDEX?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
list_delete_list( char const * name )
{
	char const * n;

    if ( ! name || ! *name )
    {
        if ( ! rs->list.name )
            return false;
        else
            n = rs->list.name;
    }
    else
	{
        list_check_list_name( name );
		n = name;
	}

    if ( FSC2_MODE == TEST )
    {
        if ( ! strcmp( n, rs->list.name ) )
            rs->list.name = T_free( rs->list.name );
        return true;
    }

	char * cmd = get_string( "LIST:DEL \"%s\"", n );
    CLOBBER_PROTECT( cmd );

	TRY
	{
		rs_write( cmd );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( cmd );
		RETHROW;
	}

	T_free( cmd );

    if ( ! strcmp( n, rs->list.name ) )
        rs->list.name = T_free( rs->list.name );

    return true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

char*
list_get_all( void )
{
    if ( FSC2_MODE == TEST )
        return T_strdup( "test_list1,test_list2,test_list3" );

    size_t length = 10000;
    char * reply = T_malloc( length );
    CLOBBER_PROTECT( reply );
    char * r;

    TRY
    {
        size_t l = rs_talk( "LIST:CAT?", reply, length );
        if ( l == length )
        {
            reply [ l - 1 ] = '\0';
            print( WARN, "List of lists too long, truncated.\n" );
            r = reply;
        }
        else
            r = T_realloc( reply, l + 1 );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( reply );
        RETHROW;
    }

    return r;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double *
list_frequencies( void )
{
    if ( ! rs->list.name )
	{
		print( FATAL, "No list has been selected.\n" );
		THROW( EXCEPTION );
	}

    if ( FSC2_MODE == TEST )
    {
        double * f = T_malloc( rs->list.len * sizeof *f );
        f[ 0 ] = 1.0e6;
        for ( int i = 1; i < rs->list.len; i++ )
            f[ i ] = f[ i - 1 ] + 1.0e3;
        return f;
    }

    return query_list( "LIST:FREQ?", rs->list.len );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

double *
list_powers( void )
{
    if ( ! rs->list.name )
	{
		print( FATAL, "No list has been selected.\n" );
		THROW( EXCEPTION );
	}

    if ( FSC2_MODE == TEST )
    {
        double * p = T_malloc( rs->list.len * sizeof *p );
        for ( int i = 0; i < rs->list.len; i++ )
            p[ i ] = 0;
        return p;
    }

    return query_list( "LIST:POW?", rs->list.len );
}


/*----------------------------------------------------*
 * We better only accept list names starting with a
 * normal letter, followed by more letters, numbers
 * or underscores.
 *----------------------------------------------------*/

void
list_check_list_name( char const * name )
{
    if ( ! name || ! *name )
	{
		print( FATAL, "Invalid empty list name.\n" );
		THROW( EXCEPTION );
	}

	for ( size_t i = 0; i < strlen( name ); i++ )
		if ( ! isalnum( ( int ) name[ i ] ) && name[ i ] != '_' )
		{
			print( FATAL, "Invalid list name \"%s\", may only contain "
				   "letters, numbers and underscores.\n", name );
			THROW( EXCEPTION );
		}

	if ( ! isalpha( ( int ) name[ 0 ] ) )
	{
		print( FATAL, "Invalid list name \"%s\", must start with a letter.\n",
			   name );
	}
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
