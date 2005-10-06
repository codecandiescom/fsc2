/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2.h"


#if defined USE_FSC2_MTA

#define QDCOUNT        0
#define ANCOUNT        1
#define NSCOUNT        2
#define ARCOUNT        3


#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>


/* On very old systems NS_MAXDNAME does not seem to be defined in
   <arpa/nameser.h> but instead MAXDNAME. Also the 'ns_class' and
   'ns_type' enumerations aren't declared, so they also need to be
   defined to the required values. */

#if ! defined NS_MAXDNAME
#define NS_MAXDNAME MAXDNAME
#endif

#if ! defined ns_c_in
#define ns_c_in C_IN
#endif

#if ! defined ns_t_mx
#define ns_t_mx T_MX
#endif

#if ! defined ns_t_name
#define ns_t_cname T_CNAME
#endif


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


/*--------------------------------------------------------------------*
 * Function for sending mails with the mail body taken from the file
 * pointed to by 'fp' from user 'from' with subject line 'subject' to
 * address 'to' and, if non-zero also to 'cc_to'.
 * There are two versions of the function, one that is used when the
 * 'USE_FSC2_MTA' variable isn't defined in the main makefile and
 * that relies on the availability of a working MTA being available
 * on the machine. The other version tries to do everything required
 * with as little external help as possible. The function is also
 * written in a way *not* to use dynamically allocated memory etc.
 * because it may be called in situations where we got a segmentation
 * fault due to memory problems and still want to be able to send out
 * mails without provoking another segmentation fault.
 *--------------------------------------------------------------------*/

#if defined MAIL_PROGRAM

int send_mail( const char *subject, const char *from UNUSED_ARG,
			   const char* cc_to, const char *to, FILE *fp )
{
	FILE *mail;
	char *cmd;
	char buf[ MAX_LINE_LENGTH ];
	size_t len;


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


	/* For IPv6 suport set the RES_USE_INET6 flag, so the gethostbyname()
	   always returns IPv6 addresses */

#if USE_IPv6
		_res.options |= RES_USE_INET6;
#endif

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


/*-----------------------------------------------------------*
 * Function for connecting to the machine receiving mail for
 * the domain and then sending the mail, see RFC 821 etc.
 *-----------------------------------------------------------*/

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
		/* Send an additional dot before lines starting with a dot (see
		   RFC 821, section 4.5.2, for why this needs to be done) */

		if ( line[ 0 ] == '.' && writen( mfd, ".", 1 ) < 1 )
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


/*------------------------------------------------------------------------*
 * Function for making a connection to a machine prepared to receive mail
 * for the domain passed to the function in 'remote'. The function also
 * needs to know about the local machine avoid creating a loop.
 *------------------------------------------------------------------------*/

static int open_mail_socket( const char *remote, const char *local )
{
	int sock_fd;
#ifdef USE_IPv6
	struct sockaddr_in6 serv_addr;
#else
	struct sockaddr_in serv_addr;
#endif
	struct hostent *hp;
	const char *host;
	struct servent *se;


	/* Get the number (in network byte order) of the SMTP port (this should
	   normally be 25) */

	if ( ( se = getservbyname( "smtp", "tcp" ) ) == NULL )
		return -1;

	/* Try to open a socket */

#ifdef USE_IPv6
	if ( ( sock_fd = socket( AF_INET6, SOCK_STREAM, 0 ) ) == -1 )
#else
	if ( ( sock_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
#endif
		return -1;

	/* If this is a local message (i.e. the domain to which the mail is to
	   be send is identical to the local host name) we try local delivery
	   first. Without doing so it would be impossible to send local mails
	   on machines without an internet connection. */

	if ( ! strcasecmp( remote, local ) )
	{
		if ( ( hp = gethostbyname( remote ) ) != NULL &&
			 hp->h_addr_list != NULL )
		{
			memset( &serv_addr, 0, sizeof( serv_addr ) );
#ifdef USE_IPv6
			serv_addr.sin6_family = AF_INET6;
			serv_addr.sin6_port = se->s_port;
			memcpy( &serv_addr.sin6_addr.s6_addr, *hp->h_addr_list,
					sizeof serv_addr.sin6_addr.s6_addr );
#else
			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = se->s_port;
			memcpy( &serv_addr.sin_addr.s_addr, *hp->h_addr_list,
					sizeof serv_addr.sin_addr.s_addr );
#endif
			if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
						  sizeof( serv_addr ) ) == 0 )
				return sock_fd;
		}
	}

	/* Otherwise we can't simply connect to the remote domain but must first
	   figure out which is the machine that takes care of mail for the domain
	   and then try to connect to this machine (or machines, there can be
	   several, so we repeat trying until we connected successfully or there
	   are no more machines prepared that would accept mail for the domain). */

	for ( host = get_mail_server( remote, local ); host != NULL;
		  host = get_mail_server( NULL, local ) )
	{
		if ( ( hp = gethostbyname( host ) ) == NULL ||
			 hp->h_addr_list == NULL )
			continue;

		memset( &serv_addr, 0, sizeof( serv_addr ) );
#ifdef USE_IPv6
		serv_addr.sin6_family = AF_INET6;
		serv_addr.sin6_port = se->s_port;
		memcpy( &serv_addr.sin6_addr.s6_addr, *hp->h_addr_list,
				sizeof serv_addr.sin6_addr.s6_addr );
#else
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = se->s_port;
		memcpy( &serv_addr.sin_addr.s_addr, *hp->h_addr_list,
				sizeof serv_addr.sin_addr.s_addr );
#endif

		if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
					  sizeof( serv_addr ) ) == 0 )
			return sock_fd;
	}

	close( sock_fd );
	return -1;
}


