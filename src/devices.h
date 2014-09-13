/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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


void device_add( const char * /* name */ );

void device_append_to_list( const char * /* dev_name */ );

void delete_devices( void );

void delete_device_name_list( void );

/* from 'device_list_lexer.flex' */

extern bool device_list_parse( void );


#endif  /* ! DEVICE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
