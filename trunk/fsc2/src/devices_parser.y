/*
  $Id$
*/

/*************************************************************/
/* This is the parser for the DEVICES section of an EDL file */
/*************************************************************/


%{

#include "fsc2.h"

extern int deviceslex( void );
extern char *devicestext;

/* locally used functions */

int deviceserror( const char *s );


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


int deviceserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *devicestext == '\0' )
		eprint( FATAL, SET, "Unexpected end of file in DEVICES section.\n" );
	else
		eprint( FATAL, SET, "Syntax error near token `%s'.\n", devicestext );
	THROW( EXCEPTION );
}
