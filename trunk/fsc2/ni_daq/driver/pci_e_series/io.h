/*
 *  $Id$
 * 
 *  Driver for National Instruments PCI E Series DAQ boards
 * 
 *  Copyright (C) 2003-2004 Jens Thoms Toerring
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


#if ! defined IO_HEADER
#define IO_HEADER


#if ! defined NI_DAQ_DEBUG
inline void pci_stc_writew( Board *board, u16 offset, u16 data );
inline void pci_stc_writel( Board *board, u16 offset, u32 data );
#else
inline void pci_stc_writew( const char *fn, Board *board, u16 offset,
							u16 data );
inline void pci_stc_writel( const char *fn, Board *board, u16 offset,
							u32 data );
#endif

inline u16 pci_stc_readw( Board *board, u16 offset );
inline u32 pci_stc_readl( Board *board, u16 offset );
void pci_init_critical_section_handling( Board *board );
inline void pci_start_critical_section( Board *board );
inline void pci_end_critical_section( Board *board );


#endif  /* IO_HEADER */


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
