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

/*------------------------------------------------------------------------*/
/* Contains all declarations possibly needed by routines using the GPIB   */
/* functions. Each of the functions returns either SUCCESS or FAILURE, in */
/* the latter case in 'gpib_error_msg' a short text is stored explaining  */
/* the cause of the error.                                                */
/*------------------------------------------------------------------------*/


#define GPIB_NONE


/* gpib_status bits (returned by all functions) */

#define GPIB_ERR    ( 1 << 15 )	  /* Function call terminated on error	     */
#define GPIB_TIMO   ( 1 << 14 )	  /* Time limit on I/O or wait exceeded	     */
#define GPIB_END    ( 1 << 13 )	  /* EOI terminated the ibrd call	     */
#define GPIB_SRQI   ( 1 << 12 )	  /* SRQ is asserted			     */
#define GPIB_RQS    ( 1 << 11 )	  /* Device requesting service		     */
#define GPIB_CMPL   ( 1 <<  8 )	  /* I/O is complete (should always be set)  */
#define GPIB_CIC    ( 1 <<  5 )	  /* GPIB interface is Controller-in-Charge  */
#define GPIB_ATN    ( 1 <<  4 )	  /* Attention is asserted		     */
#define GPIB_TACS   ( 1 <<  3 )	  /* GPIB interface is addressed as Talker   */
#define GPIB_LACS   ( 1 <<  2 )	  /* GPIB interface is addressed as Listener */
#define GPIB_DTAS   ( 1 <<  1 )	  /* Device trigger state		     */
#define GPIB_DCAS   ( 1 <<  0 )	  /* Device clear state			     */

/* GPIB_ERR error codes */

#define GPIB_EDVR	0     /* system error				     */
#define GPIB_ECIC	1     /* not CIC				     */
#define GPIB_ENOL	2     /* no listeners				     */
#define GPIB_EADR	3     /* CIC and not addressed before I/O	     */
#define GPIB_EARG	4     /* bad argument to function call		     */
#define GPIB_ESAC	5     /* not SAC				     */
#define GPIB_EABO	6     /* I/O operation was aborted		     */
#define GPIB_ENEB	7     /* non-existent board (GPIB interface offline) */
#define GPIB_EDMA	8     /* DMA hardware error detected		     */
#define GPIB_EBTO	9     /* DMA hardware uP bus timeout		     */
#define GPIB_EOIP   10     /* new I/O attempted with old I/O in progress  */
#define GPIB_ECAP   11     /* no capability for intended opeation	     */
#define GPIB_EFSO   12     /* file system operation error		     */
#define GPIB_EOWN   13     /* shareable board exclusively owned	     */
#define GPIB_EBUS   14     /* bus error				     */
#define GPIB_ESTB   15     /* lost serial poll bytes			     */
#define GPIB_ESRQ   16     /* SRQ stuck on				     */
#define GPIB_ECFG   17     /* Config file not found			     */
#define GPIB_EPAR   18     /* Parse Error in Config			     */
#define GPIB_ETAB   19     /* Table Overflow				     */
#define GPIB_ENSD   20     /* Device not found			     */
#define GPIB_ENWE   21     /* Network Error				     */
#define GPIB_ENTF   22     /* Nethandle-table full			     */
#define GPIB_EMEM   23     /* Memory allocation Error		     */
#define GPIB_ENMS   24     /* No master device			     */


/* Timeout values */

#define TNONE		0     /* Infinite timeout (disabled)	 */
#define T10us		1     /* Timeout of 10 usec		 */
#define T30us		2     /* Timeout of 30 usec		 */
#define T100us		3     /* Timeout of 100 usec		 */
#define T300us		4     /* Timeout of 300 usec		 */
#define T1ms		5     /* Timeout of 1 msec		 */
#define T3ms		6     /* Timeout of 3 msec		 */
#define T10ms		7     /* Timeout of 10 msec		 */
#define T30ms		8     /* Timeout of 30 msec		 */
#define T100ms		9     /* Timeout of 100 msec		 */
#define T300ms	   10     /* Timeout of 300 msec		 */
#define T1s	       11     /* Timeout of 1 sec		 */
#define T3s	       12     /* Timeout of 3 sec		 */
#define T10s	   13     /* Timeout of 10 sec		 */
#define T30s	   14     /* Timeout of 30 sec		 */
#define T100s	   15     /* Timeout of 100 sec		 */
#define T300s	   16     /* Timeout of 300 sec		 */
#define T1000s	   17     /* Timeout of 1000 sec (maximum)	 */


/* End-of-string (EOS) modes for use with gpib_eos() */

#define GPIB_REOS 0x01    /* Terminate reads on EOS	    */
#define GPIB_XEOS 0x02    /* Set EOI with EOS on writes */
#define GPIB_BIN  0x04    /* Do 8-bit compare on EOS    */
#define GPIB_EOT  0x08    /* Send END with last byte    */


/* Options to be used in gpib_ask() */

#define GPIB_ASK_SAD	      1
#define GPIB_ASK_PAD	      2
#define GPIB_ASK_EOS	      3
#define GPIB_ASK_XEOS	      4
#define GPIB_ASK_REOS	      5
#define GPIB_ASK_BIN	      6
#define GPIB_ASK_EOT	      7
#define GPIB_ASK_TIMO	      8
#define GPIB_ASK_IS_INIT      9
#define GPIB_ASK_IS_MASTER   10


/* Defines of bits in the value returned by gpib_lines() */

#define ValidDAV   ( 1 <<  0 )
#define ValidNDAC  ( 1 <<  1 )
#define ValidNRFD  ( 1 <<  2 )
#define ValidIFC   ( 1 <<  3 )
#define ValidREN   ( 1 <<  4 )
#define ValidSRQ   ( 1 <<  5 )
#define ValidATN   ( 1 <<  6 )
#define ValidEOI   ( 1 <<  7 )

#define BusDAV	   ( 1 <<  8 )
#define BusNDAC	   ( 1 <<  9 )
#define BusNRFD	   ( 1 << 10 )
#define BusIFC	   ( 1 << 11 )
#define BusREN	   ( 1 << 12 )
#define BusSRQ	   ( 1 << 13 )
#define BusATN	   ( 1 << 14 )
#define BusEOI	   ( 1 << 15 )


#if defined ( __GPIB_IF__ )
	#define GPIB_VARIABLE
#else
	#define GPIB_VARIABLE extern
#endif


#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */


GPIB_VARIABLE int gpib_init( const char *log_file_name, int log_level );
GPIB_VARIABLE int gpib_shutdown( void );
GPIB_VARIABLE int gpib_init_device( const char *device_name, int *dev );
GPIB_VARIABLE int gpib_timeout( int device, int period );
GPIB_VARIABLE int gpib_clear_device( int device );
GPIB_VARIABLE int gpib_local( int device );
GPIB_VARIABLE int gpib_trigger( int device );
GPIB_VARIABLE int gpib_wait( int device, int mask, int *status );
GPIB_VARIABLE int gpib_write( int device, const char *buffer, long length );
GPIB_VARIABLE int gpib_read( int device, char *buffer, long *length );
GPIB_VARIABLE int gpib_serial_poll( int device, unsigned char *stb );
GPIB_VARIABLE void gpib_log_message( const char *fmt, ... );


GPIB_VARIABLE char gpib_error_msg[ 1024 ]; /* global for GPIB error messages */


#define SUCCESS   0
#define FAILURE  -1


/*----------------------------------------------------------*/
/* definition of log levels allowed in calls of gpib_init() */
/*----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
