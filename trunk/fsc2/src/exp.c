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


#include "fsc2.h"


/* Number of program token structures to be allocated as a chunk */

#define PRG_CHUNK_SIZE 16384

/* Number of tokens to be parsed before forms are rechecked for user input -
   too small a number slows down the program quite a lot (and even moderately
   short EDL files may produce an appreciable number of tokens), while making
   it too large makes it difficult for the user to stop the interpreter. */

#define CHECK_FORMS_AFTER   8192


Token_Val exp_val;                        /* also used by exp_lexer.l */
static bool in_for_lex = UNSET;           /* set while handling for loop
											 condition part */
static long token_count;
static CB_Stack *cb_stack = NULL;         /* curly brace stack */

extern int exp_testparse( void );         /* from exp_parser.y */

extern int exp_runparse( void );          /* from exp_run_parser.y */
extern int conditionparse( void );        /* from condition_parser.y */

extern int explex( void );                /* from exp_lexer.flex */
extern FILE *expin;                       /* from exp_lexer.flex */

extern void exprestart( FILE *expin );


/* local functions */

static void push_curly_brace( const char *Fname, long Lc );
static bool pop_curly_brace( void );
static void loop_setup( void );
static void setup_while_or_repeat( int type, long *pos );
static void setup_if_else( long *pos, Prg_Token *cur_wr );
static void exp_syntax_check( void );
static void save_restore_variables( bool flag );


/*-----------------------------------------------------------------------------
  This routine stores the experiment section of an EDL file in the form of
  tokens (together with their semantic values if there are any) as returned
  by the lexer. This is necessary for two reasons: First, the experiment
  section may contain loops. Without having the EDL program stored internally
  it would be rather difficult to run through the loops since we would have to
  re-read and re-tokenise the input file again and again, which not only would
  be painfully slow but also difficult since what we read is not the real EDL
  file(s) but a pipe that contains what remains from the input files (there
  could be more than one when the #include directive has been used) after
  filtering through the program `fsc2_clean'. Second, we will have to run
  through the experiment section at least three times, first for syntax and
  sanity checks, then for the test run and finally for really doing the
  experiment. Third, handling of loops becomes rather simple because we only
  have to feed them to the parser again and again. Of course, beside storing
  the experiment section in tokenised form as done by this function, we also
  need routines that later pass the stored tokens to the parsers,
  exp_testlex() and exp_runlex().

  The function does the following:
  1. It stores each token (with the semantic values it receives from the
     lexer) in the array of program tokens, prg_token.
  2. While doing so it also does a few preliminay tests - it checks that
     opening and closing parentheses and braces match, BREAK and NEXT
	 statements only appear in loops and that THE ON_STOP statement isn't
	 in a block or within parentheses or braces.
  3. When done with storing the program it does everything needed to set up
     loops, if-else and unless-else constructs.
  4. Finally, a syntax check of the program is run. In this check the whole
     program is feed to a parser to test if any syntactic errors are found.
-----------------------------------------------------------------------------*/

