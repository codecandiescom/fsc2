/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#if ! defined LOADER_HEADER
#define LOADER_HEADER

#include "fsc2.h"


void load_all_drivers( void );

int exists_device( const char * /* name */ );

bool exists_device_type( const char * /* type */ );

bool exists_function( const char * /* name */ );

void run_test_hooks( void );

void run_end_of_test_hooks( void );

void run_exp_hooks( void );

void run_end_of_exp_hooks( void );

void run_exit_hooks( void );

void run_child_exit_hooks( void );

int get_lib_symbol( const char * /* from       */,
                    const char * /* symbol     */,
                    void **      /* symbol_ptr */  );

void unload_device( Device_T * /* dev */ );


#endif  /* ! LOADER_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
