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

#include <netdb.h>
#include <sys/un.h>
#include <sys/param.h>
#include <sys/socket.h>


#define MAIL_PORT 25

#if defined IPv6
#define AF_INET_x AF_INET6
#else
#define AF_INET_x AF_INET
#endif


#define MAX_MAIL_LINE      4096


static int do_send( const char *rec_host, const char *to,
					const char *from, const char *local_host,
					const char *subject, FILE *fp );
static int open_mail_socket( const char *host );
static ssize_t writen( int fd, const void *vptr, size_t n );
static ssize_t read_line( int fd, void *vptr, size_t max_len );
static ssize_t do_read( int fd, char *ptr );


/*---------------------------------------------------------------*/
/* Sends a mail with body taken from the file pointed to by 'fp' */
/* 'from user 'from' with subject line 'subject' to address 'to' */
/* and, if non-zero' also to 'cc_to'.                            */
/*---------------------------------------------------------------*/

void send_mail( const char *subject, const char *from, const char* cc_to,
				const char *to, FILE *fp )
{
	struct hostent *hp;
	char *rec_host;
	char local_host[ MAXHOSTNAMELEN + 1 ];


	if ( gethostname( local_host, sizeof local_host ) != 0 )
		return;

	if ( ( hp = gethostbyname( local_host ) ) == NULL || hp->h_name == NULL )
		return;

	strncpy( local_host, hp->h_name, MAXHOSTNAMELEN );

	if ( ( rec_host = strchr( to, '@' ) ) == NULL )
		rec_host = local_host;
	else
		rec_host++;

	if ( do_send( rec_host, to, from, local_host, subject, fp ) == 0
		 && cc_to != NULL )
	{
		rewind( fp );
		do_send( local_host, cc_to, from, local_host, subject, fp );
	}
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static int do_send( const char *rec_host, const char *to,
					const char *from, const char *local_host,
					const char *subject, FILE *fp )
{
	int mfd;
	char line[ MAX_MAIL_LINE ];
	ssize_t len;
	

	if ( ( mfd = open_mail_socket( rec_host ) ) < 0 )
		return -1;

	/* Now we have a connection to the other side, get the intro text -
	   the other side must send at least "220\n" */

	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "220", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line telling the other side about ourself */

	if ( ( len = snprintf( line, MAX_MAIL_LINE, "HELO %s\r\n", local_host ) )
		 > MAX_MAIL_LINE ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, it must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line about the sender of the mail */

	if ( ( len = snprintf( line, MAX_MAIL_LINE, "MAIL FROM:<%s@%s>\r\n",
						   from, local_host ) )
		 > MAX_MAIL_LINE ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line telling the other side about the receiver of the mail */

	if ( ( len = snprintf( line, MAX_MAIL_LINE, "RCPT TO:<%s>\r\n", to ) )
		 > MAX_MAIL_LINE ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Tell the other side we're no going to start with the mail body */

	if ( writen( mfd, "DATA\r\n", 6 ) < 6 )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "354" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "354", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Now send the message, starting with the header */

	if ( ( len = snprintf( line, MAX_MAIL_LINE, "Subject: %s\n", subject ) )
		 > MAX_MAIL_LINE  ||
		 writen( mfd, line, len ) < len ||
		 ( len = snprintf( line, MAX_MAIL_LINE, "From: fsc2 <fsc2@%s>\n",
						   local_host ) ) > MAX_MAIL_LINE  ||
		 writen( mfd, line, len ) < len ||
		 ( len = snprintf( line, MAX_MAIL_LINE, "To: %s\n", to ) )
		 > MAX_MAIL_LINE ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Copy the whole input file to the socket */

	while ( fgets( line, MAX_MAIL_LINE, fp ) != NULL )
	{
		/* Send an additional dot before the line if the line consists of
		   nothing more than a dot */

		if ( line[ 0 ] == '.' &&
			 ( line[ 1 ] == '\0' || line[ 1 ] == '\n' || 
			   ( line[ 1 ] == '\r' && line[ 2 ] == '\n' ) ) &&
			 writen( mfd, ".", 1 ) < 1 )
		{
			close( mfd );
			return -1;
		}

		len = strlen( line );
		if ( writen( mfd, line, len ) < len )
		{
			close( mfd );
			return -1;
		}
	}

	/* Send end-of-data indicator */

	if ( writen( mfd, "\r\n.\r\n", 5 ) < 5 )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );
	
	if ( writen( mfd, "QUIT\r\n", 6 ) < 6 )
	{
		close( mfd );
		return -1;
	}

	while ( ( len = read_line( mfd, line, MAX_MAIL_LINE ) ) >= 4 &&
			! strncmp( line, "221", 3 ) && line[ 3 ] == '-' )
		/* empty */ ;

	close( mfd );
	return 0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static int open_mail_socket( const char *host )
{
	int sock_fd;
	struct sockaddr_in serv_addr;
	struct hostent *hp;


	/* Try to get a socket */

	if ( ( sock_fd = socket( AF_INET_x, SOCK_STREAM, 0 ) ) == -1 )
		return -1;

	memset( &serv_addr, 0, sizeof( serv_addr ) );
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( MAIL_PORT );

	if ( ( hp = gethostbyname( host ) ) == NULL ||
		 hp->h_addr_list == NULL )
		return -1;

	memcpy( &serv_addr.sin_addr, * ( struct in_addr ** ) hp->h_addr_list,
			sizeof( serv_addr.sin_addr ) );
		
	if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
				  sizeof( serv_addr ) ) == -1 )
		return -1;

	return sock_fd;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static ssize_t read_line( int fd, void *vptr, size_t max_len )
{
	ssize_t n, rc;
	char c, *ptr;


	ptr = CHAR_P vptr;
	for ( n = 1; n < ( ssize_t ) max_len; n++ )
	{
		if ( ( rc = do_read( fd, &c ) ) == 1 )
		{
			*ptr++ = c;
			if ( c == '\n' )
				break;
		}
		else if ( rc == 0 )
		{
			if ( n == 1 )
				return 0;
			else
				break;
		}
		else
			return -1;
	}

	*ptr = '\0';
	return n;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static ssize_t do_read( int fd, char *ptr )
{
	static int read_cnt;
	static char *read_ptr;
	static char read_buf[ MAX_MAIL_LINE ];


	if ( read_cnt <= 0 )
	{
	  again:
		if ( ( read_cnt = read( fd, read_buf, sizeof( read_buf ) ) ) < 0 )
		{
			if ( errno == EINTR )
				goto again;
			return -1;
		}
		else if ( read_cnt == 0 )
			return 0;

		read_ptr = read_buf;
	}

	read_cnt--;
	*ptr = *read_ptr++;
	return 1;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static ssize_t writen( int fd, const void *vptr, size_t n )
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;


	ptr = CHAR_P vptr;
	nleft = n;
	while ( nleft > 0 )
	{
		if ( ( nwritten = write( fd, ptr, nleft ) ) <= 0 )
		{
			if ( errno == EINTR )
				nwritten = 0;
			else
				return -1;
		}

		nleft -= nwritten;
		ptr += nwritten;
	}

	return n;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
