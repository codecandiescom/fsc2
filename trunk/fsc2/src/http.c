/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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

#define MAX_LINES_TO_SEND 30

static void spawn_server( void );
static void http_send_error_browser( int pd );


enum {
	HTTP_PARENT_READ = 0,
	HTTP_CHILD_WRITE,
	HTTP_CHILD_READ,
	HTTP_PARENT_WRITE
};


void server_callback( FL_OBJECT *obj, long a )
{
	char *www_help;


	a = a;

	if ( fl_get_button( obj ) && Internals.http_pid >= 0 )
	{
		if ( Internals.http_pid != 0 )
			return;

		TRY
		{
			spawn_server( );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			fl_set_button( obj, 0 );
			eprint( WARN, UNSET, "Failed to start HTTP server.\n" );
			return;
		}

		www_help = get_string( "Kill the HTTP server (on port %d)\n"
							   "that allows to view fsc2's current\n"
							   "state via the internet.",
							   Internals.http_port );
		fl_set_object_helper( obj, www_help );
		T_free( www_help );
	}
	else
	{
		/* This might get called twice: First when the button gets switched
		   off and the again (with the pid set to -1) when the signal handler
		   for SIGCHLD gets triggered. */

		if ( ! fl_get_button( obj ) && Internals.http_pid > 0 &&
			 ! kill( Internals.http_pid, 0 ) )
			kill( Internals.http_pid, SIGTERM );
		else
		{
			fl_set_button( obj, 0 );
			Internals.http_pid = 0;

			www_help = get_string( "Run a HTTP server (on port %d)\n"
								   "that allows to view fsc2's current\n"
								   "state via the internet.",
								   Internals.http_port );
			fl_set_object_helper( obj, www_help );
			T_free( www_help );
		}
	}
}


static void spawn_server( void )
{
	int pr;
	char *a[ 4 ];


	/* Open pipe for communication with child that's going to be spawned */

	if ( ( pr = pipe( Comm.http_pd ) ) < 0 || pipe( Comm.http_pd + 2 ) < 0 )
	{
		if ( pr == 0 )
		{
			close( Comm.http_pd[ HTTP_PARENT_READ ] );
			close( Comm.http_pd[ HTTP_CHILD_WRITE ] );
		}

		THROW( EXCEPTION );
	}

	if ( Internals.cmdline_flags & DO_CHECK )
		a[ 0 ] = get_string( "%s%sfsc2_http_server", sdir, slash( sdir ) );
	else
		a[ 0 ] = get_string( "%s%sfsc2_http_server", bindir, slash( bindir ) );

	a[ 1 ] = get_string( "%d", Internals.http_port );
	a[ 2 ] = T_strdup( auxdir );
	a[ 3 ] = NULL;

	/* Start the HTTP server with its standard input and output redirected */

	if ( ( Internals.http_pid = fork( ) ) == 0 )
	{
		close( Comm.http_pd[ HTTP_PARENT_READ ] );
		close( Comm.http_pd[ HTTP_PARENT_WRITE ] );

		dup2( Comm.http_pd[ HTTP_CHILD_READ  ], STDIN_FILENO );
		dup2( Comm.http_pd[ HTTP_CHILD_WRITE ], STDOUT_FILENO );

		execv( a[ 0 ], a );
		_exit( EXIT_FAILURE );
	}

	T_free( a[ 0 ] );
	T_free( a[ 1 ] );
	T_free( a[ 2 ] );

	close( Comm.http_pd[ HTTP_CHILD_READ  ] );
	close( Comm.http_pd[ HTTP_CHILD_WRITE ] );

	if ( Internals.http_pid == -1 )
	{
		close( Comm.http_pd[ HTTP_PARENT_READ  ] );
		close( Comm.http_pd[ HTTP_PARENT_WRITE ] );

		Internals.http_pid = 0;

		THROW( EXCEPTION );
	}
}


void http_check( void )
{
	struct timeval tv;
	fd_set rfds;
	char query;


	tv.tv_sec = tv.tv_usec = 0;
	FD_ZERO( &rfds );
	FD_SET( Comm.http_pd[ HTTP_PARENT_READ ], &rfds );

	while ( select( Comm.http_pd[ HTTP_PARENT_READ ] + 1, &rfds,
				 NULL, NULL, &tv ) > 0 )
	{
		read( Comm.http_pd[ HTTP_PARENT_READ ], &query, 1 );

		if ( query == 'S' )
		{
			char state[ 2 ] = { ( char ) Internals.state + '0', '\n' };

			write( Comm.http_pd[ HTTP_PARENT_WRITE ], state, 2 );
		}
		else if ( query == 'E' )
			http_send_error_browser( Comm.http_pd[ HTTP_PARENT_WRITE ] );
	}
}


static void http_send_error_browser( int pd )
{
	FL_OBJECT *b = GUI.main_form->error_browser;
	const char *l;
	int i = 0;
	char newline = '\n';


	if ( ( i = fl_get_browser_maxline( b ) - MAX_LINES_TO_SEND  ) < 0 )
		i = 0;

	while ( ( l = fl_get_browser_line( b, ++i ) ) != NULL )
	{
		if ( *l == '@' )
		{
			l++;
			while ( *l++ != 'f' )
				/* empty */ ;
		}

		write( pd, l, strlen( l ) );
		write( pd, &newline, 1 );
	}

	write( pd, &newline, 1 );
}
