/* -*-C-*-
  $Id$

  Copyright (C) 2003 Jens Thoms Toerring

  Library for Rulbus (Rijksuniversiteit Leiden BUS)

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  The library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the package; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap nocase-sensitive nounput never-interactive yylineno nounput

%{

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
unsigned int rulbus_column = 0;
int rulbus_lex( void );
static void count( void );

#define TAB_WIDTH 4

%}

STRING   [\"\'][^\"\']*[\"\']
INT         [0-9]+
EXPO        [Ee][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

		/*---------------*/
%%		/*     RULES     */
		/*---------------*/

#.*        { count( ); } /* Skip comments */

"file"     { count(); return FILE_TOKEN; }
"rack"     { count(); return RACK_TOKEN; }
"card"     { count(); return CARD_TOKEN; }
"name"     { count(); return NAME_TOKEN; }
"type"     { count(); return TYPE_TOKEN; }
"addr"     { count(); return ADDR_TOKEN; }
"nchan"    { count(); return NCHAN_TOKEN; }
"range"    { count(); return RANGE_TOKEN; }
"polar"    { count(); return POLAR_TOKEN; }
"ext_trg"  { count(); return EXTTRG_TOKEN; }

0[0-3][0-7]{0,2} {   /* octal number between 0 and 0377 */
			   count( );
               sscanf( rulbus_text, "%o", &rulbus_lval.ival ); 
               if ( rulbus_lval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

[0-9]|([1-9][0-9])|([1-2][0-9]{2}) {  /* decimal number between 0 and 255 */
			   count( );
               sscanf( rulbus_text, "%d", &rulbus_lval.ival ); 
               if ( rulbus_lval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

0x[0-9A-F]{1,2} {  /* hexadecimal number between 0x0 and 0xff */
			   count( );
               sscanf( rulbus_text, "0x%x", &rulbus_lval.ival );
               if ( rulbus_lval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

(FLOAT)    {
	           count( );
			   rulbus_lval.dval = atof( rulbus_text );
			   return FLOAT_TOKEN;
		   }

[ \t\n]*   {   count( ); } /* Skip white space */

{STRING}   {
               char *p;

			   count( );
               rulbus_lval.sval = p = rulbus_text + 1;
			   while ( *p && *p != *rulbus_text )
                   p++;
               *p = '\0';
               return STR_TOKEN;
           }


"RB"[0-9]{1,4} {
			   count( );
			   rulbus_lval.ival = atoi( rulbus_text + 2 );
			   return CARD_TYPE_TOKEN;
           }

.          {
			   count( );
			   return *rulbus_text;
           }

<<EOF>>    {
			   rulbus_column = 0;
			   return 0;
           }


		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


static void count( void )
{
	int i;

	for ( i = 0; rulbus_text[ i ] != '\0'; i++ )
		if ( rulbus_text[ i ] == '\n' )
			rulbus_column = 0;
		else if ( rulbus_text[ i ] == '\t' )
			rulbus_column += TAB_WIDTH - ( rulbus_column % TAB_WIDTH );
		else
			rulbus_column++;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
