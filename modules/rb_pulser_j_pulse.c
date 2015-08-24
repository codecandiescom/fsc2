/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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


#include "rb_pulser_j.h"


/*--------------------------------------------------------*
 * Function gets called when a new pulse is to be created
 *--------------------------------------------------------*/

bool
rb_pulser_j_new_pulse( long pnum )
{
    Pulse_T *cp = rb_pulser_j.pulses;
    Pulse_T *lp = NULL;


    while ( cp != NULL )
    {
        if ( cp->num == pnum )
        {
            print( FATAL, "Can't create pulse with number %ld, it already "
                   "exists.\n", pnum );
            THROW( EXCEPTION );
        }

        lp = cp;
        cp = cp->next;
    }

    cp = T_malloc( sizeof *cp );

    if ( lp == NULL )
        rb_pulser_j.pulses = cp;
    else
        lp->next = cp;

    cp->prev = lp;
    cp->next = NULL;
    cp->num = pnum;
    cp->function = NULL;

    cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
    cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
                       = cp->initial_is_dlen = UNSET;

    cp->has_been_active = cp->was_active = UNSET;

    return OK;
}


/*---------------------------------------------*
 * Function to set the function of a new pulse
 *---------------------------------------------*/

bool
rb_pulser_j_set_pulse_function( long pnum,
                                int  function )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    /* The pulser has only tree functions */

    if (    function != PULSER_CHANNEL_MW
         && function != PULSER_CHANNEL_RF
         && function != PULSER_CHANNEL_DET )
    {
        print( FATAL, "Pulse function '%s' can't be used with this "
               "driver.\n", Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    if ( p->function != NULL )
    {
        print( FATAL, "The function of pulse #%ld has already been set to "
               "'%s'.\n", pnum, p->function->name );
        THROW( EXCEPTION );
    }

    if (    p->is_pos
         && p->pos + p->function->delay <
                              rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay )
    {
        print( FATAL, "Start position for pulse #%ld is too early.\n", pnum );
        THROW( EXCEPTION );
    }

    p->function = rb_pulser_j.function + function;

    return OK;
}


/*--------------------------------------------------*
 * Function for setting the position of a new pulse
 *--------------------------------------------------*/

bool
rb_pulser_j_set_pulse_position( long   pnum,
                                double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld has already been set "
               "to %s.\n", pnum, rb_pulser_j_ptime( p->pos ) );
        THROW( EXCEPTION );
    }

    if (    p->function != NULL
         && p_time + p->function->delay <
                              rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay )
    {
        print( FATAL, "Start position for pulse #%ld is too early.\n", pnum );
        THROW( EXCEPTION );
    }

    p->pos = p_time;
    p->is_pos = SET;
    p->initial_pos = p_time;
    p->initial_is_pos = SET;

    return OK;
}


/*------------------------------------------------*
 * Function for setting the length of a new pulse
 *------------------------------------------------*/

bool
rb_pulser_j_set_pulse_length( long   pnum,
                              double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p->is_len )
    {
        print( FATAL, "Length of pulse #%ld has already been set to %s.\n",
               pnum, rb_pulser_j_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative length set for pulse #%ld: %s.\n",
               pnum, rb_pulser_j_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->len = rb_pulser_j_double2ticks( p_time );
    p->is_len = SET;

    p->initial_len = p->len;
    p->initial_is_len = SET;

    return OK;
}


/*---------------------------------------------------------*
 * Function for setting the position change of a new pulse
 *--------------------------------------------------------*/

bool
rb_pulser_j_set_pulse_position_change( long   pnum,
                                       double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld has already been set "
               "to %s.\n", pnum, rb_pulser_j_ptime( p->dpos ) );
        THROW( EXCEPTION );
    }

    if ( p_time == 0.0 )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = p_time;
    p->is_dpos = SET;

    p->initial_dpos = p->dpos;
    p->initial_is_dpos = SET;

    return OK;
}


/*-------------------------------------------------------*
 * Function for setting the length change of a new pulse
 *-------------------------------------------------------*/

