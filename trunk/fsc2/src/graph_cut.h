/*
  $Id$
*/

#if ! defined GRAPH_CUT_HEADER
#define GRAPH_CUT_HEADER


#include "fsc2.h"

void show_cut( int dir, int pos );
void cut_form_close( void );
void cut_undo_button_callback( FL_OBJECT *a, long b );
void cut_close_callback( FL_OBJECT *a, long b );
void cut_fs_button_callback( FL_OBJECT *a, long b );


#endif   /* ! GRAPH_CUT_HEADER */
