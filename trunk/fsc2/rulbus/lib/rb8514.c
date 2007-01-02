/*
 *  $Id$
 *
 *  Copyright (C) 2003-2007 Jens Thoms Toerring
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
#include <limits.h>


typedef struct RULBUS_RB8514_DELAY_CARD RULBUS_RB8514_DELAY_CARD;

struct RULBUS_RB8514_DELAY_CARD {
    int handle;
    unsigned char ctrl;
    unsigned long delay;
    double intr_delay;
    double clock_time;
};


#define DATA_MSBYTE   0
#define DATA_LSBYTE   2
#define STATUS_ADDR   3
#define CONTROL_ADDR  3


/* Bits in the control byte */

#define START_PULSE_OUT_1_ENABLE  ( 1 << 0 )
#define END_PULSE_OUT_1_ENABLE    ( 1 << 1 )
#define START_PULSE_OUT_2_ENABLE  ( 1 << 2 )
#define END_PULSE_OUT_2_ENABLE    ( 1 << 3 )
#define INTERRUPT_ENABLE          ( 1 << 4 )
#define TRIGGER_ON_FALLING_EDGE   ( 1 << 5 )
#define START_PULSE_POSITIVE      ( 1 << 6 )
#define END_PULSE_POSITIVE        ( 1 << 7 )

/* Bits in the status byte */

#define DELAY_BUSY                ( 1 << 1 )

#define INVALID_DELAY             ( RULBUS_RB8514_DELAY_CARD_MAX + 1 )


static RULBUS_RB8514_DELAY_CARD *rulbus_rb8514_delay_card = NULL;
static int rulbus_num_delay_cards = 0;


static RULBUS_RB8514_DELAY_CARD *rulbus_rb8514_delay_card_find( int handle );

static inline long lrnd( double x );


/*------------------------------------------------------------------*
 * Function for initializing the delay card subsystem (gets invoked
 * automatically by the rulbus_open() function, so it's not supposed
 * to be called by a user directly)
 *------------------------------------------------------------------*/

int rulbus_rb8514_delay_init( void )
{
    rulbus_rb8514_delay_card = NULL;
    rulbus_num_delay_cards = 0;
    return rulbus_errno = RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the delay card subsystem (gets invoked
 * automatically by the rulbus_close() function, so it's not to be
 * called by the user directly). Usually it won't even do anything,
 * at least if rulbus_rb8514_delay_card_exit() has been called for
 * all existing cards.
 *------------------------------------------------------------------*/

void rulbus_rb8514_delay_exit( void )
{
    if ( rulbus_rb8514_delay_card == NULL )
        return;

    free( rulbus_rb8514_delay_card );
    rulbus_rb8514_delay_card = NULL;
    rulbus_num_delay_cards = 0;
}


/*---------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open() function, so it's not supposed to be
 * called by a user directly).
 *---------------------------------------------------------------------*/

int rulbus_rb8514_delay_card_init( int handle )
{
    RULBUS_RB8514_DELAY_CARD *tmp;
    unsigned char byte[ 3 ] = { 0, 0, 0 };
    int retval;


    tmp = realloc( rulbus_rb8514_delay_card,
                   ( rulbus_num_delay_cards + 1 ) * sizeof *tmp );

    if ( tmp == NULL )
        return rulbus_errno = RULBUS_NO_MEMORY;

    rulbus_rb8514_delay_card = tmp;
    tmp += rulbus_num_delay_cards++;

    tmp->handle = handle;
    tmp->delay = 0;
    tmp->clock_time = -1.0;
    tmp->ctrl = START_PULSE_POSITIVE | END_PULSE_POSITIVE;

    /* Set a few defaults: output neither start nor end pulses, disable
       interrupts, trigger on rasing edge, output polarity of start and
       end pulses is positive, finally set the delay to 0. */

    if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
                                  &tmp->ctrl, 1 ) ) != RULBUS_OK )
        return rulbus_errno = retval;
    
    if ( ( retval = rulbus_write_range( handle, DATA_MSBYTE, byte, 3 ) ) != 3 )
        return rulbus_errno = retval;

    return rulbus_errno = RULBUS_OK;
}
    

