/*
  $Id$
*/

#include "fsc2.h"
#include <signal.h>


static bool in_for_lex = UNSET;

extern int prim_exp_runparse( void );     /* from prim_exp__run_parser.y */
extern int conditionparse( void );        /* from condition_parser.y */


Token_Val prim_exp_val;                   /* exported to prim_exp_lexer.flex */

extern int prim_explex( void );           /* from prim_exp_lexer.flex */
extern FILE *prim_expin;                  /* from prim_exp_lexer.flex */
extern Token_Val prim_exp_runlval;        /* from prim_exp__run_parser.y */
extern Token_Val conditionlval;           /* from condition_parser.y */

extern char *Fname;
extern long Lc;
static long var_count = 0;

static volatile bool Stop_Signal = UNSET;

extern void prim_exprestart( FILE *prim_expin );


/* local functions */

static void prim_loop_setup( void );
static void setup_while_or_repeat( int type, long *pos );
static void setup_if_else( long *pos, Prg_Token *cur_wr );
static void save_restore_variables( bool flag );

static void set_stop_signal_handler( bool flag );
static void test_stopper( int sig );



/*
   This routine stores the experiment section of an EDL file in the form of
   tokens (together with their semantic values if there is one) as returned by
   the lexer. Thus it serves as a kind of intermediate between the lexer and
   the parser. This is necessary for two reasons: First, the experiment
   section may contain loops. Without an intermediate it would be rather
   difficult to run through the loops since we would have to re-read and
   re-tokenize the input file again and again, which not only would be slow
   but also difficult since what we read as input file is actually a pipe from
   the `cleaner'. Second, we will have to run through the experiment section
   at least two times, first for syntax and sanity checks and then again for
   really doing the experiment. Again, without an intermediate for storing the
   tokenized form of the experiment section this would necessitate reading the
   input files at least two times.

   By having an intermediate between the lexer and the parser we avoid several
   problems. We don't have to re-read and re-tokenize the input file. We also
   can find out about loops and handle them later by simply feeding the parser
   the loop again and again. Of course, beside storing the experiment section
   in tokenized form as done by this function, we also need a routine that
   later feeds the stored tokens to the parser(s).
*/


void store_exp( FILE *in )
{
	static bool is_restart = UNSET;
	int ret;
	char *cur_Fname = NULL;


	/* set input file */

	prim_expin = in;

	/* Keep the lexer happy... */

	if ( is_restart )
	    prim_exprestart( prim_expin );
	else
		 is_restart = SET;

	prg_token = NULL;

	/* get and store all tokens */

	while ( ( ret = prim_explex( ) ) != 0 )
	{
		/* get or extend memory used for storing the tokens in chunks */

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
					get_string_copy( prim_exp_val.sptr );
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
				memcpy( &prg_token[ prg_length ].tv, &prim_exp_val,
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

		/* Initialize pointers needed for flow control and the data entries
		   used in repeat loops */

		prg_token[ prg_length ].start = prg_token[ prg_length ].end = NULL;
		prg_token[ prg_length ].counter = 0;
		prg_length++;
	}

	/* check and initialize if's and loops */

	prim_loop_setup( );
}

/*------------------------------------------------------------------------*/
/* Deallocates all memory used for storing the tokens (and their semantic */
/* values) of a program.                                                  */
/*------------------------------------------------------------------------*/

void forget_prg( void )
{
	char *cur_Fname;
	long i;


	/* check if anything has to be done at all */

	if ( prg_token == NULL )
	{
		prg_length = 0;
		return;
	}

	/* get the address in the first token where the file names is stored and
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

		/* if the file name changed we store the pointer to the new name and
           free the string */

		if ( prg_token[ i ].Fname != cur_Fname )
			T_free( cur_Fname = prg_token[ i ].Fname );
	}

	/* get rid of the memory used for storing the tokens */

	T_free( prg_token );
	prg_token = NULL;
	prg_length = 0;
}


/*--------------------------------------------------------------------------*/
/* Sets up the blocks of while, repeat and for loops and if-else constructs */
/* where a block is everything a between matching pair of curly braces.     */
/*--------------------------------------------------------------------------*/

