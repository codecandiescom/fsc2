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


#include "dg2020_f.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

DG2020_T dg2020;


/*-------------------------------------------------------------------------*
 * This function is called directly after all modules are loaded. Its main
 * function is to initialize all global variables that are needed in the
 * module.
 *-------------------------------------------------------------------------*/

int
dg2020_f_init_hook( void )
{
    int i, j, k;


    Pulser_Struct.name     = DEVICE_NAME;
    Pulser_Struct.has_pods = SET;

    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Than we have to set up the global structure for the pulser, especially
       we have to set the pointers for the functions that will get called from
       pulser.c */

    dg2020.needs_update = UNSET;
    dg2020.is_running   = SET;
    dg2020.keep_all     = UNSET;
    dg2020.pulses       = NULL;
    dg2020.in_setup     = UNSET;

    Pulser_Struct.needs_phase_pulses = SET;

    Pulser_Struct.set_timebase = dg2020_store_timebase;

    Pulser_Struct.assign_function = dg2020_assign_function;
    Pulser_Struct.assign_channel_to_function =
                                             dg2020_assign_channel_to_function;
    Pulser_Struct.invert_function = dg2020_invert_function;
    Pulser_Struct.set_function_delay = dg2020_set_function_delay;
    Pulser_Struct.set_function_high_level = dg2020_set_function_high_level;
    Pulser_Struct.set_function_low_level = dg2020_set_function_low_level;

    Pulser_Struct.set_trigger_mode = dg2020_set_trigger_mode;
    Pulser_Struct.set_repeat_time = dg2020_set_repeat_time;
    Pulser_Struct.set_trig_in_level = dg2020_set_trig_in_level;
    Pulser_Struct.set_trig_in_slope = dg2020_set_trig_in_slope;
    Pulser_Struct.set_trig_in_impedance = dg2020_set_trig_in_impedance;
    Pulser_Struct.set_max_seq_len = dg2020_set_max_seq_len;

    Pulser_Struct.set_phase_reference = dg2020_set_phase_reference;

    Pulser_Struct.new_pulse = dg2020_new_pulse;
    Pulser_Struct.set_pulse_function = dg2020_set_pulse_function;
    Pulser_Struct.set_pulse_position = dg2020_set_pulse_position;
    Pulser_Struct.set_pulse_length = dg2020_set_pulse_length;
    Pulser_Struct.set_pulse_position_change = dg2020_set_pulse_position_change;
    Pulser_Struct.set_pulse_length_change = dg2020_set_pulse_length_change;
    Pulser_Struct.set_pulse_phase_cycle = dg2020_set_pulse_phase_cycle;
    Pulser_Struct.set_grace_period = dg2020_set_grace_period;

    Pulser_Struct.get_pulse_function = dg2020_get_pulse_function;
    Pulser_Struct.get_pulse_position = dg2020_get_pulse_position;
    Pulser_Struct.get_pulse_length = dg2020_get_pulse_length;
    Pulser_Struct.get_pulse_position_change = dg2020_get_pulse_position_change;
    Pulser_Struct.get_pulse_length_change = dg2020_get_pulse_length_change;
    Pulser_Struct.get_pulse_phase_cycle = dg2020_get_pulse_phase_cycle;

    Pulser_Struct.phase_setup_prep = dg2020_phase_setup_prep;
    Pulser_Struct.phase_setup = dg2020_phase_setup;

    Pulser_Struct.set_phase_switch_delay = dg2020_set_phase_switch_delay;

    Pulser_Struct.keep_all_pulses = dg2020_keep_all;

    Pulser_Struct.ch_to_num = dg2020_ch_to_num;

    /* Finally, we initialize variables that store the state of the pulser */

    dg2020.is_timebase = UNSET;
    dg2020.is_trig_in_mode = UNSET;
    dg2020.is_trig_in_slope = UNSET;
    dg2020.is_trig_in_level = UNSET;
    dg2020.is_repeat_time = UNSET;
    dg2020.is_neg_delay = UNSET;
    dg2020.neg_delay = 0;
    dg2020.is_grace_period = UNSET;
    dg2020.is_max_seq_len = UNSET;

    dg2020.dump_file = NULL;
    dg2020.show_file = NULL;

    dg2020.block[ 0 ].is_used = dg2020.block[ 1 ].is_used = UNSET;

    for ( i = 0; i < NUM_PODS; i++ )
    {
        dg2020.pod[ i ].self = i;
        dg2020.pod[ i ].function = NULL;
    }

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        dg2020.function[ i ].self = i;
        dg2020.function[ i ].name = Function_Names[ i ];
        dg2020.function[ i ].is_used = UNSET;
        dg2020.function[ i ].is_needed = UNSET;
        dg2020.function[ i ].pod = NULL;
        dg2020.function[ i ].is_phs = UNSET;
        dg2020.function[ i ].is_psd = UNSET;
        dg2020.function[ i ].num_channels = 0;
        dg2020.function[ i ].num_pulses = 0;
        dg2020.function[ i ].pulses = NULL;
        dg2020.function[ i ].next_phase = 0;
        dg2020.function[ i ].phase_func = NULL;
        dg2020.function[ i ].is_inverted = UNSET;
        dg2020.function[ i ].delay = 0;
        dg2020.function[ i ].is_delay = UNSET;
        dg2020.function[ i ].is_high_level = UNSET;
        dg2020.function[ i ].is_low_level = UNSET;
    }

    for ( i = 0; i < MAX_CHANNELS; i++ )
    {
        dg2020.channel[ i ].self = i;
        dg2020.channel[ i ].function = NULL;
    }

    for ( i = 0; i < 2; i++ )
        for ( j = 0; j < 4; j++ )
            for ( k = 0; k < 2; k++ )
                dg2020.phs[ i ].is_var[ j ][ k ] = UNSET;

    dg2020.is_needed = SET;

    dg2020.phase_numbers[ 0 ] = dg2020.phase_numbers[ 1 ] = -1;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
