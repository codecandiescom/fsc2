/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


/* Both variables are defined in 'func.c' */

extern bool No_File_Numbers;
extern bool Dont_Save;


static int get_save_file( Var **v );
static int print_array( Var *v, long cur_dim, long *start, int fid );
static int print_slice( Var *v, int fid );
static void f_format_check( Var *v );
static void ff_format_check( Var *v );
static long do_printf( int file_num, Var *v );
static int print_browser( int browser, int fid, const char* comment );
static int T_fprintf( int file_num, const char *fmt, ... );
static char *handle_escape( char *fmt_end );


/*--------------------------------------------------------------------*/
/* Function to check if a number passed to it is a valid file handle. */
/* This requires that the number is an integer variable. It returns 1 */
/* if there's an already opened file associated with this number and  */
/* 0 if there isn't.                                                  */
/*--------------------------------------------------------------------*/

Var *f_is_file( Var *v )
{
	if ( v == NULL )
	{
		eprint( FATAL, SET, "Missing argument in call of function %s().\n",
				Cur_Func );
		THROW( EXCEPTION )
	}

	if ( v->type != INT_VAR )
	{
		eprint( FATAL, SET, "Parameter of function %s() isn't a file "
				"handle.\n", Cur_Func );
		THROW( EXCEPTION )
	}

    if ( v->val.lval < 0 || v->val.lval >= File_List_Len ||
		 ( ! TEST_RUN && File_List[ v->val.lval ].fp == NULL ) )
		return vars_push( INT_VAR, 0 );

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------------*/
/* Function allows the user to select a file using the file selector. If the */
/* file already exists a confirmation by the user is required. Then the file */
/* is opened - if this fails the file selector is shown again. The FILE      */
/* pointer returned is stored in an array of FILE pointers for each of the   */
/* open files. The returned value is an INT_VAR with the index in the FILE   */
/* pointer array or -1 if no file was selected.                              */
/* Optional input variables (each will replaced by a default string if the   */
/* argument is either NULL or the empty string) are:                         */
/* 1. Message string (not allowed to start with a backslash `\'!)            */
/* 2. Default pattern for file name                                          */
/* 3. Default directory                                                      */
/* 4. Default file name                                                      */
/* 5. Default extension to add (in case it's not already there)              */
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
	char *s[ 5 ] = { NULL, NULL, NULL, NULL, NULL };
	FILE *fp;
	long len;
	struct stat stat_buf;
	static char *r = NULL;
	char *new_r, *m;
	static FILE_LIST *old_File_List;


	r = NULL;
	old_File_List = NULL;

	/* If there was a call of `f_save()' without a previous call to `f_getf()'
	   then `f_save()' already called `f_getf()' by itself and now does not
	   expects file identifiers anymore - in this case `No_File_Numbers' is
	   set. So, if we get a call to `f_getf()' while `No_File_Numbers' is set
	   we must tell the user that he can't have it both ways, i.e. (s)he
	   either has to call `f_getf()' before any call to `f_save()' or never. */

	if ( No_File_Numbers )
	{
		eprint( FATAL, SET, "Call of %s() after call of save()-type function "
				"without previous call of %s().\n", Cur_Func, Cur_Func );
		THROW( EXCEPTION )
	}

	/* While test is running just return a dummy value */

	if ( TEST_RUN )
		return vars_push( INT_VAR, File_List_Len++ );

	/* Check the arguments and supply default values if necessary */

	for ( i = 0, cur = var; i < 5 && cur != NULL; i++, cur = cur->next )
	{
		vars_check( cur, STR_VAR );
		s[ i ] = cur->val.sptr;
	}

	/* First string is the message */

	if ( s[ 0 ] != NULL && s[ 0 ][ 0 ] == '\\' )
		r = T_strdup( s[ 0 ] + 1 );

	if ( s[ 0 ] == NULL || s[ 0 ][ 0 ] == '\0' || s[ 0 ][ 0 ] == '\\' )
		s[ 0 ] = T_strdup( "Please enter a file name:" );
	else
		s[ 0 ] = T_strdup( s[ 0 ] );

	/* Second string is the file name pattern */

	if ( s[ 1 ] == NULL || s[ 1 ][ 0 ] == '\0' )
		s[ 1 ] = T_strdup( "*.dat" );
	else
		s[ 1 ] = T_strdup( s[ 1 ] );

	/* Third string is the default directory */

	if ( s[ 2 ] == NULL || s[ 2 ][ 1 ] == '\0' )
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
		   
	if ( s[ 4 ] == NULL || s[ 4 ][ 0 ] == '\0' )
		s[ 4 ] = NULL;
	else
		s[ 4 ] = T_strdup( s[ 4 ] );

getfile_retry:

	/* Try to get a filename - on 'Cancel' request confirmation (unless a
	   file name was passed to the routine and this is not a repeat call) */

	if ( r == NULL )
		r = T_strdup( show_fselector( s[ 0 ], s[ 2 ], s[ 1 ], s[ 3 ] ) );

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
		for ( i = 0; i < 5; i++ )
			T_free( s[ i ] );
		Dont_Save = SET;
		return vars_push( INT_VAR, -1 );
	}

	/* If given append default extension to the file name (but only if the
	   user didn't specified it already) */

	if ( s[ 4 ] != NULL &&
		 ( strrchr( r, '.' ) == NULL ||
		   strcmp( strrchr( r, '.' ) + 1, s[ 4 ] ) ) )
	{
		new_r = get_string( "%s.%s", r, s[ 4 ] );
		T_free( r );
		r = new_r;
	}

	/* Now ask for confirmation if the file already exists and try to open
	   it for writing */

	if  ( 0 == stat( r, &stat_buf ) )
	{
		m = get_string( "The selected file does already exist:\n%s\n"
						"\nDo you really want to overwrite it?", r );
		if ( 1 != show_choices( m, 2, "Yes", "No", NULL, 2 ) )
		{
			T_free( m );
			r = T_free( r );
			goto getfile_retry;
		}
		T_free( m );
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

	/* The reallocation for the File_List may fail but we still need to close
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
		THROW( EXCEPTION )
	}

	File_List[ File_List_Len ].fp = fp;
	File_List[ File_List_Len ].name = r;

	/* Switch buffering off so we're sure everything gets written to disk
	   immediately */

	setbuf( File_List[ File_List_Len ].fp, NULL );

	return vars_push( INT_VAR, File_List_Len++ );
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

Var *f_clonef( Var *v )
{
	char *fn;
	char *n;
	Var *new_v, *arg[ 5 ];
	int i;


	/* If the file handle passed to the function is -1 opening a file for
	   this file handle did not happen, so we also don't open the new file */

	if ( v->type == INT_VAR && v->val.lval == -1 )
		return vars_push( INT_VAR, -1 );

	/* Check all the parameter */

	if ( v->type != INT_VAR ||
		 v->val.lval < 0 || v->val.lval >= File_List_Len )
	{
		 eprint( FATAL, SET, "First argument in call of %s() isn't a vaild "
				 "file identifier.\n", Cur_Func );
		 THROW( EXCEPTION )
	}

	if ( v->next->type != STR_VAR || v->next->next->type != STR_VAR ||
		 *v->next->next->val.sptr == '\0' )
	{
		eprint( FATAL, SET, "Invalid second and third argument in call of "
				"%s().\n", Cur_Func );
		 THROW( EXCEPTION )
	}

	if ( TEST_RUN )
		return vars_push( INT_VAR, File_List_Len++ );

	fn = T_malloc(   strlen( File_List[ v->val.lval ].name )
				   + strlen( v->next->next->val.sptr ) + 3 );
	strcpy( fn, "\\" );
	strcat( fn, File_List[ v->val.lval ].name );

	n = fn + strlen( fn ) - strlen( v->next->val.sptr );
	if ( n > fn + 1 && *( n - 1 ) == '.' && 
		 ! strcmp( n, v->next->val.sptr ) )
		strcpy( n, v->next->next->val.sptr );
	else
	{
		strcat( n, "." );
		strcat( n, v->next->next->val.sptr );
	}

	arg[ 0 ] = vars_push( STR_VAR, fn );
	T_free( fn );

	n = get_string( "*.%s", v->next->next->val.sptr );
	arg[ 1 ] = vars_push( STR_VAR, n );
	T_free( n );

	arg[ 2 ] = vars_push( STR_VAR, "" );
	arg[ 3 ] = vars_push( STR_VAR, "" );
	arg[ 4 ] = vars_push( STR_VAR, v->next->next->val.sptr );

	new_v = f_getf( arg[ 0 ] );

	for ( i = 4; i >= 0; i-- )
		vars_pop( arg[ i ] );

	return new_v;
}


/*---------------------------------------------------------------------*/
/* This function is called by the functions for saving. If they didn't */
/* get a file identifier it is assumed the user wants just one file    */
/* that is opened at the first call of a function of the `save_xxx()'  */
/* family of functions.                                                */
/*---------------------------------------------------------------------*/

static int get_save_file( Var **v )
{
	Var *get_file_ptr;
	Var *file;
	int file_num;
	int acc;


	/* If no file has been selected yet get a file and then use it exclusively
	   (i.e. also expect that no file identifier is given in later calls),
	   otherwise the first variable has to be the file identifier */

	if ( File_List_Len == 0 )
	{
		if ( Dont_Save )
			return -1;

		No_File_Numbers = UNSET;

		get_file_ptr = func_get( "get_file", &acc );
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
				eprint( FATAL, SET, "First argument in %s() isn't a "
						"file identifier.\n", Cur_Func );
				THROW( EXCEPTION )
			}
			file_num = ( int ) ( *v )->val.lval;
		}
		else
		{
			eprint( WARN, SET, "Call of %s() without any arguments.\n",
					Cur_Func );
			return -1;
		}
		*v = ( *v )->next;
	}
	else
		file_num = 0;

	/* Check that the file identifier is reasonable */

	if ( file_num < 0 )
	{
		if ( ! Dont_Save )
			eprint( WARN, SET, "File has never been opened, skipping "
					"%s() command.\n", Cur_Func );
		return -1;
	}

	if ( file_num >= File_List_Len )
	{
		eprint( FATAL, SET, "Invalid file identifier in %s().\n", Cur_Func );
		THROW( EXCEPTION )
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
/* It returns the number of characters written.                             */
/*--------------------------------------------------------------------------*/

Var *f_save( Var *v )
{
	long i;
	long start;
	int file_num;
	int count = 0;
	int sub_count;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, SET, "Call of %s() without data.\n", Cur_Func );
		return vars_push( INT_VAR, 0 );
	}

	do
	{
		switch( v->type )
		{
			case INT_VAR :
				count += T_fprintf( file_num, "%ld\n", v->val.lval );
				break;

			case FLOAT_VAR :
				count += T_fprintf( file_num, "%#.9g\n", v->val.dval );
				break;

			case STR_VAR :
				count += T_fprintf( file_num, "%s\n",
									handle_escape( v->val.sptr ) );
				break;

			case INT_ARR :
				for ( i = 0; i < v->len; i++ )
					count += T_fprintf( file_num, "%ld\n", v->val.lpnt[ i ] );
				break;
				
			case FLOAT_ARR :
				for ( i = 0; i < v->len; i++ )
					count += T_fprintf( file_num, "%#.9g\n",
										v->val.dpnt[ i ] );
				break;

			case ARR_PTR :
				if ( ( sub_count = print_slice( v, file_num ) ) >= 0 )
					count += sub_count;
				else
					THROW( EXCEPTION )
				break;

			case ARR_REF :
				if ( v->from->flags && NEED_ALLOC )
				{
					eprint( WARN, SET, "Variable sized array `%s' is still "
							"undefined in %s() - skipping'.\n",
							v->from->name, Cur_Func );
					break;
				}
				start = 0;
				if ( ( sub_count =  print_array( v->from, 0, &start,
												 file_num ) ) >= 0 )
					count += sub_count;
				else
					THROW( EXCEPTION )
				break;

			default :
				fsc2_assert( 1 == 0 );
				break;
		}
	} while ( ( v = vars_pop( v ) ) );

	return vars_push( INT_VAR, ( long ) count );
}


