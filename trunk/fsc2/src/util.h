/*
  $Id$
*/

#if ! defined UTIL_HEADER
#define UTIL_HEADER

char *get_string_copy( const char *str );
char *get_string( size_t len );
char *string_to_lower( char *str );
void *get_memcpy( const void *array, size_t size );
const char *strip_path( const char *path );
long get_file_length( char *name, int *len );
void eprint( int severity, const char *fmt, ... );

inline long   rnd( double x );
inline int    imax( int    a, int    b );
inline int    imin( int    a, int    b );
inline long   lmax( long   a, long   b );
inline long   lmin( long   a, long   b );
inline float  fmax( float  a, float  b );
inline float  fmin( float  a, float  b );
inline double dmax( double a, double b );
inline double dmin( double a, double b );


#endif  /* ! UTIL_HEADER */