/*---------------------------------------------------------------*
 * Function for deactivation a card (gets invoked automatically
 * by the rulbus_card_close() function, so it's not to be called
 * by the user directly).
 *---------------------------------------------------------------*/

int rulbus_rb8514_delay_card_exit( int handle )
{
    RULBUS_RB8514_DELAY_CARD *card;


    /* Try to find the card, if it doesn't exist just return */

    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_OK;

    /* Remove the entry for the card */

    if ( rulbus_num_delay_cards > 1 )
    {
        if ( card != rulbus_rb8514_delay_card + rulbus_num_delay_cards - 1 )
            memmove( card, card + 1, sizeof *card *
                     ( rulbus_num_delay_cards -
                       ( card - rulbus_rb8514_delay_card ) - 1 ) );

        card = realloc( rulbus_rb8514_delay_card,
                        ( rulbus_num_delay_cards - 1 ) * sizeof *card );

        if ( card == NULL )
            return rulbus_errno = RULBUS_NO_MEMORY;

        rulbus_rb8514_delay_card = card;
    }
    else
    {
        free( rulbus_rb8514_delay_card );
        rulbus_rb8514_delay_card = NULL;
    }
        
    rulbus_num_delay_cards--;
    return rulbus_errno = RULBUS_OK;
}


/*------------------------------------------------------------------------*
 * Function for setting the frequency of the clock feeding the delay card
 *------------------------------------------------------------------------*/

int rulbus_rb8514_delay_set_clock_frequency( int    handle,
                                             double freq )
{
    RULBUS_RB8514_DELAY_CARD *card;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( freq <= 0.0 )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    card->clock_time = 1.0 / freq;
    return rulbus_errno = RULBUS_OK;
}


/*-----------------------------------------*
 * Function for setting the delay duration
 *-----------------------------------------*/

int rulbus_rb8514_delay_set_delay( int    handle,
                                   double delay,
                                   int    force )
{
    RULBUS_RB8514_DELAY_CARD *card;
    long ldelay;
    int retval;
    unsigned char i;
    unsigned char bytes[ 3 ];


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( card->clock_time <= 0.0 )
        return rulbus_errno = RULBUS_NO_CLOCK_FREQ;

    ldelay = lrnd( ( delay - card->intr_delay ) / card->clock_time );

    if ( ldelay < 0 || ldelay > RULBUS_RB8514_DELAY_CARD_MAX )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    if ( delay == card->delay )
        return rulbus_errno = RULBUS_OK;

    /* Check that the card isn't currently creating a delay - if it is tell
       the user it's busy unless the 'force' flag is set (which allows the
       user to set a new delay even though a delay is already created, which
       then gets ended prematurely) */

    if ( ( retval = rulbus_read( handle, STATUS_ADDR, bytes, 1 ) ) != 1 )
         return rulbus_errno = retval;

    if ( bytes[ 0 ] & DELAY_BUSY && ! force )
        return rulbus_errno = RULBUS_CARD_IS_BUSY;

    card->delay = ldelay;

    for ( i = 0; i < 3; ldelay >>= 8, i++ )
        bytes[ 2 - i ] = ( unsigned char ) ( ldelay & 0xFF );
    
    if ( ( retval = rulbus_write_range( handle, DATA_MSBYTE,
                                        bytes, 3 ) ) != 3 )
        return rulbus_errno = retval;

    return rulbus_errno = RULBUS_OK;
}


/*--------------------------------------------------------------*
 * Function for setting the delay duration given in clock ticks
 * (the intrinsic delay has to be dealt with by the caller)
 *--------------------------------------------------------------*/

int rulbus_rb8514_delay_set_raw_delay( int           handle,
                                       unsigned long delay,
                                       int           force )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char bytes[ 3 ];
    unsigned char i;
    int retval;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( delay > RULBUS_RB8514_DELAY_CARD_MAX )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    if ( delay == card->delay )
        return rulbus_errno = RULBUS_OK;

    /* Check that the card isn't currently creating a delay - if it is tell
       the user it's busy unless the 'force' flag is set (which allows the
       user to set a new delay even though a delay is already created, which
       then ends prematurely) */

    if ( ( retval = rulbus_read( handle, STATUS_ADDR, bytes, 1 ) ) != 1 )
         return rulbus_errno = retval;

    if ( bytes[ 0 ] & DELAY_BUSY && ! force )
        return rulbus_errno = RULBUS_CARD_IS_BUSY;

    card->delay = delay;

    for ( i = 0; i < 3; delay >>= 8, i++ )
        bytes[ 2 - i ] = ( unsigned char ) ( delay & 0xFF );

    if ( ( retval = rulbus_write_range( handle, DATA_MSBYTE,
                                        bytes, 3 ) ) != 3 )
        return rulbus_errno = retval;

    return rulbus_errno = RULBUS_OK;
}