void prim_loop_setup( void )
{
	long i;


	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK : case FOR_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case IF_TOK :
				setup_if_else( &i, NULL );
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

void setup_while_or_repeat( int type, long *pos )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	const char *t;


	/* Start of with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing loop condition.\n",
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
				eprint( FATAL, "%s:%ld: ELSE without IF in current block.\n",
						prg_token[ i ].Fname, prg_token[ i ].Lc );
				THROW( EXCEPTION );

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
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
	
	eprint( FATAL, "Missing `}' for %s loop starting at %s:%ld.\n",
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

void setup_if_else( long *pos, Prg_Token *cur_wr )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	bool in_if = SET;
	bool dont_need_close_paran = UNSET;      /* set for ELSE IF constructs */


	/* Start of with some sanity checks */

	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing condition after IF.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
	}

	for ( ; i < prg_length; i++ )
	{
		if ( Stop_Signal )
			THROW( EXCEPTION );

		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK : case FOR_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case CONT_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: CONTINUE not within WHILE, REPEAT "
							"or FOR loop.\n", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				prg_token[ i ].start = cur_wr;
				break;

			case BREAK_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: BREAK not within WHILE, REPEAT "
							"or FOR loop.\n", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				prg_token[ i ].start = cur_wr;
				break;

			case IF_TOK :
				setup_if_else( &i, cur_wr );

				if ( dont_need_close_paran )
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
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}
				if ( prg_token[ i + 1 ].token != '{' &&
					 prg_token[ i + 1 ].token != IF_TOK )
				{
					eprint( FATAL, "%s:%ld: Missing '{' after ELSE.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( EXCEPTION );
				}

				if ( in_if )
				{
					eprint( FATAL, "Missing `}' for ELSE block belonging to "
							"IF-ELSE construct starting at %s:%ld.\n",
							cur->Fname, cur->Lc );
					THROW( EXCEPTION );
				}


				if(  prg_token[ i + 1 ].token == IF_TOK )
				{
					cur->end = &prg_token[ i ];
					dont_need_close_paran = SET;
				}

				break;

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
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

	eprint( FATAL, "Missing `}' for %s starting at %s:%ld.\n",
			in_if ? "IF" : "ELSE", cur->Fname, cur->Lc );
	THROW( EXCEPTION );
}


/*--------------------------------------------*/
/* Function does the test run of the program. */
/*--------------------------------------------*/

void prim_exp_run( void )
{
	Prg_Token *cur;

	if ( Fname != NULL )
		T_free( Fname );
	Fname = NULL;

	if ( prg_length == 0 )                       /* no program no test... */
		return;

	TRY
	{
		/* 1. Run the test run hook functions of the modules.
		   2. Save the variables so we can reset them to their intial values
		      when the test run is completed.
		   3. Save the relevant values from the pulse structures.
		   3. Set up a signal handler so that it's possible to stop the test
		      if there are e.g. infinite loops.
		*/

		TEST_RUN = SET;
		save_restore_variables( SET );
		save_restore_pulses( SET );
		set_stop_signal_handler( SET );
		run_test_hooks( );

		cur_prg_token = prg_token;

		while ( cur_prg_token != NULL &&
				cur_prg_token < prg_token + prg_length )
		{
			if ( Stop_Signal )
				THROW( EXCEPTION );

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
					prim_exp_runparse( );           /* (re)start the parser */
					break;
			}
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		set_stop_signal_handler( UNSET );
		Fname = NULL;
		TEST_RUN = UNSET;
		PASSTHROU( );
	}

	set_stop_signal_handler( UNSET );
	Fname = NULL;
	save_restore_pulses( UNSET );
	save_restore_variables( UNSET );
	TEST_RUN = UNSET;
}


/*-------------------------------------------------------------------*/
/* Routine works as a kind of virtual lexer by simply passing the    */
/* parser the tokens we got from the lexer in store_exp(). The only  */
/* exception are tokens dealing with flow control - for most of them */
/* parser gets an end of file signaled, onyl the `}' is handled by   */
/* the parser itself (but also as an EOF)                            */
/*-------------------------------------------------------------------*/

int prim_exp_runlex( void )
{
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		if ( Stop_Signal )
			THROW( EXCEPTION );

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
				prim_exp_runlval.sptr = cur_prg_token->tv.sptr;
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
				prim_exp_runlval.vptr = ret;
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
				prim_exp_runlval.vptr = ret;
				cur_prg_token++;
				return E_VAR_REF;

			default :
				memcpy( &prim_exp_runlval, &cur_prg_token->tv,
						sizeof( Token_Val ) );
				token = cur_prg_token->token;
				cur_prg_token++;
				return token;
		}
	}
	
	return 0;
}


/*------------------------------------------------------------------------*/
/* This routines returnes the tokens to the parser while the condition of */
/* a while, repeat, for or if is parsed.                                  */
/*------------------------------------------------------------------------*/

