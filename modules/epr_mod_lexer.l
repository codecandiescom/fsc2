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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noyy_top_state nounput noinput never-interactive case-insensitive

%{

#include "epr_mod.h"


static long epr_mod_Lc,
            epr_mod_Comm_Lc;

static int ival;
static double dval;
static char *sval;

/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int epr_mod_get_lineno( void );
FILE * epr_mod_get_in( void );
FILE * epr_mod_get_out( void );
int epr_mod_get_leng( void );
char * epr_mod_get_text( void );
void epr_mod_set_lineno( int );
void epr_mod_set_in( FILE * );
void epr_mod_set_out( FILE * );
int epr_mod_get_debug( void );
void epr_mod_set_debug( int );
#endif


typedef struct FE_T  {
    double freq;
    bool   is_freq;
    double ratio;
    bool   is_ratio;
    double phase;
    bool   is_phase;
} FE_T;


typedef struct CAL_T {
    char   * name;
    size_t   count;
    FE_T   * fe;
    bool     interpolate;
    bool     is_interpolate;
    bool     extrapolate;
    bool     is_extrapolate;
} CAL_T;


static CAL_T cal;


static int epr_mod_get_new_cal( int tok );
static int epr_mod_cal_data( int tok );
static int epr_mod_get_name( int tok );
static int epr_mod_get_ip( int tok );
static int epr_mod_get_ep( int tok );
static int epr_mod_get_freq( int tok );
static int epr_mod_get_val( int tok );
static int epr_mod_get_ratio( int tok );
static int epr_mod_get_phase( int tok );
static void epr_mod_state_file_err( int print_what );


#define END_TOK    0
#define YN_TOK     1
#define CAL_TOK    2
#define FREQ_TOK   3
#define RATIO_TOK  4
#define PHASE_TOK  5
#define IP_TOK     6
#define EP_TOK     7
#define FUNIT_TOK  8
#define RUNIT_TOK  9
#define NAME_TOK  10
#define NUM_TOK   11

%}

