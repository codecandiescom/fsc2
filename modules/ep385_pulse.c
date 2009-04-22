/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "ep385.h"


/*----------------------------------------------*
 * Function for defining a completely new pulse
 *----------------------------------------------*/

bool
ep385_new_pulse( long pnum )
{
    Pulse_T *cp = ep385.pulses;
    Pulse_T *lp = NULL;


    while ( cp != NULL )
    {
        if ( cp->num == pnum )
        {
            print( FATAL, "Pulse #%ld already exists.\n", pnum );
            THROW( EXCEPTION );
        }
        lp = cp;
        cp = cp->next;
    }

    /* Append the pulse to the linked list of all pulses */

    cp = T_malloc( sizeof *cp );

    if ( ep385.pulses == NULL )
    {
        ep385.pulses = cp;
        cp->prev = NULL;
    }
    else
    {
        cp->prev = lp;
        lp->next = cp;
    }

    cp->next = NULL;

    cp->num = pnum;
    cp->is_function = UNSET;

    cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
    cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
        = cp->initial_is_dlen = UNSET;
    cp->is_old_pos = cp->is_old_len = UNSET;

    cp->pc = NULL;
    cp->needs_update = UNSET;
    cp->has_been_active = cp->was_active = UNSET;

    cp->left_shape_warning = UNSET;
    cp->right_shape_warning = UNSET;
    cp->sp = NULL;

    cp->left_twt_warning = UNSET;
    cp->right_twt_warning = UNSET;
    cp->tp = NULL;

    return OK;
}


/*--------------------------------------------------*
 * Function for setting the function of a new pulse
 *--------------------------------------------------*/

bool
ep385_set_pulse_function( long pnum,
                          int  function )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Function_T *f = &ep385.function[ function ];


    if (    function == PULSER_CHANNEL_PHASE_1
         || function == PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "Phase functions can't be used with this driver.\n" );
        THROW( EXCEPTION );
    }

    /* Check that no function has been set for the pulse yet */

    if ( p->is_function )
    {
        print( FATAL, "Function of pulse #%ld has already been set to '%s'.\n",
               pnum, p->function->name );
        THROW( EXCEPTION );
    }

    /* Check that the pulses function has been set up */

    if ( ! f->is_used )
    {
        print( FATAL, "The function '%s' of pulse #%ld hasn't been "
               "declared in the ASSIGNMENTS section.\n", f->name, p->num );
        THROW( EXCEPTION );
    }

    /* Check that the function the pulse belongs to has been associated with
       at least one channel */

    if ( f->channel[ 0 ] == NULL )
    {
        print( FATAL, "No channel has been assigned to function '%s'.\n",
               ep385.function[ function ].name );
        THROW( EXCEPTION );
    }

    /* If the pulses position has already been set check that it isn't
       negative when the delay is added */

    if ( p->is_pos && p->pos + p->function->delay < 0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld.\n",
               pnum );
        THROW( EXCEPTION );
    }

    /* Set function of the pulse */

    p->function = f;

    p->is_function = SET;
    p->function->is_needed = SET;

    return OK;
}


/*--------------------------------------------------------*
 * Function for setting the start position of a new pulse
 *--------------------------------------------------------*/

bool
ep385_set_pulse_position( long   pnum,
                          double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld has already been set "
               "to %s.\n", pnum, ep385_pticks( p->pos ) );
        THROW( EXCEPTION );
    }

    p->pos = ep385_double2ticks( p_time );

    /* If the pulses function is already set check that the position,
       including the delay, isn't negative */

    if ( p->is_function && p->pos + p->function->delay < 0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, ep385_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->is_pos = SET;

    if ( ! p->initial_is_pos && FSC2_MODE == PREPARATION )
    {
        p->initial_pos = p->pos;
        p->initial_is_pos = SET;
    }
    else if ( ! p->is_old_pos )
    {
        p->old_pos = p->pos;
        p->is_old_pos = SET;
    }

    return OK;
}


/*------------------------------------------------*
 * Function for setting the length of a new pulse
 *------------------------------------------------*/

