/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#if ! defined GPIBD_H
#define GPIBD_H

#define GPIBD_SOCK_FILE  P_tmpdir "/gpibd.uds"

#define GPIB_ERROR_BUFFER_LENGTH 256

#define GPIB_INIT           0
#define GPIB_SHUTDOWN       1
#define GPIB_INIT_DEVICE    2
#define GPIB_TIMEOUT        3
#define GPIB_CLEAR_DEVICE   4
#define GPIB_LOCAL          5
#define GPIB_LOCAL_LOCKOUT  6
#define GPIB_TRIGGER        7
#define GPIB_WAIT           8
#define GPIB_WRITE          9
#define GPIB_READ          10
#define GPIB_SERIAL_POLL   11
#define GPIB_LAST_ERROR    12


#define ACK        '\x06'
#define NAK        '\x15'
#define STR_ACK    "\x06"
#define STR_NAK    "\x15"


#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif


#endif /* ! defined GPIBD_H */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
