/*
  $Id$

  Copyright (C) 2003 Jens Thoms Toerring
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
  To contact the author send email to:
  Jens.Toerring@physik.fu-berlin.de
*/


#include <witio_48_drv.h>


int witio_48_close( void );
int witio_48_set_mode( WITIO_48_DIO dio, WITIO_48_MODE mode );
int witio_48_get_mode( WITIO_48_DIO dio, WITIO_48_MODE *mode );
int witio_48_dio_out( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
					  unsigned long value );
int witio_48_dio_in( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
					 unsigned long *value );


#define WITIO_48_OK        0

#define WITIO_48_ERR_NSB  -1
#define WITIO_48_ERR_ICA  -2
#define WITIO_48_ERR_ICM  -3
#define WITIO_48_ERR_IVD  -4
#define WITIO_48_ERR_IMD  -5
#define WITIO_48_ERR_IDV  -6
#define WITIO_48_ERR_BNO  -8
#define WITIO_48_ERR_BBS  -7
#define WITIO_48_ERR_NDV  -9
#define WITIO_48_ERR_ACS -10
#define WITIO_48_ERR_DFM -11
#define WITIO_48_ERR_DFP -12

#define WITIO_48_ERR_INT -13

#define WITIO_48_ERR_NSB_MESS  "No such board"
#define WITIO_48_ERR_ICA_MESS  "Invalid channel number"
#define WITIO_48_ERR_ICM_MESS  "Invalid channel for current I/O mode"
#define WITIO_48_ERR_IVD_MESS  "Invalid DIO number"
#define WITIO_48_ERR_IMD_MESS  "Invalid mode"
#define WITIO_48_ERR_IDV_MESS  "Invalid output value"
#define WITIO_48_ERR_BBS_MESS  "Board is busy"
#define WITIO_48_ERR_BNO_MESS  "Board not open"
#define WITIO_48_ERR_NDV_MESS  "No driver loaded for board"
#define WITIO_48_ERR_ACS_MESS  "No permissions to open file"
#define WITIO_48_ERR_DFM_MESS  "Device file does not exist"
#define WITIO_48_ERR_DFP_MESS  "Unspecified error when opening device file"

#define WITIO_48_ERR_INT_MESS  "Internal driver or library error"
