/*
  $Id$
*/

#include "fsc2.h"
#include <signal.h>


/* Number of program token structures to be allocated as a chunk */

#define PRG_CHUNK_SIZE 16384

/* Number of tokens to be parsed before forms are rechecked for user input -
   too low a number slows down the program quite a lot and even relatively
   short EDL files with loops may produce an appreciable number of tokens,
   while setting it to too large a value will make it difficult for the user
   to stop the interpreter! */

#define CHECK_FORMS_AFTER   65536


static bool in_for_lex = UNSET;   // set while handling for loop condition part
static long token_count;


extern int exp_runparse( void );          /* from exp_run_parser.y */
extern int conditionparse( void );        /* from condition_parser.y */


Token_Val exp_val;                        /* exported to exp_lexer.flex */

extern int explex( void );                /* from exp_lexer.flex */
extern FILE *expin;                       /* from exp_lexer.flex */
extern Token_Val exp_runlval;             /* from exp_run_parser.y */
extern Token_Val conditionlval;           /* from condition_parser.y */

extern char *Fname;
extern long Lc;

extern void exprestart( FILE *expin );


/* local functions */

static void loop_setup( void );
static void setup_while_or_repeat( int type, long *pos );
static void setup_if_else( long *pos, Prg_Token *cur_wr );



/*-----------------------------------------------------------------------------
   This routine stores the experiment section of an EDL file in the form of
   tokens (together with their semantic values if there are any) as returned
   by the lexer. Thus it serves as a kind of intermediate between the lexer
   and the parser. This is necessary for two reasons: First, the experiment
   section may contain loops. Without an intermediate it would be rather
   difficult to run through the loops since we would have to re-read and
   re-tokenise the input file again and again, which not only would be
   painfully slow but also difficult since what we read is not the real EDL
   file(s) but a pipe that contains what remains after filtering through the
   program `fsc2_clean'. Second, we will have to run through the experiment
   section at least two times, first for syntax and sanity checks and then
   again for really doing the experiment. Again, without an intermediate for
   storing the tokenised form of the experiment section this would necessitate
   reading the input files at least two times.

   By having an intermediate between the lexer and the parser we avoid several
   problems. We don't have to re-read and re-tokenise the input file. We also
   can find out about loops and handle them later by simply feeding the parser
   the loop again and again. Of course, beside storing the experiment section
   in tokenised form as done by this function, we also need a routine that
   later feeds the stored tokens to the parser(s) - exp_runlex().
-----------------------------------------------------------------------------*/


void store_exp( FILE *in )
{
	static bool is_restart = UNSET;
	int ret;
	char *cur_Fname = NULL;


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
			On_Stop_Pos = prg_length;
			continue;
		}

		/* Get or extend memory for storing the tokens in chunks */

		if ( prg_length % PRG_CHUNK_SIZE == 0 )
			prg_token = T_realloc( prg_token, ( prg_length + PRG_CHUNK_SIZE )
								              * sizeof( Prg_Token ) );

		prg_token[ prg_length ].token = ret;   /* store token */

		/* In most cases it's enough just to copy the union with the value.
		   But there are a few exceptions: For strings we need also a copy of
		   the string since it's not persistent. Function tokens and also
		   variable references just push their data onto the variable stack,
		   so we have to copy them from the stack into the token structure */

		switch( ret )
		{
			case E_STR_TOKEN :
				prg_token[ prg_length ].tv.sptr =
					get_string_copy( exp_val.sptr );
				break;

			case E_FUNC_TOKEN :
				prg_token[ prg_length ].tv.vptr = NULL;
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				prg_token[ prg_length ].tv.vptr->name =
					get_string_copy( Var_Stack->name );
				vars_pop( Var_Stack );
				break;

			case E_VAR_REF :
				prg_token[ prg_length ].tv.vptr = NULL;
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				vars_pop( Var_Stack );
				break;

			default :
				memcpy( &prg_token[ prg_length ].tv, &exp_val,
						sizeof( Token_Val ) );
		}

		/* If the file name changed get a copy of the new file name. Then set
		   pointer in structure to file name and copy current line number. */

		if ( cur_Fname == NULL || strcmp( Fname, cur_Fname ) )
		{
			cur_Fname = NULL;
			cur_Fname = get_string_copy( Fname );
		}
		prg_token[ prg_length ].Fname = cur_Fname;
		prg_token[ prg_length ].Lc = Lc;

		/* Initialise pointers needed for flow control and the data entries
		   used in repeat loops */

		prg_token[ prg_length ].start = prg_token[ prg_length ].end = NULL;
		prg_token[ prg_length ].counter = 0;
		prg_length++;
	}

	/* Check and initialise if's and loops */

	loop_setup( );
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

	/* Get the address in the first token where the file names is stored and
	   free the string */

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
				                                             /* NO break ! */
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
}


