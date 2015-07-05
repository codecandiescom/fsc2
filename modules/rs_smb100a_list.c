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
list_delete( char const * name )
{
    if ( name && *name )
        list_check_list_name( name );

    if ( FSC2_MODE == TEST )
    {
        if (    ! name || ! *name
             || ( rs->list.name && ! strcmp( name, rs->list.name ) ) )
            rs->list.name = T_free( rs->list.name );
        return;
    }

    char * cmd = NULL;
    CLOBBER_PROTECT( cmd );

    TRY
    {
        if ( name && *name )
            cmd = get_string( "LIST:DEL \"%s\"", name );
        else
            cmd = get_string( "LIST:DEL:ALL" );

        rs_write( cmd );

        if (    ! name || ! *name
             || ( rs->list.name && ! strcmp( name, rs->list.name ) ) )
            rs->list.name = T_free( rs->list.name );

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
        if ( ! rs->list.name || strcmp( rs->list.name, name ) )
        {
            cmd = get_string( "LIST:SEL \"%s\"", name );
            rs_write( cmd );
        }

		rs->list.name = T_free( rs->list.name );
		rs->list.name = T_strdup( name );

		cmd = T_realloc( cmd, 12 + 14 * len );
		strcpy( cmd, "LIST:FREQ " );

		char *np = cmd + strlen( cmd );

		for ( long i = 0; i < len; i++ )
			np += sprintf( np, "%.2f,", freq_list[ i ] );
		*--np = '\0';

		rs_write( cmd );

		strcpy( cmd, "LIST:POW " );
		np = cmd + strlen( cmd );

		for ( long i = 0; i < len; i++ )
			np += sprintf( np, "%.2f,", pow_list[ i ] );
		*--np = '\0';

		rs_write( cmd );

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

int
list_length( void )
{
    return rs->list.len;
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

void
list_start( void )
{
    if ( ! rs->list.name )
	{
		print( FATAL, "No list has been set up or selected.\n" );
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

    rs_write( "LIST:LEAR" );
    rs_write( "LIST:MODE STEP" );
	rs_write( "LIST:TRIG:SOUR EXT" );
    freq_list_mode( true );
    rs->list.processing_list = true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_stop( bool keep_rf_on )
{
    if ( ! rs->list.processing_list )
        return;

    if ( ! keep_rf_on )
        outp_set_state( false );

    freq_list_mode( false );
    rs->list.processing_list = false;

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

void
list_delete_list( char const * name )
{
	char * n;

    if ( ! name || ! *name )
    {
        if ( ! rs->list.name )
            return;
        else
            n = rs->list.name;
    }
    else
	{
        list_check_list_name( name );
		n = ( char * ) name;
	}

	char *cmd = T_malloc( 12 + strlen( n ) );
	strcat( strcat( strcpy( cmd, "LIST:DEL \"" ), n ), "\"" );

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
