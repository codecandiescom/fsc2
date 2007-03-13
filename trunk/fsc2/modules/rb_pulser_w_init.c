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


#include "rb_pulser_w.h"


static void rb_pulser_w_init_print( FILE * /* fp */ );

static void rb_pulser_w_basic_pulse_check( void );

static void rb_pulser_w_basic_functions_init( void );

static void rb_pulser_w_defense_pulse_create( void );

static void rb_pulser_w_rf_synth_init( void );


/*-------------------------------------------------------------*
 * Function does everything that needs to be done for checking
 * and completing the internal representation of the pulser at
 * the start of a test run.
 *-------------------------------------------------------------*/

void rb_pulser_w_init_setup( void )
{
    rb_pulser_w_basic_pulse_check( );
    rb_pulser_w_basic_functions_init( );
    rb_pulser_w_defense_pulse_create( );
    rb_pulser_w_rf_synth_init( );

    if ( rb_pulser_w.dump_file != NULL )
        rb_pulser_w_init_print( rb_pulser_w.dump_file );

    if ( rb_pulser_w.show_file != NULL )
        rb_pulser_w_init_print( rb_pulser_w.show_file );

    rb_pulser_w_update_pulses( SET );
}


/*------------------------------------------------------*
 * Initialization required for the pulser_show_pulses()
 * and pulser_dump_pulses() EDL functions
 *------------------------------------------------------*/

static void rb_pulser_w_init_print( FILE * fp )
{
    int i;


    if ( fp == NULL )
        return;

    fprintf( fp, "TB: %g\nD: %ld\n", rb_pulser_w.timebase,
             Ticks_rnd( rb_pulser_w.neg_delay / rb_pulser_w.timebase ) );

    if ( rb_pulser_w.needs_phases )
        fprintf( fp, "PC: MW\n" );

    fprintf( fp, "===\n" );

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        if ( rb_pulser_w.function[ i ].is_used )
            fprintf( fp, "%s:  %ld\n", rb_pulser_w.function[ i ].name,
                     Ticks_rnd( rb_pulser_w.function[ i ].delay /
                                rb_pulser_w.timebase ) );
}


/*------------------------------------------------------------*
 * Function runs through all pulses and checks that at least:
 * 1. the pulse is associated with a function
 * 2. its start position is set
 * 3. its length is set
 *------------------------------------------------------------*/

static void rb_pulser_w_basic_pulse_check( void )
{
    Pulse_T *p;


    for ( p = rb_pulser_w.pulses; p != NULL; p = p->next )
    {
        p->is_active = SET;

        /* Check the pulse function */

        if ( p->function == NULL )
        {
            print( FATAL, "Pulse #%ld is not associated with a function.\n",
                   p->num );
            THROW( EXCEPTION );
        }

        /* The length of the detection pulse can't be changed and is always
           assumed to be one Tick */

        if ( p->function == rb_pulser_w.function + PULSER_CHANNEL_DET )
        {
            p->is_len = SET;
            p->len = 1;
            p->function->last_pulse_len = rb_pulser_w.timebase;
        }

        /* Check start position and pulse length */

        if ( ! p->is_pos || ! p->is_len || p->len == 0 )
            p->is_active = UNSET;

        if ( p->is_active )
            p->was_active = p->has_been_active = SET;
    }
}


/*---------------------------------------------------------------------------*
 * Creates a list of pulses for each function that has pulses assigned to it
 *---------------------------------------------------------------------------*/

