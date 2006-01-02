/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#include "pci_mio_16e_1.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

struct PCI_MIO_16E_1 pci_mio_16e_1, pci_mio_16e_1_stored;


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int pci_mio_16e_1_init_hook( void )
{
	int i;


	pci_mio_16e_1.board = -1;

	pci_mio_16e_1.ai_state.is_busy = UNSET;
	pci_mio_16e_1.ai_state.is_channel_setup = UNSET;
	pci_mio_16e_1.ai_state.is_acq_setup = UNSET;
	pci_mio_16e_1.ai_state.num_channels = 0;
	pci_mio_16e_1.ai_state.data_per_channel = 0;
	pci_mio_16e_1.ai_state.ranges = NULL;
	pci_mio_16e_1.ai_state.polarities = NULL;

	for ( i = 0; i < 2; i++ )
	{
		pci_mio_16e_1.ao_state.reserved_by[ i ] = NULL;
		pci_mio_16e_1.ao_state.external_reference[ i ] = NI_DAQ_DISABLED;
		pci_mio_16e_1.ao_state.polarity[ i ] = NI_DAQ_BIPOLAR;
		pci_mio_16e_1.ao_state.is_used[ i ] = UNSET;

		pci_mio_16e_1.gpct_state.states[ i ] = 0;
	}

	pci_mio_16e_1.dio_state.reserved_by = NULL;

	pci_mio_16e_1.msc_state.daq_clock = PCI_MIO_16E_1_TEST_CLOCK;
	pci_mio_16e_1.msc_state.on_off = PCI_MIO_16E_1_TEST_STATE;
	pci_mio_16e_1.msc_state.speed = PCI_MIO_16E_1_TEST_SPEED;
	pci_mio_16e_1.msc_state.divider = PCI_MIO_16E_1_TEST_DIVIDER;
	pci_mio_16e_1.msc_state.reserved_by = NULL;

	return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int pci_mio_16e_1_test_hook( void )
{
	int i;


	pci_mio_16e_1_stored = pci_mio_16e_1;

	for ( i = 0; i < 2; i++ )
		if ( pci_mio_16e_1.ao_state.reserved_by[ i ] )
			pci_mio_16e_1_stored.ao_state.reserved_by[ i ] =
					CHAR_P T_strdup( pci_mio_16e_1.ao_state.reserved_by[ i ] );

	if ( pci_mio_16e_1.dio_state.reserved_by )
		pci_mio_16e_1_stored.dio_state.reserved_by =
						CHAR_P T_strdup( pci_mio_16e_1.dio_state.reserved_by );

	if ( pci_mio_16e_1.msc_state.reserved_by )
			pci_mio_16e_1_stored.msc_state.reserved_by =
						CHAR_P T_strdup( pci_mio_16e_1.msc_state.reserved_by );

	return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int pci_mio_16e_1_end_of_test_hook( void )
{
	if ( pci_mio_16e_1.ai_state.ranges != NULL )
		pci_mio_16e_1.ai_state.ranges =
							  DOUBLE_P T_free( pci_mio_16e_1.ai_state.ranges );

	if ( pci_mio_16e_1.ai_state.polarities != NULL )
		pci_mio_16e_1.ai_state.polarities =
			  NI_DAQ_BU_POLARITY_P T_free( pci_mio_16e_1.ai_state.polarities );

	return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int pci_mio_16e_1_exp_hook( void )
{
	int i;


	/* Restore state from before the start of the test run */

	for ( i = 0; i < 2; i++ )
		if ( pci_mio_16e_1.ao_state.reserved_by[ i ] )
			pci_mio_16e_1.ao_state.reserved_by[ i ] =
					  CHAR_P T_free( pci_mio_16e_1.ao_state.reserved_by[ i ] );

	if ( pci_mio_16e_1.dio_state.reserved_by )
		pci_mio_16e_1.dio_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.dio_state.reserved_by );

	if ( pci_mio_16e_1.msc_state.reserved_by )
		pci_mio_16e_1.msc_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.msc_state.reserved_by );

	pci_mio_16e_1 = pci_mio_16e_1_stored;

	for ( i = 0; i < 2; i++ )
		if ( pci_mio_16e_1_stored.ao_state.reserved_by[ i ] )
			pci_mio_16e_1.ao_state.reserved_by[ i ] =
			 CHAR_P T_strdup( pci_mio_16e_1_stored.ao_state.reserved_by[ i ] );

	if ( pci_mio_16e_1_stored.dio_state.reserved_by )
		pci_mio_16e_1.dio_state.reserved_by =
				 CHAR_P T_strdup( pci_mio_16e_1_stored.dio_state.reserved_by );

	if ( pci_mio_16e_1_stored.msc_state.reserved_by )
		pci_mio_16e_1.msc_state.reserved_by =
				 CHAR_P T_strdup( pci_mio_16e_1_stored.msc_state.reserved_by );

	if ( ( pci_mio_16e_1.board = ni_daq_open( BOARD_DEVICE_FILE ) ) < 0 )
	{
		switch ( pci_mio_16e_1.board )
		{
			case NI_DAQ_OK :
				break;

			case NI_DAQ_ERR_NSB :
				print( FATAL, "Invalid board name, check configuration "
					   "file for board.\n" );
				break;

			case NI_DAQ_ERR_NDV :
				print( FATAL, "Driver for board not loaded.\n" );
				THROW( EXCEPTION );

			case NI_DAQ_ERR_ACS :
				print( FATAL, "No permissions to open device file for "
					   "board.\n" );
				break;

			case NI_DAQ_ERR_DFM :
				print( FATAL, "Device file for board missing.\n" );
				break;

			case NI_DAQ_ERR_DFP :
				print( FATAL, "Unspecified error when opening device file for "
					   "board.\n" );
				break;

			case NI_DAQ_ERR_BBS :
				print( FATAL, "Board already in use by another program.\n" );
				break;

			case NI_DAQ_ERR_INT :
				print( FATAL, "Internal error in board driver or library.\n" );
				break;

			default :
				print( FATAL, "Unrecognized error when trying to access the "
					   "board.\n" );
				break;
		}

		THROW( EXCEPTION );
	}

	/* Initialize the AO channels as far as required */

	for ( i = 0; i < 2; i++ )
		if ( ni_daq_ao_channel_configuration( pci_mio_16e_1.board, 1, &i,
								 pci_mio_16e_1.ao_state.external_reference + i,
								 pci_mio_16e_1.ao_state.polarity + i ) < 0 ||


			 ( pci_mio_16e_1.ao_state.is_used &&
			   ni_daq_ao( pci_mio_16e_1.board, 1, &i,
						  pci_mio_16e_1.ao_state.volts + i  ) < 0 ) )
		{
			print( FATAL, "Failure to initialize AO channels\n ");
			THROW( EXCEPTION );
		}

	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int pci_mio_16e_1_end_of_exp_hook( void )
{
	int i;


	if ( pci_mio_16e_1.board >= 0 )
		ni_daq_close( pci_mio_16e_1.board );

	for ( i = 0; i < 2; i++ )
		if ( pci_mio_16e_1.ao_state.reserved_by[ i ] )
			pci_mio_16e_1.ao_state.reserved_by[ i ] =
					  CHAR_P T_free( pci_mio_16e_1.ao_state.reserved_by[ i ] );

	if ( pci_mio_16e_1.dio_state.reserved_by )
		pci_mio_16e_1.dio_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.dio_state.reserved_by );

	if ( pci_mio_16e_1.msc_state.reserved_by )
		pci_mio_16e_1.msc_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.msc_state.reserved_by );

	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

void pci_mio_16e_1_exit_hook( void )
{
	int i;


	for ( i = 0; i < 2; i++ )
	{
		if ( pci_mio_16e_1.ao_state.reserved_by[ i ] )
			pci_mio_16e_1.ao_state.reserved_by[ i ] =
					  CHAR_P T_free( pci_mio_16e_1.ao_state.reserved_by[ i ] );

		if ( pci_mio_16e_1_stored.ao_state.reserved_by[ i ] )
			pci_mio_16e_1_stored.ao_state.reserved_by[ i ] =
			   CHAR_P T_free( pci_mio_16e_1_stored.ao_state.reserved_by[ i ] );
	}

	if ( pci_mio_16e_1.dio_state.reserved_by )
		pci_mio_16e_1.dio_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.dio_state.reserved_by );

	if ( pci_mio_16e_1_stored.dio_state.reserved_by )
		pci_mio_16e_1_stored.dio_state.reserved_by =
				   CHAR_P T_free( pci_mio_16e_1_stored.dio_state.reserved_by );

	if ( pci_mio_16e_1.msc_state.reserved_by )
		pci_mio_16e_1.msc_state.reserved_by =
						  CHAR_P T_free( pci_mio_16e_1.msc_state.reserved_by );

	if ( pci_mio_16e_1_stored.msc_state.reserved_by )
		pci_mio_16e_1_stored.msc_state.reserved_by =
				   CHAR_P T_free( pci_mio_16e_1_stored.msc_state.reserved_by );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *daq_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
