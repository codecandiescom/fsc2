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
 *      This is the header file for code which implements the Secure
 *      Hashing Algorithm 1 as defined in FIPS PUB 180-1 published
 *      April 17, 1995.
 *
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *      Please read the file sha1.c for more information.
 */


#if ! defined SHA1_HEADER
#define SHA1_HEADER

#include <stdint.h>
#include <string.h>

/* If you do not have the ISO standard stdint.h header file, then you
 * must typdef the following:
 *    name              meaning
 *  uint32_t         unsigned 32 bit integer
 *  uint8_t          unsigned 8 bit integer (i.e. unsigned char)
 */

enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called input after Result */
};

#define SHA1HashSize 20


/* This structure will hold context information for the SHA-1
 * hashing operation */

typedef struct SHA1Context
{
    uint32_t Intermediate_Hash[ SHA1HashSize / 4 ]; /* message Digest */

    uint32_t Length_Low;            /* message length in bits */
    uint32_t Length_High;           /* message length in bits */

    int Message_Block_Index;        /* index into message block array */
    uint8_t Message_Block[ 64 ];    /* 512-bit message blocks */

    int Computed;                   /* has the digest been computed? */
    int Corrupted;                  /* is the message digest corrupted? */
} SHA1Context;


/* Function Prototypes */

int SHA1Init( SHA1Context * );
int SHA1Input( SHA1Context *, const uint8_t *, unsigned int );
int SHA1Result( SHA1Context *, uint8_t * );


#endif /* ! SHA1_HEADER */

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
