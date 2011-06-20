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


#include "rb_pulser_w.h"


/*--------------------------------------------------------*
 * Function gets called when a new pulse is to be created
 *--------------------------------------------------------*/

bool
rb_pulser_w_new_pulse( long pnum )
{
    Pulse_T *cp = rb_pulser_w.pulses;
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
        rb_pulser_w.pulses = cp;
    else
        lp->next = cp;

    cp->prev = lp;
    cp->next = NULL;
    cp->num = pnum;
    cp->function = NULL;
    cp->pc = NULL;

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
rb_pulser_w_set_pulse_function( long pnum,
                                int  function )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    /* The pulser has only five functions that can be set */

    if (    function != PULSER_CHANNEL_MW
         && function != PULSER_CHANNEL_RF
         && function != PULSER_CHANNEL_LASER
         && function != PULSER_CHANNEL_DEFENSE
         && function != PULSER_CHANNEL_DET )
    {
        print( FATAL, "Pulse function '%s' can't be used with this "
               "driver.\n", Function_Names[ function ] );
        THROW( EXCEPTION );
    }

    /* Moreover, defense pulses can only be created manually if the user
       explecitely asked for it. */

    if ( function == PULSER_CHANNEL_DEFENSE && rb_pulser_w.defense_pulse_mode )
    {
        print( FATAL, "A DEFENSE pulse can only be created if the function "
               "'pulser_defense_pulse_mode()' has been called previously "
               "with the argument \"OFF\" to switch off automatic creation "
               "of the defense pulse.\n" );
        THROW( EXCEPTION );
    }

    if ( p->function != NULL )
    {
        print( FATAL, "The function of pulse #%ld has already been set to "
               "'%s'.\n", pnum, p->function->name );
        THROW( EXCEPTION );
    }

    if ( p->pc && function != PULSER_CHANNEL_MW )
    {
        print( FATAL, "A pulse which needs phase-cycling can't be assigned "
               "to function '%s'.\n", rb_pulser_w.function[ function ].name );
        THROW( EXCEPTION );
    }

    if ( function == PULSER_CHANNEL_DET && p->is_len && p->len > 1 )
    {
        print( SEVERE, "Length of DETECTION pulse can only be either 0 or "
               "%s. Setting it to the latter value.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        p->len = 1;
    }

    if ( function == PULSER_CHANNEL_DET && p->is_dlen )
    {
        print( SEVERE, "Length change for DETECTION pulse has set but its "
               "length can be only either 0 or %s.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        p->is_dlen = UNSET;
    }

    if (    function == PULSER_CHANNEL_DEFENSE
         && p->is_pos
         && fabs( p->pos + rb_pulser_w.function[ function ].delay ) >
                                             PRECISION * rb_pulser_w.timebase )
    {
        print( FATAL, "Position of defense pulse #%ld can only be set "
               "to %s.\n", pnum,
               rb_pulser_w_ptime( - rb_pulser_w.function[ function ].delay ) );
        THROW( EXCEPTION );
    }

    if ( function == PULSER_CHANNEL_DEFENSE )
        p->pos = - rb_pulser_w.function[ function ].delay;

    p->function = rb_pulser_w.function + function;

    return OK;
}


/*--------------------------------------------------*
 * Function for setting the position of a new pulse
 *--------------------------------------------------*/

bool
rb_pulser_w_set_pulse_position( long   pnum,
                                double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld has already been set "
               "to %s.\n", pnum, rb_pulser_w_ptime( p->pos ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative start position set for pulse "
               "#%ld: %s.\n", pnum, rb_pulser_w_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    if (    p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE
         && fabs( p_time + p->function->delay ) >
                                             PRECISION * rb_pulser_w.timebase )
    {
        print( FATAL, "Position of defense pulse #%ld must always be %s.\n",
               pnum, rb_pulser_w_ptime( - p->function->delay ) );
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
rb_pulser_w_set_pulse_length( long   pnum,
                              double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->is_len )
    {
        print( FATAL, "Length of pulse #%ld has already been set to %s.\n",
               pnum, rb_pulser_w_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative length set for pulse #%ld: %s.\n",
               pnum, rb_pulser_w_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->len = rb_pulser_w_double2ticks( p_time );
    p->is_len = SET;

    if (    p->function == rb_pulser_w.function + PULSER_CHANNEL_DET
         && p->len > 1 )
    {
        print( SEVERE, "Length of DETECTION pulse can only be either 0 or "
               "%s. Setting it to the latter value.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        p->len = 1;
    }

    p->initial_len = p->len;
    p->initial_is_len = SET;

    return OK;
}


/*---------------------------------------------------------*
 * Function for setting the position change of a new pulse
 *--------------------------------------------------------*/

bool
rb_pulser_w_set_pulse_position_change( long   pnum,
                                       double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld has already been set "
               "to %s.\n", pnum, rb_pulser_w_ptime( p->dpos ) );
        THROW( EXCEPTION );
    }

    if ( p_time == 0.0 )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE )
    {
        print( FATAL, "Position change can't be set for defense pulse #%ld,\n",
               pnum );
        THROW( EXCEPTION );
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
rb_pulser_w_set_pulse_length_change( long   pnum,
                                     double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld has already been set to "
               "%s.\n", pnum, rb_pulser_w_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( rb_pulser_w_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = rb_pulser_w_double2ticks( p_time );
    p->is_dlen = SET;

    p->initial_dlen = p->dlen;
    p->initial_is_dlen = SET;

    return OK;
}


/*--------------------------------------------------------*
 * Function for assigning a phase sequence to a new pulse
 *--------------------------------------------------------*/

bool
rb_pulser_w_set_pulse_phase_cycle( long pnum,
                                   long cycle )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );
    Phs_Seq_T *pc = PA_Seq.phs_seq;
    int i;


    if (    p->function != NULL
         && p->function != rb_pulser_w.function + PULSER_CHANNEL_MW )
    {
        print( FATAL, "Pulses of function '%s' can't be phase-cycled with "
               "this device.\n",
               p->function->name );
        THROW( EXCEPTION );
    }

    if ( p->pc != NULL )
    {
        print( FATAL, "Pulse #%ld has already been assigned a phase cycle.\n",
               pnum );
        THROW( EXCEPTION );
    }

    while ( pc != NULL )
    {
        if ( pc->num == cycle )
            break;
        pc = pc->next;
    }

    if ( pc == NULL )
    {
        print( FATAL, "Referenced phase sequence #%ld hasn't been defined.\n",
               cycle );
        THROW( EXCEPTION );
    }

    for ( i = 0; i < pc->len; i++ )
        if ( pc->sequence[ i ] == PHASE_MINUS_Y )
            print( SEVERE, "Phase sequence for pulse #%ld requires "
                           "'-Y' phase which isn't available with this "
                           "setup.\n", pnum );

    p->pc = pc;

    rb_pulser_w.needs_phases = SET;
    rb_pulser_w.pc_len = pc->len;

    return OK;
}


/*------------------------------------------*
 * Function returns the function of a pulse
 *------------------------------------------*/

bool
rb_pulser_w_get_pulse_function( long  pnum,
                                int * function )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


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
rb_pulser_w_get_pulse_position( long     pnum,
                                double * p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( ! p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE )
        *p_time = - p->function->delay;
    else
        *p_time = p->pos;

    return OK;
}


/*----------------------------------------*
 * Function returns the length of a pulse
 *----------------------------------------*/

bool
rb_pulser_w_get_pulse_length( long     pnum,
                              double * p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( ! p->is_len )
    {
        /* Special treatment for the detection pulse: if its length isn't set
           the length defaults to the timebase of the pulser */

        if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DET )
            *p_time = rb_pulser_w_ticks2double( 1 );
        else
        {
            print( FATAL, "Length of pulse #%ld hasn't been set.\n", pnum );
            THROW( EXCEPTION );
        }
    }
    else
        *p_time = rb_pulser_w_ticks2double( p->len );

    return OK;
}


/*-------------------------------------------------*
 * Function returns the position change of a pulse
 *-------------------------------------------------*/

bool
rb_pulser_w_get_pulse_position_change( long     pnum,
                                       double * p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( ! p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE )
    {
        print( FATAL, "Defense pulse #%ld does not have a position change "
               "property.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = p->dpos;

    return OK;
}


/*-----------------------------------------------*
 * Function returns the length change of a pulse
 *-----------------------------------------------*/

bool
rb_pulser_w_get_pulse_length_change( long     pnum,
                                     double * p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( ! p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = rb_pulser_w_ticks2double( p->dlen );

    return OK;
}


/*-----------------------------------------------------------*
 * Function returns the phase sequence assigned to the pulse
 *-----------------------------------------------------------*/

bool
rb_pulser_w_get_pulse_phase_cycle( long   pnum,
                                   long * cycle )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->pc == NULL )
    {
        print( FATAL, "No phase cycle has been set for pulse #%ld.\n", pnum );
        THROW( EXCEPTION );
    }

    *cycle = p->pc->num;

    return OK;
}


/*------------------------------------------------------------------*
 * Function to change the position of a pulse during the experiment
 *------------------------------------------------------------------*/

bool
rb_pulser_w_change_pulse_position( long   pnum,
                                   double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );
    double new_pos = 0.0;


    CLOBBER_PROTECT( new_pos );

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, rb_pulser_w_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE )
    {
        print( FATAL, "Position of defense pulse #%ld can't be changed, it's "
               "always %s.\n", pnum,
               rb_pulser_w_ptime( - p->function->delay ) );
        THROW( EXCEPTION );
    }

    if (    p->is_pos
         && fabs( p_time - p->pos ) <= PRECISION * rb_pulser_w.timebase )
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
rb_pulser_w_change_pulse_length( long   pnum,
                                 double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );
    Ticks new_len = 0;


    CLOBBER_PROTECT( new_len );

    if ( p_time < 0 )
    {
        print( FATAL, "Invalid (negative) length for pulse #%ld: %s.\n",
               pnum, rb_pulser_w_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_len = rb_pulser_w_double2ticks( p_time );
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

    if ( p->is_len && p->len == new_len )
    {
        print( WARN, "Old and new length of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    p->len = new_len;
    p->is_len = SET;

    if (    p->function == rb_pulser_w.function + PULSER_CHANNEL_DET
         && p->len > 1 )
    {
        print( SEVERE, "Length of DETECTION pulse can only be either 0 or "
               "%s. Setting it to the latter value.\n",
               rb_pulser_w_ptime( rb_pulser_w.timebase ) );
        p->len = 1;
    }

    p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );

    return OK;
}


/*-------------------------------------------------------------------------*
 * Function to change the position change of a pulse during the experiment
 *-------------------------------------------------------------------------*/

bool
rb_pulser_w_change_pulse_position_change( long   pnum,
                                          double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );


    if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE )
    {
        print( FATAL, "Position change of defense pulse #%ld can't be set.\n",
               pnum );
        THROW( EXCEPTION );
    }

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
rb_pulser_w_change_pulse_length_change( long   pnum,
                                        double p_time )
{
    Pulse_T *p = rb_pulser_w_get_pulse( pnum );
    Ticks new_dlen = 0;


    CLOBBER_PROTECT( new_dlen );

    TRY
    {
        new_dlen = rb_pulser_w_double2ticks( p_time );
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

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
