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


#include "tegam2714a_p.h"


/*--------------------------------------------------------*
 * Function gets called when a new pulse is to be created
 *--------------------------------------------------------*/

bool
tegam2714a_p_new_pulse( long pnum )
{
    Pulse_T *cp = tegam2714a_p.pulses;
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
        tegam2714a_p.pulses = cp;
    else
        lp->next = cp;

    cp->prev = lp;
    cp->next = NULL;
    cp->num = pnum;

    if ( tegam2714a_p.function.self != PULSER_CHANNEL_NO_TYPE )
    {
        cp->function = &tegam2714a_p.function;
        cp->is_function = SET;
    }
    else
    {
        cp->function = NULL;
        cp->is_function = UNSET;
    }

    cp->is_pos = cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
    cp->initial_is_pos = cp->initial_is_len = cp->initial_is_dpos
                       = cp->initial_is_dlen = UNSET;

    cp->has_been_active = UNSET;

    return OK;
}


/*---------------------------------------------*
 * Function to set the function of a new pulse
 *---------------------------------------------*/

bool
tegam2714a_p_set_pulse_function( long pnum,
                                 int  function )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( tegam2714a_p.function.self != PULSER_CHANNEL_NO_TYPE )
    {
        if ( tegam2714a_p.function.self != function )
        {
            print( FATAL, "Pulse function '%s' can't be used since the only "
                   "channel of the device has already has been assigned to "
                   "function '%s'.\n", Function_Names[ function ],
                   tegam2714a_p.function.name );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( p->function != NULL )
        {
            print( FATAL, "The function of pulse #%ld has already been set to "
                   "'%s'.\n", pnum, p->function->name );
            THROW( EXCEPTION );
        }

        p->function = &tegam2714a_p.function;
        p->is_function = SET;

        tegam2714a_p.function.is_used = SET;
        tegam2714a_p.function.self = function;
        tegam2714a_p.function.name = Function_Names[ function ];
    }


    return OK;
}


/*--------------------------------------------------*
 * Function for setting the position of a new pulse
 *--------------------------------------------------*/

bool
tegam2714a_p_set_pulse_position( long   pnum,
                                 double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld has already been set "
               "to %s.\n", pnum, tegam2714a_p_ptime( p->pos ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0 )
    {
        print( FATAL, "Invalid negative position set for pulse #%ld: %s.\n",
               pnum, tegam2714a_p_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->pos = p->initial_pos = tegam2714a_p_double2ticks( p_time );
    p->is_pos = p->initial_is_pos = SET;

    return OK;
}


/*------------------------------------------------*
 * Function for setting the length of a new pulse
 *------------------------------------------------*/

bool
tegam2714a_p_set_pulse_length( long   pnum,
                               double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( p->is_len )
    {
        print( FATAL, "Length of pulse #%ld has already been set to %s.\n",
               pnum, tegam2714a_p_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid negative length set for pulse #%ld: %s.\n",
               pnum, tegam2714a_p_ptime( p_time ) );
        THROW( EXCEPTION );
    }

    p->len = p->initial_len = tegam2714a_p_double2ticks( p_time );
    p->is_len = p->initial_is_len = SET;

    return OK;
}


/*---------------------------------------------------------*
 * Function for setting the position change of a new pulse
 *---------------------------------------------------------*/

bool
tegam2714a_p_set_pulse_position_change( long   pnum,
                                        double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld has already been set "
               "to %s.\n", pnum, tegam2714a_p_ptime( p->dpos ) );
        THROW( EXCEPTION );
    }

    p->dpos = p->initial_dpos = tegam2714a_p_double2ticks( p_time );

    if ( p->dpos == 0 )
    {
        print( SEVERE, "Zero position change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->initial_is_dpos = p->is_dpos = SET;

    return OK;
}


/*-------------------------------------------------------*
 * Function for setting the length change of a new pulse
 *-------------------------------------------------------*/

bool
tegam2714a_p_set_pulse_length_change( long   pnum,
                                      double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld has already been set to "
               "%s.\n", pnum, tegam2714a_p_pticks( p->len ) );
        THROW( EXCEPTION );
    }

    p->dlen = p->initial_dlen = tegam2714a_p_double2ticks( p_time );

    if ( p->dlen == 0 )
    {
        print( SEVERE, "Zero length change value for pulse #%ld.\n", pnum );
        return FAIL;
    }

    p->is_dlen = p->initial_is_dlen = SET;

    return OK;
}


