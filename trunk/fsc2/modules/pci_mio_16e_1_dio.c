/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "pci_mio_16e_1.h"


/*--------------------------------------------------------------------*/
/* Functions allows to reserve (or un-reserve) the DIO so that in the */
/* following changes to the DIO require a pass-phrase (i.e. when      */
/* calling the function daq_dio_write() and daq_dio_read().           */
/*--------------------------------------------------------------------*/

Var *daq_reserve_dio( Var *v )
{
	bool lock_state = SET;


	if ( v == NULL )
		return vars_push( INT_VAR,
						  pci_mio_16e_1.ao_state.reserved_by ? 1L : 0L );

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Argument isn't a pass-phrase.\n" );
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		lock_state = get_boolean( v->next );
		too_many_arguments( v->next );
	}

	if ( pci_mio_16e_1.dio_state.reserved_by )
	{
		if ( lock_state == SET )
		{
			if ( ! strcmp( pci_mio_16e_1.dio_state.reserved_by, v->val.sptr ) )
				return vars_push( INT_VAR, 1L );
			else
				return vars_push( INT_VAR, 0L );
		}
		else
		{
			if ( ! strcmp( pci_mio_16e_1.dio_state.reserved_by, v->val.sptr ) )
			{
				pci_mio_16e_1.dio_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.dio_state.reserved_by );
				return vars_push( INT_VAR, 1L );
			}
			else
				return vars_push( INT_VAR, 0L );
		}
	}

	if ( ! lock_state )
		return vars_push( INT_VAR, 1L );

	pci_mio_16e_1.dio_state.reserved_by = T_strdup( v->val.sptr );
	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

Var *daq_dio_read( Var *v )
{
	long mask;
	unsigned char bits = 0;
	char *pass;


	if ( v != NULL && v->type == STR_VAR )
	{
		pass = T_strdup( v->val.sptr );

		if ( pci_mio_16e_1.dio_state.reserved_by )
		{
			if ( strcmp( pci_mio_16e_1.dio_state.reserved_by, pass ) )
			{
				print( FATAL, "DIO is reserved, wrong phase-phrase.\n" );
				T_free( pass );
				THROW( EXCEPTION );
			}
		}
		else 
			print( WARN, "Pass-phrase for non-reserved DIO.\n" );

		T_free( pass );
		v = vars_pop( v );
	}
	else if ( pci_mio_16e_1.dio_state.reserved_by )
	{
		print( FATAL, "DIO is reserved, phase-phrase required.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
		mask = 0xFF;
	else
	{
		mask = get_strict_long( v, "DIO bit mask" );
		if ( mask < 0 || mask > 0xFF )
		{
			print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
				   mask );
			THROW( EXCEPTION );
		}
	}

	if ( FSC2_MODE == EXPERIMENT &&
		 ni_daq_dio_read( pci_mio_16e_1.board, &bits,
						  ( unsigned char ) ( mask & 0xFF ) ) < 0 )
		{
			print( FATAL, "Can't read data from DIO.\n" );
			THROW( EXCEPTION );
		}

	return vars_push( INT_VAR, ( long ) bits );
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

Var *daq_dio_write( Var *v )
{
	long bits;
	long mask;
	char *pass;


	if ( v != NULL && v->type == STR_VAR )
	{
		pass = T_strdup( v->val.sptr );

		if ( pci_mio_16e_1.dio_state.reserved_by )
		{
			if ( strcmp( pci_mio_16e_1.dio_state.reserved_by, pass ) )
			{
				print( FATAL, "DIO is reserved, wrong phase-phrase.\n" );
				T_free( pass );
				THROW( EXCEPTION );
			}
		}
		else 
			print( WARN, "Pass-phrase for non-reserved DIO.\n" );

		T_free( pass );
		v = vars_pop( v );
	}
	else if ( pci_mio_16e_1.dio_state.reserved_by )
	{
		print( FATAL, "DIO is reserved, phase-phrase required.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	bits = get_strict_long( v, "DIO value" );

	if ( bits < 0 || bits > 0xFF )
	{
		print( FATAL, "Invalid value of %ld, valid range is [0-255].\n",
			   bits );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		mask = 0xFF;
	else
	{
		mask = get_strict_long( v, "DIO bit mask" );
		if ( mask < 0 || mask > 0xFF )
		{
			print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
				   mask );
			THROW( EXCEPTION );
		}

		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT &&
		 ni_daq_dio_write( pci_mio_16e_1.board,
						   ( unsigned char ) ( bits & 0xFF ),
						   ( unsigned char ) ( mask & 0xFF ) ) < 0 )
	{
		print( FATAL, "Can't write value to DIO.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
