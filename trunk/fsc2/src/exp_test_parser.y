/*
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

/***********************************************************************/
/* This is the syntax parser for the EXPERIMENT section of an EDL file */
/***********************************************************************/


%{

#include "fsc2.h"

extern int exp_testlex( void );

/* locally used functions */

int exp_testparse( void );
void exp_test_init( void );
static void exp_testerror( const char *s );

static bool dont_print_error;
static bool in_cond;
static int allow_str;

%}


/* the following union and the token definitions MUST be identical to the ones
   in `exp.h' ! */

%union {
	long   lval;
    double dval;
	char   *sptr;
	Var    *vptr;
}

%token E_VAR_TOKEN	  258
%token E_VAR_REF	  259
%token E_FUNC_TOKEN	  260
%token E_INT_TOKEN	  261
%token E_FLOAT_TOKEN  262
%token E_STR_TOKEN	  263
%token E_EQ	          264
%token E_NE	          265
%token E_LT	          266
%token E_LE	          267
%token E_GT	          268
%token E_GE	          269
%token E_NT_TOKEN	  270
%token E_UT_TOKEN	  271
%token E_MT_TOKEN	  272
%token E_T_TOKEN	  273
%token E_KT_TOKEN	  274
%token E_MGT_TOKEN	  275
%token E_NU_TOKEN	  276
%token E_UU_TOKEN	  277
%token E_MU_TOKEN	  278
%token E_KU_TOKEN	  279
%token E_MEG_TOKEN	  280
%token E_NEG	      281
%token E_AND          282
%token E_OR           283
%token E_XOR          284
%token E_NOT          285
%token E_PPOS         286
%token E_PLEN         287
%token E_PDPOS        288
%token E_PDLEN        289
%token E_PLSA         290
%token E_MINA         291
%token E_MULA         292
%token E_DIVA         293
%token E_MODA         294
%token E_EXPA         295

%token IF_TOK         2049
%token ELSE_TOK       2050
%token UNLESS_TOK     2051
%token WHILE_TOK      2052
%token UNTIL_TOK      2053
%token CONT_TOK       2054
%token BREAK_TOK      2055
%token REPEAT_TOK     2056
%token FOR_TOK        2057
%token FOREVER_TOK    2058


%token <vptr> E_VAR_TOKEN         /* variable name */
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN        /* function name */
%token E_INT_TOKEN
%token E_FLOAT_TOKEN
%token E_STR_TOKEN
%token E_EQ E_NE E_LT E_LE E_GT E_GE
%token E_PPOS E_PLEN E_PDPOS E_PDLEN

%token E_NT_TOKEN E_UT_TOKEN E_MT_TOKEN E_T_TOKEN E_KT_TOKEN E_MGT_TOKEN
%token E_NU_TOKEN E_UU_TOKEN E_MU_TOKEN E_KU_TOKEN E_MEG_TOKEN

%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG E_NOT
%right '^'


%%

input:   /* empty */
       | input eol
       | input cond
       | input line eol
;

eol:     ';'
       | '}'
       |                         { dont_print_error = SET; }
         error                   { eprint( FATAL, SET, "Missing semicolon "
										   "before (or on) this line.\n" );
	                               THROW( EXCEPTION ); }
;

cond:    FOR_TOK                 { in_cond = SET; }
         E_VAR_TOKEN '='
         expr ':' expr fi ls     { in_cond = UNSET; }
       | FOREVER_TOK ls
       | sc                      { in_cond = SET;
	   							   allow_str++; }
         expr ls                 { in_cond = UNSET;
                                   allow_str--; }
       | ELSE_TOK et
;

sc:      REPEAT_TOK
       | WHILE_TOK
       | UNTIL_TOK
       | IF_TOK
       | UNLESS_TOK
;

fi:      /* empty */
	   | ':' expr
;

ls:      '{'
       |                         { dont_print_error = SET; }
         error                   { eprint( FATAL, SET, "Syntax error in "
									       "loop or IF/UNLESS condition.\n" );
	                               THROW( EXCEPTION ); }
