/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"

typedef struct DPoint DPoint;

struct DPoint {
	long nx;
	long ny;
	long nc;
	int type;
	long len;
	long lval;
	double dval;
	void *ptr;
};


static DPoint *eval_display_args( Var *v, int dim, int *npoints );
static void eval_disp_array( Var **v, int dim, DPoint **dp, int *nsets );

extern sigjmp_buf alrm_env;
extern volatile sig_atomic_t can_jmp_alrm;


/*------------------------------------------------------------------------
   Prints variable number of arguments using a format string supplied as
   the first argument. Types of arguments to be printed are integer and
   float data and strings. To get a value printed the format string has
   to contain the character '#'. The escape character is the backslash,
   with a double backslash for printing one backslash. Beside the '\#'
   combination to print a '#' some of the escape sequences from printf()
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
	int in_format,            /* number of wild cards characters */
		on_stack,             /* number of arguments (beside format string ) */
		percs,                /* number of '%' characters */
		n = 0;                /* number of variables printed */
	bool print_anyway = UNSET;


	/* A call to print() without any argument prints nothing */

	if ( v == NULL )
		return vars_push( INT_VAR, 0L );

	/* Make sure the first argument is a string */

	vars_check( v, STR_VAR );
	sptr = cp = v->val.sptr;

	/* Do we also have to print while the TEST is running ? */

	if ( *sptr == '\\' && *( sptr + 1 ) == 'T' )
	{
		sptr += 2;
		print_anyway = SET;
	}

	/* Count the number of specifiers '#' in the format string but don't count
	   escaped '#' (i.e "\#"). Also count number of '%' - since this is a
	   format string we need to replace each '%' by a '%%' since printf() and
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
		print( SEVERE, "Less data than format descriptors format string.\n" );

	/* Utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		print( SEVERE, "More data than format descriptors format string.\n" );

	/* Get string long enough to replace each '#' by a 3 char sequence
	   plus a '\0' character */

	fmt = CHAR_P T_malloc( strlen( sptr ) + 3 * in_format + percs + 3 );
	strcpy( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; cp++ )
	{
		/* Skip normal characters */

		if ( *cp != '\\' && *cp != '#' && *cp != '%' )
			continue;

		/* Convert format descriptor (un-escaped '#') to 4 \x01 (as long as
           there are still variables to be printed) */

		if ( *cp == '#' )
		{
			if ( on_stack-- )
			{
				memmove( cp + 3, cp, strlen( cp ) + 1 );
				memset( cp, 0x01, 4 );
				cp += 3;
				n++;
			}
			continue;
		}

		/* Double all '%'s */

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
				print( WARN, "Unknown escape sequence \\%c in format "
					   "string.\n", *( cp + 1 ) );
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
			if ( Internals.mode != TEST || print_anyway )
				switch ( cv->type )
				{
					case INT_VAR :
						strcpy( ep, "%ld" );
						eprint( NO_ERROR, UNSET, cp, cv->val.lval );
						break;

					case FLOAT_VAR :
						strcpy( ep, "%#g" );
						eprint( NO_ERROR, UNSET, cp, cv->val.dval );
						break;

					case STR_VAR :
						strcpy( ep, "%s" );
						eprint( NO_ERROR, UNSET, cp, cv->val.sptr );
						break;

					default :
						fsc2_assert( 1 == 0 );
						break;
				}

			cv = cv->next;
		}

		cp = ep + 4;
	}

	if ( Internals.mode != TEST || print_anyway )
		eprint( NO_ERROR, UNSET, cp );

	/* Finally free the copy of the format string and return number of
	   printed variables */

	T_free( fmt );
	return vars_push( INT_VAR, ( long ) n );
}


/*------------------------------------------------------------------------
   Prints variable number of arguments using a format string supplied as
   the first argument. Types of arguments to be printed are integer and
   float data and strings. To get a value printed the format string has
   to contain the character '#'. The escape character is the backslash,
   with a double backslash for printing one backslash. Beside the '\#'
   combination to print a '#' some of the escape sequences from printf()
   ('\n', '\t', and '\"') do work. If the text should even be printed while
   the test is running the string has to start with "\T".

   The function returns the number of variables it printed, not counting
   the format string.
--------------------------------------------------------------------------*/

Var *f_sprint( Var *v )
{
	char *fmt;
	char *cp;
	char *ep;
	Var *cv;
	char *sptr;
	int in_format,            /* number of wild cards characters */
		on_stack,             /* number of arguments (beside format string ) */
		percs,                /* number of '%' characters */
		n = 0;                /* number of variables printed */
	char *is = NULL;
	char *fs;


	/* A call to print() without any argument prints nothing */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	/* Make sure the first argument is a string */

	vars_check( v, STR_VAR );
	sptr = cp = v->val.sptr;

	/* Count the number of specifiers '#' in the format string but don't count
	   escaped '#' (i.e "\#"). Also count number of '%' - since this is a
	   format string we need to replace each '%' by a '%%' since printf() and
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
		print( SEVERE, "Less data than format descriptors format string.\n" );

	/* Utter a warning if there are more data than format descriptors */

	if ( on_stack > in_format )
		print( SEVERE, "More data than format descriptors format string.\n" );

	/* Get string long enough to replace each '#' by a 3 char sequence
	   plus a '\0' character */

	fmt = CHAR_P T_malloc( strlen( sptr ) + 3 * in_format + percs + 5 );
	strcpy( fmt, "%s" );
	strcat( fmt, sptr );

	for ( cp = fmt; *cp != '\0'; cp++ )
	{
		/* Skip normal characters */

		if ( *cp != '\\' && *cp != '#' && *cp != '%' )
			continue;

		/* Convert format descriptor (un-escaped '#') to 4 \x01 (as long as
           there are still variables to be printed) */

		if ( *cp == '#' )
		{
			if ( on_stack-- )
			{
				memmove( cp + 3, cp, strlen( cp ) + 1 );
				memset( cp, 0x01, 4 );
				cp += 3;
				n++;
			}
			continue;
		}

		/* Double all '%'s */

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
				print( WARN, "Unknown escape sequence \\%c in format "
					   "string.\n", *( cp + 1 ) );
				*cp = *( cp + 1 );
				break;
		}

		memmove( cp + 1, cp + 2, strlen( cp ) - 1 );
	}

	/* Now lets start printing... */

	cp = fmt + 1;
	cv = v->next;
	fs = get_string( "" );
	while ( ( ep = strstr( cp, "\x01\x01\x01\x01" ) ) != NULL )
	{
		if ( cv != NULL )      /* skip printing if there are not enough data */
		{
			switch ( cv->type )
			{
				case INT_VAR :
					strcpy( ep, "%ld" );
					is = fs;
					fs = get_string( cp, is, cv->val.lval );
					T_free( is );
					break;

				case FLOAT_VAR :
					strcpy( ep, "%#g" );
					is = fs;
					fs = get_string( cp, is, cv->val.dval );
					T_free( is );
					break;

				case STR_VAR :
					strcpy( ep, "%s" );
					is = fs;
					fs = get_string( cp, is, cv->val.sptr );
					T_free( is );
					break;

				default :
					fsc2_assert( 1 == 0 );
					break;
			}

			cv = cv->next;
		}

		cp = ep + 2;
		*cp = '%';
		*( cp + 1 ) = 's';
	}

	is = fs;
	fs = get_string( cp, is );
	T_free( is );

	/* Finally free the copy of the format string and return number of
	   printed variables */

	T_free( fmt );

	cv = vars_push( STR_VAR, NULL );
	cv->val.sptr = fs;
	return cv;
}


