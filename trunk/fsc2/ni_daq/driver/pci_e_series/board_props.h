/*
  $Id$
 
  Driver for National Instruments 6070E boards

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

/******************************************************************/
/* Please note: this is *NOT* a normal header file that should be */
/*              included somewhere. This file is only for setting */
/*              up an array of structures to be included in the   */
/*              file init.c to define the properties of different */
/*              boards of the PCI E series.                       */
/******************************************************************/


static Board_Properties pci_e_series_boards[ ]= {

/* PCI-MIO-16E-1 */

{
	id:                 0x1180,
	name:               "pci-mio-16e-1",
	ai_num_channels:    16,
	ai_num_bits:        12,
	ai_fifo_depth:      512,
	ai_always_dither:   0,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_2, NI_DAQ_GAIN_5,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_20,
			      NI_DAQ_GAIN_50, NI_DAQ_GAIN_100 },
	ai_speed:           800,
	ao_num_channels:    2,
	ao_num_bits:        12,
	ao_fifo_depth:      2048,
	ao_unipolar:        1,
        ao_has_ext_ref:     1,
	has_analog_trig:    1,
	atrig_caldac:       mb88341,
	atrig_low_ch:       11,
	atrig_high_ch:      12,
	atrig_bits:         8,
	num_mite_channels:  4,
	caldac:             { mb88341 }
},

/* PCI-MIO-16E-4 */

{
	id:                 0x1190,
	name:               "pci-mio-16e-4",
	ai_num_channels:    16,
	ai_num_bits:        12,
	ai_fifo_depth:      512,
	ai_always_dither:   0,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_2, NI_DAQ_GAIN_5,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_20,
			      NI_DAQ_GAIN_50, NI_DAQ_GAIN_100 },
	ai_speed:           2000,
	ao_num_channels:    2,
	ao_num_bits:        12,
	ao_fifo_depth:      512,
	ao_unipolar:        1,
        ao_has_ext_ref:     1,
	has_analog_trig:    1,
	atrig_caldac:       mb88341,
	atrig_low_ch:       11,
	atrig_high_ch:      12,
	atrig_bits:         8,
	num_mite_channels:  4,
	caldac:             { mb88341 }
},

/* PCI-MIO-16XE-10 */

{
	id:                 0x1170,
	name:               "pci-mio-16xe-10",
	ai_num_channels:    16,
	ai_num_bits:        16,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_1, NI_DAQ_GAIN_2,
			      NI_DAQ_GAIN_5, NI_DAQ_GAIN_10,
			      NI_DAQ_GAIN_20, NI_DAQ_GAIN_50,
			      NI_DAQ_GAIN_100, NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           10000,
	ao_num_channels:    2,
	ao_num_bits:        16,
	ao_fifo_depth:      2048,
	ao_unipolar:        1,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       ad8522,
	atrig_low_ch:       0,
	atrig_high_ch:      1,
	atrig_bits:         12,
	num_mite_channels:  4,
	caldac:             { dac8800, dac8043, ad8522 }
},

/* PCI-MIO-16XE-50 */

{
	id:                 0x0162,
	name:               "pci-mio-16xe-50",
	ai_num_channels:    16,
	ai_num_bits:        16,
	ai_fifo_depth:      2048,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_1, NI_DAQ_GAIN_2,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_100,
			      NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           50000,
	ao_num_channels:    2,
	ao_num_bits:        12,
	ao_fifo_depth:      0,
	ao_unipolar:        0,
	has_analog_trig:    0,
	num_mite_channels:  4,
	caldac:             { dac8800, dac8043 }
},

/* PCI-6023E */

{
	id:                 0x2a60,
	name:               "pci-6023e",
	ai_num_channels:    16,
	ai_num_bits:        12,
	ai_fifo_depth:      512,
	ai_always_dither:   0,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_100,
			      NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           5000,
	ao_num_channels:    0,
	ao_num_bits:        0,
	ao_unipolar:        0,
	ao_has_ext_ref:     0,
	has_analog_trig:    0,
	num_mite_channels:  4,
	caldac:             { mb88341 }
},

/* PCI-6024E */

{
	id:                 0x2a70,
	name:               "pci-6024e",
	ai_num_channels:    16,
	ai_num_bits:        12,
	ai_fifo_depth:      512,
	ai_always_dither:   0,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_100,
			      NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           5000,
	ao_num_channels:    2,
	ao_num_bits:        12,
	ao_fifo_depth:      0,
	ao_unipolar:        0,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       mb88341,
	atrig_low_ch:       11,
	atrig_high_ch:      12,
	atrig_bits:         8,
	num_mite_channels:  4,
	caldac:             { mb88341 }
},

