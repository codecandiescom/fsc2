/*
  $Id$
*/


#include "fsc2.h"


typedef struct {
	long nx;
	long ny;
	long nc;
	int type;
	Var *v;
} DPoint;


/* Both these variables are defined in 'func.c' */

extern bool No_File_Numbers;
extern bool Dont_Save;


static DPoint *eval_display_args( Var *v, int *npoints );
static int get_save_file( Var **v, const char *calling_function );
static bool print_array( Var *v, long cur_dim, long *start, int fid );
static bool print_slice( Var *v, int fid );
static bool print_browser( int browser, int fid, const char* comment );
static void T_fprintf( int file_num, const char *fmt, ... );

extern void child_sig_handler( int signo );

extern jmp_buf alrm_env;
extern volatile sig_atomic_t can_jmp_alrm;
extern volatile bool is_alrm;

/*------------------------------------------------------------------------
   Prints variable number of arguments using a format string supplied as
   the first argument. Types of arguments to be printed are integer and
   float data and strings. To get a value printed the format string has
   to contain the character `#'. The escape character is the backslash,
   with a double backslash for printing one backslash. Beside the `\#'
   combination to print a `#' some of the escape sequences from printf()
   ('\n', '\t', and '\"') do work. If the text should even be printed while
   the test is running the string has to start with "\T".

   The function returns the number of variables it printed, not counting
   the format string.
--------------------------------------------------------------------------*/

Var *f_print( Var *v )
{
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	char *sptr;
	int in_format,               // number of wild cards characters
		on_stack,                // number of arguments (beside format string )
		percs,                   // number of `%' characters
		n = 0;                   // number of variables printed
	bool print_anyway = UNSET;

	
	/* A call to print() without any argument prints nothing */

	if ( v == NULL )
		return vars_push( INT_VAR, 0 );

	/* Make sure the first argument is a string */

	vars_check( v, STR_VAR );
	sptr = cp = v->val.sptr;

	/* Do we also have to print while the TEST is running ? */

	if ( *sptr == '\\' && *( sptr + 1 ) == 'T' )
	{
		sptr += 2;
		print_anyway = SET;
	}

	/* Count the number of specifiers `#' in the format string but don't count
	   escaped `#' (i.e "\#"). Also count number of `%' - since this is a
	   format string we need to replace each `%' by a `%%' since printf() and
	   friends use it in the conversion specification. */

	percs = *sptr == '%' ? 1 : 0;
	in_format = *sptr == '#' ? 1 : 0;

	for ( cp = sptr + 1; *cp != '\0'; cp++ )
	{
		if ( *cp == '#' && *( cp - 1 ) != '\\' )
			in_format++;
		if ( *cp == '%' )
			percs++;
	}

	/* Check and count number of variables on the stack following the format
	   string */

	for  ( cv = v->next, on_stack = 0; cv != NULL; ++on_stack, cv = cv->next )
		vars_check( cv, INT_VAR | FLOAT_VAR | STR_VAR );

	/* Check that there are at least as many variables are on the stack 
	   as there specifiers in the format string */

	if ( on_stack < in_format )
		eprint( SEVERE, "%s:%ld: Less data than format descriptors in "
				"`print()' format string.\n", Fname, Lc );

	/* Utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in "
				"`print()' format string.\n", Fname, Lc );

	/* Get string long enough to replace each `#' by a 3 char sequence
	   plus a '\0' character */

	fmt = get_string( strlen( sptr ) + 3 * in_format + percs + 2 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; cp++ )
	{
		/* Skip normal characters */

		if ( *cp != '\\' && *cp != '#' && *cp != '%' )
			continue;

		/* Convert format descriptor (un-escaped `#') to 4 \x01 (as long as
           there are still variables to be printed) */

		if ( *cp == '#' )
		{
			if ( on_stack-- )
			{
				memmove( cp + 3, cp, strlen( cp ) + 1 );
				memset( cp, '\x01', 4 );
				cp += 3;
				n++;
			}
			continue;
		}

		/* Double all `%'s */

		if ( *cp == '%' )
		{
			memmove( cp + 1, cp, strlen( cp ) + 1 );
			cp++;
			continue;
		}

		/* Replace escape sequences */

		switch ( *( cp + 1 ) )
		{
			case '#' :
				*cp = '#';
				break;

			case 'n' :
				*cp = '\n';
				break;

			case 't' :
				*cp = '\t';
				break;

			case '\\' :
				*cp = '\\';
				break;

			case '\"' :
				*cp = '"';
				break;

			default :
				eprint( WARN, "%s:%ld: Unknown escape sequence \\%c in "
						"`print()' format string.\n", Fname, Lc, *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}
		
		memmove( cp + 1, cp + 2, strlen( cp ) - 1 );
	}

	/* Now lets start printing... */

	cp = fmt;
	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			if ( ! TEST_RUN || print_anyway )
				switch ( cv->type )
				{
					case INT_VAR :
						strcpy( ep, "%ld" );
						eprint( NO_ERROR, cp, cv->val.lval );
						break;

					case FLOAT_VAR :
						strcpy( ep, "%#g" );
						eprint( NO_ERROR, cp, cv->val.dval );
						break;

					case STR_VAR :
						strcpy( ep, "%s" );
						eprint( NO_ERROR, cp, cv->val.sptr );
						break;

					default :
						assert( 1 == 0 );
				}

			cv = cv->next;
		}

		cp = ep + 4;
	}

	if ( ! TEST_RUN || print_anyway ) 
		eprint( NO_ERROR, cp );

	/* Finally free the copy of the format string and return number of
	   printed variables */

	T_free( fmt );
	return vars_push( INT_VAR, n );
}


/*-------------------------------------------------------------------*/
/* f_wait() is kind of a version of usleep() that isn't disturbed by */
/* signals - only exception: If a DO_QUIT signal is delivered to the */
/* caller of f_wait() (i.e. the child) it returns immediately.       */
/* ->                                                                */
/*  * number of seconds to sleep                                     */
/*-------------------------------------------------------------------*/

