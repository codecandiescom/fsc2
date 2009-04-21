/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#if ! defined INTERACTIVE_HEADER
#define INTERACTIVE_HEADER

#include "fsc2.h"


typedef struct Iobject Iobject_T;
typedef struct Toolbox Toolbox_T;


typedef enum {
    NORMAL_BUTTON = 1,
    PUSH_BUTTON,
    RADIO_BUTTON,
    NORMAL_SLIDER,
    VALUE_SLIDER,
    SLOW_NORMAL_SLIDER,
    SLOW_VALUE_SLIDER,
    INT_INPUT,
    FLOAT_INPUT,
    INT_OUTPUT,
    FLOAT_OUTPUT,
    STRING_OUTPUT,
    MENU,
} Iobject_Type_T;


/* The following defines need to be kept in sync with the contents of
   the above typedef for the object types! */

#define FIRST_BUTTON_TYPE    NORMAL_BUTTON
#define LAST_BUTTON_TYPE     RADIO_BUTTON
#define FIRST_SLIDER_TYPE    NORMAL_SLIDER
#define LAST_SLIDER_TYPE     SLOW_VALUE_SLIDER
#define FIRST_INPUT_TYPE     INT_INPUT
#define LAST_INPUT_TYPE      FLOAT_INPUT
#define FIRST_OUTPUT_TYPE    INT_OUTPUT
#define LAST_OUTPUT_TYPE     STRING_OUTPUT
#define FIRST_INOUTPUT_TYPE  INT_INPUT
#define LAST_INOUTPUT_TYPE   STRING_OUTPUT


struct Iobject {
    long ID;                  /* ID of object */
    FL_OBJECT *self;
    Iobject_T *prev;          /* pointer to previous object */
    Iobject_T *next;          /* pointer to next object */

    Iobject_Type_T type;      /* object type (BUTTON, SLIDER, etc.) */

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

    union {                   /* value of INT, FLOAT or STRING input or */
        long lval;            /* output objects */
        double dval;
        char *sptr;
    } val;

};


struct Toolbox {
    int layout;               /* 0 / 1 <-> vertical / horizontal */
    FL_FORM *Tools;
    FL_COORD w,               /* size of form */
             h;
    Iobject_T *objs;          /* linked list of objects in form */
    long next_ID;             /* ID for next created object */
};


#define VERT          0
#define HORI          1


/* The maximum number of characters shown in input and output objects */

#define MAX_INPUT_CHARS 32


#define IS_BUTTON( type )  \
        ( ( type ) >= FIRST_BUTTON_TYPE   && ( type ) <= LAST_BUTTON_TYPE )
#define IS_SLIDER( type )  \
        ( ( type ) >= FIRST_SLIDER_TYPE   && ( type ) <= LAST_SLIDER_TYPE )
#define IS_INPUT( type )   \
        ( ( type ) >= FIRST_INPUT_TYPE    && ( type ) <= LAST_INPUT_TYPE )
#define IS_OUTPUT( type )   \
        ( ( type )>= FIRST_OUTPUT_TYPE    && ( type ) <= LAST_OUTPUT_TYPE )
#define IS_INOUTPUT( type )   \
        ( ( type ) >= FIRST_INOUTPUT_TYPE && ( type ) <= LAST_INOUTPUT_TYPE )


/* exported functions */


void toolbox_create( long /* layout */ );

void toolbox_delete( void );

Var_T *f_layout(  Var_T * /* v */ );

Var_T *f_objdel(  Var_T * /* v */ );

Var_T *f_freeze(  Var_T * /* v */ );

Var_T *f_obj_clabel( Var_T * /* v */ );

Var_T *f_obj_xable( Var_T * /* v */ );

Iobject_T *find_object_from_ID( long /* ID */ );

void recreate_Toolbox( void );

void convert_escapes( char * /* str */ );

void check_label( const char * /* str */ );

bool check_format_string( const char * /* buf */ );

void parent_freeze( int /* freeze */ );

void tools_clear( void );

Var_T *f_tb_changed( Var_T * /* v */ );

void tb_wait_handler( long /* ID */ );

Var_T *f_tb_wait( Var_T * /* v */ );


#endif   /* ! INTERACTIVE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
