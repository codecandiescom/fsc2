#if ! defined UTIL_HEADER
#define UTIL_HEADER

char *get_string_copy( char *str );
char *get_string( size_t len );
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
