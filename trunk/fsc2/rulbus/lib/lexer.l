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

%option noyywrap nocase-sensitive nounput never-interactive 

%{

#include "parser.h"

%}

STRING   ["'][^"']+["']

		/*---------------*/
%%		/*     RULES     */
		/*---------------*/

#.*        /* Skip comments */

"file"     return FILE_TOKEN;
"rack"     return RACK_TOKEN;
"card"     return CARD_TOKEN;
"name"     return NAME_TOKEN;
"type"     return TYPE_TOKEN;
"addr"     return ADDR_TOKEN;
"range"    return RANGE_TOKEN;
"polar"    return POLAR_TOKEN;

0[0-3][0-7]{0,2} {   /* octal number between 0 and 0377 */
               sscanf( yytext, "%o", &yylval.ival ); 
               if ( yylval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

[0-9]|([1-9][0-9])|([1-2][0-9]{2}) {  /* decimal number between 0 and 255 */
               sscanf( yytext, "%d", &yylval.ival ); 
               if ( yylval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

0x[0-9A-F]{1,2} {  /* hexadecimal number between 0x0 and 0xff */
               sscanf( yytext, "0x%x", &yylval.ival );
               if ( yylval.ival > 255 )
                   return ERR_TOKEN;
               return NUM_TOKEN;
           }

[ \t\n]*   /* Skip white space */

{STRING}   {
               char *p;
               yylval.sval = malloc( strlen( yytext ) );
               strcpy( yylval.sval, yytext + 1 ); 
               p = yylval.sval;
		   
               yylval.sval = p;
               while ( *p != '"' )
                   p++;
               *p = '\0';
               return STR_TOKEN;
           }


"RB"[0-9]{1,4} {
				yylval.ival = atoi( yytext + 2 );
				return CARD_TYPE_TOKEN;
           }

.          return *yytext;

<<EOF>>    return 0;


		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