void store_exp( FILE *in )
{
	static bool is_restart = UNSET;
	int ret;
	char *cur_Fname = NULL;
	long curly_brace_in_loop_count = 0;
	long parenthesis_count = 0;
	long square_brace_count = 0;
	bool in_loop = UNSET;


	/* Set input file */

	expin = in;

	/* Let's keep the lexer happy... */

	if ( is_restart )
	    exprestart( expin );
	else
		is_restart = SET;

	prg_token = NULL;

	/* Get and store all tokens */

	while ( ( ret = explex( ) ) != 0 )
	{
		if ( ret == ON_STOP_TOK )
		{
			if ( parenthesis_count > 0 )
			{
				eprint( FATAL, SET, "Unbalanced parentheses before ON_STOP "
						"label.\n" );
				THROW( EXCEPTION )
			}

			if ( square_brace_count > 0 )
			{
				eprint( FATAL, SET, "Unbalanced square braces before ON_STOP "
						"label.\n" );
				THROW( EXCEPTION )
			}

			if ( cb_stack != NULL )
			{
				eprint( FATAL, SET, "ON_STOP label found within a block.\n" );
				THROW( EXCEPTION )
			}

			On_Stop_Pos = prg_length;
			continue;
		}

		/* Get or extend memory for storing the tokens if necessary */

		if ( prg_length % PRG_CHUNK_SIZE == 0 )
			prg_token = T_realloc( prg_token, ( prg_length + PRG_CHUNK_SIZE )
								              * sizeof( Prg_Token ) );

		prg_token[ prg_length ].token = ret;   /* store token */

		/* If the file name changed get a copy of the new file name. Then set
		   the pointer in the program token structure to the file name and
		   copy the current line number. Don't free the name of the previous
		   file - all the program token structures from the previous file
		   point to the string, and it may only be free()ed when the program
		   token structures get free()ed. */

		if ( cur_Fname == NULL || strcmp( Fname, cur_Fname ) )
		{
			cur_Fname = NULL;
			cur_Fname = T_strdup( Fname );
		}

		prg_token[ prg_length ].Fname = cur_Fname;
		prg_token[ prg_length ].Lc = Lc;

		/* Initialise pointers needed for flow control and the data entries
		   used in repeat loops */

		prg_token[ prg_length ].start = prg_token[ prg_length ].end = NULL;
		prg_token[ prg_length ].counter = 0;

		/* In most cases it's enough just to copy the union with the value.
		   But there are a few exceptions: for strings we need also a copy of
		   the string since it's not persistent. Function tokens and also
		   variable references just push their data onto the variable stack,
		   so we have to copy them from the stack into the token structure.
		   Finally, we have to do some sanity checks on parentheses etc. */

		switch( ret )
		{
			case FOR_TOK : case WHILE_TOK : case UNLESS_TOK :
			case REPEAT_TOK : case FOREVER_TOK :
				in_loop = SET;
				break;

			case '(' :
				parenthesis_count++;
				break;

			case ')' :
				if ( --parenthesis_count < 0 )
				{
					eprint( FATAL, SET, "Found ')' without matching '('.\n" );
					THROW( EXCEPTION )
				}
				break;

			case '{' :
				if ( parenthesis_count != 0 )
				{
					eprint( FATAL, SET, "More '(' than ')` found before a "
							"'{'.\n" );
					THROW( EXCEPTION )
				}

				if ( square_brace_count != 0 )
				{
					eprint( FATAL, SET, "More '[' than ']` found before a "
							"'{'.\n" );
					THROW( EXCEPTION )
				}

				push_curly_brace( cur_Fname, Lc );
				if ( in_loop )
					curly_brace_in_loop_count++;
				break;

			case '}' :
				if ( parenthesis_count != 0 )
				{
					eprint( FATAL, SET, "More '(' than ')` found before a "
							"'}'.\n" );
					THROW( EXCEPTION )
				}

				if ( square_brace_count != 0 )
				{
					eprint( FATAL, SET, "More '[' than ']` found before a "
							"'}'.\n" );
					THROW( EXCEPTION )
				}

				if ( ! pop_curly_brace( ) )
				{
					eprint( FATAL, SET, "Found '}' without matching '{'.\n" );
					THROW( EXCEPTION )
				}

				if ( in_loop && --curly_brace_in_loop_count == 0 )
					in_loop = UNSET;
					
				break;

			case '[' :
				square_brace_count++;
				break;

			case ']' :
				if ( --square_brace_count < 0 )
				{
					eprint( FATAL, SET, "Found ']' without matching '['.\n" );
					THROW( EXCEPTION )
				}
				break;

				break;

			case E_STR_TOKEN :
				prg_token[ prg_length ].tv.sptr = NULL;
				prg_token[ prg_length ].tv.sptr = T_strdup( exp_val.sptr );
				break;

			case E_FUNC_TOKEN :
				prg_token[ prg_length ].tv.vptr = NULL;
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				prg_token[ prg_length ].tv.vptr->name = NULL;
				prg_token[ prg_length ].tv.vptr->name =
					                               T_strdup( Var_Stack->name );
				vars_pop( Var_Stack );
				break;

			case E_VAR_REF :
				prg_token[ prg_length ].tv.vptr = NULL;
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				vars_pop( Var_Stack );
				break;

			case ';' :
				if ( parenthesis_count != 0 )
				{
					eprint( FATAL, SET, "More '(' than ')` found at end of"
							"statement.\n" );
					THROW( EXCEPTION )
				}

				if ( square_brace_count != 0 )
				{
					eprint( FATAL, SET, "More '[' than ']` found at end of "
							"of statement.\n" );
					THROW( EXCEPTION )
				}
				break;

			case BREAK_TOK : case NEXT_TOK :
				if ( ! in_loop )
				{
					eprint( FATAL, SET, "%s statement not within a loop.\n",
							ret == BREAK_TOK ? "BREAK" : "NEXT" );
					THROW( EXCEPTION )
				}
				break;

			default :
				memcpy( &prg_token[ prg_length ].tv, &exp_val,
						sizeof( Token_Val ) );
		}

		prg_length++;
	}

	/* Check that all parentheses and braces are balanced */

	if ( parenthesis_count > 0 )
	{
		eprint( FATAL, UNSET, "'(' without closing ')' detected at end of "
				"program.\n" );
		THROW( EXCEPTION )
	}

	if ( cb_stack != NULL  )
	{
		eprint( FATAL, UNSET, "Block starting with '{' at %s:%ld has no "
				"closing '}'.\n", cb_stack->Fname, cb_stack->Lc );
		THROW( EXCEPTION )
	}

	if ( square_brace_count > 0 )
	{
		eprint( FATAL, UNSET, "'[' without closing ']' detected at end of "
				"program.\n" );
		THROW( EXCEPTION )
	}

	/* Check and initialise if's and loops */

	loop_setup( );

	/* Now we have to do a syntax check - some syntax errors might not be
	   found even when running the test because some IF or UNLESS conditions
	   never get triggered. It has also the advantage that syntax erors
	   will be found immediately instead after a long test run. */

	exp_syntax_check( );
}


/*-------------------------------------------------------------------------*/
/* Function is called for each opening curly brace found in the input file */
/* to store the current file name and line number and thus to be able to   */
/* print more informative error messages if the braces don't match up.     */
/*-------------------------------------------------------------------------*/

static void push_curly_brace( const char *Fname, long Lc )
{
	CB_Stack *new_cb;


	new_cb = T_malloc( sizeof( CB_Stack ) );
	new_cb->next = cb_stack;
	new_cb->Fname = T_strdup( Fname );
	new_cb->Lc = Lc;
	cb_stack = new_cb;
}


/*---------------------------------------------------------------------------*/
/* Function is called when a closing curly brace is found in the input file  */
/* to remove the entry on the curly brace stack for the corresponding        */
/* opening brace. It returns OK when there was a corresponding opening brace */
/* (i.e. opening and closing braces aren't unbalanced in favour of closing   */
/* braces), otherwise FAIL is returned. This function is also used when      */
/* getting rid of the curly brace stack after an exception was thrown.       */
/*---------------------------------------------------------------------------*/

static bool pop_curly_brace( void )
{
	CB_Stack *old_cb;


	if ( cb_stack == NULL )
		return FAIL;

	old_cb = cb_stack;
	cb_stack = old_cb->next;
	T_free( old_cb->Fname );
	T_free( old_cb );

	return OK;
}


