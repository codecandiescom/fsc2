/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


/*------------------------------------------------------------------------*
 * Contains all declarations possibly needed by routines using the GPIB
 * functions. Each of the functions returns either SUCCESS or FAILURE, in
 * the latter case in 'gpib_error_msg' a short text is stored explaining
 * the cause of the error.
 *------------------------------------------------------------------------*/


#if ! defined GPIB_IF_NONE_HEADER
#define GPIB_IF_NONE_HEADER


#define GPIB_NONE


/* Timeout values */

#define TNONE		0     /* Infinite timeout (disabled)   */
#define T10us		1     /* Timeout of 10 usec		       */
#define T30us		2     /* Timeout of 30 usec		       */
#define T100us		3     /* Timeout of 100 usec		   */
#define T300us		4     /* Timeout of 300 usec		   */
#define T1ms		5     /* Timeout of 1 msec		       */
#define T3ms		6     /* Timeout of 3 msec		       */
#define T10ms		7     /* Timeout of 10 msec		       */
#define T30ms		8     /* Timeout of 30 msec		       */
#define T100ms		9     /* Timeout of 100 msec		   */
#define T300ms	   10     /* Timeout of 300 msec		   */
#define T1s	       11     /* Timeout of 1 sec		       */
#define T3s	       12     /* Timeout of 3 sec		       */
#define T10s	   13     /* Timeout of 10 sec		       */
#define T30s	   14     /* Timeout of 30 sec		       */
#define T100s	   15     /* Timeout of 100 sec		       */
#define T300s	   16     /* Timeout of 300 sec		       */
#define T1000s	   17     /* Timeout of 1000 sec (maximum) */


/* End-of-string (EOS) modes for use with gpib_eos() */

#define GPIB_REOS 0x01    /* Terminate reads on EOS	    */
#define GPIB_XEOS 0x02    /* Set EOI with EOS on writes */
#define GPIB_BIN  0x04    /* Do 8-bit compare on EOS    */
#define GPIB_EOT  0x08    /* Send END with last byte    */


#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */


int gpib_init( const char * /* log_file_name */,
			   int          /* log_level     */ );
int gpib_shutdown( void );
int gpib_init_device( const char * /* device_name */,
					  int *        /* dev         */ );
int gpib_timeout( int /* device */,
				  int /* period */ );
int gpib_clear_device( int /* device */ );
int gpib_local( int /* device */ );
int gpib_local_lockout( int /* device */ );
int gpib_trigger( int /* device */ );
int gpib_wait( int   /* device */,
			   int   /* mask   */,
			   int * /* status */ );
int gpib_write( int          /* device */,
				const char * /* buffer */,
				long         /* length */ );
int gpib_read( int    /* device */,
			   char * /* buffer */,
			   long * /* length */ );
int gpib_serial_poll( int             /* device */,
					  unsigned char * /* stb    */ );
void gpib_log_message( const char * /* fmt */,
					   ... );


extern char gpib_error_msg[ 1024 ]; /* global for GPIB error messages */


#define SUCCESS   0
#define FAILURE  -1


/*----------------------------------------------------------*
 * definition of log levels allowed in calls of gpib_init()
 *----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */


/*-------------------------------*
 * Definitions of utility macros
 *-------------------------------*/

#define GPIB_IS_TIMEOUT     0


#endif /* ! GPIB_IF_NONE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
