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


#include "pci_mio_16e_1.h"


/*---------------------------------------------------------------*
 * Converts a channel number as we get it passed from the parser
 * into a real channel number.
 *---------------------------------------------------------------*/

int
pci_mio_16e_1_channel_number( long         ch,
                              const char * snippet )
{
    switch ( ch )
    {
        case CHANNEL_CH0 :
            return 0;

        case CHANNEL_CH1 :
            return 1;

        case CHANNEL_CH2 :
            return 2;

        case CHANNEL_CH3 :
            return 3;

        case CHANNEL_CH4 :
            return 4;

        case CHANNEL_CH5 :
            return 5;

        case CHANNEL_CH6 :
            return 6;

        case CHANNEL_CH7 :
            return 7;

        case CHANNEL_CH8 :
            return 8;

        case CHANNEL_CH9 :
            return 9;

        case CHANNEL_CH10 :
            return 10;

        case CHANNEL_CH11 :
            return 11;

        case CHANNEL_CH12 :
            return 12;

        case CHANNEL_CH13 :
            return 13;

        case CHANNEL_CH14 :
            return 14;

        case CHANNEL_CH15 :
            return 15;
    }

    print( FATAL, "Invalid %s number %ld.\n", snippet, ch );
    THROW( EXCEPTION );

    return -1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
pci_mio_16e_1_check_time( double       t,
                          const char * snippet )
{
    int sign = t >= 0.0 ? 1 : -1;
    double at = fabs( t );
    double nt = lrnd( at / PCI_MIO_16E_1_TIME_RESOLUTION )
                * PCI_MIO_16E_1_TIME_RESOLUTION;


    if ( fabs( at - nt ) > 0.01 * at )
    {
        if ( at < 1.0e-6 )
            print( SEVERE, "Requested %s had to be changed from %ld ns to "
                   "%ld ns.\n", snippet, lrnd( t * 1.0e9 ),
                   sign * lrnd( nt * 1.0e9 ) );
        else if ( at < 1.0e-3 )
            print( SEVERE, "Requested %s had to be changed from %.3f us to "
                   ".3%f us.\n", snippet, t * 1.0e6, sign * nt * 1.0e6 );
        else if ( at < 1.0 )
            print( SEVERE, "Requested %s had to be changed from %.6f ms to "
                   "%.6f ms.\n", snippet, t * 1.0e3, sign * nt * 1.0e3 );
        else
            print( SEVERE, "Requested %s had to be changed from %.9f s to "
                   "%.9f s.\n", snippet, t, sign * nt );
    }

    return sign * nt;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
