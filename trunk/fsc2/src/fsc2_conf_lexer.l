/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/


%option noyywrap noinput nounput never-interactive


%{

#include "fsc2_conf_parser.h"


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int fsc2_confget_lineno( void );
FILE * fsc2_confget_in( void );
FILE * fsc2_confget_out( void );
int fsc2_confget_leng( void );
char * fsc2_confget_text( void );
void fsc2_confset_lineno( int );
void fsc2_confset_in( FILE * );
void fsc2_confset_out( FILE * );
int fsc2_confget_debug( void );
void fsc2_confset_debug( int );
int fsc2_conflex_destroy( void );
#endif


%}


        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


"#".*                         /* Skip comments */
[\n\t ]+                      /* Skip white-space */
"DEFAULT_DIRECTORY"           return DEF_DIR;
"MAIN_WINDOW_POSITION"        return MW_POS;
"MAIN_WINDOW_SIZE"            return MW_SZ;
"DISPLAY_1D_WINDOW_POSITION"  return DW1_POS;
"DISPLAY_1D_WINDOW_SIZE"      return DW1_SZ;
"DISPLAY_2D_WINDOW_POSITION"  return DW2_POS;
"DISPLAY_2D_WINDOW_SIZE"      return DW2_SZ;
"CUT_WINDOW_POSITION"         return CW_POS;
"CUT_WINDOW_SIZE"             return CW_SZ;
"TOOLBOX_POSITION"            return TB_POS;
":"|"="                       return SEPARATOR;
[+-]*[0-9]+[+-][0-9]+         {
                                  fsc2_conflval.sval = fsc2_conftext;
                                  return POS;
                              }
[0-9]+"x"[0-9]+               {
                                  fsc2_conflval.sval = fsc2_conftext;
                                  return SZ;
                              }
[^#\t\n:= ]+                  {
                                  fsc2_conflval.sval = fsc2_conftext;
                                  return TEXT;
                              }
<<EOF>>                       return 0;


        /*------------------*/
%%      /*   End of rules   */
        /*------------------*/


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
