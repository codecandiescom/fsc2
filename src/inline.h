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


#if ! defined INLINE_HEADER
#define INLINE_HEADER

#include "fsc2.h"


static inline short int i2s15( int /* i */ );

static inline unsigned short int i2u15( int /* i */ );

static inline short int i2s16( int /* i */ );

static inline unsigned short int i2u16( int /* i */ );

static inline short int s15rnd( double /* d */ );

static inline unsigned short int u15rnd( double /* d */ );

static inline short int s16rnd( double /* d */ );

static inline unsigned short int u16rnd( double /* d */ );

static inline short int srnd( double /* d */ );

static inline unsigned short int usrnd( double /* d */ );

static inline int irnd( double /* d */ );

static inline unsigned int uirnd( double /* d */ );

static inline long lrnd( double /* d */ );

static inline unsigned long ulrnd( double /* d */ );

static inline int i_max( int /* a */,
                         int /* b */  );

static inline int i_min( int /* a */,
                         int /* b */  );

static inline long l_max( long /* a */,
                          long /* b */  );

static inline long l_min( long /* a */,
                          long /* b */  );

static inline float f_max( float /* a */,
                           float /* b */  );

static inline float f_min( float /* a */,
                           float /* b */  );

static inline double d_max( double /* a */,
                            double /* b */  );

static inline double d_min( double /* a */,
                            double /* b */  );

static inline ssize_t ss_min( ssize_t /* a */,
                              ssize_t /* b */  );

static inline ssize_t ss_max( ssize_t /* a */,
                              ssize_t /* b */  );

static inline size_t s_min( size_t /* a */,
                            size_t /* b */  );

static inline size_t s_max( size_t /* a */,
                            size_t /* b */  );


/*-------------------------------------------------------------------------*
 * Converts an int to a short value in the range representable by 15 bits.
 * If the integer is too large or too small the maximum or minimum value
 * from the 15 bit range gets returned and errno gets set to ERANGE.
 *-------------------------------------------------------------------------*/

static inline short int
i2s15( int i )
{
    if ( i > 16383 )
    {
        errno = ERANGE;
        return 16383;
    }
#if SHRT_MIN != - SHRT_MAX         /* 2's complement machines */
    if ( i < -16384 )
    {
        errno = ERANGE;
        return -16384;
    }
#else
    if ( i < -16383 )
    {
        errno = ERANGE;
        return -16383;
    }
#endif

    return ( short int ) i;
}


/*--------------------------------------------------------------------------*
 * Converts an int to an unsigned short value in the range representable by
 * 15 bits. If the integer is too large the maximum value representable by
 * 15 bits gets returned and 0 if the integer is less than 0. In both cases
 * errno gets set to ERANGE.
 *--------------------------------------------------------------------------*/