/*---------------------------------------------------------------*
 * Function for selecting if the delay is started by the falling
 * or the raising edge of the external trigger
 *---------------------------------------------------------------*/

int rulbus_rb8514_delay_set_trigger( int handle,
                                     int edge )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char ctrl;
    int retval;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( edge != RULBUS_RB8514_DELAY_FALLING_EDGE &&
         edge != RULBUS_RB8514_DELAY_RAISING_EDGE )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    ctrl = card->ctrl;

    if ( edge == RULBUS_RB8514_DELAY_FALLING_EDGE )
        ctrl |= TRIGGER_ON_FALLING_EDGE;
    else
        ctrl &= ~ TRIGGER_ON_FALLING_EDGE;

    if ( ctrl == card->ctrl )
        return rulbus_errno = RULBUS_OK;

    if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
                                  &ctrl, 1 ) ) != 1 )
        return rulbus_errno = retval;

    card->ctrl = ctrl;

    return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------------------*
 * Function for selecting if a start and/or end pulse is to be
 * output at output 1 and/or 2
 *-------------------------------------------------------------*/

int rulbus_rb8514_delay_set_output_pulse( int handle,
                                          int output,
                                          int type )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char ctrl;
    int retval;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( ( output != RULBUS_RB8514_DELAY_OUTPUT_1 &&
           output != RULBUS_RB8514_DELAY_OUTPUT_2 &&
           output != RULBUS_RB8514_DELAY_OUTPUT_BOTH ) ||
         ( type != RULBUS_RB8514_DELAY_START_PULSE &&
           type != RULBUS_RB8514_DELAY_END_PULSE &&
           type != RULBUS_RB8514_DELAY_PULSE_BOTH &&
           type != RULBUS_RB8514_DELAY_PULSE_NONE ) )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    ctrl = card->ctrl;

    if ( output & RULBUS_RB8514_DELAY_OUTPUT_1 )
    {
        if ( type & RULBUS_RB8514_DELAY_START_PULSE )
            ctrl |= START_PULSE_OUT_1_ENABLE;
        else
            ctrl &= ~ START_PULSE_OUT_1_ENABLE;

        if ( type & RULBUS_RB8514_DELAY_END_PULSE )
            ctrl |= END_PULSE_OUT_1_ENABLE;
        else
            ctrl &= ~ END_PULSE_OUT_1_ENABLE;
    }
        
    if ( output & RULBUS_RB8514_DELAY_OUTPUT_2 )
    {
        if ( type & RULBUS_RB8514_DELAY_START_PULSE )
            ctrl |= START_PULSE_OUT_2_ENABLE;
        else
            ctrl &= ~ START_PULSE_OUT_2_ENABLE;

        if ( type & RULBUS_RB8514_DELAY_END_PULSE )
            ctrl |= END_PULSE_OUT_2_ENABLE;
        else
            ctrl &= ~ END_PULSE_OUT_2_ENABLE;
    }

    if ( ctrl == card->ctrl )
        return rulbus_errno = RULBUS_OK;

    if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
                                  &ctrl, 1 ) ) != 1 )
        return rulbus_errno = retval;

    card->ctrl = ctrl;

    return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------------------*
 * Function for selecting if a start and/or end pulse is to be
 * output at output 1 and/or 2
 *-------------------------------------------------------------*/

