/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap stack

%{

/* if not defined set default directory for include files */

#if ! defined DEF_INCL_DIR
#define DEF_INCL_DIR "/usr/local/lib/fsc2"
#endif


#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <math.h>

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

void include_handler( char *file );
void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
			 FILE *primary_fp, FILE **fp, int *incl_depth );
void unit_spec( char *text, int type, int power );
char *get_string_copy( const char *string );
char *get_string( size_t len );
void *T_malloc( size_t size );
void *T_calloc( size_t nmemb, size_t size );
void *T_realloc( void *ptr, size_t size );
void T_free( void *ptr );


long Lc,
	 Comm_Lc,
	 Incl_Lc,
	 Str_Lc;
char *Fname;
bool Eol;


%}

REM1     ^[\t ]*"//".*\n
REM2     [\t ]*"//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

UNIT     ("s"|"G"|"V"|"A"|"Hz")

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

FNT      {FLOAT}[\t \r]*"nT"
FUT      {FLOAT}[\t \r]*"uT"
FMT      {FLOAT}[\t \r]*"mT"
FT       {FLOAT}[\t \r]*"T"


WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

INCL    ^[ \t]*#incl?u?d?e?[\t ]*
IFN     \"[^"\n]*\"[\t ]*\n?|<[^>\n]*>[\t ]*\n?
IFNE    [^"<\t \n]*\n?
EXIT    ^[ \t]*#exit
QUIT    ^[ \t]*#quit


THROU   [+-][A-Za-z]+
KEEP    [^\t" \n(\/*),;:=%\^\-\+]+


%x      str
%x      comm
%x		incl


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


			/* handling of string constants */
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

            /* handling of C++ style comment spanning a whole line */
{REM1}		{
				Lc++;
			    Eol = SET;
			}

			/* handling of C++ style comment not spanning a whole line */
{REM2}		{
				Lc++;
				Eol = SET;
				printf( "\n" );
			}

			/* handling of C style comment */
{REM3}		{
				Comm_Lc = Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

			/* end of line character in a comment */
\n			{
				Lc++;
				Eol = SET;
			}

			/* handling of EOF within a comment -> fatal error */
<<EOF>>		THROW( EOF_IN_COMMENT_EXCEPTION );

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				/* on end of comment in #include-statement don't dump '\n' */

				if ( yy_top_state( ) == incl )
				   unput( '\n' );

				Eol = SET;
				Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT )

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      Lc++;

{LWS}       /* dumps of leading white space */

			/* dumps trailing white space */
{TWS}       {
				Lc++;
				Eol = SET;
				printf( "\n" );
			}

			/* reduces any amount of (other) white space to just one space */
{WS}        printf( " " );

			/* writes out EOL character */
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
			/* handling of C++ style comments */
{REM2}      

{WS}       /* dumps white space */

            /*  handling of file name */
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

			/* either start of C style comment or syntax error */
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

            /* here some special handling of numbers with units */

{INANO}/[^a-zA-z_]   unit_spec( yytext, 0, -9 );
{IMICRO}/[^a-zA-z_]  unit_spec( yytext, 0, -6 );
{IMILLI}/[^a-zA-z_]  unit_spec( yytext, 0, -3 );
{INONE}/[^a-zA-z_]   unit_spec( yytext, 0, 0 );
{IKILO}/[^a-zA-z_]   unit_spec( yytext, 0, 3 );
{IMEGA}/[^a-zA-z_]   unit_spec( yytext, 0, 6 );

{FNANO}/[^a-zA-z_]   unit_spec( yytext, 1, -9 );
{FMICRO}/[^a-zA-z_]  unit_spec( yytext, 1, -6 );
{FMILLI}/[^a-zA-z_]  unit_spec( yytext, 1, -3 );
{FNONE}/[^a-zA-z_]   unit_spec( yytext, 1, 0 );
{FKILO}/[^a-zA-z_]   unit_spec( yytext, 1, 3 );
{FMEGA}/[^a-zA-z_]   unit_spec( yytext, 1, 6 );

{INTs}/[^a-zA-z_]    unit_spec( yytext, 0, -5 );
{IUT}/[^a-zA-z_]     unit_spec( yytext, 0, -2 );
{IMT}/[^a-zA-z_]     unit_spec( yytext, 0, 1 );
{IT}/[^a-zA-z_]      unit_spec( yytext, 0, 4 );
{FNT}/[^a-zA-z_]     unit_spec( yytext, 1, -5 );
{FUT}/[^a-zA-z_]     unit_spec( yytext, 1, -2 );
{FMT}/[^a-zA-z_]     unit_spec( yytext, 1, 1 );
{FT}/[^a-zA-z_]      unit_spec( yytext, 1, 4 );

"nT"/[^a-zA-z_]      printf( "\x4ntesla" );
"uT"/[^a-zA-z_]      printf( "\x4utesla" );
"mT"/[^a-zA-z_]      printf( "\x4mtesla" );
"T"/[^a-zA-z_]       printf( "\x4tesla" );

"n"{UNIT}/[^a-zA-z_] printf( "\x4nunit" );
"u"{UNIT}/[^a-zA-z_] printf( "\x4uunit" );
"m"{UNIT}/[^a-zA-z_] printf( "\x4munit" );
{UNIT}/[^a-zA-z_]
"k"{UNIT}/[^a-zA-z_] printf( "\x4kunit" );
"M"{UNIT}/[^a-zA-z_] printf( "\x4megunit" );

			/* all the rest is simply copied to the output */

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


int main( int argc, char *argv[ ] )
{
#if defined MDEBUG
	mcheck( NULL );
#endif

	/* complain if no file name was passed */

	if ( argc < 2 )
	{
		printf( "\x03\nMissing file name.\n" );
		return EXIT_FAILURE;
	}

	/* try to open the input file */

	yyin = fopen( argv[ 1 ], "r" );
	if ( yyin == NULL )
	{
		printf( "\x03\nCan't open input file %s.\n", argv[ 1 ] );
		return EXIT_FAILURE;
	}

	/* set the global variables and output name of input file */

	printf( "\x01\n%s\n", argv[ 1 ] );
	Fname = get_string_copy( argv[ 1 ] );
	Lc = 1;
	Eol = SET;

	/* now try to clean up the input and output appropriate error messages
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
				"at line %ld\n.", Fname, Comm_Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( EOF_IN_STRING_EXCEPTION )
	{
		printf( "\x03\n%s: End of line in string constant starting at "
				"line %ld\n.", Fname, Str_Lc );
		exit( EXIT_FAILURE );
	}
	CATCH( DANGLING_END_OF_COMMENT )
	{
		printf( "\x03\n%s: End of comment found in program text at "
				"line %ld\n.", Fname, Lc );
		exit( EXIT_FAILURE );
	}

	/* we finished succesfully... */

	return EXIT_SUCCESS;
}


void include_handler( char *file )
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
		/* if all the files we are prepared to open are already open
		   this is a fatal error (most probably it's due to an infinite
		   recurrsion) */

		if ( incl_depth + 1 == MAX_INCLUDE_DEPTH )
			THROW( TOO_DEEPLY_NESTED_EXCEPTION );

		/* now check if file name is enclosed by `"' or '<' and '>' - in the
		   later case we have to prepend the default directory */

		if ( file[ 0 ] == '"' )
		{

			*( file + strlen( file ) - 1 ) = '\0';
			incl_file = get_string_copy( file + 1 );
		}
		else
		{
			*( file + strlen( file ) - 1 ) = '\0';
			incl_file = get_string( strlen( file ) + strlen( DEF_INCL_DIR ) );
			strcpy( incl_file, DEF_INCL_DIR );
			strcat( incl_file, "/" );
			strcat( incl_file, file + 1 );
		}

		/* the same holds for the case that the file can't be opened. */

		if ( ( fp[ incl_depth + 1 ] = fopen( incl_file, "r" ) ) == NULL )
		{
			printf( "\x03\n%s:%ld: Can't open include file %s.\n",
					Fname, Lc, incl_file );
			exit( EXIT_FAILURE );
		}

		/* if this is the very first include we store the current state in
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
		/* if the end of the very first file is reached we exit signaling
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


void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
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


/* Takes combinations like "100ns" apart and writes them out as "100 nsec" */

void unit_spec( char *text, int type, int power )
{
	double dval;
	long lval;


	if ( type == 0 )
	{
		lval = strtol( text, NULL, 10 );
		if ( ( lval == LONG_MAX || dval == LONG_MIN ) && errno == ERANGE )
		    goto try_double;

		switch ( power )
		{
			case -9 :
				if ( lval % 1000000000L )
				    printf( "%.9f", ( double ) lval * 1.0e-9 );
				else
				 	printf( "%ld", lval / 1000000000L );
				return;

			case -6 :
				if ( lval % 1000000 )
					printf( "%.6f", ( double ) lval * 1.0e-6 );
				else
				 	printf( "%ld", lval / 100000 );
				return;

			case -5 :
				if ( lval % 100000 )
					printf( "%.6f", ( double ) lval * 1.0e-5 );
				else
				 	printf( "%ld", lval / 10000 );
				return;

			case -3 :
				if ( lval % 1000 )
					printf( "%.3f", ( double ) lval * 1.0e-3 );
				else
				 	printf( "%ld", lval / 1000 );
				return;

			case -2 :
				if ( lval % 100 )
					printf( "%.3f", ( double ) lval * 1.0e-2 );
				else
				 	printf( "%ld", lval / 100 );
				return;

			case 0 :
				printf( "%ld", lval );
				return;

			case 1 :
				if ( labs( lval ) > LONG_MAX / 10 )
				    printf( "%f", ( double ) lval * 10.0 );
				else
					printf( "%ld", lval * 10 );
				return;

			case 3 :
				if ( labs( lval ) > LONG_MAX / 1000 )
				    printf( "%f", ( double ) lval * 1000.0 );
				else
					printf( "%ld", lval * 1000 );
				return;

			case 4 :
				if ( labs( lval ) > LONG_MAX / 10000 )
				    printf( "%f", ( double ) lval * 10000.0 );
				else
					printf( "%ld", lval * 10000 );
				return;

			case 6 :
				if ( labs( lval ) > LONG_MAX / 1000000L )
				    printf( "%f", ( double ) lval * 1.0e6 );
				else
					printf( "%ld", lval * 1000000L );
				return;

			case 9 :
				if ( labs( lval ) > LONG_MAX / 1000000000L )
				    printf( "%f", ( double ) lval * 1.0e9 );
				else
					printf( "%ld", lval * 1000000000L );
				return;

			default :
				 printf( "\x03\n%s:%ld: INTERNAL ERROR.\n", Fname, Lc );
				 exit( EXIT_FAILURE );
		}
	}

try_double:

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


void *T_malloc( size_t size )
{
	void *mem;


	if ( ( mem = malloc( size ) ) == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined MDEBUG
	fprintf( stderr, "malloc:  %p (%u)\n", mem, size );
#endif

	return mem;
}


void *T_calloc( size_t nmemb, size_t size )
{
	void *mem;


	if ( ( mem = calloc( nmemb, size ) ) == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined MDEBUG
	fprintf( stderr, "calloc:  %p (%u)\n", mem, nmemb * size );
#endif

	return mem;
}


void *T_realloc( void *ptr, size_t size )
{
	void *new_ptr;


	if ( ( new_ptr = realloc( ptr, size ) ) == NULL )
		THROW( OUT_OF_MEMORY_EXCEPTION );

#if defined MDEBUG
	fprintf( stderr, "realloc: %p -> %p (%u)\n", ptr, new_ptr, size );
#endif

	return new_ptr;
}


void T_free( void *ptr )
{
#if defined MDEBUG
	fprintf( stderr, "free:    %p\n", ptr );
	fflush( stderr );
	assert( mprobe( ptr ) == MCHECK_OK );
#endif

	free( ptr );
}


char *get_string_copy( const char *string )
{
	char *new;

	if ( string == NULL )
		return NULL;
	new = get_string( strlen( string ) );
	strcpy( new, string );
	return new;
}


char *get_string( size_t len )
{
	return T_malloc( ( len + 1 ) * sizeof( char ) );
}
