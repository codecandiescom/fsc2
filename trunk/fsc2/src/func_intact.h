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

	int type;               /* button type (NORMAL, PUSH, RADIO, SLIDER) */

	FL_COORD x,             /* position and dimensions of object */
		     y,
		     w,
		     h;

	char *label;            /* object label */
	char *help_text;        /* objects help text */

	volatile int state;     /* state (on/off) or press count (buttons) */
	FL_OBJECT *group;       /* group (RADIO) button belongs to */
	long partner;

	volatile double value;
	double start_val,       /* start and end value */
		   end_val;

} IOBJECT;


typedef struct {
	int layout;
	FL_FORM *Tools;
	FL_OBJECT *background_box;
	FL_COORD w,
		     h;
	IOBJECT *objs;
} TOOL_BOX;


#define NORMAL_BUTTON 0
#define PUSH_BUTTON   1
#define RADIO_BUTTON  2
#define NORMAL_SLIDER 3
#define VALUE_SLIDER  4

#define VERT          0
#define HORI          1


#if ( SIZE == HI_RES )

#define OBJ_HEIGHT                50
#define OBJ_WIDTH                 210

#define BUTTON_HEIGHT             50
#define BUTTON_WIDTH              210
#define NORMAL_BUTTON_DELTA       10

#define SLIDER_HEIGHT             35
#define SLIDER_WIDTH              210
#define SLIDER_VERT_OFFSET        15

#define VERT_OFFSET               20
#define HORI_OFFSET               20

#define OFFSET_X0                 30
#define OFFSET_Y0                 30

#define FONT_SIZE                 FL_MEDIUM_SIZE

#else

#define OBJ_HEIGHT                35
#define OBJ_WIDTH                 140

#define BUTTON_HEIGHT             35
#define BUTTON_WIDTH              150
#define NORMAL_BUTTON_DELTA       5

#define SLIDER_HEIGHT             25
#define SLIDER_WIDTH              150
#define SLIDER_VERT_OFFSET        15

#define VERT_OFFSET               10
#define HORI_OFFSET               10

#define OFFSET_X0                 20
#define OFFSET_Y0                 20

#define FONT_SIZE                 FL_DEFAULT_SIZE

#endif


/* exported functions */


Var *f_layout( Var *v );
Var *f_bcreate( Var *v );
Var *f_bdelete( Var *v );
Var *f_bstate( Var *v );
Var *f_screate( Var *v );
Var *f_sdelete( Var *v );
Var *f_svalue( Var *v );

void tools_clear( void );


#endif   /* ! INTERACTIVE_HEADER */