static inline unsigned short int
i2u15( int i )
{
    if ( i > 32767 )
    {
        errno = ERANGE;
        return 32767;
    }
    if ( i < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return ( unsigned short int ) i;
}


/*-------------------------------------------------------------------------*
 * Converts an int to a short value in the range representable by 16 bits.
 * If the integer is too large or too small the maximum or minimum value
 * fron the 16 bit range gets returned and errno gets set to ERANGE.
 *-------------------------------------------------------------------------*/

static inline short int
i2s16( int i )
{
    if ( i > 32767 )
    {
        errno = ERANGE;
        return 32767;
    }
#if SHRT_MIN != - SHRT_MAX         /* 2's complement machines */
    if ( i < -32768 )
    {
        errno = ERANGE;
        return -32768;
    }
#else
    if ( i < -32767 )
    {
        errno = ERANGE;
        return -32767;
    }
#endif

    return ( short int ) i;
}


/*--------------------------------------------------------------------------*
 * Converts an int to an unsigned short value in the range representable by
 * 16 bits. If the integer is too large the maximum value representable by
 * 16 bits gets returned and 0 if the integer is less than 0. In both cases
 * errno gets set to ERANGE.
 *--------------------------------------------------------------------------*/

static inline unsigned short int
i2u16( int i )
{
    if ( i > 65535 )
    {
        errno = ERANGE;
        return 65535;
    }
    if ( i < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return ( unsigned short int ) i;
}


/*-----------------------------------------------------------------------*
 * Rounds a double to the nearest signed integer value representable by
 * 15 bits. On overflow the maximum value is returned, on underflow the
 * smallest possible value. On both over- and underflow errno gets set
 * to ERANGE.
 *-----------------------------------------------------------------------*/

static inline short int
s15rnd( double d )
{
    if ( d >= 16383.5 )
    {
        errno = ERANGE;
        return 16383;
    }
#if SHRT_MIN != - SHRT_MAX         /* 2's complement machines */
    if ( d <= -16384.5 )
    {
        errno = ERANGE;
        return -16384;
    }
#else
    if ( d <= -16383.5 )
    {
        errno = ERANGE;
        return -16383;
    }
#endif
    return ( short int ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*-------------------------------------------------------------------------*
 * Rounds a double to the nearest unsigned intreger value representable by
 * 15 bits. On overflow the maximum value is returned, and 9 if the double
 * is smaller than 0. In both these cases errno gets set to ERANGE.
 *-------------------------------------------------------------------------*/

static inline unsigned short int
u15rnd( double d )
{
    if ( d >= 32767.5 )
    {
        errno = ERANGE;
        return 32767;
    }
    if ( d < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return floor( d + 0.5 );
}


/*-------------------------------------------------------------------------*
 * Rounds a double to the nearest signed integer value representable by
 * 16 bits. On overflow the maximum value is returned, on underflow the
 * smallest possible value. On both over- and underflow errno gets set
 * to ERANGE.
 *-------------------------------------------------------------------------*/

static inline short int
s16rnd( double d )
{
    if ( d >= 32767.5 )
    {
        errno = ERANGE;
        return 32767;
    }
#if SHRT_MIN != - SHRT_MAX         /* 2's complement machines */
    if ( d <= -32768.5 )
    {
        errno = ERANGE;
        return -32768;
    }
#else
    if ( d <= -32767.5 )
    {
        errno = ERANGE;
        return -32767;
    }
#endif
    return ( short int ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*-------------------------------------------------------------------------*
 * Rounds a double to the nearest unsigned integer value representable by
 * 16 bits. On overflow the maximum value is returned, and 9 if the double
 * is smaller than 0. In both these cases errno gets set to ERANGE.
 *-------------------------------------------------------------------------*/

static inline unsigned short int
u16rnd( double d )
{
    if ( d >= 65535.5 )
    {
        errno = ERANGE;
        return 65535;
    }
    if ( d < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return floor( d + 0.5 );
}


/*---------------------------------------------------------------------*
 * Rounds a double to the nearest short int value. On overflow largest
 * possible short int gets returned, and on underflow the smallest
 * short int value. In both these cases errno gets set to ERANGE.
 *---------------------------------------------------------------------*/

static inline short int
srnd( double d )
{
    if ( d >= SHRT_MAX + 0.5 )
    {
        errno = ERANGE;
        return SHRT_MAX;
    }
    if ( d <= SHRT_MIN - 0.5 )
    {
        errno = ERANGE;
        return SHRT_MIN;
    }

    return ( short int ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*----------------------------------------------------------------------*
 * Rounds a double to the nearest unsigned short int value. On overflow
 * the largest possible unsigned short int value gets returned, and 0
 * if the double is less than 0. In both these cases errno gets set to
 * ERANGE.
 *----------------------------------------------------------------------*/

static inline unsigned short int
usrnd( double d )
{
    if ( d >= USHRT_MAX + 0.5 )
    {
        errno = ERANGE;
        return USHRT_MAX;
    }
    if ( d < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return floor( d + 0.5 );
}


/*------------------------------------------------------------------------*
 * Rounds a double to the nearest int value. On overflow largest possible
 * int gets returned, and on underflow the smallest int value. In both
 * these cases errno gets set to ERANGE.
 *------------------------------------------------------------------------*/

static inline int
irnd( double d )
{
    if ( d >= INT_MAX + 0.5 )
    {
        errno = ERANGE;
        return INT_MAX;
    }
    if ( d <= INT_MIN - 0.5 )
    {
        errno = ERANGE;
        return INT_MIN;
    }

    return ( int ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*----------------------------------------------------------------------*
 * Rounds a double to the nearest unsigned int value. On overflow the
 * largest possible unsigned int value gets returned, and 0 if the
 * double is less than 0. In both these cases errno gets set to ERANGE.
 *----------------------------------------------------------------------*/

static inline unsigned int
uirnd( double d )
{
    if ( d >= UINT_MAX + 0.5 )
    {
        errno = ERANGE;
        return UINT_MAX;
    }
    if ( d < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return floor( d + 0.5 );
}


/*--------------------------------------------------------------------*
 * Rounds a double to the nearest long int value. On overflow largest
 * possible long int gets returned, and on underflow the smallest
 * long int value. In both these cases errno gets set to ERANGE.
 *--------------------------------------------------------------------*/

static inline long
lrnd( double d )
{
    if ( d >= LONG_MAX + 0.5 )
    {
        errno = ERANGE;
        return LONG_MAX;
    }
    if ( d <= LONG_MIN - 0.5 )
    {
        errno = ERANGE;
        return LONG_MIN;
    }

    return ( long ) ( d < 0.0 ? ceil( d - 0.5 ) : floor( d + 0.5 ) );
}


/*-------------------------------------------------------------------------*
 * Rounds a double to the nearest unsigned long int value. On overflow the
 * largest possible unsigned long int value gets returned, and 0 if the
 * double is less than 0. In both these cases errno gets set to ERANGE.
 *-------------------------------------------------------------------------*/

static inline unsigned long
ulrnd( double d )
{
    if ( d >= ULONG_MAX + 0.5 )
    {
        errno = ERANGE;
        return ULONG_MAX;
    }
    if ( d < 0 )
    {
        errno = ERANGE;
        return 0;
    }

    return floor( d + 0.5 );
}


/*--------------------------------------------------------------------*
 * Various utility functions for comparing values of different types.
 *--------------------------------------------------------------------*/

static inline int     i_max(  int     a, int     b ) { return a > b ? a : b; }
static inline int     i_min(  int     a, int     b ) { return a < b ? a : b; }
static inline long    l_max(  long    a, long    b ) { return a > b ? a : b; }
static inline long    l_min(  long    a, long    b ) { return a < b ? a : b; }
static inline float   f_max(  float   a, float   b ) { return a > b ? a : b; }
static inline float   f_min(  float   a, float   b ) { return a < b ? a : b; }
static inline double  d_max(  double  a, double  b ) { return a > b ? a : b; }
static inline double  d_min(  double  a, double  b ) { return a < b ? a : b; }
static inline ssize_t ss_min( ssize_t a, ssize_t b ) { return a < b ? a : b; }
static inline ssize_t ss_max( ssize_t a, ssize_t b ) { return a < b ? a : b; }
static inline size_t  s_min(  size_t  a, size_t  b ) { return a < b ? a : b; }
static inline size_t  s_max(  size_t  a, size_t  b ) { return a < b ? a : b; }


#endif  /* ! INLINE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
