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


#if defined USE_FSC2_MTA

#define MAIL_PORT      25
#define PROTOCOL_TCP   6
#define QDCOUNT        0
#define ANCOUNT        1
#define NSCOUNT        2
#define ARCOUNT        3


#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

/* Define the maximum length of data we're prepared to get on a DNS query
   for the MX RRs */

#define DNS_MAX_ANSWER_LEN  4096

static int do_send( const char *rec_host, const char *to,
					const char *from, const char *local_host,
					const char *subject, FILE *fp );
static int open_mail_socket( const char *remote, const char *local );
static const char *get_mail_server( const char *remote, const char *local );
static unsigned char *analyze_dns_reply_header( unsigned char *buf, int len,
												unsigned short *sec_entries,
												const char *remote,
												unsigned short type );
static int check_cname_rr( unsigned char *buf, int len, unsigned char *ans_sec,
						   char *host, unsigned short *sec_entries );
static int weed_out( unsigned char *buf, int len, unsigned char *ans_sec,
					 unsigned short num_ans, const char *host );
static const char *get_host( unsigned char *buf, int len,
							 unsigned short num_ans, unsigned char *ans_sec );
static const char *get_name( unsigned char *buf, int len, unsigned char **rr );
static unsigned short get_ushort( const unsigned char *p );

#endif


/*--------------------------------------------------------------------*/
/* Function for sending mails with the mail body taken from the file  */
/* pointed to by 'fp' from user 'from' with subject line 'subject' to */
/* address 'to' and, if non-zero also to 'cc_to'.                     */
/* There are two versions of the function, one that is used when the  */
/* 'USE_FSC2_MTA' variable isn't defined in the main makefile and     */
/* that relies on the availability of a working MTA being available   */
/* on the machine. The other version tries to do everything required  */
/* with as little external help as possible. The function is also     */
/* written in a way *not* to use malloc() etc. because it may be      */
/* called in situations where we got a segmentation fault due to      */
/* memory problems and still want to be able to send out a mail.      */
/*--------------------------------------------------------------------*/

#if defined MAIL_PROGRAM

int send_mail( const char *subject, const char *from, const char* cc_to,
				const char *to, FILE *fp )
{
	FILE *mail;
	char *cmd;
	char buf[ MAX_LINE_LENGTH ];
	size_t len;


	UNUSED_ARGUMENT( from );

	CLOBBER_PROTECT( to );
	CLOBBER_PROTECT( cc_to );

	while ( 1 )
	{
		TRY
			{
				cmd = get_string( "%s -s '%s' %s", MAIL_PROGRAM, subject, to );
				TRY_SUCCESS;
			}
		OTHERWISE
			return -1;

		if ( ( mail = popen( cmd, "w" ) ) == NULL )
			return -1;

		while ( ( len = fread( buf, 1, MAX_LINE_LENGTH, fp ) ) > 0 )
			if ( fwrite( buf, 1, len, mail ) != len )
			{
				pclose( mail );
				T_free( cmd );
				return -1;
			}

		pclose( mail );
		T_free( cmd );
		
		if ( cc_to == NULL )
			return 0;

		rewind( fp );
		to = cc_to;
		cc_to = NULL;
	}

	return 0;
}

#else

int send_mail( const char *subject, const char *from, const char* cc_to,
				const char *to, FILE *fp )
{
	struct hostent *hp;
	char *rec_host;
	char local_host[ NS_MAXDNAME ];


	if ( gethostname( local_host, sizeof local_host ) != 0 )
		return -1;

	if ( ( hp = gethostbyname( local_host ) ) == NULL || hp->h_name == NULL )
		return -1;

	strncpy( local_host, hp->h_name, NS_MAXDNAME );

	if ( ( rec_host = strchr( to, '@' ) ) == NULL )
		rec_host = local_host;
	else
		rec_host++;

	if ( do_send( rec_host, to, from, local_host, subject, fp ) == -1 )
		return -1;

	if ( cc_to != NULL )
	{
		rewind( fp );
		do_send( local_host, cc_to, from, local_host, subject, fp );
	}

	return 0;
}


/*-----------------------------------------------------------*/
/* Function for connecting to the mail receiving machine and */
/* then sending the mail, see RFC 821 etc.                   */
/*-----------------------------------------------------------*/

