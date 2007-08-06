/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


static void ep385_init_print( FILE * fp );

static void ep385_basic_pulse_check( void );

static void ep385_create_shape_pulses( void );

static void ep385_create_twt_pulses( void );

static void ep385_basic_functions_check( void );

static void ep385_create_phase_matrix( Function_T * f );

static void ep385_setup_channels( void );

static void ep385_pulse_start_setup( void );

static void ep385_channel_start_check( Channel_T * ch );

static void ep385_pulse_init_check( Function_T * f );

static void ep385_defense_shape_init_check( Function_T * shape );


/*-----------------------------------------------------------------*
 * Function does everything that needs to be done for checking and
 * completing the internal representation of the pulser at the
 * start of a test run.
 *-----------------------------------------------------------------*/

void ep385_init_setup( void )
{
    Function_T *f;
    int i, j;


    TRY
    {
        ep385_basic_pulse_check( );
        ep385_create_shape_pulses( );
        ep385_create_twt_pulses( );
        ep385_basic_functions_check( );

        ep385_init_print( ep385.dump_file );
        ep385_init_print( ep385.show_file );

        ep385_setup_channels( );
        ep385_pulse_start_setup( );

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        {
            f = ep385.function + i;
            if (    ( ! f->is_used && f->num_channels == 0 )
                 || i == PULSER_CHANNEL_PHASE_1
                 || i == PULSER_CHANNEL_PHASE_2 )
                continue;

            if ( f->max_seq_len > MAX_PULSER_BITS )
            {
                print( FATAL, "Pulse sequence for function '%s' is too "
                       "long.\n", f->name );
                THROW( EXCEPTION );
            }
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        /* Free memory already allocated when the error happend */

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        {
            f = ep385.function + i;

            if ( f->pulses != NULL )
                f->pulses = PULSE_PP T_free( f->pulses );

            if ( f->pm != NULL )
            {
                for ( j = 0; j < f->pc_len * f->num_channels; j++ )
                    T_free( f->pm[ j ] );
                f->pm = PULSE_PPP T_free( f->pm );
            }
        }

        for ( i = 0; i < MAX_CHANNELS; i++ )
            ep385.channel[ i ].pulse_params =
                    PULSE_PARAMS_P T_free( ep385.channel[ i ].pulse_params );

        RETHROW( );
    }

    if ( ep385.dump_file != NULL )
        ep385_dump_channels( ep385.dump_file );

    if ( ep385.show_file != NULL )
        ep385_dump_channels( ep385.show_file );
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_init_print( FILE * fp )
{
    Function_T *f;
    int i, j;


    if ( fp == NULL )
        return;

    fprintf( fp, "TB: %g\nD: %ld\nPC:", ep385.timebase, ep385.neg_delay );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        if ( ep385.function[ i ].phase_setup != NULL )
            fprintf( fp, " %s", Function_Names[ i ] );

    fprintf( fp, "\n===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;

        if ( ! f->is_needed && f->num_channels == 0 )
            continue;

        for ( j = 0; j < f->num_channels; j++ )
            fprintf( fp, "%s:%d %ld\n",
                     f->name, f->channel[ j ]->self, f->delay );
    }
}


/*--------------------------------------------------------------------------*
 * Function checks for all pulses that they have an initialized function,
 * sets the channel(s) for the pulses and checks all other pulse parameters
 *--------------------------------------------------------------------------*/

static void ep385_basic_pulse_check( void )
{
    Pulse_T *p;
    Function_T *f;


    for ( p = ep385.pulses; p != NULL; p = p->next )
    {
        p->is_active = SET;

        /* Check the pulse function */

        if ( ! p->is_function )
        {
            print( FATAL, "Pulse #%ld is not associated with a function.\n",
                   p->num );
            THROW( EXCEPTION );
        }

        f = p->function;

        /* Check that there's at least one channel associated with the
           pulses function */

        if ( f->num_channels == 0 )
        {
            print( FATAL, "No channel has been set for function '%s' used for "
                   "pulse #%ld.\n", p->function->name, p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_pos || ! p->is_len || p->len == 0 )
            p->is_active = UNSET;

        if ( p->is_active )
            p->was_active = p->has_been_active = SET;
    }
}


/*--------------------------------------------------------------------------*
 * The function automatically adds shape pulses for all pulses of functions
 * that have been marked by a call of pulser_automatic_shape_pulses() for
 * the automatic creation of shape pulses. Each pulse for which a shape
 * pulse is created is linked with its shape pulse (and also the other way
 * round) by a pointer, 'sp', in the pulse structure. When done with
 * creating shape pulses we still have to check that these automatically
 * created pulses don't overlap with manually set shape pulses or with each
 * other, which would beat the purpose of shape pulses.
 *--------------------------------------------------------------------------*/

static void ep385_create_shape_pulses( void )
{
    Function_T *f;
    Pulse_T *np = NULL, *cp, *rp, *p1, *p2, *old_end;


    if ( ! ep385.auto_shape_pulses || ep385.pulses == NULL )
        return;

    /* Find the end of the pulse list (to be able to add further shape
       pulses) */

    for ( cp = ep385.pulses; cp->next != NULL; cp = cp->next )
        /* empty */ ;
    old_end = cp;

    /* Loop over all pulses */

    for ( rp = ep385.pulses; rp != NULL; rp = rp->next )
    {
        f = rp->function;

        /* No shape pulses can be set for the PULSE_SHAPE function itself
           and functions that don't need shape pulses */

        if (    f->self == PULSER_CHANNEL_PULSE_SHAPE
             || ! f->uses_auto_shape_pulses )
            continue;

        np = PULSE_P T_malloc( sizeof *np );

        np->prev = cp;
        cp = cp->next = np;

        np->next = NULL;
        np->pc = NULL;

        np->function = ep385.function + PULSER_CHANNEL_PULSE_SHAPE;
        np->is_function = SET;

        /* These 'artifical' pulses get negative numbers */

        np->num = ( np->prev->num >= 0 ) ? -1 : np->prev->num - 1;

        np->pc = NULL;

        rp->sp = np;
        np->sp = rp;

        np->tp = NULL;

        /* The remaining properties are just exact copies of the
           pulse the shape pulse has to be used with */

        np->is_active = rp->is_active;
        np->was_active = rp->was_active;
        np->has_been_active = rp->has_been_active;

        np->pos = rp->pos;
        np->len = rp->len;
        np->dpos = rp->dpos;
        np->dlen = rp->dlen;

        np->is_pos = rp->is_pos;
        np->is_len = rp->is_len;
        np->is_dpos = rp->is_dpos;
        np->is_dlen = rp->is_dlen;

        np->initial_pos = rp->initial_pos;
        np->initial_len = rp->initial_len;
        np->initial_dpos = rp->initial_dpos;
        np->initial_dlen = rp->initial_dlen;

        np->initial_is_pos = rp->initial_is_pos;
        np->initial_is_len = rp->initial_is_len;
        np->initial_is_dpos = rp->initial_is_dpos;
        np->initial_is_dlen = rp->initial_is_dlen;

        np->old_pos = rp->old_pos;
        np->old_len = rp->old_len;

        np->is_old_pos = rp->is_old_pos;
        np->is_old_len = rp->is_old_len;

        np->needs_update = rp->needs_update;
    }

    if ( np != NULL )
        ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].is_needed = SET;
    else             /* no shape pulses have been created automatically */
        return;

    /* Now after we created all the necessary shape pulses we've got to check
       that they don't overlap with manually created shape pulses or with
       shape pulses for pulses of different functions (overlaps of shape
       pulses for pulses of the same function will be detected later and
       reported as overlaps of the pulses they belong to, which is the only
       reason this could happen). */

    for ( p1 = ep385.pulses; p1 != old_end->next; p1 = p1->next )
    {
        if (    ! p1->is_active
             || p1->function->self != PULSER_CHANNEL_PULSE_SHAPE )
            continue;

        for ( p2 = old_end->next; p2 != NULL; p2 = p2->next )
        {
            if (    ! p2->is_active
                 || p2->function->self != PULSER_CHANNEL_PULSE_SHAPE )
                continue;

            if (    p1->pos == p2->pos
                 || ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos )
                 || ( p1->pos > p2->pos && p1->pos < p2->pos + p2->pos ) )
            {
                print( FATAL, "PULSE_SHAPE pulse #%ld and automatically "
                       "created shape pulse for pulse #%ld (function '%s') "
                       "overlap.\n", p1->num, p2->sp->num,
                       p2->sp->function->name );
                THROW( EXCEPTION );
            }
        }
    }

    for ( p1 = old_end->next; p1 != NULL && p1->next != NULL; p1 = p1->next )
    {
        if ( ! p1->is_active )
            continue;

        for ( p2 = p1->next; p2 != NULL; p2 = p2->next )
        {
            if ( ! p2->is_active )
                continue;

            /* An overlap of automtically created shape pulses for pulses of
               the same funtion can only happen if already the pulses they
               were created for overlap. */

            if ( p1->sp->function == p2->sp->function )
                continue;

            if (    p1->pos == p2->pos
                 || ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos )
                 || ( p1->pos > p2->pos && p1->pos < p2->pos + p2->pos ) )
            {
                print( FATAL, "Automatically created shape pulses for pulse "
                       "#%ld (function '%s') and #%ld (function '%s') "
                       "overlap.\n", p1->sp->num, p1->sp->function->name,
                       p2->sp->num, p2->sp->function->name );
                THROW( EXCEPTION );
            }
        }
    }
}