Var *f_wait( Var *v )
{
	struct itimerval sleepy;
	struct sigaction sact;
	double how_long;
	double secs;


	vars_check( v, INT_VAR | FLOAT_VAR );

	how_long = VALUE( v );

	if ( how_long < 0.0 )
	{
		eprint( WARN, "%s:%ld: Negative time in call of function `wait'.\n",
				Fname, Lc );
		if ( TEST_RUN )
			return vars_push( INT_VAR, 1 );
		return vars_push( INT_VAR, do_quit ? 0 : 1 );
	}

	if ( how_long > LONG_MAX )
	{
		eprint( FATAL, "%s:%ld: Time of more that %ld seconds as argument "
				"of `wait()' function.\n", Fname, Lc, LONG_MAX );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* If the child has been asked to stop it won't wait anymore */

	if ( do_quit )
		return vars_push( INT_VAR, 0 );

	/* Set everything up for sleeping */

    sleepy.it_interval.tv_sec = sleepy.it_interval.tv_usec = 0;
	sleepy.it_value.tv_usec = lround( modf( how_long, &secs ) * 1.0e6 );
	sleepy.it_value.tv_sec = ( long ) secs;

	/* Now here comes the tricky part: We have to wait for either the SIGALRM
	   or the DO_QUIT signal but must be immune to DO_SEND signals. A naive
	   implementation would define a variable 'is_alarm' to be set in the
	   signal handler for SIGALRM, initialize it to zero and then do

	   setitimer( ITIMER_REAL, &sleepy, NULL );
	   while( ! is_alarm && ! do_quit )
	       pause( );

	   Unfortunately, there is a nasty race condition: If one of the signal
	   handlers get executed between the check for its flag and the pause()
	   the pause() gets started even though it shouldn't. And this actually
	   happens sometimes, freezing the child process.

	   To avoid this race condition a sigsetjmp() is executed before calling
	   setitimer() and starting to wait. Since the SIGALRM can't have happend
	   yet the sigsetjmp( ) will always return 0. Now we set a flag telling
	   the signal handler that the jump buffer is initialized. Next we start
	   setitimer() and finally start the pause(). This is done in an endless
	   loop because the pause() can also be ended by a DO_SEND signal which we
	   have to ignore. In the signal handler for both SIGALRM and the DO_QUIT
	   signal a siglongjmp() is done if the 'can_jmp_alrm' flag is set,
	   getting us to the point after the 'if'-block because it will always
	   return with to the point where we called sigsetjmp() with a non-zero
	   return value.

	   Actually, there is still one race condition left: When the DO_QUIT
	   signal comes in after the sigsetjmp() but before 'can_jmp_alrm' is set
	   the signal handler will not siglongjmp() but simply return. In this
	   (fortunately rather improbable case) the DO_QUIT signal will get lost
	   and pause() will be run until the SIGALRM is triggered. But the only
	   scenario for this to happen is when the user clicks on the stop button
	   at an really unlucky moment. And if the wait period isn't too long he
	   will probably never notice that he triggered the race condition -
	   otherwise he simply has to click the stop button a second time.  */

	if ( sigsetjmp( alrm_env, 1 ) == 0 )
	{
		can_jmp_alrm = 1;
		setitimer( ITIMER_REAL, &sleepy, NULL );
		for( ; ; )
			pause( );
	}

	/* Return 1 if end of sleeping time was reached, 0 if 'do_quit' was set.
	   In case the wait was ended because of a DO_QUIT signal we have to set
	   the handling of SIGALRM to ignore, because after receipt of a
	   'do_quit' signal the timer may still be running and the finally
	   arriving signal could kill the child prematurely. */

	if ( do_quit )
	{
		sact.sa_handler = SIG_IGN;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( SIGALRM, &sact, NULL ) < 0 )
			_exit( -1 );
	}

	return vars_push( INT_VAR, do_quit ? 0 : 1 );
}


/*-------------------------------------------------------------------*/
/* f_init_1d() has to be called to initialise the display system for */
/* 1-dimensional experiments.                                        */
/* Arguments:                                                        */
/* 1. Number of curves to be shown (optional, defaults to 1)         */
/* 2. Number of points (optional, 0 or negative if unknown)          */
/* 3. Real world coordinate and increment (optional)                 */
/* 4. x-axis label (optional)                                        */
/* 5. y-axis label (optional)                                        */
/*-------------------------------------------------------------------*/

Var *f_init_1d( Var *v )
{
	/* Set some default values */

	G.dim = 1;
	G.is_init = SET;
	G.nc = 1;
	G.nx = DEFAULT_1D_X_POINTS;
	G.rwc_start[ X ] = ( double ) ARRAY_OFFSET;
	G.rwc_delta[ X ] = 1.0;
	G.label[ X ] = G.label[ Z ] = G.label[ Z ] = NULL;


	if ( v == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_1d;

	vars_check( v, INT_VAR | FLOAT_VAR );             /* number of curves */

	if ( v->type == INT_VAR )
		G.nc = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				      "curves in `init_1d()'.\n", Fname, Lc );
		G.nc = lround( v->val.dval );
	}

	if ( G.nc < 1 || G.nc > MAX_CURVES )
	{
		eprint( FATAL, "%s:%ld: Invalid number of curves (%ld) in "
				       "`init_1d()'.\n", Fname, Lc, G.nc );
		THROW( EXCEPTION );
	}

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_1d;

	vars_check( v, INT_VAR | FLOAT_VAR );      /* # of points in x-direction */

	if ( v->type == INT_VAR )
		G.nx = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				      "points in `init_1d()'.\n", Fname, Lc );
		G.nx = lround( v->val.dval );
	}

	if ( G.nx <= 0 )
		G.nx = DEFAULT_1D_X_POINTS;

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_1d;

	/* If next value is an integer or a float this is the real world
	   x coordinate followed by the increment */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL || 
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			eprint( FATAL, "%s:%ld: Real word coordinate given but no "
					       "increment in `init_1d()'." );
			THROW( EXCEPTION );
		}

		G.rwc_start[ X ] = VALUE( v );
		v = v->next;
		G.rwc_delta[ X ] = VALUE( v );

		if ( G.rwc_delta[ X ] == 0.0 )
		{
			G.rwc_start[ X ] = ARRAY_OFFSET;
			G.rwc_delta[ X ] = 1.0;
		}
	}

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

labels_1d:

	vars_check ( v, STR_VAR );
	G.label[ X ] = T_strdup( v->val.sptr );

	if ( ( v = v->next ) != NULL )
	{
		vars_check ( v, STR_VAR );
		G.label[ Y ] = T_strdup( v->val.sptr );
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/* f_init_2d() has to be called to initialise the display system for */
/* 2-dimensional experiments.                                        */
/*-------------------------------------------------------------------*/

Var *f_init_2d( Var *v )
{
	/* Set some default values */

	G.dim = 2;
	G.is_init = SET;
	G.nc = 1;
	G.nx = DEFAULT_2D_X_POINTS;
	G.ny = DEFAULT_2D_Y_POINTS;
	G.rwc_start[ X ] = G.rwc_start[ Y ] = ( double ) ARRAY_OFFSET;
	G.rwc_delta[ X ] = G.rwc_delta[ Y ] = 1.0; 
	G.label[ X ] = G.label[ Z ] = G.label[ Z ] = NULL;


	if ( v == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_2d;

	vars_check( v, INT_VAR | FLOAT_VAR );                /* number of curves */

	if ( v->type == INT_VAR )
		G.nc = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				      "curves in `init_1d()'.\n", Fname, Lc );
		G.nc = lround( v->val.dval );
	}

	if ( G.nc < 1 || G.nc > MAX_CURVES )
	{
		eprint( FATAL, "%s:%ld: Invalid number of curves (%ld) in "
				"`init_1d()'.\n", Fname, Lc, G.nc );
		THROW( EXCEPTION );
	}

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_2d;

	vars_check( v, INT_VAR | FLOAT_VAR );      /* # of points in x-direction */

	if ( v->type == INT_VAR )
		G.nx = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				      "points in x-direction.\n", Fname, Lc );
		G.nx = lround( v->val.dval );
	}

	if ( G.nx <= 0 )
		G.nx = DEFAULT_2D_X_POINTS;

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_2d;

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		G.ny = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Floating point value used as number of "
				      "points in y-direction.\n", Fname, Lc );
		G.ny = lround( v->val.dval );
	}

	if ( G.ny <= 0 )
		G.ny = DEFAULT_2D_Y_POINTS;

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_2d;

	/* Now we expect 2 real world x coordinates (start and delta for
	   x-direction) */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL ||
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			eprint( FATAL, "%s:%ld: Incomplete real world x coordinates "
					       "in `init_2d()'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		G.rwc_start[ X ] = VALUE( v );
		v = v->next;
		G.rwc_delta[ X ] = VALUE( v );

		if ( G.rwc_delta[ X ] == 0.0 )
		{
			G.rwc_start[ X ] = ( double ) ARRAY_OFFSET;
			G.rwc_delta[ X ] = 1.0;
		}
	}

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	if ( v->type == STR_VAR )
		goto labels_2d;

	/* Now we expect 2 real world y coordinates (start and delta for
	   x-direction) */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL ||
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			eprint( FATAL, "%s:%ld: Incomplete real world y coordinates "
				 	        "in `init_2d()'.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		G.rwc_start[ Y ] = VALUE( v );
		v = v->next;
		G.rwc_delta[ Y ] = VALUE( v );
		
		if ( G.rwc_delta[ Y ] == 0.0 )
		{
			G.rwc_start[ Y ] = ( double ) ARRAY_OFFSET;
			G.rwc_delta[ Y ] = 1.0;
		}
	}

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

