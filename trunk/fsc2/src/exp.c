/*
  $Id$
*/
void loop_dbg( void );


#include "fsc2.h"

extern int prim_exp_runparse( void );
extern int conditionparse( void );


Token_Val prim_exp_val;

extern int prim_explex( void );
extern FILE *prim_expin;
extern Token_Val prim_exp_runlval;
extern Token_Val conditionlval;


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
	int ret;
	char *cur_Fname = NULL;


	/* forget all about last run */

	forget_prg( );

	/* set input file */

	prim_expin = in;

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
		   so we have to move them from the stack into the token structure */

		switch( ret )
		{
			case E_STR_TOKEN :
				prg_token[ prg_length ].tv.sptr =
					get_string_copy( prim_exp_val.sptr );
				break;

			case E_FUNC_TOKEN :
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				vars_pop( Var_Stack );

				prg_token[ prg_length ].tv.vptr->name =
					get_string_copy( prg_token[ prg_length ].tv.vptr->name );
				break;

			case E_VAR_REF :
				prg_token[ prg_length ].tv.vptr = T_malloc( sizeof( Var ) );
				memcpy( prg_token[ prg_length ].tv.vptr, Var_Stack,
						sizeof( Var ) );
				vars_pop( Var_Stack );
				break;

			default :
				memcpy( &prg_token[ prg_length ].tv, &prim_exp_val,
						sizeof( Token_Val ) );
		}

		/* If the file name changed get a copy of the new name. Then set
		   pointer in structure to file name and copy curent line number. */

		if ( cur_Fname == NULL || strcmp( Fname, cur_Fname ) )
			cur_Fname = get_string_copy( Fname );
		prg_token[ prg_length ].Fname = cur_Fname;
		prg_token[ prg_length ].Lc = Lc;

		/* Initialize pointers needed for flow control and the data entries
		   used in repeat loops */

		prg_token[ prg_length ].start = prg_token[ prg_length ].end = NULL;
		prg_token[ prg_length ].max_count
			= prg_token[ prg_length ].act_count = 0;

		prg_length++;
	}

	/* check and initialize if's and loops */

	prim_loop_setup( );

	loop_dbg( );
}


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
				printf( "%d: CONTINUE, end = %d\n",
						cur - prg_token, cur->end - prg_token );
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

	/* get the address where the first file names is stored and free the
	   string */

	cur_Fname = prg_token[ 0 ].Fname;
	free( cur_Fname );

	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case E_STR_TOKEN :
				free( prg_token[ i ].tv.sptr );
				break;
			
			case E_FUNC_TOKEN :       /* get rid of string for function name */
				free( prg_token[ i ].tv.vptr->name );
				                                             /* NO break ! */
			case E_VAR_REF :          /* get rid of copy of stack variable */
				free( prg_token[ i ].tv.vptr );
				break;
		}

		/* if the file name changed we store the pointer to the new name and
           free the string */

		if ( prg_token[ i ].Fname != cur_Fname )
			free( cur_Fname = prg_token[ i ].Fname );
	}

	/* get rid of the memory used for storing the tokens */

	free( prg_token );
	prg_token = NULL;
	prg_length = 0;
}


void prim_loop_setup( void )
{
	long i;


	/* set up blocks for while and repeat loops and if-else constructs */

	for ( i = 0; i < prg_length; ++i )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case IF_TOK :
				setup_if_else( prg_token[ i ].token, &i, NULL );
				break;
		}
	}

	/* break's still have stored a reference to the while or repeat they
       belong to, change this to a reference to the statement following the
       while or repeat block */

	for ( i = 0; i < prg_length; ++i )
		if ( prg_token[ i ].token == BREAK_TOK )
			prg_token[ i ].end = prg_token[ i ].end->end;

}


void setup_while_or_repeat( int type, long *pos )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;


	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( BLOCK_ERROR_EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing loop condition.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( BLOCK_ERROR_EXCEPTION );
	}

	/* Look for the start and end of the while or repeat block beginning with
	   the first token after the while or repeat. Handle nested while, repeat
	   or if tokens by calling the appropriate function recursively. break and
	   continue tokens store a pointer to the block header, i.e. the while or
	   repeat token (for break tokens the pointers later will be changed to
	   point to the first statement following the block) */

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case CONT_TOK :
				prg_token[ i ].start = cur;
				break;

			case BREAK_TOK :
				prg_token[ i ].end = cur;
				break;

			case IF_TOK :
				setup_if_else( prg_token[ i ].token, &i, cur );
				break;

			case ELSE_TOK :
				eprint( FATAL, "%s:%ld: ELSE without IF in current block.\n",
						prg_token[ i ].Fname, prg_token[ i ].Lc,
						type == WHILE_TOK ? "while" : "repeat");
				THROW( BLOCK_ERROR_EXCEPTION );

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
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

	eprint( FATAL, "Missing `}' for %s loop starting at %s:%ld.\n",
			type == WHILE_TOK ? "WHILE" : "REPEAT", cur->Fname, cur->Lc );
	THROW( BLOCK_ERROR_EXCEPTION );
}


