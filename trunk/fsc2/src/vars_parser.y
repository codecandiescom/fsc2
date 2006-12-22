/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


%{

#include "fsc2.h"


void varsparser_init( void );

extern int varslex( void );    /* defined in vars_lexer.l */
extern char *varstext;         /* defined in vars_lexer.l */


/* locally used functions */

static void varserror( const char * s );

static ssize_t Max_level;
static ssize_t Cur_level;
static int Dont_exec = 0;

%}


%union {
    long   lval;
    double dval;
    char   *sptr;
    Var_T  *vptr;
}


%token SECTION_LABEL            /* new section label */
%token <vptr> VAR_TOKEN         /* variable */
%token <vptr> FUNC_TOKEN        /* function */
%token <vptr> VAR_REF
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ NE LT LE GT GE
%token AND OR XOR NOT


%type <vptr> arrass alist alist1 aitem expr list1 list1a list2 l2e ind strs


%left '?' ':'
%left EQ NE LT LE GT GE
%left AND OR XOR
%left '+' '-'
%left '*' '/'
%left '%'
%right NEG NOT
%right '^'


%%

input:   /* empty */
       | input ';'
       | input line ';'            { fsc2_assert( EDL.Var_Stack == NULL );
                                     fsc2_assert( Dont_exec == 0 ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { fsc2_assert( Dont_exec == 0 );
                                     YYACCEPT; }
;

line:    linet
       | line ',' linet
       | line linet                { THROW( MISSING_SEMICOLON_EXCEPTION ); }

;

linet:   VAR_TOKEN                 { }        /* no assignment to be done */
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 );
                                     Max_level = $1->dim; }
         arhs
       | FUNC_TOKEN '('            { print( FATAL, "Function calls can only "
                                            "be used in variable "
                                            "initialisations.\n" );
                                     THROW( EXCEPTION ); }
       | FUNC_TOKEN                { print( FATAL, "'%s()' is a predefind "
                                            "function.\n", $1->name );
                                     THROW( EXCEPTION ); }
       | VAR_TOKEN '('             { print( FATAL, "'%s' is an array and not "
                                             "a function.\n", $1->name );
                                     THROW( EXCEPTION ); }
;