REM1     ^[\t ]*("//"|"#").*\n
REM2     [\t ]*("//"|"#").*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ]*\n
WS       [\t ]+
TWS      [\t ]+\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    [+-]?((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
D        ([+-]?{INT})|{FLOAT}

CAL      "calibration"[\t ]*":"
FREQ     "freq"[\t ]*":"
RATIO    "ratio"[\t ]*":"
PHASE    "phase"[\t ]*":"
IS_IP    "interpolate"[\t ]*":"
IS_EP    "extrapolate"[\t ]*":"
YES      "yes"
NO       "no"
RUNIT    "G"[\t ]*"/"[\t ]*"V"
FUNIT    (G|M|k|m|u|n)?"Hz"
NAME_VAL "\""[^\"]*"\""

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      epr_mod_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      epr_mod_Lc++;

            /* handling of C style comment */
{REM3}      {
                epr_mod_Comm_Lc = epr_mod_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          epr_mod_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     {
                print( FATAL, "End of state file '%s' detected within a "
                       "comment.\n", epr_mod.state_file );
                epr_mod_state_file_err( 0 );
            }

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                epr_mod_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     {
                print( FATAL, "End of comment indicator (\"*/\") without "
                       "previous start of comment in state file '%s' "
                       "at line %ld.\n", epr_mod.state_file, epr_mod_Lc );
                epr_mod_state_file_err( 0 );
            }

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      epr_mod_Lc++;

            /* dumps trailing white space */
{TWS}       epr_mod_Lc++;

            /* dump all other forms of white space */
{WS}

            /* count lines */
\n          epr_mod_Lc++;

{YES}       {
                ival = 1;
                return YN_TOK;
            }

{NO}        {
                ival = 0;
                return YN_TOK;
            }

{CAL}       return CAL_TOK;

{RATIO}     return RATIO_TOK;

{FREQ}      return FREQ_TOK;

{PHASE}     return PHASE_TOK;

{IS_IP}     return IP_TOK;

{IS_EP}     return EP_TOK;

{RUNIT}     return RUNIT_TOK;

{FUNIT}     {
                char *u = epr_mod_text;

                while ( isspace( ( unsigned char ) *u ) )
                    u++;

                switch ( *u )
                {
                    case 'G' :
                        dval = 1.0e9;
                        break;

                    case 'M' :
                        dval = 1.0e6;
                        break;

                    case 'k' :
                        dval = 1.0e3;
                        break;

                    case 'H' :
                        dval = 1.0;
                        break;

                    case 'm' :
                        dval = 1.0e-3;
                        break;

                    case 'u' :
                        dval = 1.0e-6;
                        break;

                    case 'n' :
                        dval = 1.0e-9;
                        break;
                }

                return FUNIT_TOK;
            }

{D}         {
                TRY
                {
                    dval = T_atod( epr_mod_text );
                    TRY_SUCCESS;
                }
                CATCH( EXCEPTION )
                    epr_mod_state_file_err( 0 );
                return NUM_TOK;
            }

{NAME_VAL}  {
                epr_mod_text[ epr_mod_leng - 1 ] = '\0';
                sval = epr_mod_text + 1;
                return NAME_TOK;
            }

<<EOF>>     return END_TOK;


        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
epr_mod_read_state( void )
{
    static bool is_restart = UNSET;
    int tok;


    /* Try to open the configuration file */

    if ( epr_mod.state_file == NULL )
    {
        if ( EPR_MOD_STATE_FILE[ 0 ] ==  '/' )
            epr_mod.state_file = T_strdup( EPR_MOD_STATE_FILE );
        else
            epr_mod.state_file = get_string( "%s%s%s", libdir, slash( libdir ),
                                             EPR_MOD_STATE_FILE );
    }

    raise_permissions( );

    if ( ( epr_mod_in = fopen( epr_mod.state_file, "r" ) ) == NULL )
    {
        /* It's ok if the file does not yet exist */

        if ( errno == ENOENT )
        {
            lower_permissions( );
            return;
        }

        if ( errno == EACCES )
            print( FATAL, "No read permission for state file '%s'.\n",
                   epr_mod.state_file );
        else
            print( FATAL, "Can't open state file '%s'.\n", epr_mod.state_file );

        lower_permissions( );
        epr_mod.state_file = T_free( epr_mod.state_file );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        epr_mod_restart( epr_mod_in );
    else
        is_restart = SET;

    epr_mod_Lc = 1;

    cal. name = NULL;
    cal.count = 0;
    cal.fe = NULL;
    cal.interpolate = SET;
    cal.is_interpolate = UNSET;
    cal.extrapolate = UNSET;
    cal.is_extrapolate = UNSET;

    tok = epr_mod_lex( );

    while ( tok != END_TOK )
        tok = epr_mod_get_new_cal( tok );

    fclose( epr_mod_in );

    lower_permissions( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_new_cal( int tok )
{
    size_t i;
    size_t last;


    CLOBBER_PROTECT( tok );

    if ( tok != CAL_TOK )
        epr_mod_state_file_err( 1 );

    tok = epr_mod_lex( );

    if ( tok == CAL_TOK || tok == END_TOK )
        epr_mod_state_file_err( 0 );

    while ( tok != CAL_TOK && tok != END_TOK )
        tok = epr_mod_cal_data( tok );

    TRY
    {
        last = epr_mod.count;
        epr_mod.calibrations =
            T_realloc( epr_mod.calibrations,
                       ++epr_mod.count * sizeof *epr_mod.calibrations );
        epr_mod.calibrations[ last ].name = cal.name;
        cal.name = NULL;
        epr_mod.calibrations[ last ].interpolate = cal.interpolate;
        epr_mod.calibrations[ last ].extrapolate = cal.extrapolate;
        epr_mod.calibrations[ last ].fe = NULL;
        epr_mod.calibrations[ last ].count = cal.count;

        if ( cal.count > 0 )
        {
            epr_mod.calibrations[ last ].fe =
                T_malloc(   cal.count
                          * sizeof *epr_mod.calibrations[ 0 ].fe );

            for ( i = 0; i < cal.count; i++ )
            {
                epr_mod.calibrations[ last ].fe[ i ].freq  = cal.fe[ i ].freq;
                epr_mod.calibrations[ last ].fe[ i ].ratio = cal.fe[ i ].ratio;
                epr_mod.calibrations[ last ].fe[ i ].phase = cal.fe[ i ].phase;
                epr_mod.calibrations[ last ].fe[ i ].is_phase =
                                                          cal.fe[ i ].is_phase;
            }

            cal.fe = T_free( cal.fe );
            cal.count = 0;
        }

        if ( epr_mod.calibrations[ last ].count > 2 )
            epr_mod_recalc( epr_mod.calibrations  + last );
        else
            qsort( epr_mod.calibrations[ last ].fe,
                   epr_mod.calibrations[ last ].count,
                   sizeof *epr_mod.calibrations[ last ].fe, epr_mod_comp );

        TRY_SUCCESS;
    }
    CATCH ( OUT_OF_MEMORY_EXCEPTION )
        epr_mod_state_file_err( 0 );

    cal.is_interpolate = 0;
    cal.is_extrapolate = 0;

    return tok;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_cal_data( int tok )
{
    while ( tok != CAL_TOK && tok != END_TOK )
        switch ( tok )
        {
            case NAME_TOK :
                tok = epr_mod_get_name( tok );
                break;

            case IP_TOK :
                tok = epr_mod_get_ip( tok );
                break;

            case EP_TOK :
                tok = epr_mod_get_ep( tok );
                break;

            case FREQ_TOK :
                tok = epr_mod_get_freq( tok );
                break;

            default :
                epr_mod_state_file_err( 1 );
        }

    if ( cal.name == NULL )
    {
        print( FATAL, "Missing name for entry detected in state file '%s' at "
               "line %d.\n", epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    return tok;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_name( int tok )
{
    char *name = NULL;


    CLOBBER_PROTECT( name );

    if ( tok != NAME_TOK )
        epr_mod_state_file_err( 1 );

    TRY
    {
        name = T_strdup( sval );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
        epr_mod_state_file_err( 0 );

    if ( *name == '\0' )
    {
        print( FATAL, "Empty name for entry in state file '%s' at "
               "line %d.\n", epr_mod.state_file, epr_mod_Lc );
        T_free( name );
        epr_mod_state_file_err( 0 );
    }

    if ( cal.name == NULL )
        cal.name = name;
    else
    {
        print( SEVERE, "Calibration name defined more than once in state file "
               "'%s' at line %d, keeping first definition.\n",
               epr_mod.state_file, epr_mod_Lc );
        T_free( name );
    }

    return epr_mod_lex( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_ip( int tok )
{
    if ( tok != IP_TOK )
        epr_mod_state_file_err( 3 );

    if ( cal.is_interpolate )
        print( SEVERE, "Interpolation setting found more than once in state "
               "file '%s' at line %d, keeping first definition.\n",
               epr_mod.state_file, epr_mod_Lc );

    if ( ( tok = epr_mod_lex( ) ) != YN_TOK )
        epr_mod_state_file_err( 1 );

    if ( ! cal.is_interpolate )
    {
        cal.is_interpolate = SET;
        cal.interpolate    = ival;
    }

    return epr_mod_lex( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_ep( int tok )
{
    if ( tok != EP_TOK )
        epr_mod_state_file_err( 3 );

    if ( cal.is_extrapolate )
        print( SEVERE, "Extrapolation setting found more than once in state "
               "file '%s' at line %d, keeping first definition.\n",
               epr_mod.state_file, epr_mod_Lc );

    if ( ( tok = epr_mod_lex( ) ) != YN_TOK )
        epr_mod_state_file_err( 1 );

    if ( ! cal.is_extrapolate )
    {
        cal.is_extrapolate = SET;
        cal.extrapolate    = ival;
    }

    return epr_mod_lex( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_freq( int tok )
{
    size_t last = cal.count;

    if ( tok != FREQ_TOK )
        epr_mod_state_file_err( 3 );

    TRY
    {
        cal.fe = T_realloc( cal.fe, ++cal.count * sizeof *cal.fe );
        cal.fe[ last ].is_freq = UNSET;
        cal.fe[ last ].is_ratio = UNSET;
        cal.fe[ last ].is_phase = UNSET;
        TRY_SUCCESS;
    }
    CATCH ( OUT_OF_MEMORY_EXCEPTION )
        epr_mod_state_file_err( 0 );

    tok = epr_mod_lex( );

    while ( 1 )
        switch ( tok )
        {
            case NUM_TOK :
                tok = epr_mod_get_val( tok );
                break;

            case RATIO_TOK :
                tok = epr_mod_get_ratio( tok );
                break;

            case PHASE_TOK :
                tok = epr_mod_get_phase( tok );
                break;

            default :
                if (    ! cal.fe[ last ].is_freq
                     || ! cal.fe[ last ].is_ratio )
                {
                    print( FATAL, "Incomplete data for entry in state file "
                           "'%s' at line %d detected.\n",
                           epr_mod.state_file, epr_mod_Lc );
                    epr_mod_state_file_err( 0 );
                }
                return tok;
        }

    fsc2_impossible( );
    return tok;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_val( int tok )
{
    size_t last = cal.count - 1;
    size_t i;


    if ( tok != NUM_TOK )
        epr_mod_state_file_err( 3 );

    if ( cal.fe[ last ].is_freq )
    {
        print( FATAL, "Frequency value defined more than once in state file "
               "'%s' at line %d.\n", epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    if ( dval <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative frequency found in state file "
               "'%s' at line %d.\n", epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    cal.fe[ last ].freq    = dval;
    cal.fe[ last ].is_freq = SET;

    if ( ( tok = epr_mod_lex( ) ) == FUNIT_TOK )
    {
        cal.fe[ last ].freq *= dval;
        tok = epr_mod_lex( );
    }
    else
    {
        print( FATAL, "Missing frequency unit in state file '%s' at line %d.\n",
               epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    for ( i = 0; i < last; i++ )
    {
        if  ( cal.fe[ i ].freq == cal.fe[ last ].freq )
        {
            print( FATAL, "Frequencies of %d. and %d. entry for calibration "
                   "'%s' are identical.\n", ( int ) i + 1, ( int ) last + 1,
                   cal.name );
            epr_mod_state_file_err( 0 );
        }

        if (    fabs( cal.fe[ i ].freq - cal.fe[ last ].freq )
                    / cal.fe[ i ].freq < 1.0e-2 * EPR_MOD_MIN_DEVIATION
             || fabs( cal.fe[ i ].freq - cal.fe[ last ].freq )
                    / cal.fe[ last ].freq < 1.0e-2 * EPR_MOD_MIN_DEVIATION )
        {
            print( FATAL, "Deviation between frequencies for %d. and %d. "
                   "entry for calibration '%s' smaller than minimum deviation "
                   "of %d%%.\n", ( int ) i + 1, ( int ) last + 1,
                   ( int ) EPR_MOD_MIN_DEVIATION );
            epr_mod_state_file_err( 0 );
        }
    }

    return tok;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_ratio( int tok )
{
    int last = cal.count - 1;


    if ( cal.fe[ last ].is_ratio )
    {
        print( FATAL, "Ratio defined more than once in state file "
               "'%s' at line %d.\n", epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    if ( ( tok = epr_mod_lex( ) ) != NUM_TOK )
        epr_mod_state_file_err( 1 );

    cal.fe[ last ].ratio = dval;
    cal.fe[ last ].is_ratio = SET;

    /* Allow an optional "G/V" after the number */

    if ( ( tok = epr_mod_lex( ) ) != RUNIT_TOK )
        return tok;

    return epr_mod_lex( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
epr_mod_get_phase( int tok )
{
    int last = cal.count - 1;


    if ( cal.fe[ last ].is_phase )
    {
        print( FATAL, "Phase defined more than once in state file "
               "'%s' at line %d.\n", epr_mod.state_file, epr_mod_Lc );
        epr_mod_state_file_err( 0 );
    }

    if ( ( tok = epr_mod_lex( ) ) != NUM_TOK )
        epr_mod_state_file_err( 1 );

    cal.fe[ last ].phase = dval;
    cal.fe[ last ].is_phase = SET;

    while ( cal.fe[ last ].phase > 360.0 )
        cal.fe[ last ].phase -= 360.0;

    while ( cal.fe[ last ].phase < -360.0 )
        cal.fe[ last ].phase += 360.0;

    return epr_mod_lex( );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static void
epr_mod_state_file_err( int print_what )
{
    fclose( epr_mod_in );
    lower_permissions( );

    switch ( print_what )
    {
        case 1 :
            print( FATAL, "Unexpected input in state file '%s' at line %d.\n",
                   epr_mod.state_file, epr_mod_Lc, epr_mod_text );
            break;

        case 2 :
            print( FATAL, "State file '%s' ends prematurely.\n",
                   epr_mod.state_file );
            break;

        case 3 :
            print( FATAL, "Internal error detected while reading state file "
                   "'%s' at line %d.\n", epr_mod.state_file, epr_mod_Lc );
            break;
    }

    if ( cal.name )
        T_free( cal.name );
    if ( cal.fe )
        T_free( cal.fe );

    epr_mod_clear( &epr_mod );

    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
