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

%option noyywrap case-sensitive stack nounput noyy_top_state

%{

#include "fsc2.h"

int func_listlex( void );
static int fll_count_functions( Func *fncts, int num_predef_func );
static int ffl_func_cmp2( const void *a, const void *b );
static void fll_get_functions( Func *fncts, int num_predef_func );

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
{REM5}      /* skipping all '*'s not followed by a '/' */

			/* end of line character in a comment */
\n			{
				Lc++;
				Eol = SET;
			}

			/* handling of EOF within a comment -> fatal error */
<<EOF>>     {
				eprint( FATAL, UNSET, "%s: End of file in comment starting at "
						"line %ld.\n", Fname, Comm_Lc );
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
				eprint( FATAL, UNSET, "End of comment found at line %ld in "
						"function data base `%s'.\n", Lc, Fname );
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

{WS}        /* dump (other) white space */

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
				eprint( FATAL, UNSET, "Syntax error at line %ld in function "
							   "data base `%s'.\n", Lc, Fname );
				THROW( EXCEPTION );
			}

<<EOF>>		return 0;



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


int func_list_parse( Func **fncts, int num_func )
{
	static bool is_restart = UNSET;
	static int nf;
	int num_predef_func = num_func;
	int num;
	int cur;


	nf = num_func;
	T_free( Fname );
	Fname = get_string( "%s%sFunctions", libdir, slash( libdir ) );

	if ( ( func_listin = fopen( Fname, "r" ) ) == NULL )
	{
		eprint( FATAL, UNSET, "Can't open function data base `%s'.\n", Fname );
		THROW( EXCEPTION );
	}

	/* Keep the lexer happy... */

	if ( is_restart )
	    func_listrestart( func_listin );
	else
		 is_restart = SET;

	/* Count the number of functions defined in the file */

	TRY
	{
		num = fll_count_functions( *fncts, num_predef_func );
		fsc2_assert( num >= 0 );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( func_listin );
		PASSTHROU( );
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
		*fncts = T_realloc( *fncts, nf * sizeof( Func ) );

	for ( cur = num_predef_func; cur < nf; cur++ )
	{
		( *fncts )[ cur ].name = NULL;
		( *fncts )[ cur ].fnct = NULL;
		( *fncts )[ cur ].nargs = 0;
		( *fncts )[ cur ].access_flag = ACCESS_EXP;
		( *fncts )[ cur ].device = NULL;
		( *fncts )[ cur ].to_be_loaded = SET;
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
		PASSTHROU( );
	}

	fclose( func_listin );
	return nf;
}


/*---------------------------------------------------*/
/* Function runs once through the function data base */
/* file and counts the number of defined functions   */
/*---------------------------------------------------*/

static int fll_count_functions( Func *fncts, int num_predef_func )
{
	int ret_token;
	int last_token = 0;
	int num = 0;
	Func *f = NULL;


	Lc = 1;
	Eol = SET;

	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		/* If the name of a loadable function conflicts with the name of a
		   built-in function the loadable function will replace the built-in
		   function completely (even if it doesn't get loaded!) */

		if ( ret_token == IDENT_TOKEN )
		{
			if ( ( f = bsearch( func_listtext, fncts, num_predef_func,
								sizeof( Func ), ffl_func_cmp2 ) ) != NULL )
			{
				eprint( WARN, UNSET, "Loadable function `%s()' hides "
						"built-in function (line %ld in function data "
						"base `%s').\n", f->name, Lc, Fname );

				f->name = T_strdup( f->name );
				f->fnct = NULL;
				f->nargs = 0;
				f->access_flag = ACCESS_EXP;
				f->device = NULL;
				f->to_be_loaded = SET;
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
		eprint( FATAL, UNSET, "Missing semiciolon at end of function data "
				"base `%s'.\n", Fname );
		THROW( EXCEPTION );
	}

	return num;
}


/*--------------------------------------------------------------------------*/
/* Function for comparing function names, used in sorting the function list */
/*--------------------------------------------------------------------------*/

static int ffl_func_cmp2( const void *a, const void *b )
{
	return strcmp( ( char * ) a, ( ( Func * ) b )->name );
}


/*------------------------------------------------------------*/
/* Function runs a second time through the function data base */
/* and tries to make sense from the entries                   */
/*------------------------------------------------------------*/

static void fll_get_functions( Func *fncts, int num_predef_func )
{
	int ret_token;
	int state;
	int cur;
	int act;
	int i;


	Lc = 1;
	Eol = SET;
	state = 0;
	act = cur = num_predef_func;

	while ( ( ret_token = func_listlex( ) ) != 0 )
	{
		switch ( ret_token )
		{
			case IDENT_TOKEN :
				if ( state != 0 )
				{
					eprint( FATAL, UNSET, "Syntax error at line %ld in "
							"function data base `%s'.\n", Lc, Fname );
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
							eprint( FATAL, UNSET, "Function `%s()' is declared"
									"more than once in function data base "
									"`%s'.\n", func_listtext, Fname );
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
					eprint( FATAL, UNSET, "Syntax error at line %ld of "
							"function data base `%s'.\n", Lc, Fname );
					THROW( EXCEPTION );
				}

				fncts[ act ].nargs = atol( func_listtext );
				state = 2;
				break;

			case ALL_TOKEN : case EXP_TOKEN : case PREP_TOKEN :
				if ( state != 2 )
				{
					eprint( FATAL, UNSET, "Syntax error at line %ld of "
							"function data base `%s'.\n", Lc, Fname );
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
					eprint( FATAL, UNSET, "Missing number of arguments for "
                            "function `%s' at line %ld in function data base "
                            "`%s'.\n", fncts[ act ].name, Lc, Fname );
					THROW( EXCEPTION );
				}
				state = 0;
				if ( act == cur )   /* "new" function ? */
					act = ++cur;
				else                /* built-in function has been overloaded */
					act = cur;
				break;

			default:                             /* this can never happen... */
				fsc2_assert( 1 == 0 );
				break;
		}
	}
}
