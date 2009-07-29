/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  A lot of ideas for this SHA1 implementation came from the example
 *  implementation from RFC 3174 by D. Eastlake, 3rd (Motorola) and P.
 *  Jones (Cisco Systems), see e.g.
 *
 *  http://www.faqs.org/rfcs/rfc3174.html
 *
 *  The part for dealing with 64-bit numbers on systems that lack such
 *  a type has directly been taken from code written by Paul Eggert,
 *  and which is part of the GNU Coreutils in the file 'lib/u64.h'
 *  and can be downloaded e.g. from
 *
 *  http://www.gnu.org/software/coreutils/
 */


#define NEED_U64_SET
#define NEED_U64_PLUS
#define NEED_U64_LT
#define NEED_U64_SHR
#define NEED_U64_LOW

#include "sha1.h"

/* Circular left rotation of 32-bit value 'val' left by 'bits' bits
   (assumes that 'bits' is always within range from 0 to 32) */

#define ROTL( bits, val ) \
        ( SHA_T32( ( val ) << ( bits ) ) | ( ( val ) >> ( 32 - ( bits ) ) ) )


/* Initial hash values (see p. 14 of FIPS 180-3) */

static const sha_u32 H[ ] = { 0x67452301,
                              0xefcdab89,
                              0x98badcfe,
                              0x10325476,
                              0xc3d2e1f0 };

/* Constants required for hash calculation (see p. 11 of FIPS 180-3) */

static const sha_u32 K[ ] = { 0x5a827999,
                              0x6ed9eba1,
                              0x8f1bbcdc,
                              0xca62c1d6 };

/* Local functions */

static void sha1_process_block( SHA1_Context * context );
static void sha1_evaluate( SHA1_Context * context );


/*----------------------------------------------------------------*
 * Sets up the context structure (or resets it)
 *----------------------------------------------------------------*/

int
sha1_initialize( SHA1_Context * context )
{
    if ( ! context )
        return SHA_DIGEST_INVALID_ARG;

    memcpy( context->H, H, sizeof H );
    context->count         = sha_u64_set( 0, 0 );
    context->index         = 0;
    context->is_calculated = 0;
    context->error         = SHA_DIGEST_OK;

    return SHA_DIGEST_OK;
}


/*----------------------------------------------------------------*
 * Adds data for the calculation of the hash
 *----------------------------------------------------------------*/

int
sha1_add_data( SHA1_Context * context,
			   const void   * data,
			   size_t         length )
{
    if ( ! context || ! data )
        return SHA_DIGEST_INVALID_ARG;

    if ( context->error )
        return context->error;

    if ( context->is_calculated )
        return context->error = SHA_DIGEST_NO_MORE_DATA;

    /* Split up the input into 512 bit sized chunks from which the hash
       value gets calculated */

    while ( length )
    {
        size_t len = length >= 64 ? 64 : length;

        if ( context->index + len > 64 )
            len = 64 - context->index;

        memcpy( context->buf + context->index, data, len );

        /* Increment bit count, abort on input of 2^64 or more bits */

        context->count = sha_u64_plus( context->count,
                                       sha_u64_set( 0, 8 * len ) );
        if ( sha_u64_lt( context->count, sha_u64_set( 0, 8 * len ) ) )
             return context->error = SHA_DIGEST_INPUT_TOO_LONG;

        data    = ( unsigned char * ) data + len;
        length -= len;

        if ( ( context->index += len ) == 64 )
            sha1_process_block( context );
    }

    return SHA_DIGEST_OK;
}


/*----------------------------------------------------------------*
 * If the digest hadn't been calculated before finalize the
 * calculation, then copy the result iver to the use supplied
 * buffer.
 *----------------------------------------------------------------*/

int
sha1_calculate( SHA1_Context  * context,
				unsigned char   digest[ SHA1_HASH_SIZE ] )
{
    size_t i,
           j;


    if ( ! context || ! digest )
        return SHA_DIGEST_INVALID_ARG;

    if ( context->error )
        return context->error;

    if ( ! context->is_calculated )
        sha1_evaluate( context );

    for ( i = j = 0; j < SHA1_HASH_SIZE; i++ )
    {
        digest[ j++ ] = context->H[ i ] >> 24;
        digest[ j++ ] = context->H[ i ] >> 16;
        digest[ j++ ] = context->H[ i ] >>  8;
        digest[ j++ ] = context->H[ i ];
    }

    return SHA_DIGEST_OK;
}


/*----------------------------------------------------------------*
 * Central routine for calculating the hash value. See the FIPS
 * 180-3 standard p. 17f for a detailed explanation.
 *----------------------------------------------------------------*/

