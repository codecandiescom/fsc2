/*
  $Id$
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

	char *label;            /* object label */
	char *help_text;        /* objects help text */
	char *form_str;         /* C format string for in/output objects */

	volatile int state;     /* state (on/off) of press count (buttons) */

	FL_OBJECT *group;       /* group (RADIO) button belongs to */
	long partner;

	volatile double value;  /* current value of slider */
	double start_val,       /* maximum and minimum value */
		   end_val,
		   step;

	union {
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

#if ( SIZE == HI_RES )

#define OBJ_HEIGHT                50
#define OBJ_WIDTH                 210

#define BUTTON_HEIGHT             50
#define BUTTON_WIDTH              210
#define NORMAL_BUTTON_DELTA       10

#define SLIDER_HEIGHT             35
#define SLIDER_WIDTH              210

#define INPUT_HEIGHT              35
#define INPUT_WIDTH               210

#define LABEL_VERT_OFFSET         15
#define VERT_OFFSET               15
#define HORI_OFFSET               20

#define OFFSET_X0                 20
#define OFFSET_Y0                 20

#else

#define OBJ_HEIGHT                35
#define OBJ_WIDTH                 140

#define BUTTON_HEIGHT             35
#define BUTTON_WIDTH              150
#define NORMAL_BUTTON_DELTA       5

#define SLIDER_HEIGHT             25
#define SLIDER_WIDTH              150

#define INPUT_HEIGHT              25
#define INPUT_WIDTH               150

#define LABEL_VERT_OFFSET         15
#define VERT_OFFSET               10
#define HORI_OFFSET               10

#define OFFSET_X0                 20
#define OFFSET_Y0                 20

#endif


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

void tools_clear( void );


#endif   /* ! INTERACTIVE_HEADER */
