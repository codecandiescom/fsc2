/*
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


/***********************************************************************/
/* This is the syntax parser for the EXPERIMENT section of an EDL file */
/***********************************************************************/


%{

#include "fsc2.h"

void exp_test_init( void );

/* locally used functions */

static void exp_testerror( const char * s );

static bool Dont_print_error;
static bool In_cond;

%}


/* the following union and the token definitions MUST be identical to the ones
   in 'exp.h' ! */

%union {
    long   lval;
    double dval;
    char   *sptr;
    Var_T  *vptr;
}


%token <vptr> E_VAR_TOKEN    258       /* variable name */
%token <vptr> E_VAR_REF      259
%token <vptr> E_FUNC_TOKEN   260       /* function name */
%token E_INT_TOKEN           261
%token E_FLOAT_TOKEN         262
%token E_STR_TOKEN           263
%token E_EQ                  264
%token E_NE                  265
%token E_LT                  266
%token E_LE                  267
%token E_GT                  268
%token E_GE                  269
%token E_NEG                 270
%token E_AND                 271
%token E_OR                  272
%token E_XOR                 273
%token E_NOT                 274
%token E_PPOS                275
%token E_PLEN                276
%token E_PDPOS               277
%token E_PDLEN               278
%token E_PLSA                279
%token E_MINA                280
%token E_MULA                281
%token E_DIVA                282
%token E_MODA                283
%token E_EXPA                284

%token IF_TOK               2049
%token ELSE_TOK             2050
%token UNLESS_TOK           2051
%token WHILE_TOK            2052
%token UNTIL_TOK            2053
%token CONT_TOK             2054
%token BREAK_TOK            2055
%token REPEAT_TOK           2056
%token FOR_TOK              2057
%token FOREVER_TOK          2058

%left '?' ':'
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
       |                         { Dont_print_error = SET; }
         error                   { print( FATAL, "Missing semicolon before "
                                          "(or on) this line.\n" );
                                   THROW( EXCEPTION ); }
;

cond:    FOR_TOK                 { In_cond = SET; }
         E_VAR_TOKEN '='
         expr ':' expr fi ls     { In_cond = UNSET; }
       | FOREVER_TOK ls
       | sc                      { In_cond = SET; }
         expr ls                 { In_cond = UNSET; }
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
       |                         { Dont_print_error = SET; }
         error                   { print( FATAL, "Syntax error in loop or "
                                          "IF/UNLESS condition.\n" );
                                   THROW( EXCEPTION ); }
;

et:      ls
       | IF_TOK                  { In_cond = SET; }
         expr ls                 { In_cond = UNSET; }
;

line:    E_VAR_TOKEN ass               { }
       | E_VAR_TOKEN '[' list1 ']' ass { }
       | E_FUNC_TOKEN '(' list2 ')'    { }
       | E_FUNC_TOKEN                  { print( FATAL, "'%s' is a predefined "
                                                "function.\n", $1->name );
                                         THROW( EXCEPTION ); }
       | E_VAR_TOKEN '('               { print( FATAL, "'%s' is a variable, "
                                                "not a funnction.\n",
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

expr:    E_INT_TOKEN                  { }
       | E_FLOAT_TOKEN                { }
       | E_VAR_TOKEN                  { }
       | strs
       | E_VAR_REF                    { }
       | E_VAR_TOKEN '[' list1 ']'    { }
       | E_VAR_TOKEN '('              { print( FATAL, "'%s' is a variable, "
                                               "not a function.\n", $1->name );
                                        THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '(' list2 ')'   { }
       | E_FUNC_TOKEN                 { print( FATAL, "'%s' is a predefined "
                                               "function.\n", $1->name );
                                        THROW( EXCEPTION ); }
       | pt
       | bin
       | '+' expr %prec E_NEG
       | '-' expr %prec E_NEG
       | '(' expr ')'
       | E_NOT expr
       | expr '?' expr ':' expr
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

/* list of indices of array element */

list1:   /* empty */
       | l1e
;

l1e:     ind
       | l1e ',' ind
;

ind:     expr
       | expr ':' expr
;

/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     expr
       | l2e ',' expr
;

strs:    E_STR_TOKEN
       | strs E_STR_TOKEN
;

%%


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static void
exp_testerror( const char * s  UNUSED_ARG )
{
    if ( ! Dont_print_error && ! In_cond )
    {
        print( FATAL, "Syntax error in EXPERIMENT section.\n" );
        THROW( EXCEPTION );
    }

    if ( In_cond )
    {
        if ( ( EDL.cur_prg_token - 1 )->token == '=' )
            print( FATAL, "Assignment '=' used in loop or IF/UNLESS condition "
                   "instead of comparison '=='.\n" );
        else
            print( FATAL, "Syntax error in loop or IF/UNLESS condition.\n" );

        In_cond = UNSET;
        THROW( EXCEPTION );
    }

    Dont_print_error = UNSET;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

void
exp_test_init( void )
{
    Dont_print_error = UNSET;
    In_cond = UNSET;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
