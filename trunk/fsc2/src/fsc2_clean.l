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


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap stack

%{

/* If not defined set default directory for include files */

#if ! defined DEF_INCL_DIR
#define DEF_INCL_DIR "/usr/local/lib/fsc2"
#endif


#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>

#include "exceptions.h"
#include "errno.h"

typedef unsigned char bool;

#define UNSET ( ( bool ) 0 )
#define SET   ( ( bool ) 1 )


#if defined MDEBUG
#include <mcheck.h>
#endif


#define MAX_INCLUDE_DEPTH  16


int yylex( void );

static void include_handler( char *file );
static void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
					FILE *primary_fp, FILE **fp, int *incl_depth );
static void unit_spec( char *text, int power );
static char *T_strdup( const char *string );
static void *T_malloc( size_t size );
static void *T_free( void *ptr );
static void lower_permissions( void );


long Lc,
	 Comm_Lc,
	 Incl_Lc,
	 Str_Lc;
char *Fname;
bool Eol;


%}

REM1     ^[\t ]*"//".*\n?
REM2     [\t ]*"//".*\n?
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n?

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

UNIT     ("s"|"G"|"V"|"A"|"Hz"|"dB"|"db")

INANO    {INT}[\t \r]*"n"{UNIT}
IMICRO   {INT}[\t \r]*"u"{UNIT}
IMILLI   {INT}[\t \r]*"m"{UNIT}
INONE    {INT}[\t \r]*{UNIT}
IKILO    {INT}[\t \r]*"k"{UNIT}
IMEGA    {INT}[\t \r]*"M"{UNIT}

FNANO    {FLOAT}[\t \r]*"n"{UNIT}
FMICRO   {FLOAT}[\t \r]*"u"{UNIT}
FMILLI   {FLOAT}[\t \r]*"m"{UNIT}
FNONE    {FLOAT}[\t \r]*{UNIT}
FKILO    {FLOAT}[\t \r]*"k"{UNIT}
FMEGA    {FLOAT}[\t \r]*"M"{UNIT}

INTs     {INT}[\t \r]*"nT"
IUT      {INT}[\t \r]*"uT"
IMT      {INT}[\t \r]*"mT"
IT       {INT}[\t \r]*"T"
IKT      {INT}[\t \r]*"kT"
IMGT     {INT}[\t \r]*"MT"

FNT      {FLOAT}[\t \r]*"nT"
FUT      {FLOAT}[\t \r]*"uT"
FMT      {FLOAT}[\t \r]*"mT"
FT       {FLOAT}[\t \r]*"T"
FKT      {FLOAT}[\t \r]*"kT"
FMGT     {FLOAT}[\t \r]*"MT"


WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

SHEBANG ^#!.*$
INCL    ^[ \t]*#INCLUDE[\t ]*
IFN     \"[^"\n]*\"[\t ]*\n?|<[^>\n]*>[\t ]*\n?
IFNE    [^"<\t \n]*\n?
EXIT    ^[ \t]*#EXIT
QUIT    ^[ \t]*#QUIT


THROU   [+-][A-Za-z]+
KEEP    [^\t" \n(\/*),;:=%\^\-\+]+


%x      str
%x      comm
%x		incl


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


			/* Handling of string constants */
\"          {
				printf( "\x05" );
				Str_Lc = Lc;
				yy_push_state( str );
			}

<str>{

\\\"        printf( "\\\"" );


\"          {
				printf( "\x06" );
				yy_pop_state( );
			}

\n          {
				Lc++;
				Eol = SET;
			}