static int do_send( const char *rec_host, const char *to,
					const char *from, const char *local_host,
					const char *subject, FILE *fp )
{
	int mfd;
	char line[ MAX_LINE_LENGTH ];
	ssize_t len;
	

	if ( ( mfd = open_mail_socket( rec_host, local_host ) ) < 0 )
		return -1;

	/* Now we have a connection to the other side, get the intro text -
	   the other side must send at least "220\n" */

	do
	{
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
			 strncmp( line, "220", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line telling the other side about ourself */

	if ( ( len = snprintf( line, MAX_LINE_LENGTH, "HELO %s\r\n", local_host ) )
		 > MAX_LINE_LENGTH ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, it must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line about the sender of the mail */

	if ( ( len = snprintf( line, MAX_LINE_LENGTH, "MAIL FROM:<%s@%s>\r\n",
						   from, local_host ) )
		 > MAX_LINE_LENGTH ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
			 strncmp( line, "250", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Send the line telling the other side about the receiver of the mail */

	if ( ( len = snprintf( line, MAX_LINE_LENGTH, "RCPT TO:<%s>\r\n", to ) )
		 > MAX_LINE_LENGTH ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Read reply, must start with "250" to indicate success */
	
	do
	{
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
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
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
			 strncmp( line, "354", 3 ) )
		{
			close( mfd );
			return -1;
		}
	} while ( line[ 3 ] == '-' );

	/* Now send the message, starting with the header */

	if ( ( len = snprintf( line, MAX_LINE_LENGTH, "Subject: %s\n", subject ) )
		 > MAX_LINE_LENGTH  ||
		 writen( mfd, line, len ) < len ||
		 ( len = snprintf( line, MAX_LINE_LENGTH, "From: fsc2 <fsc2@%s>\n",
						   local_host ) ) > MAX_LINE_LENGTH  ||
		 writen( mfd, line, len ) < len ||
		 ( len = snprintf( line, MAX_LINE_LENGTH, "To: %s\n", to ) )
		 > MAX_LINE_LENGTH ||
		 writen( mfd, line, len ) < len )
	{
		close( mfd );
		return -1;
	}

	/* Copy the whole input file to the socket */

	while ( fgets( line, MAX_LINE_LENGTH, fp ) != NULL )
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
		if ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) <= 4 ||
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

	while ( ( len = read_line( mfd, line, MAX_LINE_LENGTH ) ) >= 4 &&
			! strncmp( line, "221", 3 ) && line[ 3 ] == '-' )
		/* empty */ ;

	close( mfd );
	return 0;
}


/*--------------------------------------------------------------------------*/
/* Function for making a connection to the machine prepared to receive mail */
/* for the domain passed to the function in 'remote'. The function also     */
/* needs to know about the local machine avoid creating a loop.             */
/*--------------------------------------------------------------------------*/

static int open_mail_socket( const char *remote, const char *local )
{
	int sock_fd;
	struct sockaddr_in serv_addr;
	struct hostent *hp;
	const char *host;


	/* Try to open a socket */

	if ( ( sock_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
		return -1;

	/* We can't simply connect to the remote host but must first figure
	   out which is the machine that takes care of mail for the domain
	   and then send the mail to this machine (there can be sseveral). */

	while ( ( host = get_mail_server( remote, local ) ) != NULL )
	{
		memset( &serv_addr, 0, sizeof( serv_addr ) );
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons( MAIL_PORT );

		if ( ( hp = gethostbyname( host ) ) == NULL ||
			 hp->h_addr_list == NULL )
			continue;

		memcpy( &serv_addr.sin_addr, * ( struct in_addr ** ) hp->h_addr_list,
				sizeof( serv_addr.sin_addr ) );
		
		if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
					  sizeof( serv_addr ) ) == 0 )
		{
			get_mail_server( NULL, NULL );
			return sock_fd;
		}
	}

	close( sock_fd );
	return -1;
}


/*------------------------------------------------------------------------*/
/* Function for trying to figure out which machine takes care of mail for */
/* the domain passed to the function in 'remote'. To find out we have to  */
/* query a DNS server and interpret the reply according to RFC 1035 and   */
/* RFC 974 (and take into account that RFC 1123 tells that, in contrast   */
/* to what's written in RFC 974, we're not supposed top check for the WKS */
/* record for the machine anymore but instead simply try to connect to it */
/* and test what happens.                                                 */
/* The function might be called several times in a row to get another     */
/* mail-receiving machine if no connection could be made to the machine   */
/* it was told about. If it returns NULL there's no machine that would    */
/* accept mail and the calling function is supposed to stop calling it.   */
/* If the calling function succeeded in connecting to the machines it is  */
/* supposed to call this function for last time with a NULL pointer in    */   
/* 'remote' so that the function can set itself up for new queries.       */
/*------------------------------------------------------------------------*/

static const char *get_mail_server( const char *remote, const char *local )
{
	static bool is_init = UNSET;
	static char host[ NS_MAXDNAME ];
	static unsigned char buf[ DNS_MAX_ANSWER_LEN ];
	static unsigned char *ans_sec;
	static int len;
	static unsigned short sec_entries[ 4 ];
	static int rrs_left;
	const char *phost;


	/* A NULL pointer for remote indicates that the calling function has
	   successfully connected to one of the machines we were telling it
	   about and will not call us again for more mail-exchangers. */

	if ( remote == NULL )
	{
		rrs_left = 0;
		is_init = UNSET;
		return NULL;
	}

	/* If the DNS server hasn't been queried for the MX records yet do it
	   now and check the headers and the qestion section of the reply.
	   If necessary also find the canoncial name of the host we're looking
	   for. */

	if ( ! is_init )
	{
		strcpy( host, remote );

	reget_mx_rr:

		if ( res_init( ) != 0 )
			return NULL;

		/* Ask the DNS server for the MX RRs for the remote machine */

		if ( ( len = res_query( host, ns_c_in, ns_t_mx,
								buf, sizeof buf ) ) == -1 )
			return NULL;

		/* Check the header of the reply, the return value is (normally) a
		   pointer to the start of the answer section. */

		if ( ( ans_sec = analyze_dns_reply_header( buf, len, sec_entries,
												   host, ns_t_mx ) ) == NULL )
			return NULL;

		/* Check that the answer didn't contain a CNAME RR for our host.
		   Otherwise repeat the query with the canonical name of the host. */

		switch ( check_cname_rr( buf, len, ans_sec, host, sec_entries ) )
		{
			case -1 :
				return NULL;

			case 1 :
				goto reget_mx_rr;
		}

		is_init = SET;
		rrs_left = sec_entries[ ANCOUNT ];

		/* If there aren't any MX RRs in the answer section all we can try
		   is to talk with the host directly. */

		if ( rrs_left == 0 )
			return host;

		/* We still have to check if the host itseld is in the list and
		   weed out all hosts that have the same or lower priority than
		   it to avoid mail loops. If we end up with no hosts left we
		   can't send mail. */

		if ( ( rrs_left = weed_out( buf, len, ans_sec, rrs_left, local ) )
			 <= 0 )
		{
			is_init = UNSET;
			return NULL;
		}
	}

	/* If there aren't any MX RRs left do a reset */

	if ( rrs_left == 0 )
	{
		is_init = UNSET;
		return NULL;
	}

	/* Otherwise return the name of the next host with the highest
	   priority */

	if ( ( phost = get_host( buf, len, sec_entries[ ANCOUNT ], ans_sec ) )
		 == NULL )
	{
		rrs_left = 0;
		is_init = UNSET;
		return NULL;
	}

	rrs_left--;
	return phost;
}


/*------------------------------------------------------------------------*/
/* Checks that the header of the reply from the DNS server doesn't report */
/* errors and extracts the number of entries in the different sections,   */
/* storing them in 'sec_entries'. Then it also reads the question section */
/* records (if there are any). Unless there is an error (in which case    */
/* the function returns -1) '*buf' points to the first byte after the     */
/* question section of the reply,, i.e. the start of the answers section, */
/* on return.                                                             */
/*------------------------------------------------------------------------*/

static unsigned char *analyze_dns_reply_header( unsigned char *buf, int len,
												unsigned short *sec_entries,
												const char *remote,
												unsigned short type )
{
	unsigned short rpq;
	static char host[ NS_MAXDNAME ];
	unsigned char *cur_pos = buf;
	int i;


	/* Skip the ID field */

	cur_pos += 2;

	/* Get the field with information about the result of the query */

	rpq = get_ushort( cur_pos );
	cur_pos += 2;

	/* The top-most bit must be set or this isn't a reply */

	if ( ! ( rpq & 0x8000 ) )
		return NULL;

	/* If bit 9 is set we only got a truncated reply because the buffer
	   wasn't large enough - assume that this never happens and give up if
	   it ever should happen... */

	if ( rpq & 0x0200 )
		return NULL;

	/* The lowest 4 bits most be unset or some kind of error happened */

	if ( rpq & 0x000F )
		return NULL;

	/* Ok, everything looks well, lets get the numbers of entries in the
	   sections */

	for ( i = 0; i < 4; i++ )
	{
		sec_entries[ i ] = get_ushort( cur_pos );
		cur_pos += 2;
	}

	/* If there's no entry in the qestion section we're done */

	if ( sec_entries[ QDCOUNT ] == 0 )
		return cur_pos;

	/* Otherwise read the question section entries and check them */

	for ( i = 0; i < sec_entries[ QDCOUNT ]; i++ )
	{
		/* Question must be for the host we were asking about */

		if ( dn_expand( buf, buf + len, cur_pos, host, sizeof host ) == -1 )
			return NULL;

		if ( strcasecmp( host, remote ) )
			return NULL;

		/* Find the end of the host name, in the question section it's
		   always a sequence of labels, consisting of a length byte
		   followed that number of bytes. It's terminated by a zero byte. */

		while ( *cur_pos != '\0' )
			cur_pos += *cur_pos + 1;

		cur_pos++;

		/* Get the QTYPE field and check it */

		rpq = get_ushort( cur_pos );
		cur_pos += 2;

		if ( type != rpq )
			return NULL;

		/* Check the QCLASS field, it must be ns_c_in */

		rpq = get_ushort( cur_pos );
		cur_pos += 2;

		if ( rpq != ns_c_in )
			return NULL;
	}

	return cur_pos;
}


/*------------------------------------------------------------------*/
/* Function loops over all resource records looking for a CNAME RR. */
/* If one is found the canonical name is copied into 'host' and the */
/* function returns 1. If none is found 0 is returned and -1 on     */
/* errors.                                                          */
/*------------------------------------------------------------------*/

static int check_cname_rr( unsigned char *buf, int len, unsigned char *ans_sec,
						   char *host, unsigned short *sec_entries )
{
	unsigned int num_recs =   sec_entries[ ANCOUNT ]
							+ sec_entries[ NSCOUNT ]
							+ sec_entries[ ARCOUNT ];
	unsigned int i;
	const char *for_host;


	for ( i = 0; i < num_recs; i++ )
	{
		if ( ( for_host = get_name( buf, len, &ans_sec ) )== NULL )
			 return -1;

		/* If the RR isn't a CNAME entry or is not of class Internet or if
		   it's for differenrt host skip the RR and ceck the next one */

		if ( get_ushort( ans_sec ) != ns_t_cname ||
			 get_ushort( ans_sec + 2 ) != ns_c_in ||
			 strcasecmp( host, for_host ) )
		{
			ans_sec += 10 + get_ushort( ans_sec + 8 );
			continue;
		}

		/* Otherwise get the canonical name and copy it into 'host' */

		ans_sec += 12;
		if ( ( for_host = get_name( buf, len, &ans_sec ) )== NULL )
			 return -1;

		strcpy( host, for_host );
		return 1;
	}

	return 0;
}


/*--------------------------------------------------------------------*/
/* Loops over all entries in the answer section, checking if the host */
/* itself is listed as being prepared to accept mail, and if it does  */
/* sets the priority of all hosts with equal or lower priority to     */
/* lowest possible priority. All RRs with that low a priority will    */
/* never be used.                                                     */
/*--------------------------------------------------------------------*/

static int weed_out( unsigned char *buf, int len, unsigned char *ans_sec,
					 unsigned short num_ans, const char *local )
{
	unsigned short i;
	int rrs_left = num_ans;
	unsigned char *cur_pos = ans_sec;
	const char *host;
	unsigned char rpq;
	unsigned short prior_level = 0xFFFF;
	unsigned short *non_local_prior;


	/* Loop over all answers for the first time to check if the local
	   host itself is in the list. If it is get it s priority. */

	for ( i = 0; i < num_ans; i++ )
	{
		if ( get_name( buf, len, &cur_pos ) == NULL ||
			 get_ushort( cur_pos ) != ns_t_mx )
			return -1;

		rpq = get_ushort( cur_pos + 10 );
		cur_pos += 12;

		if ( ( host = get_name( buf, len, &cur_pos ) ) == NULL )
			return -1;

		if ( ! strcasecmp( local, host ) )
		{
			prior_level = rpq;
			break;
		}
	}

	/* If the host wasn't in the list of answers nothing needs to be removed
	   and we simply return */

	if ( i == num_ans )
		return num_ans;

	/* Otherwise we set the priority for all hosts with equal or lower
	   priority than host itself to the lowest possible priority value
	   of 0xFFFF (the priority is the lower the larger this number is).
	   In the following hosts with that low a priority are never going
	   to be used. */

	for ( i = 0; i < num_ans; i++ )
	{
		if ( get_name( buf, len, &cur_pos ) == NULL )
			return -1;
		
		non_local_prior = ( unsigned short * ) ( cur_pos + 10 );
		rpq = get_ushort( cur_pos + 10 );

		cur_pos += 12;

		if ( ( host = get_name( buf, len, &cur_pos ) ) == NULL )
			return -1;

		if ( rpq > prior_level ||
			 ( rpq == prior_level && strcmp( host, local ) ) )
		{
			*non_local_prior = 0xFFFF;
			rrs_left--;
		}

	}

	return rrs_left;
}


/*------------------------------------------------------------------*/
/* Loops over the list of records in the answer section to find the */
/* machine with the highest priority. If one is found its name is   */
/* returned and its priority level is lowered to the minimum value  */
/* in order to avoid using it again.                                */
/*------------------------------------------------------------------*/

static const char *get_host( unsigned char *buf, int len,
							 unsigned short num_ans, unsigned char *ans_sec )
{
	unsigned short i;
	unsigned short prior = 0xFFFF;
	unsigned short rpq;
	unsigned char *cur_pos;
	unsigned char *this_one = NULL;


	for ( cur_pos = ans_sec, i = 0; i < num_ans; i++ )
	{
		if ( get_name( buf, len, &cur_pos ) == NULL )
			return NULL;

		cur_pos += 10;
		rpq = get_ushort( cur_pos );
		if ( rpq < prior )
		{
			prior = rpq;
			this_one = cur_pos;
		}

		cur_pos += get_ushort( cur_pos - 2 );
	}

	if ( this_one == NULL )
		return NULL;
	else
	{
		*this_one++ = 0xFF;
		*this_one++ = 0xFF;		
		return get_name( buf, len, &this_one );
	}
}


/*------------------------------------------------------------------------*/
/* Function expects that '*rr' is pointing to the start of the NAME field */
/* of a resource record. It returns a static buffer with the name of the  */
/* machine addressed by the record, leaving '*rr' pointing to the byte    */
/* following the name in the RR. On errors NULL is returned (and '*rr'    */
/* remains unchanged).                                                    */
/*------------------------------------------------------------------------*/

static const char *get_name( unsigned char *buf, int len, unsigned char **rr )
{
	static char for_host[ NS_MAXDNAME ];


	if ( dn_expand( buf, buf + len, *rr, for_host, sizeof for_host ) == -1 )
		return NULL;

	while ( 1 )
	{
		/* Check if we got to the end of the name, this is the case when
		   we either hit a NUL-character or a pointer (than the top-most
		   two bits are both set) */

		if ( **rr == '\0' )
		{
			(*rr )++;
			break;
		}

		if ( ( **rr & 0xC0 ) == 0xC0 )
		{
			*rr += 2;
			break;
		}

		/* Otherwise go to the length byte before the next label */

		*rr += **rr + 1;
	}

	return for_host;
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static unsigned short get_ushort( const unsigned char *p )
{
	return ( ( *p & 0xFF ) << 8 ) + ( * ( p + 1 ) & 0xFF );
}


#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
