/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive stack nounput

%{

#include "fsc2.h"

int func_listlex( void );
int fll_count_functions( void );
void fll_get_functions( Func *fncts, int num_def_func );

static long Lc;
static char *Fname;

static long Comm_Lc;
static bool Eol;

#define INT_TOKEN    256
#define ALL_TOKEN    257
#define EXP_TOKEN    258
#define IDENT_TOKEN  259


%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

INT      [+-]?[0-9]+

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

IDENT    [A-Za-z][A-Za-z_0-9]*

%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}		Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		{
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
<<EOF>>     {
				eprint( FATAL, "%s: End of file in comment starting at line "
						"%ld\n.", Fname, Comm_Lc );
				THROW( FUNCTION_EXCEPTION );
			}

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				Eol = SET;
				Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     {  /* End of comment without start */
				eprint( FATAL, "End of comment found at line %ld in function "
						"data base `%s'\n.", Lc, Fname );
				THROW( FUNCTION_EXCEPTION );
			}

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      Lc++;

{LWS}       /* dumps of leading white space */

			/* dumps trailing white space */
{TWS}       {
				Lc++;
				Eol = SET;
			}

{WS}        /* dum (other) white space */

			/* writes out EOL character */
\n			{
				Lc++;
				Eol = SET;
			}

,           /* skip commas */

ALL         return ALL_TOKEN;
EXP         return EXP_TOKEN;
{INT}       return INT_TOKEN;
{IDENT}     return IDENT_TOKEN;
;           return ';';

.           {
				eprint( FATAL, "Syntax error at line %ld in function data "
						"base `%s'.\n", Lc, Fname );
				THROW( FUNCTION_EXCEPTION );
			}

<<EOF>>		return( 0 );



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


void func_list_parse( Func **fncts, int num_def_func, int *num_func )
{
	int num;
	int cur;


	Fname = get_string_copy( "Functions" );

	if ( ( func_listin = fopen( Fname, "r" ) ) == NULL )
	{
		eprint( FATAL, "Can't open function data base `%s'", Fname );
		THROW( FUNCTION_EXCEPTION );
	}

	eprint( NO_ERROR, "Parsing function data base `%s'.\n", Fname );

	/* count the number of functions defined in the file */

	TRY
	{
		num = fll_count_functions( );
		TRY_SUCCESS;
	}
	CATCH( FUNCTION_EXCEPTION )
	{
		fclose( func_listin );
		free( Fname );
		PASSTHROU( );
	}

	if ( num == 0 )                  /* none found in input file ? */
	    return;

	/* rewind input file, allocate memory for the functions and set defaults */

	rewind( func_listin );
	*fncts = T_realloc( *fncts, ( num + num_def_func + 1 ) * sizeof( Func ) );
	*num_func = num + num_def_func;

	for ( cur = num_def_func; cur <= *num_func; cur++ )
	{
		( *fncts )[ cur ].name = NULL;
		( *fncts )[ cur ].fnct = NULL;
		( *fncts )[ cur ].nargs = 0;
		( *fncts )[ cur ].access_flag = ACCESS_RESTRICTED;
		( *fncts )[ cur ].to_be_loaded = UNSET;
	}

	/* Now parse file again */

	TRY
	{
		fll_get_functions( *fncts, num_def_func );
		TRY_SUCCESS;
	}
	CATCH( FUNCTION_EXCEPTION )
	{
		fclose( func_listin );
		free( Fname );
		PASSTHROU( );
	}
	

	fclose( func_listin );
	free( Fname );
}


/*---------------------------------------------------*/
/* Function runs once to the function data base file */
/* and counts the number of defined functions        */
/*---------------------------------------------------*/

int fll_count_functions( void )
{
	int ret_token;
	int last_token;
	int num = 0;


	Lc = 1;
	Eol = SET;

	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		if ( ret_token == ';' && last_token != ';' )
		   num++;
		last_token = ret_token;
	}

	if ( last_token != ';' )
	{
		eprint( FATAL, "Missing semiciolon at end of function data base "
				"`%s'.\n", Fname );
		THROW( FUNCTION_EXCEPTION );
	}

	return num;
}


/*------------------------------------------------------------*/
/* Function runs a second time through the function data base */
/* and tries to make sense from the entries                   */
/*------------------------------------------------------------*/

void fll_get_functions( Func *fncts, int num_def_func )
{
	int ret_token;
	int state;
	int cur;
	int act;
	int i;


	Lc = 1;
	Eol = SET;
	state = 0;
	act = cur = num_def_func;

	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		switch ( ret_token )
		{
			case IDENT_TOKEN :
				if ( state != 0 )
				{
					eprint( FATAL, "Syntax error at line %ld in function data "
							"base `%s' .\n", Lc, Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				/* Allow overloading of buit-in functions (but warn)... */

				for ( i = 0; i < num_def_func; i++ )
					if ( ! strcmp( fncts[ i ].name, func_listtext ) )
					{
						eprint( WARN, "User-defined function `%s()' will hide "
								"built-in function (line %ld in function data "
								"base `%s'.\n", func_listtext, Lc, Fname );
						act = i;
					}

				/* ... but don't allow multiple user definitions */

				for ( ; i < cur; i++ )
					if ( ! strcmp( fncts[ i ].name, func_listtext ) )
					{
						eprint( FATAL, "Function `%s()' is declared more than "
								"once in function data base `%s'.\n",
								func_listtext, Fname );
						THROW( FUNCTION_EXCEPTION );
					}


				if ( act >= num_def_func )
				    fncts[ act ].name = get_string_copy( func_listtext );
				fncts[ act ].to_be_loaded = SET;
				state = 1;
				break;

			case INT_TOKEN :
				if ( state != 1 )
				{
					eprint( FATAL, "Syntax error at line %ld of function data "
							"base `%s'.\n", Lc, Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				fncts[ act ].nargs = atol( func_listtext );
				state = 2;
				break;

			case ALL_TOKEN : case EXP_TOKEN :
				if ( state != 2 )
				{
					eprint( FATAL, "Syntax error at line %ld of function data "
							"base `%s'.\n", Lc, Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				if ( ret_token == ALL_TOKEN )				
					fncts[ act ].access_flag = ACCESS_ALL_SECTIONS;
				else
					fncts[ act ].access_flag = ACCESS_RESTRICTED,
				state = 3;
				break;

			case ';' :
				 if ( state != 0 && state < 2 )
				{
					eprint( FATAL, "Missing number of arguments for function "
							"`%s' at line %ld in function data base `%s'.\n",
							fncts[ act ].name, Lc, Fname );
					THROW( FUNCTION_EXCEPTION );
				}
				state = 0;
				act = ++cur;
				break;

			default:
				assert( 1 == 0 );
		}
	}
}
