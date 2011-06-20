/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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



#if ! defined EXP_PARSER_COMMON_HEADER
#define EXP_PARSER_COMMON_HEADER


#define E_VAR_TOKEN     258
#define E_VAR_REF       259
#define E_FUNC_TOKEN    260
#define E_INT_TOKEN     261
#define E_FLOAT_TOKEN   262
#define E_STR_TOKEN     263
#define E_EQ            264
#define E_NE            265
#define E_LT            266
#define E_LE            267
#define E_GT            268
#define E_GE            269
#define E_NEG           270
#define E_AND           271
#define E_OR            272
#define E_XOR           273
#define E_NOT           274
#define E_PPOS          275
#define E_PLEN          276
#define E_PDPOS         277
#define E_PDLEN         278
#define E_PLSA          279
#define E_MINA          280
#define E_MULA          281
#define E_DIVA          282
#define E_MODA          283
#define E_EXPA          284


/* The following don't get defined via `exp_run_parser.y' but are for local
   use only (actually, also `run.c' needs them also and gets them from here) */

#define IF_TOK         2049
#define ELSE_TOK       2050
#define UNLESS_TOK     2051
#define WHILE_TOK      2052
#define UNTIL_TOK      2053
#define NEXT_TOK       2054
#define BREAK_TOK      2055
#define REPEAT_TOK     2056
#define FOR_TOK        2057
#define FOREVER_TOK    2058
#define ON_STOP_TOK    3333


#endif  /* ! EXP_PARSER_COMMON_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
