/* -*-C-*-
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-sensitive stack nounput noyy_top_state

%{

#include "fsc2.h"

static long Comm_Lc;

int devices_listlex( void );

%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

IDENT    [A-Za-z0-9][A-Za-z_0-9]*

%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}		EDL.Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		EDL.Lc++;

			/* handling of C style comment */
{REM3}		{
				Comm_Lc = EDL.Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

			/* end of line character in a comment */
\n			EDL.Lc++;

			/* handling of EOF within a comment -> fatal error */
<<EOF>>     {
				eprint( FATAL, UNSET, "%s: End of device data base '%s' in "
							   "comment starting at line %ld.\n",
							   EDL.Fname, Comm_Lc );
				THROW( EXCEPTION );
			}

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				EDL.Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     {  /* End of comment without start */
				eprint( FATAL, UNSET, "End of comment found at line %ld in "
							   "device data base '%s'.\n", EDL.Lc, EDL.Fname );
				THROW( EXCEPTION );
			}

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      EDL.Lc++;

{LWS}       /* dumps of leading white space */

			/* dumps trailing white space */
{TWS}       EDL.Lc++;

{WS}        /* dum (other) white space */

			/* writes out EOL character */
\n			EDL.Lc++;

,           /* skip commas */
;           /* skip semicolon */

{IDENT}     return 1;

.           {
				eprint( FATAL, UNSET, "Syntax error in devices data base '%s' "
							   "at line %ld.\n", EDL.Fname, EDL.Lc );
				THROW( EXCEPTION );
			}

<<EOF>>		return 0;



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


bool device_list_parse( void )
{
	static bool is_restart = UNSET;
	Device_Name *new_device_name;


	EDL.Fname = CHAR_P T_free( EDL.Fname );
	EDL.Fname = get_string( "%s%sDevices", libdir, slash( libdir ) );

	if ( ( devices_listin = fopen( EDL.Fname, "r" ) ) == NULL )
	{
		eprint( FATAL, UNSET, "Can't open device data base '%s'.\n",
					   EDL.Fname );
		return FAIL;
	}

	/* Keep the lexer happy... */

	if ( is_restart )
	    devices_listrestart( devices_listin );
	else
		 is_restart = SET;

	EDL.Lc = 1;

	TRY
	{
		while ( devices_listlex( ) )
		{
			new_device_name = DEVICE_NAME_P T_malloc( sizeof( Device_Name ) );
			new_device_name->name = T_strdup( devices_listtext );
			string_to_lower( new_device_name->name );
			new_device_name->next = EDL.Device_Name_List;
			EDL.Device_Name_List = new_device_name;
		}
   		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( devices_listin );
		RETHROW( );
	}

	fclose( devices_listin );
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
