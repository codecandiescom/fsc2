/*
  $Id$
*/


#include "fsc2.h"

extern int prim_exp_runparse( void );
extern int conditionparse( void );


Token_Val prim_exp_val;

extern int prim_explex( void );
extern FILE *prim_expin;



/* This routine stores the experiment section of an EDL file in the form of
   tokens (together with their semantic values) as returned by the lexer. Thus
   it serves as a kind of intermediate between the lexer and the parser. This
   is necessary for two reasons: First, the experiment section may contain
   loops. Without an intermediate it would be rather difficult to run through
   the loops since we would have to re-read and re-tokenize the input file
   again and again, which not only would be slow but also difficult since what
   we read as input file is actually a pipe from the `cleaner'. Second, we
   will have to run through the experiment section at least two times, first
   for sanity checks and then again for really doing the experiment. Again,
   without an intermediate for storing the tokenized form of the experiment
   section this would necessitate reading the input files at least two times.

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
	long cur_len = 0;
	char *cur_Fname = NULL;


	/* forget all about last run */

	forget_prg( );

	/* set input file */

	prim_expin = in;

	/* get and store all tokens */

	while ( ( ret = prim_explex( ) ) != 0 )
	{
		/* get or extend memory used for storing the tokens */

		if ( cur_len % PRG_CHUNK_SIZE == 0 )
			prg_token = T_realloc( prg_token,
								   PRG_CHUNK_SIZE * sizeof( Prg_Token ) );

		prg_token[ cur_len ].token = ret;

		/* copy the union with the value unless its a string token - in this
		   case we have to copy the string since its not persistent */

		if ( ret != E_STR_TOKEN )
			memcpy( &prg_token[ cur_len ].tv, &prim_exp_val,
					sizeof( Token_Val ) );
		else
			prg_token[ cur_len ].tv.sptr =
				get_string_copy( prim_exp_val.sptr );

		/* If the file name changed get a copy of the new name. Then set
		   pointer in structure to file name and copy curent line number. */

		if ( cur_Fname == NULL || strcmp( Fname, cur_Fname ) )
			cur_Fname = get_string_copy( Fname );
		prg_token[ cur_len ].Fname = cur_Fname;
		prg_token[ cur_len ].Lc = Lc;


		prg_token[ cur_len ].start = prg_token[ cur_len ].altern
			                       = prg_token[ cur_len ].end = NULL;

		cur_len++;
	}

	prg_length = cur_len;

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
		return;

	/* get the address where the first file names ios stored and free the
	   string */

	cur_Fname = prg_token[ 0 ].Fname;
	free( cur_Fname );

	for ( i = 0; i < prg_length; ++i )
	{
		/* for string tokens free the string */

		if ( prg_token[ i ].token == E_STR_TOKEN )
			free( prg_token[ i ].tv.sptr );

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
	}

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
				cur->start = &prg_token[ i ];
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
				if ( in_if )
					cur->start = &prg_token[ i ];
				break;

			case '}' :
				if ( in_if && i + 1 < prg_length &&
					 prg_token[ i + 1 ].token == ELSE_TOK )
				{
					cur->altern = &prg_token[ i + 1 ];
					in_if = UNSET;
					break;
				}

				cur->end = &prg_token[ i + 1 ];
				if ( cur->altern != NULL )
					( cur->altern - 1 )->end = cur->end;
				else
					prg_token[ i ].end = cur->end;
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
	cur_prg_token = prg_token;

	while ( cur_prg_token != prg_token + prg_length )
	{
		switch ( cur_prg_token->token )
		{
			case WHILE_TOK :
				cur_prg_token++;
				conditionparse( );
				break;

			case BREAK_TOK :
				break;

			default :
				prim_exp_runparse( );
				break;
		}
	}
}


int prim_exp_runlex( void )
{
	

	return( 0 );
}

int conditionlex( void )
{
	return( 0 );
}
