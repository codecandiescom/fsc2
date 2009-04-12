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

%option noyywrap noinput stack never-interactive

%{

#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/types.h>

#include "fsc2_config.h"


#ifndef __cplusplus

typedef enum
{
    false = 0,
    true  = 1
} bool;

#endif


#define UNSET   false
#define SET     true

#include "exceptions.h"

#if defined FSC2_MDEBUG
#include <mcheck.h>
#endif


char *Prog_Name;


/* Functions and defines we can't do without but which aren't available
   when compiling with -ansi on Linux */

#if defined __STRICT_ANSI__
extern int seteuid( uid_t euid );
extern int setegid( gid_t egid );
extern int vsnprintf( char *       str,
                      size_t       size,
                      const char * format,
                      va_list      ap );
extern int fileno( FILE * stream );
#endif

#define MAX_INCLUDE_DEPTH  16


static void include_handler( char * file );
static void xclose( YY_BUFFER_STATE   primary_buf,
                    YY_BUFFER_STATE * buf_state,
                    FILE *            primary_fp,
                    FILE **           fp,
                    int *             incl_depth );
static void unit_spec( char * text,
                       int    power );
static char *T_strdup( const char * string );
static void *T_malloc( size_t size );
static void *T_realloc( void * ptr,
                        size_t size );
static void *T_realloc_or_free( void * ptr,
                                size_t size );
static void *T_free( void * ptr );
static void lower_permissions( void );
static ssize_t output( const char * fmt,
                       ... );
static void usage( void );


long Lc,
     Comm_Lc,
     Incl_Lc,
     Str_Lc;
char *Fname;
bool Eol;


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int yyget_lineno( void );
FILE * yyget_in( void );
FILE * yyget_out( void );
int yyget_leng( void );
char * yyget_text( void );
void yyset_lineno( int );
void yyset_in( FILE * );
void yyset_out( FILE * );
int yyget_debug( void );
void yyset_debug( int );
int yylex_destroy( void );
#endif


%}

REM1     ^[\t ]*"//".*\n?
REM2     [\t ]*"//".*\n?
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n?

INT      [0-9]+
EXPO     [Ee][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

UNIT     ("m"|"s"|"G"|"V"|"A"|"Hz"|"dBm"|"dB"|"dbm"|"db"|"K")

INANO    {INT}[\t \r]*"n"{UNIT}
IMICRO   {INT}[\t \r]*"u"{UNIT}
IMILLI   {INT}[\t \r]*"m"{UNIT}
INONE    {INT}[\t \r]*({UNIT}|cm^-1)
IKILO    {INT}[\t \r]*"k"{UNIT}
IMEGA    {INT}[\t \r]*"M"{UNIT}
IGIGA    {INT}[\t \r]*"G"{UNIT}

FNANO    {FLOAT}[\t \r]*"n"{UNIT}
FMICRO   {FLOAT}[\t \r]*"u"{UNIT}
FMILLI   {FLOAT}[\t \r]*"m"{UNIT}
FNONE    {FLOAT}[\t \r]*({UNIT}|cm^-1)
FKILO    {FLOAT}[\t \r]*"k"{UNIT}
FMEGA    {FLOAT}[\t \r]*"M"{UNIT}
FGIGA    {FLOAT}[\t \r]*"G"{UNIT}

INTs     {INT}[\t \r]*"nT"
IUT      {INT}[\t \r]*"uT"
IMT      {INT}[\t \r]*"mT"
IT       {INT}[\t \r]*"T"
IKT      {INT}[\t \r]*"kT"
IMGT     {INT}[\t \r]*"MT"
IGGT     {INT}[\t \r]*"GT"

FNT      {FLOAT}[\t \r]*"nT"
FUT      {FLOAT}[\t \r]*"uT"
FMT      {FLOAT}[\t \r]*"mT"
FT       {FLOAT}[\t \r]*"T"
FKT      {FLOAT}[\t \r]*"kT"
FMGT     {FLOAT}[\t \r]*"MT"
FGGT     {FLOAT}[\t \r]*"GT"


WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

SHEBANG  ^#!.*\n
INCL     ^[ \t]*#INCLUDE[\t ]*
IFN      \"[^"\n]*\"[\t ]*\n?|<[^>\n]*>[\t ]*\n?
IFNE     [^"<\t \n]*\n?
EXIT     ^[ \t]*#EXIT[\t ]*
QUIT     ^[ \t]*#QUIT[\t ]*


THROU    [+-][A-Za-z]+
KEEP     [^\t" \n(\/*),;:=%\^\-\+]+


%x       str
%x       comm
%x       incl


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* Drop characters used for internal purposes */

\001        /* Indicates that next item is a file name */
\002        /* Indicates that next item is a line number */
\003        /* Indicates a fatal error */
\004        /* Indicates a unit */
\005        /* Indicates a start of string */
\006        /* Indicates an end of string */

            /* Handling of string constants */
\"          {
                output( "\x05" );
                Str_Lc = Lc;
                yy_push_state( str );
            }

<str>{

\\\"        output( "\\\"" );


\"          {
                output( "\x06" );
                yy_pop_state( );
            }

