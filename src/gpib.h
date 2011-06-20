/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if ! defined GPIB_H
#define GPIB_H


#define SUCCESS   0
#define FAILURE  -1


/* Timeout values */

#define TNONE           0     /* Infinite timeout (disabled)     */
#define T10us           1     /* Timeout of 10 usec              */
#define T30us           2     /* Timeout of 30 usec              */
#define T100us          3     /* Timeout of 100 usec             */
#define T300us          4     /* Timeout of 300 usec             */
#define T1ms            5     /* Timeout of 1 msec               */
#define T3ms            6     /* Timeout of 3 msec               */
#define T10ms           7     /* Timeout of 10 msec              */
#define T30ms           8     /* Timeout of 30 msec              */
#define T100ms          9     /* Timeout of 100 msec             */
#define T300ms         10     /* Timeout of 300 msec             */
#define T1s            11     /* Timeout of 1 sec                */
#define T3s            12     /* Timeout of 3 sec                */
#define T10s           13     /* Timeout of 10 sec               */
#define T30s           14     /* Timeout of 30 sec               */
#define T100s          15     /* Timeout of 100 sec              */
#define T300s          16     /* Timeout of 300 sec              */
#define T1000s         17     /* Timeout of 1000 sec (maximum)   */


int gpib_init( void );
int gpib_shutdown( void );
int gpib_init_device( const char * /* name */,
					  int        * /* dev  */ );
int gpib_timeout( int /* dev     */,
				  int /* timeout */ );
int gpib_clear_device( int /* dev */ );
int gpib_local( int /* dev */ );
int gpib_local_lockout( int /* dev */ );
int gpib_trigger( int /* dev */ );
int gpib_wait( int   /* dev    */,
			   int   /* mask   */,
			   int * /* status */ );
int gpib_write( int          /* dev    */,
				const char * /* buffer */,
				long         /* length */ );
int gpib_read( int    /* dev    */,
			   char * /* buffer */,
			   long * /* length */);
int gpib_serial_poll( int             /* dev */,
					  unsigned char * /* stb */ );
const char * gpib_last_error( void );

#endif  /* ! defined GPIB_H */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