/*---------------------------------------------------------------------*/
/* Writes a complete array to a file, returns number of chars it wrote */
/*---------------------------------------------------------------------*/

static int print_array( Var *v, long cur_dim, long *start, int fid )
{
	long i;
	int total_count = 0;
	int count = 0;


	if ( cur_dim < v->dim - 1 )
	{
		for ( i = 0; i < v->sizes[ cur_dim ]; i++ )
			if ( ( count = print_array( v, cur_dim + 1, start, fid ) ) < 0 )
				return -1;
			else
				total_count += count;
	}
	else
	{
		for ( i = 0; i < v->sizes[ cur_dim ]; ( *start )++, i++ )
		{
			if ( v->type == INT_CONT_ARR )
				total_count += T_fprintf( fid, "%ld\n",
										  v->val.lpnt[ *start ] );
			else
				total_count += T_fprintf( fid, "%#.9g\n",
										  v->val.dpnt[ *start ] );
		}

		total_count += T_fprintf( fid, "\n" );
	}

	return total_count;
}


/*-------------------------------------------------------------------*/
/* Writes an array slice to a file, returns number of chars it wrote */
/*-------------------------------------------------------------------*/

static int print_slice( Var *v, int fid )
{
	long i;
	int count = 0;


	for ( i = 0; i < v->from->sizes[ v->from->dim - 1 ]; i++ )
		if ( v->from->type == INT_CONT_ARR )
			count += T_fprintf( fid, "%ld\n",
								* ( ( long * ) v->val.gptr + i ) );
		else
			count += T_fprintf( fid, "%#.9g\n",
								* ( ( double * ) v->val.gptr + i ) );
	return count;
}


