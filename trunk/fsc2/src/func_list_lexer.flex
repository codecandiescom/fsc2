/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-sensitive stack nounput noyy_top_state

%{

#include "fsc2.h"

int func_listlex( void );
int fll_count_functions( Func *fncts, int num_def_func );
void fll_get_functions( Func *fncts, int num_def_func );

static long Comm_Lc;
static bool Eol;

#define INT_TOKEN    256
#define ALL_TOKEN    257
#define EXP_TOKEN    258
#define PREP_TOKEN   259
#define IDENT_TOKEN  260


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
						"%ld.\n", Fname, Comm_Lc );
				THROW( EXCEPTION );
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
						"data base `%s'.\n", Lc, Fname );
				THROW( EXCEPTION );
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
PREP        return PREP_TOKEN;
{INT}       return INT_TOKEN;
{IDENT}     return IDENT_TOKEN;
;           return ';';

.           {
				eprint( FATAL, "Syntax error at line %ld in function data "
						"base `%s'.\n", Lc, Fname );
				THROW( EXCEPTION );
			}

<<EOF>>		return 0;



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


int func_list_parse( Func **fncts, int num_func )
{
	static bool is_restart = UNSET;
	int num_def_func = num_func;
	int num;
	int cur;


	T_free( Fname );
	Fname = get_string( strlen( libdir ) + strlen( "Functions" ) );
	strcpy( Fname, libdir );
	if ( libdir[ strlen( libdir ) - 1 ] != '/' )
		strcat( Fname, "/" );
	strcat( Fname, "Functions" );

	if ( ( func_listin = fopen( Fname, "r" ) ) == NULL )
	{
		eprint( FATAL, "Can't open function data base `%s'.\n", Fname );
		THROW( EXCEPTION );
	}

	/* Keep the lexer happy... */

	if ( is_restart )
	    func_listrestart( func_listin );
	else
		 is_restart = SET;

	/* count the number of functions defined in the file */

	TRY
	{
		num = fll_count_functions( *fncts, num_def_func );
		assert( num >= 0 );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( func_listin );
		PASSTHROU( );
	}

	/* Rewind input file, allocate memory for the additional functions and
	   set defaults values */

	rewind( func_listin );
	num_func = num + num_def_func;

	if ( num != 0 )
		*fncts = T_realloc( *fncts, ( num_func + 1 ) * sizeof( Func ) );

	for ( cur = num_def_func; cur <= num_func; cur++ )
	{
		( *fncts )[ cur ].name = NULL;
		( *fncts )[ cur ].fnct = NULL;
		( *fncts )[ cur ].nargs = 0;
		( *fncts )[ cur ].access_flag = ACCESS_EXP;
		( *fncts )[ cur ].to_be_loaded = SET;
	}

	( *fncts )[ num_func ].to_be_loaded = UNSET;

	/* Now parse file again */

	TRY
	{
		fll_get_functions( *fncts, num_def_func );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( func_listin );
		PASSTHROU( );
	}
	
	fclose( func_listin );
	return num_func;
}


/*---------------------------------------------------*/
/* Function runs once through the function data base */
/* file and counts the number of defined functions   */
/*---------------------------------------------------*/

int fll_count_functions( Func *fncts, int num_def_func )
{
	int ret_token;
	int last_token = 0;
	int num = 0;
	int i = -1;


	Lc = 1;
	Eol = SET;

	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		/* If the name of a loadable function conflicts with the name of a
		   built-in function the loadable function will replace the built-in
		   function completely (even if it doesn't get loaded!) */

		if ( ret_token == IDENT_TOKEN )
		{
			for ( i = 0; i < num_def_func; i++ )
				if ( ! strcmp( fncts[ i ].name, func_listtext ) )
				{
					eprint( WARN, "Loadable function `%s()' hides "
							"built-in function (line %ld in function data "
							"base `%s').\n", func_listtext, Lc, Fname );

					fncts[ i ].name = get_string_copy( func_listtext );
					fncts[ i ].fnct = NULL;
					fncts[ i ].nargs = 0;
					fncts[ i ].access_flag = ACCESS_EXP;
					fncts[ i ].to_be_loaded = SET;
					break;
				}
		}

		/* Increment number of functions only after a semicolon (but not if
		   the previous token was also a semicolon) and if the new function
		   didn't overload a built-in function */

		if ( ret_token == ';' && last_token != ';' && i == num_def_func )
		   num++;
		last_token = ret_token;
	}

	if ( last_token != ';' )
	{
		eprint( FATAL, "Missing semiciolon at end of function data base "
				"`%s'.\n", Fname );
		THROW( EXCEPTION );
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
							"base `%s'.\n", Lc, Fname );
					THROW( EXCEPTION );
				}

				/* Allow overloading of buit-in functions but don't allow
				   multiple overloading */

				for ( i = 0; i < cur; i++ )
					if ( ! strcmp( fncts[ i ].name, func_listtext ) )
					{
						if ( i < num_def_func )
							act = i;
						else
						{
							eprint( FATAL, "Function `%s()' is declared more "
									"than once in function data base `%s'.\n",
									func_listtext, Fname );
							THROW( EXCEPTION );
						}
						break;
					}

				if ( act >= num_def_func )
				    fncts[ act ].name = get_string_copy( func_listtext );

				state = 1;
				break;

			case INT_TOKEN :
				if ( state != 1 )
				{
					eprint( FATAL, "Syntax error at line %ld of function data "
							"base `%s'.\n", Lc, Fname );
					THROW( EXCEPTION );
				}

				fncts[ act ].nargs = atol( func_listtext );
				state = 2;
				break;

			case ALL_TOKEN : case EXP_TOKEN : case PREP_TOKEN :
				if ( state != 2 )
				{
					eprint( FATAL, "Syntax error at line %ld of function data "
							"base `%s'.\n", Lc, Fname );
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
					eprint( FATAL, "Missing number of arguments for function "
							"`%s' at line %ld in function data base `%s'.\n",
							fncts[ act ].name, Lc, Fname );
					THROW( EXCEPTION );
				}
				state = 0;
				if ( act == cur )   /* "new" function ? */
					act = ++cur;
				else                /* built-in function has been overloaded */
					act = cur;
				break;

			default:
				assert( 1 == 0 );              /* this can never happen... */
		}
	}
}
