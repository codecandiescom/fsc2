/*
  $Id$
*/


#if ! defined XINIT_HEADER
#define XINIT_HEADER


bool xforms_init( int *argc, char *argv[ ] );
void xforms_close( void );
int main_form_close_handler( FL_FORM *a, void *b );
void win_slider_callback( FL_OBJECT *a, long b );


#endif   /* ! XINIT__HEADER */