/*--------------------------------------------------------------------------*/
/* Sets up the blocks of while, repeat and for loops and if-else constructs */
/* where a block is everything a between matching pair of curly braces.     */
/*--------------------------------------------------------------------------*/

static void loop_setup( void )
{
	long i;
	long cur_pos;


	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK : case FOR_TOK :
				cur_pos = i;
				setup_while_or_repeat( prg_token[ i ].token, &i );
				if ( cur_pos < On_Stop_Pos && i > On_Stop_Pos )
				{
					eprint( FATAL, "ON_QUIT label is located within a loop." );
					THROW( EXCEPTION );
				}
				break;

			case IF_TOK :
				cur_pos = i;
				setup_if_else( &i, NULL );
				if ( cur_pos < On_Stop_Pos && i > On_Stop_Pos )
				{
					eprint( FATAL, "ON_QUIT label is located within an "
							"if-else construct." );
					THROW( EXCEPTION );
				}
				break;
		}
	}
}


/*----------------------------------------------------------------*/
/* Does the real work for setting up while, repeat and for loops. */
/* Can be called recursively to allow nested loops.               */
/* ->                                                             */
/*    1. Type of loop (WHILE_TOK, REPEAT_TOK or FOR_TOK)          */
/*    2. pointer to number of token                               */
/*----------------------------------------------------------------*/

static void setup_while_or_repeat( int type, long *pos )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	const char *t = NULL;


	/* Start of with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file in EXPERIMENT "
				"section.", prg_token[ i - 1 ].Fname,
				prg_token[ i - 1 ].Lc );
		THROW( EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing loop condition.",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( EXCEPTION );
	}

	/* Look for the start and end of the while, repeat or for block
	   beginning with the first token after the while, repeat or for. Handle
	   nested while, repeat, for or if tokens by calling the appropriate
	   setup functions recursively. break and continue tokens store in `start'
	   a pointer to the block header, i.e. the while, repeat or for token. */

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK : case FOR_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case CONT_TOK :
				prg_token[ i ].start = cur;
				break;

			case BREAK_TOK :
				prg_token[ i ].start = cur;
				break;

			case IF_TOK :
				setup_if_else( &i, cur );
				break;

			case ELSE_TOK :
				eprint( FATAL, "%s:%ld: ELSE without IF in current block.",
						prg_token[ i ].Fname, prg_token[ i ].Lc );
				THROW( EXCEPTION );

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file in "
							"EXPERIMENT section.", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION );
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
	if ( type == REPEAT_TOK )
		t = "REPEAT";
	if ( type == FOR_TOK )
		t = "FOR";
	
	if ( t == NULL)
		eprint( FATAL, "Internal error at %s:%d.", __FILE__, __LINE__ );
	else
		eprint( FATAL, "Missing `}' for %s loop starting at %s:%ld.",
				t, cur->Fname, cur->Lc );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------------------*/
/* Does the real work for setting up if-else constructs. Can be called */
/* recursively to allow nested if's.                                   */
/* ->                                                                  */
/*    1. pointer to number of token                                    */
/*    2. Pointer to program token of enclosing while, repeat or for    */
/*       loop (needed for handling of `break' and `continue').         */
/*---------------------------------------------------------------------*/