expr:    INT_TOKEN                 { if ( ! Dont_exec )
                                         $$ = vars_push( INT_VAR, $1 ); }
       | FLOAT_TOKEN               { if ( ! Dont_exec )
                                         $$ = vars_push( FLOAT_VAR, $1 ); }
       | VAR_TOKEN                 { if ( ! Dont_exec )
                                     {
                                         if ( $1->flags & NEW_VARIABLE )
                                         {
                                             print( FATAL, "Variable '%s' "
                                                    "does not exist.\n",
                                                    $1->name );
                                             THROW( EXCEPTION );
                                         }
                                         $$ = vars_push_copy( $1 ); }
                                   }
       | VAR_TOKEN '['             { if ( ! Dont_exec )
                                     {
                                         if ( $1->flags & NEW_VARIABLE )
                                         {
                                             print( FATAL, "Variable '%s' "
                                                    "does not exist.\n",
                                                    $1->name );
                                             THROW( EXCEPTION );
                                         }
                                         vars_arr_start( $1 ); }
                                   }
         list2 ']'                 { if ( ! Dont_exec )
                                         $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list3 ')'  { if ( ! Dont_exec )
                                         $$ = func_call( $1 ); }
       | FUNC_TOKEN                { print( FATAL, "'%s()' is a predefind "
                                            "function.\n", $1->name );
                                     THROW( EXCEPTION ); }
       | strs
       | VAR_REF
       | VAR_TOKEN '('             { Dont_exec = 0;
                                     print( FATAL, "'%s' isn't a function.\n",
                                            $1->name );
                                     THROW( EXCEPTION ); }
       | expr AND                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                         {
                                             Dont_exec++;
                                             vars_pop( $1 );
                                         }
                                     }
                                     else
                                         Dont_exec++;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_AND, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 0L );;
                                   }
       | expr OR                   { if ( ! Dont_exec )
                                     {
                                         if ( check_result( $1 ) )
                                         {
                                             Dont_exec++;
                                             vars_pop( $1 );
                                         }
                                     }
                                     else
                                         Dont_exec++;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_OR, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 1L );
                                   }
       | expr XOR expr             { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr                  { if ( ! Dont_exec )
                                         $$ = vars_lnegate( $2 ); }
       | expr EQ expr              { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr              { if ( ! Dont_exec )
                                      $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $1, $3 ); }
       | expr GE expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $3, $1 ); }
       | expr '+' expr             { if ( ! Dont_exec )
                                         $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { if ( ! Dont_exec )
                                         $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { if ( ! Dont_exec )
                                         $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { if ( ! Dont_exec )
                                         $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { if ( ! Dont_exec )
                                         $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { if ( ! Dont_exec )
                                         $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec NEG        { if ( ! Dont_exec )
                                         $$ = $2; }
       | '-' expr %prec NEG        { if ( ! Dont_exec )
                                         $$ = vars_negate( $2 ); }
       | '(' expr ')'              { if ( ! Dont_exec )
                                         $$ = $2; }
       | expr '?'                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                             Dont_exec++;
                                         vars_pop( $1 );
                                     }
                                     else
                                         Dont_exec +=2;
                                   }
         expr ':'                  { if ( ! Dont_exec )
                                         Dont_exec++;
                                     else
                                         Dont_exec--;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = $7;
                                     else if ( ! --Dont_exec )
                                         $$ = $4;
                                   }
;

/* list of sizes of newly declared array */

list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
       | list1a list1b
;

list1a:   expr                     { $$ = $1; }
        | '*'                      { ( $$ = vars_push( INT_VAR, 0 ) )->flags
                                                               |= IS_DYNAMIC; }
;

list1b:   /* empty */
        | list1b ',' list1a
;

/* list of data for initialization of a newly declared array */

arhs:    /* empty */               { vars_arr_init( NULL ); }
        | '='                      { Cur_level = Max_level; }
          arrass                   { vars_arr_init( $3 ); }
        | '=' expr                 { vars_assign( $2, $2->prev ); }
;
         

arrass:  '{'                       { Cur_level--;
                                     vars_push( REF_PTR, NULL ); }
         alist '}'                 { $$ = vars_init_list( $3,
                                                          Cur_level++ ); }
;

alist:   /* empty */               { ( $$ = vars_push( UNDEF_VAR ) )->dim =
                                                                   Cur_level; }
        | alist1                   { $$ = $1; }
;

alist1:  aitem                     { $$ = $1; }
       | alist1 ',' aitem          { $$ = $3; }
;

aitem:   expr                      { $$ = $1; }
       | arrass                    { $$ = $1; }
;

/* list of indices for access of an array element */

list2:   /* empty */               { if ( ! Dont_exec )
                                         $$ = vars_push( UNDEF_VAR ); }
       | l2e                       { if ( ! Dont_exec )
                                         $$ = $1; }
;

l2e:     ind                       { if ( ! Dont_exec )
                                         $$ = $1; }
       | l2e ',' ind               { if ( ! Dont_exec )
                                         $$ = $3; }
;

ind:     expr                      { if ( ! Dont_exec )
                                         $$ = $1; }
       | expr ':'                  { if ( ! Dont_exec )
                                         vars_push( STR_VAR, ":" ); }
         expr                      { if ( ! Dont_exec )
                                         $$ = $4; }
;

/* list of function arguments */

list3:   /* empty */
       | l3e
;

l3e:      expr                     { }
        | l3e ',' expr             { }
;

strs:    STR_TOKEN                 { if ( ! Dont_exec )
                                          $$ = vars_push( STR_VAR, $1 ); }
       | strs STR_TOKEN            { if ( ! Dont_exec )
                                     {
                                          Var_T *v = vars_push( STR_VAR, $2 );
                                          $$ = vars_add( v->prev, v ); }
                                   }
;

%%


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void varserror( const char * s  UNUSED_ARG )
{
    if ( *varstext == '\0' )
        print( FATAL, "Unexpected end of file in VARIABLES section.\n");
    else
        print( FATAL, "Syntax error near '%s'.\n", varstext );
    THROW( EXCEPTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void varsparser_init( void )
{
    Dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
