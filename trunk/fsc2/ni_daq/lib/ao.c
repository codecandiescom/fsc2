/*
  $Id$
 
  Library for National Instruments DAQ boards based on a DAQ-STC

  Copyright (C) 2003 Jens Thoms Toerring

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


#include "ni_daq_lib.h"


/*---------------------------------------------------------------*/
/* Function for configuration of the AO channels - it can be set */
/* individually for each channel if an external or the internal  */
/* reference is to be used and if the channels is to be used in  */
/* unipolar or bipolar mode. If this function doesn't get called */
/* all channels are per default set to use of the internal       */
/* reference and bipolar mode.                                   */
/*---------------------------------------------------------------*/

int ni_daq_ao_channel_configuration( int board, int num_channels,
									 int *channels,
									 NI_DAQ_STATE *external_reference,
									 NI_DAQ_BU_POLARITY *polarity )
{
    int ret;
	int i;
	int ch;
	double null_volts = 0.0;
	NI_DAQ_AO_ARG ao;
	NI_DAQ_AO_CHANNEL_ARGS arg;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( ni_daq_dev[ board ].props.num_ao_channels == 0 )
		return ni_daq_errno = NI_DAQ_ERR_NAO;

	if ( channels == NULL || num_channels <= 0 ||
		 num_channels > ni_daq_dev[ board ].props.num_ao_channels ||
		 external_reference == NULL || polarity == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	for ( i = 0; i < num_channels; i++ )
	{
		if ( channels[ i ] < 0 ||
			 channels[ i ] >= ni_daq_dev[ board ].props.num_ao_channels )
			return ni_daq_errno = NI_DAQ_ERR_IVA;

		if ( external_reference[ i ] == NI_DAQ_ENABLED &&
			 ! ni_daq_dev[ board ].props.ao_has_ext_ref )
			return ni_daq_errno = NI_DAQ_ERR_NER;

		if ( polarity[ i ] == NI_DAQ_UNIPOLAR &&
			 ! ni_daq_dev[ board ].props.ao_does_unipolar )
			return ni_daq_errno = NI_DAQ_ERR_UAO;
	}

	ao.cmd = NI_DAQ_AO_CHANNEL_SETUP;

	for ( i = 0; i < num_channels; i++ )
	{
		ch = channels[ i ];
		ao.channel = ch;
		ao.channel_args = &arg;
		arg.ground_ref = NI_DAQ_DISABLED;
		arg.external_ref = external_reference[ i ];
		arg.reglitch = NI_DAQ_DISABLED;
		arg.polarity = polarity[ i ];

		/* Before changing between internal and external reference or changing
		   the polarity set the voltage to zero - someday someone is going to
		   be grateful... */

		if ( ( ni_daq_dev[ board ].ao_state.ext_ref[ ch ] !=
			   										 external_reference[ i ] ||
			   ni_daq_dev[ board ].ao_state.polarity[ ch ] != polarity[ i ] )
			 &&
			 ( ret = ni_daq_ao( board, 1, &ch, &null_volts ) ) < 0 )
			return ret;

		if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AO, &ao ) )
			 < 0 )
			return ni_daq_errno = NI_DAQ_ERR_INT;

		if ( ni_daq_dev[ board ].ao_state.ext_ref[ ch ] !=
			 										  external_reference[ i ] )
			ni_daq_dev[ board ].ao_state.volts[ i ] *= 10.0;

		if ( ni_daq_dev[ board ].ao_state.ext_ref[ ch ] == NI_DAQ_DISABLED &&
			 external_reference[ i ] == NI_DAQ_ENABLED )
			ni_daq_dev[ board ].ao_state.volts[ i ] *= 0.1;
			 
		ni_daq_dev[ board ].ao_state.polarity[ ch ] = polarity[ i ];
		ni_daq_dev[ board ].ao_state.ext_ref[ ch ] = external_reference[ i ];
	}

	return ni_daq_errno = NI_DAQ_OK;
}

/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_ao( int board, int num_channels, int *channels, double *values )
{
    int ret;
	int i;
	double dval;
	NI_DAQ_AO_ARG ao;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( ni_daq_dev[ board ].props.num_ao_channels == 0 )
		return ni_daq_errno = NI_DAQ_ERR_NAO;

	if ( channels == NULL || values == NULL || num_channels <= 0 ||
		 num_channels > ni_daq_dev[ board ].props.num_ao_channels )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	for ( i = 0; i < num_channels; i++ )
	{
		int ch = channels[ i ];

		if ( ch < 0 || ch >= ni_daq_dev[ board ].props.num_ao_channels )
			return ni_daq_errno = NI_DAQ_ERR_IVA;

		if ( ni_daq_dev[ board ].ao_state.ext_ref[ ch ] == NI_DAQ_DISABLED )
		{
			if ( values[ i ] > 10.00001 ||
				 ( ni_daq_dev[ board ].ao_state.polarity[ ch ] ==
				   											  NI_DAQ_BIPOLAR &&
				   values[ i ] < -10.00001 ) )
				return ni_daq_errno = NI_DAQ_ERR_IVA;
		}
		else
		{
			if ( values[ i ] > 1.000001 ||
				 ( ni_daq_dev[ board ].ao_state.polarity[ ch ] ==
															  NI_DAQ_BIPOLAR &&
				   values[ i ] < -1.000001 ) )
				return ni_daq_errno = NI_DAQ_ERR_IVA;
		}
	}

	ao.cmd = NI_DAQ_AO_DIRECT_OUTPUT;

	for ( i = 0; i < num_channels; i++ )
	{
		int ch = channels[ i ];
		ao.channel = ch;

		dval = values[ i ];
		if ( ni_daq_dev[ board ].ao_state.ext_ref[ ch ] == NI_DAQ_DISABLED )
			dval *= 0.1;

		ao.value = ( int ) floor( dval * 0xFFFF + 0.5 );

		if ( ni_daq_dev[ board ].ao_state.polarity[ ch ] == NI_DAQ_BIPOLAR )
			ao.value /= 2;

		ao.value >>= 16 - ni_daq_dev[ board ].props.num_ao_bits;

		if ( ( ret = ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AO, &ao ) )
			 < 0 )
			return ni_daq_errno = NI_DAQ_ERR_INT;
	}

	return ni_daq_errno = NI_DAQ_OK;
}


/*---------------------------------------------------------------*/
/* Function for intializing the AO channels to the default state */
/*---------------------------------------------------------------*/

int ni_daq_ao_init( int board )
{
	NI_DAQ_AO_ARG ao;
	NI_DAQ_AO_CHANNEL_ARGS arg;
	int i;


	if ( ni_daq_dev[ board ].props.num_ao_channels == 0 )
		return 0;

	/* Switch all AO channels to bipolar mode and to use the internal
	   reference voltage */

	ao.cmd = NI_DAQ_AO_CHANNEL_SETUP;
	ao.channel_args = &arg;
	arg.ground_ref   = NI_DAQ_DISABLED;
	arg.external_ref = NI_DAQ_DISABLED;
	arg.reglitch     = NI_DAQ_DISABLED;
	arg.polarity     = NI_DAQ_BIPOLAR;

	for ( i = 0; i < ni_daq_dev[ board ].props.num_ao_channels; i++ )
	{
		ao.channel = i;	
		if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_AO, &ao ) < 0 )
			return 1;
	}

	return 0;
}