bool
ep385_set_pulse_length( long   pnum,
                        double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( p->is_len )
    {
        print( FATAL, "Length of pulse #%ld has already been set to %s.\n",
               pnum, ep385_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative length set for pulse #%ld: %s.\n",
               pnum, ep385_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->len = ep385_double2ticks( p_time );
    p->is_len = SET;

    if ( ! p->initial_is_len && FSC2_MODE == PREPARATION )
    {
        p->initial_len = ep385_double2ticks( p_time );
        p->initial_is_len = SET;
    }
    else if ( ! p->is_old_len )
    {
        p->old_len = p->len;
        p->is_old_len = SET;
    }

    return OK;
}


/*------------------------------------------------------------*
 * Function for setting the position increment of a new pulse
 *------------------------------------------------------------*/

bool
ep385_set_pulse_position_change( long   pnum,
                                 double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld has already been set "
               "to %s.\n", pnum, ep385_pticks( p->dpos ) );
        THROW( EXCEPTION );
    }

    if ( ep385_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero position change set for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = ep385_double2ticks( p_time );
    p->is_dpos = SET;

    if ( ! p->initial_is_dpos && FSC2_MODE == PREPARATION )
    {
        p->initial_dpos = ep385_double2ticks( p_time );
        p->initial_is_dpos = SET;
    }

    return OK;
}


/*----------------------------------------------------------*
 * Function for setting the length increment of a new pulse
 *----------------------------------------------------------*/

bool
ep385_set_pulse_length_change( long   pnum,
                               double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld has already been set to "
               "%s.\n", pnum, ep385_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( ep385_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero length change set for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = ep385_double2ticks( p_time );
    p->is_dlen = SET;

    if ( ! p->initial_is_dlen && FSC2_MODE == PREPARATION )
    {
        p->initial_dlen = ep385_double2ticks( p_time );
        p->initial_is_dlen = SET;
    }

    return OK;
}


/*-----------------------------------------------------*
 * Function for setting the phase cycle of a new pulse
 *-----------------------------------------------------*/

bool
ep385_set_pulse_phase_cycle( long pnum,
                             long cycle )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Phs_Seq_T *pc = PA_Seq.phs_seq;


    /* Check that no phase cycle has been asigned to the pulse yet */

    if ( p->pc != NULL )
    {
        print( FATAL, "Pulse #%ld has already been assigned a phase cycle.\n",
               pnum );
        THROW( EXCEPTION );
    }

    /* Check that a phase sequence has been associated with the function
       the pulse belongs to */

    if ( p->function->phase_setup == NULL )
    {
        print( FATAL, "Function %s of pulse #%ld has not not been set up for "
               "for phase cycling\n", p->function->name, pnum );
        THROW( EXCEPTION );
    }

    /* Get a pointer to the phase cycle */

    while ( pc != NULL )
    {
        if ( pc->num == cycle )
            break;
        pc = pc->next;
    }

    if ( pc == NULL )
    {
        print( FATAL, "Referenced phase sequence %ld hasn't been defined.\n",
               cycle );
        THROW( EXCEPTION );
    }

    p->pc = pc;

    return OK;
}


/*-----------------------------------------------*
 * Function for querying the function of a pulse
 *-----------------------------------------------*/

bool
ep385_get_pulse_function( long  pnum,
                          int * function )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( ! p->is_function )
    {
        print( FATAL, "The function of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *function = p->function->self;
    return OK;
}


/*-------------------------------------------------------------*
 * Function for querying the current start position of a pulse
 *-------------------------------------------------------------*/

bool
ep385_get_pulse_position( long     pnum,
                          double * p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( ! p->is_pos )
    {
        print( FATAL, "Start position of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = ep385_ticks2double( p->pos );
    return OK;
}


/*-----------------------------------------------------*
 * Function for querying the current length of a pulse
 *-----------------------------------------------------*/

bool
ep385_get_pulse_length( long     pnum,
                        double * p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( ! p->is_len )
    {
        print( FATAL, "Length of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = ep385_ticks2double( p->len );
    return OK;
}


/*-----------------------------------------------------------------------*
 * Function for querying the current start position increment of a pulse
 *-----------------------------------------------------------------------*/

bool
ep385_get_pulse_position_change( long     pnum,
                                 double * p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( ! p->is_dpos )
    {
        print( FATAL, "Position change of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = ep385_ticks2double( p->dpos );
    return OK;
}


/*---------------------------------------------------------------*
 * Function for querying the current length increment of a pulse
 *---------------------------------------------------------------*/

bool
ep385_get_pulse_length_change( long     pnum,
                               double * p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( ! p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = ep385_ticks2double( p->dlen );
    return OK;
}


/*--------------------------------------------------*
 * Function for querying the phase cycle of a pulse
 *--------------------------------------------------*/

bool
ep385_get_pulse_phase_cycle( long   pnum,
                             long * cycle )
{
    Pulse_T *p = ep385_get_pulse( pnum );


    if ( p->pc == NULL )
    {
        print( FATAL, "No phase cycle has been set for pulse #%ld.\n", pnum );
        THROW( EXCEPTION );
    }

    *cycle = p->pc->num;
    return OK;
}


/*---------------------------------------------------------------------------*
 * Function for changing the pulse position while the experiment is running.
 *---------------------------------------------------------------------------*/

bool
ep385_change_pulse_position( long   pnum,
                             double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Ticks new_pos = 0;


    CLOBBER_PROTECT( new_pos );

    if ( p_time + p->function->delay < 0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, ep385_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_pos = ep385_double2ticks( p_time );
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
        RETHROW( );

    if ( p->is_pos && new_pos == p->pos )
    {
        print( WARN, "Old and new position of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    if ( p->is_pos && ! p->is_old_pos )
    {
        p->old_pos = p->pos;
        p->is_old_pos = SET;
    }

    p->pos = new_pos;
    p->is_pos = SET;

    p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
    p->needs_update = NEEDS_UPDATE( p );

    /* Also shift shape and TWT pulses associated with the pulse */

    if ( p->sp != NULL )
    {
        p->sp->pos = new_pos;
        p->sp->is_pos = SET;
        p->sp->old_pos = p->old_pos;
        p->sp->is_old_pos = p->is_old_pos;

        p->sp->has_been_active |= ( p->sp->is_active = p->is_active );
        p->sp->needs_update = p->needs_update;
    }

    if ( p->tp != NULL )
    {
        p->tp->pos = p->pos;
        p->tp->old_pos = p->old_pos;
        p->tp->is_old_pos = p->is_old_pos;

        p->tp->has_been_active |= ( p->tp->is_active = p->is_active );
        p->tp->needs_update = p->needs_update;
    }

    if ( p->needs_update )
        ep385.needs_update = SET;

    return OK;
}


/*-------------------------------------------------------------------------*
 * Function for changing the pulse length while the experiment is running.
 *-------------------------------------------------------------------------*/

bool
ep385_change_pulse_length( long   pnum,
                           double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Ticks new_len = 0;


    CLOBBER_PROTECT( new_len );

    if ( p_time < 0 )
    {
        print( FATAL, "Invalid (negative) length for pulse #%ld: %s.\n",
               pnum, ep385_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_len = ep385_double2ticks( p_time );
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
        RETHROW( );

    if (    p->is_len
         && p->is_function
         && p->function->channel
         && p->len == new_len )
    {
        print( WARN, "Old and new length of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    if ( p->is_len && ! p->is_old_len )
    {
        p->old_len = p->len;
        p->is_old_len = SET;
    }

    p->len = new_len;
    p->is_len = SET;

    p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
    p->needs_update = NEEDS_UPDATE( p );

    /* Also lengthen shape and TWT pulses associated with the pulse */

    if ( p->sp != NULL )
    {
        p->sp->len = new_len;
        p->sp->is_len = SET;
        p->sp->old_len = p->old_len;
        p->sp->is_old_len = p->is_old_len;

        p->sp->has_been_active |= ( p->sp->is_active = p->is_active );
        p->sp->needs_update = p->needs_update;
    }

    if ( p->tp != NULL )
    {
        p->tp->len = p->len;
        p->tp->old_len = p->old_len;
        p->tp->is_old_len = p->is_old_len;

        p->tp->has_been_active |= ( p->tp->is_active = p->is_active );
        p->tp->needs_update = p->needs_update;
    }

    if ( p->needs_update )
        ep385.needs_update = SET;

    return OK;
}


/*----------------------------------------------------*
 * Function for changing the pulse position increment
 * while the experiment is running.
 *----------------------------------------------------*/

bool
ep385_change_pulse_position_change( long   pnum,
                                    double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Ticks new_dpos = 0;


    CLOBBER_PROTECT( new_dpos );

    TRY
    {
        new_dpos = ep385_double2ticks( p_time );
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
        RETHROW( );

    if ( new_dpos == 0 && FSC2_MODE == TEST )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = new_dpos;
    p->is_dpos = SET;

    if ( p->sp != NULL )
    {
        p->sp->dpos = new_dpos;
        p->sp->is_dpos = SET;
    }

    return OK;
}


/*--------------------------------------------------*
 * Function for changing the pulse length increment
 * while the experiment is running.
 *--------------------------------------------------*/

bool
ep385_change_pulse_length_change( long   pnum,
                                  double p_time )
{
    Pulse_T *p = ep385_get_pulse( pnum );
    Ticks new_dlen = 0;


    CLOBBER_PROTECT( new_dlen );

    TRY
    {
        new_dlen = ep385_double2ticks( p_time );
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
        RETHROW( );

    if ( new_dlen == 0 && FSC2_MODE == TEST )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = new_dlen;
    p->is_dlen = SET;

    if ( p->sp != NULL )
    {
        p->sp->dlen = new_dlen;
        p->sp->is_dlen = SET;
    }

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