/*--------------------------------------------------------------------------*/
/* Saves data to a file. If `get_file()' hasn't been called yet it will be  */
/* called now - in this case the file opened this way is the only file to   */
/* be used and no file identifier is allowed as first argument to `save()'. */
/* This function is the formated save with the same meaning of the format   */
/* string as in `print()'.                                                  */
/* The function returns the number of chars it wrote to the file.           */
/*--------------------------------------------------------------------------*/

Var *f_fsave( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, SET, "Call of %s() without format string and data.\n",
				Cur_Func );
		return vars_push( INT_VAR, 0 );
	}

	if ( v->type != STR_VAR )
	{
		eprint( FATAL, SET, "Missing format string in call of %s()\n",
				Cur_Func );
		THROW( EXCEPTION )
	}

	/* Rewrite the format string and check thar arguments are ok, then print */

	f_format_check( v );

	return vars_push( INT_VAR, ( long ) do_printf( file_num, v ) );
}


/*-------------------------------------------------------------------------*/
/* This function gets called from f_fsave() to make from its format string */
/* a format string that can be passed to fprintf()'s format string.        */
/*-------------------------------------------------------------------------*/

static void f_format_check( Var *v )
{
	char *cp;
	Var *cv;
	ptrdiff_t s2c;


	/* Replace the '#' characters with the appropriate conversion specifiers */

	for ( cp = v->val.sptr, cv = v->next; *cp != '\0'; cp++ )
	{
		/* '%' must be replaced by "%%" */

		if ( *cp == '%' )
		{
			s2c = cp - v->val.sptr;
			v->val.sptr = T_realloc( v->val.sptr, strlen( v->val.sptr ) + 2 );
			cp = v->val.sptr + s2c;
			memmove( cp + 1, cp, strlen( cp ) + 1 );
			cp++;
			continue;
		}

		if ( *cp == '\\' )
		{
			int sc;

			for ( sc = 0; *cp == '\\'; sc++, cp++ )
				;

			if ( sc & 1 && *cp =='#' )
				memmove( cp - 1, cp, strlen( cp ) + 1 );

			cp--;
			continue;
		}

		if ( *cp != '#' )
			continue;

		if ( cv == NULL )
		{
			eprint( FATAL, SET, "Less data than format descriptors in %s() "
					"format string.\n", Cur_Func );
			THROW( EXCEPTION )
		}

		switch ( cv->type )
		{
			case INT_VAR :
				s2c = cp - v->val.sptr;
				v->val.sptr = T_realloc( v->val.sptr,
										 strlen( v->val.sptr ) + 2 );
				cp = v->val.sptr + s2c;
			    memmove( cp + 2, cp + 1, strlen( cp ) );
				memcpy( cp, "%d", 2 );
				cp++;
				break;

			case FLOAT_VAR :
				s2c = cp - v->val.sptr;
				v->val.sptr = T_realloc( v->val.sptr,
										 strlen( v->val.sptr ) + 5 );
				cp = v->val.sptr + s2c;
			    memmove( cp + 5, cp + 1, strlen( cp ) );
				memcpy( cp, "%#.9g", 5 );
				cp += 4;
				break;

			case STR_VAR :
				s2c = cp - v->val.sptr;
				v->val.sptr = T_realloc( v->val.sptr,
										 strlen( v->val.sptr ) + 2 );
				cp = v->val.sptr + s2c;
			    memmove( cp + 2, cp + 1, strlen( cp ) );
				memcpy( cp, "%s", 2 );
				handle_escape( cv->val.sptr );
				cp++;
				break;

			default :
				eprint( FATAL, SET, "Function %s() can only write numbers and "
						"strings to a file.\n", Cur_Func );
				THROW( EXCEPTION )
		}

		cv = cv->next;
	}

	if ( cv != NULL )
	{
		eprint( SEVERE, SET, "More data than format descriptors in "
				"%s() format string.\n", Cur_Func );
		while ( ( cv = vars_pop( cv ) ) != NULL )
			;
	}

	/* Finally replace the escpe sequences in the format string */

	handle_escape( v->val.sptr );
}