#define f1 	( ( B & C ) ^ ( SHA_T32( ~ B ) & D ) )

#define f2  ( B ^ C ^ D )

#define f3  ( ( B & C ) ^ ( B & D ) ^ ( C & D ) )

#define f4  f2

static void
sha1_process_block( SHA1_Context * context )
{
    size_t         t;
    sha_u32        W[ 80 ];
    sha_u32        A, B, C, D, E, tmp;
	unsigned char *buf = context->buf;


    A = context->H[ 0 ];
    B = context->H[ 1 ];
    C = context->H[ 2 ];
    D = context->H[ 3 ];
    E = context->H[ 4 ];

    for ( t = 0; t < 16; t++ )
    {
		W[ t ]  = SHA_T8( *buf++ ) << 24;
		W[ t ] |= SHA_T8( *buf++ ) << 16;
		W[ t ] |= SHA_T8( *buf++ ) <<  8;
		W[ t ] |= SHA_T8( *buf++ );

        tmp = SHA_T32( ROTL( 5, A ) + f1 + E + W[ t ] + K[ 0 ] );
        E = D;
        D = C;
        C = ROTL( 30, B );
        B = A;
        A = tmp;
    }

    for ( ; t < 20; t++ )
    {
		W[ t ] = ROTL( 1,   W[ t -  3 ] ^ W[ t -  8 ]
					      ^ W[ t - 14 ] ^ W[ t - 16 ] );

        tmp = SHA_T32( ROTL( 5, A ) + f1 + E + W[ t ] + K[ 0 ] );
        E = D;
        D = C;
        C = ROTL( 30, B );
        B = A;
        A = tmp;
    }

    for ( ; t < 40; t++ )
    {
		W[ t ] = ROTL( 1,   W[ t -  3 ] ^ W[ t -  8 ]
					      ^ W[ t - 14 ] ^ W[ t - 16 ] );

        tmp = SHA_T32( ROTL( 5, A ) + f2 + E + W[ t ] + K[ 1 ] );
        E = D;
        D = C;
        C = ROTL( 30, B );
        B = A;
        A = tmp;
    }

    for ( ; t < 60; t++ )
    {
		W[ t ] = ROTL( 1,   W[ t -  3 ] ^ W[ t -  8 ]
					      ^ W[ t - 14 ] ^ W[ t - 16 ] );

        tmp = SHA_T32( ROTL( 5, A ) + f3 + E + W[ t ] + K[ 2 ] );
        E = D;
        D = C;
        C = ROTL( 30, B );
        B = A;
        A = tmp;
    }

    for ( ; t < 80; t++ )
    {
		W[ t ] = ROTL( 1,   W[ t -  3 ] ^ W[ t -  8 ]
					      ^ W[ t - 14 ] ^ W[ t - 16 ] );

        tmp = SHA_T32( ROTL( 5, A ) + f4 + E + W[ t ] + K[ 3 ] );
        E = D;
        D = C;
        C = ROTL( 30, B );
        B = A;
        A = tmp;
    }

    context->H[ 0 ] = SHA_T32( context->H[ 0 ] + A );
    context->H[ 1 ] = SHA_T32( context->H[ 1 ] + B );
    context->H[ 2 ] = SHA_T32( context->H[ 2 ] + C );
    context->H[ 3 ] = SHA_T32( context->H[ 3 ] + D );
    context->H[ 4 ] = SHA_T32( context->H[ 4 ] + E );

    context->index = 0;
}


/*----------------------------------------------------------------*
 * To be called when all data have been entered, applies padding
 * and does the final round of the calculation.
 *----------------------------------------------------------------*/

static void
sha1_evaluate( SHA1_Context * context )
{
    int     i;
    sha_u64 count;


    /* If the block is too short for padding (at least one bit plus the
     * bit count as a 64-bit number) padd to the end of the block with 0
     * and then start a new block that contains just 0 and the bit count. */

    if ( context->index > 55 )
    {
        context->buf[ context->index ] = 0x80;
        memset( context->buf + context->index + 1, 0, 63 - context->index );

        sha1_process_block( context );

        memset( context->buf, 0, 56 );
    }
    else
    {
        context->buf[ context->index ] = 0x80;
        memset( context->buf + context->index + 1, 0, 55 - context->index );
    }

    /* Store bit count at end and do the final round of the calculation */

    for ( count = context->count, i = 63; i > 55;
          count = sha_u64_shr( count, 8 ), i-- )
        context->buf[ i ] = sha_u64_low( count );

    sha1_process_block( context );

    /* Wipe memory used for storing data supplied by user */

    memset( context->buf, 0, sizeof context->buf );

    context->is_calculated = 1;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
