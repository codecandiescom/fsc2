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


#include "dg2020_b.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_new_pulse( long pnum )
{
    Pulse_T *cp = dg2020.pulses;
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

    if ( dg2020.pulses == NULL )
    {
        dg2020.pulses = cp;
        cp->prev = NULL;
    }
    else
    {
        cp->prev = lp;
        lp->next = cp;
    }

    cp->next = NULL;
    cp->pc = NULL;

    cp->num = pnum;
    cp->is_function = UNSET;

    cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
    cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
        = cp->initial_is_dlen = UNSET;
    cp->is_old_pos = cp->is_old_len = UNSET;

    cp->channel = NULL;

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


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_function( long pnum,
                           int  function )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if (    function == PULSER_CHANNEL_PHASE_1
         || function == PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "Phase functions can't be used with this driver.\n" );
        THROW( EXCEPTION );
    }

    if ( p->is_function )
    {
        print( FATAL, "The function of pulse #%ld has already been set to "
               "'%s'.\n", pnum, p->function->name );
        THROW( EXCEPTION );
    }

    if ( dg2020.function[ function ].num_pods == 0 )
    {
        print( FATAL, "No pods have been assigned to function '%s' needed "
               "for pulse #%ld.\n", dg2020.function[ function ].name, pnum );
        THROW( EXCEPTION );
    }

    if ( p->is_pos && p->pos + p->function->delay < 0 )
    {
        print( FATAL, "Start position for pulse #%ld is nagtive.\n", pnum );
        THROW( EXCEPTION );
    }

    p->function = dg2020.function + function;
    p->is_function = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_position( long   pnum,
                           double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld has already been set "
               "to %s.\n", pnum, dg2020_pticks( p->pos ) );
        THROW( EXCEPTION );
    }

    if (    p->is_function
         && p_time + p->function->delay * dg2020.timebase < 0.0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, dg2020_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->pos = dg2020_double2ticks( p_time );
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


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_length( long   pnum,
                         double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( p->is_len )
    {
        print( FATAL, "Length of pulse #%ld has already been set to %s.\n",
               pnum, dg2020_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative length set for pulse #%ld: %s.\n",
               pnum, dg2020_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->len = dg2020_double2ticks( p_time );
    p->is_len = SET;

    if ( ! p->initial_is_len && FSC2_MODE == PREPARATION )
    {
        p->initial_len = dg2020_double2ticks( p_time );
        p->initial_is_len = SET;
    }
    else if ( ! p->is_old_len )
    {
        p->old_len = p->len;
        p->is_old_len = SET;
    }

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_position_change( long   pnum,
                                  double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld has already been set "
               "to %s.\n", pnum, dg2020_pticks( p->dpos ) );
        THROW( EXCEPTION );
    }

    if ( dg2020_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = dg2020_double2ticks( p_time );
    p->is_dpos = SET;

    if ( ! p->initial_is_dpos && FSC2_MODE == PREPARATION )
    {
        p->initial_dpos = dg2020_double2ticks( p_time );
        p->initial_is_dpos = SET;
    }

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_length_change( long   pnum,
                                double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld has already been set to "
               "%s.\n", pnum, dg2020_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( dg2020_double2ticks( p_time ) == 0 )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dlen = dg2020_double2ticks( p_time );
    p->is_dlen = SET;

    if ( ! p->initial_is_dlen && FSC2_MODE == PREPARATION )
    {
        p->initial_dlen = dg2020_double2ticks( p_time );
        p->initial_is_dlen = SET;
    }

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_set_pulse_phase_cycle( long pnum,
                              long cycle )
{
    Pulse_T *p = dg2020_get_pulse( pnum );
    Phs_Seq_T *pc = PA_Seq.phs_seq;


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
        print( FATAL, "Referenced phase sequence %ld hasn't been defined.\n",
               cycle );
        THROW( EXCEPTION );
    }

    p->pc = pc;
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_function( long  pnum,
                           int * function )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( ! p->is_function )
    {
        print( FATAL, "The function of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *function = p->function->self;
    return OK;
}

/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_position( long     pnum,
                           double * p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( ! p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = dg2020_ticks2double( p->pos );
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_length( long     pnum,
                         double * p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( ! p->is_len )
    {
        print( FATAL, "Length of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = dg2020_ticks2double( p->len );
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_position_change( long     pnum,
                                  double * p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( ! p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = dg2020_ticks2double( p->dpos );
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_length_change( long     pnum,
                                double * p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( ! p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = dg2020_ticks2double( p->dlen );
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_get_pulse_phase_cycle( long  pnum,
                              long *cycle )
{
    Pulse_T *p = dg2020_get_pulse( pnum );


    if ( p->pc == NULL )
    {
        print( FATAL, "No phase cycle has been set for pulse #%ld.\n", pnum );
        THROW( EXCEPTION );
    }

    *cycle = p->pc->num;
    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_change_pulse_position( long   pnum,
                              double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );
    Ticks volatile new_pos = 0;


    if ( p_time + p->function->delay * dg2020.timebase < 0.0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, dg2020_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_pos = dg2020_double2ticks( p_time );
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
        p->sp->pos = p->pos;
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
        dg2020.needs_update = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_change_pulse_length( long   pnum,
                            double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );
    Ticks volatile new_len = 0;


    if ( p_time < 0 )
    {
        print( FATAL, "Invalid (negative) length for pulse #%ld: %s.\n",
               pnum, dg2020_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_len = dg2020_double2ticks( p_time );
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
        p->sp->len = p->len;
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
        dg2020.needs_update = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_change_pulse_position_change( long   pnum,
                                     double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );
    Ticks volatile new_dpos = 0;


    TRY
    {
        new_dpos = dg2020_double2ticks( p_time );
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

    if ( new_dpos == 0 && FSC2_MODE == TEST )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->dpos = new_dpos;
    p->is_dpos = SET;

    return OK;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
dg2020_change_pulse_length_change( long   pnum,
                                   double p_time )
{
    Pulse_T *p = dg2020_get_pulse( pnum );
    Ticks volatile new_dlen = 0;


    TRY
    {
        new_dlen = dg2020_double2ticks( p_time );
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
