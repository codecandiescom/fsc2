/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


typedef struct _IOBJ_ {

	long ID;                /* ID of object */
	FL_OBJECT *self;
	struct _IOBJ_ *prev;    /* pointer to previous object */
	struct _IOBJ_ *next;    /* pointer to next object */

	int type;               /* object type (BUTTON, SLIDER, etc.) */

	FL_COORD x,             /* position and dimensions of object */
		     y,
		     w,
		     h;

	const char *label;      /* object label */
	const char *help_text;  /* objects help text */
	char *form_str;         /* C format string for in/output objects */

	volatile int state;     /* state (on/off) of press count (buttons) */

	FL_OBJECT *group;       /* group a radio button belongs to */
	long partner;

	volatile double value;  /* current value of slider */
	double start_val,       /* maximum and minimum value */
		   end_val,
		   step;            /* step width of slider (0.0 means not set) */

	union {                 /* value of INT or FLOAT in/output objects */
		long lval;
		double dval;
	} val;

} IOBJECT;


typedef struct {
	int layout;             /* 0 / 1 <-> vertical / horizontal */
	FL_FORM *Tools;
	FL_COORD w,             /* size of form */
		     h;
	IOBJECT *objs;          /* linked list of objects in form */
} TOOL_BOX;


#define NORMAL_BUTTON 0
#define PUSH_BUTTON   1
#define RADIO_BUTTON  2
#define NORMAL_SLIDER 3
#define VALUE_SLIDER  4
#define INT_INPUT     5
#define FLOAT_INPUT   6
#define INT_OUTPUT    7
#define FLOAT_OUTPUT  8

#define VERT          0
#define HORI          1


#define MAX_INPUT_CHARS 32


/* exported functions */


Var *f_layout(  Var *v );
Var *f_bcreate( Var *v );
Var *f_bdelete( Var *v );
Var *f_bstate(  Var *v );
Var *f_screate( Var *v );
Var *f_sdelete( Var *v );
Var *f_svalue(  Var *v );
Var *f_icreate( Var *v );
Var *f_idelete( Var *v );
Var *f_ivalue(  Var *v );
Var *f_idelete( Var *v );
Var *f_ivalue(  Var *v );
Var *f_objdel(  Var *v );
Var *f_freeze(  Var *v );

void parent_freeze( int freeze );
void tools_clear( void );


#endif   /* ! INTERACTIVE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
