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


#if ! defined SHA1_HASH_HEADER_
#define SHA1_HASH_HEADER_

#include "sha_types.h"
#include <stdlib.h>
#include <string.h>

#define SHA1_HASH_SIZE        20


#if ! defined SHA_DIGEST_OK
#define SHA_DIGEST_OK               0
#endif
#if ! defined SHA_DIGEST_INVALID
#define SHA_DIGEST_INVALID_ARG      1
#endif
#if ! defined SHA_DIGEST_INPUT_TOO_LONG
#define SHA_DIGEST_INPUT_TOO_LONG   2
#endif
#if ! defined SHA_DIGEST_NO_MORE_DATA
#define SHA_DIGEST_NO_MORE_DATA     3
#endif


typedef struct {
	sha_u32       H[ 5 ];
    sha_u64       count;
	unsigned char buf[ 64 ];
	size_t        index;
	int           is_calculated;
	int           error;
} SHA1_Context;


int sha1_initialize( SHA1_Context * context );
int sha1_add_data( SHA1_Context * context,
				   const void   * data,
				   size_t         length );
int sha1_calculate( SHA1_Context  * context,
					unsigned char   digest[ SHA1_HASH_SIZE ] );

#endif /* ! SHA1_HASH_HEADER_ */


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
