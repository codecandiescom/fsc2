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


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

Var *daq_ao_channel_setup( Var *v )
{
	int num_channels = 0;
	int channels[ 2 ];
	int er;
	int pol;
	NI_DAQ_STATE external_reference[ 2 ];
	NI_DAQ_BU_POLARITY polarity[ 2 ];
	int other_count;
	int ret;
	int i;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	do
	{
		pol = -1;
		er = -1;

		if ( num_channels >= 2 )
			too_many_arguments( v );

		channels[ num_channels ] =
			pci_mio_16e_1_channel_number( get_strict_long( v,
														 "AO channel number" ),
										  "AO channel" );

		other_count = 0;
		while ( ( v = vars_pop( v ) ) != NULL &&
				other_count++ < 2 &&
				v->type == STR_VAR )
		{
			if ( ! strcasecmp( v->val.sptr, "BIPOLAR" ) )
			{
				if ( pol != -1 )
				{
					print( FATAL, "Polarity for AO %d. channel specified more "
						   "than once.\n", num_channels + 1 );
					THROW( EXCEPTION );
				}

				pol = NI_DAQ_BIPOLAR;
			}
			else if ( ! strcasecmp( v->val.sptr, "UNIPOLAR" ) )
			{
				if ( pol != -1 )
				{
					print( FATAL, "Polarity for AO %d. channel specified more "
						   "than once.\n", num_channels + 1 );
					THROW( EXCEPTION );
				}

				pol = NI_DAQ_BIPOLAR;
			}
			else if ( ! strcasecmp( v->val.sptr, "INTERNAL_REFERENCE" ) )
			{
				if ( er != -1 )
				{
					print( FATAL, "Reference type for AO %d. channel "
						   "specified more than once.\n", num_channels + 1 );
					THROW( EXCEPTION );
				}

				er = NI_DAQ_DISABLED;
			}
			else if ( ! strcasecmp( v->val.sptr, "EXTERNAL_REFERENCE" ) )
			{
				if ( er != -1 )
				{
					print( FATAL, "Reference type for AO %d. channel "
						   "specified more than once.\n", num_channels + 1 );
					THROW( EXCEPTION );
				}

				er = NI_DAQ_ENABLED;
			}
			else
			{
				print( FATAL, "Invalid argument.\n" );
				THROW( EXCEPTION );
			}
		}

		polarity[ num_channels ] = pol == -1 ? NI_DAQ_BIPOLAR : pol;
		external_reference[ num_channels++ ] = er == -1 ? NI_DAQ_DISABLED : er;

	} while ( v != NULL );

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ( ret = ni_daq_ao_channel_configuration( pci_mio_16e_1.board,
													  num_channels, channels,
													  external_reference,
													  polarity ) ) < 0 )
		{
			switch ( ret )
			{
				case NI_DAQ_ERR_NER :
					print( FATAL, "Board does not allow use of external "
						   "reference for AO.\n" );
					break;

				case NI_DAQ_ERR_UAO :
					print( FATAL, "Board allows only bipolar AO.\n" );
					break;

				default :
					print( FATAL, "AO channel setup failed.\n" );
			}

			THROW( EXCEPTION );
		}
	}

	for ( i = 0; i < num_channels; i++ )
	{
		pci_mio_16e_1.ao_state.external_reference[ channels[ i ] ] =
													   external_reference[ i ];
		pci_mio_16e_1.ao_state.polarity[ channels[ i ] ] = polarity[ i ];
	}

	return vars_push( INT_VAR, ( long ) num_channels );	
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

Var *daq_set_voltage( Var *v )
{
	int channel;
	double volts;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	channel =
		pci_mio_16e_1_channel_number( get_strict_long( v,
													   "AO channel number" ),
									  "AO channel" );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	volts = get_double( v, "AO output voltage" );

	if ( pci_mio_16e_1.ao_state.polarity[ channel ] == NI_DAQ_UNIPOLAR &&
		 volts < 0.0 )
	{
		print( FATAL, "AO channel %d is configured for unipolar output, "
			   "invalid negative voltage.\n", channel + 1 );
		THROW( EXCEPTION );
	}

	if ( pci_mio_16e_1.ao_state.external_reference[ channel ]
		 == NI_DAQ_DISABLED && fabs( volts ) > 10.00001 )
	{
		print( FATAL, "Output voltage of %.f V not within allowed range.\n",
			   volts );
		THROW( EXCEPTION );
	}
	else if ( pci_mio_16e_1.ao_state.external_reference[ channel ]
			  == NI_DAQ_ENABLED && fabs( volts ) > 1.00001 )
	{
		print( FATAL, "When using an external reference the output "
			   "voltage argument must be with [ %d, 1 ].\n",
			   pci_mio_16e_1.ao_state.polarity[ channel ] == NI_DAQ_UNIPOLAR ?
			   0 : -1 );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ni_daq_ao( pci_mio_16e_1.board, 1, &channel, &volts ) < 0 )
		{
			print( FATAL, "Setting AO volatge failed.\n" );
			THROW( EXCEPTION );
		}
	}

	return vars_push( FLOAT_VAR, volts );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