/*------------------------------------------------------------------------*
 * The function automatically adds TWT pulses for all pulses of functions
 * that have been marked by a call of pulser_automatic_twt_pulses() for
 * the automatic creation of TWT pulses. In contrast to automatic shape
 * pulses the automatic created TWT pulses may overlap, these overlaps
 * will be taken care of later.
 *------------------------------------------------------------------------*/

static void ep385_create_twt_pulses( void )
{
    Function_T *f;
    Pulse_T *np = NULL, *cp, *rp, *old_end;


    if ( ! ep385.auto_twt_pulses || ep385.pulses == NULL )
        return;

    /* Find the end of the pulse list (to be able to add further TWT
       pulses) */

    for ( cp = ep385.pulses; cp->next != NULL; cp = cp->next )
        /* empty */ ;
    old_end = cp;

    /* Loop over all pulses */

    for ( rp = ep385.pulses; rp != NULL; rp = rp->next )
    {
        f = rp->function;

        /* No TWT pulses can be set for the TWT or TWT_GATE function and
           functions that don't need TWT pulses */

        if (    f->self == PULSER_CHANNEL_TWT
             || f->self == PULSER_CHANNEL_TWT_GATE
             || ! f->uses_auto_twt_pulses )
            continue;

        np = PULSE_P T_malloc( sizeof *np );

        np->prev = cp;
        cp = cp->next = np;

        np->next = NULL;
        np->pc = NULL;

        np->function = ep385.function + PULSER_CHANNEL_TWT;
        np->is_function = SET;

        /* These 'artifical' pulses get negative numbers */

        np->num = ( np->prev->num >= 0 ) ? -1 : np->prev->num - 1;

        np->pc = NULL;

        rp->tp = np;
        np->tp = rp;

        np->sp = NULL;

        /* The remaining properties are just exact copies of the
           pulse the shape pulse has to be used with */

        np->is_active = rp->is_active;
        np->was_active = rp->was_active;
        np->has_been_active = rp->has_been_active;

        np->pos = rp->pos;
        np->len = rp->len;
        np->dpos = rp->dpos;
        np->dlen = rp->dlen;

        np->is_pos = rp->is_pos;
        np->is_len = rp->is_len;
        np->is_dpos = rp->is_dpos;
        np->is_dlen = rp->is_dlen;

        np->initial_pos = rp->initial_pos;
        np->initial_len = rp->initial_len;
        np->initial_dpos = rp->initial_dpos;
        np->initial_dlen = rp->initial_dlen;

        np->initial_is_pos = rp->initial_is_pos;
        np->initial_is_len = rp->initial_is_len;
        np->initial_is_dpos = rp->initial_is_dpos;
        np->initial_is_dlen = rp->initial_is_dlen;

        np->old_pos = rp->old_pos;
        np->old_len = rp->old_len;

        np->is_old_pos = rp->is_old_pos;
        np->is_old_len = rp->is_old_len;

        np->needs_update = rp->needs_update;
    }

    if ( np != NULL )
        ep385.function[ PULSER_CHANNEL_TWT ].is_needed = SET;
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_basic_functions_check( void )
{
    Function_T *f;
    Pulse_T *p;
    int i;
    Ticks delay;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;

        /* Don't do anything if the function has never been mentioned */

        if ( ! f->is_used )
            continue;

        /* Phase functions are not supported in this driver... */

        fsc2_assert(    i != PULSER_CHANNEL_PHASE_1
                     && i != PULSER_CHANNEL_PHASE_2 );

        /* Check if the function has pulses assigned to it */

        if ( ! f->is_needed )
        {
            print( WARN, "No pulses have been assigned to function '%s'.\n",
                   Function_Names[ i ] );
            continue;
        }

        /* Assemble a list of all pulses assigned to the function and
           determine the length of the phase cycle for the function */

        f->num_pulses = 0;
        f->pc_len = 1;

        for ( p = ep385.pulses; p != NULL; p = p->next )
        {
            if ( p->function != f )
                continue;

            f->num_pulses++;
            f->pulses = PULSE_PP T_realloc( f->pulses,
                                           f->num_pulses * sizeof *f->pulses );
            f->pulses[ f->num_pulses - 1 ] = p;

            if ( p->pc != NULL )
                f->pc_len = i_max( f->pc_len, p->pc->len );
        }
    }

    if ( ep385.neg_delay )
    {
        delay = MAX_PULSER_BITS;

        for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
            if (    ep385.function[ i ].is_used
                 && delay > ep385.function[ i ].delay )
                delay = ep385.function[ i ].delay;
        if ( delay != 0 )
            for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
                ep385.function[ i ].delay -= delay;
    }

    /* Create a matrix of pulse lists that contain the pulses for each
       combination of phase and channel */

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        if ( ep385.function[ i ].is_needed )
        {
            ep385_create_phase_matrix( ep385.function + i );

            /* Warn the user if there is more than one channel assigned to the
               function but there's no PHASE_SETUP that we're going to use
               only the very first channel for creating pulses */

            if (    ep385.function[ i ].phase_setup == NULL
                 && ep385.function[ i ].num_channels > 1 )
                print( NO_ERROR, "Using only channel %d for %s pulses.\n",
                       ep385.function[ i ].channel[ 0 ]->self,
                       ep385.function[ i ].name );
        }
}