int conditionlex( void )
{
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		if ( Stop_Signal )
			THROW( EXCEPTION );

		Fname = cur_prg_token->Fname;
		Lc = cur_prg_token->Lc;

		switch( cur_prg_token->token )
		{
			case '{' :                 /* always signifies end of condition */
				return 0;

			case ':' :                 /* separator in for loop condition */
				if ( in_for_lex )
					return 0 ;
				eprint( FATAL, "%s:%ld: Syntax error in condition at "
						"token `:'.\n", Fname, Lc  );
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
				eprint( FATAL, "%s:%ld: For comparisions `==' must be used "
						"(`=' is for assignments only).\n", Fname, Lc );
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
		const char *t;

		if ( cur->token == WHILE_TOK )
			t = "WHILE loop";
		if ( cur->token == REPEAT_TOK )
			t = "REPEAT loop";
		if ( cur->token == FOR_TOK )
			t = "FOR loop";
		if ( cur->token == IF_TOK )
			t = "IF construct";

		cur++;
		eprint( FATAL, "%s:%ld: Invalid condition for %s.\n",
				cur->Fname, cur->Lc, t );
		THROW( EXCEPTION );
	}
			 
	/* Test the result - erverything nonzero returns OK */

	if ( Var_Stack->type == INT_VAR )
		condition = Var_Stack->val.lval ? OK : FAIL;
	else
		condition = Var_Stack->val.dval ? OK : FAIL;

	vars_pop( Var_Stack );
	return( condition );
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
		eprint( FATAL, "%s:%ld: Invalid counter for REPEAT loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* Set the repeat count - warn if value is float an convert to integer */

	if ( Var_Stack->type == INT_VAR )
		cur->count.repl.max = Var_Stack->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: WARNING: Floating point value used as maximum "
				"count in REPEAT loop.\n",
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
		eprint( FATAL, "%s:%ld: Syntax error in condition of FOR loop.\n",
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

	/* Make sure loop variable is either integer or float */

	if ( ! ( cur_prg_token->tv.vptr->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		eprint( FATAL, "%s:%ld: FOR loop variable must be integer or float "
				"variable.\n", cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* store pointer to loop variable */

	cur->count.forl.act = cur_prg_token->tv.vptr;

	/* Now get start value to be assigned to loop variable */

	cur_prg_token +=2;                  /* skip variable and `=' token */
	in_for_lex = SET;                   /* allow `:' as separator */
	conditionparse( );                  /* get start value */
	assert( Var_Stack->next == NULL );  /* Paranoia as usual... */

	/* Make sure there is at least one more token for the end loop value */

	if ( cur_prg_token->token != ':' )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: Missing end value for FOR loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* Make sure the returned value is either integer or float */

	if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: Invalid start value in FOR loop.\n",
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
					"to integer FOR loop variable %s.\n", ( cur + 1 )->Fname,
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
		eprint( FATAL, "%s:%ld: Invalid end value for FOR loop.\n",
				cur->Fname, cur->Lc );
		THROW( EXCEPTION );
	}

	/* If loop variable is integer `end' must also be integer */

	if ( cur->count.forl.act->type == INT_VAR && Var_Stack->type == FLOAT_VAR )
	{
		cur++;
		in_for_lex = UNSET;
		eprint( FATAL, "%s:%ld: End value of FOR loop is floating point "
				"value while loop variable is an integer.\n",
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
	else                                    /* get for loop incr value */
	{	
		cur_prg_token++;                      /* skip the `:' */
		conditionparse( );                    /* get end value */
		in_for_lex = UNSET;
		assert( Var_Stack->next == NULL );    /* Paranoia as usual... */

		/* Make sure increment is either inetger or float */

		if ( ! ( Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			cur++;
			eprint( FATAL, "%s:%ld: Invalid increment for FOR loop.\n",
					cur->Fname, cur->Lc );
			THROW( EXCEPTION );
		}

		/* If loop variable is integer `incr' must also be integer */

		if ( cur->count.forl.act->type == INT_VAR &&
			 Var_Stack->type == FLOAT_VAR )
		{
			cur++;
			in_for_lex = UNSET;
			eprint( FATAL, "%s:%ld: FOR loop increment is floating point "
					"value while loop variable is an integer.\n",
				cur->Fname, cur->Lc );
			THROW( EXCEPTION );
		}

		cur->count.forl.incr.type = Var_Stack->type;

		/* Check that increent isn't zero */

		if ( Var_Stack->type == INT_VAR )
		{
			if ( Var_Stack->val.lval == 0 )
			{
				cur++;
				eprint( FATAL, "%s:%ld: Zero increment for FOR loop.\n",
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
				eprint( FATAL, "%s:%ld: Zero increment for FOR loop.\n",
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

	/* get sign of increment */
	
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
	static Var *old_var_list;


	assert( Var_Stack == NULL );

	if ( flag )
	{
		assert( var_list_copy == NULL );          /* don't save twice ! */

		/* count the number of variables and get memory for storing them */

		if ( var_count == 0 )
			for ( var_count = 0, src = var_list; src != NULL;
				  var_count++, src = src->next )
				;
		prg_token[ prg_length ].tv.vptr = NULL;
		var_list_copy = T_malloc( var_count * sizeof( Var ) );

		/* copy all of them into the backup region */

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

			if ( cpy->type & ( INT_ARR | FLOAT_ARR ) )
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
		assert( var_list_copy != NULL );    /* don't restore without save ! */
		assert( var_list == old_var_list );       /* just a bit paranoid... */

		/* get rid of memory for arrays that might have been changed during
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

		/* deallocate memory used in arrays */

		T_free( var_list_copy );
		var_list_copy = NULL;
	}
}


/*----------------------------------------------------------------------*/
/* This takes care of deleting the copy of the stored variables when an */
/* exception happens while the test run is underway (or even if we ran  */
/* out of memory while we made the copy. (free_vars() will take care of */
/* the variables themselves.)                                           */
/*----------------------------------------------------------------------*/

void delete_var_list_copy( void )
{
	Var *cpy;
	long i;


	if ( var_list_copy == NULL )
		return;

	for ( cpy = var_list_copy, i = 0; i < var_count ; i++, cpy++ )
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

	T_free( var_list_copy );
	var_list_copy = NULL;
}


/*------------------------------------------------------*/
/* Functions sets up a handler for SIGHUP signals which */
/* are used for user breaks.                            */
/* ->                                                   */
/*    Flag, if set install signal handler, otherwise    */
/*    reinstall the signal the default signal handler.  */
/*------------------------------------------------------*/

void set_stop_signal_handler( bool flag )
{
	static void ( * old_sig_handler ) ( int ) = NULL;

	if ( flag )
		old_sig_handler = signal( SIGHUP, test_stopper );
	else
	{
		if ( old_sig_handler != NULL )
			signal( SIGHUP, old_sig_handler );
	}

	if ( Stop_Signal )
		eprint( FATAL, "Program test aborted, received user break.\n" );

	Stop_Signal = UNSET;
}


/*-----------------------------------------------------------------------*/
/* Signal handler for SIGHUP signals just sets a global variable, which  */
/* has to be evaluated again and again. While just throwing an exception */
/* would look much nicer, it would require all functions used in the     */
/* program to be written in a way to allow asynchronous breaks at every  */
/* moment. And this would be rather difficult if not even impossible     */
/* if the routines have to deal with real world devices.                 */
/*-----------------------------------------------------------------------*/

void test_stopper( int sig )
{
	if ( sig != SIGHUP )
		return;

	Stop_Signal = SET;
}
	

/*###########################################################################*/

/* Left in if necessary later on... */

void loop_dbg( void );
void loop_dbg( void )
{
	Prg_Token *cur = prg_token;


	for ( ; cur < prg_token + prg_length; cur++ )
	{
		switch ( cur->token )
		{
			case WHILE_TOK :
				printf( "%d: WHILE, start = %d, end = %d\n",
						cur - prg_token, cur->start - prg_token,
						cur->end - prg_token );
				break;

			case FOR_TOK :
				printf( "%d: FOR, start = %d, end = %d\n",
						cur - prg_token, cur->start - prg_token,
						cur->end - prg_token );
				break;


			case REPEAT_TOK :
				printf( "%d: REPEAT, start = %d, end = %d\n",
						cur - prg_token, cur->start - prg_token,
						cur->end - prg_token );
				break;

			case CONT_TOK :
				printf( "%d: CONTINUE, start = %d\n",
						cur - prg_token, cur->start - prg_token );
				break;

			case BREAK_TOK :
				printf( "%d: CONTINUE, start = %d\n",
						cur - prg_token, cur->start - prg_token );
				break;

			case IF_TOK :
				printf( "%d: IF, start = %d, end = %d\n",
						cur - prg_token, cur->start - prg_token,
						cur->end - prg_token );
				break;

			case ELSE_TOK :
				printf( "%d ELSE\n",
						cur - prg_token );
				break;

			case '}' :
				printf( "%d: `}', end = %d\n",
						cur - prg_token, cur->end - prg_token );
				break;
		}
	}
}
