/*
 *  Copyright (C) 2003-2009 Jens Thoms Toerring
 *
 *  Library for Rulbus (Rijksuniversiteit Leiden BUS)
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  The library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the package; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  To contact the author send email to jt@toerring.de
 */


#include "rulbus_lib.h"


typedef struct RULBUS_RB8515_CLOCK_CARD RULBUS_RB8515_CLOCK_CARD;

struct RULBUS_RB8515_CLOCK_CARD {
    int handle;
    unsigned char ctrl;
};


static RULBUS_RB8515_CLOCK_CARD *rulbus_rb8515_clock_card = NULL;
static int rulbus_num_clock_cards = 0;


static RULBUS_RB8515_CLOCK_CARD *rulbus_rb8515_clock_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the clock card subsystem (gets invoked
 * automatically by the rulbus_open() function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_rb8515_clock_init( void )
{
    rulbus_rb8515_clock_card = NULL;
    rulbus_num_clock_cards = 0;
    return rulbus_errno = RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the clock card subsystem (gets invoked
 * automatically by the rulbus_close() function, so it's not to be
 * called by the user directly). Usually it won't even do anything,
 * at least if rulbus_rb8515_clock_card_exit() has been called for
 * all existing cards.
 *------------------------------------------------------------------*/

void rulbus_rb8515_clock_exit( void )
{
    if ( rulbus_rb8515_clock_card == NULL )
        return;

    free( rulbus_rb8515_clock_card );
    rulbus_rb8515_clock_card = NULL;
    rulbus_num_clock_cards = 0;
}


/*---------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open() function, so it's not to be called by the
 * user directly)
 *---------------------------------------------------------------------*/

int rulbus_rb8515_clock_card_init( int handle )
{
    RULBUS_RB8515_CLOCK_CARD *tmp;
    int retval;


    tmp = realloc( rulbus_rb8515_clock_card,
                   ( rulbus_num_clock_cards + 1 ) * sizeof *tmp );

    if ( tmp == NULL )
        return rulbus_errno = RULBUS_NO_MEMORY;

    rulbus_rb8515_clock_card = tmp;
    tmp += rulbus_num_clock_cards++;

    tmp->handle = handle;
    tmp->ctrl = RULBUS_RB8515_CLOCK_FREQ_OFF;

    /* Stop the clock */

    if ( ( retval = rulbus_write( handle, 0, &tmp->ctrl, 1 ) ) != 1 )
        return rulbus_errno = retval;

    return rulbus_errno = RULBUS_OK;
}
    

/*---------------------------------------------------------------*
 * Function for deactivation a card (gets invoked automatically
 * by the rulbus_card_close() function, so it's not to be called
 * by the user directly)
 *---------------------------------------------------------------*/

int rulbus_rb8515_clock_card_exit( int handle )
{
    RULBUS_RB8515_CLOCK_CARD *card;


    /* Try to find the card, if it doesn't exist just return */

    if ( ( card = rulbus_rb8515_clock_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_OK;

    /* Remove the entry for the card */

    if ( rulbus_num_clock_cards > 1 )
    {
        if ( card != rulbus_rb8515_clock_card + rulbus_num_clock_cards - 1 )
            memmove( card, card + 1, sizeof *card *
                     ( rulbus_num_clock_cards -
                       ( card - rulbus_rb8515_clock_card ) - 1 ) );

        card = realloc( rulbus_rb8515_clock_card,
                        ( rulbus_num_clock_cards - 1 ) * sizeof *card );

        if ( card == NULL )
            return rulbus_errno = RULBUS_NO_MEMORY;

        rulbus_rb8515_clock_card = card;
    }
    else
    {
        free( rulbus_rb8515_clock_card );
        rulbus_rb8515_clock_card = NULL;
    }

    rulbus_num_clock_cards--;

    return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------*
 * Function for setting the frequency of the clock
 *-------------------------------------------------*/

int rulbus_rb8515_clock_set_frequency( int handle,
                                       int freq )
{
    RULBUS_RB8515_CLOCK_CARD *card;
    int retval;


    /* Try to find the card, if it doesn't exist just return */

    if ( ( card = rulbus_rb8515_clock_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( freq < RULBUS_RB8515_CLOCK_FREQ_OFF ||
         freq > RULBUS_RB8515_CLOCK_FREQ_100MHz )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    if ( card->ctrl == freq )
        return rulbus_errno = RULBUS_OK;

    card->ctrl = freq;
    if ( ( retval = rulbus_write( handle, 0, &card->ctrl, 1 ) ) != 1 )
        return rulbus_errno = retval;

    return rulbus_errno = RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_RB8515_CLOCK_CARD *rulbus_rb8515_clock_card_find( int handle )
{
    int i;


    if ( handle < 0 )
        return NULL;

    for ( i = 0; i < rulbus_num_clock_cards; i++ )
        if ( handle == rulbus_rb8515_clock_card[ i ].handle )
            return rulbus_rb8515_clock_card + i;

    return NULL;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
