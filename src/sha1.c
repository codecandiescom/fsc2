/*
 * The code in this file is taken (with minor modifications) from the
 * hashgen utility. It was written by Szabó Péter (pts@fazekas.hu,
 * http://www.inf.bme.hu/~pts/index__en.html), Department of Control
 * Engineering and Information Technology, Budapest University of
 * Technology and is available under the GPL either from the project
 * page at Freshmeat
 *
 * http://freshmeat.net/projects/hashgen
 *
 * or directly from
 *
 * http://www.inf.bme.hu/~pts/hashgen-latest.tar.gz
 */

/*
 *  Description:
 *      This file implements the Secure Hashing Algorithm 1 as
 *      defined in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The SHA-1, produces a 160-bit message digest for a given
 *      data stream.  It should take about 2**n steps to find a
 *      message with the same digest as a given message and
 *      2**(n/2) to find any two messages with the same digest,
 *      when n is the digest size in bits.  Therefore, this
 *      algorithm can serve as a means of providing a
 *      "fingerprint" for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code
 *      uses <stdint.h> (included via "sha1.h" to define 32 and 8
 *      bit unsigned integer types.  If your C compiler does not
 *      support 8 and 32 bit unsigned integers, this code is not
 *      appropriate.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long.  Although SHA-1 allows a message digest to be generated
 *      for messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is
 *      a multiple of the size of an 8-bit character.
 */

#include "sha1.h"

/* Define the SHA1 circular left shift macro */

#define SHA1CircularShift( bits, word ) \
           ( ( ( word ) << ( bits ) ) | ( ( word ) >> ( 32 - ( bits ) ) ) )


/* Local Function Prototyptes */

static void SHA1PadMessage( SHA1Context * );
static void SHA1ProcessMessageBlock( SHA1Context * );


/*------------------------------------------------------------------------*
 * SHA1Init
 *
 *  Description:
 *      This function initializes the SHA1Context in preparation for
 *      the computation of a new SHA1 message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to initialize.
 *
 *  Returns:
 *      sha Error Code.
 *------------------------------------------------------------------------*/

int
SHA1Init( SHA1Context * context )
{
    if ( ! context )
        return shaNull;

    context->Length_Low          = 0;
    context->Length_High         = 0;
    context->Message_Block_Index = 0;

    context->Intermediate_Hash[ 0 ] = 0x67452301;
    context->Intermediate_Hash[ 1 ] = 0xEFCDAB89;
    context->Intermediate_Hash[ 2 ] = 0x98BADCFE;
    context->Intermediate_Hash[ 3 ] = 0x10325476;
    context->Intermediate_Hash[ 4 ] = 0xC3D2E1F0;

    context->Computed  = 0;
    context->Corrupted = 0;

    return shaSuccess;
}


/*------------------------------------------------------------------------*
 * SHA1Result
 *
 *  Description:
 *      This function will return the 160-bit message digest into the
 *      Message_Digest array  provided by the caller.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to use to calculate the SHA-1 hash.
 *      Message_Digest: [out]
 *          Where the digest is returned.
 *
 *  Returns:
 *      sha Error Code.
 *------------------------------------------------------------------------*/

int
SHA1Result( SHA1Context * context,
            uint8_t       Message_Digest[ SHA1HashSize ] )
{
    int i, j;


    if ( ! context || ! Message_Digest )
        return shaNull;

    if ( context->Corrupted )
        return context->Corrupted;

    if ( ! context->Computed )
    {
        SHA1PadMessage( context );

        /* Message may be sensitive, clear it out */

        memset( context->Message_Block, 64, 0 );
    }

    for ( i = j = 0; j < SHA1HashSize; i++ )
    {
        Message_Digest[ j++ ] = context->Intermediate_Hash[ i ] >> 24;
        Message_Digest[ j++ ] = context->Intermediate_Hash[ i ] >> 16;
        Message_Digest[ j++ ] = context->Intermediate_Hash[ i ] >>  8;
        Message_Digest[ j++ ] = context->Intermediate_Hash[ i ];
    }

    return shaSuccess;
}


/*------------------------------------------------------------------------*
 * SHA1Input
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message. It can be called repeatedly whenever new portions
 *      of input become available.
 *
 *  Parameters:
 *      context: [in/out]
 *          The SHA context to update
 *      message: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of message (in octets)
 *
 *  Returns:
 *      sha Error Code.
 *------------------------------------------------------------------------*/

int
SHA1Input( SHA1Context   * context,
           const uint8_t * message,
           unsigned int    length )
{
    if ( ! context || ! message )
        return shaNull;

    if ( context->Corrupted )
        return context->Corrupted;

    if ( context->Computed )
        return context->Corrupted = shaStateError;

	/* Split up the input into 512 bit sized chunks from which the hash
	   value gets calculated */

    while ( length )
    {
        unsigned int len = length >= 64 ? 64 : length;

        if ( context->Message_Block_Index + len > 64 )
            len = 64 - context->Message_Block_Index;

        memcpy( context->Message_Block + context->Message_Block_Index,
                message, len );

        /* Increment bit count, abort on overflow */

        if (    ( context->Length_Low += 8 * len ) < 8 * len
             && ++context->Length_High == 0 )
            return context->Corrupted = shaInputTooLong;

        message += len;
        length  -= len;
        if ( ( context->Message_Block_Index += len ) == 64 )
            SHA1ProcessMessageBlock( context );
    }

    return shaSuccess;
}


