/* -*-C-*-
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


/****************************************************************/
/* This is the lexer for the EXPERIMENTS section of an EDL file */
/****************************************************************/


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap noinput nounput never-interactive

%{

/* We have to make the lexer read the input byte by byte since it might be
   called from within another lexer and wants to use the same input - otherwise
   it would also read in input into its internal buffer belonging to the
   calling lexer. */

#define YY_INPUT( buf, result, max_size )                      \
{                                                              \
    char c;                                                    \
    ssize_t count;                                             \
    while (    ( count = read( fileno( expin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                \
        /* empty */ ;                                          \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );   \
}


#include "fsc2.h"


extern Token_Val_T Exp_Val;        /* defined in exp.c */


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int expget_lineno( void );
FILE * expget_in( void );
FILE * expget_out( void );
int expget_leng( void );
char * expget_text( void );
void expset_lineno( int );
void expset_in( FILE * );
void expset_out( FILE * );
int expget_debug( void );
void expset_debug( int );
int explex_destroy( void );
#endif


/* Locally used global variables */

int Exp_next_section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ESTR        \x5.*\x3\n.*\n
STR         \x5[^\x6]*\x6

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*ASS(IGNMENTS?)?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:
ON_STOP     ^[ \t]*ON_STOP:


INT         [0-9]+
EXPO        [Ee][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}(#(INT))?

F           F(UNC(TION)?)?
S           S(TART)?
L           L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PH          PH(ASE(SEQ(UENCE)?)?)?_?([0-9])?

CHT         ((CH((1[0-5])|[0-9]))|(AUX[12]?)|(MATH[1-3]?)|(REF[1-4])|(LIN(E)?)|(MEM_[CD])|(FUNC_[EF])|(TRACE_[A-D])|(M[1-4])|(F[1-8])|(EXT(10)?)|(DEFAULT_SOURCE)|(SOURCE_[0-3])|(NEXT_GATE)|(TIMEBASE_[12]))

DIO         DIO_?{INT}

WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*
FUNC        {IDENT}"#"[0-9]+



        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


            /* Handling of file name lines */
{FILE}      {
                *( exptext + expleng - 1 ) = '\0';
                EDL.Fname = T_free( EDL.Fname );
                EDL.Fname = T_strdup( exptext + 2 );
            }

            /* Handling of line number lines */
{LNUM}      {
                *( exptext + expleng - 1 ) = '\0';
                EDL.Lc = atol( exptext + 2 );
            }

            /* Handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, UNSET, "%s\n", exptext + 2 );
                THROW( EXCEPTION );
            }

{ESTR}      {
                exptext = strchr( exptext, '\x03' );
                eprint( FATAL, UNSET, "%s\n", exptext + 2 );
                THROW( EXCEPTION );
            }

            /* Handling of DEVICES: labels */
{DEV}       {
                Exp_next_section = DEVICES_SECTION;
                return 0;
            }

            /* Handling of ASSIGNMENTS: labels */
{ASS}       {
                Exp_next_section = ASSIGNMENTS_SECTION;
                return 0;
            }

"=="        return E_EQ;      /* equal */
"!="        return E_NE;      /* unequal */
"<"         return E_LT;      /* less than */
"<="        return E_LE;      /* less than or equal */
">"         return E_GT;      /* greater than */
">="        return E_GE;      /* greater than or equal */
"="         return '=';       /* assignment operator */
"["         return '[';       /* start of array indices */
"]"         return ']';       /* end of array indices */
","         return ',';       /* list separator */
"("         return '(';       /* start of function argument list */
")"         return ')';       /* end of function argument list */
"+"         return '+';       /* addition operator */
"-"         return '-';       /* subtraction operator or unary minus */
"*"         return '*';       /* multiplication operator */
"/"         return '/';       /* division operator */
"%"         return '%';       /* modulo operator */
"^"         return '^';       /* exponentiation operator */
":"         return ':';       /* condition separator in for loops */
"!"         return E_NOT;     /* logical negation operator */
"NOT"       return E_NOT;     /* logical negation operator */
"&"         return E_AND;     /* logical and operator */
"AND"       return E_AND;     /* logical and operator */
"|"         return E_OR;      /* logical or operator */
"OR"        return E_OR;      /* logical or operator */
"~"         return E_XOR;     /* logical exclusive or (xor) operator */
"XOR"       return E_XOR;     /* logical exclusive or (xor) operator */
"+="        return E_PLSA;
"-="        return E_MINA;
"*="        return E_MULA;
"/="        return E_DIVA;
"%="        return E_MODA;
"^="        return E_EXPA;
"?"         return '?';

            /* Handling of VARIABLES: labels */
{VAR}       {
                Exp_next_section = VARIABLES_SECTION;
                return 0;
            }

            /* Handling of PHASES: labels */
{PHAS}      {
                Exp_next_section = PHASES_SECTION;
                return 0;
            }

            /* Handling of PREPARATIONS: labels */
{PREP}      {
                Exp_next_section = PREPARATIONS_SECTION;
                return 0;
            }

            /* Handling of EXPERIMENT: labels */
{EXP}       {
                Exp_next_section = EXPERIMENT_SECTION;
                return 0;
            }

{ON_STOP}   return ON_STOP_TOK;

"WHILE"     return WHILE_TOK;
"UNTIL"     return UNTIL_TOK;
"BREAK"     return BREAK_TOK;
"NEXT"      return NEXT_TOK;
"IF"        return IF_TOK;
"ELSE"      return ELSE_TOK;
"UNLESS"    return UNLESS_TOK;
"REPEAT"    return REPEAT_TOK;
"FOR"       return FOR_TOK;
"FOREVER"   return FOREVER_TOK;
"{"         return '{';          /* block start delimiter */
"}"         return '}';          /* block end delimiter */

{INT}/[^_a-zA-Z] {
                Exp_Val.lval = T_atol( exptext );
                return E_INT_TOKEN;
            }

{FLOAT}/[^_a-zA-Z] {
                Exp_Val.dval = T_atod( exptext );
                return E_FLOAT_TOKEN;
            }

{STR}       {
                exptext[ expleng - 1 ] = '\0';
                Exp_Val.sptr = exptext + 1;
                return E_STR_TOKEN;
            }

{P}         {
                Exp_Val.lval = p_num( exptext );
                return E_INT_TOKEN;
            }

            /* Combinations of pulse and property, e.g. 'P3.LEN' */

{P}"."{F}   {
                Exp_Val.vptr = p_get( exptext, P_FUNC );
                return E_VAR_REF;
            }

{P}"."{S}   {
                Exp_Val.lval = p_num( exptext );
                return E_PPOS;
            }

{P}"."{L}   {
                Exp_Val.lval = p_num( exptext );
                return E_PLEN;
            }

{P}"."{DS}  {
                Exp_Val.lval = p_num( exptext );
                return E_PDPOS;
            }

{P}"."{DL}  {
                Exp_Val.lval = p_num( exptext );
                return E_PDLEN;
            }

{CHT}       {
                Exp_Val.vptr = convert_to_channel_number( exptext );
                return E_VAR_REF;
            }

{DIO}       {
                Exp_Val.lval = T_atol( exptext + 3 );
                return E_INT_TOKEN;
            }

{FUNC}      {
                char *t;
                int acc;

                t = strrchr( exptext, '#' );
                if ( t != NULL && ! strcmp( t, "#1" ) )
                    *t = '\0';

                Exp_Val.vptr = func_get( exptext, &acc );
                if ( Exp_Val.vptr != NULL )
                {
                    if ( acc == ACCESS_PREP )
                    {
                        print( FATAL, "Function %s() can't be used in the "
                               "EXPERIMENT section.\n", exptext );
                        THROW( EXCEPTION );
                    }

                    if ( ! strcmp( exptext, "end" ) && EDL.On_Stop_Pos >= 0 )
                    {
                        print( FATAL, "Function %s() can not be called "
                               "after the ON_STOP label.\n", exptext );
                        THROW( EXCEPTION );
                    }

                    return E_FUNC_TOKEN;
                }
                else
                {
                    print( FATAL, "Function '%s' is not loaded.\n", exptext );
                    THROW( EXCEPTION );
                }
            }

{IDENT}     {
                int acc;

                Exp_Val.vptr = func_get( exptext, &acc );
                if ( Exp_Val.vptr != NULL )
                {
                    if ( acc == ACCESS_PREP )
                    {
                        print( FATAL, "Function '%s' can't be used in the "
                               "EXPERIMENT section.\n", exptext );
                        THROW( EXCEPTION );
                    }

                    if ( ! strcmp( exptext, "end" ) && EDL.On_Stop_Pos >= 0 )
                    {
                        print( FATAL, "Function %s() can not be called "
                               "after the ON_STOP label.\n", exptext );
                        THROW( EXCEPTION );
                    }

                    return E_FUNC_TOKEN;
                }

                Exp_Val.vptr = vars_get( exptext );
                if ( Exp_Val.vptr == NULL )
                {
                    print( FATAL, "Variable or function '%s' has not been "
                           "declared or loaded.\n", exptext );
                    THROW( EXCEPTION );
                }

                return E_VAR_TOKEN;
            }

            /* Handling of end of statement character */
";"         return ';';

{WS}        /* Skip white space */

            /* Handling of invalid input */
.           {
                print( FATAL, "Invalid input near '%s'.\n", exptext );
                THROW( EXCEPTION );
            }

<<EOF>>     {
                Exp_next_section = NO_SECTION;
                return 0;
            }


        /*----------------------*/
%%      /*     End of Rules     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

int
experiment_parser( FILE * in )
{
    if ( EDL.compilation.sections[ EXPERIMENT_SECTION ] )
    {
        print( FATAL, "Multiple instances of EXPERIMENT section label.\n" );
        THROW( EXCEPTION );
    }
    EDL.compilation.sections[ EXPERIMENT_SECTION ] = SET;

    EDL.prg_length = 0;
    store_exp( in );
    exp_test_run( );

    if ( Exp_next_section != NO_SECTION )
    {
        print( FATAL, "EXPERIMENT section has to be the very last "
               "section.\n" );
        THROW( EXCEPTION );
    }

    return Exp_next_section;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