bool
rb_pulser_j_set_pulse_length_change( long   pnum,
                                     double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld has already been set to "
               "%s.\n", pnum, rb_pulser_j_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( rb_pulser_j_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = rb_pulser_j_double2ticks( p_time );
    p->is_dlen = SET;

    p->initial_dlen = p->dlen;
    p->initial_is_dlen = SET;

    return OK;
}


/*------------------------------------------*
 * Function returns the function of a pulse
 *------------------------------------------*/

bool
rb_pulser_j_get_pulse_function( long  pnum,
                                int * function )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p->function == NULL )
    {
        print( FATAL, "The function of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *function = p->function->self;
    return OK;
}


/*-----------------------------------------*
 * Function returns the position of a pulse
 *-----------------------------------------*/

bool
rb_pulser_j_get_pulse_position( long     pnum,
                                double * p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( ! p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = p->pos;

    return OK;
}


/*----------------------------------------*
 * Function returns the length of a pulse
 *----------------------------------------*/

bool
rb_pulser_j_get_pulse_length( long     pnum,
                              double * p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( ! p->is_len )
    {
        print( FATAL, "Length of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = rb_pulser_j_ticks2double( p->len );

    return OK;
}


/*-------------------------------------------------*
 * Function returns the position change of a pulse
 *-------------------------------------------------*/

bool
rb_pulser_j_get_pulse_position_change( long     pnum,
                                       double * p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( ! p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = p->dpos;

    return OK;
}


/*-----------------------------------------------*
 * Function returns the length change of a pulse
 *-----------------------------------------------*/

bool
rb_pulser_j_get_pulse_length_change( long     pnum,
                                     double * p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( ! p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = rb_pulser_j_ticks2double( p->dlen );

    return OK;
}


/*------------------------------------------------------------------*
 * Function to change the position of a pulse during the experiment
 *------------------------------------------------------------------*/

bool
rb_pulser_j_change_pulse_position( long   pnum,
                                   double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, rb_pulser_j_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    if ( p_time + p->function->delay <
                              rb_pulser_j.delay_card[ INIT_DELAY ].intr_delay )
    {
        print( FATAL, "Start position for pulse #%ld is too early.\n",
               pnum );
        THROW( EXCEPTION );
    }

    if (    p->is_pos
         && fabs( p_time - p->pos ) <= PRECISION * rb_pulser_j.timebase )
    {
        print( WARN, "Old and new position of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    p->pos = p_time;
    p->is_pos = SET;

    p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );

    return OK;
}


/*----------------------------------------------------------------*
 * Function to change the length of a pulse during the experiment
 *----------------------------------------------------------------*/

bool
rb_pulser_j_change_pulse_length( long   pnum,
                                 double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );
    Ticks volatile new_len = 0;


    if ( p_time < 0 )
    {
        print( FATAL, "Invalid (negative) length for pulse #%ld: %s.\n",
               pnum, rb_pulser_j_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_len = rb_pulser_j_double2ticks( p_time );
        TRY_SUCCESS;
    }
    CATCH( EXCEPTION )
    {
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW;

    if ( p->is_len && p->len == new_len )
    {
        print( WARN, "Old and new length of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    p->len = new_len;
    p->is_len = SET;

    p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );

    return OK;
}


/*-------------------------------------------------------------------------*
 * Function to change the position change of a pulse during the experiment
 *-------------------------------------------------------------------------*/

bool
rb_pulser_j_change_pulse_position_change( long   pnum,
                                          double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );


    if ( p_time == 0 && FSC2_MODE == TEST )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = p_time;
    p->is_dpos = SET;

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function to change the length change of a pulse during the experiment
 *-----------------------------------------------------------------------*/

bool
rb_pulser_j_change_pulse_length_change( long   pnum,
                                        double p_time )
{
    Pulse_T *p = rb_pulser_j_get_pulse( pnum );
    Ticks volatile new_dlen = 0;


    TRY
    {
        new_dlen = rb_pulser_j_double2ticks( p_time );
        TRY_SUCCESS;
    }
    CATCH( EXCEPTION )
    {
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW;

    if ( new_dlen == 0 && FSC2_MODE == TEST )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = new_dlen;
    p->is_dlen = SET;

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
