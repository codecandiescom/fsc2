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




%token SECTION_LABEL         /* new section label */
%token DEV_TOKEN


%%


input:   /* empty */
       | input DEV_TOKEN              { device_add( devicestext ); }
       | input SECTION_LABEL          { YYACCEPT; }
       | input error
;

%%


int deviceserror ( const char *s )
{
	if ( *devicestext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in DEVICES "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, devicestext );
	THROW( DEVICES_EXCEPTION );
}
