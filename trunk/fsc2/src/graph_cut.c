/*
  $Id$
*/


#include "fsc2.h"


void show_cut( int dir, int pos )
{
	if ( ! fl_form_is_visible( cut_form->cut ) )
		 fl_show_form( cut_form->cut,
					   FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
					   "fsc2: Cross section" );
	fl_raise_form( cut_form->cut );

}


void cut_form_close( void )
{
	if ( fl_form_is_visible( cut_form->cut ) )
		fl_hide_form( cut_form->cut );
	fl_free_form( cut_form->cut );
}


void cut_undo_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}


void cut_close_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
	fl_hide_form( cut_form->cut );
}


void cut_fs_button_callback( FL_OBJECT *a, long b )
{
	a = a;
	b = b;
}
