/*
  $Id$
*/

#if ! defined FSC2_ASSERT_HEADER
#define FSC2_ASSERT_HEADER


typedef struct {
	const char *expression;
	unsigned int line;
	const char *filename;
} Fsc2_Assert;


void fsc2_assert_print( const char *expression, const char *filename,
						unsigned int line );

#ifdef NDEBUG
#define fsc2_assert( ignore ) ( ( void ) 0 )
#else
#define fsc2_assert( expression )    \
       ( ( void ) ( ( expression ) ? \
       0 : fsc2_assert_print( #expression, __FILE__, __LINE__) ) )
#endif


#endif   /* ! FSC2_ASSERT__HEADER */
