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
bool exp_objdel( void *buffer, long len );


#endif  /* ! CHLD_FUNC_HEADER */