static void setup_if_else( long *pos, Prg_Token *cur_wr )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	bool in_if = SET;
	bool dont_need_close_parans = UNSET;      /* set for ELSE IF constructs */


	/* Start with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file in EXPERIMENT "
				"section.", prg_token[ i - 1 ].Fname,
				prg_token[ i - 1 ].Lc );
		THROW( EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing condition after IF.",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
	}

	/* Now let's get things done... */

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK : case FOR_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case CONT_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: NEXT not within WHILE, REPEAT "
							"or FOR loop.", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				prg_token[ i ].start = cur_wr;
				break;

			case BREAK_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: BREAK not within WHILE, REPEAT "
							"or FOR loop.", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				prg_token[ i ].start = cur_wr;
				break;

			case IF_TOK :
				setup_if_else( &i, cur_wr );

				if ( dont_need_close_parans )
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
					eprint( FATAL, "%s:%ld: Unexpected end of file in "
							"EXPERIMENT section.",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				if ( prg_token[ i + 1 ].token != '{' &&
					 prg_token[ i + 1 ].token != IF_TOK )
				{
					eprint( FATAL, "%s:%ld: Missing '{' after ELSE.",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}

				if ( in_if )
				{
					eprint( FATAL, "Missing `}' for ELSE block belonging to "
							"IF-ELSE construct starting at %s:%ld.",
							cur->Fname, cur->Lc );
					THROW( EXCEPTION );
				}


				if(  prg_token[ i + 1 ].token == IF_TOK )
				{
					cur->end = &prg_token[ i ];
					dont_need_close_parans = SET;
				}

				break;

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file in "
							"EXERIMENT section.",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION );
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

	eprint( FATAL, "Missing `}' for %s starting at %s:%ld.",
			in_if ? "IF" : "ELSE", cur->Fname, cur->Lc );
	THROW( EXCEPTION );
}


/*--------------------------------------------*/
/* Function does the test run of the program. */
/*--------------------------------------------*/

void exp_test_run( void )
{
	Prg_Token *cur;
	long old_FLL = File_List_Len;

	if ( Fname != NULL )
		T_free( Fname );
	Fname = NULL;

	TRY
	{
		/* 1. Run the test run hook functions of the modules.
		   2. Save the variables so we can reset them to their initial values
		      when the test run is finished.
		   3. Save the relevant values from the pulse structures.
		   3. Set up a signal handler so that it's possible to stop the test
		      if there are e.g. infinite loops.
		*/

		TEST_RUN = SET;
		save_restore_variables( SET );
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

			switch ( cur_prg_token->token )
			{
				case '}' :
					cur_prg_token = cur_prg_token->end;
					break;

				case WHILE_TOK :
					cur = cur_prg_token;
					if ( test_condition( cur ) )
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
						cur->counter++;
						cur_prg_token = cur->start;
					}
					else
					{
						cur->counter = 0;
						cur_prg_token = cur->end;
					}
					break;

				case BREAK_TOK :
					cur_prg_token = cur_prg_token->start->end;
					break;

				case CONT_TOK :
					cur_prg_token = cur_prg_token->start;
					break;

				case IF_TOK :
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

		run_end_of_test_hooks( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		Fname = NULL;
		save_restore_variables( UNSET );
		
		File_List_Len = old_FLL;
		close_all_files( );
			
		TEST_RUN = UNSET;
		PASSTHROU( );
	}


	Fname = NULL;
	save_restore_variables( UNSET );
	File_List_Len = old_FLL;
	TEST_RUN = UNSET;
}


/*-------------------------------------------------------------------*/
/* Routine works as a kind of virtual lexer by simply passing the    */
/* parser the tokens we stored while running store_exp(). The only   */
/* exception are tokens dealing with flow control - for most of them */
/* the parser gets signalled an end of file, only the `}' is handled */
/* by the parser itself (but also as an EOF).                        */
/*-------------------------------------------------------------------*/

int exp_runlex( void )
{
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		token_count++;

		Fname = cur_prg_token->Fname;
		Lc = cur_prg_token->Lc;

		switch( cur_prg_token->token )
		{
			case WHILE_TOK : case REPEAT_TOK : case BREAK_TOK :
			case CONT_TOK : case FOR_TOK   : case IF_TOK : case ELSE_TOK :
				return 0;

			case '}' :
				return '}';

			case E_STR_TOKEN :
				exp_runlval.sptr = cur_prg_token->tv.sptr;
				cur_prg_token++;
				return E_STR_TOKEN;

			case E_FUNC_TOKEN :
				ret = vars_push( INT_VAR, 0 );
				from = ret->from;
				next = ret->next;
				prev = ret->prev;
				memcpy( ret, cur_prg_token->tv.vptr, sizeof( Var ) );
				ret->name = get_string_copy( ret->name );
				ret->from = from;
				ret->next = next;
				ret->prev = prev;
				exp_runlval.vptr = ret;
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
				exp_runlval.vptr = ret;
				cur_prg_token++;
				return E_VAR_REF;

			default :
				memcpy( &exp_runlval, &cur_prg_token->tv,
						sizeof( Token_Val ) );
				token = cur_prg_token->token;
				cur_prg_token++;
				return token;
		}
	}
	
	return 0;
}