;

et:      ls
       | IF_TOK                  { in_cond = SET;
                                   allow_str++; }
         expr ls                 { in_cond = UNSET;
                                   allow_str--; }


line:    E_VAR_TOKEN ass                             { }
       | E_VAR_TOKEN '[' list ']' ass                { }
       | E_FUNC_TOKEN '('                            { allow_str++; }
	     list
         ')'                                         { allow_str--; }
       | E_FUNC_TOKEN '['
          { eprint( FATAL, SET, "`%s' is a predefined function.\n", $1->name );
		    THROW( EXCEPTION ); }
       | E_VAR_TOKEN '('
          { eprint( FATAL, SET, "`%s' is a variable, not a funnction.\n",
					$1->name );
		    THROW( EXCEPTION ); }
       | pt ass
       | BREAK_TOK
       | CONT_TOK
;

pt:      E_PPOS
       | E_PLEN
       | E_PDPOS
       | E_PDLEN
;

ass:     '=' expr
       | E_PLSA expr
       | E_MINA expr
       | E_MULA expr
       | E_DIVA expr
       | E_MODA expr
       | E_EXPA expr
;

expr:    E_INT_TOKEN unit               { }
       | E_FLOAT_TOKEN unit             { }
       | E_VAR_TOKEN unit               { }
       | E_VAR_TOKEN '[' list ']' unit  { }
       | E_FUNC_TOKEN '('               { allow_str++; }
	     list
	     ')'                            { allow_str--; }
	     unit                           { }
       | E_VAR_REF                      { }
       | E_STR_TOKEN              { if ( allow_str <= 0 )
	                                {
		                                eprint( FATAL, SET, "No string "
												"allowed in this context.\n" );
										THROW( EXCEPTION );
									}
	                              }
       | E_FUNC_TOKEN '['         { eprint( FATAL, SET, "`%s' is a predefined "
									        "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_VAR_TOKEN '('          { eprint( FATAL, SET, "`%s' is a variable, "
											"not a function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | pt
       | bin
       | '+' expr %prec E_NEG
       | '-' expr %prec E_NEG
       | '(' expr ')' unit
       | E_NOT expr
;

bin:     expr E_AND expr
       | expr E_OR expr
       | expr E_XOR expr
       | expr E_EQ expr
       | expr E_NE expr
       | expr E_LT expr
       | expr E_GT expr
       | expr E_LE expr
       | expr E_GE expr
       | expr '+' expr
       | expr '-' expr
       | expr '*' expr
       | expr '/' expr
       | expr '%' expr
       | expr '^' expr
;

unit:    /* empty */
       | E_NT_TOKEN
       | E_UT_TOKEN
       | E_MT_TOKEN
       | E_T_TOKEN
       | E_KT_TOKEN
       | E_MGT_TOKEN
       | E_NU_TOKEN
       | E_UU_TOKEN
       | E_MU_TOKEN
       | E_KU_TOKEN
       | E_MEG_TOKEN
;

/* list of indices of array element or list of function arguments */

list:    /* empty */
	   | expr
       | list ',' expr
;

%%


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

void exp_test_init( void )
{
	dont_print_error = UNSET;
	in_cond = UNSET;
	allow_str = 0;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

static void exp_testerror( const char *s )
{
	s = s;                    /* avoid compiler warning */


	if ( ! dont_print_error && ! in_cond )
	{
		eprint( FATAL, SET, "Syntax error in EXPERIMENT section.\n" );
		THROW( EXCEPTION );
	}

	if ( in_cond )
	{
		if ( ( EDL.cur_prg_token - 1 )->token == '=' )
			eprint( FATAL, SET, "Assignment '=' used in loop or IF/UNLESS "
					"condition instead of comparison '=='.\n" );
		else
			eprint( FATAL, SET, "Syntax error in loop or IF/UNLESS "
					"condition.\n" );

		in_cond = UNSET;
		THROW( EXCEPTION );
	}

	dont_print_error = UNSET;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
