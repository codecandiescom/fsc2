/*
  $Id$
*/


#include "fsc2.h"

extern File *prim_expin;


bool store_exp( File *in )
{
	int ret;
	long cur_len = prg_length;

	irf ( cur_len != 0 )
		free( prg_token );

	prg_token = T_alloc( PRG_CHUNK_SIZE * sizeof( Prg_Token ) );



	prim_expin = in;

	while ( ret = prim_explex( ) != 0 )
	{
		prg_token[ cur_len ].token = ret;
		memcpy( prg_token[ cur_len ]->type, prim_explval, sizeof( ) );
	}
}