/*-----------------------------------------------------------------------*
 * Function creates a matrix of pulse lists for each phase (columns) and
 * each channel (rows) of the function (in case of functions not set up
 * for phase cycling this will be just a 1x1 matrix). The pulse lists
 * are unsorted, i.e. when using the pulse lists we still have to sort
 * them according to their start times and check if they are switched on
 * and don't overlap.
 *-----------------------------------------------------------------------*/

static void ep385_create_phase_matrix( Function_T * f )
{
    Pulse_T *p;
    Pulse_T **pm_elem;
    int phase_type;
    Channel_T *ch;
    int i, j, k, l, m;


    f->pm = PULSE_PPP T_malloc( f->pc_len * f->num_channels * sizeof *f->pm );

    for ( j = 0; j < f->pc_len * f->num_channels; j++ )
        f->pm[ j ] = NULL;

    for ( j = 0; j < f->pc_len * f->num_channels; j++ )
    {
        f->pm[ j ] = PULSE_PP T_malloc(   ( f->num_pulses + 1 )
                                        * sizeof **f->pm );
        *f->pm[ j ] = NULL;         /* list is still empty */
    }

    for ( i = 0; i < f->num_pulses; i++ )
    {
        p = f->pulses[ i ];

        /* For pulses without phase cycling we always use the +x-phase */

        if ( p->pc == NULL )
        {
            /* Find the index of the channel for +x-phase in the list of
               channels of the function (if there is no phase setup for
               the function this is automatically the only channel of the
               function) */

            if ( f->phase_setup != NULL )
            {
                /* Check that a +x-phase channel has been set for the
                   function */

                if ( ! f->phase_setup->is_set[ PHASE_PLUS_X ] )
                {
                    print( FATAL, "Phase type '%s' is needed for pulse #%ld "
                           "but it hasn't been not defined in a PHASE_SETUP "
                           "command for its function '%s'.\n",
                           Phase_Types[ PHASE_PLUS_X ], p->num, f->name );
                    THROW( EXCEPTION );
                }

                f->phase_setup->is_needed[ PHASE_PLUS_X ] = SET;

                for ( l = 0; l < f->num_channels; l++ )
                    if ( f->phase_setup->channels[ PHASE_PLUS_X ] ==
                         f->channel[ l ] )
                        break;

                fsc2_assert( l < f->num_channels );
            }
            else
                l = 0;

            /* For each of the phase rows add the pulse to the lists
               of pulses of the +x-channels (i.e. the l-th row)*/

            for ( k = 0; k < f->pc_len; k++ )
            {
                pm_elem = f->pm[ k * f->num_channels + l ];

                /* Find the end of the current pulse list, indicated
                   by a NULL pointer */

                for ( m = 0; m < f->num_pulses && pm_elem[ m ] != NULL; m++ )
                    /* empty */ ;

                fsc2_assert( m < f->num_pulses );

                pm_elem[ m ] = p;
                pm_elem[ m + 1 ] = NULL;
            }

            continue;
        }

        /* Now comes the more complicated case: pulses with phase cycling.
           We loop over all phases of the pulse, i.e. all rows of the phase
           matrix. */

        for ( k = 0; k < p->pc->len; k++ )
        {
            /* Figure out the current phase of the pulse */

            phase_type = p->pc->sequence[ k ];

            /* Check that a phase setup for this phase has happended */

            if ( ! f->phase_setup->is_set[ phase_type ] )
            {
                print( FATAL, "Phase type '%s' is needed for pulse #%ld "
                       "but it hasn't been not defined in a PHASE_SETUP "
                       "command for its function '%s'.\n",
                       Phase_Types[ phase_type ], p->num, f->name );
                THROW( EXCEPTION );
            }

            /* Figure out in which row the pulse must be appended to
               the pulse list */

            ch = f->phase_setup->channels[ phase_type ];
            for ( l = 0; l < f->num_channels; l++ )
                if ( ch == f->channel[ l ] )
                    break;

            fsc2_assert( l < f->num_channels );

            pm_elem = f->pm[ k * f->num_channels + l ];

            /* Find the end of the current pulse list, indicated by a NULL
               pointer */

            for ( m = 0; m < f->num_pulses &&  pm_elem[ m ] != NULL; m++ )
                /* empty */ ;

            fsc2_assert( m < f->num_pulses );

            /* Append the pulse to the current pulse list */

            pm_elem[ m ] = p;
            pm_elem[ m + 1 ] = NULL;
        }
    }
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_setup_channels( void )
{
    Channel_T *ch;
    Function_T *f;
    Pulse_T **pm_elem;
    int i, j, k, l, m;


    for ( i = 0; i < MAX_CHANNELS; i++ )
    {
        ch = ep385.channel + i;

        if ( ( f = ch->function ) == NULL )
            continue;

        /* Count the maximum number of pulses that could be appear in the
           channel by evaluating the phase matrix of the functions the
           channel is associated with */

        for ( l = 0; l < f->num_channels; l++ )
            if ( ch == f->channel[ l ] )
                break;

        fsc2_assert( l < f->num_channels );

        for ( k = 0; k < f->pc_len; k++ )
        {
            /* Get phase matrix entry for current phase */

            pm_elem = f->pm[ k * f->num_channels + l ];

            /* Count number of pulses for current phase */

            for ( m = 0; m < f->num_pulses && pm_elem[ m ] != NULL; m++ )
                /* empty */ ;

            ch->num_pulses = i_max( ch->num_pulses, m );
        }

        /* Create two lists of pulse parameters for the channel */

        if ( ch->num_pulses > 0 )
        {
            ch->pulse_params =
                PULSE_PARAMS_P T_malloc( 2 * ch->num_pulses *
                                         sizeof *ch->pulse_params );
            ch->old_pulse_params = ch->pulse_params + ch->num_pulses;

            for ( j = 0; j < ch->num_pulses; j++ )
            {
                ch->pulse_params->pos = ch->pulse_params->len =
                    ch->old_pulse_params->pos = ch->old_pulse_params->len = 0;
                ch->pulse_params->pulse = ch->old_pulse_params->pulse = NULL;
            }
        }
        else
            ch->pulse_params = ch->old_pulse_params = NULL;
    }
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_pulse_start_setup( void )
{
    Function_T *f;
    Channel_T *ch;
    Pulse_T **pm_elem;
    Pulse_T *p;
    int i, j, m;
    Pulse_Params_T *pp, *opp;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;

        /* Nothing to be done for unused functions and the phase functions */

        if (    ( ! f->is_used && f->num_channels == 0 )
             || i == PULSER_CHANNEL_PHASE_1
             || i == PULSER_CHANNEL_PHASE_2 )
            continue;

        ep385_pulse_init_check( f );

        /* Run over all channels associated with the current function and
           set the pulse pointers, positions and lengths for the current
           phase */

        for ( j = 0; j < f->num_channels; j++ )
        {
            ch = f->channel[ j ];

            ch->needs_update = SET;

            if ( ch->num_pulses == 0 )
            {
                ch->num_active_pulses = ch->old_num_active_pulses = 0;
                continue;
            }

            pm_elem = f->pm[ f->next_phase * f->num_channels + j ];

            ch->num_active_pulses = 0;
            for ( m = 0; m < f->num_pulses && ( p = pm_elem[ m ] ) != NULL;
                  m++ )
            {
                if ( ! p->is_active )
                    continue;

                pp = ch->pulse_params + ch->num_active_pulses;
                opp = ch->old_pulse_params + ch->num_active_pulses++;

                pp->pos = opp->pos = p->pos + f->delay;
                pp->len = pp->len = p->len;
                pp->pulse = opp->pulse = p;

                /* Extend pulses for which a shape pulse has been created
                   automatically a bit */

                if (    p->function->self != PULSER_CHANNEL_PULSE_SHAPE
                     && p->sp != NULL )
                {
                    pp->pos -= p->function->left_shape_padding;
                    pp->len +=   p->function->left_shape_padding
                               + p->function->right_shape_padding;
                }

                /* Extend TWT pulses that were automatically created */

                if ( p->function->self == PULSER_CHANNEL_TWT && p->tp != NULL )
                {
                    pp->pos -= p->tp->function->left_twt_padding;
                    pp->len +=   p->tp->function->left_twt_padding
                               + p->tp->function->right_twt_padding;
                }
            }

            ch->old_num_active_pulses = ch->num_active_pulses;

            ep385_channel_start_check( ch );
        }

        if (    f->self == PULSER_CHANNEL_PULSE_SHAPE
             && ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used
             && (    ep385.is_shape_2_defense
                  || ep385.is_defense_2_shape
                  || ep385.function[ PULSER_CHANNEL_TWT ].is_used
                  || ep385.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
            ep385_defense_shape_init_check( f );
    }

    ep385_shape_padding_check_2( );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;
        if (    ( ! f->is_used && f->num_channels == 0 )
             || i == PULSER_CHANNEL_PHASE_1
             || i == PULSER_CHANNEL_PHASE_2 )
            continue;

        for ( j = 0; j < f->num_channels; j++ )
        {
            ch = f->channel[ j ];
            if ( ch->num_active_pulses == 0 )
                continue;

            f->max_seq_len = Ticks_max( f->max_seq_len,
                           ch->pulse_params[ ch->num_active_pulses - 1 ].pos +
                           ch->pulse_params[ ch->num_active_pulses - 1 ].len );
        }
    }

    /* Now we still need a final check that the distance between the last
       defense pulse and the first shape pulse isn't too short - this is
       only relevant for very fast repetitions of the pulse sequence but
       needs to be tested - thanks to Celine Elsaesser for pointing this
       out. */

    if (    ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used
         && ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].is_used )
    {
        Ticks add;
        Channel_T *cs =
                    ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].channel[ 0 ];
        Channel_T *cd = ep385.function[ PULSER_CHANNEL_DEFENSE ].channel[ 0 ];
        Pulse_Params_T *shape_p, *defense_p;


        if ( cd->num_active_pulses != 0 && cs->num_active_pulses != 0 )
        {
            shape_p = cs->pulse_params;
            defense_p = cd->pulse_params + cd->num_active_pulses - 1;
            add = Ticks_min( ep385.defense_2_shape, shape_p->pos
                             + Ticks_max( cd->function->max_seq_len,
                                          cs->function->max_seq_len )
                             - defense_p->pos - defense_p->len );

            if ( add < ep385.defense_2_shape )
                cd->function->max_seq_len = cs->function->max_seq_len =
                                    Ticks_max( cd->function->max_seq_len,
                                               cs->function->max_seq_len )
                                    + ep385.defense_2_shape - add;
    
            shape_p = cs->pulse_params + cs->num_active_pulses - 1;
            defense_p = cd->pulse_params;
            add = Ticks_min( ep385.shape_2_defense, defense_p->pos
                             + Ticks_max( cd->function->max_seq_len,
                                          cs->function->max_seq_len )
                             - shape_p->pos - shape_p->len );

            if ( add < ep385.shape_2_defense )
                cd->function->max_seq_len = cs->function->max_seq_len = 
                                    Ticks_max( cd->function->max_seq_len,
                                               cs->function->max_seq_len )
                                    + ep385.shape_2_defense - add;
        }
    }

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = ep385.function + i;
        if (    ( ! f->is_used && f->num_channels == 0 )
             || i == PULSER_CHANNEL_PHASE_1
             || i == PULSER_CHANNEL_PHASE_2 )
            continue;

        if ( f->max_seq_len > MAX_PULSER_BITS )
        {
            print( FATAL, "Pulse sequence for function '%s' is too long.\n",
                   f->name );
            THROW( EXCEPTION );
        }
    }
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_channel_start_check( Channel_T * ch )
{
    /* Check that there aren't more pulses per channel than the pulser can
       deal with */

    if ( ch->num_active_pulses > MAX_PULSES_PER_CHANNEL )
    {
        print( FATAL, "More than %d pulses (%d) are required on channel %d "
               "associated with function '%s'.\n", MAX_PULSES_PER_CHANNEL,
               ch->num_active_pulses, ch->self, ch->function->name );
        THROW( EXCEPTION );
    }

    qsort( ch->pulse_params, ch->num_active_pulses,
           sizeof *ch->pulse_params, ep385_pulse_compare );

    ep385_shape_padding_check_1( ch );
    if ( ch->function->self == PULSER_CHANNEL_TWT )
        ep385_twt_padding_check( ch );
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void ep385_pulse_init_check( Function_T * f )
{
    Pulse_T *p1, *p2;
    int i, j;


    if ( f->num_pulses < 2 )
        return;

    /* Loop over all active pulses of the function and check that they don't
       overlap. A few things have to be taken into account:
       1. Overlaps of automatically created shape pulses for pulses of the
          same function should not be reported - they can only clash if also
          the pulses they were created for do overlap.
       2. Automatically created TWT pulses can't overlap - if they do we
          will automatically reduce their lengths to avoid overlaps.
       3. Also user defined TWT pulses and automatically created ones can't
          overlap. Again, the automatically generated ones will shrink if
          necessary.
       4. User defined TWT pulses can overlap and thus must be tested.
    */

    for ( i = 0; i < f->num_pulses - 1; i++ )
    {
        p1 = f->pulses[ i ];

        /* Skip checks for inactive pulses and automatically generated
           shape and TWT pulses */

        if (    ! p1->is_active
             || ( f->self == PULSER_CHANNEL_PULSE_SHAPE && p1->sp != NULL )
             || ( f->self == PULSER_CHANNEL_TWT && p1->tp != NULL ) )
            continue;

        for ( j = i + 1; j < f->num_pulses; j++ )
        {
            p2 = f->pulses[ j ];

            /* Skip checks for inactive pulses and automatically generated
               shape and TWT pulses */

            if (    ! p2->is_active
                 || ( f->self == PULSER_CHANNEL_PULSE_SHAPE && p2->sp != NULL )
                 || ( f->self == PULSER_CHANNEL_TWT && p2->tp != NULL ) )
                continue;

            if (    p1->pos == p2->pos
                 || ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos )
                 || ( p2->pos < p1->pos && p2->pos + p2->len > p1->pos ) )
            {
                print( FATAL, "Pulses #%ld and #%ld (function '%s') "
                       "overlap.\n", p1->num, p2->num, f->name );
                THROW( EXCEPTION );
            }
        }
    }
}


