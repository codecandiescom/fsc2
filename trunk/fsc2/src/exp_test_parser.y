/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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

extern int exp_testlex( void );           /* defined in exp.c */

/* locally defined functions */

int exp_testparse( void );
static int exp_testerror( const char *s );

%}


/* The following union and the token definitions MUST be identical to the ones
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
%token NEXT_TOK       2054
%token BREAK_TOK      2055
%token REPEAT_TOK     2056
%token FOR_TOK        2057
%token FOREVER_TOK    2058


%token <vptr> E_VAR_TOKEN
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN
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
%right E_NEG
%right '^'


%%


input:   /* empty */ 
       | input eol
       | input fc
       | input line eol
       | input line line
       { eprint( FATAL, SET, "Missing semicolon before (or on) this line.\n" );
	             THROW( EXCEPTION ); }
       | input line ','
       { eprint( FATAL, SET, "Missing semicolon before (or on) this line.\n" );
	             THROW( EXCEPTION ); }
;

eol:     ';'
       | '}'
;

/* Flow control statements */

fc :     FOR_TOK E_VAR_TOKEN '=' expr ':' expr fi '{'
       | FOREVER_TOK '{'
       | REPEAT_TOK expr '{'
       | WHILE_TOK expr '{'
       | UNTIL_TOK expr '{'
       | IF_TOK expr '{'
       | UNLESS_TOK expr '{'
       | ELSE_TOK et
       | BREAK_TOK eol
       | NEXT_TOK eol
;

fi:      /* empty */
	   | ':' expr
;

et:      '{'
       | IF_TOK expr '{'
;

/* Normal lines are either assignments or function calls */

line:    lhs ass_op expr	                          { }
       | E_FUNC_TOKEN '(' list2 ')'                   { }
       | E_FUNC_TOKEN '['
          { eprint( FATAL, SET, "`%s' is a predefined function.\n", $1->name );
	        THROW( EXCEPTION ); }
;

/* Left hand side of an assignment */ 

lhs:     lhs_var
       | E_PPOS
       | E_PLEN
       | E_PDPOS
       | E_PDLEN
;

lhs_var: E_VAR_TOKEN                                  { }
       | E_VAR_TOKEN '[' list1 ']'                    { }
       | E_VAR_TOKEN '('
          { eprint( FATAL, SET, "`%s' is a variable, not a function.\n",
					$1->name );
	        THROW( EXCEPTION ); }
       | E_FUNC_TOKEN
          { eprint( FATAL, SET, "Function `%s' can't be assigned a value.\n",
					$1->name );
	        THROW( EXCEPTION ); }
;

/* Assignment operators */

ass_op:  '='
       | E_PLSA
       | E_MINA
       | E_MULA
       | E_DIVA
       | E_MODA
       | E_EXPA
;

/* Right hand side expressions */

expr:    una_op u_expr %prec E_NEG
       | expr bin_op una_op u_expr
;

u_expr:  bt
       | '(' expr ')'
;

/* basic tokens of a right hand side expression */

bt:      E_INT_TOKEN unit                             { }
       | E_FLOAT_TOKEN unit                           { }
       | E_VAR_TOKEN unit                             { }
       | E_VAR_TOKEN '[' list1 ']' unit               { }
       | E_FUNC_TOKEN '(' list2 ')' unit              { }
       | E_VAR_REF                                    { }
       | E_VAR_TOKEN '('
          { eprint( FATAL, SET, "`%s' isn't a function.\n", $1->name );
	        THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['
          { eprint( FATAL, SET, "`%s' is a predefined function.\n", $1->name );
	        THROW( EXCEPTION ); }
       | E_PPOS                                       { }
       | E_PLEN                                       { }
       | E_PDPOS                                      { }
       | E_PDLEN                                      { }
;

/* Unary operators */

una_op:  /* empty */
       | '+'
       | '-'
       | E_NOT
;

/* Binary operators */

bin_op:  '+'
       | '-'
       | '*'
       | '/'
       | '%'
       | '^'
       | E_AND
       | E_OR
       | E_XOR
       | E_EQ
       | E_NE
       | E_LT
       | E_GT
       | E_LE
       | E_GE
;

/* Unit tokens */

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


/* List of indices for accessing array elements */

list1:   /* empty */
	   | expr
       | list1 ',' expr
;

/* List of function arguments */

list2:   /* empty */
       | exprs
	   | list2 ',' exprs
;

exprs:   expr
       | E_STR_TOKEN
         strs
;

strs:    /* empty */
       | strs E_STR_TOKEN
;


%%


static int exp_testerror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */


	eprint( FATAL, SET, "Syntax error in EXPERIMENT section.\n" );
	THROW( EXCEPTION );
}
