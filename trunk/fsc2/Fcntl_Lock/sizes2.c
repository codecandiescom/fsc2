/*
  This program is free software; you can redistribute it and/or modify it
  under the same terms as Perl itself.

  Copyright (C) 2002 Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>

  $Id$
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>


int main( void )
{
	struct flock f;
	FILE *fp;


	if ( ( fp = fopen( "Fcntl_Lock.h", "w" ) ) == NULL )
		 return EXIT_FAILURE;

	fprintf( fp, "/* Please don't change - created automatically during "
			     "installation. */\n\n" );

	if ( sizeof( f.l_type ) == sizeof( char ) )
		fprintf( fp, "#define LTYPE_TYPE      char\n" );
	else if ( sizeof( f.l_type ) == sizeof( short int ) )
		fprintf( fp, "#define LTYPE_TYPE      short int\n" );
	else if ( sizeof( f.l_type ) == sizeof( int ) )
		fprintf( fp, "#define LTYPE_TYPE      int\n" );
	else if ( sizeof( f.l_type ) == sizeof( long ) )
		fprintf( fp, "#define LTYPE_TYPE      long\n" );
	else
	{
		fclose( fp );
		unlink( "Fcntl_Lock.h" );
		return EXIT_FAILURE;
	}

	if ( sizeof( f.l_whence ) == sizeof( char ) )
		fprintf( fp, "#define LWHENCE_TYPE    char\n" );
	else if ( sizeof( f.l_whence ) == sizeof( short int ) )
		fprintf( fp, "#define LWHENCE_TYPE    short int\n" );
	else if ( sizeof( f.l_whence ) == sizeof( int ) )
		fprintf( fp, "#define LWHENCE_TYPE    int\n" );
	else if ( sizeof( f.l_whence ) == sizeof( long ) )
		fprintf( fp, "#define LWHENCE_TYPE    long\n" );
	else
	{
		fclose( fp );
		unlink( "Fcntl_Lock.h" );
		return EXIT_FAILURE;
	}

	if ( sizeof( f.l_start ) == sizeof( char ) )
		fprintf( fp, "#define LSTART_TYPE     char\n" );
	else if ( sizeof( f.l_start ) == sizeof( short int ) )
		fprintf( fp, "#define LSTART_TYPE     short int\n" );
	else if ( sizeof( f.l_start ) == sizeof( int ) )
		fprintf( fp, "#define LSTART_TYPE     int\n" );
	else if ( sizeof( f.l_start ) == sizeof( long ) )
		fprintf( fp, "#define LSTART_TYPE     long\n" );
	else
	{
		fclose( fp );
		unlink( "Fcntl_Lock.h" );
		return EXIT_FAILURE;
	}

	if ( sizeof( f.l_len ) == sizeof( char ) )
		fprintf( fp, "#define LLEN_TYPE       char\n" );
	else if ( sizeof( f.l_len ) == sizeof( short int ) )
		fprintf( fp, "#define LLEN_TYPE       short int\n" );
	else if ( sizeof( f.l_len ) == sizeof( int ) )
		fprintf( fp, "#define LLEN_TYPE       int\n" );
	else if ( sizeof( f.l_len ) == sizeof( long ) )
		fprintf( fp, "#define LLEN_TYPE       long\n" );
	else
	{
		fclose( fp );
		unlink( "Fcntl_Lock.h" );
		return EXIT_FAILURE;
	}

	if ( sizeof( f.l_pid ) == sizeof( char ) )
		fprintf( fp, "#define LPID_TYPE       char\n" );
	else if ( sizeof( f.l_pid ) == sizeof( short int ) )
		fprintf( fp, "#define LPID_TYPE       short int\n" );
	else if ( sizeof( f.l_pid ) == sizeof( int ) )
		fprintf( fp, "#define LPID_TYPE       int\n" );
	else if ( sizeof( f.l_pid ) == sizeof( long ) )
		fprintf( fp, "#define LPID_TYPE       long\n" );
	else
	{
		fclose( fp );
		unlink( "Fcntl_Lock.h" );
		return EXIT_FAILURE;
	}

	fprintf( fp, "#define LTYPE_OFFSET    %ld\n",
			 ( unsigned long ) ( ( char * ) &f.l_type - ( char * ) &f ) );
	fprintf( fp, "#define LWHENCE_OFFSET  %ld\n",
			 ( unsigned long ) ( ( char * ) &f.l_whence - ( char * ) &f ) );
	fprintf( fp, "#define LSTART_OFFSET   %ld\n",
			 ( unsigned long ) ( ( char * ) &f.l_start - ( char * ) &f ) );
	fprintf( fp, "#define LLEN_OFFSET     %ld\n",
			 ( unsigned long ) ( ( char * ) &f.l_len - ( char * ) &f ) );
	fprintf( fp, "#define LPID_OFFSET     %ld\n",
			 ( unsigned long ) ( ( char * ) &f.l_pid - ( char * ) &f ) );

	fprintf( fp, "#define STRUCT_SIZE     %ld\n",
			 ( unsigned long ) ( sizeof f ) );

	fprintf( fp, "#define REAL_F_GETLK    %d\n", F_GETLK );
	fprintf( fp, "#define REAL_F_SETLK    %d\n", F_SETLK );
	fprintf( fp, "#define REAL_F_SETLKW   %d\n", F_SETLKW );

	fclose( fp );
	return EXIT_SUCCESS;
}