[^"\\\n]+   printf( "%s", yytext );

<<EOF>>		THROW( EOF_IN_STRING_EXCEPTION );

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
{REM1}		{
				Lc++;
			    Eol = SET;
			}

			/* Handling of C++ style comment not spanning a whole line */
{REM2}		{
				Lc++;
				Eol = SET;
				printf( "\n" );
			}

			/* Handling of C style comment */
{REM3}		{
				Comm_Lc = Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* Skip everything that's not a '*' */
{REM5}      /* Skip all '*'s not followed by '/'s */

			/* End of line character in a comment */
\n			{
				Lc++;
				Eol = SET;
			}

			/* Handling of EOF within a comment -> fatal error */
<<EOF>>		THROW( EOF_IN_COMMENT_EXCEPTION );

			/* End of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* End of comment and end of line */
{EREM2}     {
				/* On end of comment in #include-statement don't dump '\n' */

				if ( yy_top_state( ) == incl )
				   unput( '\n' );

				Eol = SET;
				Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT )

			/* Dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      Lc++;

{LWS}       /* Dumps of leading white space */

			/* Dumps trailing white space */
{TWS}       {
				Lc++;
				Eol = SET;
				printf( "\n" );
			}

			/* Reduces any amount of (other) white space to just one space */
{WS}        printf( " " );

			/* Writes out EOL character */
\n			{
				Lc++;
				Eol = SET;
				printf( "\n" );
			}

            /* "#exit" means that rest of current file is to be discarded */
{EXIT}		{
				YY_FLUSH_BUFFER;
				include_handler( NULL );
			}

   		   /* "#quit" means that all following input is to be discarded */
{QUIT}		exit( EXIT_SUCCESS );

		    /* Handling of lines starting with "#include" */
{INCL}      {
				Incl_Lc = Lc;
				BEGIN( incl );
			}

<incl>{
			/* Handling of C++ style comments */
{REM2}      

{WS}        /* Dumps white space */

            /*  Handling of file name */
{IFN} 		{
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
					printf( "\x03\n%s:%ld: `#include' expects \"FILENAME\" "
							"or <FILENAME>.\n", Fname, Incl_Lc );
					exit( EXIT_FAILURE );
				}
			}
} /* end of <incl> */

            /* Here some special handling of numbers with units
			   take care they will be converted to double automatically */

{INANO}/[^a-zA-z_]   unit_spec( yytext, -9 );
{IMICRO}/[^a-zA-z_]  unit_spec( yytext, -6 );
{IMILLI}/[^a-zA-z_]  unit_spec( yytext, -3 );
{INONE}/[^a-zA-z_]   unit_spec( yytext,  0 );
{IKILO}/[^a-zA-z_]   unit_spec( yytext,  3 );
{IMEGA}/[^a-zA-z_]   unit_spec( yytext,  6 );
										
{FNANO}/[^a-zA-z_]   unit_spec( yytext, -9 );
{FMICRO}/[^a-zA-z_]  unit_spec( yytext, -6 );
{FMILLI}/[^a-zA-z_]  unit_spec( yytext, -3 );
{FNONE}/[^a-zA-z_]   unit_spec( yytext,  0 );
{FKILO}/[^a-zA-z_]   unit_spec( yytext,  3 );
{FMEGA}/[^a-zA-z_]   unit_spec( yytext,  6 );
										
{INTs}/[^a-zA-z_]    unit_spec( yytext, -5 );
{IUT}/[^a-zA-z_]     unit_spec( yytext, -2 );
{IMT}/[^a-zA-z_]     unit_spec( yytext,  1 );
{IT}/[^a-zA-z_]      unit_spec( yytext,  4 );
{IKT}/[^a-zA-z_]     unit_spec( yytext,  7 );
{IMGT}/[^a-zA-z_]    unit_spec( yytext, 10 );
{FNT}/[^a-zA-z_]     unit_spec( yytext, -5 );
{FUT}/[^a-zA-z_]     unit_spec( yytext, -2 );
{FMT}/[^a-zA-z_]     unit_spec( yytext,  1 );
{FT}/[^a-zA-z_]      unit_spec( yytext,  4 );
{FKT}/[^a-zA-z_]     unit_spec( yytext,  7 );
{FMGT}/[^a-zA-z_]    unit_spec( yytext, 10 );

"nT"/[^a-zA-z_]      printf( "\x4ntesla" );
"uT"/[^a-zA-z_]      printf( "\x4utesla" );
"mT"/[^a-zA-z_]      printf( "\x4mtesla" );
"T"/[^a-zA-z_]       printf( "\x4tesla" );
"kT"/[^a-zA-z_]      printf( "\x4ktesla" );
"MT"/[^a-zA-z_]      printf( "\x4Mtesla" );

"n"{UNIT}/[^a-zA-z_] printf( "\x4nunit" );
"u"{UNIT}/[^a-zA-z_] printf( "\x4uunit" );
"m"{UNIT}/[^a-zA-z_] printf( "\x4munit" );
{UNIT}/[^a-zA-z_]
"k"{UNIT}/[^a-zA-z_] printf( "\x4kunit" );
"M"{UNIT}/[^a-zA-z_] printf( "\x4megunit" );

			/* All the rest is simply copied to the output */

{THROU}		{
				if ( Eol )
				{
					printf( "\x02\n%ld\n", Lc );
					Eol = UNSET;
				}
				printf( "%s", yytext );
			}				

{KEEP}		{
				if ( Eol )
				{
					printf( "\x02\n%ld\n", Lc );
					Eol = UNSET;
				}
				printf( "%s", yytext );
			}				

<<EOF>>		include_handler( NULL );



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

int main( int argc, char *argv[ ] )
{
#if defined MDEBUG
	mcheck( NULL );
#endif

	lower_permissions( );

	/* Use stdin if no file name was passed to the program */

	if ( argc < 2 )
	{
		yyin = stdin;
		printf( "\x01\n" );
		Fname = T_strdup( "" );
	}
	else
	{
		yyin = fopen( argv[ 1 ], "r" );
		if ( yyin == NULL )
		{
			printf( "\x03\nCan't open input file %s.\n", argv[ 1 ] );
			return EXIT_FAILURE;
		}

		/* Set the global variables and output name of input file */

		printf( "\x01\n%s\n", argv[ 1 ] );
		Fname = T_strdup( argv[ 1 ] );
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
		printf( "\x03\n%s:%ld: Running out of system resources "
				"(memory).\n", Fname, Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( TOO_DEEPLY_NESTED_EXCEPTION )
	{
		printf( "\x03\n%s:%ld: Include levels too deeply nested.\n",
				Fname, Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( EOF_IN_COMMENT_EXCEPTION )
	{
		printf( "\x03\n%s: End of file in comment starting "
				"at line %ld.\n", Fname, Comm_Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( EOF_IN_STRING_EXCEPTION )
	{
		printf( "\x03\n%s: End of line in string constant starting at "
				"line %ld.\n", Fname, Str_Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( DANGLING_END_OF_COMMENT )
	{
		printf( "\x03\n%s: End of comment found in program text at "
				"line %ld.\n", Fname, Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( MISPLACED_SHEBANG )
	{
		printf( "\x03\n%s: `#!' must appear on first line instead of "
				"line %ld.\n", Fname, Lc );
		exit( EXIT_FAILURE );
	}

	/* Finished succesfully... */

	return EXIT_SUCCESS;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void include_handler( char *file )
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


	if ( file != NULL )		      /* at the start of an include file */
	{
		/* If all the files we are prepared to open are already open
		   this is a fatal error (most probably it's due to an infinite
		   recurrsion) */

		if ( incl_depth + 1 == MAX_INCLUDE_DEPTH )
			THROW( TOO_DEEPLY_NESTED_EXCEPTION );

		/* Now check if file name is enclosed by `"' or '<' and '>' - in the
		   later case we have to prepend the default directory */

		if ( file[ 0 ] == '"' )
		{

			*( file + strlen( file ) - 1 ) = '\0';
			incl_file = T_strdup( file + 1 );
		}
		else
		{
			*( file + strlen( file ) - 1 ) = '\0';
			incl_file = T_malloc( strlen( file )
								  + strlen( DEF_INCL_DIR ) + 2 );
			strcpy( incl_file, DEF_INCL_DIR );
			strcat( incl_file, "/" );
			strcat( incl_file, file + 1 );
		}

		/* The same holds for the case that the file can't be opened. */

		if ( ( fp[ incl_depth + 1 ] = fopen( incl_file, "r" ) ) == NULL )
		{
			printf( "\x03\n%s:%ld: Can't open include file %s.\n",
					Fname, Lc, incl_file );
			exit( EXIT_FAILURE );
		}

		/* If this is the very first include we store the current state in
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
		printf( "\x01\n%s\n", Fname );

		/* ...and switch to the new file after allocating a buffer for it */

		buf_state[ incl_depth ] = yy_create_buffer( fp[ incl_depth ],
													YY_BUF_SIZE );
		yy_switch_to_buffer( buf_state[ incl_depth ] );
		yyin = fp[ incl_depth ];
	}
	else						            /* at the end of an include file */
	{
		/* If the end of the very first file is reached we exit signaling
		   success, otherwise we switch back to the previous buffer and file
		   (and also restore the it's name, line number and EOL state) */

		if ( incl_depth == -1 )
		{
			T_free( Fname );
			fclose( yyin );
			exit( EXIT_SUCCESS );
		}
		else
		{
			T_free( Fname );
			Fname = fname[ incl_depth ];
			Lc = lc[ incl_depth ];
			Eol = eol_state[ incl_depth ];
			xclose( primary_buf, buf_state, primary_fp, fp, &incl_depth );
			printf( "\x01\n%s\n", Fname );
		}
	}
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
			 		FILE *primary_fp, FILE **fp, int *incl_depth )
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


/*-------------------------------------------------------------------------*/
/* Takes combinations like "100ns" apart and writes them out as "100 nsec" */
/*-------------------------------------------------------------------------*/

static void unit_spec( char *text, int power )
{
	double dval;


	dval = strtod( text, NULL );
	if ( ( dval == HUGE_VAL || dval == 0.0 ) && errno == ERANGE )
	{
		printf( "\x03\n%s:%ld: Overflow or underflow occurred while "
				"reading a number.\n", Fname, Lc );
		exit( EXIT_FAILURE );
	}

	switch ( power )
	{
		case -9 :
			 printf( "%.12f", dval * 1.0e-9 );
			 break;

		case -6 :
			 printf( "%.12f", dval * 1.0e-6 );
			 break;

		case -5 :
			 printf( "%.12f", dval * 1.0e-5 );
			 break;

		case -3 :
			 printf( "%.12f", dval * 1.0e-3 );
			 break;

		case -2 :
			 printf( "%.12f", dval * 1.0e-2 );
			 break;

		case 0 :
			 printf( "%.12f", dval );
			 break;

		case 1 :
			 printf( "%.12f", dval * 10.0 );
			 break;

		case 3 :
			 printf( "%.12f", dval * 1.0e3 );
			 break;

		case 4 :
			 printf( "%.12f", dval * 1.0e4 );
			 break;

		case 6 :
			 printf( "%.12f", dval * 1.0e6 );
			 break;

		case 9 :
			 printf( "%f", dval * 1.0e9 );
			 break;

		default :
			 printf( "\x03\n%s:%ld: INTERNAL ERROR.\n", Fname, Lc );
			 exit( EXIT_FAILURE );
	}
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void *T_malloc( size_t size )
{
	void *mem;


	if ( ( mem = malloc( size ) ) == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined MDEBUG
	fprintf( stderr, "malloc:  %p (%u)\n", mem, size );
#endif

	return mem;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void *T_free( void *ptr )
{
#if defined MDEBUG
	fprintf( stderr, "free:    %p\n", ptr );
	fflush( stderr );
	assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
	return NULL;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static char *T_strdup( const char *string )
{
	char *new;

	if ( string == NULL )
		return NULL;

	if ( ( new = strdup( string ) ) == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined MDEBUG
	fprintf( stderr, "strdup:  %p (%u)\n", new, strlen( string ) );
#endif

	return new;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void lower_permissions( void )
{
	seteuid( getuid( ) );
	setegid( getgid( ) );
}