labels_2d:

	vars_check( v, STR_VAR );
	G.label[ X ] = T_strdup( v->val.sptr );

	if ( ( v = v->next ) == NULL )
		return vars_push( INT_VAR, 1 );

	vars_check( v, STR_VAR );
	G.label[ Y ] = T_strdup( v->val.sptr );

	if ( ( v = v->next ) != NULL )
	{
		vars_check( v, STR_VAR );
		G.label[ Z ] = T_strdup( v->val.sptr );
	}

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/


Var *f_cscale( Var *v )
{
	double x_0, y_0, dx, dy;
	int is_set = 0;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	void *ptr;
	int type = D_CHANGE_SCALE;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		eprint( WARN, "%s:%ld: Can't change scale, missing initialization.\n",
				Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing parameter in call of "
				"`change_scale'.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		x_0 = VALUE( v );
		is_set = 1;
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		is_set <<= 1;
		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			dx = VALUE( v );
			is_set++;
		}
	}

	if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
	{
		if ( G.dim == 1 )
		{
			eprint( FATAL, "%s:%ld: With 1D graphics only the x-sclae can be "
					"changed.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		is_set <<= 1;
		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			y_0 = VALUE( v );
			is_set++;
		}
	}

	if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
	{
		is_set <<= 1;
		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		{
			dy = VALUE( v );
			is_set++;
		}
	}

	/* In a test run we're already done */

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* Function can only be used in experiment section */

	assert( I_am == CHILD );

	len =   sizeof( len )                 /* length field itself */
		  + 2 * sizeof( int )             /* type field and flags */
		  + 4 * sizeof( double );         /* new scale settings */

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( dp );
		eprint( FATAL, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = buf;

	memcpy( ptr, &len, sizeof( long ) );               /* total length */
	ptr += sizeof( long );

	memcpy( ptr, &type, sizeof( int ) );               /* type indicator  */
	ptr += sizeof( int );

	memcpy( ptr, is_set, sizeof( int ) );              /* flags */
	ptr += sizeof( int );

	memcpy( ptr, x_0, sizeof( int ) );                 /* new x-offset */
	ptr += sizeof( double );

	memcpy( ptr, dx, sizeof( int ) );                  /* new x-increment */
	ptr += sizeof( double );

	memcpy( ptr, y_0, sizeof( int ) );                 /* new y-offset */
	ptr += sizeof( double );

	memcpy( ptr, dy, sizeof( int ) );                  /* new y-increment */
	ptr += sizeof( double );

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	sema_wait( semaphore );
	Key->shm_id = shm_id;
	Key->type = DATA;
	kill( getppid( ), NEW_DATA );

	/* That's all, folks... */

	return vars_push( INT_VAR, 1 );
}

/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display( Var *v )
{
	DPoint *dp;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	void *ptr;
	int nsets;
	int i;


	/* We can't display data without a previous initialization */

	if ( ! G.is_init )
	{
		if ( ! G.is_warn )                         /* warn only once */
		{
			eprint( WARN, "%s:%ld: Can't display data, missing "
					"initialisation.\n", Fname, Lc );
			G.is_warn = SET;
		}

		return vars_push( INT_VAR, 0 );
	}

	/* Check the arguments and get them into some reasonable form */

	dp = eval_display_args( v, &nsets );

	if ( TEST_RUN )
	{
		T_free( dp );
		return vars_push( INT_VAR, 1 );
	}

	assert( I_am == CHILD );

	/* Determine the needed amount of shared memory */

	len =   sizeof( len )                 /* length field itself */
		  + sizeof( int )                 /* number of sets to be sent */
		  + 3 * nsets * sizeof( long )    /* x-, y-index and curve */
		  + nsets * sizeof( int );        /* data type */
	
	for ( i = 0; i < nsets; i++ )
	{
		switch( dp[ i ].v->type )
		{
			case INT_VAR :
				len += sizeof( long );
				break;

			case FLOAT_VAR :
				len += sizeof( double );
				break;

			case ARR_PTR :
				len += sizeof( long );
				if ( dp[ i ].v->from->type == INT_ARR )
					len += dp[ i ].v->from->sizes[ dp[ i ].v->from->dim - 1 ]
						   * sizeof( long );
				else
					len += dp[ i ].v->from->sizes[ dp[ i ].v->from->dim - 1 ]
						   * sizeof( double );
				break;

			case ARR_REF :
				if ( dp[ i ].v->from->dim != 1 )
				{
					eprint( FATAL, "%s:%ld: Only one-dimensional arrays or "
							"slices of more-dimensional arrays can be "
							"displayed.\n", Fname, Lc );
					T_free( dp );
					THROW( EXCEPTION );
				}

				len += sizeof( long );

				if ( dp[ i ].v->from->type == INT_ARR )
					len += dp[ i ].v->from->sizes[ 0 ] * sizeof( long );
				else
					len += dp[ i ].v->from->sizes[ 0 ] * sizeof( double );
				break;

			case INT_TRANS_ARR :
				len += sizeof( long );
				len += dp[ i ].v->len * sizeof( long );
				break;

			case FLOAT_TRANS_ARR :
				len += sizeof( long );
				len += dp[ i ].v->len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, "Internal communication error at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( dp );
		eprint( FATAL, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = buf;

	memcpy( ptr, &len, sizeof( long ) );               /* total length */
	ptr += sizeof( long );

	memcpy( ptr, &nsets, sizeof( int ) );              /* # data sets  */
	ptr += sizeof( int );

	for ( i = 0; i < nsets; i++ )
	{
		memcpy( ptr, &dp[ i ].nx, sizeof( long ) );     /* x-index */
		ptr += sizeof( long );

		memcpy( ptr, &dp[ i ].ny, sizeof( long ) );     /* y-index */
		ptr += sizeof( long );

		memcpy( ptr, &dp[ i ].nc, sizeof( long ) );     /* curve number */
		ptr += sizeof( int );

		switch( dp[ i ].v->type )                       /* and now the data  */
		{
			case INT_VAR :
				memcpy( ptr, &dp[ i ].v->type, sizeof( int ) );
				ptr += sizeof( int );
				memcpy( ptr, &dp[ i ].v->val.lval, sizeof( long ) );
				ptr += sizeof( long );
				break;

			case FLOAT_VAR :
				memcpy( ptr, &dp[ i ].v->type, sizeof( int ) );
				ptr += sizeof( int );
				memcpy( ptr, &dp[ i ].v->val.dval, sizeof( double ) );
				ptr += sizeof( double );
				break;

			case ARR_PTR :
				assert( dp[ i ].v->from->type == INT_ARR ||
						dp[ i ].v->from->type == FLOAT_ARR );
				memcpy( ptr, &dp[ i ].v->from->type, sizeof( int ) );
				ptr += sizeof( int );

				len = dp[ i ].v->from->sizes[ dp[ i ].v->from->dim - 1 ];
				memcpy( ptr, &len, sizeof( long ) );
				ptr += sizeof( long );

				if ( dp[ i ].v->from->type == INT_ARR )
				{
					memcpy( ptr, dp[ i ].v->val.gptr,
							len * sizeof( long ) );
					ptr += len * sizeof( long );
				}
				else
				{
					memcpy( ptr, dp[ i ].v->val.gptr,
							len * sizeof( double ) );
					ptr += len * sizeof( double );
				}
				break;

			case ARR_REF :
				assert( dp[ i ].v->from->type == INT_ARR ||
						dp[ i ].v->from->type == FLOAT_ARR );
				memcpy( ptr, &dp[ i ].v->from->type, sizeof( int ) );
				ptr += sizeof( int );
					
				len = dp[ i ].v->from->sizes[ 0 ];
				memcpy( ptr, &len, sizeof( long ) );
				ptr += sizeof( long );

				if ( dp[ i ].v->from->type == INT_ARR )
				{
					memcpy( ptr, dp[ i ].v->from->val.lpnt,
							len * sizeof( long ) );
					ptr += len * sizeof( long );
				}
				else
				{
					memcpy( ptr, dp[ i ].v->from->val.dpnt,
							len * sizeof( double ) );
					ptr += len * sizeof( double );
				}
				break;

			case INT_TRANS_ARR :
				*( ( int * ) ptr ) = INT_ARR;
				ptr += sizeof( int );

				len = dp[ i ].v->len;
				memcpy( ptr, &len, sizeof( long ) );
				ptr += sizeof( long );

				memcpy( ptr, dp[ i ].v->val.lpnt,
						len * sizeof( long ) );
				ptr += len * sizeof( long );
				break;

			case FLOAT_TRANS_ARR :
				*( ( int * ) ptr ) = FLOAT_ARR;
				ptr += sizeof( int );

				len = dp[ i ].v->len;
				memcpy( ptr, &len, sizeof( long ) );
				ptr += sizeof( long );

				memcpy( ptr, dp[ i ].v->val.lpnt,
						len * sizeof( double ) );
				ptr += len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, "Internal communication error at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Get rid of the array of structures returned by eval_display_args() */

	T_free( dp );
	
	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	sema_wait( semaphore );
	Key->shm_id = shm_id;
	Key->type = DATA;
	kill( getppid( ), NEW_DATA );

	/* That's all, folks... */

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static DPoint *eval_display_args( Var *v, int *nsets )
{
	DPoint *dp = NULL;


	*nsets = 0;
	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: Missing x-index in `display()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	do
	{
		/* Get (more) memory for the sets */

		dp = T_realloc( dp, ( *nsets + 1 ) * sizeof( DPoint ) );

		/* check and store the x-index */

		vars_check( v, INT_VAR | FLOAT_VAR );
	
		if ( v->type == INT_VAR )
			dp[ *nsets ].nx = v->val.lval - ARRAY_OFFSET;
		else
			dp[ *nsets ].nx = lround( v->val.dval - ARRAY_OFFSET );

		if ( dp[ *nsets ].nx < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid x-index (= %ld) in `display()'.\n",
					Fname, Lc, dp[ *nsets ].nx + ARRAY_OFFSET );
			T_free( dp );
			THROW( EXCEPTION );
		}

		v = v->next;

		/* for 2D experiments test and get y-index */

		if ( G.dim == 2 )
		{
			if ( v == NULL )
			{
				eprint( FATAL, "%s:%ld: Missing y-index in `display()'.\n",
						Fname, Lc );
				T_free( dp );
				THROW( EXCEPTION );
			}

			vars_check( v, INT_VAR | FLOAT_VAR );
	
			if ( v->type == INT_VAR )
				dp[ *nsets ].ny = v->val.lval - ARRAY_OFFSET;
			else
				dp[ *nsets ].ny = lround( v->val.dval - ARRAY_OFFSET );

			if ( dp[ *nsets ].ny < 0 )
			{
				eprint( FATAL, "%s:%ld: Invalid y-index (= %ld) in "
						"`display()'.\n",
						Fname, Lc, dp[ *nsets ].ny + ARRAY_OFFSET );
				T_free( dp );
				THROW( EXCEPTION );
			}

			v = v->next;
		}

		/* Now test and get the data, i. e. store the pointer to the variable
		   containing it */

		if ( v == NULL )
		{
			eprint( FATAL, "%s:%ld: Missing data in `display()'.\n",
					Fname, Lc );
			T_free( dp );
			THROW( EXCEPTION );
		}

		vars_check( v, INT_VAR | FLOAT_VAR | ARR_PTR | ARR_REF |
					   INT_TRANS_ARR | FLOAT_TRANS_ARR );

		dp[ *nsets ].v = v;

		v = v->next;

		/* There can be several curves and we check if there's a curve number,
		then we test and store it. If there are no more argument we default to
		the first curve.*/

		if ( v == NULL )
		{
			dp[ *nsets ].nc = 0;
			( *nsets )++;
			return dp;
		}

		vars_check( v, INT_VAR | FLOAT_VAR );
	
		if ( v->type == INT_VAR )
			dp[ *nsets ].nc = v->val.lval - 1;
		else
			dp[ *nsets ].nc = lround( v->val.dval - 1 );

		if ( dp[ *nsets ].nc < 0 || dp[ *nsets ].nc >= G.nc )
		{
			eprint( FATAL, "%s:%ld: Invalid curve number (%ld) in "
					"`display()'.\n", Fname, Lc, dp[ *nsets ].nc + 1 );
			T_free( dp );
			THROW( EXCEPTION );
		}

		v = v->next;

		( *nsets )++;
	} while ( v != NULL );

	return dp;
}


/*-------------------------------------------------*/
/* Function makes all points of a curve invisible. */
/*-------------------------------------------------*/

Var *f_clearcv( Var *v )
{
	long curve;
	long count = 0;
	long *ca = NULL;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	void *ptr;
	int type = D_CLEAR_CURVE;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		if ( TEST_RUN )
			eprint( WARN, "$s:%ld: Can't clear curve, missing graphics "
					"initialisation.\n!\n", Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	/* If there is no argument default to curve 1 */

	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( INT_VAR, 1 );

		ca = T_malloc( sizeof( long ) );
		*ca = 0;
		count = 1;
	}
	else
	{
		/* Otherwise run through all arguments, treating each as a new curve
		   number */

		do
		{
			/* Check variable type and get curve number */

			vars_check( v, INT_VAR | FLOAT_VAR );     /* get number of curve */

			if ( v->type == INT_VAR )
				curve = v->val.lval;
			else
			{
				if ( TEST_RUN )
					eprint( WARN, "%s:%ld: Floating point value used as curve "
							"number in function `clear_curve'.\n", Fname, Lc );
				curve = lround( v->val.dval );
			}

			/* Make sure the curve exists */

			if ( curve < 0 || curve > G.nc )
			{
				if ( TEST_RUN )
					eprint( SEVERE, "%s:%ld: Can't clear curve %ld, curve "
							"does not exist.\n", Fname, Lc, curve );
				continue;
			}

			/* Store curve number */

			ca = T_realloc( ca, ( count + 1 ) * sizeof( long ) );
			ca[ count++ ] = curve - 1;
		} while ( ( v = v->next ) != NULL );
		
		if ( ca == NULL )
		{
			eprint( SEVERE, "%s:%ld: No valid argument found in function "
					"`clear_curve'.\n", Fname, Lc );
			return vars_push( INT_VAR, 0 );
		}
	}

	/* In a test run this all there is to be done */

	if ( TEST_RUN )
	{
		T_free( ca );
		return vars_push( INT_VAR, 1 );
	}

	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	assert( I_am == CHILD );

	/* Now try to get a shared memory segment */

	len = sizeof( int ) + count * sizeof( long );

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( ca );
		eprint( FATAL, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = buf;

	memcpy( ptr, &len, sizeof( long ) );           /* total length */
	ptr += sizeof( long );

	memcpy( ptr, &type, sizeof( int ) );           /* type indicator  */
	ptr += sizeof( int );

	memcpy( ptr, &count, sizeof( long ) );         /* curve number count */
	ptr += sizeof( long );

	memcpy( ptr, ca, count * sizeof( long ) );     /* array of curve numbers */

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Get rid of the array of curve numbers */

	T_free( ca );
	
	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	sema_wait( semaphore );
	Key->shm_id = shm_id;
	Key->type = DATA;
	kill( getppid( ), NEW_DATA );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------------*/
/* Function allows the user to select a file using the file selector. If the */
/* already file exists confirmation by the user is required. Then the file   */
/* is opened - if this fails the file selector is shown again. The FILE      */
/* pointer returned is stored in an array of FILE pointers for each of the   */
/* open files. The returned value is an INT_VAR with the index in the FILE   */
/* pointer array or -1 if no file was selected.                              */
/* (Optional) Input variables (each will replaced by a default string if the */
/* argument is either NULL or the empty string) :                            */
/* 1. Message string (not allowed to start with a backslash `\'!)            */
/* 2. Default pattern for file name                                          */
/* 3. Default directory                                                      */
/* 4. Default file name                                                      */
/* Alternatively, to hardcode a file name into the EDL program only send the */
/* file name instead of the message string, but with a backslash `\' as the  */
/* very first character (it will be skipped and not be used as part of the   */
/* file name). The other strings still can be set but will only be used if   */
/* opening the file fails.                                                   */
/*---------------------------------------------------------------------------*/

Var *f_getf( Var *var )
{
	Var *cur;
	int i;
	char *s[ 4 ] = { NULL, NULL, NULL, NULL };
	FILE *fp;
	long len;
	struct stat stat_buf;
	static char *r = NULL;
	static FILE_LIST *old_File_List;


	r = NULL;
	old_File_List = NULL;

	/* If there was a call of `f_save()' without a previous call to `f_getf()'
	   `f_save()' did already call `f_getf()' by itself and now don't expects
	   file identifiers anymore - in this case `No_File_Numbers' is set. So,
	   if we get a call to `f_getf()' while `No_File_Numbers' is set we must
	   tell the user that he can't have it both ways, i.e. (s)he either has to
	   call `f_getf()' before any call to `f_save()' or never. */

	if ( No_File_Numbers )
	{
		eprint( FATAL, "%s:%ld: Call of `get_filename()' after call of "
				"`save()' without previous call of `get_filename()'.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* While test is running just return a dummy value */

	if ( TEST_RUN )
		return vars_push( INT_VAR, File_List_Len++ );

	/* Check the arguments and supply default values if necessary */

	for ( i = 0, cur = var; i < 4 && cur != NULL; i++, cur = cur->next )
	{
		vars_check( cur, STR_VAR );
		s[ i ] = cur->val.sptr;
	}

	/* First string is the message */

	if ( s[ 0 ] != NULL && s[ 0 ][ 0 ] == '\\' )
		r = T_strdup( s[ 0 ] + 1 );

	if ( s[ 0 ] == NULL || s[ 0 ] == "" || s[ 0 ][ 0 ] == '\\' )
		s[ 0 ] = T_strdup( "Please enter a file name:" );
	else
		s[ 0 ] = T_strdup( s[ 0 ] );

	/* Second string is the is the file name pattern */

	if ( s[ 1 ] == NULL || s[ 1 ] == "" )
		s[ 1 ] = T_strdup( "*.dat" );
	else
		s[ 1 ] = T_strdup( s[ 1 ] );

	/* Third string is the default directory */

	if ( s[ 2 ] == NULL || s[ 2 ] == "" )
	{
		s[ 2 ] = NULL;
		len = 0;

		do
		{
			len += PATH_MAX;
			s[ 2 ] = T_realloc( s[ 2 ], len );
			getcwd( s[ 2 ], len );
		} while ( s[ 2 ] == NULL && errno == ERANGE );

		if ( s[ 2 ] == NULL )
			s[ 2 ] = T_strdup( "" );
	}
	else
		s[ 2 ] = T_strdup( s[ 2 ] );

	if ( s[ 3 ] == NULL )
		s[ 3 ] = T_strdup( "" );
	else
		s[ 3 ] = T_strdup( s[ 3 ] );
		   
getfile_retry:

	/* Try to get a filename - on 'Cancel' request confirmation (unless a
	   file name was passed to the routine and this is not a repeat call) */

	if ( r == NULL )
		r = T_strdup( show_fselector( s[ 0 ], s[ 2 ], 
											 s[ 1 ], s[ 3 ] ) );

	if ( ( r == NULL || *r == '\0' ) &&
		 show_choices( "Do you really want to cancel saving data?\n"
					   "        The data will be lost!",
					   2, "Yes", "No", NULL, 2 ) != 1 )
	{
		r = T_free( r );
		goto getfile_retry;
	}

	if ( r == NULL || *r == '\0' )         /* on 'Cancel' with confirmation */
	{
		T_free( r );
		for ( i = 0; i < 4; i++ )
			T_free( s[ i ] );
		return vars_push( INT_VAR, -1 );
	}

	/* Now ask for confirmation if the file already exists and try to open
	   it for writing */

	if  ( 0 == stat( r, &stat_buf ) &&
		  1 != show_choices( "The selected file does already exist!\n"
							 " Do you really want to overwrite it?",
							 2, "Yes", "No", NULL, 2 ) )
	{
		r = T_free( r );
		goto getfile_retry;
	}

	if ( ( fp = fopen( r, "w+" ) ) == NULL )
	{
		switch( errno )
		{
			case EMFILE :
				show_message( "Sorry, you have too many open files!\n"
							  "Please close at least one and retry." );
				break;

			case ENFILE :
				show_message( "Sorry, system limit for open files exceeded!\n"
							  " Please try to close some files and retry." );
				break;

			case ENOSPC :
				show_message( "Sorry, no space left on device for more file!\n"
							  "    Please delete some files and retry." );
				break;

			default :
				show_message( "Sorry, can't open selected file for writing!\n"
							  "       Please select a different file." );
		}

		r = T_free( r );
		goto getfile_retry;
	}

	for ( i = 0; i < 4; i++ )
		T_free( s[ i ] );

	/* The reallocation for the File_List may file but we still need to close
	   all files and get rid of memory for the file names, thus we save the
	   current File_List before we try to reallocate */

	if ( File_List )
	{
		old_File_List = T_malloc( File_List_Len * sizeof( FILE_LIST ) );
		memcpy( old_File_List, File_List,
				File_List_Len * sizeof( FILE_LIST ) );
	}

	TRY
	{
		File_List = T_realloc( File_List, 
							   ( File_List_Len + 1 ) * sizeof( FILE_LIST ) );
		if ( old_File_List != NULL )
			T_free( old_File_List );
		TRY_SUCCESS;
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
	{
		File_List = old_File_List;
		THROW( EXCEPTION );
	}

	File_List[ File_List_Len ].fp = fp;
	File_List[ File_List_Len ].name = r;

	/* Switch buffering of so we're sure everything gets written to disk
	   immediately */

	setbuf( File_List[ File_List_Len ].fp, NULL );

	return vars_push( INT_VAR, File_List_Len++ );
}


/*---------------------------------------------------------------------*/
/* This function is called by the functions for saving. If they didn't */
/* get a file identifier it is assumed the user wants just one file    */
/* that is opened at the first call of a function of the `save_xxx()'  */
/* family of functions.                                                */
/*---------------------------------------------------------------------*/

static int get_save_file( Var **v, const char *calling_function )
{
	Var *get_file_ptr;
	Var *file;
	int file_num;
	int access;


	/* If no file has been selected yet get a file and then use it exclusively
	   (i.e. also expect that no file identifier is given in later calls),
	   otherwise the first variable has to be the file identifier */

	if ( File_List_Len == 0 )
	{
		if ( Dont_Save )
			return -1;

		No_File_Numbers = UNSET;

		get_file_ptr = func_get( "get_file", &access );
		file = func_call( get_file_ptr );         /* get the file name */

		No_File_Numbers = SET;

		if ( file->val.lval < 0 )
		{
			vars_pop( file );
			Dont_Save = SET;
			return -1;
		}
		vars_pop( file );
		file_num = 0;
	}
	else if ( ! No_File_Numbers )                    /* file number is given */
	{
		if ( *v != NULL )
		{
			/* Check that the first variable is an integer, i.e. can be a
			   file identifier */

			if ( ( *v )->type != INT_VAR )
			{
				eprint( FATAL, "%s:%ld: First argument in `%s' isn't a "
						"file identifier.\n", Fname, Lc, calling_function );
				THROW( EXCEPTION );
			}
			file_num = ( int ) ( *v )->val.lval;
		}
		else
		{
			eprint( WARN, "%s:%ld: Call of `%s' without any arguments.\n",
					Fname, Lc, calling_function );
			return -1;
		}
		*v = ( *v )->next;
	}
	else
		file_num = 0;

	/* Check that the file identifier is reasonable */

	if ( file_num < 0 )
	{
		eprint( WARN, "%s:%ld: File has never been opened, skipping "
				"`%s' command.\n", Fname, Lc, calling_function );
		return -1;
	}

	if ( file_num >= File_List_Len )
	{
		eprint( FATAL, "%s:%ld: Invalid file identifier in `%s'.\n",
				Fname, Lc, calling_function );
		THROW( EXCEPTION );
	}

	return file_num;
}


/*--------------------------------*/
/* Closes all opened output files */
/*--------------------------------*/

void close_all_files( void )
{
	int i;

	if ( File_List == NULL )
	{
		File_List_Len = 0;
		return;
	}

	for ( i = 0; i < File_List_Len; i++ )
	{
		if ( File_List[ i ].name )
			T_free( File_List[ i ].name );
		if ( File_List[ i ].fp )
			fclose( File_List[ i ].fp );
	}

	T_free( File_List );
	File_List = NULL;
	File_List_Len = 0;
}


/*--------------------------------------------------------------------------*/
/* Saves data to a file. If `get_file()' hasn't been called yet it will be  */
/* called now - in this case the file opened this way is the only file to   */
/* be used and no file identifier is allowed as first argument to `save()'. */
/* This version of save writes the data in an unformatted way, i.e. each    */
/* on its own line with the only exception of arrays of more than one       */
/* dimension where a empty line is put between the slices.                  */
/*--------------------------------------------------------------------------*/

Var *f_save( Var *v )
{
	long i;
	long start;
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, "%s:%ld: Call of `save()' without data to save.\n",
				Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	do
	{
		switch( v->type )
		{
			case INT_VAR :
				T_fprintf( file_num, "%ld\n", v->val.lval );
				break;

			case FLOAT_VAR :
				T_fprintf( file_num, "%#.9g\n", v->val.dval );
				break;

			case STR_VAR :
				T_fprintf( file_num, "%s\n", v->val.sptr );
				break;

			case INT_TRANS_ARR :
				for ( i = 0; i < v->len; i++ )
					T_fprintf( file_num, "%ld\n", v->val.lpnt[ i ] );
				break;
				
			case FLOAT_TRANS_ARR :
				for ( i = 0; i < v->len; i++ )
					T_fprintf( file_num, "%#.9g\n", v->val.dpnt[ i ] );
				break;

			case ARR_PTR :
				if ( ! print_slice( v, file_num ) )
					THROW( EXCEPTION );
				break;

			case ARR_REF :
				if ( v->from->flags && NEED_ALLOC )
				{
					eprint( WARN, "%s:%ld: Variable sized array `%s' is still "
							"undefined - skipping'.\n", 
							Fname, Lc, v->from->name );
					break;
				}
				start = 0;
				if ( ! print_array( v->from, 0, &start, file_num ) )
					THROW( EXCEPTION );
				break;

			default :
				assert( 1 == 0 );
		}

		v = v->next;
	} while ( v );

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static bool print_array( Var *v, long cur_dim, long *start, int fid )
{
	long i;

	if ( cur_dim < v->dim - 1 )
	{
		for ( i = 0; i < v->sizes[ cur_dim ]; i++ )
			if ( ! print_array( v, cur_dim + 1, start, fid ) )
				return FAIL;
	}
	else
	{
		for ( i = 0; i < v->sizes[ cur_dim ]; (*start)++, i++ )
		{
			if ( v->type == INT_ARR )
				T_fprintf( fid, "%ld\n", v->val.lpnt[ *start ] );
			else
				T_fprintf( fid, "%#.9g\n", v->val.dpnt[ *start ] );
		}

		T_fprintf( fid, "\n" );
	}

	return OK;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static bool print_slice( Var *v, int fid )
{
	long i;

	for ( i = 0; i < v->from->sizes[ v->from->dim - 1 ]; i++ )
		if ( v->from->type == INT_ARR )
			T_fprintf( fid, "%ld\n", * ( ( long * ) v->val.gptr + i ) );
		else
			T_fprintf( fid, "%#.9g\n", * ( ( double * ) v->val.gptr + i ) );

	return OK;
}


/*--------------------------------------------------------------------------*/
/* Saves data to a file. If `get_file()' hasn't been called yet it will be  */
/* called now - in this case the file opened this way is the only file to   */
/* be used and no file identifier is allowed as first argument to `save()'. */
/* This function is the formated save with the same meaning of the format   */
/* string as in `print()'.                                                  */
/*--------------------------------------------------------------------------*/

Var *f_fsave( Var *v )
{
	int file_num;
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	char *sptr;
	int in_format,
		on_stack,
		percs,
		n = 0;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "fsave()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, "%s:%ld: Call of `fsave()' without format string and "
				"data.\n", Fname, Lc );
		return vars_push( INT_VAR, 0 );
	}

	if ( v->type != STR_VAR )
	{
		eprint( FATAL, "%s:%ld: Missing format string in call of `fsave()'\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	sptr = cp = v->val.sptr;

	/* Count the number of specifiers `#' in the format string but don't count
	   escaped `#' (i.e "\#") */

	percs = *sptr == '%' ? 1 : 0;
	in_format = *sptr == '#' ? 1 : 0;

	for ( cp = sptr + 1; *cp != '\0'; ++cp )
	{
		if ( *cp == '#' && *( cp - 1 ) != '\\' )
			in_format++;
		if ( *cp == '%' )
			percs++;
	}

	/* Check and count number of variables on the stack following the format
	   string */

	for  ( cv = v->next, on_stack = 0; cv != NULL; ++on_stack, cv = cv->next )
		vars_check( cv, INT_VAR | FLOAT_VAR | STR_VAR );

	/* Check that there are at least as many variables are on the stack 
	   as there specifiers in the format string */

	if ( on_stack < in_format )
		eprint( SEVERE, "%s:%ld: Less data than format descriptors in "
				"`save()' format string.\n", Fname, Lc );

	/* Warn if there are more data than format descriptors */

	if ( on_stack > in_format )
		eprint( SEVERE, "%s:%ld: More data than format descriptors in "
				"`save()' format string.\n", Fname, Lc );

	/* Get string long enough to replace each `#' by a 5-char sequence 
	   plus a '\0' */

	fmt = get_string( strlen( sptr ) + 5 * in_format + percs + 2 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; ++cp )
	{
		/* Skip normal characters */

		if ( *cp != '\\' && *cp != '#' && *cp != '%' )
			continue;

		/* Convert format descriptor (un-escaped `#') to 5 \x01 */

		if ( *cp == '#' )
		{
			if ( on_stack-- )
			{
				memmove( cp + 5, cp, strlen( cp ) + 1 );
				memset( cp, '\x01', 6 );
				cp += 5;
				n++;
			}
			continue;
		}

		/* Double each `%' */

		if ( *cp == '%' )
		{
			memmove( cp + 1, cp, strlen( cp ) + 1 );
			cp++;
			continue;
		}

		/* Replace escape sequences */

		switch ( *( cp + 1 ) )
		{
			case '#' :
				*cp = '#';
				break;

			case 'n' :
				*cp = '\n';
				break;

			case 't' :
				*cp = '\t';
				break;

			case '\\' :
				*cp = '\\';
				break;

			case '\"' :
				*cp = '"';
				break;

			default :
				eprint( WARN, "%s:%ld: Unknown escape sequence \\%c in "
						"`save()' format string.\n", Fname, Lc, *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}
		
		memmove( cp + 1, cp + 2, strlen( cp ) - 1 );
	}

	/* Now lets start printing... */

	cp = fmt;
	cv = v->next;
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			switch ( cv->type )
			{
				case INT_VAR :
					strcpy( ep, "%ld" );
					T_fprintf( file_num, cp, cv->val.lval );
					break;

				case FLOAT_VAR :
					strcpy( ep, "%#.9g" );
					T_fprintf( file_num, cp, cv->val.dval );
					break;

				case STR_VAR :
					strcpy( ep, "%s" );
					T_fprintf( file_num, cp, cv->val.sptr );
					break;

				default :
					assert( 1 == 0 );
			}

			cv = cv->next;
		}
		else
		{
			strcpy( ep, "#" );
			T_fprintf( file_num, cp );
		}

		cp = ep + 6;
	}

	T_fprintf( file_num, cp );

	/* Finally free the copy of the format string and return */

	T_free( fmt );

	return vars_push( INT_VAR, n );
}


/*-------------------------------------------------------------------------*/
/* Saves the EDL program to a file. If `get_file()' hasn't been called yet */
/* it will be called now - in this case the file opened this way is the    */
/* only file to be used and no file identifier is allowed as first argu-   */
/* ment to `save()'.                                                       */
/* Beside the file identifier the other (optional) parameter is a string   */
/* that gets prepended to each line of the EDL program (i.e. a comment     */
/* character).                                                             */
/*-------------------------------------------------------------------------*/

Var *f_save_p( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_program()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	if ( ! print_browser( 0, file_num, v != NULL ? v->val.sptr : "" ) )
		THROW( EXCEPTION );

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------------------*/
/* Saves the content of the output window to a file. If `get_file()' hasn't */
/* been called yet it will be called now - in this case the file opened     */
/* this way is the only file to be used and no file identifier is allowed   */
/* as first argument to `save()'.                                           */
/* Beside the file identifier the other (optional) parameter is a string    */
/* that gets prepended to each line of the output (i.e. a comment char).    */
/*--------------------------------------------------------------------------*/

Var *f_save_o( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_output()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	if ( ! print_browser( 1, file_num, v != NULL ? v->val.sptr : "" ) )
		THROW( EXCEPTION );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------*/
/* Writes the content of the program or the error browser into a file. */
/* Input parameter:                                                    */
/* 1. 0: writes program browser, 1: error browser                      */
/* 2. File identifier                                                  */
/* 3. Comment string to prepend to each line                           */
/*---------------------------------------------------------------------*/

static bool print_browser( int browser, int fid, const char* comment )
{
	char *line;
	char *lp;


	writer( browser ==  0 ? C_PROG : C_OUTPUT );
	if ( comment == NULL )
		comment = "";
	T_fprintf( fid, "%s\n", comment );

	while ( 1 )
	{
		reader( &line );
		if ( line != NULL )
		{
			if ( *line == '@' )
			{
				lp = line + 1;
				while ( *lp++ != 'f' );
				T_fprintf( fid, "%s%s\n", comment, lp );
			}
			else
				T_fprintf( fid, "%s%s\n", comment, line );
		}
		else
			break;
	}

	T_fprintf( fid, "%s\n", comment );

	return OK;
}


/*---------------------------------------------------------------------*/
/* Writes a comment into a file.                                       */
/* Arguments:                                                          */
/* 1. If first argument is a number it's treated as a file identifier. */
/* 2. It follows a comment string to prepend to each line of text      */
/* 3. A text to appear in the editor                                   */
/* 4. The label for the editor                                         */
/*---------------------------------------------------------------------*/

Var *f_save_c( Var *v )
{
	int file_num;
	const char *cc = NULL;
	char *c = NULL,
		 *l = NULL,
		 *r,
		 *cl, *nl;

	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v, "save_comment()" ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	/* Try to get the comment chars to prepend to each line */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		cc = v->val.sptr;
		v = v->next;
	}

	/* Try to get the predefined content of the editor */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		c = v->val.sptr;
		correct_line_breaks( c );
		v = v->next;
	}

	/* Try to get a label string for the editor */

	if ( v != NULL )
	{
		vars_check( v, STR_VAR );
		l = v->val.sptr;
	}

	/* Show the comment editor and get the returned contents (just one string
	   with embedded newline chars) */

	r = T_strdup( show_input( c, l ) );

	if ( r == NULL )
		return vars_push( INT_VAR, 1 );

	if ( *r == '\0' )
	{
		T_free( r );
		return vars_push( INT_VAR, 1 );
	}

	cl = r;
	if ( cc == NULL )
		cc = "";
	T_fprintf( file_num, "%s\n", cc );

	while ( cl != NULL )
	{
		nl = strchr( cl, '\n' );
		if ( nl != NULL )
			*nl++ = '\0';
		T_fprintf( file_num, "%s%s\n", cc, cl );
		cl = nl;
	}

	if ( cc != NULL )
		T_fprintf( file_num, "%s\n", cc );

	T_free( r );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

static void T_fprintf( int file_num, const char *fmt, ... )
{
	int n;                      /* number of bytes we need to write */
	static int size;            /* guess for number of characters needed */
	char *p;
	va_list ap;
	char *new_name;
	FILE *new_fp;
	struct stat old_stat, new_stat;
	char *buffer[ 1024 ];
	int rw;
	char *mess;


	/* If the file has been closed because of insufficient place and no
       replacement file has been given just don't print */

	if ( File_List[ file_num ].fp == NULL )
		return;

	size = 1024;

	/* First we've got to find out how many characters we need to write out */

	p = T_malloc( size * sizeof( char ) );

	while ( 1 ) {

        /* Try to print in the allocated space */

        va_start( ap, fmt );
        n = vsnprintf( p, size, fmt, ap );
        va_end( ap );

        /* If that worked, try to write out the string */

        if ( n > -1 && n < size )
            break;

        /* Else try again with more space */

        if ( n > -1 )            /* glibc 2.1 */
            size = n + 1;        /* precisely what is needed */
        else                     /* glibc 2.0 */
        {
            if ( size < INT_MAX / 2 )
                size *= 2;           /* twice the old size */
            else
            {
				show_message( "String to be written is too long." );
                T_free( p );
                THROW( EXCEPTION );
            }
        }

		p = T_realloc( p, size * sizeof( char ) );
    }

    /* Now we try to write the string to the file */

    if ( fprintf( File_List[ file_num ].fp, "%s", p ) == n )
    {
        T_free( p );
        return;
	}

    /* Couldn't write as many bytes as needed - disk seems to be full */

	mess = get_string( strlen( "Disk full while writing to file" )
					   + strlen( File_List[ file_num ].name )
					   + strlen( "Please choose a new file." ) );
	strcpy( mess, "Disk full while writing to file" );
	strcat( mess, "\n" );
	strcat( mess, File_List[ file_num ].name );
	strcat( mess, "\n" );
	strcat( mess, "Please choose a new file." );
    show_message( mess );
	T_free( mess );

	/* Get a new file name, make user confirm it if he either selected no file
	   at all or a file that already exists (exception: if the file was
	   selected to which we were already writing, we assume that the user
	   deleted some stuff on the disk in between) */

get_repl_retry:

	new_name = T_strdup( show_fselector( "Replacement file:", NULL,
										 NULL, NULL ) );

	if ( ( new_name == NULL || *new_name == '\0' ) &&
		 1 != show_choices( "Do you really want to stop saving data?\n"
							"     All further data will be lost!",
							2, "Yes", "No", NULL, 2 ) )
	{
		if ( new_name != NULL )
			new_name = T_free( new_name );
		goto get_repl_retry;
	}

	if ( new_name == NULL || *new_name == '\0' )       /* confirmed 'Cancel' */
	{
		if ( new_name != NULL )
			T_free( new_name );
		T_free( File_List[ file_num ].name );
		File_List[ file_num ].name = NULL;
		fclose( File_List[ file_num ].fp );
		File_List[ file_num ].fp = NULL;
		return;
	}

	stat( File_List[ file_num ].name, &old_stat );
	if ( 0 == stat( new_name, &new_stat ) &&
		 ( old_stat.st_dev != new_stat.st_dev ||
		   old_stat.st_ino != new_stat.st_ino ) &&
		  1 != show_choices( "The selected file does already exist!\n"
							 " Do you really want to overwrite it?",
							 2, "Yes", "No", NULL, 2 ) )
	{
		new_name = T_free( new_name );
		goto get_repl_retry;
	}

	/* Try to open new file */

	new_fp = NULL;
	if ( ( old_stat.st_dev != new_stat.st_dev ||
		   old_stat.st_ino != new_stat.st_ino ) &&
		 ( new_fp = fopen( new_name, "w+" ) ) == NULL )
	{
		switch( errno )
		{
			case EMFILE :
				show_message( "Sorry, you have too many open files!\n"
							  "Please close at least one and retry." );
				break;

			case ENFILE :
				show_message( "Sorry, system limit for open files exceeded!\n"
							  " Please try to close some files and retry." );
				break;

			case ENOSPC :
				show_message( "Sorry, no space left on device for more file!\n"
							  "    Please delete some files and retry." );
				break;

			default :
				show_message( "Sorry, can't open selected file for writing!\n"
							  "       Please select a different file." );
		}

		new_name = T_free( new_name );
		goto get_repl_retry;
	}

	/* Switch off IO buffering for the new file */

	if ( new_fp != NULL )
		setbuf( new_fp, NULL );

	/* Check if the new and the old file are identical. If they are we simpy
	   continue to write to the old file, otherwise we first have to copy
	   everything from the old to the new file and get rid of it */

	if ( old_stat.st_dev == new_stat.st_dev &&
		 old_stat.st_ino == new_stat.st_ino )
		T_free( new_name );
	else
	{
		fseek( File_List[ file_num ].fp, 0, SEEK_SET );
		while( 1 )
		{
			clearerr( File_List[ file_num ].fp );
			clearerr( new_fp );

			rw = fread( buffer, sizeof( char ), 1024,
						File_List[ file_num ].fp );

			if ( rw != 0 )
				fwrite( buffer, sizeof( char ), rw, new_fp );

			if ( ferror( new_fp ) )
			{
				mess = get_string( strlen( "Can't write to replacement file" )
								   + strlen( new_name )
								   + strlen( "Please choose a different "
											 "file." )
								   + 2 );
				strcpy( mess, "Can't write to replacement file" );
				strcat( mess, "\n" );
				strcat( mess, new_name );
				strcat( mess, "\n" );
				strcat( mess, "Please choose a different file." );

				show_message( mess );
				T_free( mess );
				fclose( new_fp );
				unlink( new_name );
				new_name = T_free( new_name );
				goto get_repl_retry;
			}

			if ( feof( File_List[ file_num ].fp ) )
				break;
		}

		fclose( File_List[ file_num ].fp );
		unlink( File_List[ file_num ].name );
		T_free( File_List[ file_num ].name );
		File_List[ file_num ].name = new_name;
		File_List[ file_num ].fp = new_fp;
	}

	/* Finally also write the string that failed to be written */

	if ( fprintf( File_List[ file_num ].fp, "%s", p ) == n )
	{
		T_free( p );
		return;
	}

	/* Ooops, also failed to write to new file */

	mess = get_string( strlen( "Can't write to (replacement) file" )
					   + strlen( File_List[ file_num ].name )
					   + strlen( "Please choose a different file." ) + 2 );
	strcpy( mess, "Can't write to (replacement) file" );
	strcat( mess, "\n" );
	strcat( mess, File_List[ file_num ].name );
	strcat( mess, "\n" );
	strcat( mess, "Please choose a different file." );
	show_message( mess );
	T_free( mess );
	goto get_repl_retry;
}