dg2020_f_test_hook( void )
{
    if ( dg2020.pulses == NULL )
    {
        dg2020.is_needed = UNSET;
        print( WARN, "Driver loaded but no pulses are defined.\n" );
        return 1;
    }

    /* Check consistency of pulser settings and do everything to setup the
       pulser for the test run */

    TRY
    {
        dg2020.in_setup = SET;
        dg2020_init_setup( );
        dg2020.in_setup = UNSET;
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        dg2020.in_setup = UNSET;
        if ( dg2020.dump_file )
        {
            fclose( dg2020.dump_file );
            dg2020.dump_file = NULL;
        }

        if ( dg2020.show_file )
        {
            fclose( dg2020.show_file );
            dg2020.show_file = NULL;
        }

        RETHROW;
    }


    /* We need some somewhat different functions (or disable some) for setting
       of pulse properties */

    Pulser_Struct.set_pulse_function = NULL;
    Pulser_Struct.set_pulse_phase_cycle = NULL;

    Pulser_Struct.set_pulse_position = dg2020_change_pulse_position;
    Pulser_Struct.set_pulse_length = dg2020_change_pulse_length;
    Pulser_Struct.set_pulse_position_change =
        dg2020_change_pulse_position_change;
    Pulser_Struct.set_pulse_length_change = dg2020_change_pulse_length_change;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
dg2020_f_end_of_test_hook( void )
{
    if ( dg2020.dump_file != NULL )
    {
        fclose( dg2020.dump_file );
        dg2020.dump_file = NULL;
    }

    if ( dg2020.show_file != NULL )
    {
        fclose( dg2020.show_file );
        dg2020.show_file = NULL;
    }

    if ( ! dg2020.is_needed )
        return 1;

    /* First we have to reset the internal representation back to its initial
       state */

    dg2020_full_reset( );

    /* Now we've got to find out about the maximum sequence length, set up
       padding to achieve the repeat time and set the lengths of the last
       phase pulses in the channels needing phase cycling */

    dg2020.max_seq_len = dg2020_get_max_seq_len( );
    dg2020_calc_padding( );
    dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_1 );
    dg2020_finalize_phase_pulses( PULSER_CHANNEL_PHASE_2 );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
dg2020_f_exp_hook( void )
{
    int i;


    if ( ! dg2020.is_needed )
        return 1;

    /* Initialize the device */

    if ( ! dg2020_init( DEVICE_NAME ) )
    {
        print( FATAL, "Failure to initialize the pulser: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    /* Now we have to tell the pulser about all the pulses */

    dg2020.in_setup = SET;
    if ( ! dg2020_reorganize_pulses( UNSET ) )
    {
        dg2020.in_setup = UNSET;
        THROW( EXCEPTION );
    }
    dg2020.in_setup = UNSET;

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
    {
        if ( ! dg2020.function[ i ].is_used )
            continue;
        dg2020_set_pulses( dg2020.function + i );
        dg2020_clear_padding_block( dg2020.function + i );
    }

    /* Finally tell the pulser to update (we're always running in manual
       update mode) and than switch the pulser into run mode */

    dg2020_update_data( );
    dg2020_run( dg2020.is_running );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
dg2020_f_end_of_exp_hook( void )
{
    if ( ! dg2020.is_needed )
        return 1;

    dg2020_run( STOP );
    gpib_local( dg2020.device );

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
dg2020_f_exit_hook( void )
{
    Pulse_T *p, *np;
    int i;


    if ( ! dg2020.is_needed )
        return;

    /* Free all the memory allocated within the module */

    for ( p = dg2020.pulses; p != NULL; np = p->next, T_free( p ), p = np )
        /* empty */ ;
    dg2020.pulses = NULL;

    for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
        if ( dg2020.function[ i ].pulses != NULL )
            T_free( dg2020.function[ i ].pulses );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_show_pulses( Var_T * v  UNUSED_ARG )
{
    int pd[ 2 ];
    pid_t pid;


    if ( FSC2_IS_CHECK_RUN || FSC2_IS_BATCH_MODE )
        return vars_push( INT_VAR, 1L );

    if ( dg2020.show_file != NULL )
        return vars_push( INT_VAR, 1L );

    if ( pipe( pd ) == -1 )
    {
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Failure, running out of system resources.\n" );
        return vars_push( INT_VAR, 0L );
    }

    if ( ( pid =  fork( ) ) < 0 )
    {
        if ( errno == ENOMEM || errno == EAGAIN )
            print( FATAL, "Failure, running out of system resources.\n" );
        return vars_push( INT_VAR, 0L );
    }

    /* Here's the childs code */

    if ( pid == 0 )
    {
        char * volatile cmd = NULL;

        close( pd[ 1 ] );

        if ( dup2( pd[ 0 ], STDIN_FILENO ) == -1 )
        {
            goto filter_failure;
            close( pd[ 0 ] );
        }

        close( pd[ 0 ] );

        TRY
        {
            cmd = get_string( "%s%sfsc2_pulses", bindir, slash( bindir ) );
            execl( cmd, "fsc2_pulses", NULL );
        }

    filter_failure:

        if ( cmd != NULL )
            T_free( cmd );
        _exit( EXIT_FAILURE );
    }

    /* And finally the code for the parent */

    close( pd[ 0 ] );
    dg2020.show_file = fdopen( pd[ 1 ], "w" );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_dump_pulses( Var_T * v  UNUSED_ARG )
{
    char *name;
    char *m;
    struct stat stat_buf;


    if ( FSC2_IS_CHECK_RUN || FSC2_IS_BATCH_MODE )
        return vars_push( INT_VAR, 1L );

    if ( dg2020.dump_file != NULL )
    {
        print( WARN, "Pulse dumping is already switched on.\n" );
        return vars_push( INT_VAR, 1L );
    }

    do
    {
        name = T_strdup( fsc2_show_fselector( "File for dumping pulses:", NULL,
                                              "*.pls", NULL ) );
        if ( name == NULL || *name == '\0' )
        {
            if ( name != NULL )
                T_free( name );
            return vars_push( INT_VAR, 0L );
        }

        if  ( 0 == stat( name, &stat_buf ) )
        {
            m = get_string( "The selected file does already exist:\n%s\n"
                            "\nDo you really want to overwrite it?", name );
            if ( 1 != show_choices( m, 2, "Yes", "No", NULL, 2 ) )
            {
                T_free( m );
                name = T_free( name );
                continue;
            }
            T_free( m );
        }

        if ( ( dg2020.dump_file = fopen( name, "w+" ) ) == NULL )
        {
            switch( errno )
            {
                case EMFILE :
                    show_message( "Sorry, you have too many open files!\n"
                                  "Please close at least one and retry." );
                    break;

                case ENFILE :
                    show_message( "Sorry, system limit for open files "
                                  "exceeded!\n Please try to close some "
                                  "files and retry." );
                break;

                case ENOSPC :
                    show_message( "Sorry, no space left on device for more "
                                  "file!\n    Please delete some files and "
                                  "retry." );
                    break;

                default :
                    show_message( "Sorry, can't open selected file for "
                                  "writing!\n       Please select a "
                                  "different file." );
            }

            name = T_free( name );
            continue;
        }
    } while ( dg2020.dump_file == NULL );

    T_free( name );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_phase_switch_delay( Var_T * v )
{
    long func;
    double psd;


    if ( v == NULL || v->next == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    func = get_strict_long( v, "pulser function" );

    if (    func != PULSER_CHANNEL_PHASE_1
         || func != PULSER_CHANNEL_PHASE_2 )
    {
        print( FATAL, "A phase switch delay can only be set for the two PHASE "
               "functions.\n" );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    psd = get_double( v, "phase switch delay" );
    is_mult_ns( psd, "Phase switch delay" );

    too_many_arguments( v );

    dg2020_set_phase_switch_delay( func, psd );
    return vars_push( FLOAT_VAR,
                      dg2020.function[ func ].psd * dg2020.timebase );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_grace_period( Var_T * v )
{
    double gp;

    gp = get_double( v, "grace period" );
    is_mult_ns( gp, "Grace period" );

    dg2020_set_grace_period( gp );
    return vars_push( FLOAT_VAR, dg2020.grace_period * dg2020.timebase );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_keep_all_pulses( Var_T * v  UNUSED_ARG )
{
    dg2020_keep_all( );
    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *pulser_maximum_pattern_length( Var_T * v )
{
    double pl;

    pl = get_double( v, "maximum pattern length" );
    is_mult_ns( pl, "Maximum pattern length" );
    dg2020_set_max_seq_len( pl );
    return vars_push( FLOAT_VAR, dg2020.max_seq_len * dg2020.timebase );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_state( Var_T * v )
{
    bool state;


    if ( v == NULL )
        return vars_push( INT_VAR, ( long ) dg2020.is_running );

    state = get_boolean( v );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, ( long ) ( dg2020.is_running = state ) );

    dg2020_run( state );
    return vars_push( INT_VAR, ( long ) dg2020.is_running );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_channel_state( Var_T * v  UNUSED_ARG )
{
    print( SEVERE, "Individual pod channels can't be switched on or off for "
           "this device.\n" );
    return vars_push( INT_VAR, 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_update( Var_T * v  UNUSED_ARG )
{
    bool state = OK;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    /* Send all changes to the pulser */

    if ( dg2020.needs_update )
        state = dg2020_do_update( );

    /* If we're doing a real experiment also tell the pulser to start */

    if ( FSC2_MODE == EXPERIMENT )
        dg2020_run( dg2020.is_running );

    return vars_push( INT_VAR, state ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_shift( Var_T * v )
{
    Pulse_T *p;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to shift all active pulses that
       have a position change time value set */

    if ( v == NULL )
        for( p = dg2020.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dpos )
                pulser_shift( vars_push( INT_VAR, p->num ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

        if ( ! p->is_pos )
        {
            print( FATAL, "Pulse #%ld has no position set, so shifting it is "
                   "impossible.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_dpos )
        {
            print( FATAL, "Time for position change hasn't been defined for "
                   "pulse #%ld.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_old_pos )
        {
            p->old_pos = p->pos;
            p->is_old_pos = SET;
        }

        if ( ( p->pos += p->dpos ) < 0 )
        {
            print( FATAL, "Shifting the position of pulse #%ld leads to an "
                   "invalid  negative position of %s.\n",
                   p->num, dg2020_pticks( p->pos ) );
            THROW( EXCEPTION );
        }

        if ( p->pos == p->old_pos )       /* nothing really changed ? */
            p->is_old_pos = UNSET;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        if ( p->needs_update )
            dg2020.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_increment( Var_T * v )
{
    Pulse_T *p;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to increment all active pulses
       that have a length change time value set */

    if ( v == NULL )
        for( p = dg2020.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 && p->is_active && p->is_dlen )
                pulser_increment( vars_push( INT_VAR, p->num ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

        if ( ! p->is_len )
        {
            print( FATAL, "Pulse #%ld has no length set, so imcrementing it "
                   "is impossibe.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_dlen )
        {
            print( FATAL, "Length change time hasn't been defined for pulse "
                   "#%ld.\n", p->num );
            THROW( EXCEPTION );
        }

        if ( ! p->is_old_len )
        {
            p->old_len = p->len;
            p->is_old_len = SET;
        }

        if ( ( p->len += p->dlen ) < 0 )
        {
            print( FATAL, "Incrementing the length of pulse #%ld leads to an "
                   "invalid negative pulse length of %s.\n",
                   p->num, dg2020_pticks( p->len ) );
            THROW( EXCEPTION );
        }

        if ( p->old_len == p->len )
            p->is_old_len = UNSET;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        /* If the pulse was or is active we've got to update the pulser */

        if ( p->needs_update )
            dg2020.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_next_phase( Var_T * v )
{
    Function_T *f;
    long phase_number;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    if ( v == NULL )
    {
        if (    ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used
             && ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used
             && FSC2_MODE == TEST )
        {
            print( SEVERE, "No phase functions are in use.\n" );
            return vars_push( INT_VAR, 0L );
        }

        if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
            pulser_next_phase( vars_push( INT_VAR, 1L ) );
        if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
            pulser_next_phase( vars_push( INT_VAR, 2L ) );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        phase_number = get_strict_long( v, "phase number" );

        if ( phase_number != 1 && phase_number != 2 )
        {
            print( FATAL, "Invalid phase number: %ld.\n", phase_number );
            THROW( EXCEPTION );
        }

        f = dg2020.function + ( phase_number == 1 ? PULSER_CHANNEL_PHASE_1 :
                                PULSER_CHANNEL_PHASE_2 );

        if ( ! f->is_used && FSC2_MODE == TEST )
        {
            print( SEVERE, "Phase function '%s' is not used.\n", f->name );
            return vars_push( INT_VAR, 0L );
        }

        if ( f->next_phase >= f->num_channels )
            f->next_phase = 0;

        if ( FSC2_MODE == EXPERIMENT )
        {
            if (    ! dg2020_channel_assign(
                                           f->channel[ f->next_phase++ ]->self,
                                           f->pod->self )
                 || ! dg2020_channel_assign(
                                           f->channel[ f->next_phase++ ]->self,
                                           f->pod2->self )
                 || ! dg2020_update_data( ) )
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            f->next_phase += 2;
            if ( dg2020.dump_file != NULL )
                dg2020_dump_channels( dg2020.dump_file );
            if ( dg2020.show_file != NULL )
                dg2020_dump_channels( dg2020.show_file );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_reset( Var_T * v  UNUSED_ARG )
{
    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    if (    dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used
         || dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
        vars_pop( pulser_pulse_reset( NULL ) );
    vars_pop( pulser_pulse_reset( NULL ) );

    return pulser_update( NULL );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_phase_reset( Var_T * v )
{
    Function_T *f;
    long phase_number;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    if ( v == NULL )
    {
        if (    ! dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used
             && ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used
             && FSC2_MODE == TEST )
        {
            print( SEVERE, "No phase functions are in use.\n" );
            return vars_push( INT_VAR, 0L );
        }

        if ( dg2020.function[ PULSER_CHANNEL_PHASE_1 ].is_used )
            vars_pop( pulser_phase_reset( vars_push( INT_VAR, 1L ) ) );
        if ( dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_used )
            vars_pop( pulser_phase_reset( vars_push( INT_VAR, 2L ) ) );
    }

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        phase_number = get_strict_long( v, "phase number" );

        if ( phase_number != 1 && phase_number != 2 )
        {
            print( FATAL, "Invalid phase number: %ld.\n", phase_number );
            THROW( EXCEPTION );
        }

        f = dg2020.function + (  phase_number == 1 ? PULSER_CHANNEL_PHASE_1 :
                                 PULSER_CHANNEL_PHASE_2 );

        if ( ! f->is_used && FSC2_MODE == TEST )
        {
            print( SEVERE, "Phase function '%s' is not used.\n", f->name );
            return vars_push( INT_VAR, 0L );
        }

        if ( FSC2_MODE == EXPERIMENT )
        {
            if (    ! dg2020_channel_assign( f->channel[ 0 ]->self,
                                             f->pod->self )
                 || ! dg2020_channel_assign( f->channel[ 1 ]->self,
                                             f->pod2->self )
                 || ! dg2020_update_data( ) )
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( dg2020.dump_file != NULL )
                dg2020_dump_channels( dg2020.dump_file );
            if ( dg2020.show_file != NULL )
                dg2020_dump_channels( dg2020.show_file );
        }

        f->next_phase = 2;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_pulse_reset( Var_T * v )
{
    Pulse_T *p;


    if ( ! dg2020.is_needed )
        return vars_push( INT_VAR, 1L );

    /* An empty pulse list means that we have to reset all pulses (even the
       inactive ones) */

    if ( v == NULL )
        for( p = dg2020.pulses; p != NULL; p = p->next )
            if ( p->num >= 0 )
                vars_pop( pulser_pulse_reset( vars_push( INT_VAR, p->num ) ) );

    /* Otherwise run through the supplied pulse list */

    for ( ; v != NULL; v = vars_pop( v ) )
    {
        p = dg2020_get_pulse( get_strict_long( v, "pulse number" ) );

        /* Reset all changeable properties back to their initial values */

        if ( p->is_pos )
        {
            p->old_pos = p->pos;
            p->is_old_pos = SET;
        }
        p->pos = p->initial_pos;
        p->is_pos = p->initial_is_pos;

        if ( p->is_len )
        {
            p->old_len = p->len;
            p->is_old_len = SET;
        }

        p->len = p->initial_len;
        if ( p->len != 0.0 )
            p->is_len = p->initial_is_len;
        else
            p->is_len = UNSET;

        p->dpos = p->initial_dpos;
        p->is_dpos = p->initial_is_dpos;

        p->dlen = p->initial_dlen;
        p->is_dlen = p->initial_is_dlen;

        p->has_been_active |= ( p->is_active = IS_ACTIVE( p ) );
        p->needs_update = NEEDS_UPDATE( p );

        if ( p->needs_update )
            dg2020.needs_update = SET;
    }

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_lock_keyboard( Var_T * v )
{
    bool lock;


    if ( v == NULL )
        lock = SET;
    else
    {
        lock = get_boolean( v );
        too_many_arguments( v );
    }

    if ( FSC2_MODE == EXPERIMENT )
        dg2020_lock_state( lock );

    return vars_push( INT_VAR, lock ? 1L : 0L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
pulser_command( Var_T * v )
{
    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        char * volatile cmd = NULL;

        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            dg2020_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW;
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