static void rb_pulser_w_basic_functions_init( void )
{
    Function_T *f;
    int i;
    Pulse_T *cp;


    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        f = rb_pulser_w.function + i;

        if ( ! f->is_used )
            continue;

        /* Assemble a list of all pulses assigned to the function */

        for ( f->num_pulses = 0, cp = rb_pulser_w.pulses;
              cp != NULL; cp = cp->next )
        {
            if ( cp->function != f )
                continue;

            f->num_pulses++;
            f->pulses = PULSE_PP T_realloc( f->pulses, f->num_pulses
                                                       * sizeof *f->pulses );
            f->pulses[ f->num_pulses - 1 ] = cp;
        }

        if ( f->num_pulses == 0 )
        {
            if(  f->is_declared &&
                 ! ( f == rb_pulser_w.function + PULSER_CHANNEL_DEFENSE &&
                     ! rb_pulser_w.defense_pulse_mode ) )
                print( WARN, "No pulses have been assigned to function "
                       "'%s'.\n", f->name );
            f->is_used = UNSET;
        }

        if ( i == PULSER_CHANNEL_MW && f->num_pulses > MAX_MW_PULSES )
        {
            print( FATAL, "Pulser function 'MICROWAVE' allows only %d pulses "
                   "but %d are defined.\n", MAX_MW_PULSES, f->num_pulses );
            THROW( EXCEPTION );
        }

        if ( i != PULSER_CHANNEL_MW && f->num_pulses > 1 )
        {
            print( FATAL, "Pulser function '%s' allows only 1 pulse but %d "
                   "are defined.\n", Function_Names[ i ], f->num_pulses );
            THROW( EXCEPTION );
        }
    }
}


/*--------------------------------------------------------------------*
 * Function for creating a pulse for the defense function (unless the
 * user told us not to create one)
 *--------------------------------------------------------------------*/

static void rb_pulser_w_defense_pulse_create( void )
{
    Function_T *mw = rb_pulser_w.function + PULSER_CHANNEL_MW;
    Function_T *def = rb_pulser_w.function + PULSER_CHANNEL_DEFENSE;
    Pulse_T *cp = rb_pulser_w.pulses;
    Pulse_T *lp = NULL;


    /* No defense pulse is created if there aren't any microwave pulses or
       the user explicitely told us that (s)he will take care of that pulse.
       In the latter case add a pulse position (always the start of the pulse
       pattern) if a pulse exists but its position hasn't been set by the
       user. If the user has set the length the pulse must also be marked as
       active. */

    if ( ! mw->is_used )
        return;

    if ( ! rb_pulser_w.defense_pulse_mode &&
         def->num_pulses != 0 && ! def->pulses[ 0 ]->is_pos )
    {
        def->pulses[ 0 ]->is_pos = def->pulses[ 0 ]->initial_is_pos = SET;
        def->pulses[ 0 ]->pos = def->pulses[ 0 ]->initial_pos = - def->delay;

        if ( def->pulses[ 0 ]->is_len && def->pulses[ 0 ]->len != 0 )
        {
            def->pulses[ 0 ]->is_active = SET;
            def->num_active_pulses = 1;
        }

        def->is_used = SET;

        return;
    }

    /* Also return if the user asked not to set an automatically generated
       defense pulse but didn't create a defense pulse (thus switching off
       the defense pulse completely, hopefully only for testing purposes...) */

    if ( ! rb_pulser_w.defense_pulse_mode &&
         def->num_pulses == 0 )
        return;

    def->is_used = SET;

    def->num_pulses = def->num_active_pulses = 1;
    def->pulses = PULSE_PP T_malloc( sizeof *def->pulses );

    /* Create the defense pulse and append it to the list of all pulses */

    while ( cp != NULL )
    {
        lp = cp;
        cp = cp->next;
    }

    cp = PULSE_P T_malloc( sizeof *cp );

    lp->next = cp;
    cp->prev = lp;
    cp->next = NULL;
    cp->num = -1;
    cp->function = def;
    cp->pc = NULL;

    cp->has_been_active = cp->was_active = UNSET;
        
    def->pulses == PULSE_PP T_malloc( sizeof *def->pulses );
    *def->pulses = cp;

    cp->is_pos = cp->initial_is_pos = SET;

    cp->pos = cp->initial_pos = - def->delay;
    cp->is_len = cp->is_dpos = cp->is_dlen = UNSET;
    cp->initial_is_len = cp->initial_is_dpos = cp->initial_is_dlen = UNSET;
}