/*-----------------------------------------------*/
/* Called for the EDF function "show_message()". */
/*-----------------------------------------------*/

Var *f_showm( Var *v )
{
	char *mess;
	char *mp;


	vars_check( v, STR_VAR );
	mess = T_strdup( v->val.sptr );

	for ( mp = mess; *mp; mp++ )
	{
		if ( *mp != '\\' )
			continue;

		switch ( *( mp + 1 ) )
		{
			case 'n' :
				*mp = '\n';
				break;

			case 't' :
				*mp = '\t';
				break;

			case '\\' :
				*mp = '\\';
				break;

			case '\"' :
				*mp = '"';
				break;

			default :
				print( WARN, "Unknown escape sequence \\%c in message "
					   "string.\n", *( mp + 1 ) );
				*mp = *( mp + 1 );
				break;
		}

		memmove( mp + 1, mp + 2, strlen( mp ) - 1 );
	}

	if ( Internals.mode == EXPERIMENT )
		show_message( mess );

	T_free( mess );
	return vars_push( INT_VAR, 1L );
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
	double how_long;
	double secs;


	vars_check( v, INT_VAR | FLOAT_VAR );

	how_long = VALUE( v );

	if ( how_long < 0.0 )
	{
		print( WARN, "Negative time value.\n" );
		if ( Internals.mode == TEST )
			return vars_push( INT_VAR, 1L );
		return vars_push( INT_VAR, EDL.do_quit ? 0L : 1L );
	}

	if ( how_long > ( double ) LONG_MAX )
	{
		print( FATAL, "Argument larger than %ld seconds.\n", LONG_MAX );
		THROW( EXCEPTION );
	}

	if ( lrnd( modf( how_long, &secs ) * 1.0e6 ) == 0 && lrnd( secs ) == 0 )
	{
		print( WARN, "Argument is less than 1 ms.\n" );
		return 0;
	}

	/* During the test run we not really wait but just add the expected
	   waiting time to the global variable that is used to give the modules
	   a very rough time estimate about the used time. */

	if ( Internals.mode == TEST )
	{
		EDL.experiment_time += how_long;
		return vars_push( INT_VAR, 1L );
	}

	/* Set everything up for sleeping */

    sleepy.it_interval.tv_sec = sleepy.it_interval.tv_usec = 0;
	sleepy.it_value.tv_usec = lrnd( modf( how_long, &secs ) * 1.0e6 );
	sleepy.it_value.tv_sec = lrnd( secs );

	/* Now here comes the tricky part: We have to wait for either the SIGALRM
	   or the DO_QUIT signal but must be immune to DO_SEND signals. A naive
	   implementation (i.e. my first idea ;-) would be to define a variable
	   'is_alarm' to be set in the signal handler for SIGALRM, initialize it
	   to zero and then do

	   setitimer( ITIMER_REAL, &sleepy, NULL );
	   while( ! is_alarm && ! EDL.do_quit )
	       pause( );

	   Unfortunately, there is a nasty race condition: If one of the signal
	   handlers gets executed between the check for its flag and the pause()
	   the pause() gets started even though it shouldn't. And this actually
	   happens sometimes, freezing the child process.

	   To avoid this race condition a sigsetjmp() is executed before calling
	   setitimer() and starting to wait. Since the SIGALRM can't have happend
	   yet the sigsetjmp( ) will always return 0. Now we set a flag telling
	   the signal handler that the jump buffer is initialized. Next we start
	   setitimer() and finally start the pause(). This is done in an endless
	   loop because the pause() can also be ended by a DO_SEND signal which we
	   have to ignore. In the signal handler for both the SIGALRM and DO_QUIT
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
	   otherwise he simply has to click the stop button a second time. */

	if ( sigsetjmp( alrm_env, 1 ) == 0 )
	{
		can_jmp_alrm = 1;
		setitimer( ITIMER_REAL, &sleepy, NULL );
		while ( 1 )
			pause( );
	}

	/* Return 1 if end of sleeping time was reached, 0 if 'EDL.do_quit' was
	   set. In case the wait ended due to a DO_QUIT signal we have to switch
	   off the timer because after the receipt of a DO_QUIT signal the timer
	   would still be running and the finally arriving SIGALARM signal could
	   kill the child prematurely. */

	if ( EDL.do_quit )
		sleepy.it_value.tv_usec = sleepy.it_value.tv_sec  = 0;

	return vars_push( INT_VAR, EDL.do_quit ? 0L : 1L );
}


/*-------------------------------------------------------------------*/
/* f_init_1d() has to be called to initialize the display system for */
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
	int i;


	/* Set some default values */

	G.dim |= 1;
	G.is_init = SET;
	G.mode = NORMAL_DISPLAY;

	G1.nc = 1;
	G1.nx = DEFAULT_1D_X_POINTS;
	G1.rwc_start[ X ] = ( double ) ARRAY_OFFSET;
	G1.rwc_delta[ X ] = 1.0;
	for ( i = X; i <= Y; i++ )
		G1.label[ i ] = G1.label_orig[ i ] = NULL;

	/* Now evaluate the arguments */

	if ( v == NULL )
		goto init_1d_done;

	if ( v->type == STR_VAR )
		goto labels_1d;

	G1.nc = get_long( v, "number of curves" );

	if ( G1.nc < 1 || G1.nc > MAX_CURVES )
	{
		print( FATAL, "Invalid number of curves (%ld).\n", G1.nc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_1d_done;

	if ( v->type == STR_VAR )
		goto labels_1d;

	G1.nx = get_long( v, "number of points" );

	if ( G1.nx <= 0 )
		G1.nx = DEFAULT_1D_X_POINTS;

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_1d_done;

	if ( v->type == STR_VAR )
		goto labels_1d;

	/* If next value is an integer or a float this is the real world
	   x coordinate followed by the increment */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL ||
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			print( FATAL, "Start x value found but delta x is missing.\n" );
			THROW( EXCEPTION );
		}

		G1.rwc_start[ X ] = VALUE( v );
		v = v->next;
		G1.rwc_delta[ X ] = VALUE( v );

		if ( G1.rwc_delta[ X ] == 0.0 )
		{
			G1.rwc_start[ X ] = ARRAY_OFFSET;
			G1.rwc_delta[ X ] = 1.0;
		}

		if ( G1.rwc_delta[ X ] == 0.0 )
			G1.rwc_delta[ X ] = 1.0;
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_1d_done;

 labels_1d:

	vars_check ( v, STR_VAR );
	G1.label[ X ] = T_strdup( v->val.sptr );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check ( v, STR_VAR );
		G1.label[ Y ] = T_strdup( v->val.sptr );
	}

 init_1d_done:

	G1.nx_orig = G1.nx;
	G1.rwc_start_orig[ X ] = G1.rwc_start[ X ];
	G1.rwc_delta_orig[ X ] = G1.rwc_delta[ X ];

	for ( i = X; i <= Y; i++ )
	{
		G1.label_orig[ i ] = G1.label[ i ];
		G1.label[ i ] = NULL;
	}

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/* f_init_2d() has to be called to initialize the display system for */
/* 2-dimensional experiments.                                        */
/*-------------------------------------------------------------------*/

