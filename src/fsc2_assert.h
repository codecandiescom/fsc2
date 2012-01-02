/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Thanks to Robert Wessel <robertwessel2@yahoo.com> and Eric Sosman
 * <esosman@ieee.org> for suggestions on c.l.c on how to imprive it.
 */


#if ! defined FSC2_ASSERT_HEADER
#define FSC2_ASSERT_HEADER


typedef struct Fsc2_Assert Fsc2_Assert_T;

struct Fsc2_Assert {
    const char *expression;
    int line;
    const char *filename;
};


int fsc2_assert_print( const char * /* expression */,
                       const char * /* filename   */,
                       int          /* line       */  );

int fsc2_impossible_print( const char * /* filename */,
                           int          /* line     */  );

#endif   /* ! FSC2_ASSERT__HEADER */


/* Note: like the real <assert.h> it should be possibe to include
   the file more than once with different settings for NDEBUG */

#undef fsc2_assert
#undef fsc2_impossible


#ifdef NDEBUG
#define fsc2_assert( expression ) ( ( void ) ( 0 ) )
#define fsc2_impossible( ) ( ( void ) ( 0 ) )
#else
#define fsc2_assert( expression )                                     \
       ( ( void ) ( ( expression ) ?                                  \
       0 : fsc2_assert_print( #expression, __FILE__, __LINE__ ) ) )
#define fsc2_impossible( )                                            \
       ( ( void ) fsc2_impossible_print( __FILE__, __LINE__ ) )
#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