/*----------------------------------------------------*/
/* Deallocates all memory used for storing the tokens */
/* (and their semantic values) of a program.          */
/*----------------------------------------------------*/

void forget_prg( void )
{
	char *cur_Fname;
	long i;


	On_Stop_Pos = -1;

	/* Check if anything has to be done at all */

	if ( prg_token == NULL )
	{
		prg_length = 0;
		return;
	}

	/* Get the address in the first token where the file name is stored and
	   free the string, then free memory allocated in the program token
	   structures. */

	cur_Fname = prg_token[ 0 ].Fname;
	T_free( cur_Fname );

	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case E_STR_TOKEN :
				T_free( prg_token[ i ].tv.sptr );
				break;
			
			case E_FUNC_TOKEN :       /* get rid of string for function name */
				T_free( prg_token[ i ].tv.vptr->name );
				/* fall through */

			case E_VAR_REF :          /* get rid of copy of stack variable */
				T_free( prg_token[ i ].tv.vptr );
				break;
		}

		/* If the file name changed we store the pointer to the new name and
           free the string */

		if ( prg_token[ i ].Fname != cur_Fname )
			T_free( cur_Fname = prg_token[ i ].Fname );
	}

	/* Get rid of the memory used for storing the tokens */

	T_free( prg_token );
	prg_token = NULL;
	prg_length = 0;

	/* Get rid of structures for curly braces that may have been survived
	   when an exception got thrown */

	while ( pop_curly_brace( ) )
		;
}


/*----------------------------------------------------------------*/
/* Sets up the blocks of WHILE, UNTIL, REPEAT and FOR loops and   */
/* IF-ELSE and UNLESS-ELSE constructs where a block is everything */
/* between a matching pair of curly braces.                       */
/*----------------------------------------------------------------*/

static void loop_setup( void )
{
	long i;
	long cur_pos;


	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case FOREVER_TOK :
				prg_token[ i ].counter = 1;
				/* fall through */

			case WHILE_TOK : case REPEAT_TOK :
			case FOR_TOK   : case UNTIL_TOK :
				cur_pos = i;
				setup_while_or_repeat( prg_token[ i ].token, &i );
				fsc2_assert( ! ( cur_pos < On_Stop_Pos && i > On_Stop_Pos ) );
				break;

			case IF_TOK : case UNLESS_TOK :
				cur_pos = i;
				setup_if_else( &i, NULL );
				fsc2_assert( ! ( cur_pos < On_Stop_Pos && i > On_Stop_Pos ) );
				break;
		}
	}
}


/*----------------------------------------------------------------*/
/* Does the real work for setting up while, repeat and for loops. */
/* Can be called recursively to allow nested loops.               */
/* ->                                                             */
/*  1. Type of loop (WHILE_TOK, UNTIL_TOK, REPEAT_TOK or FOR_TOK) */
/*  2. Pointer to number of token                                 */
/*----------------------------------------------------------------*/

static void setup_while_or_repeat( int type, long *pos )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	const char *t = NULL;


	/* Start of with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in EXPERIMENT "
				"section.\n", prg_token[ i - 1 ].Fname,
				prg_token[ i - 1 ].Lc );
		THROW( EXCEPTION )
	}

	if ( type != FOREVER_TOK && prg_token[ i ].token == '{' )
	{
		eprint( FATAL, UNSET, "%s:%ld: Missing loop condition.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( EXCEPTION )
	}

	if ( type == FOREVER_TOK && prg_token[ i ].token != '{' )
	{
		eprint( FATAL, UNSET, "%s:%ld: No condition can be used for a FOREVER "
				"loop.\n", prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( EXCEPTION )
	}

	/* Look for the start and end of the WHILE, UNTIL, REPEAT, FOR or FOREVER
	   block beginning with the first token after the keyword. Handle nested
	   WHILE, UNTIL, REPEAT, FOR, FOREVER, IF or UNLESS tokens by calling the
	   appropriate setup functions recursively. BREAK and NEXT tokens store
	   in `start' a pointer to the block header, i.e. the WHILE, UNTIL, REPEAT,
	   FOR or FOREVER token. */

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK :
			case FOR_TOK : case FOREVER_TOK :
			case UNTIL_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case NEXT_TOK :
				prg_token[ i ].start = cur;
				break;

			case BREAK_TOK :
				prg_token[ i ].start = cur;
				break;

			case IF_TOK : case UNLESS_TOK :
				setup_if_else( &i, cur );
				break;

			case ELSE_TOK :
				eprint( FATAL, UNSET, "%s:%ld: ELSE without IF/UNLESS in "
						"current block.\n", prg_token[ i ].Fname,
						prg_token[ i ].Lc );
				THROW( EXCEPTION )

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
							"EXPERIMENT section.\n", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION )
				}
				cur->start = &prg_token[ i + 1 ];
				break;

			case '}' :
				cur->end = &prg_token[ i + 1 ];
				prg_token[ i ].end = cur;
				*pos = i;
				return;
		}
	}

	if ( type == WHILE_TOK )
		t = "WHILE";
	if ( type == UNTIL_TOK )
		t = "UNTIL";
	if ( type == REPEAT_TOK )
		t = "REPEAT";
	if ( type == FOR_TOK )
		t = "FOR";
	if ( type == FOREVER_TOK )
		t = "FOREVER";
	
	if ( t == NULL)
		eprint( FATAL, UNSET, "Internal error at %s:%d.\n",
				__FILE__, __LINE__ );
	else
		eprint( FATAL, UNSET, "Missing `}' for %s loop starting at %s:%ld.\n",
				t, cur->Fname, cur->Lc );
	THROW( EXCEPTION )
}


