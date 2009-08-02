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
    unsigned char off_count;
	unsigned char buf[ 65 ];
	size_t        index;
	int           is_calculated;
	int           error;
} SHA1_Context;


#define sha1_add_data  sha1_add_bytes


int sha1_initialize( SHA1_Context * context );
int sha1_add_bytes( SHA1_Context * context,
                    const void   * data,
                    size_t         num_bytes );
int sha1_add_bits( SHA1_Context * context,
                   const void   * data,
                   size_t         num_bits );
int sha1_calculate( SHA1_Context  * context,
					unsigned char   digest[ SHA1_HASH_SIZE ] );

#endif /* ! SHA1_HASH_HEADER_ */


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
