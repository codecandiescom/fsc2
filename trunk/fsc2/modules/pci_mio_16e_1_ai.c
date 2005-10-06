/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
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


typedef struct {
	int type;
	NI_DAQ_INPUT start;
	NI_DAQ_POLARITY start_polarity;
	NI_DAQ_INPUT scan_start;
	NI_DAQ_POLARITY scan_polarity;
	double scan_duration;
	NI_DAQ_INPUT conv_start;
	NI_DAQ_POLARITY conv_polarity;
	double conv_duration;
	double delay_duration;
} PCI_MIO_16E_1_AI_TRIG_ARGS;


static void pci_mio_16e_1_ai_get_trigger_args( Var_T *v );
static int pci_mio_16e_1_ai_get_trigger_method( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_S_start( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_T_scan( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_S_scan( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_T_conv( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_S_conv( Var_T *v );
static Var_T *pci_mio_16e_1_ai_get_T_delay( Var_T *v );
static NI_DAQ_INPUT pci_mio_16e_1_ai_get_trigger( const char *tname,
												  const char *snippet );
static bool pci_mio_16e_1_ai_get_polarity( const char *pname,
										   NI_DAQ_POLARITY *pol );
static void pci_mio_16e_1_ai_check_T_scan( double t );
static void pci_mio_16e_1_ai_check_T_conv( double t, double t_scan );

static PCI_MIO_16E_1_AI_TRIG_ARGS trig;


#define TRIGGER_NONE              0
#define TRIGGER_CONV              1
#define TRIGGER_SCAN              2
#define TRIGGER_SCAN_CONV         3
#define TRIGGER_OUT               4
#define TRIGGER_START             5
#define TRIGGER_START_CONV        6
#define TRIGGER_START_SCAN        7
#define TRIGGER_START_SCAN_CONV   8


/*---------------------------------------------------------------------*
 * Function for doing the channel setup. For each channel between two
 * and five arguments are expected, first the channel number, then the
 * range (upper voltage in volts), and then, as optional arguments and
 * in non-fidxed order, string arguments for the type of the channel
 * (either "Ground", "Floating" or "Differential" - comparison is case
 * insensitive and "Ground" is the default), the polarity (either "BIPOLAR"
 * or "UNIPOLAR" - comparsion is case insensitive, and "BIPOLAR" is the
 * default) and, finally, if hardware dithering is to be enabled for the
 * channel (argument must be either "DITHER_OFF" or "DITHER_ON" - the test
 * is case insensitive and default is dithering disabled). The Function
 * returns the number of channels that have been set up on success. 
 * Please note: the allowed settings for the ranges depend on the
 *              polarity setting of the channel!
 *---------------------------------------------------------------------*/

Var_T *daq_ai_channel_setup( Var_T *v )
{
	int channel;
	double range;
	int *channels = NULL;
	double *ranges = NULL;
	int type;
	NI_DAQ_AI_TYPE *types = NULL;
	int pol;
	NI_DAQ_BU_POLARITY *polarities = NULL;
	int dither_enable;
	NI_DAQ_STATE *dither_enables = NULL;
	int num_channels = 0;
	double bpr_list[ ] = { 10.0, 5.0, 2.5, 1.0, 0.5, 0.25, 0.1, 0.05 };
	double upr_list[ ] = { 10.0, 5.0, 2.0, 1.0, 0.5, 0.2, 0.1 };
	double *r_list;
	size_t r_list_len;
	const char *types_list[ ] = { "Ground", "Floating", "Differential" };
	const char *pol_list[ ] = { "BIPOLAR", "UNIPOLAR" };
	const char *enable_list[ ] = { "DITHER_OFF", "DITHER_ON" };
	int range_index, old_range_index = -1;
	int other_count;
	size_t i, j;
	int ret = NI_DAQ_OK;


	CLOBBER_PROTECT( channels );
	CLOBBER_PROTECT( ranges );
	CLOBBER_PROTECT( types );
	CLOBBER_PROTECT( type );
	CLOBBER_PROTECT( polarities );
	CLOBBER_PROTECT( pol );
	CLOBBER_PROTECT( dither_enables );
	CLOBBER_PROTECT( dither_enable );
	CLOBBER_PROTECT( num_channels );
	CLOBBER_PROTECT( old_range_index );
	CLOBBER_PROTECT( v );
	CLOBBER_PROTECT( ret );


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( pci_mio_16e_1.ai_state.ranges != NULL )
			pci_mio_16e_1.ai_state.ranges =
							  DOUBLE_P T_free( pci_mio_16e_1.ai_state.ranges );
		if ( pci_mio_16e_1.ai_state.polarities != NULL )
			pci_mio_16e_1.ai_state.polarities =
			  NI_DAQ_BU_POLARITY_P T_free( pci_mio_16e_1.ai_state.polarities );
	}

	pci_mio_16e_1.ai_state.ampl_switch_needed = UNSET;

	do
	{
		type = -1;
		pol = -1;
		dither_enable = -1;

		/* Try to get the channel number and range, then get allocate memory
		   for the arrays */

		TRY
		{
			channel = pci_mio_16e_1_channel_number(
									 get_strict_long( v, "AI channel number" ),
									 "AI channel" );

			if ( ( v = vars_pop( v ) ) == NULL )
			{
				print( FATAL, "Missing arguments.\n" );
				THROW( EXCEPTION );
			}

			range = get_double( v, "voltage range" );

			num_channels++;
			channels = INT_P T_realloc( channels,
										num_channels * sizeof *channels );
			ranges = DOUBLE_P T_realloc( ranges,
										 num_channels * sizeof *ranges );
			types = NI_DAQ_AI_TYPE_P T_realloc( types,
												num_channels * sizeof *types );
			polarities = NI_DAQ_BU_POLARITY_P T_realloc( polarities,
										   num_channels * sizeof *polarities );
			dither_enables = NI_DAQ_STATE_P T_realloc( dither_enables,
									   num_channels * sizeof *dither_enables );

			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( channels )
				T_free( channels );
			if ( ranges )
				T_free( ranges );
			if ( types )
				T_free( types );
			if ( polarities )
				T_free( polarities );
			if ( dither_enables )
				T_free( dither_enables );

			RETHROW( );
		}

		/* Now look for the optional arguments - they're all strings and
		   there can be not more than three */

		other_count = 0;
		while ( ( v = vars_pop( v ) ) != NULL &&
				other_count < 3 &&
				v->type == STR_VAR )
		{
			other_count++;

			if ( type < 0 )
			{
				for ( i = 0; i < NUM_ELEMS( types_list ); i++ )
					if ( ! strcasecmp( v->val.sptr, types_list[ i ] ) )
					{
						switch ( i )
						{
							case 0 :
								type = NI_DAQ_AI_TYPE_RSE;
								break;

							case 1 :
								type = NI_DAQ_AI_TYPE_NRSE;
								break;

							case 2 :
								type = NI_DAQ_AI_TYPE_Differential;
								break;
						}
						break;
					}

				if ( i < NUM_ELEMS( types_list ) )
					continue;
			}

			if ( pol < 0 )
			{
				for ( i = 0; i < NUM_ELEMS( pol_list ); i++ )
					if ( ! strcasecmp( v->val.sptr, pol_list[ i ] ) )
					{
						pol = i;
						break;
					}

				if ( i < NUM_ELEMS( pol_list ) )
					continue;
			}

			if ( dither_enable < 0 )
			{
				for ( i = 0; i < NUM_ELEMS( enable_list ); i++ )
					if ( ! strcasecmp( v->val.sptr, enable_list[ i ] ) )
					{
						dither_enable = i;
						break;
					}

				if ( i < NUM_ELEMS( enable_list ) )
					continue;
			}

			print( FATAL, "Invalid argument: '%s'.\n", v->val.sptr );

			T_free( channels );
			T_free( ranges );
			T_free( types );
			T_free( polarities );
			T_free( dither_enables );

			THROW( EXCEPTION );
		}

		/* We can check the range argument only after knowing if it's to be
		   used with BIPOLAR or UNIPOLAR conversion because the allowed range
		   settings are different in both cases. If necessary the range is
		   set to he next allowed value, if it's off by more than 1% a warning
		   is printed. */

		if ( pol <= 0 )
		{
			r_list = bpr_list;
			r_list_len = NUM_ELEMS( bpr_list );
		}
		else
		{
			r_list = upr_list;
			r_list_len = NUM_ELEMS( upr_list );
		}

		for ( range_index = -1, i = 0; i < r_list_len - 1; i++ )
			if ( range <= r_list[ i ] && range >= r_list[ i + 1 ] )
			{
				if ( r_list[ i ] / range < range / r_list[ i + 1 ] )
					range_index = i;
				else
					range_index = i + 1;
				break;
			}

		if ( range_index >= 0 &&
			 fabs( range - r_list[ range_index ] ) > range * 1.0e-2 )
			print( WARN, "Can't set range to %.2f V, using %.2f V "
				   "instead.\n", range, r_list[ range_index ] );

		if ( range_index < 0 )
		{
			if ( range > r_list[ 0 ] )
				range_index = 0;
			else
				range_index = r_list_len - 1;

			print( WARN, "Can't set range to %.2f V, using %.2f V "
				   "instead.\n", range, r_list[ range_index ] );
		}

		/* Check if the range settung differs from the setting of the
		   other channels (in this case we need a longer time distance
		   between the CONVERT triggers to allow for the settling settling
		   time of the preamplifier). */

		if ( num_channels > 1 )
		{
			if (  range_index != old_range_index )
				pci_mio_16e_1.ai_state.ampl_switch_needed = SET;
		}
		else
			old_range_index = range_index;

		/* Set the values to be send to the library */

		channels[ num_channels - 1 ] = channel;
		ranges[ num_channels - 1 ] = range;

		switch ( type )
		{
			case NI_DAQ_AI_TYPE_Differential :
				types[ num_channels - 1 ] = NI_DAQ_AI_TYPE_Differential;
				break;

			case NI_DAQ_AI_TYPE_NRSE :
				types[ num_channels - 1 ] = NI_DAQ_AI_TYPE_NRSE;
				break;

			case NI_DAQ_AI_TYPE_RSE :
			default :
				types[ num_channels - 1 ] = NI_DAQ_AI_TYPE_RSE;
		}

		switch ( pol )
		{
			case 1 :
				polarities[ num_channels - 1 ] = NI_DAQ_UNIPOLAR;
				break;

			case 0 :
			default :
				polarities[ num_channels - 1 ] = NI_DAQ_BIPOLAR;
		}

		switch ( dither_enable )
		{
			case 1 :
				dither_enables[ num_channels - 1 ] = NI_DAQ_ENABLED;
				break;

			case 0 :
			default :
				dither_enables[ num_channels - 1 ] = NI_DAQ_DISABLED;
				break;
		}
	} while ( v != NULL );

	/* We've got to check that for channels of "Differential" type the channel
	   number is between 0 and 7 and that the second channel used in the pair
	   isn't used */

	for ( i = 0; i < ( size_t ) num_channels; i++ )
	{
		if ( i != 0 && polarities[ i ] != polarities[ i - 1 ] )
			pci_mio_16e_1.ai_state.ampl_switch_needed = SET;			

		if ( types[ i ] == NI_DAQ_AI_TYPE_Differential )
		{
			if ( channels[ i ] % 16 < 8 )
			{
				for ( j = 0; j < ( size_t ) num_channels; j++ )
				{
					if ( j == i || channels[ j ] != channels[ i ] + 8 )
						continue;

					print( FATAL, "When CH%d is used in \"Differential\" "
						   "mode CH%d can't be used.\n", channels[ i ],
						   channels[ j ] );
					break;
				}

				if ( j == ( size_t ) num_channels )
					continue;
			}
			else
				print( FATAL, "Channel number for channels of type "
					   "\"Differential\" must be between CH0 and CH7, "
					   "can't be CH%d.\n", channels[ i ] );

			T_free( channels );
			T_free( ranges );
			T_free( types );
			T_free( polarities );
			T_free( dither_enables );

			THROW( EXCEPTION );
		}
	}

	if ( FSC2_MODE == EXPERIMENT )
		ret = ni_daq_ai_channel_configuration( pci_mio_16e_1.board,
											   num_channels, channels, types,
											   polarities, ranges,
											   dither_enables );

	T_free( channels );
	T_free( types );
	T_free( dither_enables );

	if ( ret != NI_DAQ_OK )
	{
		switch ( ret )
		{
			case NI_DAQ_ERR_NEM :
				print( FATAL, "Running out of memory.\n" );
				break;

			default :
				print( FATAL, "AI channel setup failed.\n" );
		}

		T_free( ranges );
		T_free( polarities );
		THROW( EXCEPTION );
	}

	
	if ( FSC2_MODE == TEST )
	{
		pci_mio_16e_1.ai_state.ranges = ranges;
		pci_mio_16e_1.ai_state.polarities = polarities;
	}
	else
	{
		T_free( ranges );
		T_free( polarities );
	}

	pci_mio_16e_1.ai_state.num_channels = num_channels;
	pci_mio_16e_1.ai_state.is_channel_setup = SET;

	return vars_push( INT_VAR, ( long ) num_channels );	
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

Var_T *daq_ai_acq_setup( Var_T *v )
{
	long num_scans;
	int ret;


	/* Acquisition setup requires a previous channel setup */

	if ( ! pci_mio_16e_1.ai_state.is_channel_setup )
	{
		print( FATAL, "Function can only be invoked after "
			   "daq_ai_channel_setup() had been called successfully.\n" );
		THROW( EXCEPTION );
	}

	/* Minimum number of arguments is 3 */

	if ( v == NULL || v->next == NULL || v->next->next == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	num_scans = get_long( v, "number of scans" );

	if ( num_scans <= 0 )
	{
		print( FATAL, "Invalid number of scans: %ld\n", num_scans );
		THROW( EXCEPTION );
	}

	if ( num_scans > MAX_NUM_SCANS )
	{
		print( FATAL, "Number of scans of %ld is too large, maximum is %ld\n",
			   num_scans, MAX_NUM_SCANS );
		THROW( EXCEPTION );
	}
		
	v = vars_pop( v );

	pci_mio_16e_1_ai_get_trigger_args( v );

	pci_mio_16e_1.ai_state.is_acq_setup = UNSET;

	/* Now we finally can call the function for setting up the acquisition */

	if ( FSC2_MODE == EXPERIMENT )
	{
		/* For TRIG_OUT triger type (i.e. with the scans started by a signal
		   from channel 0 of the GPCT pulser and a trigger being output after
		   the delay delay time on channel 1) we have to setup the
		   counters. */

		if ( trig.type == TRIGGER_OUT )
			ni_daq_two_channel_pulses( trig.delay_duration,
									   trig.scan_duration );

		if ( ( ret = ni_daq_ai_acq_setup( pci_mio_16e_1.board, trig.start,
										  trig.start_polarity,
										  trig.scan_start, trig.scan_polarity,
										  trig.scan_duration, trig.conv_start,
										  trig.conv_polarity,
										  trig.conv_duration,
										  num_scans ) ) != NI_DAQ_OK )
		{
			switch ( ret )
			{
				case NI_DAQ_ERR_NPT :
					print( FATAL, "Impossible to use the requested "
						   "timings.\n" );
					break;

				case NI_DAQ_ERR_NEM :
					print( FATAL, "Running out of memory.\n" );
					break;

				default :
					print( FATAL, "AI acquisition setup failed: %s.\n",
						   ni_daq_strerror( ));
			}

			THROW( EXCEPTION );
		}

		if ( trig.type == TRIGGER_OUT )
			ni_daq_gpct_stop_pulses( pci_mio_16e_1.board, 2 );
	}

	pci_mio_16e_1.ai_state.is_acq_setup = SET;
	pci_mio_16e_1.ai_state.data_per_channel = num_scans;
	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

Var_T *daq_ai_start_acquisition( Var_T *v UNUSED_ARG )
{
	int ret;


	if ( ! pci_mio_16e_1.ai_state.is_channel_setup )
	{
		print( FATAL, "Missing AI channel and acquisition setup, call "
			   "daq_ai_channel_setup() and daq_ai_acq_setup() first.\n" );
		THROW( EXCEPTION );
	}

	if ( ! pci_mio_16e_1.ai_state.is_acq_setup )
	{
		print( FATAL, "Missing AI acquisition setup, call daq_ai_acq_setup() "
			   "first.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( trig.type == TRIGGER_OUT )
			 ni_daq_two_channel_pulses( trig.delay_duration,
										trig.scan_duration );

		/* Start the acquisition (unless the board starts the acquisition all
		   by itself, i.e. in TRIGGER_NONE mode) */

		if ( trig.type != TRIGGER_NONE &&
			 ( ret = ni_daq_ai_start_acq( pci_mio_16e_1.board ) )
			 													 != NI_DAQ_OK )
		{
			if ( ret == NI_DAQ_ERR_NEM )
				print( FATAL, "Running out if memory\n" );
			else
				print( FATAL, "Starting AI acquisition failed: %s\n",
					   ni_daq_strerror( ) );
			THROW( EXCEPTION );
		}

		if ( trig.type == TRIGGER_OUT &&
			 ni_daq_gpct_start_pulses( pci_mio_16e_1.board, 2 ) != NI_DAQ_OK )
		{
			print( FATAL, "Starting AI acquisition failed: %s\n",
				   ni_daq_strerror( ) );
			THROW( EXCEPTION );
		}
	}

	pci_mio_16e_1.ai_state.is_acq_running = SET;
	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

Var_T *daq_ai_get_curve( Var_T *v UNUSED_ARG )
{
	size_t i, j;
	Var_T *nv = NULL;
	double **volts = NULL;
	ssize_t received_data;
	size_t to_be_fetched = 0;


	CLOBBER_PROTECT( volts );
	CLOBBER_PROTECT( nv );
	CLOBBER_PROTECT( to_be_fetched );

	if ( ! pci_mio_16e_1.ai_state.is_acq_running )
	{
		print( FATAL, "No AI acquisition has been started.\n ");
		THROW( EXCEPTION );
	}

	TRY
	{
		volts = DOUBLE_PP T_malloc( pci_mio_16e_1.ai_state.num_channels
									* sizeof *volts );

		if ( pci_mio_16e_1.ai_state.num_channels == 1 )
		{
			nv = vars_push( FLOAT_ARR, NULL,
							( long ) pci_mio_16e_1.ai_state.data_per_channel );
			volts[ 0 ] = nv->val.dpnt;
		}
		else
		{
			nv = vars_push_matrix( FLOAT_REF, 2,
							( long ) pci_mio_16e_1.ai_state.num_channels,
							( long ) pci_mio_16e_1.ai_state.data_per_channel );
			for ( i = 0;
				  i < ( size_t ) pci_mio_16e_1.ai_state.num_channels; i++ )
				volts[ i ] = nv->val.vptr[ i ]->val.dpnt;
		}
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( volts )
			T_free( volts );
		RETHROW( );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		to_be_fetched = pci_mio_16e_1.ai_state.data_per_channel;

		received_data = ni_daq_ai_get_acq_data( pci_mio_16e_1.board, volts,
												0, to_be_fetched, 1 );

		if ( received_data < ( ssize_t ) to_be_fetched )
		{
			vars_pop( nv );
			T_free( volts );

			switch ( received_data )
			{
				case NI_DAQ_ERR_SIG :
					stop_on_user_request( );

				case NI_DAQ_ERR_NEM :
					print( FATAL, "Running out of memory.\n" );
					break;

				default :
					print( FATAL, "Failed to get AI data: %s.\n",
						   ni_daq_strerror( ) );
			}

			THROW( EXCEPTION );
		}

		/* If a trigger is output stop the counters creating the triggers */

		if ( trig.type == TRIGGER_OUT )
			ni_daq_gpct_stop_pulses( pci_mio_16e_1.board, 2 );
	}
	else
	{
		double r;
		double o;

		for ( i = 0; i < ( size_t ) pci_mio_16e_1.ai_state.num_channels; i++ )
		{
			r = pci_mio_16e_1.ai_state.ranges[ i ];
			o = 0.0;
			if ( pci_mio_16e_1.ai_state.polarities[ i ] == NI_DAQ_UNIPOLAR )
			{
				r *= 0.5;
				o = r;
			}
				
			for ( j = 0;
				  j < ( size_t ) pci_mio_16e_1.ai_state.data_per_channel; j++ )
				*( volts[ i ] + j ) = r * sin( M_PI * j / 122.0 ) + o;
		}

	}

	T_free( volts );

	return nv;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void pci_mio_16e_1_ai_get_trigger_args( Var_T *v )
{
	int method;

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Invalid trigger method argument.\n" );
		THROW( EXCEPTION );
	}

	method = pci_mio_16e_1_ai_get_trigger_method( v );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	trig.start = trig.scan_start = trig.conv_start = NI_DAQ_INTERNAL;
	trig.start_polarity = trig.scan_polarity =
						   trig.conv_polarity = NI_DAQ_NORMAL;
	trig.scan_duration = trig.conv_duration = - HUGE_VAL;

	switch ( method )
	{
		case TRIGGER_NONE :
			v = pci_mio_16e_1_ai_get_T_scan( v );
			v = pci_mio_16e_1_ai_get_T_conv( v );
			break;

		case TRIGGER_CONV :
			v = pci_mio_16e_1_ai_get_T_scan( v );
			v = pci_mio_16e_1_ai_get_S_conv( v );
			break;

		case TRIGGER_SCAN :
			v = pci_mio_16e_1_ai_get_S_scan( v );
			v = pci_mio_16e_1_ai_get_T_conv( v );
			break;

		case TRIGGER_SCAN_CONV :
			v = pci_mio_16e_1_ai_get_S_scan( v );
			v = pci_mio_16e_1_ai_get_S_conv( v );
			break;

		case TRIGGER_START :
			v = pci_mio_16e_1_ai_get_S_start( v );
			v = pci_mio_16e_1_ai_get_T_scan( v );
			v = pci_mio_16e_1_ai_get_T_conv( v );
			break;

		case TRIGGER_START_CONV :
			v = pci_mio_16e_1_ai_get_S_start( v );
			v = pci_mio_16e_1_ai_get_T_scan( v );
			v = pci_mio_16e_1_ai_get_S_conv( v );
			break;

		case TRIGGER_START_SCAN :
			v = pci_mio_16e_1_ai_get_S_start( v );
			v = pci_mio_16e_1_ai_get_S_scan( v );
			v = pci_mio_16e_1_ai_get_T_conv( v );
			break;

		case TRIGGER_START_SCAN_CONV :
			v = pci_mio_16e_1_ai_get_S_start( v );
			v = pci_mio_16e_1_ai_get_S_scan( v );
			v = pci_mio_16e_1_ai_get_S_conv( v );
			break;

		case TRIGGER_OUT :
			trig.scan_start = NI_DAQ_GOUT_0;
			v = pci_mio_16e_1_ai_get_T_delay( v );
			v = pci_mio_16e_1_ai_get_T_scan( v );
			v = pci_mio_16e_1_ai_get_T_conv( v );
			break;
			

		default :
			fsc2_assert( 1 == 0 );
	}

	too_many_arguments( v );

	trig.type = method;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static int pci_mio_16e_1_ai_get_trigger_method( Var_T *v )
{
	const char *methods[ ] = { "TRIGGER_NONE", "TRIG_NONE",
							   "TRIGGER_CONV", "TRIG_CONV",
							   "TRIGGER_SCAN", "TRIG_SCAN",
							   "TRIGGER_SCAN_CONV", "TRIG_SCAN_CONV",
							   "TRIGGER_OUT", "TRIG_OUT",
							   "TRIGGER_START", "TRIG_START",
							   "TRIGGER_START_CONV", "TRIG_START_CONV",
							   "TRIGGER_START_SCAN", "TRIG_START_SCAN",
							   "TRIGGER_START_SCAN_CONV",
							   "TRIG_START_SCAN_CONV",
							   "TRIGGER_ALL", "TRIG_ALL" };
	size_t i;


	for ( i = 0; i < NUM_ELEMS( methods ); i++ )
		if ( ! strcasecmp( v->val.sptr, methods[ i ] ) )
			break;

	if ( i == NUM_ELEMS( methods ) )
	{
		print( FATAL, "Invalid trigger method argument.\n" );
		THROW( EXCEPTION );
	}

	i /= 2;
	if ( i == TRIGGER_START_SCAN_CONV + 1 )
		i = TRIGGER_START_SCAN_CONV;

	return i;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_S_start( Var_T *v )
{
	NI_DAQ_POLARITY pol;


	if ( v == NULL )	
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Invalid trigger type argument.\n" );
		THROW( EXCEPTION );
	}

	trig.start = pci_mio_16e_1_ai_get_trigger( v->val.sptr, "start trigger" );
	v = vars_pop( v );

	if ( v && v->type == STR_VAR &&
		 pci_mio_16e_1_ai_get_polarity( v->val.sptr, &pol ) )
	{
		trig.start_polarity = pol;
		v = vars_pop( v );
	}

	return v;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_T_scan( Var_T *v )
{
	double t;

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	t = get_double( v, "scan time" );
	t = pci_mio_16e_1_check_time( t, "scan time" );
	pci_mio_16e_1_ai_check_T_scan( t );
	trig.scan_duration = t;

	return 	vars_pop( v );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_S_scan( Var_T *v )
{
	NI_DAQ_POLARITY pol;


	if ( v == NULL )	
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Invalid trigger type argument.\n" );
		THROW( EXCEPTION );
	}

	trig.scan_start = pci_mio_16e_1_ai_get_trigger( v->val.sptr,
													"scan trigger" );
	v = vars_pop( v );

	if ( v && v->type == STR_VAR &&
		 pci_mio_16e_1_ai_get_polarity( v->val.sptr, &pol ) )
	{
		trig.scan_polarity = pol;
		v = vars_pop( v );
	}

	return v;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_T_conv( Var_T *v )
{
	double t;

	if ( v != NULL )
	{
		t = get_double( v, "conversion time" );
		t = pci_mio_16e_1_check_time( t, "conversion time" );
		pci_mio_16e_1_ai_check_T_conv( t, trig.scan_duration );
		trig.conv_duration = t;
		return vars_pop( v );
	}

	trig.conv_duration = pci_mio_16e_1.ai_state.ampl_switch_needed ?
			   PCI_MIO_16E_1_AMPL_SWITCHING_TIME : PCI_MIO_16E_1_MIN_CONV_TIME;
	return NULL;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_S_conv( Var_T *v )
{
	NI_DAQ_POLARITY pol;


	if ( v == NULL )	
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	if ( v->type != STR_VAR )
	{
		print( FATAL, "Invalid trigger type argument.\n" );
		THROW( EXCEPTION );
	}

	trig.conv_start = pci_mio_16e_1_ai_get_trigger( v->val.sptr,
													 "conversion trigger" );
	v = vars_pop( v );

	if ( v && v->type == STR_VAR
		 && pci_mio_16e_1_ai_get_polarity( v->val.sptr, &pol ) )
	{
		trig.conv_polarity = pol;
		v = vars_pop( v );
	}

	return v;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static Var_T *pci_mio_16e_1_ai_get_T_delay( Var_T *v )
{
	double t;

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	t = get_double( v, "delay time" );
	t = pci_mio_16e_1_check_time( t, "delay time" );
	trig.delay_duration = t;

	return 	vars_pop( v );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static NI_DAQ_INPUT pci_mio_16e_1_ai_get_trigger( const char *tname,
												  const char *snippet )
{
	NI_DAQ_INPUT pfi = NI_DAQ_PFI0;
	int i;


	for ( i = 0; i < 10; i++ )
		if ( ! strncasecmp( "PFI", tname, 3 ) && tname[ 3 ] == '0' + i  )
			return ( NI_DAQ_INPUT ) ( pfi + i );

	if ( ! strcasecmp( "TRIG1", tname ) )
		return NI_DAQ_PFI0;

	if ( ! strcasecmp( "GOUT_0", tname ) )
		return NI_DAQ_GOUT_0;

	print( FATAL, "Invalid source for %s.\n", snippet );
	THROW( EXCEPTION );

	return ( NI_DAQ_INPUT ) -1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static bool pci_mio_16e_1_ai_get_polarity( const char *pname,
										   NI_DAQ_POLARITY *pol )
{
	const char *p[ ] = { "POS", "POSITIVE", "NEG", "NEGATIVE" };
	size_t i;


	for ( i = 0; i < NUM_ELEMS( p ); i++ )
		if ( ! strcasecmp( p[ i ], pname ) )
		{
			*pol = i < 2 ? NI_DAQ_NORMAL : NI_DAQ_INVERTED;
			return OK;
		}

	return FAIL;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void pci_mio_16e_1_ai_check_T_scan( double t )
{
	if ( t / pci_mio_16e_1.ai_state.num_channels < 
		 ( pci_mio_16e_1.ai_state.ampl_switch_needed ?
		   PCI_MIO_16E_1_AMPL_SWITCHING_TIME : PCI_MIO_16E_1_MIN_CONV_TIME ) )
	{
		print( FATAL, "Requested scan time of %ld ns is too short, at least "
			   "%ld ns are required under the current conditions.\n",
			   lrnd( t * 1.0e9 ),
			   lrnd ( 1.0e9 * pci_mio_16e_1.ai_state.num_channels *
					  ( pci_mio_16e_1.ai_state.ampl_switch_needed ?
						PCI_MIO_16E_1_AMPL_SWITCHING_TIME :
						PCI_MIO_16E_1_MIN_CONV_TIME ) ) );
		THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void pci_mio_16e_1_ai_check_T_conv( double t, double t_scan )
{
	if ( t == 0.0 )
		t = pci_mio_16e_1.ai_state.ampl_switch_needed ?
			PCI_MIO_16E_1_AMPL_SWITCHING_TIME : PCI_MIO_16E_1_MIN_CONV_TIME;

	if ( t * pci_mio_16e_1.ai_state.num_channels > t_scan && t_scan > 0.0 )
	{
		print( FATAL, "Requested conversion time of %ld ns is too long, with "
			   "the current setting for the scan time %ld ns can't be "
			   "exceeded.\n", lrnd( t * 1.0e9 ),
			   lrnd ( 1.0e9 * t_scan /
					  pci_mio_16e_1.ai_state.num_channels ) );
		THROW( EXCEPTION );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