/* PCI-6031E */

{
	id:                 0x1330,
	name:               "pci-6031e",
	ai_num_channels:    64,
	ai_num_bits:        16,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_1, NI_DAQ_GAIN_2,
			      NI_DAQ_GAIN_5, NI_DAQ_GAIN_10,
			      NI_DAQ_GAIN_20, NI_DAQ_GAIN_50,
			      NI_DAQ_GAIN_100, NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           10000,
	ao_num_channels:    2,
	ao_num_bits:        16,
	ao_fifo_depth:      2048,
	ao_unipolar:        1,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       ad8522,
	atrig_low_ch:       0,
	atrig_high_ch:      1,
	atrig_bits:         12,
	num_mite_channels:  4,
	caldac:             { dac8800, dac8043, ad8522 }
},

/* PCI-6032E */

{
	id:                 0x1270,
	name:               "pci-6032e",
	ai_num_channels:    16,
	ai_num_bits:        16,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_1, NI_DAQ_GAIN_2,
			      NI_DAQ_GAIN_5, NI_DAQ_GAIN_10,
			      NI_DAQ_GAIN_20, NI_DAQ_GAIN_50,
			      NI_DAQ_GAIN_100, NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           10000,
	ao_num_channels:    0,
	ao_num_bits:        0,
	ao_fifo_depth:      0,
	ao_unipolar:        1,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       ad8522,
	atrig_low_ch:       0,
	atrig_high_ch:      1,
	atrig_bits:         12,
	num_mite_channels:  4,
	caldac:             { dac8800, dac8043, ad8522 }
},

/* PCI-6033E */

{
	id:                 0x1340,
	name:               "pci-6033e",
	ai_num_channels:    64,
	ai_num_bits:        16,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_1, NI_DAQ_GAIN_2,
			      NI_DAQ_GAIN_5, NI_DAQ_GAIN_10,
			      NI_DAQ_GAIN_20, NI_DAQ_GAIN_50,
			      NI_DAQ_GAIN_100, NI_DAQ_GAIN_NOT_AVAIL },
	ai_speed:           10000,
	ao_num_channels:    0,
	ao_num_bits:        0,
	ao_fifo_depth:      0,
	ao_unipolar:        1,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       ad8522,
	atrig_low_ch:       0,
	atrig_high_ch:      1,
	atrig_bits:         12,
	num_mite_channels:  4,
	caldac:             { dac8800, dac8043, ad8522 }
},

/* PCI-6052E */

{
	id:                 0x18b0,
	name:               "pci-6052e",
	ai_num_channels:    16,
	ai_num_bits:        16,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_2, NI_DAQ_GAIN_5,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_20,
			      NI_DAQ_GAIN_50, NI_DAQ_GAIN_100 },
	ai_speed:           3000,
	ao_num_channels:    2,
	ao_num_bits:        16,
	ao_unipolar:        1,
	ao_fifo_depth:      2048,
	ao_has_ext_ref:     0,
	has_analog_trig:    1,
	atrig_caldac:       ad8522,
	atrig_low_ch:       0,
	atrig_high_ch:      1,
	atrig_bits:         12,
	num_mite_channels:  4,
	caldac:             { mb88341, ad8522 }
},

/* PCI-6071E */

{
	id:                 0x1350,
	name:               "pci-6071e",
	ai_num_channels:    64,
	ai_num_bits:        12,
	ai_fifo_depth:      512,
	ai_always_dither:   1,
	ai_gains:           { NI_DAQ_GAIN_0_5, NI_DAQ_GAIN_1,
			      NI_DAQ_GAIN_2, NI_DAQ_GAIN_5,
			      NI_DAQ_GAIN_10, NI_DAQ_GAIN_20,
			      NI_DAQ_GAIN_50, NI_DAQ_GAIN_100 },
	ai_speed:           800,
	ao_num_channels:    2,
	ao_num_bits:        12,
	ao_fifo_depth:      2048,
	ao_unipolar:        1,
	ao_has_ext_ref:     1,
	has_analog_trig:    1,
	atrig_caldac:       mb88341,
	atrig_low_ch:       11,
	atrig_high_ch:      12,
	atrig_bits:         8,
	num_mite_channels:  4,
	caldac:             { mb88341 }
},
};


/*
 * Local variables:
 * c-basic-offset: 8
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: 0
 * c-argdecl-indent: 4
 * c-label-ofset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: t
 * tab-width: 8
 * End:
 */