void setup_if_else( int type, long *pos, Prg_Token *cur_wr )
{
	Prg_Token *cur = prg_token + *pos;
	long i = *pos + 1;
	bool in_if = SET;


	if ( i == prg_length )
	{
		eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
		THROW( BLOCK_ERROR_EXCEPTION );
	}
	if ( prg_token[ i ].token == '{' )
	{
		eprint( FATAL, "%s:%ld: Missing condition after IF.\n",
				prg_token[ i ].Fname, prg_token[ i ].Lc );
	}

	for ( ; i < prg_length; i++ )
	{
		switch( prg_token[ i ].token )
		{
			case WHILE_TOK : case REPEAT_TOK :
				setup_while_or_repeat( prg_token[ i ].token, &i );
				break;

			case CONT_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: CONTINUE not within WHILE or "
							"REPEAT loop.\n", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
				}
				prg_token[ i ].start = cur_wr;
				break;

			case BREAK_TOK :
				if ( cur_wr == NULL )
				{
					eprint( FATAL, "%s:%ld: BREAK not within WHILE or REPEAT "
							"loop.\n", prg_token[ i ].Fname,
							prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
				}
				prg_token[ i ].end = cur_wr;
				break;

			case IF_TOK :
				setup_if_else( prg_token[ i ].token, &i, cur_wr );
				break;

			case ELSE_TOK :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
				}
				if ( prg_token[ i + 1 ].token != '{' )
				{
					eprint( FATAL, "%s:%ld: Missing '{' after ELSE.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
				}
				break;

			case '{' :
				if ( i + 1 == prg_length )
				{
					eprint( FATAL, "%s:%ld: Unexpected end of file.\n",
							prg_token[ i ].Fname, prg_token[ i ].Lc );
					THROW( BLOCK_ERROR_EXCEPTION );
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

	eprint( FATAL, "Missing `}' for %s at %s:%ld.\n", in_if ? "IF" : "ELSE",
			cur->Fname, cur->Lc );
	THROW( BLOCK_ERROR_EXCEPTION );
}


void prim_exp_run( void )
{
	Prg_Token *cur;
	long i;


	cur_prg_token = prg_token;

	while ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		switch ( cur_prg_token->token )
		{
			case '}' :
				cur_prg_token = cur_prg_token->end;
				break;

			case WHILE_TOK :
				cur = cur_prg_token;
				cur_prg_token = test_condition( cur ) ? cur->start : cur->end;
				break;

			case REPEAT_TOK :
				cur = cur_prg_token;
				if ( cur->act_count == 0 )
					get_max_repeat_count( cur );
				else
				{
					if ( ++cur->act_count <= cur->max_count )
						cur_prg_token = cur->start;
					else
						cur_prg_token = cur->end;
				}
				break;

			case BREAK_TOK :
				cur_prg_token = cur_prg_token->end;
				break;

			case CONT_TOK :
				cur_prg_token = cur_prg_token->start;
				break;

			case IF_TOK :
				cur = cur_prg_token;
				cur_prg_token = test_condition( cur ) ? cur->start : cur->end;
				break;

			case ELSE_TOK :
				cur_prg_token += 2;
				break;

			default :
				prim_exp_runparse( );
				break;
		}
	}

	/* reset the repeat counter */

	for ( i = 0; i < prg_length; i++ )
		if ( prg_token[ i ].token == REPEAT_TOK )
			prg_token[ i ].act_count = 0;
}


int prim_exp_runlex( void )
{
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		switch( cur_prg_token->token )
		{
			case WHILE_TOK : case REPEAT_TOK : case BREAK_TOK : case CONT_TOK :
			case IF_TOK :    case ELSE_TOK :
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

int conditionlex( void )
{
	int token;
	Var *ret, *from, *next, *prev;


	if ( cur_prg_token != NULL && cur_prg_token < prg_token + prg_length )
	{
		switch( cur_prg_token->token )
		{
			case '{' :
				return '{';

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


bool test_condition( Prg_Token *cur )
{
	bool condition;


	cur_prg_token++;             /* skip the keyword */
	conditionparse( );
	assert( Var_Stack->next == NULL );
	assert( cur_prg_token->token == '{' );

	if ( ! ( Var_Stack->type == INT_VAR ) )
	{
		const char *t1 = "WHILE loop",
			       *t2 = "REPEAT loop",
		           *t3 = "IF construct";
		const char *t;

		if ( cur->token == WHILE_TOK )
			t = t1;
		if ( cur->token == REPEAT_TOK )
			t = t2;
		if ( cur->token == IF_TOK )
			t = t3;

		cur++;
		eprint( FATAL, "%s:%ld: Invalid condition for %s.\n",
				cur->Fname, cur->Lc, t3 );
		THROW( CONDITION_EXCEPTION );
	}
			 
	condition = Var_Stack->val.lval ? OK : FAIL;
	vars_pop( Var_Stack );
	return( condition );
}


void get_max_repeat_count( Prg_Token *cur )
{
	cur_prg_token++;             /* skip the keyword */
	conditionparse( );
	assert( Var_Stack->next == NULL );
	assert( cur_prg_token->token == '{' );

	if ( ! ( Var_Stack->type & ( INT_VAR || FLOAT_VAR ) ) )
	{
		cur++;
		eprint( FATAL, "%s:%ld: Invalid counter for REPEAT loop.\n",
				cur->Fname, cur->Lc );
		THROW( CONDITION_EXCEPTION );
	}

	if ( Var_Stack->type == INT_VAR )
		cur->max_count = Var_Stack->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: WARNING: Floating point value used as maximum "
				"count in REPEAT loop.\n",
				( cur + 1 )->Fname, ( cur + 1 )->Lc );
		cur->max_count = ( long ) Var_Stack->val.dval;
	}

	vars_pop( Var_Stack );
	cur->act_count = 1;
	cur_prg_token++;
	return;
}

