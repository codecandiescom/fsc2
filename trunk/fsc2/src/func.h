/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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

#if ! defined FUNC_HEADER
#define FUNC_HEADER

#include "fsc2.h"


typedef struct {
	void *handle;
	int ( * init_hook ) ( void );
	bool is_init_hook;
	int ( * test_hook ) ( void );
	bool is_test_hook;
	int ( * end_of_test_hook ) ( void );
	bool is_end_of_test_hook;
	int ( * exp_hook ) ( void );
	bool is_exp_hook;
	int ( * end_of_exp_hook ) ( void );
	bool is_end_of_exp_hook;
	bool exp_hook_is_run;
	void ( * exit_hook ) ( void );
	bool is_exit_hook;
} Lib_Struct;


#include "devices.h"


typedef struct
{
	const char *name;                 /* name of the function */
    Var * ( * fnct )( Var * );        /* pointer to the function */
	long nargs;                       /* number of arguments */
    int access_flag;                  /* asscessibility flag */
	Device *device;                   /* handle of defining device module */
	bool to_be_loaded;                /* set if function has to be loaded */
} Func;


bool functions_init( void );
void functions_exit( void );
Var *func_get( const char *name, int *access );
Var *func_get_long( const char *name, int *access, bool flag );
Var *func_call( Var *f );
void close_all_files( void );

/* from func_list_lexer.flex */

int func_list_parse( Func **fncts, int num_func );

#endif  /* ! FUNC_HEADER */
