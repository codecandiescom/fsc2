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

/*************************************************************/
/* This is the parser for the DEVICES section of an EDL file */
/*************************************************************/


%{

#include "fsc2.h"

extern int deviceslex( void );
extern char *devicestext;

/* locally used functions */

static void deviceserror( const char *s );


%}


%union {
	char *sptr;
}


%token SECTION_LABEL         /* new section label */
%token <sptr> DEV_TOKEN


%%


input:   /* empty */
       | input ';'
       | input DEV_TOKEN              { device_add( $2 ); }
         sep
       | input SECTION_LABEL          { YYACCEPT; }
       | input error
;

sep:     ';'
	   | DEV_TOKEN                    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
	   | SECTION_LABEL                { THROW( MISSING_SEMICOLON_EXCEPTION ); }


%%


void deviceserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *devicestext == '\0' )
		eprint( FATAL, SET, "Unexpected end of file in DEVICES section.\n" );
	else
		eprint( FATAL, SET, "Syntax error near token `%s'.\n", devicestext );
	THROW( EXCEPTION );
}