/*------------------------------------------------------------------------*
 * Function for trying to figure out which machine takes care of mail for
 * the domain passed to the function in 'remote'. To find out we have to
 * query a DNS server and interpret the reply according to RFC 1035 and
 * RFC 974 (and take into account that RFC 1123 tells that, in contrast
 * to what's written in RFC 974, we're not supposed to check for the WKS
 * records for the machine anymore but instead simply try to connect to
 * it and test if it works).
 * The function might be called several times in a row to get another
 * mail-receiving machine each time round if no connection could be made
 * to the machine that was returned the last time round. The first time
 * it must be called with the remote machine name as the first argument,
 * while on further invocations for the same remote host the 'remote'
 * argument must be NULL. The function returns NULL if there's no machine
 * left that would accept mail, otherwise a pointer to the name of the
 * next machine that should be tried.
 * The second argument must be the name of the machine we're trying to
 * send the mail from. It also must only be set each time a new mail
 * accepting machine is asked for the first time, in later calls it can
 * be a NULL pointer.
 * In case of errors the function also returns NULL, so an error can't be
 * distinguished from the case that there are no machines prepared to
 * accept mail for the remote machine.
 *------------------------------------------------------------------------*/

static const char *get_mail_server( const char *remote, const char *local )
{
    static char host[ NS_MAXDNAME ];
    static unsigned char buf[ DNS_MAX_ANSWER_LEN ];
    static unsigned char *ans_sec;
    static int len;
    static unsigned short sec_entries[ 4 ];
    static int rrs_left = 0;
    const char *phost;
    static int is_res_init = 0;


    /* If 'remote' isn't NULL this is a query for a new machine and we have
       to ask the DNS server and initialize some internal variables. Otherwise
       it's a query for the next machine prepared to accept mail for the
       domain that has been passed to the function already in a previous
       invocation. */

    if ( remote != NULL )
    {
        /* The first time round we need the name of the machine the mail is
           going to be send from */

        if ( local == NULL )
            return NULL;

        /* Initialize the resolver library if necessary */

        if ( ! is_res_init && res_init( ) != 0 )
            return NULL;

        is_res_init = 1;
        rrs_left = 0;
        strcpy( host, remote );

    reget_mx_rr:

        /* Ask the DNS server for the MX RRs for the remote machine, check
           the header and skip the question section of the reply. */

        if ( ( len = res_query( host, ns_c_in, ns_t_mx,
                                buf, sizeof buf ) ) == -1 ||
             ( ans_sec = analyze_dns_reply_header( buf, len, sec_entries,
                                                   host, ns_t_mx ) ) == NULL )
            return NULL;

        /* Check that the answer didn't contain a CNAME RR for the remote
           host. Otherwise repeat the query with the canonical name. */

        switch ( check_cname_rr( buf, len, ans_sec, host, sec_entries ) )
        {
            case -1 :
                return NULL;

            case 1 :
                goto reget_mx_rr;
        }

        /* If there aren't any MX RRs in the answer section all we can try
           is to talk with the host directly. */

        if ( sec_entries[ ANCOUNT ] == 0 )
            return host;

        /* We still have to check if the host itself is in the list and weed
           out all hosts that have the same or lower priority to avoid loops.
           If we end up with no hosts left we can't send mail. */

        if ( ( rrs_left = weed_out( buf, len, ans_sec,
                                    sec_entries[ ANCOUNT ], local ) ) <= 0 )
            return NULL;
    }

    /* If there aren't any MX RRs left return NULL */

    if ( rrs_left <= 0 )
        return NULL;

    /* Otherwise return the name of the next host with the highest priority */

    if ( ( phost = get_host( buf, len, sec_entries[ ANCOUNT ], ans_sec ) )
         == NULL )
    {
        rrs_left = 0;
        return NULL;
    }

    /* Ok, got a new mail exchanger, decrement the number of exchangers
       left and return a pointer to its name */

    rrs_left--;
    return phost;
}


/*------------------------------------------------------------------------*
 * Checks that the header of the reply from the DNS server didn't report
 * errors and extracts the numbers of entries in the different sections,
 * storing them in 'sec_entries'. Then it also reads the question section
 * records (if there are any). Unless there is an error (in which case
 * the function returns -1) '*buf' points to the first byte after the
 * question section of the reply, i.e. the start of the answers section,
 * on return.
 *------------------------------------------------------------------------*/