/*------------------------------------------*
 * Function returns the function of a pulse
 *------------------------------------------*/

bool
tegam2714a_p_get_pulse_function( long  pnum,
                                 int * function )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


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
tegam2714a_p_get_pulse_position( long     pnum,
                                 double * p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( ! p->is_pos )
    {
        print( FATAL, "The start position of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = tegam2714a_p_ticks2double( p->pos );

    return OK;
}


/*----------------------------------------*
 * Function returns the length of a pulse
 *----------------------------------------*/

bool
tegam2714a_p_get_pulse_length( long     pnum,
                               double * p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( ! p->is_len )
    {
        print( FATAL, "Length of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = tegam2714a_p_ticks2double( p->len );

    return OK;
}


/*-------------------------------------------------*
 * Function returns the position change of a pulse
 *-------------------------------------------------*/

bool
tegam2714a_p_get_pulse_position_change( long     pnum,
                                        double * p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( ! p->is_dpos )
    {
        print( FATAL, "The position change of pulse #%ld hasn't been set.\n",
               pnum );
        THROW( EXCEPTION );
    }

    *p_time = tegam2714a_p_ticks2double( p->dpos );

    return OK;
}


/*-----------------------------------------------*
 * Function returns the length change of a pulse
 *-----------------------------------------------*/

bool
tegam2714a_p_get_pulse_length_change( long     pnum,
                                      double * p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );


    if ( ! p->is_dlen )
    {
        print( FATAL, "Length change of pulse #%ld hasn't been set.\n", pnum );
        THROW( EXCEPTION );
    }

    *p_time = tegam2714a_p_ticks2double( p->dlen );

    return OK;
}


/*------------------------------------------------------------------*
 * Function to change the position of a pulse during the experiment
 *------------------------------------------------------------------*/

bool
tegam2714a_p_change_pulse_position( long   pnum,
                                    double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );
    Ticks new_pos;


    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid (negative) start position for pulse #%ld: "
               "%s.\n", pnum, tegam2714a_p_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_pos = tegam2714a_p_double2ticks( p_time );
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

    if ( p->is_pos && p->pos == new_pos )
    {
        print( WARN, "Old and new position of pulse #%ld are identical.\n",
               pnum );
        return OK;
    }

    p->pos = new_pos;
    p->is_pos = SET;

    return OK;
}


/*----------------------------------------------------------------*
 * Function to change the length of a pulse during the experiment
 *----------------------------------------------------------------*/

bool
tegam2714a_p_change_pulse_length( long   pnum,
                                  double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );
    Ticks new_len = 0;


    CLOBBER_PROTECT( new_len );

    if ( p_time < 0.0 )
    {
        print( FATAL, "Invalid (negative) length for pulse #%ld: %s.\n",
               pnum, tegam2714a_p_ptime( p_time ) );
        if ( FSC2_MODE == EXPERIMENT )
            return FAIL;
        else
            THROW( EXCEPTION );
    }

    TRY
    {
        new_len = tegam2714a_p_double2ticks( p_time );
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
tegam2714a_p_change_pulse_position_change( long   pnum,
                                           double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );
    Ticks new_dpos;


    TRY
    {
        new_dpos = tegam2714a_p_double2ticks( p_time );
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

    if ( p->is_dpos && p->dpos == new_dpos )
    {
        print( WARN, "Old and new positon change of pulse #%ld are "
               "identical.\n", pnum );
        return OK;
    }

    p->dpos = new_dpos;
    p->is_dpos = SET;

    return OK;
}


/*-----------------------------------------------------------------------*
 * Function to change the length change of a pulse during the experiment
 *-----------------------------------------------------------------------*/

bool
tegam2714a_p_change_pulse_length_change( long   pnum,
                                         double p_time )
{
    Pulse_T *p = tegam2714a_p_get_pulse( pnum );
    Ticks new_dlen = 0;


    CLOBBER_PROTECT( new_dlen );

    TRY
    {
        new_dlen = tegam2714a_p_double2ticks( p_time );
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

    if ( p->is_dlen && p->dlen == new_dlen )
    {
        print( WARN, "Old and new length change of pulse #%ld are "
               "identical.\n", pnum );
        return OK;
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