/*------------------------------------------------------------------*
 * Finds out about the names of synthesizer related functions if RF
 * pulses could be required (i.e. if there's at least one pulse
 * marked as belonging to the RF function)
 *------------------------------------------------------------------*/

static void rb_pulser_w_rf_synth_init( void )
{
    Function_T *f = rb_pulser_w.function + PULSER_CHANNEL_RF;
    int dev_num;
    char *func;
    Var_T *func_ptr;
    int acc;


    if ( ! f->is_used || f->pulses == NULL )
        return;

    /* Try to find the functions for setting the pulse width and delay for
       the synthesizer */

    if ( ( dev_num = exists_device( SYNTHESIZER_MODULE ) ) == 0 )
    {
        print( FATAL, "Can't find the module '%s' - it must be listed in the "
               "DEVICES section to allow RF pulses.\n", SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    if ( dev_num == 1 )
        func = T_strdup( SYNTHESIZER_PULSE_STATE );
    else
        func = get_string( SYNTHESIZER_PULSE_STATE "#%d", dev_num );
    
    if ( ! func_exists( func ) ||
         ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        T_free( func );
        print( FATAL, "Function for switching pulse modulation on or off is "
               "missing from the synthesizer module '%s'.\n",
               SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( func_ptr );
    rb_pulser_w.synth_pulse_state = func;

    if ( dev_num == 1 )
        func = T_strdup( SYNTHESIZER_PULSE_WIDTH );
    else
        func = get_string( SYNTHESIZER_PULSE_WIDTH "#%d", dev_num );
    
    if ( ! func_exists( func ) ||
         ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        rb_pulser_w.synth_pulse_state =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_state );
        T_free( func );
        print( FATAL, "Function for setting pulse widths is missing from "
               "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( func_ptr );
    rb_pulser_w.synth_pulse_width = func;
    
    if ( dev_num == 1 )
        func = T_strdup( SYNTHESIZER_PULSE_DELAY );
    else
        func = get_string( SYNTHESIZER_PULSE_DELAY "#%d", dev_num );
    
    if ( ! func_exists( func ) ||
         ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        rb_pulser_w.synth_pulse_state =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_state );
        rb_pulser_w.synth_pulse_width =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_width );
        T_free( func );
        print( FATAL, "Function for setting pulse delays is missing from "
               "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( func_ptr );
    rb_pulser_w.synth_pulse_delay = func;

    if ( dev_num == 1 )
        func = T_strdup( SYNTHESIZER_TRIG_SLOPE );
    else
        func = get_string( SYNTHESIZER_TRIG_SLOPE "#%d", dev_num );
    
    if ( ! func_exists( func ) ||
         ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        rb_pulser_w.synth_pulse_state =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_state );
        rb_pulser_w.synth_pulse_delay =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_delay );
        rb_pulser_w.synth_pulse_width =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_width );
        T_free( func );
        print( FATAL, "Function for setting the trigger slope is missing from "
               "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( func_ptr );
    rb_pulser_w.synth_trig_slope = func;

    if ( dev_num == 1 )
        func = T_strdup( SYNTHESIZER_STATE );
    else
        func = get_string( SYNTHESIZER_STATE "#%d", dev_num );
    
    if ( ! func_exists( func ) ||
         ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        rb_pulser_w.synth_pulse_state =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_state );
        rb_pulser_w.synth_pulse_delay =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_delay );
        rb_pulser_w.synth_pulse_width =
                                CHAR_P T_free( rb_pulser_w.synth_pulse_width );
        rb_pulser_w.synth_trig_slope =
                                CHAR_P T_free( rb_pulser_w.synth_trig_slope );
        T_free( func );
        print( FATAL, "Function for setting the trigger slope is missing from "
               "the synthesizer module '%s'.\n", SYNTHESIZER_MODULE );
        THROW( EXCEPTION );
    }

    vars_pop( func_ptr );
    rb_pulser_w.synth_state = func;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
