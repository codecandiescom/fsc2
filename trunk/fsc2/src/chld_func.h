/*
  $Id$
*/

#if ! defined CHLD_FUNC_HEADER
#define CHLD_FUNC_HEADER



#include "fsc2.h"


typedef struct {
	int res;
	union {
		long lval;
		double dval;
	} val;
} INPUT_RES;


void show_message( const char *str );
void show_alert( const char *str );
int show_choices( const char *text, int numb, const char *b1, const char *b2,
				  const char *b3, int def );
const char *show_fselector( const char *message, const char *directory,
							const char *pattern, const char *def );
const char *show_input( const char *content, const char *label );
bool exp_layout( void *buffer, long len );
long *exp_bcreate( void *buffer, long len );
bool exp_bdelete( void *buffer, long len );
long exp_bstate( void *buffer, long len );
long *exp_screate( void *buffer, long len );
bool exp_sdelete( void *buffer, long len );
double *exp_sstate( void *buffer, long len );
long *exp_icreate( void *buffer, long len );
bool exp_idelete( void *buffer, long len );
INPUT_RES *exp_istate( void *buffer, long len );


#endif  /* ! CHLD_FUNC_HEADER */
