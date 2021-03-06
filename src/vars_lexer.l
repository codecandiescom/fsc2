/* -*-C-*-
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


/**************************************************************/
/* This is the lexer for the VARIABLES section of an EDL file */
/**************************************************************/


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap noinput nounput never-interactive

%{

/* We have to make the lexer read the input byte by byte since it might
   be called from within another lexer and wants to use the same input -
   otherwise it would also read in input into its internal buffer belonging
   to the calling lexer. */

#define YY_INPUT( buf, result, max_size )                       \
{                                                               \
    char c;                                                     \
    ssize_t count;                                              \
    while (    ( count = read( fileno( varsin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                 \
        /* empty */ ;                                           \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );    \
}


#include "fsc2.h"
#include "vars_parser.h"


extern void varsparser_init( void );

#if defined FLEX_NEEDS_BISON_DECLARATIONS
extern void varsparse( void );
#endif


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int varsget_lineno( void );
FILE * varsget_in( void );
FILE * varsget_out( void );
int varsget_leng( void );
char * varsget_text( void );
void varsset_lineno( int );
void varsset_in( FILE * );
void varsset_out( FILE * );
int varsget_debug( void );
void varsset_debug( int );
int varslex_destroy( void );
#endif


/* Locally used global variables */

int Vars_next_section;


%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n
INCL        \x7.*\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?S?:

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

INT         [0-9]+
EXPO        [Ee][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

CHT         ((CH((1[0-5])|[0-9]))|(AUX[12]?)|(MATH[1-4])|(REF[1-4])|(LIN(E)?)|(MEM_[CD])|(FUNC_[EF])|(TRACE_[A-D])|(M[1-4])|(F[1-8])|(EXT(10)?)|(DEFAULT_SOURCE)|(SOURCE_[0-3])|(NEXT_GATE)|(TIMEBASE_[12]))

DIO         DIO_?{INT}

IDENT       [A-Za-z]+[A-Za-z0-9_]*
FUNC        {IDENT}"#"[0-9]+

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L           L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?

CTRL        (WHILE)|(UNTIL)|(BREAK)|(NEXT)|(IF)|(ELSE)|(REPEAT)|(FOR)

WS          [\n \t]+

        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


            /* Handling of file name lines */
{FILE}      {
                *( varstext + varsleng - 1 ) = '\0';
                EDL.Fname = T_free( EDL.Fname );
                EDL.Fname = T_strdup( varstext + 2 );
            }

            /* Handling of line number lines */
{LNUM}      {
                *( varstext + varsleng - 1 ) = '\0';
                EDL.Lc = atol( varstext + 2 );
            }

            /* Handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, false, "%s\n", varstext + 2 );
                THROW( EXCEPTION );
            }

            /* Handling of new file included via #INCLUDE */
{INCL}      {
                varstext[ strlen( varstext ) - 1 ] = '\0';
                get_edl_file( varstext + 1 );
            }

{ESTR}      {
                varstext = strchr( varstext, '\x03' );
                eprint( FATAL, false, "%s\n", varstext + 2 );
                THROW( EXCEPTION );
            }

            /* Handling of DEVICES: labels */
{DEV}       {
                Vars_next_section = DEVICES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of ASSIGNMENTS: label */
{ASS}       {
                Vars_next_section = ASSIGNMENTS_SECTION;
                return SECTION_LABEL;
            }

"=="        return EQ;        /* equal */
"!="        return NE;        /* unequal */
"<"         return LT;        /* less than */
"<="        return LE;        /* less than or equal */
">"         return GT;        /* greater than */
">="        return GE;        /* greater than or equal */
"="         return '=';       /* assignment operator */
"["         return '[';       /* start of array indices */
"]"         return ']';       /* end of array indices */
","         return ',';       /* list separator */
"("         return '(';       /* start of function argument list */
")"         return ')';       /* end of function argument list */
"{"         return '{';       /* start of initialisation data list */
"}"         return '}';       /* end of initialisation data list */
"+"         return '+';       /* addition operator */
"-"         return '-';       /* subtraction operator or unary minus */
"*"         return '*';       /* multiplication operator */
"/"         return '/';       /* division operator */
"%"         return '%';       /* modulo operator */
"^"         return '^';       /* exponentiation operator */
"!"         return NOT;       /* logical negation operator */
"NOT"       return NOT;       /* logical negation operator */
"&"         return AND;       /* logical and operator */
"AND"       return AND;       /* logical and operator */
"|"         return OR;        /* logical or operator */
"OR"        return OR;        /* logical or operator */
"~"         return XOR;       /* logical exclusive or (xor) operator */
"XOR"       return XOR;       /* logical exclusive or (xor) operator */
"?"         return '?';
":"         return ':';

            /* Handling of VARIABLES: label */
{VAR}       {
                Vars_next_section = VARIABLES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of PHASES: label */
{PHAS}      {
                Vars_next_section = PHASES_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of PREPARATIONS: label */
{PREP}      {
                Vars_next_section = PREPARATIONS_SECTION;
                return SECTION_LABEL;
            }

            /* Handling of EXPERIMENT: label */
{EXP}       {
                Vars_next_section = EXPERIMENT_SECTION;
                return SECTION_LABEL;
            }

            /* All needed pulse related keywords... */

{P}"."{F}   {
                varslval.vptr = p_get( varstext, P_FUNC );
                return VAR_REF;
            }

{P}"."{S}   {
                varslval.vptr = p_get( varstext, P_POS );
                return VAR_REF;
            }

{P}"."{L}   {
                varslval.vptr = p_get( varstext, P_LEN );
                return VAR_REF;
            }

{P}"."{DS}  {
                varslval.vptr = p_get( varstext, P_DPOS );
                return VAR_REF;
            }

{P}"."{DL}  {
                varslval.vptr = p_get( varstext, P_DLEN );
                return VAR_REF;
            }

{CHT}       {
                varslval.vptr = convert_to_channel_number( varstext );
                return VAR_REF;
            }

{DIO}       {
                varslval.lval = T_atol( varstext + 3 );
                return INT_TOKEN;
            }

            /* Handling of integer and floating point numbers */
{INT}/[^_a-zA-Z] {
                varslval.lval = T_atol( varstext );
                return INT_TOKEN;
            }

{FLOAT}/[^_a-zA-Z] {
                varslval.dval = T_atod( varstext );
                return FLOAT_TOKEN;
            }

            /* Handling of string constants */
{STR}       {
                varstext[ strlen( varstext ) - 1 ] = '\0';
                varslval.sptr = varstext + 1;
                return STR_TOKEN;
            }

{CTRL}      {
                print( FATAL, "Flow control structures can't be used "
                       "in the VARIABLES section.\n" );
                THROW( EXCEPTION );
            }

            /* Handling of function, variable and array identifiers */
{FUNC}      {
                char *t;
                int acc;

                t = strrchr( varstext, '#' );
                if ( t != NULL && ! strcmp( t, "#1" ) )
                    *t = '\0';

                varslval.vptr = func_get( varstext, &acc );
                if ( varslval.vptr != NULL )
                {
                    /* If it's a function check that the function can be used
                       in the current context */

                    if ( acc == ACCESS_EXP )
                    {
                        print( FATAL, "Function '%s' can only be used "
                               "in the EXPERIMENT section.\n", varstext );
                        THROW( EXCEPTION );
                    }
                    return FUNC_TOKEN;
                }
                else
                {
                    print( FATAL, "Function '%s' is not loaded.\n", varstext );
                    THROW( EXCEPTION );
                }
            }

{IDENT}     {
                int acc;

                /* First check if the identifier is a function name */

                varslval.vptr = func_get( varstext, &acc );
                if ( varslval.vptr != NULL )
                {
                    /* If it's a function check that the function can be used
                       in the current context */

                    if ( acc == ACCESS_EXP )
                    {
                        print( FATAL, "Function '%s' can only be used "
                               "in the EXPERIMENT section.\n", varstext );
                        THROW( EXCEPTION );
                    }
                    return FUNC_TOKEN;
                }

                /* If it's not a function it's got to be a variable */

                if ( ( varslval.vptr = vars_get( varstext ) ) == NULL )
                    varslval.vptr = vars_new( varstext );
                return VAR_TOKEN;
            }

            /* Handling of end of statement character */
";"         return ';';

{WS}        /* Skip white space */

            /* Handling of invalid input */
.           {
                print( FATAL, "Invalid input near '%s'.\n", varstext );
                THROW( EXCEPTION );
            }

            /* Handling of end of file */
<<EOF>>     {
                Vars_next_section = NO_SECTION;
                return 0;
            }


        /*----------------------*/
%%      /*     End of Rules     */
        /*----------------------*/


/*---------------------------------------------------------------*
 * variables_parser() first checks that it wasn't called before,
 * initializes a few variables and then starts the lexer/parser
 * combination. If this works out ok, either the number of the
 * next section or, on end of file, NO_SECTION is stored in the
 * global variable 'Vars_next_section', which is returned to the
 * calling section_parser(). Otherwise, some special exceptions
 * are caught here (resulting in FAIL return state) or, for the
 * more comman exceptions, are caught in section_parser() (in
 * this case, an OK state is returned).
 *---------------------------------------------------------------*/

int
variables_parser( FILE * in )
{
    /* Don't allow more than one VARIABLES section */

    if ( EDL.compilation.sections[ VARIABLES_SECTION ] )
    {
        print( FATAL, "Multiple instances of VARIABLES section label.\n" );
        THROW( EXCEPTION );
    }

    EDL.compilation.sections[ VARIABLES_SECTION ] = true;

    varsin = in;
    varsparser_init( );
    varsparse( );
    varslex_destroy( );

    return Vars_next_section;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