static unsigned char *analyze_dns_reply_header( unsigned char *buf, int len,
                                                unsigned short *sec_entries,
                                                const char *remote,
                                                unsigned short type )
{
    unsigned short rpq;
    static char host[ NS_MAXDNAME ];
    unsigned char *cur_pos = buf;
    int i;


    /* Skip the ID field of the reply */

    cur_pos += 2;

    /* Get the field with information about the success of the query */

    rpq = get_ushort( cur_pos );
    cur_pos += 2;

    /* The top-most bit must be set or this isn't a reply */

    if ( ! ( rpq & 0x8000 ) )
        return NULL;

    /* If bit 9 is set we only got a truncated reply because the buffer
       wasn't large enough - lets hope that this never happens and give
       up if it ever should... */

    if ( rpq & 0x0200 )
        return NULL;

    /* The lowest 4 bits must be unset or some kind of error happened */

    if ( rpq & 0x000F )
        return NULL;

    /* Ok, everything looks well, lets get the numbers of entries in the
       sections */

    for ( i = 0; i < 4; i++ )
    {
        sec_entries[ i ] = get_ushort( cur_pos );
        cur_pos += 2;
    }

    /* If there's no entry in the question section we're done */

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
           always a sequence of labels, consisting of a length byte,
           followed that number of bytes and a terminating zero byte. */

        while ( *cur_pos != '\0' )
            cur_pos += *cur_pos + 1;

        cur_pos++;

        /* Get the QTYPE field and check its type */

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

    return cur_pos;           /* return pointer to start of answer section */
}


/*------------------------------------------------------------------*
 * Function loops over all resource records looking for a CNAME RR.
 * If one is found the canonical name is copied into 'host' and the
 * function returns 1. If none is found returns 0 and -1 on errors.
 *------------------------------------------------------------------*/

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
        if ( ( for_host = get_name( buf, len, &ans_sec ) ) == NULL )
             return -1;

        /* If the RR isn't a CNAME entry or is not of class Internet or if
           it's for different host skip it and check the next one */

        if ( get_ushort( ans_sec ) != ns_t_cname ||
             get_ushort( ans_sec + 2 ) != ns_c_in ||
             strcasecmp( host, for_host ) )
        {
            ans_sec += 10 + get_ushort( ans_sec + 8 );
            continue;
        }

        /* Otherwise get the canonical name and copy it into 'host' */

        ans_sec += 12;
        if ( ( for_host = get_name( buf, len, &ans_sec ) ) == NULL )
             return -1;

        strcpy( host, for_host );
        return 1;
    }

    return 0;
}


/*--------------------------------------------------------------------*
 * Loops over all entries in the answer section, checking if the host
 * itself is listed as being prepared to accept mail, and if it does
 * sets the priority of all hosts with equal or lower priority to the
 * lowest possible priority. RRs with that low a priority will never
 * be used.
 *--------------------------------------------------------------------*/

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
       host itself is in the list. If it is get its priority value. */

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

    /* If the host wasn't in the list of answers no entries need to be
       removed and we can simply return */

    if ( i == num_ans )
        return num_ans;

    /* Otherwise we set the priority for all hosts with equal or lower
       priority than host itself to the lowest possible priority value
       of 0xFFFF (the priority is the lower the larger this number is).
       Hosts with that low a priority are never going to be used. */

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


/*------------------------------------------------------------------*
 * Loops over the list of records in the answer section to find the
 * machine with the highest priority. If one is found its name is
 * returned and its priority level is lowered to the minimum value
 * to keep it from getting used it again.
 *------------------------------------------------------------------*/

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

    /* Return NULL if there's no host left with a sufficiently high priority,
       otherwise return the name of the machine and mark it as used by
       lowering its priority value to the minimum. */

    if ( this_one == NULL )
        return NULL;

    *this_one++ = 0xFF;
    *this_one++ = 0xFF;     
    return get_name( buf, len, &this_one );
}


/*-------------------------------------------------------------------------*
 * Function expects that '*rr' is pointing to the start of the NAME field.
 * It returns a static buffer with the name of the machine, leaving '*rr'
 * pointing to the byte following the name in the RR. On errors NULL is
 * returned (and '*rr' remains unchanged).
 *-------------------------------------------------------------------------*/

static const char *get_name( unsigned char *buf, int len, unsigned char **rr )
{
    static char for_host[ NS_MAXDNAME ];


    if ( dn_expand( buf, buf + len, *rr, for_host, sizeof for_host ) == -1 )
        return NULL;

    while ( 1 )
    {
        /* Check if we got to the end of the name. This is the case when
           we either hit a NUL-character or a pointer (indicated by the
           two top-most bits being both set) */

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

        /* Otherwise jump to the length byte of the next label */

        *rr += **rr + 1;
    }

    return for_host;
}


/*----------------------------------------------*
 * Function for getting an unsigned short value
 * from the reply of the DNS server
 *----------------------------------------------*/

static unsigned short get_ushort( const unsigned char *p )
{
    return ( ( *p & 0xFF ) << 8 ) + ( * ( p + 1 ) & 0xFF );
}


#endif /* ! defined MAIL_PROGRAM */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
