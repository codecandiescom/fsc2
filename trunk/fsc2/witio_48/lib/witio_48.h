/*
 *  $Id$
 * 
 *  Copyright (C) 2003-2006 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#ifdef __cplusplus
extern "C" {
#endif


#include <witio_48_drv.h>


int witio_48_close( void );

int witio_48_set_mode( WITIO_48_DIO  /* dio */,
					   WITIO_48_MODE /* mode */ );

int witio_48_get_mode( WITIO_48_DIO    /* dio */,
					   WITIO_48_MODE * /* mode */ );

int witio_48_dio_out( WITIO_48_DIO     /* dio */,
					  WITIO_48_CHANNEL /* channel */,
					  unsigned long    /* value */ );

int witio_48_dio_in( WITIO_48_DIO     /* dio */,
					 WITIO_48_CHANNEL /* channel */,
					 unsigned long *  /* value */ );

int witio_48_perror( const char * /* s */ );

const char *witio_48_strerror( void );


extern const char *witio_48_errlist[ ];
extern const int witio_48_nerr;


#define WITIO_48_OK        0
#define WITIO_48_ERR_ICA  -1
#define WITIO_48_ERR_ICM  -2
#define WITIO_48_ERR_IVD  -3
#define WITIO_48_ERR_IMD  -4
#define WITIO_48_ERR_IDV  -5
#define WITIO_48_ERR_BNO  -6
#define WITIO_48_ERR_BBS  -7
#define WITIO_48_ERR_NDV  -8
#define WITIO_48_ERR_ACS  -9
#define WITIO_48_ERR_DFM -10
#define WITIO_48_ERR_DFP -11
#define WITIO_48_ERR_INT -12


#ifdef __cplusplus
} /* extern "C" */
#endif
