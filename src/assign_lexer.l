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
/* This is the lexer for the ASSIGNMENTS section of an EDL file */
/****************************************************************/


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap noinput nounput never-interactive


%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                         \
{                                                                 \
    char c;                                                       \
    ssize_t count;                                                \
    while (    ( count = read( fileno( assignin ), &c, 1 ) ) < 0  \
            && errno == EINTR )                                   \
        /* empty */ ;                                             \
    result = ( count <= 0 ) ? YY_NULL : ( buf[ 0 ] = c, 1 );      \
}


#include "fsc2.h"
#include "assign_parser.h"


extern void assignparser_init( void );
extern void assignparse( void );


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int assignget_lineno( void );
FILE * assignget_in( void );
FILE * assignget_out( void );
int assignget_leng( void );
char * assignget_text( void );
void assignset_lineno( int );
void assignset_in( FILE * );
void assignset_out( FILE * );
int assignget_debug( void );
void assignset_debug( int );
int assignlex_destroy( void );
#endif


/* locally used global variables */

static int Assign_next_section;
static bool In_phs = UNSET;

/* locally used functions */

static void set_cur_pulser( void );

%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[ \t]*DEV(ICE)?S?:
ASS         ^[ \t]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

INT         [0-9]+
EXPO        [Ee][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

F           F(UNC(TION)?)?
S           S(TART)?
L           L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?

STR         \x5[^\x6]*\x6
ESTR        \x5.*\x3\n.*\n

IDENT       [A-Za-z]+[A-Za-z0-9_]*

MW          M(ICRO)?_?W(AVE)?(#{INT})?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?(#{INT})?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?(#{INT})?:?
DET         DET(ECTION)?(#{INT})?:?
DET_GATE    DET(ECTION)?_?G(ATE)?(#{INT})?:?
DEF         DEF(ENSE)?(#{INT})?:?
RF          R(ADIO)?_?F(REQ(UENCY)?)?(#{INT})?:?
RF_GATE     R(ADIO)?_?F(REQ(UENCY)?)?_?G(ATE)?(#{INT})?:?
LSR         L(ASER)?(#{INT})?:?
PSH         P(ULSE)?_?SH(APE(R)?)?(#{INT})?:?
OI          O(THER)?(_?1)?(#{INT})?:?
OII         O(THER)?_?2?(#{INT})?:?
OIII        O(THER)?_?3?(#{INT})?:?
OIV         O(THER)?_?4?(#{INT})?:?
PH1         PH(ASE)?(_?1)?(#{INT})?:?
PH2         PH(ASE)?_?2(#{INT})?:?

PHS1        PH(ASE)?_S(ETUP)?(_?1)?(#{INT})?:?
PHS2        PH(ASE)?_S(ETUP)?_?2(#{INT})?:?

PX          "+"?[xX]:?
PY          "+"?[yY]:?
PMX         "-"[xX]:?
PMY         "-"[yY]:?
ON          ON
OFF         OFF

DEL         ((DEL)|(DELAY)):?
PODSPEC     P([0-9]|(1[01]))
POD         P(OD)?
POD1        POD_?1
POD2        POD_?2
CH          CH(ANNEL)?
INV         INV(ERT(ED)?)?
VH          V_?H(IGH)?
VL          V_?L(OW)?
CHN         ([A-H]([0-9]|(1[0-5])))|(CH([0-9]|(1[0-5])))|(TRIG(GER)?_?OUT)


TB          T(IME)?_?B(ASE)?(#{INT})?:?
TM          T(RIG(GER)?)?_?M(ODE)?(#{INT})?:?

INTERN      INT(ERN(AL)?)?
EXTERN      EXT(ERN(AL)?)?
SL          SL(OPE)?
NEG         NEG(ATIVE)?
POS         POS(ITIVE)?
THRESH      LEV(EL)?
IMP         IMP(EDANCE)?
LOW         L(OW)?
HIGH        H(IGH)?
REPT        REP(EAT)?_?T(IME)?
REPF        REP(EAT)?_?F(REQ(UENCY)?)?

CTRL        (WHILE)|(BREAK)|(NEXT)|(IF)|(ELSE)|(REPEAT)|(FOR)

/* The next five keywords are deprecated */

PSD1        PH(ASE)?_?S(W(ITCH)?)?_?D(EL(AY)?)?(_?1)?(#{INT})?:?
PSD2        PH(ASE)?_?S(W(ITCH)?)?_?D(EL(AY)?)?_?2(#{INT})?:?
GP          G(RACE)?_?P(ERIOD)?(#{INT})?:?
MPL         MAX(IMUM)?_?PATT(ERN)?_LEN(GTH)(#{INT})?
KAP         KEEP_?ALL_?PULSES(#{INT})?:?

WS          [\n=: ]+



        /*---------------*/
%%      /*     Rules     */
        /*---------------*/


            /* handling of file name lines */
{FILE}      {
                *( assigntext + assignleng - 1 ) = '\0';
                EDL.Fname = T_free( EDL.Fname );
                EDL.Fname = T_strdup( assigntext + 2 );
            }

            /* handling of line number lines */
{LNUM}      {
                *( assigntext + assignleng - 1 ) = '\0';
                EDL.Lc = T_atol( assigntext + 2 );
            }

            /* handling of error messages from the cleaner */
{ERR}       {
                eprint( FATAL, UNSET, "%s\n", assigntext + 2 );
                THROW( EXCEPTION );
            }

{ESTR}      {
                assigntext = strchr( assigntext, '\x03' );
                eprint( FATAL, UNSET, "%s\n", assigntext + 2 );
                THROW( EXCEPTION );
            }

            /* handling of DEVICES: labels */
{DEV}       {
                Assign_next_section = DEVICES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of ASSIGNMENTS: labels */
{ASS}       {
                Assign_next_section = ASSIGNMENTS_SECTION;
                return SECTION_LABEL;
            }

            /* handling of VARIABLES: labels */
{VAR}       {
                Assign_next_section = VARIABLES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of PHASES: labels */
{PHAS}      {
                Assign_next_section = PHASES_SECTION;
                return SECTION_LABEL;
            }

            /* handling of PREPARATIONS: labels */
{PREP}      {
                Assign_next_section = PREPARATIONS_SECTION;
                return SECTION_LABEL;
            }

            /* handling of EXPERIMENT: labels */
{EXP}       {
                Assign_next_section = EXPERIMENT_SECTION;
                return SECTION_LABEL;
            }

{PHS1}      {
                set_cur_pulser( );
                In_phs = SET;
                assignlval.lval = 0;
                return PHS_TOK;
            }
{PHS2}      {
                set_cur_pulser( );
                In_phs = SET;
                assignlval.lval = 1;
                return PHS_TOK;
            }

{PX}        {
                assignlval.lval = PHASE_PLUS_X;
                return PXY_TOK;
            }
{PY}        {
                assignlval.lval = PHASE_PLUS_Y;
                return PXY_TOK;
            }
{PMX}       {
                assignlval.lval = PHASE_MINUS_X;
                return PXY_TOK;
            }
{PMY}       {
                assignlval.lval = PHASE_MINUS_Y;
                return PXY_TOK;
            }

{POD1}      return POD1_TOK;
{POD2}      return POD2_TOK;

{ON}        return ON_TOK;
{OFF}       return OFF_TOK;

{MW}        {
                set_cur_pulser( );
                return MW_TOKEN;
            }

{TWT}       {
                set_cur_pulser( );
                return TWT_TOKEN;
            }

{TWT_GATE}  {
                set_cur_pulser( );
                return TWT_GATE_TOKEN;
            }

{DET}       {
                set_cur_pulser( );
                return DET_TOKEN;
            }

{DET_GATE}  {
                set_cur_pulser( );
                return DET_GATE_TOKEN;
            }

{DEF}       {
                set_cur_pulser( );
                return DEF_TOKEN;
            }

{RF}        {
                set_cur_pulser( );
                return RF_TOKEN;
            }

{RF_GATE}   {
                set_cur_pulser( );
                return RF_GATE_TOKEN;
            }

{LSR}       {
                set_cur_pulser( );
                return PSH_TOKEN;
            }

{PSH}       {
                set_cur_pulser( );
                return PSH_TOKEN;
            }

{PH1}       {
                set_cur_pulser( );
                return PH1_TOKEN;
            }

{PH2}       {
                set_cur_pulser( );
                return PH2_TOKEN;
            }

{OI}        {
                set_cur_pulser( );
                return OI_TOKEN;
            }

{OII}       {
                set_cur_pulser( );
                return OII_TOKEN;
            }

{OIII}      {
                set_cur_pulser( );
                return OIII_TOKEN;
            }

{OIV}       {
                set_cur_pulser( );
                return OIV_TOKEN;
            }

{PODSPEC}  {
                assignlval.lval = T_atol( assigntext + 1 );
                return INT_TOKEN;
            }

{CHN}       {
                Var_T *v = convert_to_channel_number( assigntext );
                assignlval.lval = p_ch2num( v->val.lval );
                vars_pop( v );
                return INT_TOKEN;
            }

"TTL"       return TTL_TOKEN;
"ECL"       return ECL_TOKEN;

{POD}       return POD_TOKEN;

{DEL}       return DEL_TOKEN;

{CH}        return CH_TOKEN;

{INV}       return INV_TOKEN;

{VH}        return VH_TOKEN;

{VL}        return VL_TOKEN;

            /* keywords related to time base and trigger mode */

{TB}        {
                set_cur_pulser( );
                return TB_TOKEN;
            }

{TM}        {
                set_cur_pulser( );
                return TM_TOKEN;
            }

{MPL}       {
                set_cur_pulser( );
                return MPL_TOKEN;
            }

{INTERN}    return INTERN_TOKEN;
{EXTERN}    return EXTERN_TOKEN;
{SL}        return SLOPE_TOKEN;
{NEG}       return NEG_TOKEN;
{POS}       return POS_TOKEN;
{THRESH}    return THRESH_TOKEN;
{IMP}       return IMP_TOKEN;
{KAP}       return KAP_TOKEN;
{LOW}       {
                assignlval.lval = LOW;
                return HL_TOKEN;
            }
{HIGH}      {
                assignlval.lval = HIGH;
                return HL_TOKEN;
            }
{REPT}      return REPT_TOKEN;
{REPF}      return REPF_TOKEN;

{GP}        {
                set_cur_pulser( );
                return GP_TOKEN;
            }

{PSD1}      {
                assignlval.lval = 0;
                return PSD_TOKEN;
            }

{PSD2}      {
                assignlval.lval = 1;
                return PSD_TOKEN;
            }

{INT}/[^_a-zA-Z] {
                assignlval.lval = T_atol( assigntext );
                return INT_TOKEN;
            }

{FLOAT}/[^_a-zA-Z] {
                assignlval.dval = T_atod( assigntext );
                return FLOAT_TOKEN;
            }

            /* handling of string constants (to be used as format strings in
               the print() function only */
{STR}       {
                assigntext[ strlen( assigntext ) - 1 ] = '\0';
                assignlval.sptr = assigntext + 1;
                return STR_TOKEN;
            }

{CTRL}      {
                print( FATAL, "Flow control structures can't be used in the "
                       "ASSIGNMENTS section.\n" );
                THROW( EXCEPTION );
            }

            /* handling of function, variable and array identifiers */
{IDENT}     {
                int acc;

                /* first check if the identifier is a function name */

                assignlval.vptr = func_get( assigntext, &acc );
                if ( assignlval.vptr != NULL )
                {
                    /* if it's a function check that the function can be used
                       in the current context */

                    if ( acc == ACCESS_EXP )
                    {
                        print( FATAL, "Function '%s' can only be used in the "
                               "EXPERIMENT section.\n", assigntext );
                        THROW( EXCEPTION );
                    }
                    return FUNC_TOKEN;
                }

                /* if it's not a function it should be a variable */

                if ( ( assignlval.vptr = vars_get( assigntext ) ) == NULL )
                {
                    print(  FATAL, "Variable or function '%s' has not been "
                            "declared or loaded.\n", assigntext );
                     THROW( EXCEPTION );
                }

                return VAR_TOKEN;
            }

            /* stuff used with functions, arrays and math */

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
"?"         return '?';
":"         return ':';

{WS}        /* skip prettifying characters */

";"         {
                In_phs = UNSET;
                return ';';
            }

            /* handling of invalid input (i.e. everything else) */
.           {
                print( FATAL, "Invalid input near '%s'.\n",
                       assigntext );
                THROW( EXCEPTION );
            }

<<EOF>>     {
                Assign_next_section = NO_SECTION;
                return 0;
            }


        /*----------------------*/
%%      /*     End of Rules     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

int
assignments_parser( FILE * in )
{
    static bool needs_restart = UNSET;


    if ( EDL.compilation.sections[ ASSIGNMENTS_SECTION ] )
    {
        print( FATAL, "Multiple instances of ASSIGNMENTS section label.\n" );
        THROW( EXCEPTION );
    }
    EDL.compilation.sections[ ASSIGNMENTS_SECTION ] = SET;

    assignin = in;

    /* Keep the lexer happy... */

    if ( needs_restart )
        assignrestart( assignin );
    else
         needs_restart = SET;

    assignparser_init( );

    Cur_Pulser = 0;

    assignparse( );

    return Assign_next_section;
}


/*-------------------------------------------*
 *-------------------------------------------*/

static void
set_cur_pulser( void )
{
    const char *t;


    if ( In_phs )
        return;

    Cur_Pulser = 0;
    if ( ( t = strrchr( assigntext, '#' ) ) == NULL )
        return;

    if ( ( Cur_Pulser = T_atol( t + 1 ) - 1 ) >= EDL.Num_Pulsers )
    {
        print( FATAL, "There is no pulser #%ld\n", Cur_Pulser + 1 );
        THROW( EXCEPTION );
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