/*-----------------------------------------------------------------------*/
/* This routines returns the tokens to the parser while the condition of */
/* a while, repeat or for or if is parsed.                               */
/*-----------------------------------------------------------------------*/

int conditionlex( void )
{
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
				eprint( FATAL, "%s:%ld: Syntax error in condition at "
						"token `:'.", Fname, Lc  );
				THROW( EXCEPTION );

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
				ret->name = get_string_copy( ret->name );
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
				eprint( FATAL, "%s:%ld: For comparisons `==' must be used "
						"(`=' is for assignments only).", Fname, Lc );
				THROW( EXCEPTION );

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


/*------------------------------------------------*/
/* Function tests the condition of a while or if. */
/*------------------------------------------------*/

bool test_condition( Prg_Token *cur )
{
	bool condition;


	cur_prg_token++;                     /* skip the WHILE or IF */
	conditionparse( );                   /* get the value */
	assert( Var_Stack->next == NULL );   /* Paranoia as usual... */
	assert( cur_prg_token->token == '{' );

	/* Make sure returned value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		const char *t = NULL;

		if ( cur->token == WHILE_TOK )
			t = "WHILE loop";
		if ( cur->token == REPEAT_TOK )
			t = "REPEAT loop";
		if ( cur->token == FOR_TOK )
			t = "FOR loop";
		if ( cur->token == IF_TOK )
			t = "IF construct";

		cur++;
		if ( t == NULL )
			eprint( FATAL, "Internal error at %s:%d.", __FILE__, __LINE__ );
		else
			eprint( FATAL, "%s:%ld: Invalid condition for %s.",
					cur->Fname, cur->Lc, t );
		THROW( EXCEPTION );
	}
			 
	/* Test the result - everything nonzero returns OK */

	if ( Var_Stack->type == INT_VAR )
		condition = Var_Stack->val.lval ? OK : FAIL;
	else
		condition = Var_Stack->val.dval ? OK : FAIL;

	vars_pop( Var_Stack );
	return condition;
}


/*---------------------------------------------------------*/
/* Functions determines the repeat count for a repeat loop */
/*---------------------------------------------------------*/

void get_max_repeat_count( Prg_Token *cur )
{
	cur_prg_token++;                    /* skip the REPEAT token */
	conditionparse( );                  /* get the value */
	assert( Var_Stack->next == NULL );  /* Paranoia as usual... */

	/* Make sure the repeat count is either int or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		eprint( FATAL, "%s:%ld: Invalid counter for REPEAT loop.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* Set the repeat count - warn if value is float an convert to integer */

	if ( Var_Stack->type == INT_VAR )
		cur->count.repl.max = Var_Stack->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: WARNING: Floating point value used as maximum "
				"count in REPEAT loop.",
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
		eprint( FATAL, "%s:%ld: Syntax error in condition of FOR loop.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
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
		eprint( FATAL, "%s:%ld: FOR loop variable must be integer or float "
				"variable.", cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* Store pointer to loop variable */

	cur->count.forl.act = cur_prg_token->tv.vptr;

	/* Now get start value to be assigned to loop variable */

	cur_prg_token +=2;                    /* skip variable and `=' token */
	in_for_lex = SET;                     /* allow `:' as separator */
	conditionparse( );                    /* get start value */
	assert( Var_Stack->next == NULL );    /* Paranoia as usual... */

	/* Make sure there is at least one more token, i.e. the loops end value */

	if ( cur_prg_token->token != ':' )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: Missing end value in FOR loop.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* Make sure the returned value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: Invalid start value in FOR loop.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
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
			eprint( WARN, "%s:%ld: Using floating point value in assignment "
					"to integer FOR loop variable %s.", ( cur + 1 )->Fname,
					( cur + 1 )->Lc, cur->count.forl.act->name );
			cur->count.forl.act->val.lval = ( long ) Var_Stack->val.dval;
		}
		else
			cur->count.forl.act->val.dval = Var_Stack->val.dval;
	}

	vars_pop( Var_Stack );

	/* Get for loop end value */

	cur_prg_token++;                      /* skip the `:' */
	conditionparse( );                    /* get end value */
	assert( Var_Stack->next == NULL );    /* Paranoia as usual... */

	/* Make sure end value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: Invalid end value in FOR loop.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* If loop variable is integer `end' must also be integer */

	if ( cur->count.forl.act->type == INT_VAR && Var_Stack->type == FLOAT_VAR )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: End value in FOR loop is floating point "
				"value while loop variable is an integer.",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
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
		assert( Var_Stack->next == NULL );      /* Paranoia as usual... */

		/* Make sure the increment is either an integer or a float */

		if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			cur++;
			eprint( FATAL, "%s:%ld: Invalid increment for FOR loop.",
					cur->Fname, cur->Lc );
			THROW( EXCEPTION );
		}

		/* If loop variable is an integer, `incr' must also be integer */

		if ( cur->count.forl.act->type == INT_VAR &&
			 Var_Stack->type == FLOAT_VAR )
		{
			cur++;
			in_for_lex = UNSET;
			eprint( FATAL, "%s:%ld: FOR loop increment is floating point "
					"value while loop variable is an integer.",
					cur->Fname, cur->Lc );
			THROW( EXCEPTION );
		}

		cur->count.forl.incr.type = Var_Stack->type;

		/* Check that increment isn't zero */

		if ( Var_Stack->type == INT_VAR )
		{
			if ( Var_Stack->val.lval == 0 )
			{
				cur++;
				eprint( FATAL, "%s:%ld: Zero increment in FOR loop.",
						cur->Fname, cur->Lc );
				THROW( EXCEPTION );
			}
			cur->count.forl.incr.lval = Var_Stack->val.lval;
		}
		else
		{
			if ( Var_Stack->val.dval == 0 )
			{
				cur++;
				eprint( FATAL, "%s:%ld: Zero increment for FOR loop.",
						cur->Fname, cur->Lc );
				THROW( EXCEPTION );
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
				( cur->count.forl.incr.type == INT_VAR ?
				       cur->count.forl.incr.lval : cur->count.forl.incr.dval );
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

	assert( 1 == 0 );
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

void save_restore_variables( bool flag )
{
	Var *src;
	Var *cpy;
	static Var *var_list_copy = NULL;      /* area for storing variables */
	static Var *old_var_list;              /* needed to satisfy my paranoia */
	long var_count;                        /* number of variables to save */


	if ( flag )
	{
		assert( var_list_copy == NULL );           /* don't save twice ! */

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

			if ( cpy->type == INT_ARR && ! ( cpy->flags & NEED_ALLOC ) )
			{
				src->val.lpnt = NULL;
				src->val.lpnt =
					  get_memcpy( cpy->val.lpnt, cpy->len * sizeof( long ) );
			}
			if ( cpy->type == FLOAT_ARR && ! ( cpy->flags & NEED_ALLOC ) )
			{
				src->val.dpnt = NULL;
				src->val.dpnt =
					  get_memcpy( cpy->val.dpnt, cpy->len * sizeof( double ) );
			}

			if ( cpy->type & ( INT_ARR | FLOAT_ARR ) && cpy->sizes != NULL )
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
		assert( var_list_copy != NULL );     /* don't restore without save ! */
		assert( var_list == old_var_list );  /* just being a bit paranoid... */

		/* Get rid of memory for arrays that might have been changed during
		   the test */

		for ( cpy = var_list; cpy != NULL; cpy = cpy->next )
		{
			if ( cpy->type == INT_ARR && ! ( cpy->flags & NEED_ALLOC ) &&
				 cpy->val.lpnt != NULL )
				T_free( cpy->val.lpnt );
			if ( cpy->type == FLOAT_ARR && ! ( cpy->flags & NEED_ALLOC ) &&
				 cpy->val.dpnt != NULL )
				T_free( cpy->val.dpnt );
			if ( cpy->type & ( INT_ARR | FLOAT_ARR ) && cpy->sizes != NULL )
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