/*------------------------------------------------------------------------*
 * Function checks if the distance between pulse shape pulses and defense
 * pulses is large enough. The minimum lengths the shape_2_defense and
 * defense_2_shape members of the ep385 structure. Both are set to rather
 * large values at first but can be customized by calling the EDL
 * functions pulser_shape_to_defense_minimum_distance() and
 * pulser_defense_to_shape_minimum_distance() (names are intentionally
 * that long).
 * The function is called only if pulse shape and defense pulses are used
 * and either also TWT or TWT_GATE pulses or at least one of both the
 * mentioned EDL functions have been called.
 *------------------------------------------------------------------------*/

static void ep385_defense_shape_init_check( Function_T * shape )
{
    Function_T *defense = ep385.function + PULSER_CHANNEL_DEFENSE;
    Pulse_T *shape_p, *defense_p;
    long i, j;


    for ( i = 0; i < shape->num_pulses; i++ )
    {
        shape_p = shape->pulses[ i ];

        if ( ! shape_p->is_active )
            continue;

        for ( j = 0; j < defense->num_pulses; j++ )
        {
            defense_p = defense->pulses[ j ];

            if ( ! defense_p->is_active )
                continue;

            if (    shape_p->pos <= defense_p->pos
                 && shape_p->pos + shape_p->len + ep385.shape_2_defense >
                                                               defense_p->pos )
            {
                if ( ep385.shape_2_defense_too_near == 0 )
                {
                    if ( shape_p->sp == NULL )
                        print( FATAL, "Distance between PULSE_SHAPE pulse "
                               "#%ld and DEFENSE pulse #%ld is shorter than "
                               "%s.\n", shape_p->num, defense_p->num,
                               ep385_ptime( ep385_ticks2double(
                                                   ep385.shape_2_defense ) ) );
                    else
                        print( FATAL, "Distance between shape pulse for "
                               "pulse #%ld (function '%s') and DEFENSE pulse "
                               "#%ld is shorter than %s.\n", shape_p->sp->num,
                               shape_p->sp->function->name, defense_p->num,
                               ep385_pticks( ep385.shape_2_defense ) );
                }

                ep385.shape_2_defense_too_near++;
            }

            if (    defense_p->pos < shape_p->pos
                 && defense_p->pos + defense_p->len + ep385.defense_2_shape >
                                                                 shape_p->pos )
            {
                if ( ep385.defense_2_shape_too_near == 0 )
                {
                    if ( shape_p->sp == NULL )
                        print( FATAL, "Distance between DEFENSE pulse #%ld "
                               "and PULSE_SHAPE pulse #%ld is shorter than "
                               "%s.\n", defense_p->num, shape_p->num,
                               ep385_ptime( ep385_ticks2double(
                                                   ep385.defense_2_shape ) ) );
                    else
                        print( FATAL, "Distance between DEFENSE pulse #%ld "
                               "and shape pulse for pulse #%ld (function "
                               "'%s') is shorter than %s.\n", defense_p->num,
                               shape_p->sp->num, shape_p->sp->function->name,
                               ep385_pticks( ep385.defense_2_shape ) );
                }

                ep385.defense_2_shape_too_near++;
            }
        }
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
