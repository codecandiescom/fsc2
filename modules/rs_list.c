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


#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_init( void )
{
	rs->list.processing_list = false;

	if ( FSC2_MODE == PREPARATION )
	{
		rs->list.default_name = "fsc2_def_list";
		rs->list.name = NULL;
	}
	else if ( FSC2_MODE == TEST )
	{
		if ( rs->list.name )
			rs->list.name = T_strdup( rs->list.name );
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
	list_check_list_name( name );

	char * n = T_strdup( ( name && *name ) ? name : rs->list.default_name );

	if ( FSC2_MODE != EXPERIMENT )
	{
		T_free( rs->list.name );
		rs->list.name = n;
		return;
	}	

	char *cmd = NULL;
	CLOBBER_PROTECT( cmd );

	TRY
	{
		size_t l = strlen( n );
		if ( rs->list.name )
			l = s_max( l, strlen( rs->list.name ) );

		T_malloc( 12 + l );
		strcat( strcat( strcpy( cmd, "LIST:SEL \"" ), n ), "\"" );
		rs_write( cmd );

		char reply[ 50 ];
		rs_talk( "LIST:FREQ:POIN?", reply, 50 );

		if ( query_int( reply ) < 1 )
		{
			strcat( strcat( strcpy( cmd, "LIST:DEL \"" ) , n ), "\"" );
			rs_write( cmd );

			if ( rs->list.name )
			{
				strcat( strcat( strcpy( cmd, "LIST:SEL \"") , rs->list.name ),
						"\"" );
				rs_write( cmd );
			}
			
			print( FATAL, "List \"%s\" does not exist.\n", name );
			THROW( EXCEPTION );
		}

		T_free( rs->list.name );
		rs->list.name = n;

		T_free( cmd );

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( cmd );
		T_free( n );
		RETHROW;
	}
}
    

/*----------------------------------------------------*
 *----------------------------------------------------*/

void
list_setup_A( double const * freqs,
			  double const * pows,
			  size_t         len,
			  char   const * name )
{
    // Check arguments

	list_check_list_name( name );

    if ( ! len )\
	{
		print( FATAL, "Empty list of frequencies.\n" );
		THROW( EXCEPTION );
	}

	if ( len > INT_MAX )
	{
		print( FATAL, "List is too long.\n" );
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

		for ( size_t i = 0; i < len; i++ )
		{
			freq_list[ i ] = freq_check_frequency( freqs[ i ] );
			pow_list[ i ]  = pow_check_power( pows[ i ], freq_list[ i ] );
		}

		T_free( rs->list.name );
		rs->list.name = T_strdup( ( name && *name ) ?
								  name : rs->list.default_name );

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
		T_free( pow_list );
		T_free( freq_list );
		return;
	}

	char * cmd = NULL;
	CLOBBER_PROTECT( cmd );

	TRY
	{
		cmd = T_malloc( 12 + sizeof( rs->list.name ) );
		strcat( strcat( strcpy( cmd, "LIST:SEL \"" ), rs->list.name ), "\"" );
		rs_write( cmd );

		cmd = T_realloc( cmd, 12 + 14 * len );
		strcpy( cmd, "LIST:FREQ " );

		char *np = cmd + strlen( cmd );

		for ( size_t i = 0; i < len; i++ )
			np += sprintf( np, "%.2f,", freq_list[ i ] );
		*--np = '\0';

		rs_write( cmd );

		strcpy( cmd, "LIST:POW " );

		np = cmd + strlen( cmd );

		for ( size_t i = 0; i < len; i++ )
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

void
list_setup_B( double const * freqs,
			  double         pow,
			  size_t         len,
			  char   const * name )
{
	double * pows = T_malloc( len * sizeof *pows );
	CLOBBER_PROTECT( pows );

	for ( size_t i = 0; i < len; ++i )
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
			  size_t         len,
			  char   const * name )
{
	list_setup_B( freqs, pow_power( ), len, name );
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

    rs_write( "LIST:LEAR" );

    if ( ! outp_state( ) )
        outp_set_state( true );

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
	if ( FSC2_MODE != EXPERIMENT )
		return 0;

    return query_int( "LIST::INDEX?" );
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
