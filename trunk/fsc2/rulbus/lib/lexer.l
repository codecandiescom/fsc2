/* -*-C-*-
  $Id$

  Copyright (C) 2003-2004 Jens Thoms Toerring

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

%option noyywrap case-sensitive nounput never-interactive yylineno stack

%{

#define YY_NO_TOP_STATE

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int rulbus_column = 0;
int rulbus_lex( void );
static void count( void );

#define TAB_WIDTH 4

%}

STRING   [\"\'][^\"\']*[\"\']
INT      [0-9]+
EXPO     [Ee][+-]?{INT}
FLOAT    [+-]*((({INT}"."[0-9]*)|([0-9]*"."{INT})){EXPO}?)|({INT}{EXPO})
CARD     "rb"("8506_pia"|"8506_sifu"|"8506_via"|"8509_adc12"|"8510_dac12"|"8513_timebase"|"8514_delay"|"8515_clock"|"8905_adc12"|"9005_amplifier"|"9603_monochromator")
BOOL_T   "true"|"yes"
BOOL_F   "false"|"no"
UNIT     ("f"|"p"|"n"|"u"|"m"|"k"|"M"|"G"|"T")
OCT      "0"[0-9]*
HEX      "0"[xX][0-9a-fA-F]*
DEC      [+-]*[1-9][0-9]*


%x       comm

		/*---------------*/
%%		/*     RULES     */
		/*---------------*/

"#".*          { count( ); } /* Skip UNIX configuration file style comments */
"//".*         { count( ); } /* Skip C++ style comments */
"/*"           {  /* C style comment */
	                count( );
					yy_push_state( comm );
               }

<comm>{        /* Handling of C style comments */
"*"+"/"        { /* End of comment found */
	               count( );
				   yy_pop_state( );
			   }

.              { count( ); } /* Skip everything else in a comment */

<<EOF>>        { return EOFC_TOKEN; }  /* End of file in comment */

}              /* End of C style comment handling */

"file"         { count(); return FILE_TOKEN; }
"rack"         { count(); return RACK_TOKEN; }
"address"      { count(); return ADDR_TOKEN; }
"num_channels" { count(); return NCHAN_TOKEN; }
"volt_per_bit" { count(); return VPB_TOKEN; }
"bipolar"      { count(); return POL_TOKEN; }
"intr_delay"   { count(); return IDEL_TOKEN; }
"ext_trigger"  { count(); return EXTTRIG_TOKEN; }

"rb_generic"   {
			       count( );
			       rulbus_lval.ival = 0;
			       return CARD_TOKEN;
			   }

{CARD}         {
			       count( );
			       rulbus_lval.ival = atoi( rulbus_text + 2 );
			       return CARD_TOKEN;
               }

{UNIT}         {
	               count( );
	               switch ( *rulbus_text )
				   {
					   case 'f' :
						   rulbus_lval.dval = 1.0e-15;
						   break;
	
					   case 'p' :
						   rulbus_lval.dval = 1.0e-12;
						   break;

					   case 'n' :
						   rulbus_lval.dval = 1.0e-9;
						   break;

					   case 'u' :
						   rulbus_lval.dval = 1.0e-6;
						   break;

					   case 'm' :
						   rulbus_lval.dval = 1.0e-3;
						   break;

					   case 'k' :
						   rulbus_lval.dval = 1.0e3;
						   break;

					   case 'M' :
						   rulbus_lval.dval = 1.0e6;
						   break;

					   case 'G' :
						   rulbus_lval.dval = 1.0e9;
						   break;

					   case 'T' :
						   rulbus_lval.dval = 1.0e12;
						   break;
				   }

				   return UNIT_TOKEN;
			   }

{FLOAT}        {
	               count( );
			       rulbus_lval.dval = atof( rulbus_text );
			       return FLOAT_TOKEN;
		       }

{BOOL_T}       { /* true boolean value */
	               count( );
                   rulbus_lval.ival = 1;
                   return NUM_TOKEN;
               }

{BOOL_F}       { /* false boolean value */
	               count( );
                   rulbus_lval.ival = 0;
                   return NUM_TOKEN;
               }

{OCT}          { /* octal number */
			       count( );
                   sscanf( rulbus_text, "%o", &rulbus_lval.ival ); 
                   return NUM_TOKEN;
               }

{DEC}          { /* decimal number */
			       count( );
                   sscanf( rulbus_text, "%d", &rulbus_lval.ival ); 
                   return NUM_TOKEN;
               }

{HEX}          { /* hexadecimal number */
			       count( );
                   sscanf( rulbus_text, "0x%x", &rulbus_lval.ival );
                   return NUM_TOKEN;
               }

[ \t\n]+       { count( ); } /* Skip white space */

{STRING}       {
                   char *p;

			       count( );
                   rulbus_lval.sval = p = rulbus_text + 1;
			       while ( *p && *p != *rulbus_text )
                 	   p++;
			       if ( ! *p )
			     	   return ERR_TOKEN;
                   *p = '\0';
                   return STR_TOKEN;
               }

.              { count( ); return *rulbus_text; }

<<EOF>>        { rulbus_column = 0; return 0; }


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