/*----------------------------------------------------------------------*/
/* Does the real work for setting up IF-ELSE or UNLESS_ELSE constructs. */
/* Can be called recursively to allow nested IF and UNLESS tokens.      */
/* ->                                                                   */
/*    1. pointer to number of token                                     */
/*    2. Pointer to program token of enclosing while, repeat or for     */
/*       loop (needed for handling of `break' and `continue').          */
/*----------------------------------------------------------------------*/

static void setup_if_else( long *pos, Prg_Token *cur_wr )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	bool in_if = SET;
	bool dont_need_close_parens = UNSET;           /* set for IF-ELSE and 
													  UNLESS-ELSE constructs */


	/* Start with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in EXPERIMENT "
				"section.\n", prg_token[ i - 1 ].Fname,
				prg_token[ i - 1 ].Lc );
		THROW( EXCEPTION )
	}

	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, UNSET, "%s:%ld: Missing condition after IF/UNLESS.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
	}

	/* Now let's get things done... */

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK :
			case FOR_TOK : case FOREVER_TOK :
			case UNTIL_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case NEXT_TOK : case BREAK_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, UNSET, "%s:%ld: %s statement not within "
							"a loop.\n", prg_token[ i ].token == NEXT_TOK ?
							"NEXT" : "BREAK",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION )
				}
				prg_token[ i ].start = cur_wr;
				break;

			case IF_TOK : case UNLESS_TOK :
				setup_if_else( &i, cur_wr );

				if ( dont_need_close_parens )
				{
					if ( cur->end != NULL )
						( cur->end - 1 )->end = &prg_token[ i + 1 ];
					else
						cur->end = &prg_token[ i + 1 ];
					prg_token[ i ].end = &prg_token[ i + 1 ];
					*pos = i;
					return;
				}

				break;

			case ELSE_TOK :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
							"EXPERIMENT section.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION )
				}
				if ( prg_token[ i + 1 ].token != '{' &&
					 prg_token[ i + 1 ].token != IF_TOK &&
					 prg_token[ i + 1 ].token != UNLESS_TOK )
				{
					eprint( FATAL, UNSET, "%s:%ld: Missing '{' after ELSE.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION )
				}

				if ( in_if )
				{
					eprint( FATAL, UNSET, "Missing `}' for ELSE block "
							"belonging to IF-ELSE construct starting at "
							"%s:%ld.\n", cur->Fname, cur->Lc );
					THROW( EXCEPTION )
				}

				if (  prg_token[ i + 1 ].token == IF_TOK ||
					  prg_token[ i + 1 ].token == UNLESS_TOK )
				{
					cur->end = &prg_token[ i ];
					dont_need_close_parens = SET;
				}

				break;

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
							"EXPERIMENT section.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION )
				}
				if ( in_if )
					cur->start = &prg_token[ i + 1 ];
				else
					cur->end = &prg_token[ i - 1 ];
				break;

			case '}' :
				if ( in_if )
				{
					in_if = UNSET;
					if ( i + 1 < prg_length &&
						 prg_token[ i + 1 ].token == ELSE_TOK )
						break;
				}

				if ( cur->end != NULL )
					( cur->end - 1 )->end = &prg_token[ i + 1 ];
				else
					cur->end = &prg_token[ i + 1 ];
				prg_token[ i ].end = &prg_token[ i + 1 ];
				*pos = i;
				return;
		}
	}

	eprint( FATAL, UNSET, "Missing `}' for %s starting at %s:%ld.\n",
			in_if ? "IF/UNLESS" : "ELSE", cur->Fname, cur->Lc );
	THROW( EXCEPTION )
}


/*-------------------------------------------------------------------*/
/* This function is called to start the syntax test of the currently */
/* loaded EDL program. After initializing the pointer to the first   */
/* program token all it does is calling the test parser defined in   */
/* exp_test_parser.y. On syntax errors the test parser throws an     */
/* exception, otherwise this routine returns normally.               */
/*-------------------------------------------------------------------*/

static void exp_syntax_check( void )
{
	if ( prg_token == NULL )
		return;

	Fname = T_free( Fname );

	TRY
	{
		cur_prg_token = prg_token;
		exp_testparse( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		Fname = NULL;
		PASSTHROU( )
	}

	Fname = NULL;
}


/*-------------------------------------------------------------------*/
/* This function just feeds the stored program token by token to the */
/* syntax test parser exp_testparse(), defined in exp_test_parser.y. */
/* No actions are associated with this parsing stage, all that is    */
/* done is a test if the syntax of the program is ok.                */
/*-------------------------------------------------------------------*/

int exp_testlex( void )
{
	extern Token_Val exp_testlval;            /* from exp_test_parser.y */


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		Fname = cur_prg_token->Fname;
		Lc = cur_prg_token->Lc;
		memcpy( &exp_testlval, &cur_prg_token->tv,
				sizeof( Token_Val ) );
		return cur_prg_token++->token;
	}

	return 0;
}


/*-------------------------------------------------------------------*/
/* Function does the test run of the program. For most of the stored */
/* tokens of the EDL program it simply invokes the parser defined in */
/* exp_run_parser.y, which will then call the function exp_runlex()  */
/* to get further tokens. Exceptions are tokens that are related to  */
/* flow control that aren't handled by the parser but by this        */
/* function.                                                         */
/*-------------------------------------------------------------------*/

