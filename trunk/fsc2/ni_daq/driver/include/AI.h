/*
 *  $Id$
 * 
 *  Driver for National Instruments DAQ boards with a DAQ-STC
 * 
 *  Copyright (C) 2003-2005 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 * 
 *  To contact the author send email to
 *  Jens.Toerring@physik.fu-berlin.de
 */


#if ! defined AI_HEADER
#define AI_HEADER


void AI_reset_all( Board *board );
int AI_ioctl_handler( Board *board, NI_DAQ_AI_ARG *arg );
void AI_irq_handler( Board *board );
void AI_SC_irq_handler( Board *board );
int AI_start_acq( Board *board );


#endif /* AI_HEADER */


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
