/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive stack nounput

%{

/* if not defined set default directory for include files */




#include "fsc2.h"

int func_listlex( void );

extern long Lc;
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

IDENT    [A-ZA-Z_0-9]+

%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


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
				eprint( FATAL, "%s: Syntax error at line %ld.\n", Fname, Lc );
				THROW( FUNCTION_EXCEPTION );
			}

<<EOF>>		return( 0 );



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


void func_list_parse( Func **fncts, int num_def_func, int *num_func )
{
	int ret_token;
	int last_token;
	int num = 0;
	int cur;
	int state;
	int i;


	Fname = get_string_copy( "Functions" );
	Lc = 1;
	Eol = SET;

	if ( ( func_listin = fopen( Fname, "r" ) ) == NULL )
	   return;

	/* count the number of functions defined in the file */

	TRY
	{
		while ( ( ret_token = func_listlex( ) ) != 0 )
		{
			if ( ret_token == ';' && last_token != ';' )
			   num++;
			last_token = ret_token;
		}

		if ( last_token != ';' )
		{
			eprint( FATAL, "%s: Missing semiciolon at end of file.\n", Fname );
			THROW( FUNCTION_EXCEPTION );
		}
	}
	if ( exception_id == NO_EXCEPTION )    /* everything worked out fine */
   		TRY_SUCCESS;
	CATCH( FUNCTION_EXCEPTION )
	{
		free( Fname );
		fclose( func_listin );
		THROW( FUNCTION_EXCEPTION );
	}
	CATCH( DANGLING_END_OF_COMMENT )
	{
		free( Fname );
		fclose( func_listin );
		eprint( FATAL, "%s: End of comment found at line %ld\n.", Fname, Lc );
		THROW( FUNCTION_EXCEPTION );
	}
	CATCH( EOF_IN_COMMENT_EXCEPTION )
	{
		free( Fname );
		fclose( func_listin );
		eprint( FATAL, "%s: End of file in comment starting at line %ld\n.",
				Fname, Comm_Lc );
		THROW( FUNCTION_EXCEPTION );
	}

	if ( num == 0 )     /* none found in input file ? */
	    return;

	/* rewind input file, allocate memory for the functions and set defaults */

	rewind( func_listin );
	*fncts = T_realloc( *fncts, ( num + num_def_func + 1 ) * sizeof( Func ) );
	*num_func = num + num_def_func;

	for ( cur = num_def_func; cur <= *num_func; cur++ )
	{
		( *fncts )[ cur ].name = NULL;
		( *fncts )[ cur ].fnct = NULL;
		( *fncts )[ cur ].access_flag = ACCESS_RESTRICTED;
	}

	/* Now parse file again */

	cur = num_def_func;
	Lc = 1;
	Eol = SET;
	state = 0;
	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		switch ( ret_token )
		{
			case IDENT_TOKEN :
				if ( state != 0 )
				{
					fclose( func_listin );
					eprint( FATAL, "%s: Syntax error at line %ld.\n",
							Fname, Lc );
					free( Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				for ( i = 0; i < cur; i++ )
					if ( ! strcmp( ( *fncts )[ i ].name, func_listtext ) )
					{
						fclose( func_listin );
						eprint( FATAL, "%s:%ld: Function `%s' is declared "
								"more than once.\n",
								Fname, Lc, func_listtext );
						free( Fname );
						THROW( FUNCTION_EXCEPTION );
					}

				( *fncts )[ cur ].name = get_string_copy( func_listtext );
				state = 1;
				break;

			case INT_TOKEN :
				if ( state != 1 )
				{
					fclose( func_listin );
					eprint( FATAL, "%s: Syntax error at line %ld.\n",
							Fname, Lc );
					free( Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				( *fncts )[ cur ].nargs = atol( func_listtext );
				state = 2;
				break;

			case ALL_TOKEN : case EXP_TOKEN :
				if ( state != 2 )
				{
					fclose( func_listin );
					eprint( FATAL, "%s: Syntax error at line %ld.\n",
							Fname, Lc );
					free( Fname );
					THROW( FUNCTION_EXCEPTION );
				}

				if ( ret_token == ALL_TOKEN )				
					( *fncts )[ cur ].access_flag = ACCESS_ALL_SECTIONS;
				else
					( *fncts )[ cur ].access_flag = ACCESS_RESTRICTED,
				state = 3;
				break;

			case ';' :
				 if ( state != 0 && state < 2 )
				{
					fclose( func_listin );
					eprint( FATAL, "%s:%ld: Missing number of arguments.\n",
							Fname, Lc );
					free( Fname );
					THROW( FUNCTION_EXCEPTION );
				}
				state = 0;
				cur++;
				break;

			default:
				assert( 1 == 0 );
		}
	}


	fclose( func_listin );
	free( Fname );
}
