/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

struct PCI_MIO_16E_1 pci_mio_16e_1;


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int pci_mio_16e_1_init_hook( void )
{
	int i;


	pci_mio_16e_1.ai_state.is_busy = UNSET;
	pci_mio_16e_1.ai_state.is_channel_setup = UNSET;
	pci_mio_16e_1.ai_state.is_acq_setup = UNSET;
	pci_mio_16e_1.ai_state.num_channels = 0;
	pci_mio_16e_1.ai_state.data_per_channel = 0;
	pci_mio_16e_1.ai_state.ranges = NULL;
	pci_mio_16e_1.ai_state.polarities = NULL;

	for ( i = 0; i < 2; i++ )
		pci_mio_16e_1.gpct_state.states[ i ] = 0;

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int pci_mio_16e_1_end_of_test_hook( void )
{
	if ( pci_mio_16e_1.ai_state.ranges != NULL )
		pci_mio_16e_1.ai_state.ranges =
									   T_free( pci_mio_16e_1.ai_state.ranges );
		if ( pci_mio_16e_1.ai_state.polarities != NULL )
			pci_mio_16e_1.ai_state.polarities =
								   T_free( pci_mio_16e_1.ai_state.polarities );

		return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int pci_mio_16e_1_exp_hook( void )
{
	if ( ( pci_mio_16e_1.board = ni_daq_open( BOARD_DEVICE_FILE ) ) < 0 )
		switch ( pci_mio_16e_1.board )
		{
			case NI_DAQ_OK :
				break;

			case NI_DAQ_ERR_NSB :
				print( FATAL, "Invalid board name, check configuration "
					   "file for board.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_NDV :
				print( FATAL, "Driver for board not loaded.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_ACS :
				print( FATAL, "No permissions to open device file for "
					   "board.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_DFM :
				print( FATAL, "Device file for board missing.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_DFP :
				print( FATAL, "Unspecified error when opening device file for "
					   "board.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_BBS :
				print( FATAL, "Board already in use by another program.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_INT :
				print( FATAL, "Internal error in board driver or library.\n" );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Unrecognized error when trying to access the "
					   "board.\n" );
				THROW( EXCEPTION );
		}

	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int pci_mio_16e_1_end_of_exp_hook( void )
{
	ni_daq_close( pci_mio_16e_1.board );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void pci_mio_16e_1_exit_hook( void )
{
	/* This shouldn't be necessary, I just want to make 100% sure that
	   the device file for the board is really closed */

	ni_daq_close( pci_mio_16e_1.board );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *daq_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
