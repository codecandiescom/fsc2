/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive stack

%{

/* if not defined set default directory for include files */

#if ! defined DEF_INCL_DIR
#define DEF_INCL_DIR "/usr/local/lib/fsc"
#endif


#define FSC2_MAIN
#include "fsc2.h"

#define MAX_INCLUDE_DEPTH  16


int yylex( void );

void include_handler( char *file );
void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
			 FILE *primary_fp, FILE **fp, int *incl_depth, char *fname );
void time_spec( char *text, long len, const char *unit );


long Lc,
	 Comm_Lc,
	 Incl_Lc,
	 Str_Lc;
char *Fname;
bool Eol;


%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

INT      [+-]?[0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

NS       N(ANO)?_?S(ECOND(S)?)?
US       (MICRO?_?S(ECOND(S)?)?)|(US)
MS       M(ILLI)?_?S(SECOND(S)?)?
S        S(ECOND(S)?)?
		 
INS      {INT}{NS}
IUS      {INT}{US}
IMS      {INT}{MS}
IS       {INT}{S}
		 
FNS      {FLOAT}{NS}
FUS      {FLOAT}{US}
FMS      {FLOAT}{MS}
FS       {FLOAT}{S}

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

INCL    ^[ \t]*#incl?u?d?e?[\t ]*
IFN     \"[^"\n]*\"[\t ]*\n?|<[^>\n]*>[\t ]*\n?
IFNE    [^"<\t \n]*\n?
EXIT    ^[ \t]*#exit
QUIT    ^[ \t]*#quit

KEEP    [^\t" \n(\/*),;:=%\^\-\+]+


%x      str
%x      comm
%x		incl


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/





			/* handling of string constants (use only in print() function) */
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
{REM1}		Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		{
		 		printf( "\n" );
				Lc++;
				Eol = SET;
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
				/* on end of comment in #include-statement don't dunp '\n' */

				if ( yy_top_state( ) == incl )
				   unput( '\n' );

				Eol = SET;
				Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

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
{REM2}      Lc++;

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

            /* here some special handling of time specifications */
{NS}/(,|;)  printf( "\x04nsec" );
{NS}        printf( "\x04nsec" );
{US}/(;|,)  printf( "\x04usec" );
{US}        printf( "\x04usec" );
{MS}/(;|,)  printf( "\x04msec" );
{MS}        printf( "\x04msec" );
{S}/(;|,)   printf( "\x04sec" );
{S}         printf( "\x04sec" );
{INS}/(;|,) time_spec( yytext, yyleng, "\x04nsec" );
{INS}       time_spec( yytext, yyleng, "\x04nsec" );
{IUS}/(;|,) time_spec( yytext, yyleng, "\x04usec" );
{IUS}       time_spec( yytext, yyleng, "\x04usec" );
{IMS}/(;|,) time_spec( yytext, yyleng, "\x04msec" );
{IMS}       time_spec( yytext, yyleng, "\x04msec" );
{IS}/(;|,)  time_spec( yytext, yyleng, "\x04sec" );
{IS}        time_spec( yytext, yyleng, "\x04sec" );
{FNS}/(;|,) time_spec( yytext, yyleng, "\x04nsec" );
{FNS}       time_spec( yytext, yyleng, "\x04nsec" );
{FUS}/(;|,) time_spec( yytext, yyleng, "\x04usec" );
{FUS}       time_spec( yytext, yyleng, "\x04usec" );
{FMS}/(;|,) time_spec( yytext, yyleng, "\x04msec" );
{FMS}       time_spec( yytext, yyleng, "\x04msec" );
{FS}/(;|,)  time_spec( yytext, yyleng, "\x04sec" );
{FS}        time_spec( yytext, yyleng, "\x04sec" );

			/* all the rest is simply copied to the output */

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
	Fname = argv[ 1 ];
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
			incl_file = get_string_copy( file + 1 );
			*( incl_file + strlen( incl_file ) - 1 ) = '\0';
		}
		else
		{
			incl_file = get_string( strlen( file ) + strlen( DEF_INCL_DIR ) );
			strcpy( incl_file, DEF_INCL_DIR );
			strcat( incl_file, "/" );
			strcat( incl_file, file + 1 );
			*( incl_file + strlen( incl_file ) - 1 ) = '\0';
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

		fname[ incl_depth ] = get_string_copy( Fname );
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
			exit( EXIT_SUCCESS );
		else
		{
			free( Fname );
			Fname = fname[ incl_depth ];
			Lc = lc[ incl_depth ];
			Eol = eol_state[ incl_depth ];
			xclose( primary_buf, buf_state, primary_fp, fp,
					&incl_depth, fname[ incl_depth ] );
			printf( "\x01\n%s\n", Fname );
		}
	}
}


void xclose( YY_BUFFER_STATE primary_buf, YY_BUFFER_STATE *buf_state,
			 FILE *primary_fp, FILE **fp, int *incl_depth, char *fname )
{
	if ( *incl_depth > 0 )
	{
		yy_switch_to_buffer( buf_state[ *incl_depth - 1 ] );
		yyin = fp[ *incl_depth - 1 ];
		fclose( fp[ *incl_depth ] );
		yy_delete_buffer( buf_state[ *incl_depth ] );
		free( fname );
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

void time_spec( char *text, long len, const char *unit )
{
	register char *q = text + len - 1;

	while ( isalpha( *q ) || *q == '_' )
		q--;
	*++q = '\0';
	printf( "%s", text );
	printf( " %s", unit );
}