Var *f_init_2d( Var *v )
{
	int i;


	/* Set some default values */

	G.dim |= 2;
	G.is_init = SET;
	G2.nc = 1;
	G2.nx = DEFAULT_2D_X_POINTS;
	G2.ny = DEFAULT_2D_Y_POINTS;
	G2.rwc_start[ X ] = G2.rwc_start[ Y ] = ( double ) ARRAY_OFFSET;
	G2.rwc_delta[ X ] = G2.rwc_delta[ Y ] = 1.0;
	for ( i = X; i <= Z; i++ )
		G2.label[ i ] = G2.label_orig[ i ] = NULL;

	/* Now evaluate the arguments */

	if ( v == NULL )
		goto init_2d_done;

	if ( v->type == STR_VAR )
		goto labels_2d;

	G2.nc = get_long( v, "number of curves" );

	if ( G2.nc < 1 || G2.nc > MAX_CURVES )
	{
		print( FATAL, "Invalid number of curves (%ld).\n", G2.nc );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_2d_done;

	if ( v->type == STR_VAR )
		goto labels_2d;

	G2.nx = get_long( v, "number of points in x-direction" );

	if ( G2.nx <= 0 )
		G2.nx = DEFAULT_2D_X_POINTS;

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_2d_done;

	if ( v->type == STR_VAR )
		goto labels_2d;

	G2.ny = get_long( v, "number of points in y-direction" );

	if ( G2.ny <= 0 )
		G2.ny = DEFAULT_2D_Y_POINTS;

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_2d_done;

	if ( v->type == STR_VAR )
		goto labels_2d;

	/* Now we expect 2 real world x coordinates (start and delta for
	   x-direction) */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL ||
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			print( FATAL, "Start x value found but delta x is missing.\n" );
			THROW( EXCEPTION );
		}

		G2.rwc_start[ X ] = VALUE( v );
		v = v->next;
		G2.rwc_delta[ X ] = VALUE( v );

		if ( G2.rwc_delta[ X ] == 0.0 )
		{
			G2.rwc_start[ X ] = ( double ) ARRAY_OFFSET;
			G2.rwc_delta[ X ] = 1.0;
		}
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_2d_done;

	if ( v->type == STR_VAR )
		goto labels_2d;

	/* Now we expect 2 real world y coordinates (start and delta for
	   x-direction) */

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->next == NULL ||
			 ! ( v->next->type & ( INT_VAR | FLOAT_VAR ) ) )
		{
			print( FATAL, "Start y value found but delta y is missing.\n" );
			THROW( EXCEPTION );
		}

		G2.rwc_start[ Y ] = VALUE( v );
		v = v->next;
		G2.rwc_delta[ Y ] = VALUE( v );

		if ( G2.rwc_delta[ Y ] == 0.0 )
		{
			G2.rwc_start[ Y ] = ( double ) ARRAY_OFFSET;
			G2.rwc_delta[ Y ] = 1.0;
		}
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		goto init_2d_done;

 labels_2d:

	vars_check( v, STR_VAR );
	G2.label[ X ] = T_strdup( v->val.sptr );

	if ( ( v = vars_pop( v ) ) == NULL )
		return vars_push( INT_VAR, 1L );

	vars_check( v, STR_VAR );
	G2.label[ Y ] = T_strdup( v->val.sptr );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		G2.label[ Z ] = T_strdup( v->val.sptr );
	}

 init_2d_done:

	G2.nx_orig = G2.nx;
	G2.ny_orig = G2.ny;
	for ( i = X; i <= Y; i++ )
	{
		G2.rwc_start_orig[ i ] = G2.rwc_start[ i ];
		G2.rwc_delta_orig[ i ] = G2.rwc_delta[ i ];
	}

	for ( i = X; i <= Z; i++ )
	{
		G2.label_orig[ i ] = G2.label[ i ];
		G2.label[ i ] = NULL;
	}

	return vars_push( INT_VAR, 1L );
}


/*------------------------------------*/
/* Function to change 1D display mode */
/*------------------------------------*/

