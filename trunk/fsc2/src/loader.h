/*
  $Id$
*/


#if ! defined LOADER_HEADER
#define LOADER_HEADER

#include "fsc2.h"


void load_all_drivers( void );
bool exist_device( const char *name );
bool exist_function( const char *name );
void run_test_hooks( void );
void run_exp_hooks( void );
void run_end_of_exp_hooks( void );
void run_exit_hooks( void );
int get_lib_symbol( const char *from, const char *symbol, void **symbol_ptr );
void unload_device( Device *dev );


#endif  /* ! LOADER_HEADER */
