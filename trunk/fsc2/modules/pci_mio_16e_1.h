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


#if ! defined PCI_MIO_16E_1_FSC2_HEADER
#define PCI_MIO_16E_1_FSC2_HEADER

#include "fsc2_module.h"


/* Include the header file for the library for the card */

#include <ni_daq.h>


#ifdef __cplusplus
#define NI_DAQ_POLARITY_P              ( NI_DAQ_POLARITY * )
#else
#define NI_DAQ_POLARITY_P
#endif


/* Include configuration information for the device */

#include "pci_mio_16e_1.conf"


#define PCI_MIO_16E_1_TIME_RESOLUTION      5.0e-8     /*  50 ns */
#define PCI_MIO_16E_1_MIN_CONV_TIME        8.0e-7     /* 800 ns */
#define PCI_MIO_16E_1_AMPL_SWITCHING_TIME  2.0e-6     /*   2 us */

#define MAX_NUM_SCANS                      16777216L


#define PCI_MIO_16E_1_TEST_CLOCK    NI_DAQ_FAST_CLOCK
#define PCI_MIO_16E_1_TEST_SPEED    NI_DAQ_FULL_SPEED
#define PCI_MIO_16E_1_TEST_STATE    NI_DAQ_ENABLED
#define PCI_MIO_16E_1_TEST_DIVIDER  1


struct PCI_MIO_16E_1 {
	int board;

	struct {
		bool is_busy;
		bool is_channel_setup;
		bool is_acq_setup;
		bool ampl_switch_needed;
		bool is_acq_running;
		int num_channels;
		ssize_t data_per_channel;
		double *ranges;
		NI_DAQ_POLARITY *polarities;
	} ai_state;

	struct {
		bool is_used[ 2 ];
		double volts[ 2 ];
		char *reserved_by[ 2 ] ;
		NI_DAQ_STATE external_reference[ 2 ];
		NI_DAQ_BU_POLARITY polarity[ 2 ];
	} ao_state;

	struct {
		int states[ 2 ];
	} gpct_state;

	struct {
		char *reserved_by;
	} dio_state;

	struct {
		NI_DAQ_CLOCK_TYPE daq_clock;
		NI_DAQ_STATE on_off;
		NI_DAQ_CLOCK_SPEED_VALUE speed;
		int divider;
		char *reserved_by;
	} msc_state;

} PCI_MIO_16E_1;


extern struct PCI_MIO_16E_1 pci_mio_16e_1, pci_mio_16e_1_stored;


/* Functions from pci_mio_16e_1.c */

int pci_mio_16e_1_init_hook( void );
int pci_mio_16e_1_test_hook( void );
int pci_mio_16e_1_end_of_test_hook( void );
int pci_mio_16e_1_exp_hook( void );
int pci_mio_16e_1_end_of_exp_hook( void );
void pci_mio_16e_1_exit_hook( void );

Var *daq_name( Var *v );

/* Functions from pci_mio_16e_1_ai.c */

Var *daq_ai_channel_setup( Var *v );
Var *daq_ai_acq_setup( Var *v );
Var *daq_ai_start_acquisition( Var *v );
Var *daq_ai_get_curve( Var * v );


/* Functions from pci_mio_16e_1_ao.c */

Var *daq_reserve_dac( Var *v );
Var *daq_ao_channel_setup( Var *v );
Var *daq_set_voltage( Var *v );


/* Functions from pci_mio_16e_1_gpct.c */

Var *daq_start_continuous_counter( Var *v );
Var *daq_start_timed_counter( Var *v );
Var *daq_intermediate_count( Var *v );
Var *daq_timed_count( Var *v );
Var *daq_final_count( Var *v );
Var *daq_stop_counter( Var *v );
Var *daq_single_pulse( Var *c );
Var *daq_continuous_pulses( Var *v );

void ni_daq_two_channel_pulses( double delay, double scan_duration );

/* Functions from pci_mio_16e_1_dio.c */

Var *daq_reserve_dio( Var *v );
Var *daq_dio_read( Var *v );
Var *daq_dio_write( Var *v );


/* Functions from pci_mio_16e_1_msc.c */

Var *daq_reserve_freq_out( Var *v );
Var *daq_freq_out( Var *v );
Var *daq_trigger_setup( Var *v );


/* Functions from pci_mio_16e_1_util.c */

int pci_mio_16e_1_channel_number( long ch, const char *snippet );
double pci_mio_16e_1_check_time( double t, const char *snippet );



#endif /* ! PCI_MIO_16E_1_FSC2_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