void exp_test_run( void )
{
	Prg_Token *cur;
	long old_FLL = File_List_Len;
	bool is_do_quit = UNSET;


	Fname = T_free( Fname );

	TRY
	{
		/* 1. Run the test run hook functions of the modules.
		   2. Save the variables so we can reset them to their initial values
		      when the test run is finished.
		   3. Call the function f_dtime() to initialize the delta time
		      counter.
		   4. Save the relevant values from the pulse structures.
		   5. Set up a signal handler so that it's possible to stop the test
		      if there are e.g. infinite loops.
		*/

		TEST_RUN = SET;
		save_restore_variables( SET );
		vars_pop( f_dtime( NULL ) );
		run_test_hooks( );

		cur_prg_token = prg_token;
		token_count = 0;

		while ( cur_prg_token != NULL &&
				cur_prg_token < prg_token + prg_length )
		{
			token_count++;

			/* Give the `Stop Test' button a chance to get tested... */

			if ( ! just_testing && token_count >= CHECK_FORMS_AFTER )
			{
				fl_check_only_forms( );
				token_count %= CHECK_FORMS_AFTER;
			}

			/* This will be set by the end() function which simulates
			   pressing the "Stop" button in the display window */

			if ( do_quit )
			{
				if ( On_Stop_Pos < 0 )                /* no ON_STOP part ? */
				{
					cur_prg_token = NULL;             /* -> stop immediately */
					break;
				}
				else                                  /* goto ON_STOP part */
					cur_prg_token = prg_token + On_Stop_Pos;

				do_quit = UNSET;
			}

			switch ( cur_prg_token->token )
			{
				case '}' :
					cur_prg_token = cur_prg_token->end;
					break;

				case WHILE_TOK :
					cur = cur_prg_token;
					if ( test_condition( cur ) )
					{
						cur->counter = 1;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case UNTIL_TOK :
					cur = cur_prg_token;
					if ( ! test_condition( cur ) )
					{
						cur->counter = 1;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case REPEAT_TOK :
					cur = cur_prg_token;
					if ( cur->counter == 0 )
						get_max_repeat_count( cur );
					if ( ++cur->count.repl.act <= cur->count.repl.max )
					{
						cur->counter++;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case FOR_TOK :
					cur = cur_prg_token;
					if ( cur->counter == 0 )
						get_for_cond( cur );
					if ( test_for_cond( cur ) )
					{
						cur->counter = 1;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case FOREVER_TOK :
					cur = cur_prg_token;          /* just test once ! */
					if ( cur->counter )
					{
						cur->counter = 0;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case BREAK_TOK :
					cur_prg_token->start->counter = 0;
					cur_prg_token = cur_prg_token->start->end;
					break;

				case NEXT_TOK :
					cur_prg_token = cur_prg_token->start;
					break;

				case IF_TOK : case UNLESS_TOK :
					cur = cur_prg_token;
					cur_prg_token
						       = test_condition( cur ) ? cur->start : cur->end;
					break;

				case ELSE_TOK :
					if ( ( cur_prg_token + 1 )->token == '{' )
						cur_prg_token += 2;
					else
						cur_prg_token++;
					break;

				default :
					exp_runparse( );               /* (re)start the parser */
					break;
			}
		}

		tools_clear( );
		run_end_of_test_hooks( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		tools_clear( );

		Fname = NULL;
		save_restore_variables( UNSET );
		
		File_List_Len = old_FLL;
		close_all_files( );
			
		TEST_RUN = UNSET;
		PASSTHROU( )
	}

	Fname = NULL;
	save_restore_variables( UNSET );
	File_List_Len = old_FLL;
	TEST_RUN = UNSET;
}


/*----------------------------------------------------------------*/
/* Routine works as a kind of virtual lexer by simply passing the */
/* parser the tokens that got stored while running store_exp().   */
/* The only exceptions are tokens dealing with flow control - for */
/* most of them the parser gets signaled an end of file, only the */
/* `}' is handled by the parser itself (but also as an EOF but    */
/* also doing some sanity checks).                                */
/*----------------------------------------------------------------*/

int exp_runlex( void )
{
	extern Token_Val exp_runlval;             /* from exp_run_parser.y */
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		token_count++;

		Fname = cur_prg_token->Fname;
		Lc = cur_prg_token->Lc;

		switch( cur_prg_token->token )
		{
			case WHILE_TOK : case REPEAT_TOK : case BREAK_TOK :
			case NEXT_TOK : case FOR_TOK : case FOREVER_TOK :
			case UNTIL_TOK : case IF_TOK : case UNLESS_TOK : case ELSE_TOK :
				return 0;

			case '}' :
				return '}';

			case E_STR_TOKEN :
				exp_runlval.sptr = cur_prg_token->tv.sptr;
				break;

			case E_FUNC_TOKEN :
				ret = vars_push( INT_VAR, 0 );
				from = ret->from;
				next = ret->next;
				prev = ret->prev;
				memcpy( ret, cur_prg_token->tv.vptr, sizeof( Var ) );
				ret->name = T_strdup( ret->name );
				ret->from = from;
				ret->next = next;
				ret->prev = prev;
				exp_runlval.vptr = ret;
				break;

			case E_VAR_REF :
				ret = vars_push( INT_VAR, 0 );
				from = ret->from;
				next = ret->next;
				prev = ret->prev;
				memcpy( ret, cur_prg_token->tv.vptr, sizeof( Var ) );
				ret->from = from;
				ret->next = next;
				ret->prev = prev;
				exp_runlval.vptr = ret;
				break;

			default :
				memcpy( &exp_runlval, &cur_prg_token->tv,
						sizeof( Token_Val ) );
				break;
		}

		return cur_prg_token++->token;
	}
	
	return 0;
}


/*-----------------------------------------------------------------------*/
/* This routines returns the tokens to the parser while the condition of */
/* a while, repeat or for or if is parsed.                               */
/*-----------------------------------------------------------------------*/

int conditionlex( void )
{
	extern Token_Val conditionlval;           /* from condition_parser.y */
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		token_count++;

		Fname = cur_prg_token->Fname;
		Lc = cur_prg_token->Lc;

		switch( cur_prg_token->token )
		{
			case '{' :                 /* always signifies end of condition */
				return 0;

			case ':' :                 /* separator in for loop condition */
				if ( in_for_lex )
					return 0;
				eprint( FATAL, SET, "Syntax error in condition at token "
						"`:'.\n" );
				THROW( EXCEPTION )

			case E_STR_TOKEN :
				conditionlval.sptr = cur_prg_token->tv.sptr;
				cur_prg_token++;
				return E_STR_TOKEN;

			case E_FUNC_TOKEN :
				ret = vars_push( INT_VAR, 0 );
				from = ret->from;
				next = ret->next;
				prev = ret->prev;
				memcpy( ret, cur_prg_token->tv.vptr, sizeof( Var ) );
				ret->name = T_strdup( ret->name );
				ret->from = from;
				ret->next = next;
				ret->prev = prev;
				conditionlval.vptr = ret;
				cur_prg_token++;
				return E_FUNC_TOKEN;

			case E_VAR_REF :
				ret = vars_push( INT_VAR, 0 );
				from = ret->from;
				next = ret->next;
				prev = ret->prev;
				memcpy( ret, cur_prg_token->tv.vptr, sizeof( Var ) );
				ret->from = from;
				ret->next = next;
				ret->prev = prev;
				conditionlval.vptr = ret;
				cur_prg_token++;
				return E_VAR_REF;

			case '=' :
				eprint( FATAL, SET, "For comparisons `==' must be used "
						"(`=' is for assignments only).\n", Fname, Lc );
				THROW( EXCEPTION )

			default :
				memcpy( &conditionlval, &cur_prg_token->tv,
						sizeof( Token_Val ) );
				token = cur_prg_token->token;
				cur_prg_token++;
				return token;
		}
	}

	return 0;
}


/*-----------------------------------------------------------------*/
/* Function tests the condition of a WHILE, UNTIL or IF or UNLESS. */
/*-----------------------------------------------------------------*/

bool test_condition( Prg_Token *cur )
{
	bool condition;
	bool neg_cond = UNSET;


	cur_prg_token++;                          /* skip the WHILE or IF etc. */
	conditionparse( );                        /* get the value */
	fsc2_assert( Var_Stack->next == NULL );   /* Paranoia as usual... */
	fsc2_assert( cur_prg_token->token == '{' );

	/* Make sure returned value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		const char *t = NULL;

		if ( cur->token == WHILE_TOK )
			t = "WHILE loop";
		if ( cur->token == UNTIL_TOK )
			t = "UNTIL loop";
		if ( cur->token == REPEAT_TOK )
			t = "REPEAT loop";
		if ( cur->token == FOR_TOK )
			t = "FOR loop";
		if ( cur->token == IF_TOK )
			t = "IF construct";
		if ( cur->token == UNLESS_TOK )
		{
			t = "UNLESS construct";
			neg_cond = SET;
		}

		cur++;
		if ( t == NULL )
			eprint( FATAL, UNSET, "Internal error at %s:%d.\n",
					__FILE__, __LINE__ );
		else
			eprint( FATAL, UNSET, "%s:%ld: Invalid condition for %s.\n",
					cur->Fname, cur->Lc, t );
		THROW( EXCEPTION )
	}

	/* Test the result - everything nonzero returns OK */

	if ( Var_Stack->type == INT_VAR )
		condition = Var_Stack->val.lval ? OK : FAIL;
	else
		condition = Var_Stack->val.dval ? OK : FAIL;

	if ( neg_cond )
		condition = condition == OK ? FAIL : OK;

	vars_pop( Var_Stack );
	return condition;
}


/*---------------------------------------------------------*/
/* Functions determines the repeat count for a repeat loop */
/*---------------------------------------------------------*/

void get_max_repeat_count( Prg_Token *cur )
{
	cur_prg_token++;                         /* skip the REPEAT token */
	conditionparse( );                       /* get the value */
	fsc2_assert( Var_Stack->next == NULL );  /* Paranoia as usual... */

	/* Make sure the repeat count is either int or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		eprint( FATAL, UNSET, "%s:%ld: Invalid counter for REPEAT loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* Set the repeat count - warn if value is float an convert to integer */

	if ( Var_Stack->type == INT_VAR )
		cur->count.repl.max = Var_Stack->val.lval;
	else
	{
		eprint( WARN, UNSET, "%s:%ld: WARNING: Floating point value used as "
				"maximum count in REPEAT loop.\n",
				( cur + 1 )->Fname, ( cur + 1 )->Lc );
		cur->count.repl.max = ( long ) Var_Stack->val.dval;
	}

	vars_pop( Var_Stack );
	cur->count.repl.act = 0;
	cur_prg_token++;                   /* skip the `{' */
	return;
}


/*--------------------------------------------------------------------------*/
/* Function gets all parts of a for loop condition, i.e. the loop variable, */
/* the start value assigned to the loop variable, the end value and and op- */
/* tional increment value. If the loop variable is an integer also the end  */
/* and increment variable have to be integers.                              */
/*--------------------------------------------------------------------------*/

void get_for_cond( Prg_Token *cur )
{
	/* First of all get the loop variable */

	cur_prg_token++;             /* skip the FOR keyword */

	/* Make sure token is a variable and next token is `=' */

	if ( cur_prg_token->token != E_VAR_TOKEN ||
		 ( cur_prg_token + 1 )->token != '=' )
	{
		cur++;
		eprint( FATAL, UNSET, "%s:%ld: Syntax error in condition of FOR "
				"loop.\n", cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* If loop variable is new set its type */

	if ( cur_prg_token->tv.vptr->type == UNDEF_VAR )
	{
		cur_prg_token->tv.vptr->type =
			IF_FUNC( cur_prg_token->tv.vptr->name ) ? INT_VAR : FLOAT_VAR;
		cur_prg_token->tv.vptr->flags &= ~ NEW_VARIABLE;
	}

	/* Make sure the loop variable is either an integer or a float */

	if ( ! ( cur_prg_token->tv.vptr->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		eprint( FATAL, UNSET, "%s:%ld: FOR loop variable must be integer or "
				"float variable.\n", cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* Store pointer to loop variable */

	cur->count.forl.act = cur_prg_token->tv.vptr;

	/* Now get start value to be assigned to loop variable */

	cur_prg_token +=2;                        /* skip variable and `=' token */
	in_for_lex = SET;                         /* allow `:' as separator */
	conditionparse( );                        /* get start value */
	fsc2_assert( Var_Stack->next == NULL );   /* Paranoia as usual... */

	/* Make sure there is at least one more token, i.e. the loops end value */

	if ( cur_prg_token->token != ':' )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, UNSET, "%s:%ld: Missing end value in FOR loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* Make sure the returned value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, UNSET, "%s:%ld: Invalid start value in FOR loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* Set start value of loop variable */

	if ( Var_Stack->type == INT_VAR )
	{
		if ( cur->count.forl.act->type == INT_VAR )
			cur->count.forl.act->val.lval = Var_Stack->val.lval;
		else
			cur->count.forl.act->val.dval = ( double ) Var_Stack->val.lval;
	}
	else
	{
		if ( cur->count.forl.act->type == INT_VAR )
		{
			eprint( WARN, UNSET, "%s:%ld: Using floating point value in "
					"assignment to integer FOR loop variable %s.\n",
					( cur + 1 )->Fname,
					( cur + 1 )->Lc, cur->count.forl.act->name );
			cur->count.forl.act->val.lval = ( long ) Var_Stack->val.dval;
		}
		else
			cur->count.forl.act->val.dval = Var_Stack->val.dval;
	}

	vars_pop( Var_Stack );

	/* Get for loop end value */

	cur_prg_token++;                           /* skip the `:' */
	conditionparse( );                         /* get end value */
	fsc2_assert( Var_Stack->next == NULL );    /* Paranoia as usual... */

	/* Make sure end value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, UNSET, "%s:%ld: Invalid end value in FOR loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* If loop variable is integer `end' must also be integer */

	if ( cur->count.forl.act->type == INT_VAR && Var_Stack->type == FLOAT_VAR )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, UNSET, "%s:%ld: End value in FOR loop is floating "
				"point value while loop variable is an integer.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION )
	}

	/* Set end value of loop */

	cur->count.forl.end.type = Var_Stack->type;
	if ( Var_Stack->type == INT_VAR )
		cur->count.forl.end.lval = Var_Stack->val.lval;
	else
		cur->count.forl.end.dval = Var_Stack->val.dval;

	vars_pop( Var_Stack );

	/* Set the increment */

	if ( cur_prg_token->token != ':' )   /* no increment given, default to 1 */
	{
		if ( cur->count.forl.act->type == INT_VAR )
		{
			cur->count.forl.incr.type = INT_VAR;
			cur->count.forl.incr.lval = 1;
		}
		else
		{
			cur->count.forl.incr.type = FLOAT_VAR;
			cur->count.forl.incr.dval = 1.0;
		}

	}
	else                                        /* get for loop increment */
	{	
		cur_prg_token++;                        /* skip the `:' */
		conditionparse( );                      /* get end value */
		in_for_lex = UNSET;
		fsc2_assert( Var_Stack->next == NULL ); /* Paranoia as usual... */

		/* Make sure the increment is either an integer or a float */

		if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			cur++;
			eprint( FATAL, UNSET, "%s:%ld: Invalid increment for FOR loop.\n",
					cur->Fname, cur->Lc );
			THROW( EXCEPTION )
		}

		/* If loop variable is an integer, `incr' must also be integer */

		if ( cur->count.forl.act->type == INT_VAR &&
			 Var_Stack->type == FLOAT_VAR )
		{
			cur++;
			in_for_lex = UNSET;
			eprint( FATAL, UNSET, "%s:%ld: FOR loop increment is floating "
					"point value while loop variable is an integer.\n",
					cur->Fname, cur->Lc );
			THROW( EXCEPTION )
		}

		cur->count.forl.incr.type = Var_Stack->type;

		/* Check that increment isn't zero */

		if ( Var_Stack->type == INT_VAR )
		{
			if ( Var_Stack->val.lval == 0 )
			{
				cur++;
				eprint( FATAL, UNSET, "%s:%ld: Zero increment in FOR loop.\n",
						cur->Fname, cur->Lc );
				THROW( EXCEPTION )
			}
			cur->count.forl.incr.lval = Var_Stack->val.lval;
		}
		else
		{
			if ( Var_Stack->val.dval == 0 )
			{
				cur++;
				eprint( FATAL, UNSET, "%s:%ld: Zero increment for FOR loop.\n",
						cur->Fname, cur->Lc );
				THROW( EXCEPTION )
			}
			cur->count.forl.incr.dval = Var_Stack->val.dval;
		}

		vars_pop( Var_Stack );
	}

	in_for_lex = UNSET;
	cur_prg_token++;                /* skip the `{' */
	return;

}


/*--------------------------------------------------*/
/* Function tests the loop condition of a for loop. */
/*--------------------------------------------------*/

bool test_for_cond( Prg_Token *cur )
{
	bool sign = UNSET;


	/* If this isn't the very first call, increment the loop variable */

	if ( cur->counter != 0 )
	{
		if ( cur->count.forl.act->type == INT_VAR )
				cur->count.forl.act->val.lval += cur->count.forl.incr.lval;
		else
			cur->count.forl.act->val.dval += 
				cur->count.forl.incr.type == INT_VAR ?
					cur->count.forl.incr.lval : cur->count.forl.incr.dval;
	}

	/* Get sign of increment */
	
	if ( ( cur->count.forl.incr.type == INT_VAR &&
		   cur->count.forl.incr.lval < 0 ) ||
		 ( cur->count.forl.incr.type == FLOAT_VAR &&
		   cur->count.forl.incr.dval < 0 ) )
		sign = SET;

	/* If the increment is positive test if loop variable is less or equal to
	   the end value, if increment is negative if loop variable is larger or
	   equal to the end value. Return the result. */

	if ( cur->count.forl.act->type == INT_VAR )
	{
		if ( ! sign )     /* `incr' has positive sign */
		{
			if ( cur->count.forl.act->val.lval <= cur->count.forl.end.lval )
				return OK;
			else
				return FAIL;
		}
		else              /* `incr' has negative sign */
		{
			if ( cur->count.forl.act->val.lval >= cur->count.forl.end.lval )
				return OK;
			else
				return FAIL;
		}
	}
	else
	{
		if ( ! sign )     /* `incr' has positive sign */
		{
			if ( cur->count.forl.act->val.dval <= 
				   ( cur->count.forl.end.type == INT_VAR ?
			            cur->count.forl.end.lval : cur->count.forl.end.dval ) )
				return OK;
			else
				return FAIL;
		}
		else              /* `incr' has negative sign */
		{
			if ( cur->count.forl.act->val.dval >=
				   ( cur->count.forl.end.type == INT_VAR ?
				        cur->count.forl.end.lval : cur->count.forl.end.dval ) )
				return OK;
			else
				return FAIL;
		}
	}

	/* We better never end here... */

	eprint( FATAL, UNSET, "Internal error at %s:%d.\n", __FILE__, __LINE__ );
	THROW( EXCEPTION )

	return FAIL;
}


/*--------------------------------------------------------------------------*/
/* Functions saves or restores all variables depending on `flag'. If `flag' */
/* is set the variables are saved, otherwise they are copied back from the  */
/* backup into the normal variables space. Don't use the saved variables -  */
/* the internal pointers are not adjusted. Currently, for string variables  */
/* the string is not saved but just the pointer to the string but since     */
/* they should never change this shouldn't be a problem.                    */
/* Never ever change the memory locations of the variables between saving   */
/* and restoring !!!                                                        */
/*--------------------------------------------------------------------------*/

static void save_restore_variables( bool flag )
{
	Var *src;
	Var *cpy;
	static Var *var_list_copy = NULL;      /* area for storing variables    */
	static Var *old_var_list;              /* needed to satisfy my paranoia */
	long var_count;                        /* number of variables to save   */


	if ( flag )
	{
		fsc2_assert( var_list_copy == NULL );      /* don't save twice ! */

		/* Count the number of variables and get memory for storing them */

		for ( var_count = 0, src = var_list; src != NULL;
			  var_count++, src = src->next )
			;
		var_list_copy = T_malloc( var_count * sizeof( Var ) );

		/* Copy all of them into the backup region */

		for ( src = var_list, cpy = var_list_copy; src != NULL;
			  cpy++, src = src->next )
		{
			memcpy( cpy, src, sizeof( Var ) );

			if ( cpy->type == INT_CONT_ARR && ! ( cpy->flags & NEED_ALLOC ) )
			{
				src->val.lpnt = NULL;
				src->val.lpnt =
					  get_memcpy( cpy->val.lpnt, cpy->len * sizeof( long ) );
			}
			if ( cpy->type == FLOAT_CONT_ARR && ! ( cpy->flags & NEED_ALLOC ) )
			{
				src->val.dpnt = NULL;
				src->val.dpnt =
					  get_memcpy( cpy->val.dpnt, cpy->len * sizeof( double ) );
			}

			if ( cpy->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) &&
				 cpy->sizes != NULL )
			{
				src->sizes = NULL;
				src->sizes
					      = get_memcpy( cpy->sizes, cpy->dim * sizeof( int ) );
			}
		}

		old_var_list = var_list;
	}
	else
	{
		fsc2_assert( var_list_copy != NULL );   /* no restore without save ! */
		fsc2_assert( var_list == old_var_list );   /* just a bit paranoid... */

		/* Get rid of memory for arrays that might have been changed during
		   the test */

		for ( cpy = var_list; cpy != NULL; cpy = cpy->next )
		{
			if ( cpy->type == INT_CONT_ARR && ! ( cpy->flags & NEED_ALLOC ) &&
				 cpy->val.lpnt != NULL )
				T_free( cpy->val.lpnt );
			if ( cpy->type == FLOAT_CONT_ARR &&
				 ! ( cpy->flags & NEED_ALLOC ) &&
				 cpy->val.dpnt != NULL )
				T_free( cpy->val.dpnt );
			if ( cpy->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) &&
				 cpy->sizes != NULL )
				T_free( cpy->sizes );
		}

		for ( src = var_list_copy, cpy = var_list; cpy != NULL;
			  cpy = src->next, src++  )
			memcpy( cpy, src, sizeof( Var ) );

		/* Deallocate memory used in arrays */

		T_free( var_list_copy );
		var_list_copy = NULL;
	}
}
