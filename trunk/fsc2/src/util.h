/*
  $Id$
*/

#if ! defined UTIL_HEADER
#define UTIL_HEADER


char *get_string_copy( const char *str );
char *get_string( size_t len );
char *string_to_lower( char *str );
void *get_memcpy( const void *array, size_t size );
char *correct_line_breaks( char *str );
const char *strip_path( const char *path );
long get_file_length( char *name, int *len );
void eprint( int severity, const char *fmt, ... );
bool fsc2_locking( void );
int is_in( const char *supplied_in, const char **altern, int max );
void i2rgb( double h, int *rgb );
void create_colors( void );
inline unsigned long d2color( double a );


inline short  d2shrt( double a );
inline short  i2shrt( int a );

inline int    i_max( int    a, int    b );
inline int    i_min( int    a, int    b );
inline long   l_max( long   a, long   b );
inline long   l_min( long   a, long   b );
inline float  f_max( float  a, float  b );
inline float  f_min( float  a, float  b );
inline double d_max( double a, double b );
inline double d_min( double a, double b );


/* Needed for glib versions below 2.0 (or 2.1 ?) */

#if ( ! defined ( lround ) )
#define NEED_LROUND 1
inline long lround( double x );
#else
#undef NEED_LROUND
#endif


#endif  /* ! UTIL_HEADER */
