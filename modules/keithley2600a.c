/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#include "keithley2600a.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



Keithley2600A_T keithley2600a;


int
keithley2600a_exp_hook( void )
{
#if 0
    char reply[ 100 ];
    size_t length;

	if ( vxi11_open( DEVICE_NAME, NETWORK_ADDRESS, VXI11_NAME,
                     UNSET, 100000 ) == FAILURE )
        return FAIL;

	vxi11_set_timeout( READ, READ_TIMEOUT );
	vxi11_set_timeout( WRITE, WRITE_TIMEOUT );

	vxi11_device_clear( );

	vxi11_lock_out( SET );

    length = sizeof reply - 1;
    keithley2600a_talk( "print(smua.sense)", reply, &length );
    reply[ length ]= '\0';

    fprintf( stderr, "Reply: %s\n", reply );
#endif

    return OK;
}
   

/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