/*-------------------------------------------------------------------------*/
/* This function writes data to a file according to a format string, which */
/* resembles strongly the format string that printf() and friends accepts. */
/* The only things not supported (because they don't make sense here) are  */
/* printing of chars, unsigned quantities and pointers and pointers (i.e.  */
/* the 'c', 'o', 'x', 'X', 'u' and 'p' conversion specifiers cannot be     */
/* used) and no length specifiers (i.e. 'h', 'l' and 'L') are accepted.    */
/* Everything else should work, including all escape sequences (with the   */
/* exception of trigraph sequences).                                       */
/* The function returns the number of chars written to the file.           */
/*-------------------------------------------------------------------------*/

Var *f_ffsave( Var *v )
{
	int file_num;


	/* Determine the file identifier */

	if ( ( file_num = get_save_file( &v ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v == NULL )
	{
		eprint( WARN, SET, "Call of %s() without data.\n", Cur_Func );
		return vars_push( INT_VAR, 0 );
	}

	if ( v->type != STR_VAR )
	{
		eprint( FATAL, SET, "Missing format string in call of %s()\n",
				Cur_Func );
		THROW( EXCEPTION )
	}

	/* Check that format string and arguments are ok, then print */

	ff_format_check( v );

	return vars_push( INT_VAR, do_printf( file_num, v ) );
}


/*--------------------------------------------------------------------------*/
/* Function is used by ff_save( ) to check that the format string is well-  */
/* formed and to find out if there are at least as many arguments as needed */
/* by the format string (if there are more a warning is printed and the     */
/* superfluous arguments are discarded).                                    */
/*--------------------------------------------------------------------------*/

static void ff_format_check( Var *v )
{
	const char *sptr = v->val.sptr;
	Var *vptr = v->next;


	/* Replace escape characters in the format string */

	handle_escape( v->val.sptr );

	/* Loop over the format string to figure out if there are enough arguments
	   for the format string and that the argument types are the ones expected
	   by the conversion modifiers. */

	while ( 1 )
	{
		/* Skip over everything that's not a conversion specifier */

		for ( ; *sptr != '\0' && *sptr != '%'; sptr++ )
			;

		if ( *sptr++ == '\0' )
			break;

		if ( *sptr == '\0' )
		{
			eprint( FATAL, SET, "'%%' found at end of format string in call "
					"of %s().\n", Cur_Func );
			THROW( EXCEPTION )
		}

		if ( *sptr == '%' )
			continue;

		/* First thing to be expected in a conversion specifier are flags */

		while ( *sptr == '-' || *sptr == '+' || *sptr == ' ' ||
				*sptr == '0' || *sptr == '#' )
		{		
			sptr++;
			if ( *sptr == '\0' )
			{
				eprint( FATAL, SET, "End of format string within conversion "
						"specifier in call of %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}
		}

		/* Next a minimum length and precision might follow, if one or both
		   of these are given by a '*' we need an integer argument to
		   specifiy the number to be used */

		if ( *sptr == '*' )
		{
			if ( vptr->type != INT_VAR )
			{
				eprint( FATAL, SET, "Non-integer variable used as field "
						"length in format string in %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}

			sptr++;
			vptr = vptr->next;
		}
		else if ( isdigit( *sptr ) )
			while ( isdigit( *++sptr ) )
				;

		if ( *sptr == '\0' )
		{
			eprint( FATAL, SET, "End of format string within conversion "
					"specifier in call of %s().\n", Cur_Func );
			THROW( EXCEPTION )
		}

		if ( *sptr == '.' )
		{
			sptr++;

			if ( *sptr == '*' )
			{
				if ( vptr == NULL )
				{
					eprint( FATAL, SET, "Not enough arguments for format "
							"string in %s().\n", Cur_Func );
					THROW( EXCEPTION )
				}

				if ( vptr->type != INT_VAR )
				{
					eprint( FATAL, SET, "Non-integer variable used as field "
							"length in format string in %s().\n", Cur_Func );
					THROW( EXCEPTION )
				}

				sptr++;
				vptr = vptr->next;
			}
			else if ( isdigit( *sptr ) )
				while ( isdigit( *++sptr ) )
					;

			if ( *sptr == '\0' )
			{
				eprint( FATAL, SET, "End of format string within conversion "
						"specifier in call of %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}
		}

		/* Now the conversion specifier has to follow, this can be either
		   's', 'd', 'i', 'f', 'e', 'g', 'E', 'G', or 'n'. For 'n' no
		   argument is needed because it just prints the number of character
		   written up to the moment the 'n' is found in the format string. */

		if ( *sptr == 's' )
		{
			if ( vptr == NULL )
			{
				eprint( FATAL, SET, "Not enough arguments for format "
						"string in %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}

			if ( vptr->type != STR_VAR )
			{
				eprint( FATAL, SET, "Non-string variable found for string "
						"type conversion specifier in format string in "
						"%s().\n", Cur_Func );
				THROW( EXCEPTION )
			}
				
			handle_escape( vptr->val.sptr );
			sptr++;
			vptr = vptr->next;
			continue;
		}

		if ( *sptr == 'd' || *sptr == 'i' )
		{
			if ( vptr == NULL )
			{
				eprint( FATAL, SET, "Not enough arguments for format "
						"string in %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}

			if ( vptr->type != INT_VAR )
			{
				eprint( WARN, SET, "Non-integer variable found for integer "
						"type conversion specifier in format string in "
						"%s().\n", Cur_Func );
				THROW( EXCEPTION )
			}
				
			sptr++;
			vptr = vptr->next;
			continue;
		}

		if ( *sptr == 'f' || *sptr == 'e' || *sptr == 'g' ||
			 *sptr == 'E' || *sptr == 'G' )
		{
			if ( vptr == NULL )
			{
				eprint( FATAL, SET, "Not enough arguments for format "
						"string in %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}

			if ( vptr->type != FLOAT_VAR )
			{
				eprint( WARN, SET, "Non-floating point variable found for "
						"float type conversion specifier in format string "
						"in %s().\n", Cur_Func );
				THROW( EXCEPTION )
			}
				
			sptr++;
			vptr = vptr->next;
			continue;
		}

		eprint( FATAL, SET, "Unknown conversion specifier '%c' found in "
				"format string in %s().\n", *sptr, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Check for superfluous arguments, print a warning if there are any and
	   discard them */

	if ( vptr != NULL )
	{
		eprint( SEVERE, SET, "More arguments than conversion specifiers found "
				"in format string in %&s().\n", Cur_Func );
		while ( ( vptr = vars_pop( vptr ) ) != NULL )
			;
	}
}


/*-----------------------------------------------------------------------*/
/* This function is called by f_fsave() and f_ffsave() to do the actual  */
/* printing. It loops over the format string and prints it argument by   */
/* argument (unfortunately, there's portable no way to create a variable */
/* argument list, so it must be done this way). The fuinction returns    */
/* the number of character written.                                      */
/*-----------------------------------------------------------------------*/

static long do_printf( int file_num, Var *v )
{
	static char *fmt_start,
		        *fmt_end,
		        *sptr;
	static Var *cv;
	static int count;
	char store;
	int need_vars;
	int need_type;

	
	sptr = v->val.sptr;
	fmt_start = fmt_end = T_malloc( strlen( sptr ) + 2 );
	strcpy( fmt_start, sptr );
	cv = v->next;
	count = 0;

	TRY
	{
		/* Print everything up to the first conversion specifier */

		while ( *fmt_end != '\0' )
		{
			if ( *fmt_end == '%' )
			{
				if ( *( fmt_end + 1 ) == '%' )
				{
					fmt_end += 2;
					continue;
				}
				else
					break;
			}

			fmt_end++;
		}

		store = *fmt_end;
		*fmt_end = '\0';

		if ( fmt_start != fmt_end )
			count += T_fprintf( file_num, fmt_start );

		if ( store == '\0' )                         /* already at the end ? */
		{
			TRY_SUCCESS;
			T_free( fmt_start );
			return ( long ) count;
		}

		sptr += fmt_end - fmt_start;
		strcpy( fmt_start, sptr );
		fmt_end = fmt_start + 1;

		/* Now repeat printing. starting each time with a conversion specifier
		   and ending just before the next one until the end of the format
		   string has been reached */

		while ( 1 )
		{
			need_vars = 0;                  /* how many arguments are needed */
			need_type = -1;                 /* type of the argument to print */

			/* Skip over flags */

			while ( *fmt_end == '-' || *fmt_end == '+' || *fmt_end == ' ' ||
					*fmt_end == '0' || *fmt_end == '#' )
				fmt_end++;

			/* Deal with the minumum length and precision fields, checking
			   if, due to a '*' an argument is needed */

			if ( *fmt_end == '*' )
			{
				need_vars++;
				fmt_end++;
			}
			else if ( isdigit( *fmt_end ) )
				while ( isdigit( *++fmt_end ) )
					;

			if ( *fmt_end == '.' )
			{
				if ( *++fmt_end == '*' )
				{
					need_vars++;
					fmt_end++;
				}
				else if ( isdigit( *fmt_end ) )
					while ( isdigit( *++fmt_end ) )
						;
			}

			/* Find out about the type of the argument to be printed */

			if ( *fmt_end == 's' )
			{
				need_type = 0;       /* string */
				need_vars++;
			}

			if ( *fmt_end == 'd' || *fmt_end == 'i' )
			{
				/* Integers are always 'long' integers, so an 'l' must be
				   inserted in front of the 'd' or 'i' */

				memmove( fmt_end + 1, fmt_end, strlen( fmt_end ) + 1 );
				*fmt_end++ = 'l';

				need_type = 1;       /* integer */
				need_vars++;
			}

			if ( *fmt_end == 'f' || *fmt_end == 'e' || *fmt_end == 'g' ||
				 *fmt_end == 'E' || *fmt_end == 'G' )
			{
				need_type = 2;       /* float */
				need_vars++;
			}

			/* Now get rest of string until the next conversion specifier or
			   the end of the format string is reached */

			while ( *fmt_end != '\0' )
			{
				if ( *fmt_end == '%' )
				{
					if ( *( fmt_end + 1 ) == '%' )
					{
						fmt_end += 2;
						continue;
					}
					else
						break;
				}

				fmt_end++;
			}

			store = *fmt_end;
			*fmt_end = '\0';

			/* Call the function doing the printing, supplying it with the
			   necessary number of arguments */

			switch ( need_vars )
			{
				case 1 :
					switch ( need_type )
					{
						case 0 : /* string */
							count += T_fprintf( file_num, fmt_start,
												cv->val.sptr );
							break;

						case 1 : /* integer */
							if ( cv->type == INT_VAR )
								count += T_fprintf( file_num, fmt_start,
													cv->val.lval );
							else
								count += T_fprintf( file_num, fmt_start,
													lround( cv->val.dval ) );

							break;

						case 2 : /* double */
							if ( cv->type == FLOAT_VAR )
								count += T_fprintf( file_num, fmt_start,
													cv->val.dval );
							else
								count += T_fprintf( file_num, fmt_start,
													( double ) cv->val.lval );
							break;

						default :
							fsc2_assert( 1 == 0 );
					}

					cv = vars_pop( cv );
					break;

				case 2 :
					switch ( need_type )
					{
						case 0 : /* string */
							count += T_fprintf( file_num, fmt_start,
												( int ) cv->val.lval,
												cv->next->val.sptr );
							break;

						case 1 : /* integer */
							if ( cv->next->type == INT_VAR )
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													cv->next->val.lval );
							else
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													lround(
														cv->next->val.dval ) );
							break;

						case 2 : /* double */
							if ( cv->next->type == FLOAT_VAR )
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													cv->next->val.dval );
							else
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													( long )
													      cv->next->val.lval );
							break;

						default :
							fsc2_assert( 1 == 0 );
					}

					cv = vars_pop( vars_pop( cv ) );
					break;

				case 3 :
					switch ( need_type )
					{
						case 0 : /* string */
							count += T_fprintf( file_num, fmt_start,
												( int ) cv->val.lval,
												( int ) cv->next->val.lval,
												cv->next->next->val.sptr );
							break;

						case 1 : /* integer */
							if ( cv->next->next->type == INT_VAR )
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													( int ) cv->next->val.lval,
													cv->next->next->val.lval );
							else
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													( int ) cv->next->val.lval,
													lround(
													cv->next->next->val.dval )
									               );
							break;

						case 2 : /* double */
							if ( cv->next->next->type == FLOAT_VAR )
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													( int ) cv->next->val.lval,
													cv->next->next->val.dval );
							else
								count += T_fprintf( file_num, fmt_start,
													( int ) cv->val.lval,
													( int ) cv->next->val.lval,
													( long )
													cv->next->next->val.lval );
							break;

						default :
							fsc2_assert( 1 == 0 );
					}

					cv = vars_pop( vars_pop( vars_pop( cv ) ) );
					break;

				default :
					fsc2_assert( 1 == 0 );
			}

			if ( store == '\0' )             /* end reached ? */
				break;

			sptr += fmt_end - fmt_start - ( need_type == 1 ? 1 : 0 );
			strcpy( fmt_start, sptr );
			fmt_end = fmt_start + 1;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( fmt_start );
		PASSTHROU( )
	}

	T_free( fmt_start );
	return ( long ) count;
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

	if ( ( file_num = get_save_file( &v ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	if ( ! print_browser( 0, file_num, v != NULL ? v->val.sptr : "" ) )
		THROW( EXCEPTION )

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

	if ( ( file_num = get_save_file( &v ) ) == -1 )
		return vars_push( INT_VAR, 0 );

	if ( v != NULL )
		vars_check( v, STR_VAR );

	if ( TEST_RUN )
		return vars_push( INT_VAR, 1 );

	if ( ! print_browser( 1, file_num, v != NULL ? v->val.sptr : "" ) )
		THROW( EXCEPTION )

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------------*/
/* Writes the content of the program or the error browser into a file. */
/* Input parameter:                                                    */
/* 1. 0: writes program browser, 1: error browser                      */
/* 2. File identifier                                                  */
/* 3. Comment string to prepend to each line                           */
/*---------------------------------------------------------------------*/

static int print_browser( int browser, int fid, const char* comment )
{
	char *line;
	char *lp;
	int count;


	writer( browser ==  0 ? C_PROG : C_OUTPUT );
	if ( comment == NULL )
		comment = "";
	count = T_fprintf( fid, "%s\n", comment );

	while ( 1 )
	{
		reader( &line );
		lp = line;
		if ( line != NULL )
		{
			if ( browser == 0 )
				while ( *lp++ != ':' )
					;
			else if ( *line == '@' )
			{
				lp++;
				while ( *lp++ != 'f' )
					;
			}

			count += T_fprintf( fid, "%s%s\n", comment, lp );
		}
		else
			break;
	}

	count += T_fprintf( fid, "%s\n", comment );

	return count;
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

	if ( ( file_num = get_save_file( &v ) ) == -1 )
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


/*--------------------------------------------------------------------*/
/* Function that does all the actual printing. It does a lot of tests */
/* in order to make sure that really everything get written to a file */
/* and tries to handle situations gracefully where there isn't enough */
/* room left on a disk by asking for a replacement file and copying   */
/* everything already written to the file on the full disk into the   */
/* replacement file. The function returns the number of chars written */
/* to the file (but not the copied bytes in case a replacement file   */
/* had to be used).                                                   */
/*--------------------------------------------------------------------*/

static int T_fprintf( int file_num, const char *fmt, ... )
{
	int n;                      /* number of bytes we need to write */
	static int size;            /* guess for number of characters needed */
	static char *p;
	va_list ap;
	char *new_name;
	FILE *new_fp;
	struct stat old_stat, new_stat;
	char *buffer[ 1024 ];
	int rw;
	char *mess;
	int count;


	/* If the file has been closed because of insufficient place and no
       replacement file has been given just don't print */

	if ( ! TEST_RUN )
	{
		if ( file_num < 0 || file_num >= File_List_Len )
		{
			eprint( FATAL, SET, "Invalid file handle used in call of "
					"function %s().\n", Cur_Func );
			THROW( EXCEPTION )
		}

		if ( File_List[ file_num ].fp == NULL )
			return 0;
	}

	size = 1024;

	/* First we've got to find out how many characters we need to write out */

	p = T_malloc( size );

	while ( 1 ) {

        /* Try to print into the allocated space */

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
                THROW( EXCEPTION )
            }
        }

		TRY
		{
			p = T_realloc( p, size );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( p );
			PASSTHROU( )
		}
    }

	if ( TEST_RUN )
	{
		T_free( p );
		return n;
	}

    /* Now we try to write the string to the file */

    if ( ( count = fprintf( File_List[ file_num ].fp, "%s", p ) ) == n )
    {
        T_free( p );
        return n;
	}

    /* Couldn't write as many bytes as needed - disk seems to be full */

	mess = get_string( "Disk full while writing to file\n%s\n"
					   "Please choose a new file.",
					   File_List[ file_num ].name );
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
		return count;
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

	/* Check if the new and the old file are identical. If they are we simply
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

			rw = fread( buffer, 1, 1024, File_List[ file_num ].fp );

			if ( rw != 0 )
				fwrite( buffer, 1, rw, new_fp );

			if ( ferror( new_fp ) )
			{
				mess = get_string( "Can't write to replacement file\n%s\n"
								   "Please choose a different file.",
								   new_name );
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
		return n;
	}

	/* Ooops, also failed to write to new file */

	mess = get_string( "Can't write to (replacement) file\n%s\n"
					   "Please choose a different file.",
					   File_List[ file_num ].name );
	show_message( mess );
	T_free( mess );
	goto get_repl_retry;
}


/*-----------------------------------------------------------------*/
/* Function replaces escape sequences in a string by the character */
/* the stand for - all C type escape sequences are supported.      */         
/*-----------------------------------------------------------------*/

static char *handle_escape( char *str )
{
	char *cp;
	int esc_len;


	for ( cp = str; *cp != '\0'; cp++)
	{
		if ( *cp != '\\' )
			continue;

		switch ( *( cp + 1 ) )
		{
			case '\0' :
				eprint( FATAL, SET, "End of string directly following escape "
						"character '\\'.\n" );
				THROW( EXCEPTION )
				break;

			case 'a' :
				*cp = '\a';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'b' :
				*cp = '\b';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'f' :
				*cp = '\f';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'n' :
				*cp = '\n';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'r' :
				*cp = '\r';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 't' :
				*cp = '\t';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'v' :
				*cp = '\v';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case '\\' :
				*cp = '\\';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case '\?' :
				*cp = '\?';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case '\'' :
				*cp = '\'';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case '\"' :
				*cp = '\"';
				memmove( cp + 1, cp + 2, strlen( cp + 1 ) );
				break;

			case 'x' :
				if ( ! isdigit( *( cp + 2 ) ) &&
					 ( toupper( *( cp + 2 ) ) < 'A' ||
					   toupper( *( cp + 2 ) ) > 'F' ) )
				{
					eprint( FATAL, SET, "'\\x' with no following hex digits "
							"in string.\n" );
					THROW( EXCEPTION )
				}
				esc_len = 1;
				*cp = *( cp + 2 )
					  - ( isdigit( *( cp + 2 ) ) ? '0' : ( 'A' + 10 ) );

				if ( isdigit( *( cp + 3 ) ) ||
					 ( toupper( *( cp + 3 ) ) >= 'A' &&
					   toupper( *( cp + 3 ) ) <= 'F' ) )
				{
					esc_len++;
					*cp = *cp * 16
					      + ( *( cp + 3 )
						  - ( isdigit( *( cp + 3 ) ) ? '0' : ( 'A' + 10 ) ) );
				}

				memmove( cp + 1, cp + 2 + esc_len,
						 strlen( cp + 1 + esc_len ) );
				break;

			default :
				if ( *( cp + 1 ) < '0' || *( cp + 1 ) > '7' )
				{
					eprint( FATAL, SET, "Unknown escape sequence '\\%c' in "
							"string.\n", *( cp + 1 ) );
					THROW( EXCEPTION )
				}

				*cp = *( cp + 1 ) - '0';
				esc_len = 1;

				if ( *( cp + 2 ) >= '0' && *( cp + 2 ) <= '7' )
				{
					*cp = *cp * 8 + *( cp + 2 ) - '0';
					esc_len++;

					if ( *( cp + 3 ) >= '0' && *( cp + 3 ) <= '7' &&
						 *cp < 0x1F )
					{
						*cp = *cp * 8 + *( cp + 3 ) - '0';
						esc_len++;
					}
				}

				memmove( cp + 1, cp + 1 + esc_len, strlen( cp + esc_len ) );
				break;
		}
	}

	return str;
}
