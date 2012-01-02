/* -*-C-*-
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


%{

#include "fsc2.h"

extern int fsc2_conflex( void );
static void fsc2_conferror( const char * s );

%}

%union {
    char   *sval;
}

%token SEPARATOR DEF_DIR MW_POS MW_SZ DW1_POS DW1_SZ
%token DW2_POS DW2_SZ CW_POS CW_SZ TB_POS
%token <sval> TEXT POS SZ

%%

input:    /* empty */
        | input line
        | error                     { THROW( EXCEPTION ); }
;

line:     DEF_DIR SEPARATOR TEXT    { Fsc2_Internals.def_directory =
                                                              T_strdup( $3 ); }
        | MW_POS SEPARATOR POS      { if ( sscanf( $3, "%d%d", &GUI.win_x,
                                                   &GUI.win_y ) == 2 )
                                          GUI.win_has_pos = SET; }
        | MW_SZ SEPARATOR SZ        { if ( sscanf( $3, "%ux%u", &GUI.win_width,
                                                   &GUI.win_height ) == 2 )
                                          GUI.win_has_size = SET; }
        | DW1_POS SEPARATOR POS     { if ( sscanf( $3, "%d%d",
                                                   &GUI.display_1d_x,
                                                   &GUI.display_1d_y ) == 2 )
                                          GUI.display_1d_has_pos = SET; }
        | DW1_SZ SEPARATOR SZ       { if ( sscanf( $3, "%ux%u",
                                                   &GUI.display_1d_width,
                                                   &GUI.display_1d_height )
                                           == 2 )
                                          GUI.display_1d_has_size = SET; }
        | DW2_POS SEPARATOR POS     { if ( sscanf( $3, "%d%d",
                                                   &GUI.display_2d_x,
                                                   &GUI.display_2d_y ) == 2 )
                                          GUI.display_2d_has_pos = SET; }
        | DW2_SZ SEPARATOR SZ       { if ( sscanf( $3, "%ux%u",
                                                   &GUI.display_2d_width,
                                                   &GUI.display_2d_height )
                                           == 2 )
                                          GUI.display_2d_has_size = SET; }
        | CW_POS SEPARATOR POS      { if ( sscanf( $3, "%d%d", &GUI.cut_win_x,
                                                   &GUI.cut_win_y ) == 2 )
                                          GUI.cut_win_has_pos = SET; }
        | CW_SZ SEPARATOR SZ        { if ( sscanf( $3, "%ux%u",
                                                   &GUI.cut_win_width,
                                                   &GUI.cut_win_height ) == 2 )
                                          GUI.cut_win_has_size = SET; }
        | TB_POS SEPARATOR POS      { if ( sscanf( $3, "%d%d", &GUI.toolbox_x,
                                                   &GUI.toolbox_y ) == 2 )
                                          GUI.toolbox_has_pos = SET; }
;

%%


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void
fsc2_conferror( const char * s  UNUSED_ARG )
{
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
