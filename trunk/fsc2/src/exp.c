/*
  $Id$
*/


#include "fsc2.h"
#include "prim_exp_parser.h"


Token_Val prim_exp_val;

extern int prim_explex( void );
extern FILE *prim_expin;



/* This routine stores the experiment section of an EDL file in the form of
   tokens as returned by the lexer. Thus it serves as a kind of intermediate
   between the lexer and the parser. This is necessary for two reasons: First,
   the experiment section may contain loops. Without an intermediate it would
   be rather difficult to run through the loops since we would have to re-read
   and re-tokenize the input file again and again which not only would be slow
   but also difficult since what we read as input file is actually a pipe from
   the `cleaner'. Second, we will have to run through the experiment section
   at least two times, first for sanity checks and then again for really doing
   the experiment. Again, without an intermediate for storing the tokenized
   form of the experiment section this would necessitate reading the input
   files at least two times.

   By having an intermediate between the lexer and the parser we avoid several
   problems. We don't have to re-read and re-tokenize the input file. We also
   can find out about about loops and handle them later by simply feeding the
   parser the loop again and again. Of course, beside storing the experiment
   section in tokenized form as done by this function, we also need a routine
   that later feeds the stored tokens to the parser(s).
*/


bool store_exp( FILE *in )
{
	int ret;
	long cur_len = 0;
	char *cur_Fname = NULL;


	/* forgert all about last run */

	forget_prg( );

	/* set input file */

	prim_expin = in;

	/* run through all the tokens */

	while ( ( ret = prim_explex( ) ) != 0 )
	{
		/* get or extend memory used for storing the tokens */

		if ( cur_len % PRG_CHUNK_SIZE == 0 )
			prg_token = T_realloc( prg_token,
								   PRG_CHUNK_SIZE * sizeof( Prg_Token ) );

		prg_token[ cur_len ].token = ret;

		/* copy the union with the value unless its a string token - in this
		   case we have to copy the string since its not persistent */

		if ( ret != STR_TOKEN )
			memcpy( &prg_token[ cur_len ].tv, &prim_exp_val,
					sizeof( Token_Val ) );
		else
			prg_token[ cur_len ].tv.sptr =
				get_string_copy( prim_explval.sptr );

		/* If the file name changed get a copy of the new name. Than set
		   pointer in structure to file name and copy curent line number. */

		if ( cur_Fname == NULL || strcmp( Fname, cur_Fname ) )
			cur_Fname = get_string_copy( Fname );
		prg_token[ cur_len ].Fname = cur_Fname;
		prg_token[ cur_len ].Lc = Lc;

		cur_len++;
	}

	prg_length = cur_len;

	/* check and initialize if's and loops &c */

	prim_check( );
}


void forget_prg( void )
{
	char *cur_Fname;
	long i;


	/* check if anything has to be done */

	if ( prg_token == NULL )
		return;

	/* get the first file names address and free the string */

	cur_Fname = prg_token[ 0 ].Fname;
	free( cur_Fname );

	for ( i = 0; i < prg_length; ++i )
	{
		/* for string tokens free the string */

		if ( prg_token[ i ].token == STR_TOKEN )
			free( prg_token[ i ].tv.sptr );

		/* if the file name changed we store the pointer to the new name and
           free the string */

		if ( prg_token[ i ].Fname != cur_Fname )
			free( cur_Fname = prg_token[ i ].Fname );
	}

	free( prg_token );
	prg_token = NULL;
	prg_length = 0;
}


void prim_check( void )
{

}