int rulbus_rb8514_delay_set_output_pulse_polarity( int handle,
                                                   int type,
                                                   int pol )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char ctrl;
    int retval;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( ( type != RULBUS_RB8514_DELAY_START_PULSE &&
           type != RULBUS_RB8514_DELAY_END_PULSE &&
           type != RULBUS_RB8514_DELAY_PULSE_BOTH ) ||
         ( pol != RULBUS_RB8514_DELAY_POLARITY_NEGATIVE &&
           pol != RULBUS_RB8514_DELAY_POLARITY_POSITIVE ) )
        return rulbus_errno = RULBUS_INVALID_ARGUMENT;

    ctrl = card->ctrl;

    if ( type & RULBUS_RB8514_DELAY_START_PULSE )
    {
        if ( pol == RULBUS_RB8514_DELAY_POLARITY_NEGATIVE )
            ctrl &= ~ START_PULSE_POSITIVE;
        else
            ctrl |= START_PULSE_POSITIVE;
    }

    if ( type & RULBUS_RB8514_DELAY_END_PULSE )
    {
        if ( pol == RULBUS_RB8514_DELAY_POLARITY_NEGATIVE )
            ctrl &= ~ END_PULSE_POSITIVE;
        else
            ctrl |= END_PULSE_POSITIVE;
    }

    if ( ctrl == card->ctrl )
        return rulbus_errno = RULBUS_OK;

    if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
                                  &ctrl, 1 ) ) != 1 )
        return rulbus_errno = retval;

    card->ctrl = ctrl;

    return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------------------------*
 * Function to start a delay via software (by setting the trigger
 * first to falling, then to raising edge). Afterwards, the trigger
 * slope is set back to the original setting. And, to make sure we
 * have a state transition from raising to falling edge the slope
 * first is set to raising edge (probably that's being too careful).
 *-------------------------------------------------------------------*/

int rulbus_rb8514_software_start( int handle )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char bytes[ 4 ];
    size_t cnt;
    int retval;

    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    bytes[ 0 ] = bytes[ 2 ] = card->ctrl & ~ TRIGGER_ON_FALLING_EDGE;
    bytes[ 1 ] = bytes[ 3 ] = card->ctrl | TRIGGER_ON_FALLING_EDGE;
    
    cnt = card->ctrl & TRIGGER_ON_FALLING_EDGE ? 4 : 3;

    /* Take care not to set rulbus_errno to a positive value in case
       rulbus_write() returned with too short a byte count! */

    if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
                                  bytes, cnt ) ) != ( int ) cnt )
        return rulbus_errno = retval < 0 ? retval : RULBUS_WRITE_ERROR;

    return rulbus_errno = RULBUS_OK;
}
    

/*-----------------------------------------------------------------*
 * Function to determine if the card is currently creating a delay
 *-----------------------------------------------------------------*/

int rulbus_rb8514_delay_busy( int handle )
{
    RULBUS_RB8514_DELAY_CARD *card;
    unsigned char byte;
    int retval;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    if ( ( retval = rulbus_read( handle, STATUS_ADDR, &byte, 1 ) ) != 1 )
        return rulbus_errno = retval;

    rulbus_errno = RULBUS_OK;
    return ( byte & DELAY_BUSY ) ? 1 : 0;
}


/*---------------------------------------------------------*
 * Function returns the minimum delay the card can produce
 *---------------------------------------------------------*/

double rulbus_rb8514_delay_get_intrinsic_delay( int handle )
{
    RULBUS_RB8514_DELAY_CARD *card;


    if ( ( card = rulbus_rb8514_delay_card_find( handle ) ) == NULL )
        return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

    rulbus_errno = RULBUS_OK;
    return card->intr_delay;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_RB8514_DELAY_CARD *rulbus_rb8514_delay_card_find( int handle )
{
    int i;


    if ( handle < 0 )
        return NULL;

    for ( i = 0; i < rulbus_num_delay_cards; i++ )
        if ( handle == rulbus_rb8514_delay_card[ i ].handle )
            return rulbus_rb8514_delay_card + i;

    return NULL;
}


/*---------------------------------------------------
 * Utility function for rounding double values.
 *---------------------------------------------------*/

static inline long lrnd( double x )
{
    if ( x > LONG_MAX )
        return LONG_MAX;
    if ( x < LONG_MIN )
        return LONG_MIN;

    return ( long ) ( x < 0.0 ? ceil( x - 0.5 ) : floor( x + 0.5 ) );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