Var *f_dmode( Var *v )
{
	long mode;
	long width = 0;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CHANGE_MODE;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change display mode, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	/* No change of display mode for 2D graphics possible */

	if ( G.dim == 2 )
	{
		print( SEVERE, "Display mode can only be changed for 1D display.\n" );
		return vars_push( INT_VAR, 0L );
	}

	/* Check the arguments */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | STR_VAR );

	if ( v->type == INT_VAR )
		mode = v->val.lval == 0 ? NORMAL_DISPLAY : SLIDING_DISPLAY;
	else
	{
		if ( ! strcasecmp( v->val.sptr, "NORMAL" ) ||
			 ! strcasecmp( v->val.sptr, "NORMAL_DISPLAY" ) )
			 mode = NORMAL_DISPLAY;
		else if ( ! strcasecmp( v->val.sptr, "SLIDING" ) ||
				  ! strcasecmp( v->val.sptr, "SLIDING_DISPLAY" ) )
			 mode = SLIDING_DISPLAY;
		else
		{
			print( FATAL, "Invalid argument: '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		width = get_long( v , "display width\n" );
		if ( width < DEFAULT_1D_X_POINTS )
			width = DEFAULT_1D_X_POINTS;
	}

	if ( width == 0 )
		width = G1.nx;

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
	{
		G1.nx = width;
		return vars_push( INT_VAR, 1L );
	}

	/* If we get here as the parent we're still in the PREPARATIONS phase
	   and we just store the values in the graphics main structure. */

	if ( Internals.I_am == PARENT )
	{
		if ( G.mode == mode )
			print( WARN, "Display is already in \"%s\" mode.\n",
				   G.mode ? "SLIDING" : "NORMAL" );
		else
		{
			G.mode = mode;
			G1.nx_orig = G1.nx = width;
		}

		return vars_push( INT_VAR, 1L );
	}

	/* Otherwise we're running the experiment and must tell the parent to
	   do all necessary changes */

	len = sizeof len + sizeof type + sizeof mode + sizeof width;

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &mode, sizeof mode );                 /* new mode */
	ptr += sizeof mode;

	memcpy( ptr, &width, sizeof width );               /* new width */
	ptr += sizeof width;

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_1D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*/
/* Function to change the scale during the experiment */
/*----------------------------------------------------*/

Var *f_cscale( Var *v )
{
	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change scale, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function change_scale_1d() or change_scale_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_cscale_1d( v );
	else
		return f_cscale_2d( v );
}


/*----------------------------------------------------*/
/* Function to change the scale during the experiment */
/*----------------------------------------------------*/

Var *f_cscale_1d( Var *v )
{
	double x_0, dx;                  /* new scale settings */
	int is_set = 0;                  /* flags, indicating what to change */
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CHANGE_SCALE;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change scale, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( FATAL, "No 1D display has been initialized, use function "
			   "change_scale_2d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		x_0 = VALUE( v );
		is_set = 1;
	}

	if ( ( v = vars_pop( v ) ) != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		dx = VALUE( v );
		if ( dx != 0.0 )
			is_set |= 2;
	}

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len =   sizeof len + sizeof type + sizeof is_set
		  + sizeof x_0 + sizeof dx;

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &is_set, sizeof is_set );             /* flags */
	ptr += sizeof is_set;

	memcpy( ptr, &x_0, sizeof x_0 );                   /* new x-offset */
	ptr += sizeof x_0;

	memcpy( ptr, &dx, sizeof dx );                     /* new x-increment */
	ptr += sizeof dx;

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_1D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*/
/* Function to change the scale during the experiment */
/*----------------------------------------------------*/

Var *f_cscale_2d( Var *v )
{
	double x_0, y_0, dx, dy;         /* new scale settings */
	int is_set = 0;                  /* flags, indicating what to change */
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CHANGE_SCALE;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change scale, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( FATAL, "No 2D display has been initialized, use function "
			   "change_scale_1d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		x_0 = VALUE( v );
		is_set = 1;
	}

	if ( ( v = vars_pop( v ) ) != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		dx = VALUE( v );
		if ( dx != 0.0 )
			is_set |= 2;
	}

	if ( ( v = vars_pop( v ) ) != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		y_0 = VALUE( v );
		is_set |= 4;
	}

	if ( ( v = vars_pop( v ) ) != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
	{
		dy = VALUE( v );
		if ( dy != 0.0 )
			is_set |= 8;
	}

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len =   sizeof len + sizeof type + sizeof is_set
		  + sizeof x_0 + sizeof dx + sizeof y_0 + sizeof dy;

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &is_set, sizeof is_set );             /* flags */
	ptr += sizeof is_set;

	memcpy( ptr, &x_0, sizeof x_0 );                   /* new x-offset */
	ptr += sizeof x_0;

	memcpy( ptr, &dx, sizeof dx );                     /* new x-increment */
	ptr += sizeof dx;

	memcpy( ptr, &y_0, sizeof y_0 );                   /* new y-offset */
	ptr += sizeof y_0;

	memcpy( ptr, &dy, sizeof dy );                     /* new y-increment */
	ptr += sizeof dy;

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_2D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------------*/
/* Function to change one or more axis labels during the experiment */
/*------------------------------------------------------------------*/

Var *f_clabel( Var *v )
{
	/* No changing of labels without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change labels, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function change_label_1d() or change_label_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_clabel_1d( v );
	else
		return f_clabel_2d( v );
}


/*------------------------------------------------------------------*/
/* Function to change one or more axis labels during the experiment */
/*------------------------------------------------------------------*/

Var *f_clabel_1d( Var *v )
{
	char *l[ 2 ] = { NULL, NULL };
	long lengths[ 2 ] = { 1, 1 };
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int i;
	int type = D_CHANGE_LABEL;


	/* No changing of labels without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change labels, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( FATAL, "No 1D display has been initialized, use function "
			   "change_label_2d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, STR_VAR );

	l[ X ] = T_strdup( v->val.sptr );
	lengths[ X ] = ( long ) strlen( l[ X ] ) + 1;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		l[ Y ] = T_strdup( v->val.sptr );
		lengths[ Y ] = ( long ) strlen( l[ Y ] ) + 1;
	}

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len = sizeof len + sizeof type;
	for ( i = X; i <= Y; i++ )
		len += sizeof lengths[ i ] + lengths[ i ];

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		T_free( l[ Y ] );
		T_free( l[ X ] );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	for ( i = X; i <= Y; i++ )
	{
		memcpy( ptr, lengths + i, sizeof lengths[ i ] );
		ptr += sizeof lengths[ i ];
		if ( lengths[ i ] > 1 )
		{
			memcpy( ptr, l[ i ], lengths[ i ] );
			T_free( l[ i ] );
		}
		else
			* ( char * ) ptr = '\0';
		ptr += lengths[ i ];
	}

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_1D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------------*/
/* Function to change one or more axis labels during the experiment */
/*------------------------------------------------------------------*/

Var *f_clabel_2d( Var *v )
{
	char *l[ 3 ] = { NULL, NULL, NULL };
	long lengths[ 3 ] = { 1, 1, 1 };
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int i;
	int type = D_CHANGE_LABEL;


	/* No changing of labels without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change labels, missing graphics "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( FATAL, "No 2D display has been initialized, use function "
			   "change_label_1d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, STR_VAR );

	l[ X ] = T_strdup( v->val.sptr );
	lengths[ X ] = ( long ) strlen( l[ X ] ) + 1;

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		vars_check( v, STR_VAR );
		l[ Y ] = T_strdup( v->val.sptr );
		lengths[ Y ] = ( long ) strlen( l[ Y ] ) + 1;

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			vars_check( v, STR_VAR );
			l[ Z ] = T_strdup( v->val.sptr );
			lengths[ Z ] = ( long ) strlen( l[ Z ] ) + 1;
		}
	}

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len = sizeof len + sizeof type;
	for ( i = X; i <= Z; i++ )
		len += sizeof lengths[ i ] + lengths[ i ];

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		T_free( l[ Z ] );
		T_free( l[ Y ] );
		T_free( l[ X ] );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	for ( i = X; i <= Z; i++ )
	{
		memcpy( ptr, lengths + i, sizeof lengths[ i ] );
		ptr += sizeof lengths[ i ];
		if ( lengths[ i ] > 1 )
		{
			memcpy( ptr, l[ i ], lengths[ i ] );
			T_free( l[ i ] );
		}
		else
			* ( char * ) ptr = '\0';
		ptr += lengths[ i ];
	}

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_2D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*/
/* Function to change the number of points to be displayed */
/* during the experiment.                                  */
/*---------------------------------------------------------*/

Var *f_rescale( Var *v )
{
	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change number of points, missing "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function rescale_1d() or rescale_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_rescale_1d( v );
	else
		return f_rescale_2d( v );
}


/*---------------------------------------------------------*/
/* Function to change the number of points to be displayed */
/* during the experiment.                                  */
/*---------------------------------------------------------*/

Var *f_rescale_1d( Var *v )
{
	long new_nx;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CHANGE_POINTS;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change number of points, missing "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( FATAL, "No 1D display has been initialized, use function "
			   "rescale_2d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	new_nx = get_long( v, "number of points" );

	if ( new_nx < -1 )
	{
		print( FATAL, "Invalid negative number of points.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len = sizeof len + sizeof type + sizeof new_nx;

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &new_nx, sizeof new_nx );             /* new # of x points */
	ptr += sizeof new_nx;

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_1D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*/
/* Function to change the number of points to be displayed */
/* during the experiment.                                  */
/*---------------------------------------------------------*/

Var *f_rescale_2d( Var *v )
{
	long new_nx, new_ny;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CHANGE_POINTS;


	/* No rescaling without graphics... */

	if ( ! G.is_init )
	{
		print( SEVERE, "Can't change number of points, missing "
			   "initialization.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( FATAL, "No 2D display has been initialized, use function "
			   "rescale_1d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check and store the parameter */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	new_nx = get_long( v, "number of points in x-direction" );

	if ( new_nx < -1 )
	{
		print( FATAL, "Invalid negative number of points in x-direction.\n" );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		new_ny = get_long( v, "number of points in y-direction" );

		if ( new_ny < -1 )
		{
			print( FATAL, "Invalid negative number of points (%ld) in "
				   "y-direction.\n", new_ny );
			THROW( EXCEPTION );
		}
	} else
		new_ny = -1;

	/* In a test run we're already done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );

	/* Function can only be used in experiment section */

	fsc2_assert( Internals.I_am == CHILD );

	len = sizeof len + sizeof type + sizeof new_nx + sizeof new_ny;

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );                 /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &new_nx, sizeof new_nx );             /* new # of x points */
	ptr += sizeof new_nx;

	memcpy( ptr, &new_ny, sizeof new_ny );             /* new # of y points */

	/* Detach from the segment with the data segment */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_2D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display( Var *v )
{
	/* We can't display data without a previous initialization */

	if ( ! G.is_init )
	{
		if ( ! G.is_warn )                         /* warn only once */
		{
			print( WARN, "Can't display data, missing initialisation\n" );
			G.is_warn = SET;
		}

		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function display_1d() or display_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_display_1d( v );
	else
		return f_display_2d( v );
}


/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display_1d( Var *v )
{
	DPoint *dp;
	int shm_id;
	long len = 0;                     /* total length of message to send */
	void *buf;
	char *ptr;
	int nsets;
	int i;


	/* We can't display data without a previous initialization */

	if ( ! G.is_init )
	{
		if ( ! G.is_warn )                         /* warn only once */
		{
			print( WARN, "Can't display data, missing initialisation\n" );
			G.is_warn = SET;
		}

		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( FATAL, "No 1D display has been initialized, use function "
			   "display_2d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check the arguments and get them into some reasonable form */

	dp = eval_display_args( v, 1, &nsets );

	if ( nsets == 0 )
	{
		T_free( dp );
		return vars_push( INT_VAR, 0L );
	}

	if ( Internals.mode == TEST )
	{
		T_free( dp );
		return vars_push( INT_VAR, 1L );
	}

	fsc2_assert( Internals.I_am == CHILD );

	/* Determine the required amount of shared memory */

	len =   sizeof len                  /* length field itself */
		  + sizeof nsets                /* number of sets to be sent */
		  + nsets * (                   /* x-, y-index, number and data type */
					    sizeof dp->nx + sizeof dp->nc 
					  + sizeof dp->type );

	for ( i = 0; i < nsets; i++ )
	{
		fsc2_assert( dp[ i ].len > 0 );

		switch( dp[ i ].type )
		{
			case INT_VAR :
				len += sizeof( long );
				break;

			case FLOAT_VAR :
				len += sizeof( double );
				break;

			case INT_ARR :
				len += sizeof( long ) + dp[ i ].len * sizeof( long );
				break;

			case FLOAT_ARR :
				len += sizeof( long ) + dp[ i ].len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, UNSET, "Internal communication error at "
						"%s:%d.\n", __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( dp );
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &nsets, sizeof nsets );               /* # data sets  */
	ptr += sizeof nsets;

	for ( i = 0; i < nsets; i++ )
	{
		memcpy( ptr, &dp[ i ].nx, sizeof dp[ i ].nx );  /* x-index */
		ptr += sizeof dp[ i ].nx;

		memcpy( ptr, &dp[ i ].nc, sizeof dp[ i ].nc );  /* curve number */
		ptr += sizeof dp[ i ].nc;

		memcpy( ptr, &dp[ i ].type, sizeof dp[ i ].type );
		ptr += sizeof dp[ i ].type;

		switch( dp[ i ].type )                          /* and now the data  */
		{
			case INT_VAR :
				memcpy( ptr, &dp[ i ].lval, sizeof dp[ i ].lval );
				ptr += sizeof dp[ i ].lval;
				break;

			case FLOAT_VAR :
				memcpy( ptr, &dp[ i ].dval, sizeof dp[ i ].dval );
				ptr += sizeof dp[ i ].dval;
				break;

			case INT_ARR :
				memcpy( ptr, &dp[ i ].len, sizeof dp[ i ].len );
				ptr += sizeof dp[ i ].len;
				memcpy( ptr, dp[ i ].ptr, dp[ i ].len * sizeof( long ) );
				ptr += dp[ i ].len * sizeof( long );
				break;

			case FLOAT_ARR :
				memcpy( ptr, &dp[ i ].len, sizeof dp[ i ].len );
				ptr += sizeof dp[ i ].len;
				memcpy( ptr, dp[ i ].ptr, dp[ i ].len * sizeof( double ) );
				ptr += dp[ i ].len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, UNSET, "Internal communication error at "
						"%s:%d.\n", __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Get rid of the array of structures returned by eval_display_args() */

	T_free( dp );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_1D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*/
/* f_display() is used to send new data to the display system. */
/*-------------------------------------------------------------*/

Var *f_display_2d( Var *v )
{
	DPoint *dp;
	int shm_id;
	long len = 0;                     /* total length of message to send */
	void *buf;
	char *ptr;
	int nsets;
	int i;


	/* We can't display data without a previous initialization */

	if ( ! G.is_init )
	{
		if ( ! G.is_warn )                         /* warn only once */
		{
			print( WARN, "Can't display data, missing initialisation\n" );
			G.is_warn = SET;
		}

		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( FATAL, "No 2D display has been initialized, use function "
			   "display_1d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* Check the arguments and get them into some reasonable form */

	dp = eval_display_args( v, 2, &nsets );

	if ( nsets == 0 )
	{
		T_free( dp );
		return vars_push( INT_VAR, 0L );
	}

	if ( Internals.mode == TEST )
	{
		T_free( dp );
		return vars_push( INT_VAR, 1L );
	}

	fsc2_assert( Internals.I_am == CHILD );

	/* Determine the required amount of shared memory */

	len =   sizeof len                  /* length field itself */
		  + sizeof nsets                /* number of sets to be sent */
		  + nsets * (                   /* x-, y-index, number and data type */
					    sizeof dp->nx + sizeof dp->ny + sizeof dp->nc 
					  + sizeof dp->type );

	for ( i = 0; i < nsets; i++ )
	{
		fsc2_assert( dp[ i ].len > 0 );

		switch( dp[ i ].type )
		{
			case INT_VAR :
				len += sizeof( long );
				break;

			case FLOAT_VAR :
				len += sizeof( double );
				break;

			case INT_ARR :
				len += sizeof( long ) + dp[ i ].len * sizeof( long );
				break;

			case FLOAT_ARR :
				len += sizeof( long ) + dp[ i ].len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, UNSET, "Internal communication error at "
						"%s:%d.\n", __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Now try to get a shared memory segment */

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( dp );
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy the data to the segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );                   /* total length */
	ptr += sizeof len;

	memcpy( ptr, &nsets, sizeof nsets );               /* # data sets  */
	ptr += sizeof nsets;

	for ( i = 0; i < nsets; i++ )
	{
		memcpy( ptr, &dp[ i ].nx, sizeof dp[ i ].nx );  /* x-index */
		ptr += sizeof dp[ i ].nx;

		memcpy( ptr, &dp[ i ].ny, sizeof dp[ i ].ny );  /* y-index */
		ptr += sizeof dp[ i ].ny;

		memcpy( ptr, &dp[ i ].nc, sizeof dp[ i ].nc );  /* curve number */
		ptr += sizeof dp[ i ].nc;

		memcpy( ptr, &dp[ i ].type, sizeof dp[ i ].type );
		ptr += sizeof dp[ i ].type;

		switch( dp[ i ].type )                          /* and now the data  */
		{
			case INT_VAR :
				memcpy( ptr, &dp[ i ].lval, sizeof dp[ i ].lval );
				ptr += sizeof dp[ i ].lval;
				break;

			case FLOAT_VAR :
				memcpy( ptr, &dp[ i ].dval, sizeof dp[ i ].dval );
				ptr += sizeof dp[ i ].dval;
				break;

			case INT_ARR :
				memcpy( ptr, &dp[ i ].len, sizeof dp[ i ].len );
				ptr += sizeof dp[ i ].len;
				memcpy( ptr, dp[ i ].ptr, dp[ i ].len * sizeof( long ) );
				ptr += dp[ i ].len * sizeof( long );
				break;

			case FLOAT_ARR :
				memcpy( ptr, &dp[ i ].len, sizeof dp[ i ].len );
				ptr += sizeof dp[ i ].len;
				memcpy( ptr, dp[ i ].ptr, dp[ i ].len * sizeof( double ) );
				ptr += dp[ i ].len * sizeof( double );
				break;

			default :                   /* this better never happens... */
				T_free( dp );
				eprint( FATAL, UNSET, "Internal communication error at "
						"%s:%d.\n", __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Get rid of the array of structures returned by eval_display_args() */

	T_free( dp );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell parent about the data */

	send_data( DATA_2D, shm_id );

	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static DPoint *eval_display_args( Var *v, int dim, int *nsets )
{
	DPoint *dp = NULL;


	CLOBBER_PROTECT( v );

	*nsets = 0;
	if ( v == NULL )
	{
		print( FATAL, "Missing x-index.\n" );
		THROW( EXCEPTION );
	}

	do
	{
		/* Get (more) memory for the sets */

		TRY
		{
			dp = DPOINT_P T_realloc( dp, ( *nsets + 1 ) * sizeof *dp );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( dp );
			RETHROW( );
		}

		/* Check and store the x-index */

		dp[ *nsets ].nx = get_long( v, "x-index" ) - ARRAY_OFFSET;

		if ( dp[ *nsets ].nx < 0 )
		{
			print( FATAL, "Invalid x-index (%ld).\n",
				   dp[ *nsets ].nx + ARRAY_OFFSET );
			T_free( dp );
			THROW( EXCEPTION );
		}

		/* For 2D experiments test and get y-index */

		if ( dim == 2 )
		{
			if ( ( v = v->next ) == NULL )
			{
				print( FATAL, "Missing y-index.\n" );
				T_free( dp );
				THROW( EXCEPTION );
			}

			dp[ *nsets ].ny = get_long( v, "y-index" ) - ARRAY_OFFSET;

			if ( dp[ *nsets ].ny < 0 )
			{
				print( FATAL, "Invalid y-index (%ld).\n",
						dp[ *nsets ].ny + ARRAY_OFFSET );
				T_free( dp );
				THROW( EXCEPTION );
			}
		}

		/* Now test and get the data, i. e. store the pointer to the variable
		   containing it */

		if ( ( v = v->next ) == NULL )
		{
			print( FATAL, "Missing data.\n" );
			T_free( dp );
			THROW( EXCEPTION );
		}

		TRY
		{
			vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
						INT_REF | FLOAT_REF );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( dp );
			RETHROW( );
		}

		dp[ *nsets ].type = v->type;

		switch ( v->type )
		{
			case INT_VAR :
				dp[ *nsets ].len = 1;
				dp[ *nsets ].lval = v->val.lval;
				break;

			case FLOAT_VAR :
				dp[ *nsets ].len = 1;
				dp[ *nsets ].dval = v->val.dval;
				break;

			case INT_ARR :
				if ( v->len == 0 )
				{
					print( WARN, "Request to draw zero-length array.\n" );
					if ( ( v = v->next ) != NULL )
						v = v->next;
					continue;
				}
				dp[ *nsets ].len = v->len;
				dp[ *nsets ].ptr = v->val.lpnt;
				break;

			case FLOAT_ARR :
				if ( v->len == 0 )
				{
					print( WARN, "Request to draw zero-length array.\n" );
					if ( ( v = v->next ) != NULL )
						v = v->next;
					continue;
				}
				dp[ *nsets ].len = v->len;
				dp[ *nsets ].ptr = v->val.dpnt;
				break;

			case INT_REF : case FLOAT_REF :
				eval_disp_array( &v, dim, &dp, nsets );
				continue;
		}

		/* There can be several curves and we check if there's a curve number,
		   then we test and store it. If there were only coordinates and data
		   for on curve and there are no more argument we default to the first
		   curve.*/

		if ( ( v = v->next ) == NULL )
		{
			if ( *nsets != 0 )
			{
				print( FATAL, "Missing curve number\n" );
				T_free( dp );
				THROW( EXCEPTION );
			}

			dp[ *nsets ].nc = 0;
			( *nsets )++;
			return dp;
		}

		dp[ *nsets ].nc = get_long( v, "curve number" ) - 1;

		v = v->next;

		if ( dp[ *nsets ].nc < 0 ||
			 dp[ *nsets ].nc >= ( dim == 1 ? G1.nc : G2.nc ) )
		{
			print( FATAL, "Invalid curve number (%ld).\n",
				   dp[ *nsets ].nc + 1 );
			T_free( dp );
			THROW( EXCEPTION );
		}

		( *nsets )++;

	} while ( v != NULL );

	return dp;
}


/*-------------------------------------------------*/
/*-------------------------------------------------*/

static void eval_disp_array( Var **v, int dim, DPoint **dp, int *nsets )
{
	long count;
	ssize_t i;
	long j;
	long nx_offset;
	long ny_offset;
	long cn;


	CLOBBER_PROTECT( count );

	/* Drawing 2-dimensional arrays only works for 2D-display */

	if ( dim != 2 )
	{
		print( FATAL, "Can't draw more than 1D-arrays in 1D-display.\n" );
		T_free( *dp );
		THROW( EXCEPTION );
	}

	if ( ( *v )->dim > 2 )
	{
		print( FATAL, "Can't draw arrays with more than 2 dimensions.\n" );
		T_free( *dp );
		THROW( EXCEPTION );
	}

	if ( ( *v )->len == 0 )
	{
		print( WARN, "Request to draw zero-length 2D-array.\n" );
		if ( ( *v = ( *v )->next ) != NULL )
			*v = ( *v )->next;
		return;
	}

	/* Figure out how many 1D-arrays we really have to draw */

	for ( count = i = 0; i < ( *v )->len; i++ )
		if ( ( *v )->val.vptr[ i ] != NULL && ( *v )->val.vptr[ i ]->len != 0 )
			count++;

	/* If all sub-arrays have zero length nothing further has to be done
	   except removing the variable (and possibly the curve number) from
	   the stack. */

	if ( count == 0 )
	{
		print( WARN, "All sub-arrays of 2D-array to be drawn have zero "
			   "length.\n" );
		if ( ( *v = ( *v )->next ) != NULL )
			*v = ( *v )->next;
		return;
	}

	/* Extend the array of structures to as many as we need for all the
	   1D-arrays */

	if ( count > 1 )
	{
		TRY
		{
			*dp = DPOINT_P T_realloc( *dp, ( *nsets + count ) * sizeof **dp );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( *dp );
			THROW( EXCEPTION );
		}
	}

	/* The x- and y-coordinate of the first array to be drawn have already
	   been set but we need them also for the other arrays */

	nx_offset = ( *dp + *nsets )->nx;
	ny_offset = ( *dp + *nsets )->ny;

	/* Now set up all the structures for the different array */

	for ( j = *nsets, i = 0; i < ( *v )->len; i++ )
	{
		if ( ( *v )->val.vptr[ i ] == NULL || ( *v )->val.vptr[ i ]->len == 0 )
			continue;

		( *dp + j )->nx   = nx_offset;
		( *dp + j )->ny   = ny_offset + i;
		( *dp + j )->type = ( *v )->val.vptr[ i ]->type;
		( *dp + j )->len  = ( *v )->val.vptr[ i ]->len;
		( *dp + j )->ptr  = ( *dp + j )->type == INT_ARR ?
								( void * ) ( *v )->val.vptr[ i ]->val.lpnt :
								( void * ) ( *v )->val.vptr[ i ]->val.dpnt;
		j++;
	}

	/* Finally, the curve number has to set for all the new structures */

	if ( ( *v = ( *v )->next ) == NULL )
	{
		if ( *nsets != 0 )
		{
			print( FATAL, "Missing curve number\n" );
			T_free( *dp );
			THROW( EXCEPTION );
		}

		cn = 0;
	}
	else
	{
		cn = get_long( *v, "curve number" ) - 1;
		*v = ( *v )->next;

		if ( cn < 0 || cn >= G2.nc )
		{
			print( FATAL, "Invalid curve number (%ld).\n", cn );
			T_free( *dp );
			THROW( EXCEPTION );
		}
	}

	for ( j = 0; j < count; j++ )
		( *dp + ( *nsets )++ )->nc = cn;
}


/*-------------------------------------------------*/
/* Function makes all points of a curve invisible. */
/*-------------------------------------------------*/

Var *f_clearcv( Var *v )
{
	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		if ( Internals.mode == TEST )
			print( WARN, "Can't clear curve, missing graphics "
				   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function clear_curve_1d() or clear__curve_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_clearcv_1d( v );
	else
		return f_clearcv_2d( v );
}


/*-------------------------------------------------*/
/* Function makes all points of a curve invisible. */
/*-------------------------------------------------*/

Var *f_clearcv_1d( Var *v )
{
	long curve;
	long count = 0;
	long *ca = NULL;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CLEAR_CURVE;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		if ( Internals.mode == TEST )
			print( WARN, "Can't clear curve, missing graphics "
				   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( FATAL, "No 1D display has been initialized, use function "
			   "clear_curve_2d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* If there is no argument default to curve 1 */

	if ( v == NULL )
	{
		if ( Internals.mode == TEST )
			return vars_push( INT_VAR, 1L );

		ca = LONG_P T_malloc( sizeof *ca );
		*ca = 0;
		count = 1;
	}
	else
	{
		/* Otherwise run through all arguments, treating each as a new curve
		   number */

		do
		{
			/* Get the curve number and make sure the curve exists */

			curve = get_long( v, "curve number" );

			if ( curve < 1 || curve > G1.nc )
			{
				if ( Internals.mode == TEST )
					print( SEVERE, "Can't clear 1D curve %ld, curve "
						   "does not exist.\n", curve );
				continue;
			}

			/* Store curve number */

			ca = LONG_P T_realloc( ca, ( count + 1 ) * sizeof *ca );
			ca[ count++ ] = curve - 1;

		} while ( ( v = v->next ) != NULL );

		if ( ca == NULL )
		{
			print( SEVERE, "No valid argument found.\n" );
			return vars_push( INT_VAR, 0L );
		}
	}

	/* In a test run this already everything to be done */

	if ( Internals.mode == TEST )
	{
		T_free( ca );
		return vars_push( INT_VAR, 1L );
	}

	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len =   sizeof len + sizeof type + sizeof count
		  + count * sizeof *ca;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( ca );
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &count, sizeof count );           /* curve number count */
	ptr += sizeof count;

	memcpy( ptr, ca, count * sizeof *ca );         /* array of curve numbers */

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Get rid of the array of curve numbers */

	T_free( ca );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_1D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------*/
/* Function makes all points of a curve invisible. */
/*-------------------------------------------------*/

Var *f_clearcv_2d( Var *v )
{
	long curve;
	long count = 0;
	long *ca = NULL;
	int shm_id;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CLEAR_CURVE;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		if ( Internals.mode == TEST )
			print( WARN, "Can't clear curve, missing graphics "
				   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( FATAL, "No 2D display has been initialized, use function "
			   "clear_curve_1d() instead.\n" );
		THROW( EXCEPTION );
	}

	/* If there is no argument default to curve 1 */

	if ( v == NULL )
	{
		if ( Internals.mode == TEST )
			return vars_push( INT_VAR, 1L );

		ca = LONG_P T_malloc( sizeof *ca );
		*ca = 0;
		count = 1;
	}
	else
	{
		/* Otherwise run through all arguments, treating each as a new curve
		   number */

		do
		{
			/* Get the curve number and make sure the curve exists */

			curve = get_long( v, "curve number" );

			if ( curve < 1 || curve > G2.nc )
			{
				if ( Internals.mode == TEST )
					print( SEVERE, "Can't clear 2D curve %ld, curve "
						   "does not exist.\n", curve );
				continue;
			}

			/* Store curve number */

			ca = LONG_P T_realloc( ca, ( count + 1 ) * sizeof *ca );
			ca[ count++ ] = curve - 1;

		} while ( ( v = v->next ) != NULL );

		if ( ca == NULL )
		{
			print( SEVERE, "No valid argument found.\n" );
			return vars_push( INT_VAR, 0L );
		}
	}

	/* In a test run this already everything to be done */

	if ( Internals.mode == TEST )
	{
		T_free( ca );
		return vars_push( INT_VAR, 1L );
	}

	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len =   sizeof len + sizeof type + sizeof count
		  + count * sizeof *ca;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		T_free( ca );
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &count, sizeof count );           /* curve number count */
	ptr += sizeof count;

	memcpy( ptr, ca, count * sizeof *ca );         /* array of curve numbers */

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Get rid of the array of curve numbers */

	T_free( ca );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_2D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}


/*-------------------------*/
/* Function draws a marker */
/*-------------------------*/

Var *f_setmark( Var *v )
{
	if ( ! G.is_init )
	{
		if ( Internals.mode == TEST )
			print( WARN, "Can't draw marker, missing graphics "
				   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function draw_marker_1d() or draw_marker_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_setmark_1d( v );
	else
		return f_setmark_2d( v );
}


/*----------------------------*/
/* Function draws a 1D marker */
/*----------------------------*/

Var *f_setmark_1d( Var *v )
{
	long position;
	long color = 0;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_SET_MARKER;
	int shm_id;
	const char *colors[ ] = { "WHITE", "RED", "GREEN", "YELLOW",
							  "BLUE", "BLACK", "DELETE" };
	long num_colors = sizeof colors / sizeof *colors;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		print( WARN, "Can't set a marker, missing graphics "
			   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
	{
		print( WARN, "Can't set 1D marker, use draw_marker_2d() instead.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( v == NULL )
	{
		print( WARN, "Missing arguments\n" );
		return vars_push( INT_VAR, 0L );
	}

	position = get_long( v, "marker position" ) - ARRAY_OFFSET;

	if ( position < 0 )
	{
		print( FATAL, "Invalid x-index (%ld).\n", position + ARRAY_OFFSET );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		if ( v->type & ( INT_VAR | FLOAT_VAR ) )
			color = get_long( v, "marker color" );
		else
		{
			vars_check( v, STR_VAR );
			for ( color = 0; color < num_colors; color++ )
				if ( ! strcasecmp( colors[ color ], v->val.sptr ) )
					break;
		}

		if ( color >= num_colors )
		{
			if ( v->type & ( INT_VAR | FLOAT_VAR ) )
				print( WARN, "Invalid marker color number (%ld), using "
					   "default of 0 (white).\n", color );
			else
				print( WARN, "Invalid marker color (\"%s\"), using default of "
					   "\"WHITE\".\n", v->val.sptr );
			color = 0;
		}
	}

	too_many_arguments( v );

	/* In a test run this all there is to be done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );
	
	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len = sizeof len + sizeof type + sizeof position + sizeof color;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &position, sizeof position );
	ptr += sizeof position;

	memcpy( ptr, &color, sizeof color );

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_1D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}


/*----------------------------*/
/* Function draws a 2D marker */
/*----------------------------*/

Var *f_setmark_2d( Var *v )
{
	long x_pos;
	long y_pos;
	long color = 0;
	long curve = -1;
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_SET_MARKER;
	int shm_id;
	const char *colors[ ] = { "WHITE", "RED", "GREEN", "YELLOW",
							  "BLUE", "BLACK", "DELETE" };
	long num_colors = sizeof colors / sizeof *colors;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		print( WARN, "Can't set a marker, missing graphics "
			   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
	{
		print( WARN, "Can't set 2D marker, use draw_marker_1d() instead.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( v == NULL )
	{
		print( WARN, "Missing arguments\n" );
		return vars_push( INT_VAR, 0L );
	}

	x_pos = get_long( v, "marker x-position" ) - ARRAY_OFFSET;

	if ( x_pos < 0 )
	{
		print( FATAL, "Invalid x-index (%ld).\n", x_pos + ARRAY_OFFSET );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( WARN, "Missing y-position arguments\n" );
		return vars_push( INT_VAR, 0L );
	}

	y_pos = get_long( v, "marker y-position" ) - ARRAY_OFFSET;

	if ( y_pos < 0 )
	{
		print( FATAL, "Invalid y-index (%ld).\n", y_pos + ARRAY_OFFSET );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		curve = get_long( v, "curve number" ) - 1;

		if ( curve < 0 || curve >= G2.nc )
		{
			print( SEVERE, "Can't clear marker on 2D curve %ld, curve does "
				   "not exist.\n", curve + 1 );
			return vars_push( INT_VAR, 0L );
		}

		if ( ( v = vars_pop( v ) ) != NULL )
		{
			if ( v->type & ( INT_VAR | FLOAT_VAR ) )
				color = get_long( v, "marker color" );
			else
			{
				vars_check( v, STR_VAR );
				for ( color = 0; color < num_colors; color++ )
					if ( ! strcasecmp( colors[ color ], v->val.sptr ) )
					break;
			}

			if ( color >= num_colors )
			{
				if ( v->type & ( INT_VAR | FLOAT_VAR ) )
					print( WARN, "Invalid marker color number (%ld), using "
						   "default of 0 (white).\n", color );
				else
					print( WARN, "Invalid marker color (\"%s\"), using "
						   "default of \"WHITE\".\n", v->val.sptr );
				color = 0;
			}
		}
	}

	too_many_arguments( v );

	/* In a test run this all there is to be done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );
	
	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len =   sizeof len + sizeof type + sizeof x_pos + sizeof y_pos
		  + sizeof color + sizeof curve;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, &x_pos, sizeof x_pos );
	ptr += sizeof x_pos;

	memcpy( ptr, &y_pos, sizeof y_pos );
	ptr += sizeof y_pos;

	memcpy( ptr, &color, sizeof color );
	ptr += sizeof color;

	memcpy( ptr, &curve, sizeof curve );
	
	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_2D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}


/*------------------------------*/
/* Function deletes all markers */
/*------------------------------*/

Var *f_clearmark( Var *v )
{
	if ( ! G.is_init )
	{
		if ( Internals.mode == TEST )
			print( WARN, "Can't clear markers, missing graphics "
				   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( G.dim == 3 )
	{
		print( FATAL, "Both 1D- and 2D-display are in use, use either "
			   "function clear_marker_1d() or clear_marker_2d().\n" );
		THROW( EXCEPTION );
	}

	if ( G.dim == 1 )
		return f_clearmark_1d( v );
	else
		return f_clearmark_2d( v );
}


/*---------------------------------*/
/* Function deletes all 1D markers */
/*---------------------------------*/

Var *f_clearmark_1d( Var *v )
{
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CLEAR_MARKERS;
	int shm_id;


	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		print( WARN, "Can't clear markers, missing graphics "
			   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 1 ) )
    {
        print( WARN, "Can't clear 1D markers, use clear_marker_2d() "
			   "instead.\n" );
        return vars_push( INT_VAR, 0L );
    }

	if ( v != NULL )
		print( SEVERE, "Superfluous argument%s.\n",
			   v->next != NULL ? "s" : "" );

	/* In a test run this all there is to be done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );
	
	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len = sizeof len + sizeof type;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_1D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}
	

/*---------------------------------*/
/* Function deletes all 2D markers */
/*---------------------------------*/

Var *f_clearmark_2d( Var *v )
{
	long len = 0;                    /* total length of message to send */
	void *buf;
	char *ptr;
	int type = D_CLEAR_MARKERS;
	int shm_id;
	int i;
	long curves[ MAX_CURVES ] = { -1L, -1L, -1L, -1L };


	UNUSED_ARGUMENT( v );

	/* This function can only be called in the EXPERIMENT section and needs
	   a previous graphics initialisation */

	if ( ! G.is_init )
	{
		print( WARN, "Can't clear markers, missing graphics "
			   "initialisation.\n" );
		return vars_push( INT_VAR, 0L );
	}

	if ( ! ( G.dim & 2 ) )
    {
        print( WARN, "Can't clear 2D markers, use clear_marker_1d() "
			   "instead.\n" );
        return vars_push( INT_VAR, 0L );
    }

	/* Check for a list of curve numbers */

	for ( i = 0; v != NULL && i < G2.nc; i++, v = vars_pop( v ) )
	{
		curves[ i ] = get_strict_long( v, "curve number" ) - 1;

		if ( curves[ i ] < 0 || curves[ i ] >= G2.nc )
		{
			print( FATAL, "There is no curve numbered %ld\n",
				   curves[ i ] + 1 );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	/* In a test run this all there is to be done */

	if ( Internals.mode == TEST )
		return vars_push( INT_VAR, 1L );
	
	/* Now starts the code only to be executed by the child, i.e. while the
	   measurement is running. */

	fsc2_assert( Internals.I_am == CHILD );

	/* Now try to get a shared memory segment */

	len = sizeof len + sizeof type + MAX_CURVES * sizeof *curves;

	if ( ( buf = get_shm( &shm_id, len ) ) == ( void * ) - 1 )
	{
		eprint( FATAL, UNSET, "Internal communication problem at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Copy all data into the shared memory segment */

	ptr = CHAR_P buf;

	memcpy( ptr, &len, sizeof len );               /* total length */
	ptr += sizeof len;

	memcpy( ptr, &type, sizeof type );             /* type indicator  */
	ptr += sizeof type;

	memcpy( ptr, curves, MAX_CURVES * sizeof curves );

	/* Detach from the segment with the data */

	detach_shm( buf, NULL );

	/* Wait for parent to become ready to accept new data, then store
	   identifier and send signal to tell it about them */

	send_data( DATA_2D, shm_id );

	/* All the rest has now to be done by the parent process... */

	return vars_push( INT_VAR, 1L );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
