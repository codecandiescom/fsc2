/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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


#ifdef NDEBUG
#define fsc2_assert( expression ) do { } while ( 0 )
#define fsc2_impossible( ) do { } while ( 0 )
#else
#define fsc2_assert( expression )                                     \
       ( ( void ) ( ( expression ) ?                                  \
       0 : fsc2_assert_print( #expression, __FILE__, __LINE__ ) ) )
#define fsc2_impossible( )                                            \
       ( ( void ) fsc2_impossible_print( __FILE__, __LINE__ ) )
#endif


#endif   /* ! FSC2_ASSERT__HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
