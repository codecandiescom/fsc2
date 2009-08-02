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
 */


#define NEED_U64_SET
#define NEED_U64_PLUS
#define NEED_U64_SIZET_PLUS
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
 * Sets up the context structure (or resets it to be used anew)
 *----------------------------------------------------------------*/

int
sha1_initialize( SHA1_Context * context )
{
    if ( ! context )
        return SHA_DIGEST_INVALID_ARG;

    memcpy( context->H, H, sizeof H );
    context->count         = sha_u64_set( 0, 0 );
    context->off_count     = 0;
    context->index         = 0;
    context->is_calculated = 0;
    context->error         = SHA_DIGEST_OK;

    return SHA_DIGEST_OK;
}


/*----------------------------------------------------------------*
 * Adds byte-oriented data for the calculation of the hash
 *----------------------------------------------------------------*/

int
sha1_add_bytes( SHA1_Context * context,
                const void   * data,
                size_t         num_bytes )
{
    /* If necessary use bit-oriented routine */

    if ( context->off_count != 0 )
        return sha1_add_bits( context, data, 8 * num_bytes );

    if ( ! context || ! data )
        return SHA_DIGEST_INVALID_ARG;

    if ( context->error )
        return context->error;

    if ( context->is_calculated )
        return context->error = SHA_DIGEST_NO_MORE_DATA;

    /* Split up the input into 512 bit sized chunks from which the hash
       value gets calculated */

    while ( num_bytes )
    {
        unsigned int len = num_bytes >= 64 ? 64 : num_bytes;

        if ( context->index + len > 64 )
            len = 64 - context->index;

        /* Increment bit count, abort on input of 2^64 or more bits */

        context->count = sha_u64_plus( context->count,
                                       sha_u64_set( 0, 8 * len ) );
        if ( sha_u64_lt( context->count, sha_u64_set( 0, 8 * len ) ) )
             return context->error = SHA_DIGEST_INPUT_TOO_LONG;

        memcpy( context->buf + context->index, data, len );
        data       = ( unsigned char * ) data + len;
        num_bytes -= len;

        if ( ( context->index += len ) == 64 )
            sha1_process_block( context );
    }

    return SHA_DIGEST_OK;
}


/*----------------------------------------------------------------*
 * Adds bit-oriented data for the calculation of the hash
 *----------------------------------------------------------------*/

int
sha1_add_bits( SHA1_Context * context,
               const void   * data,
               size_t         num_bits )
{
    const unsigned char *d = data;
    unsigned char shift = 8 - context->off_count;


    /* Always try to use the byte-oriented routine as far as possible */

    if ( context->off_count == 0 && num_bits > 7 )
    {
        int ret = sha1_add_bytes( context, data, num_bits / 8 );

        d += num_bits / 8;

        if ( ( num_bits &= 0x07 ) == 0 || ret != SHA_DIGEST_OK )
            return ret;
    }
    else
    {
        if ( ! context || ! data )
            return SHA_DIGEST_INVALID_ARG;

        if ( context->error )
            return context->error;

        if ( context->is_calculated )
            return context->error = SHA_DIGEST_NO_MORE_DATA;
    }

    if ( num_bits == 0 )
        return SHA_DIGEST_OK;

    /* Increment bit count, abort on input of 2^64 or more bits (note:
       sha_u64_sizet_add() returns a 0 value on overflow!) */

    context->count = sha_u64_sizet_plus( context->count, num_bits );
    if ( sha_u64_eq( context->count, sha_u64_set( 0, 0 ) ) )
        return context->error = SHA_DIGEST_INPUT_TOO_LONG;

    /* Deal with all full (8-bit) bytes of input */

    while ( num_bits > 7 )
    {
        context->buf[ context->index++ ] |= SHA_T8( *d ) >> context->off_count;
        context->buf[ context->index   ]  = *d++ << shift;
        num_bits -= 8;

        if ( context->index == 64 )
        {
            sha1_process_block( context );
            context->buf[ 0 ] = context->buf[ 64 ];
        }
    }

    /* Now we're left with no more that 7 bits, they may or may not fit into
       the current byte of the context's buffer */

    if ( num_bits > 0 )
    {
        if ( num_bits <= shift )
        {
            if ( context->off_count == 0 )
                context->buf[ context->index ] = 0;

            context->buf[ context->index ] |=
                  ( SHA_T8( *d ) >> ( 8 - num_bits ) ) << ( shift - num_bits );

            if ( ( context->off_count += num_bits ) == 8 )
            {
                context->off_count = 0;
                if ( ++context->index == 64 )
                    sha1_process_block( context );
            }
        }
        else
        {
            context->buf[ context->index++ ] |=
                                            SHA_T8( *d ) >> context->off_count;
            context->buf[ context->index   ]  = *d << shift;

            context->off_count = ( context->off_count + num_bits ) % 8;

            if ( context->index == 64 )
            {
                sha1_process_block( context );
                context->buf[ 0 ] = context->buf[ 64 ];
            }
        }
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
		W[ t ]  = SHA_T8L( *buf++ ) << 24;
		W[ t ] |= SHA_T8L( *buf++ ) << 16;
		W[ t ] |= SHA_T8L( *buf++ ) <<  8;
		W[ t ] |= SHA_T8L( *buf++ );

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

    if ( context->off_count == 0 )
        context->buf[ context->index++ ] = 0x80;
    else
        context->buf[ context->index++ ] |= 0x80 >> context->off_count;

    if ( context->index > 56 )
    {
        memset( context->buf + context->index, 0, 64 - context->index );
        sha1_process_block( context );
        memset( context->buf, 0, 56 );
    }
    else
        memset( context->buf + context->index, 0, 56 - context->index );

    /* Store bit count at end and do the final round of the calculation */

    for ( count = context->count, i = 63; i > 55;
          count = sha_u64_shr( count, 8 ), i-- )
        context->buf[ i ] = sha_u64_low( count );

    sha1_process_block( context );
    context->is_calculated = 1;

    /* Wipe memory used for storing data supplied by user */

    memset( context->buf, 0, sizeof context->buf );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
