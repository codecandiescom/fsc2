/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#if ! defined INTERACTIVE_HEADER
#define INTERACTIVE_HEADER

#include "fsc2.h"


typedef struct IOBJECT IOBJECT;
typedef struct TOOLBOX TOOLBOX;


struct IOBJECT {
	long ID;                  /* ID of object */
	FL_OBJECT *self;
	IOBJECT *prev;            /* pointer to previous object */
	IOBJECT *next;            /* pointer to next object */

	int type;                 /* object type (BUTTON, SLIDER, etc.) */

	FL_COORD x,               /* position and dimensions of object */
		     y,
		     w,
			 h;

	int wt,                   /* total width and height of object */
		ht;                   /* (including the label) */

	char *label;              /* object label */
	char *help_text;          /* objects help text */
	char *form_str;           /* C format string for in/output objects */
	char **menu_items;        /* list of menu items */
    int num_items;            /* number of entries of a menu */

	bool enabled;
	volatile int state;       /* state (on/off) of press count (buttons)
								 or currently seletced menu item */
	volatile bool is_changed; /* set when objects state changed but state 
								 change wasn't detected by the script yet */
	bool report_change;

	FL_OBJECT *group;         /* group a radio button belongs to */
	long partner;

	volatile double value;    /* current value of slider */
	double start_val,         /* maximum and minimum value */
		   end_val,
		   step;              /* step width of slider (0.0 means not set) */

	union {                   /* value of INT or FLOAT in/output objects */
		long lval;
		double dval;
	} val;

};


struct TOOLBOX {
	int layout;               /* 0 / 1 <-> vertical / horizontal */
	bool has_been_shown;
	FL_FORM *Tools;
	FL_COORD w,               /* size of form */
		     h;
	IOBJECT *objs;            /* linked list of objects in form */
	long next_ID;             /* ID for next created object */
};



#define NORMAL_BUTTON      0
#define PUSH_BUTTON        1
#define RADIO_BUTTON       2
#define NORMAL_SLIDER      3
#define VALUE_SLIDER       4
#define SLOW_NORMAL_SLIDER 5
#define SLOW_VALUE_SLIDER  6
#define INT_INPUT          7
#define FLOAT_INPUT        8
#define INT_OUTPUT         9
#define FLOAT_OUTPUT      10
#define MENU              11

#define VERT          0
#define HORI          1


#define MAX_INPUT_CHARS 32

#define IS_BUTTON( io )  \
		( ( io )->type >= NORMAL_BUTTON && ( io )->type <= RADIO_BUTTON )
#define IS_SLIDER( io )  \
		( ( io )->type >= NORMAL_SLIDER && ( io )->type <= SLOW_VALUE_SLIDER )



/* exported functions */


void toolbox_create( long layout );
void toolbox_delete( void );
Var *f_layout(  Var *v );
Var *f_objdel(  Var *v );
Var *f_freeze(  Var *v );
Var *f_obj_clabel( Var * );
Var *f_obj_xable( Var *v );
IOBJECT *find_object_from_ID( long ID );
void recreate_Toolbox( void );
void convert_escapes( char *str );
void check_label( char *str );
bool check_format_string( char *buf );
void store_geometry( void );
void parent_freeze( int freeze );
void tools_clear( void );
Var *f_tb_changed( Var *v );
void tb_wait_handler( long ID );
Var *f_tb_wait( Var *v );


#endif   /* ! INTERACTIVE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
