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


#if ! defined DEVICE_HEADER
#define DEVICE_HEADER

#include "fsc2.h"

typedef struct Device Device_T;
typedef struct Device_Name Device_Name_T;


struct Device {
	char *name;
	Lib_Struct_T driver;
	bool is_loaded;
	const char *generic_type;
	const char *device_name;
	Device_T *next;
	Device_T *prev;
	int count;
};


struct Device_Name {
	char *name;
	Device_Name_T *next;
};


void device_add( const char *name );
void device_append_to_list( const char *dev_name );
void delete_devices( void );

void delete_device_name_list( void );

/* from `device_list_lexer.flex' */

bool device_list_parse( void );


#endif  /* ! DEVICE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