\n          {
                Lc++;
                Eol = SET;
            }

[^"\\\n]+   output( "%s", yytext );
"\\"        output( "%s", yytext );

<<EOF>>     THROW( EOF_IN_STRING_EXCEPTION );

} /* end of <str> */

{SHEBANG}   {
                if ( Lc == 1 )
                {
                    Lc++;
                    Eol = SET;
                }
                else
                    THROW( MISPLACED_SHEBANG );
            }

            /* Handling of C++ style comment spanning a whole line */
{REM1}      {
                Lc++;
                Eol = SET;
            }

            /* Handling of C++ style comment not spanning a whole line */
{REM2}      {
                Lc++;
                Eol = SET;
                output( "\n" );
            }

            /* Handling of C style comment */
{REM3}      {
                Comm_Lc = Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* Skips everything that's not a '*' */
{REM5}      /* Skips all '*'s not followed by '/'s */

            /* End of line character in a comment */
\n          {
                Lc++;
                Eol = SET;
            }

            /* Handling of EOF within a comment -> fatal error */
<<EOF>>     THROW( EOF_IN_COMMENT_EXCEPTION );

            /* End of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* End of comment and end of line */
{EREM2}     {
                /* On end of comment in #INCLUDE-statement don't dump '\n' */

                if ( yy_top_state( ) == incl )
                   unput( '\n' );

                Eol = SET;
                Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT );

            /* Dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      Lc++;

{LWS}       /* Dumps of leading white space */

            /* Dumps trailing white space */
{TWS}       {
                Lc++;
                Eol = SET;
                output( "\n" );
            }

            /* Reduces any amount of (other) white space to just one space */
{WS}        output( " " );

            /* Writes out EOL character */
\n          {
                Lc++;
                Eol = SET;
                output( "\n" );
            }

            /* "#EXIT" means that rest of current file is to be discarded */
{EXIT}      {
                YY_FLUSH_BUFFER;
                include_handler( NULL );
            }

            /* "#QUIT" means that all following input is to be discarded */
{QUIT}      exit( EXIT_SUCCESS );

            /* Handling of lines starting with "#INCLUDE" */
{INCL}      {
                Incl_Lc = Lc;
                BEGIN( incl );
            }

<incl>{
            /* Handling of C++ style comments */
{REM2}

{WS}        /* Dumps white space */

            /*  Handling of file name */
{IFN}       {
                register char *q = yytext + yyleng - 1,
                              *p = yytext;

                if ( *q == '\n' )
                {
                    Eol = SET;
                    Lc++;
                    --q;
                }
                while ( *q == '\t' || *q == ' ' )
                     --q;
                *++q = '\0';
                BEGIN( INITIAL );
                include_handler( p );
            }

            /* Either start of C style comment or syntax error */
{IFNE}      {
                if ( yytext[ 0 ] == '/' && yytext[ 1 ] == '*' )
                   yy_push_state( comm );
                else
                {
                    output( "\x03\n%s:%ld: '#INCLUDE' expects \"FILENAME\" "
                            "or <FILENAME>.\n", Fname, Incl_Lc );
                    exit( EXIT_FAILURE );
                }
            }
} /* end of <incl> */

            /* Here's handling of numbers with units - they are converted
               to double automatically */

{INANO}/[^a-zA-z0-9_]   unit_spec( yytext, -9 );
{IMICRO}/[^a-zA-z0-9_]  unit_spec( yytext, -6 );
{IMILLI}/[^a-zA-z0-9_]  unit_spec( yytext, -3 );
{INONE}/[^a-zA-z0-9_]   unit_spec( yytext,  0 );
{IKILO}/[^a-zA-z0-9_]   unit_spec( yytext,  3 );
{IMEGA}/[^a-zA-z0-9_]   unit_spec( yytext,  6 );
{IGIGA}/[^a-zA-z0-9_]   unit_spec( yytext,  9 );

{FNANO}/[^a-zA-z0-9_]   unit_spec( yytext, -9 );
{FMICRO}/[^a-zA-z0-9_]  unit_spec( yytext, -6 );
{FMILLI}/[^a-zA-z0-9_]  unit_spec( yytext, -3 );
{FNONE}/[^a-zA-z0-9_]   unit_spec( yytext,  0 );
{FKILO}/[^a-zA-z0-9_]   unit_spec( yytext,  3 );
{FMEGA}/[^a-zA-z0-9_]   unit_spec( yytext,  6 );
{FGIGA}/[^a-zA-z0-9_]   unit_spec( yytext,  9 );

{INTs}/[^a-zA-z0-9_]    unit_spec( yytext, -5 );
{IUT}/[^a-zA-z0-9_]     unit_spec( yytext, -2 );
{IMT}/[^a-zA-z0-9_]     unit_spec( yytext,  1 );
{IT}/[^a-zA-z0-9_]      unit_spec( yytext,  4 );
{IKT}/[^a-zA-z0-9_]     unit_spec( yytext,  7 );
{IMGT}/[^a-zA-z0-9_]    unit_spec( yytext, 10 );
{IGGT}/[^a-zA-z0-9_]    unit_spec( yytext, 13 );
{FNT}/[^a-zA-z0-9_]     unit_spec( yytext, -5 );
{FUT}/[^a-zA-z0-9_]     unit_spec( yytext, -2 );
{FMT}/[^a-zA-z0-9_]     unit_spec( yytext,  1 );
{FT}/[^a-zA-z0-9_]      unit_spec( yytext,  4 );
{FKT}/[^a-zA-z0-9_]     unit_spec( yytext,  7 );
{FMGT}/[^a-zA-z0-9_]    unit_spec( yytext, 10 );
{FGGT}/[^a-zA-z0-9_]    unit_spec( yytext, 13 );

            /* All the rest is simply copied to the output */

{THROU}     {
                if ( Eol )
                {
                    output( "\x02\n%ld\n", Lc );
                    Eol = UNSET;
                }
                output( "%s", yytext );
            }

{KEEP}      {
                if ( Eol )
                {
                    output( "\x02\n%ld\n", Lc );
                    Eol = UNSET;
                }
                output( "%s", yytext );
            }

.           output( "%s", yytext );

<<EOF>>     include_handler( NULL );



        /*----------------------*/
%%      /*     End of rULES     */
        /*----------------------*/


/* The following is for some versions of flex that undefine YY_FLUSH_BUFFER
   immediately after the rules section */

#if ! defined YY_FLUSH_BUFFER
#define YY_FLUSH_BUFFER yy_flush_buffer( YY_CURRENT_BUFFER )
#endif


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

int
main( int    argc,
      char * argv[ ] )
{
    struct passwd *pwe;


#if defined FSC2_MDEBUG
    mcheck( NULL );
#endif

    lower_permissions( );

    Prog_Name = argv[ 0 ];

    if ( argc < 2 )
    {
        yyin = stdin;
        if ( yyin == NULL )
        {
            output( "\x03\nCan't read from stdin.\n" );
            return EXIT_FAILURE;
        }

        output( "\x01\n" );
        Fname = T_strdup( "" );
    }
    else if ( argc == 2 )
    {
        if ( ! strcmp( argv[ 1 ], "-h" ) || ! strcmp( argv[ 1 ], "--help" ) )
        {
            usage( );
            return EXIT_FAILURE;
        }

        yyin = stdin;
        if ( yyin == NULL )
        {
            output( "\x03\nCan't read input file %s.\n", argv[ 1 ] );
            return EXIT_FAILURE;
        }

        /* Analyze name of input file - if it starts with "~/" expand it
           to the name of the home directory of the user. */

        output( "\x01\n%s\n", argv[ 1 ] );

        if ( argv[ 1 ][ 0 ] == '~' && argv[ 1 ][ 1 ] == '/' )
        {
            pwe = getpwuid( getuid( ) );
            Fname = T_malloc( strlen( pwe->pw_dir ) + strlen( argv[ 1 ] ) );
            strcpy( Fname, pwe->pw_dir );
            strcat( Fname, &argv[ 1 ][ 1 ] );
        }
        else
            Fname = T_strdup( argv[ 1 ] );
    }
    else
    {
        usage( );
        return EXIT_FAILURE;
    }

    Lc = 1;
    Eol = SET;

    /* Now try to clean up the input and output appropriate error messages
       if there are problems */

    TRY
    {
        yylex( );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        output( "\x03\n%s:%ld: Running out of memory.\n", Fname, Lc );
        exit( EXIT_FAILURE );
    }
    CATCH( TOO_DEEPLY_NESTED_EXCEPTION )
    {
        output( "\x03\n%s:%ld: Include levels too deeply nested.\n",
                Fname, Lc );
        exit( EXIT_FAILURE );
    }
    CATCH( EOF_IN_COMMENT_EXCEPTION )
    {
        output( "\x03\n%s: End of file in comment starting at line %ld.\n",
                Fname, Comm_Lc );
        exit( EXIT_FAILURE );
    }
    CATCH( EOF_IN_STRING_EXCEPTION )
    {
        output( "\x03\n%s: End of line in string constant starting at "
                "line %ld.\n", Fname, Str_Lc );
        exit( EXIT_FAILURE );
    }
    CATCH( DANGLING_END_OF_COMMENT )
    {
        output( "\x03\n%s: End of comment found in program text at "
                "line %ld.\n", Fname, Lc );
        exit( EXIT_FAILURE );
    }
    CATCH( MISPLACED_SHEBANG )
    {
        output( "\x03\n%s: '#!' must appear on the very first line instead of "
                "line %ld.\n", Fname, Lc );
    }
    OTHERWISE
        exit( EXIT_FAILURE );

    /* Finished succesfully... */

    return EXIT_SUCCESS;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
include_handler( char * file )
{
    static YY_BUFFER_STATE primary_buf;
    static YY_BUFFER_STATE buf_state[ MAX_INCLUDE_DEPTH ];
    static FILE *primary_fp;
    static FILE *fp[ MAX_INCLUDE_DEPTH ];
    static int incl_depth = -1;
    static long lc[ MAX_INCLUDE_DEPTH ];
    static char *fname[ MAX_INCLUDE_DEPTH ];
    static bool eol_state[ MAX_INCLUDE_DEPTH ];
    char *incl_file;
    struct passwd *pwe;


    if ( file != NULL )           /* at the start of an include file */
    {
        /* If all the files the program is prepared to open are already open
           this is a fatal error (most probably it's due to an infinite
           recurrsion) */

        if ( incl_depth + 1 == MAX_INCLUDE_DEPTH )
            THROW( TOO_DEEPLY_NESTED_EXCEPTION );

        /* Now check if file name is enclosed by '"' or '<' and '>' - in the
           later case prepend the default directory */

        if ( file[ 0 ] == '"' )
        {
            *( file + strlen( file ) - 1 ) = '\0';

            /* If the include file name is an absolute path or if we don't
               know where the EDL script we're currently reading (i.e. its
               name does not start with an absolute file name) we take the
               include file name as it is. If it starts with "~/" we replace
               it by the home directory of the user. In all other cases we
               prepend the path of the current file to the include files
               name. */

            if ( file[ 1 ] == '/' || Fname[ 0 ] != '/' )
                incl_file = T_strdup( file + 1 );
            else if ( file[ 1 ] == '~' && file[ 2 ] == '/' )
            {
                pwe = getpwuid( getuid( ) );
                incl_file = T_malloc(   strlen( pwe->pw_dir ) 
                                      + strlen( file ) - 1 );
                strcpy( incl_file, pwe->pw_dir );
                strcat( incl_file, file + 2 );
            }
            else
            {
                incl_file = T_strdup( Fname );
                *strrchr( incl_file, '/' ) = '\0';
                incl_file = T_realloc(   incl_file, strlen( incl_file )
                                       + strlen( file ) + 1 );
                strcat( incl_file, "/" );
                strcat( incl_file, file + 1 );
            }
        }
        else
        {
#if defined DEF_INCL_DIR
            *( file + strlen( file ) - 1 ) = '\0';
            incl_file = T_malloc( strlen( file ) + strlen( DEF_INCL_DIR ) + 2 );
            strcpy( incl_file, DEF_INCL_DIR );
            if ( incl_file[ strlen( incl_file ) - 1 ] != '/' )
                strcat( incl_file, "/" );
            strcat( incl_file, file + 1 );
#else
            output( "\x03\n%s:%ld: No default directory has been compiled "
                    "into fsc2 for EDL include files.\n", Fname, Lc );
            exit( EXIT_FAILURE );
#endif
        }


        /* The same holds for the case that the file can't be opened. */

        if ( ( fp[ incl_depth + 1 ] = fopen( incl_file, "r" ) ) == NULL )
        {
            output( "\x03\n%s:%ld: Can't open include file %s.\n",
                    Fname, Lc, incl_file );
            exit( EXIT_FAILURE );
        }

        /* If this is the very first include store the current state in
           some special variables... */

        if ( ++incl_depth == 0 )
        {
            primary_buf = YY_CURRENT_BUFFER;
            primary_fp = yyin;
        }

        /* ...store the still current file name, line number and EOL state,
           set them for the new file to be included and write the new file's
           name into the output file... */

        fname[ incl_depth ] = Fname;
        lc[ incl_depth ] = Lc;
        eol_state[ incl_depth ] = Eol;
        Fname = incl_file;
        Lc = 1;
        Eol = SET;
        output( "\x01\n%s\n", Fname );

        /* ...and switch to the new file after allocating a buffer for it */

        buf_state[ incl_depth ] = yy_create_buffer( fp[ incl_depth ],
                                                    YY_BUF_SIZE );
        yy_switch_to_buffer( buf_state[ incl_depth ] );
        yyin = fp[ incl_depth ];
    }
    else                                    /* at the end of an include file */
    {
        /* If the end of the very first file is reached exit signaling
           success, otherwise switch back to the previous buffer and file
           (and also restore the it's name, line number and EOL state) */

        if ( incl_depth == -1 )
        {
            T_free( Fname );
            exit( EXIT_SUCCESS );
        }
        else
        {
            T_free( Fname );
            Fname = fname[ incl_depth ];
            Lc = lc[ incl_depth ];
            Eol = eol_state[ incl_depth ];
            xclose( primary_buf, buf_state, primary_fp, fp, &incl_depth );
            output( "\x01\n%s\n", Fname );
        }
    }
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void xclose( YY_BUFFER_STATE   primary_buf,
                    YY_BUFFER_STATE * buf_state,
                    FILE *            primary_fp,
                    FILE **           fp,
                    int *             incl_depth )
{
    if ( *incl_depth > 0 )
    {
        yy_switch_to_buffer( buf_state[ *incl_depth - 1 ] );
        yyin = fp[ *incl_depth - 1 ];
        fclose( fp[ *incl_depth ] );
        yy_delete_buffer( buf_state[ *incl_depth ] );
    }
    else
    {
        yy_switch_to_buffer( primary_buf );
        yyin = primary_fp;
        fclose( fp[ 0 ] );
        yy_delete_buffer( buf_state[ 0 ] );
    }

    *incl_depth -= 1;
}


/*---------------------------------------------------------------------*
 * Rewrites combinations like "100ns" or "100 ns" as "0.000000100000".
 *---------------------------------------------------------------------*/

static void
unit_spec( char * text,
           int    power )
{
    double dval;


    dval = strtod( text, NULL );
    if ( ( dval == HUGE_VAL || dval == 0.0 ) && errno == ERANGE )
    {
        output( "\x03\n%s:%ld: Overflow or underflow occurred while "
                "reading a number.\n", Fname, Lc );
        exit( EXIT_FAILURE );
    }

    switch ( power )
    {
        case -9 :
             output( "%.12f", dval * 1.0e-9 );
             break;

        case -6 :
             output( "%.12f", dval * 1.0e-6 );
             break;

        case -5 :
             output( "%.12f", dval * 1.0e-5 );
             break;

        case -3 :
             output( "%.12f", dval * 1.0e-3 );
             break;

        case -2 :
             output( "%.12f", dval * 1.0e-2 );
             break;

        case 0 :
             output( "%.12f", dval );
             break;

        case 1 :
             output( "%.12f", dval * 10.0 );
             break;

        case 3 :
             output( "%.12f", dval * 1.0e3 );
             break;

        case 4 :
             output( "%.12f", dval * 1.0e4 );
             break;

        case 6 :
             output( "%.12f", dval * 1.0e6 );
             break;

        case 9 :
             output( "%f", dval * 1.0e9 );
             break;

        default :
             output( "\x03\n%s:%ld: Internal error in fsc2_clean.\n",
                     Fname, Lc );
             exit( EXIT_FAILURE );
    }
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void *T_malloc( size_t size )
{
    void *mem;


    if ( ( mem = malloc( size ) ) == NULL )
        THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined FSC2_MDEBUG
    fprintf( stderr, "malloc:  %p (%u)\n", mem, size );
#endif

    return mem;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void *
T_free( void * ptr )
{
#if defined FSC2_MDEBUG
    fprintf( stderr, "free:    %p\n", ptr );
    assert( mprobe( ptr ) == MCHECK_OK );
#endif

    free( ptr );
    return NULL;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static void *
T_realloc( void * ptr,
           size_t size )
{
    void *new_ptr;

#if defined FSC2_MDEBUG
    fprintf( stderr, "malloc:  %p (%u)\n", ptr, size );
#endif

    new_ptr = realloc( ptr, size );

    if ( new_ptr == NULL )
        THROW( OUT_OF_MEMORY_EXCEPTION );

    return new_ptr;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static void *
T_realloc_or_free( void * ptr,
                   size_t size )
{
    void *new_ptr;

#if defined FSC2_MDEBUG
    fprintf( stderr, "malloc:  %p (%u)\n", ptr, size );
#endif

    new_ptr = realloc( ptr, size );

    if ( new_ptr == NULL )
    {
        if ( ptr != NULL )
            T_free( ptr );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

    return new_ptr;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static char *
T_strdup( const char * string )
{
    char *new_mem;

    if ( string == NULL )
        return NULL;

    if ( ( new_mem = malloc( strlen( string ) + 1 ) ) == NULL )
        THROW( OUT_OF_MEMORY_EXCEPTION );
    strcpy( new_mem, string );

#if defined FSC2_MDEBUG
    fprintf( stderr, "strdup:  %p (%u)\n", new_mem, strlen( string ) );
#endif

    return new_mem;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
lower_permissions( void )
{
    seteuid( getuid( ) );
    setegid( getgid( ) );
}


/*---------------------------------------------------------------------------*
 * Most of the time the program will write to a pipe and not to a real file.
 * To avoid writing only parts of the text when the pipe should get full we
 * loop over a write() call until all bytes got written.
 *---------------------------------------------------------------------------*/

#define OUTPUT_TRY_LENGTH 256

static ssize_t
output( const char * fmt,
        ... )
{
    static char *c = NULL;
    static size_t len = OUTPUT_TRY_LENGTH;
    size_t bytes_to_write;
    ssize_t last_written;
    size_t total_bytes_written = 0;


    if ( c == NULL )
        c = T_malloc( len );

    while ( 1 )
    {
        va_list ap;
        int wr;
        size_t new_len;


        va_start( ap, fmt );
        wr = vsnprintf( c, len, fmt, ap );
        va_end( ap );

        /* Older versions of glibc (up to 2.0.6) return a negativ value when
           there wasn't enough space in the output string, newer ones return
           the number of characters needed, not counting the trailing '\0'. */

        if ( wr >= 0 && ( size_t ) wr + 1 <= len )
            break;

        /* Usually double the size of the buffer, only when this wouldn't be
           enough (as far as we can figure out) allocate as much as needed.
           Update the 'len' variable only after the realloc call succeeded,
           otherwise it would be wrong when we're running out of memory in
           the realloc call. */

        new_len = 2 * len;
        if ( wr >= 0 && ( size_t ) wr + 1 > new_len )
            new_len = ( size_t ) wr + 1;

        c = T_realloc_or_free( c, new_len );
        len = new_len;
    }

    bytes_to_write = strlen( c );

    while ( bytes_to_write > 0 )
    {
        if ( ( last_written = write( STDOUT_FILENO, c + total_bytes_written,
                                     bytes_to_write ) ) < 0 )
        {
            if ( errno == EINTR )   /* non-deadly signal has been received ? */
                continue;
            THROW( EXCEPTION );     /* sorry, can't continue */
        }

        bytes_to_write -= last_written;
        total_bytes_written += last_written;
    }

    return total_bytes_written;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
usage( void )
{
    fprintf( stderr, "Usage: fsc2_clean [ filename ]\n"
             "This program predigests EDL files and is only meant for\n"
             "internal use by fsc2(1). But since you're already here:\n"
             "fsc2_clean expects input (i.e. an EDL programs) on STDIN\n"
             "and writes a cleaned up version to STDOUT. It is e.g.\n"
             "responsible for getting rid of comments, inclusion of\n"
             "further EDL files (so fsc2 has only to deal with a flat\n"
             "and simple input stream) and also already may detect some\n"
             "flaws in an EDL file. If you want to see what fsc2 really\n"
             "sees of your EDL file try\n\n"
             "    fsc2_clean `pwd`/my_program.edl < my_program.edl\n\n"
             "(but be prepared to get some non-printable character).\n"
             "If it gets passed a file name as the only argument this\n"
             "name is used in error messages. When called with an argument\n"
             "of either \"-h\" or \"--help\" (or with more than one "
             "argument)\nit prints out this text.\n" );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */