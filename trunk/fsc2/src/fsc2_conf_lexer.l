/* -*-C-*-
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


%option noyywrap case-sensitive nounput


%{

#include "fsc2_conf_parser.h"

int fsc2_conflex( void );

FILE *fsc2_confin;

%}


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


"#".*                /* Skip comments */
[\n\t ]+             /* Skip white-space */
"DEFAULT_DIRECTORY"  return DEF_DIR;
":"|"="              return SEPARATOR;
[^#\t\n:= ]+         {
						 fsc2_conflval.sval = fsc2_conftext;
						 return TEXT;
					 }
<<EOF>>              return 0;

%%
