/*
  $Id$
 
  Library for National Instruments DAQ boards based on a DAQ-STC

  Copyright (C) 2003-2004 Jens Thoms Toerring

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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <math.h>


#include <ni_daq.h>


typedef struct NI_DAQ_DEV        NI_DAQ_DEV;
typedef struct NI_DAQ_AI_STATE   NI_DAQ_AI_STATE;
typedef struct NI_DAQ_AO_STATE   NI_DAQ_AO_STATE;
typedef struct NI_DAQ_GPCT_STATE NI_DAQ_GPCT_STATE;
typedef struct NI_DAQ_MSC_STATE  NI_DAQ_MSC_STATE;


struct NI_DAQ_AI_STATE {
	size_t num_scans;
	int num_channels;
	int num_data_per_scan;
	struct {
		double range;
		NI_DAQ_POLARITY pol;
	} *channels;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
};


struct NI_DAQ_AO_STATE {
	NI_DAQ_BU_POLARITY polarity[ 2 ];
	int is_channel_setup[ 2 ];
	NI_DAQ_STATE ext_ref[ 2 ];
	double volts[ 2 ];
	NI_DAQ_CLOCK_SPEED_VALUE speed;
};


struct NI_DAQ_GPCT_STATE {
	int state[ 2 ];
	int not_started[ 2 ];
	NI_DAQ_CLOCK_SPEED_VALUE speed;
};


struct NI_DAQ_MSC_STATE {
	NI_DAQ_CLOCK_TYPE clock;
	NI_DAQ_STATE output_state;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
	unsigned int divider;
};


struct NI_DAQ_DEV {
	int fd;
	NI_DAQ_BOARD_PROPERTIES props;
	NI_DAQ_AI_STATE ai_state;
	NI_DAQ_AO_STATE ao_state;
	NI_DAQ_GPCT_STATE gpct_state;
	NI_DAQ_MSC_STATE msc_state;
};
	

extern NI_DAQ_DEV ni_daq_dev[ NI_DAQ_MAX_BOARDS ];

extern int ni_daq_errno;

extern const char *error_message;

extern int ni_daq_basic_check( int board );
extern int ni_daq_msc_init( int board );
extern int ni_daq_ai_init( int board );
extern int ni_daq_ao_init( int board );
extern int ni_daq_gpct_init( int board );