/*------------------------------------------------------------------------*
 * SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in this code, especially the
 *      single character names, are used because they're the names
 *      used in the publication.
 *------------------------------------------------------------------------*/

static void
SHA1ProcessMessageBlock( SHA1Context * context )
{
    const uint32_t K[ ] = { 0x5A827999,    /* constants as defined in SHA-1 */
                            0x6ED9EBA1,
                            0x8F1BBCDC,
                            0xCA62C1D6 };
    int      t;                            /* loop counter */
    uint32_t temp;                         /* temporary word value */
    uint32_t W[ 80 ];                      /* word sequence */
    uint32_t A, B, C, D, E;                /* word buffers */


    /* Initialize the first 16 words in array W from the message block */

    for ( t = 0; t < 16; t++ )
        W[ t ]  =   context->Message_Block[ 4 * t     ] << 24
                  | context->Message_Block[ 4 * t + 1 ] << 16
                  | context->Message_Block[ 4 * t + 2 ] <<  8
                  | context->Message_Block[ 4 * t + 3 ];

    for ( t = 16; t < 80; t++ )
       W[ t ] = SHA1CircularShift( 1, W[ t -  3 ] ^ W[ t -  8 ] ^
                                      W[ t - 14 ] ^ W[ t - 16 ] );

    A = context->Intermediate_Hash[ 0 ];
    B = context->Intermediate_Hash[ 1 ];
    C = context->Intermediate_Hash[ 2 ];
    D = context->Intermediate_Hash[ 3 ];
    E = context->Intermediate_Hash[ 4 ];

    for ( t = 0; t < 20; t++ )
    {
        temp =  SHA1CircularShift( 5, A ) +
                ( ( B & C ) | ( ( ~B ) & D ) ) + E + W[ t ] + K[ 0 ];
        E = D;
        D = C;
        C = SHA1CircularShift( 30, B );

        B = A;
        A = temp;
    }

    for ( t = 20; t < 40; t++ )
    {
        temp = SHA1CircularShift( 5, A ) + ( B ^ C ^ D ) + E + W[ t ] + K[ 1 ];
        E = D;
        D = C;
        C = SHA1CircularShift( 30, B );
        B = A;
        A = temp;
    }

    for ( t = 40; t < 60; t++ )
    {
        temp = SHA1CircularShift( 5, A ) +
               ( ( B & C ) | ( B & D ) | ( C & D ) ) + E + W[ t ] + K[ 2 ];
        E = D;
        D = C;
        C = SHA1CircularShift( 30, B );
        B = A;
        A = temp;
    }

    for ( t = 60; t < 80; t++ )
    {
        temp = SHA1CircularShift( 5, A ) + ( B ^ C ^ D) + E + W[ t ] + K[ 3 ];
        E = D;
        D = C;
        C = SHA1CircularShift( 30, B );
        B = A;
        A = temp;
    }

    context->Intermediate_Hash[ 0 ] += A;
    context->Intermediate_Hash[ 1 ] += B;
    context->Intermediate_Hash[ 2 ] += C;
    context->Intermediate_Hash[ 3 ] += D;
    context->Intermediate_Hash[ 4 ] += E;

    context->Message_Block_Index = 0;
}


/*------------------------------------------------------------------------*
 * SHA1PadMessage
 *
 *  Description:
 *      According to the standard the message must be padded to contain
 *      512 bits. The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between must be 0. This function will pad the message according
 *      to those rules by filling the Message_Block array accordingly.
 *      It will also call the ProcessMessageBlock function. On returns it
 *      can be assumed that the message digest has been computed.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to pad
 *
 *  Returns:
 *      Nothing.
 *-----------------------------------------------------------------------*/

static void
SHA1PadMessage( SHA1Context * context )
{
    int i;


    /* Check to see if the current message block is too small to hold
     * the initial padding bits and length.  If so, we will pad the
     * block, process it, and then continue padding into a second
     * block. */

    if ( context->Message_Block_Index > 55 )
    {
        context->Message_Block[ context->Message_Block_Index++ ] = 0x80;
        memset( context->Message_Block + context->Message_Block_Index, 0,
                64 - context->Message_Block_Index );

        SHA1ProcessMessageBlock( context );

        memset( context->Message_Block, 0, 56 );
    }
    else
    {
        context->Message_Block[ context->Message_Block_Index++ ] = 0x80;
        memset( context->Message_Block + context->Message_Block_Index, 0,
                56 - context->Message_Block_Index );
    }

    /* Store the message length (in bits) as the last 8 octets of the
       message block. */

    for ( i = 59; i > 55; context->Length_High >>= 8, i-- )
        context->Message_Block[ i ] = context->Length_High;

    for ( i = 63; i > 59; context->Length_Low >>= 8, i-- )
        context->Message_Block[ i ] = context->Length_Low;

    SHA1ProcessMessageBlock( context );

    context->Computed = 1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
