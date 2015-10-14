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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noinput nounput noyy_top_state never-interactive

%{

#include "fsc2.h"

static size_t fll_count_functions( Func_T * fncts,
                                   size_t   num_predef_func );
static int ffl_func_cmp2( const void * a,
                          const void * b );
static void fll_get_functions( Func_T * fncts,
                               size_t   num_predef_func );

static long Comm_lc;

#define INT_TOKEN    256
#define ALL_TOKEN    257
#define EXP_TOKEN    258
#define PREP_TOKEN   259
#define IDENT_TOKEN  260


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int func_listget_lineno( void );
FILE * func_listget_in( void );
FILE * func_listget_out( void );
int func_listget_leng( void );
char * func_listget_text( void );
void func_listset_lineno( int );
void func_listset_in( FILE * );
void func_listset_out( FILE * );
int func_listget_debug( void );
void func_listset_debug( int );
int func_listlex_destroy( void );
#endif


%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

INT      ([+-]?[0-9]+)|"-"

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

IDENT    [A-Za-z][A-Za-z_0-9]*

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      EDL.Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      EDL.Lc++;

            /* handling of C style comment */
{REM3}      {
                Comm_lc = EDL.Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by a '/' */

            /* end of line character in a comment */
\n          EDL.Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     {
                eprint( FATAL, false, "%s: End of file in comment starting at "
                        "line %ld.\n", EDL.Fname, Comm_lc );
                THROW( EXCEPTION );
            }

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                EDL.Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     {  /* End of comment without start */
                eprint( FATAL, false, "End of comment found at line %ld in "
                        "function data base '%s'.\n", EDL.Lc, EDL.Fname );
                THROW( EXCEPTION );
            }

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      EDL.Lc++;

{LWS}       /* dumps of leading white space */

            /* dumps trailing white space */
{TWS}       EDL.Lc++;

{WS}        /* dump (other) white space */

            /* writes out EOL character */
\n          EDL.Lc++;

,           /* skip commas */

ALL         return ALL_TOKEN;
EXP         return EXP_TOKEN;
PREP        return PREP_TOKEN;
{INT}       return INT_TOKEN;
{IDENT}     return IDENT_TOKEN;
;           return ';';

.           {
                eprint( FATAL, false, "Syntax error at line %ld in function "
                               "data base '%s'.\n", EDL.Lc, EDL.Fname );
                THROW( EXCEPTION );
            }

<<EOF>>     return 0;



        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

size_t
func_list_parse( Func_T ** fncts,
                 size_t    num_func )
{
    static bool is_restart = false;
    size_t volatile nf = num_func;
    size_t num_predef_func = num_func;
    size_t num;
    size_t cur;
    struct stat buf;
    char * ld_path;
    char * ld = NULL;
    char * ldc;


    EDL.Fname = T_free( EDL.Fname );

    /* If we're supposed only to use the files from the source directories
       only try to use those */

    if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
    {
        EDL.Fname = T_strdup( confdir "Functions" );
        if ( lstat( EDL.Fname, &buf ) < 0 )
            EDL.Fname = T_free( EDL.Fname );
    }
    else
    {
        /* As usual, we first check paths defined by the environment variable
           'LD_LIBRARY_PATH' and then the compiled-in path (except when this
           is a check run). */

        if ( ( ld_path = getenv( "LD_LIBRARY_PATH" ) ) != NULL )
        {
            ld = T_strdup( ld_path );

            for ( ldc = strtok( ld, ":" ); ldc != NULL;
                  ldc = strtok( NULL, ":" ) )
            {
                EDL.Fname = get_string( "%s%sFunctions", ldc, slash( ldc ) );
                if ( lstat( EDL.Fname, &buf ) == 0 )
                    break;
                EDL.Fname = T_free( EDL.Fname );
            }

            T_free( ld );
        }

        if (    EDL.Fname == NULL
             && ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            EDL.Fname = T_strdup( libdir "Functions" );
            if ( lstat( EDL.Fname, &buf ) < 0 )
                EDL.Fname = T_free( EDL.Fname );
        }
    }

    if ( EDL.Fname == NULL )
    {
        eprint( FATAL, false, "Can't open function data base 'Functions'.\n" );
        THROW( EXCEPTION );
    }

    if ( ( func_listin = fopen( EDL.Fname, "r" ) ) == NULL )
    {
        eprint( FATAL, false, "Can't open function data base '%s'.\n",
                EDL.Fname );
        EDL.Fname = T_free( EDL.Fname );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        func_listrestart( func_listin );
    else
         is_restart = true;

    /* Count the number of functions defined in the file */

    TRY
    {
        num = fll_count_functions( *fncts, num_predef_func );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( func_listin );
        RETHROW;
    }

    /* If no functions were found return immediately */

    if ( num == 0 )
    {
        fclose( func_listin );
        return num_predef_func;
    }

    /* Rewind input file, allocate memory for the additional functions and
       set defaults values */

    rewind( func_listin );
    nf = num + num_predef_func;

    if ( num != 0 )
        *fncts = T_realloc( *fncts, nf * sizeof **fncts );

    for ( cur = num_predef_func; cur < nf; cur++ )
    {
        ( *fncts )[ cur ].name = NULL;
        ( *fncts )[ cur ].fnct = NULL;
        ( *fncts )[ cur ].nargs = 0;
        ( *fncts )[ cur ].access_flag = ACCESS_EXP;
        ( *fncts )[ cur ].device = NULL;
        ( *fncts )[ cur ].to_be_loaded = true;
    }

    /* Now parse the function database file again */

    TRY
    {
        fll_get_functions( *fncts, num_predef_func );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( func_listin );
        RETHROW;
    }

    fclose( func_listin );

    return nf;
}


/*---------------------------------------------------*
 * Function runs once through the function data base
 * file and counts the number of defined functions
 *---------------------------------------------------*/

static size_t
fll_count_functions( Func_T * fncts,
                     size_t   num_predef_func )
{
    int ret_token;
    int last_token = 0;
    size_t num = 0;
    Func_T *f = NULL;


    EDL.Lc = 1;

    while ( ( ret_token = func_listlex( ) ) != 0 )
    {
        /* If the name of a loadable function conflicts with the name of a
           built-in function the loadable function will replace the built-in
           function completely (even if it doesn't get loaded!) */

        if ( ret_token == IDENT_TOKEN )
        {
            if ( ( f = bsearch( func_listtext, fncts, num_predef_func,
                                sizeof *fncts, ffl_func_cmp2 ) )
                 != NULL )
            {
                eprint( WARN, false, "Loadable function '%s()' hides "
                        "built-in function (line %ld in function data "
                        "base '%s').\n", f->name, EDL.Lc, EDL.Fname );

                f->name = T_strdup( f->name );
                f->fnct = NULL;
                f->nargs = 0;
                f->access_flag = ACCESS_EXP;
                f->device = NULL;
                f->to_be_loaded = true;
            }
        }

        /* Increment number of functions only after a semicolon (but not if
           the previous token was also a semicolon) and if the new function
           didn't overload a built-in function */

        if ( ret_token == ';' && last_token != ';' && f == NULL )
           num++;
        last_token = ret_token;
    }

    if ( last_token != ';' )
    {
        eprint( FATAL, false, "Missing semiciolon at end of function data "
                "base '%s'.\n", EDL.Fname );
        THROW( EXCEPTION );
    }

    return num;
}


/*--------------------------------------------------------------------------*
 * Function for comparing function names, used in sorting the function list
 *--------------------------------------------------------------------------*/

static int
ffl_func_cmp2( const void * a,
               const void * b )
{
    return strcmp( ( const char * ) a, ( ( const Func_T * ) b )->name );
}


/*------------------------------------------------------------*
 * Function runs a second time through the function data base
 * and tries to make sense from the entries
 *------------------------------------------------------------*/

static void
fll_get_functions( Func_T * fncts,
                   size_t   num_predef_func )
{
    int ret_token;
    int state;
    size_t cur;
    size_t act;
    size_t i;


    EDL.Lc = 1;
    state = 0;
    act = cur = num_predef_func;

    while ( ( ret_token = func_listlex( ) ) != 0 )
    {
        switch ( ret_token )
        {
            case IDENT_TOKEN :
                if ( state != 0 )
                {
                    eprint( FATAL, false, "Syntax error at line %ld in "
                            "function data base '%s'.\n", EDL.Lc, EDL.Fname );
                    THROW( EXCEPTION );
                }

                /* Allow overloading of built-in functions but don't allow
                   multiple overloading (this got nothing to do with several
                   modules defining the same function, because here we only
                   care about the declarations in 'Functions') */

                for ( i = 0; i < cur; i++ )
                    if ( ! strcmp( fncts[ i ].name, func_listtext ) )
                    {
                        if ( i < num_predef_func )
                            act = i;
                        else
                        {
                            eprint( FATAL, false, "Function '%s()' is declared"
                                    "more than once in function data base "
                                    "'%s'.\n", func_listtext, EDL.Fname );
                            THROW( EXCEPTION );
                        }
                        break;
                    }

                if ( act >= num_predef_func )
                    fncts[ act ].name = T_strdup( func_listtext );

                state = 1;
                break;

            case INT_TOKEN :
                if ( state != 1 )
                {
                    eprint( FATAL, false, "Syntax error at line %ld of "
                            "function data base '%s'.\n", EDL.Lc, EDL.Fname );
                    THROW( EXCEPTION );
                }

                if (    ! strcmp( func_listtext, "-" )
                     || ( fncts[ act ].nargs = atol( func_listtext ) ) < 0 )
                    fncts[ act ].nargs = INT_MIN;

                state = 2;
                break;

            case ALL_TOKEN : case EXP_TOKEN : case PREP_TOKEN :
                if ( state != 2 )
                {
                    eprint( FATAL, false, "Syntax error at line %ld of "
                            "function data base '%s'.\n", EDL.Lc, EDL.Fname );
                    THROW( EXCEPTION );
                }

                switch ( ret_token )
                {
                    case ALL_TOKEN :
                        fncts[ act ].access_flag = ACCESS_ALL;
                        break;

                    case EXP_TOKEN :
                        fncts[ act ].access_flag = ACCESS_EXP;
                        break;

                    case PREP_TOKEN :
                        fncts[ act ].access_flag = ACCESS_PREP;
                        break;
                }

                state = 3;
                break;

            case ';' :
                if ( state != 0 && state < 2 )
                {
                    eprint( FATAL, false, "Missing number of arguments for "
                            "function '%s' at line %ld in function data base "
                            "'%s'.\n", fncts[ act ].name, EDL.Lc, EDL.Fname );
                    THROW( EXCEPTION );
                }

                if ( state == 0 )   /* if there was a superfluous semicolon */
                    break;          /* in the input file */

                state = 0;
                if ( act == cur )   /* "new" function ? */
                    act = ++cur;
                else                /* built-in function has been overloaded */
                    act = cur;
                break;

            default:                             /* this can never happen... */
                fsc2_impossible( );
        }
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
