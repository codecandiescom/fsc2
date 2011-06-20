/* -*-C-*-
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


#include "rs_smb100a.h"


/*--------------------------------*/
/* Global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

RS_SMB100A_T rs_smb100a;

static const char *mod_types[ ] =   { "FM", "AM", "PHASE", "OFF" };


struct MOD_RANGES fm_mod_ranges[ ] = { { 7.600000e6, 1.00e6 },
                                       { 1.513125e8, 1.25e5 },
                                       { 3.026250e8, 2.50e6 },
                                       { 6.052500e8, 5.00e6 },
                                       { 1.100000e9, 1.00e6 } };

struct MOD_RANGES pm_mod_ranges[ ] = { { 7.600000e6, 10.0 },
                                       { 1.513125e8, 1.25 },
                                       { 3.026250e8, 2.50 },
                                       { 6.052500e8, 5.00 },
                                       { 1.100000e9, 10.0 } };

size_t num_fm_mod_ranges = NUM_ELEMS( fm_mod_ranges );
size_t num_pm_mod_ranges = NUM_ELEMS( pm_mod_ranges );

static RS_SMB100A_T rs_smb100a_backup;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
rs_smb100a_init_hook( void )
{
    int i;


    /* Set global variable to indicate that LAN is needed */

	Need_LAN = SET;

	rs_smb100a.device = -1;

    rs_smb100a.state = UNSET;

    rs_smb100a.freq_is_set = UNSET;
    rs_smb100a.step_freq_is_set = UNSET;
    rs_smb100a.start_freq_is_set = UNSET;
    rs_smb100a.attenuation_is_set = UNSET;
    rs_smb100a.min_attenuation = MIN_ATTEN;

    rs_smb100a.table_file = NULL;
    rs_smb100a.use_table = UNSET;
    rs_smb100a.att_table = NULL;
    rs_smb100a.att_table_len = 0;
    rs_smb100a.real_attenuation = MAX_ATTEN - 100.0;  /* invalid value ! */

    rs_smb100a.att_ref_freq = DEF_ATT_REF_FREQ;

    rs_smb100a.mod_type = UNDEFINED;
    rs_smb100a.mod_type_is_set = UNSET;
    for ( i = 0; i < NUM_MOD_TYPES; i++ )
    {
        rs_smb100a.mod_source_is_set[ i ] = UNSET;
        rs_smb100a.mod_freq_is_set[ i ] = UNSET;
        rs_smb100a.mod_ampl_is_set[ i ] = UNSET;
    }

    rs_smb100a.freq_change_delay = 0.0;

#if defined WITH_PULSE_MODULATION
    rs_smb100a.pulse_mode_state_is_set = UNSET;
    rs_smb100a.pulse_mode_state = UNSET;
    rs_smb100a.double_pulse_mode_is_set = UNSET;
    rs_smb100a.pulse_trig_slope_is_set = UNSET;
    rs_smb100a.pulse_width_is_set = UNSET;
    rs_smb100a.pulse_delay_is_set = UNSET;
    rs_smb100a.double_pulse_delay_is_set = UNSET;
#endif /* WITH_PULSE_MODULATION */

    return 1;
}


/*------------------------------------*
 * Test hook function for the module.
 *------------------------------------*/

int
rs_smb100a_test_hook( void )
{
    /* If a table has been set check that at the frequency the attenuations
       refer to the table is defined and if it is get the attenuation at the
       reference frequency */

    if ( rs_smb100a.use_table )
    {
        if (    rs_smb100a.att_ref_freq < rs_smb100a.min_table_freq
             || rs_smb100a.att_ref_freq > rs_smb100a.max_table_freq )
        {
            print( FATAL, "Reference frequency for attenuation settings of "
                   "%g MHz is not covered by the table.\n",
                   rs_smb100a.att_ref_freq );
            THROW( EXCEPTION );
        }

        rs_smb100a.att_at_ref_freq =
                      rs_smb100a_get_att_from_table( rs_smb100a.att_ref_freq );
    }

    /* Save the current state of the device structure which always has to be
       reset to this state at the start of the experiment */

    rs_smb100a_backup = rs_smb100a;

    /* Set up a few default values */

    if ( ! rs_smb100a.freq_is_set )
    {
        rs_smb100a.freq = RS_SMB100A_TEST_RF_FREQ;
        rs_smb100a.freq_is_set = SET;
    }

    if ( ! rs_smb100a.mod_type_is_set )
    {
        rs_smb100a.mod_type = RS_SMB100A_TEST_MOD_TYPE;
        rs_smb100a.mod_type_is_set = SET;
    }

    if ( ! rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] )
    {
        rs_smb100a.mod_source[ rs_smb100a.mod_type ] =
                                                    RS_SMB100A_TEST_MOD_SOURCE;
        rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] = SET;
    }

    if ( ! rs_smb100a.mod_freq_is_set[ rs_smb100a.mod_type ] )
    {
        rs_smb100a.mod_freq[ rs_smb100a.mod_type ] = RS_SMB100A_TEST_MOD_FREQ;
        rs_smb100a.mod_freq_is_set[ rs_smb100a.mod_type ] = SET;
    }

    if ( ! rs_smb100a.mod_ampl_is_set[ rs_smb100a.mod_type ] )
    {
        rs_smb100a.mod_ampl[ rs_smb100a.mod_type ] = RS_SMB100A_TEST_MOD_AMPL;
        rs_smb100a.mod_ampl_is_set[ rs_smb100a.mod_type ] = SET;
    }

#if defined WITH_PULSE_MODULATION
    if ( ! rs_smb100a.pulse_mode_state_is_set )
    {
        rs_smb100a.pulse_mode_state = RS_SMB100A_TEST_PULSE_MODE_STATE;
        rs_smb100a.pulse_mode_state_is_set = SET;
    }

    if ( ! rs_smb100a.pulse_trig_slope_is_set )
    {
        rs_smb100a.pulse_trig_slope_is_set = RS_SMB100A_TEST_PULSE_TRIG_SLOPE;
        rs_smb100a.pulse_trig_slope_is_set = SET;
    }

    if ( ! rs_smb100a.double_pulse_mode_is_set )
    {
        rs_smb100a.double_pulse_mode = RS_SMB100A_TEST_DOUBLE_PULSE_MODE;
        rs_smb100a.double_pulse_mode_is_set = SET;
    }

    if ( ! rs_smb100a.double_pulse_delay_is_set )
    {
        rs_smb100a.double_pulse_delay = RS_SMB100A_TEST_DOUBLE_PULSE_DELAY;
        if ( rs_smb100a.pulse_width_is_set )
            rs_smb100a.double_pulse_delay += rs_smb100a.pulse_width;
        rs_smb100a.double_pulse_delay_is_set = SET;
    }
    if ( ! rs_smb100a.pulse_width_is_set )
    {
        rs_smb100a.pulse_width = RS_SMB100A_TEST_PULSE_WIDTH;
        rs_smb100a.pulse_width_is_set = SET;
    }

    if ( ! rs_smb100a.pulse_delay_is_set )
    {
        rs_smb100a.pulse_delay = RS_SMB100A_TEST_PULSE_DELAY;
        rs_smb100a.pulse_delay_is_set = SET;
    }

#endif /* WITH_PULSE_MODULATION */

    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
rs_smb100a_exp_hook( void )
{
    /* Restore device structure to the state at the start of the test run */

    rs_smb100a = rs_smb100a_backup;

    if ( ! rs_smb100a_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device %s failed.\n", DEVICE_NAME );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
rs_smb100a_end_of_exp_hook( void )
{
    rs_smb100a_finished( );

    rs_smb100a = rs_smb100a_backup;

    return 1;
}


/*------------------------------------------*
 * For final work before module is unloaded
 *------------------------------------------*/

void
rs_smb100a_exit_hook( void )
{
    if ( rs_smb100a.table_file != NULL )
        rs_smb100a.table_file = T_free( rs_smb100a.table_file );

    if ( rs_smb100a.use_table && rs_smb100a.att_table != NULL )
        rs_smb100a.att_table = T_free( rs_smb100a.att_table );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*
 * Switches RF output on or off, or, if called with no argument,
 * returns 0 or 1, indicating that RF output is off or on.
 *---------------------------------------------------------------*/

Var_T *
synthesizer_state( Var_T * v )
{
    bool state;


    if ( v == NULL )              /* i.e. return the current state */
        switch( FSC2_MODE )
        {
            case PREPARATION :
                no_query_possible( );

            case TEST :
                return vars_push( INT_VAR, ( long ) rs_smb100a.state );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) ( rs_smb100a.state =
                                            rs_smb100a_get_output_state( ) ) );
        }

    state = get_boolean( v );
    too_many_arguments( v );

    rs_smb100a.state = state;

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, ( long ) state );

    return vars_push( INT_VAR, ( long ) rs_smb100a_set_output_state( state ) );
}


/*---------------------------------------------------------------------*
 * Function sets or returns (if called with no argument) the frequency
 * of the synthesizer. If called for setting the frequency before the
 * experiment is started the frequency value is stored and set in the
 * setup phase of the experiment. The frequency set the first time the
 * function is called is also set as the start frequency to be used in
 * calls of 'synthesizer_reset_frequency'. The function can only be
 * called once in the PREPARATIONS section, further calls just result
 * in a warning and the new value isn't accepted.
 *---------------------------------------------------------------------*/

Var_T *
synthesizer_frequency( Var_T * v )
{
    double freq;
    double att;


    if ( v == NULL )              /* i.e. return the current frequency */
    {
        if ( FSC2_MODE != EXPERIMENT )
        {
            if ( ! rs_smb100a.freq_is_set )
            {
                print( FATAL, "RF frequency hasn't been set yet.\n" );
                THROW( EXCEPTION );
            }
            else
                return vars_push( FLOAT_VAR, rs_smb100a.freq );
        }

        rs_smb100a.freq = rs_smb100a_get_frequency( );
        return vars_push( FLOAT_VAR, rs_smb100a.freq );
    }

    freq = get_double( v, "RF frequency" );

    if ( freq < 0 )
    {
        print( FATAL, "Invalid negative RF frequency.\n" );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.freq );
        else
            THROW( EXCEPTION );
    }

    too_many_arguments( v );

    /* In test run stop program if value is out of range while in real run
       just keep the current value on errors */

    if ( freq < MIN_FREQ || freq > MAX_FREQ )
    {
        print( FATAL, "RF frequency (%f MHz) not within synthesizers range "
               "(%.3f kHz - %g Mhz).\n", 1.0e-6 * freq, 1.0e-3 * MIN_FREQ,
               1.0e-6 * MAX_FREQ );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.freq );
        else
            THROW( EXCEPTION );
    }

    rs_smb100a_check_mod_ampl( freq );

    switch ( FSC2_MODE )
    {
        case PREPARATION :
            rs_smb100a.freq = rs_smb100a.start_freq = freq;
            rs_smb100a.freq_is_set = SET;
            rs_smb100a.start_freq_is_set = SET;
            break;

        case TEST :
            rs_smb100a.freq = freq;
            rs_smb100a.freq_is_set = SET;
            if ( ! rs_smb100a.start_freq_is_set )
            {
                rs_smb100a.start_freq = freq;
                rs_smb100a.start_freq_is_set = SET;
            }

            /* Calculate the attenuation needed to level out the non-flatness
               of the RF field in the resonator if a table has been set - in
               the test run we only do this to check that we stay within all
               the limits */

            if ( rs_smb100a.use_table )
                rs_smb100a.real_attenuation = rs_smb100a_get_att( freq );
            break;

        case EXPERIMENT :
            if ( ! rs_smb100a.start_freq_is_set )
            {
                rs_smb100a.start_freq = freq;
                rs_smb100a.start_freq_is_set = SET;
            }

            /* Take care of setting the correct attenuation to level out the
               non-flatness of the RF field in the resonator if a table has
               been set */

            if ( rs_smb100a.use_table )
            {
                att = rs_smb100a_get_att( freq );
                if ( att != rs_smb100a.real_attenuation )
                {
                    rs_smb100a_set_attenuation( att );
                    rs_smb100a.real_attenuation = att;
                }
            }

            /* Finally set the frequency */

            rs_smb100a.freq = rs_smb100a_set_frequency( freq );
    }

    return vars_push( FLOAT_VAR, freq );
}


/*-----------------------------------------------------------------------*
 * Function sets or returns (if called with no argument) the attenuation
 * of the synthesizer. If called for setting the attenuation before the
 * experiment is started the attenuation value is stored and set in the
 * setup phase of the attenuation. The function can only be called once
 * in the PREPARATIONS section, further calls just result in a warning
 * and the new value isn't accepted.
 *-----------------------------------------------------------------------*/

Var_T *
synthesizer_attenuation( Var_T * v )
{
    double att;


    if ( v == NULL )              /* i.e. return the current attenuation */
    {
        if ( FSC2_MODE != EXPERIMENT )
        {
            if ( ! rs_smb100a.attenuation_is_set )
            {
                if ( FSC2_MODE == TEST )
                    return vars_push( FLOAT_VAR, MAX_ATTEN );
                else
                {
                    print( FATAL, "RF attenuation has not been set yet.\n" );
                    THROW( EXCEPTION );
                }
            }
            else
                return vars_push( FLOAT_VAR, rs_smb100a.attenuation );
        }

        return vars_push( FLOAT_VAR, rs_smb100a_get_attenuation( ) );
    }

    att = get_double( v, "RF attenuation" );

    too_many_arguments( v );

    /* Check that attenuation is within valid range, if not throw exception
       in test run, but in real run just don't change the attenuation */

    if ( att > rs_smb100a.min_attenuation || att < MAX_ATTEN )
    {
        print( FATAL, "RF attenuation (%g dBm) not within valid range (%g dBm "
               "to %g dBm).\n", att, MAX_ATTEN, rs_smb100a.min_attenuation );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.attenuation );
        else
            THROW( EXCEPTION );
    }

    switch ( FSC2_MODE )
    {
        case PREPARATION :
            if ( rs_smb100a.attenuation_is_set )
            {
                print( SEVERE, "RF attenuation has already been set to %g "
                       "dBm, keeping old value.\n", rs_smb100a.attenuation );
                return vars_push( FLOAT_VAR, rs_smb100a.attenuation );
            }

            rs_smb100a.attenuation = att;
            rs_smb100a.attenuation_is_set = SET;
            break;

        case TEST :
            rs_smb100a.attenuation = rs_smb100a.real_attenuation = att;
            rs_smb100a.attenuation_is_set = SET;
            break;

        case EXPERIMENT :
            rs_smb100a.attenuation = rs_smb100a.real_attenuation =
                                             rs_smb100a_set_attenuation( att );
            break;
    }

    return vars_push( FLOAT_VAR, att );
}


/*-------------------------------------------------------------*
 * Sets (or returns) the minimum attentuation that can be set.
 *-------------------------------------------------------------*/

Var_T *
synthesizer_minimum_attenuation( Var_T * v )
{
    double min_atten;


    if ( v == NULL )          /* i.e. return the current minimum attenuation */
        return vars_push( FLOAT_VAR, rs_smb100a.min_attenuation );


    min_atten = get_double( v, "minimum RF attenuation" );

    too_many_arguments( v );

    if ( min_atten > MIN_MIN_ATTEN )
    {
        print( FATAL, "Minimum attenuation must be below %g dBm.\n",
               MIN_MIN_ATTEN );
        THROW( EXCEPTION );
    }

    if ( min_atten < MAX_ATTEN )
    {
        print( FATAL, "Minimum attenuation must be more than %g dBm.\n",
               MAX_ATTEN );
        THROW( EXCEPTION );
    }

    rs_smb100a.min_attenuation = min_atten;

    return vars_push( FLOAT_VAR, rs_smb100a.min_attenuation );
}

/*-----------------------------------------------------------*
 * Function sets or returns (if called with no argument) the
 * step frequency for RF sweeps.
 *-----------------------------------------------------------*/

Var_T *
synthesizer_step_frequency( Var_T * v )
{
    if ( v != NULL )
    {
        /* Allow setting of the step frequency in the PREPARATIONS section
           only once */

        if ( FSC2_MODE == PREPARATION && rs_smb100a.step_freq_is_set )
        {
            print( SEVERE, "RF step frequency has already been set to %f MHz, "
                   "keeping old value.\n", 1.0e-6 * rs_smb100a.step_freq );
            return vars_push( FLOAT_VAR, rs_smb100a.step_freq );
        }

        rs_smb100a.step_freq = get_double( v, "RF step frequency" );
        rs_smb100a.step_freq_is_set = SET;

        too_many_arguments( v );
    }
    else if ( ! rs_smb100a.step_freq_is_set )
    {
        print( FATAL, "RF step frequency has not been set yet.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, rs_smb100a.step_freq );
}


/*---------------------------------------------------------------------*
 * Function increments the frequency by a single sweep-step-sized step
 *---------------------------------------------------------------------*/

Var_T *
synthesizer_sweep_up( Var_T * v  UNUSED_ARG )
{
    double att;


    if ( ! rs_smb100a.step_freq_is_set )
    {
        print( FATAL, "RF step frequency hasn't been set.\n" );
        THROW( EXCEPTION );
    }

    rs_smb100a.freq += rs_smb100a.step_freq;

    /* Check that frequency stays within the synthesizers range */

    if ( rs_smb100a.freq < MIN_FREQ )
    {
        print( FATAL, "RF frequency dropping below lower limit of %f kHz.\n",
               1.0e-3 * MIN_FREQ );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.freq );
        else
            THROW( EXCEPTION );
    }

    if ( rs_smb100a.freq > MAX_FREQ )
    {
        print( FATAL, "RF frequency increased above upper limit of %f MHz.\n",
               1.0e-6 * MAX_FREQ );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.freq );
        else
            THROW( EXCEPTION );
    }

    /* Check that the modulation amplitude isn't too high for the new
       frequency */

    rs_smb100a_check_mod_ampl( rs_smb100a.freq );

    if ( FSC2_MODE == TEST )
        rs_smb100a.real_attenuation = rs_smb100a_get_att( rs_smb100a.freq );
    else
    {
        att = rs_smb100a_get_att( rs_smb100a.freq );
        if ( att != rs_smb100a.real_attenuation )
        {
            rs_smb100a.real_attenuation = att;
            rs_smb100a_set_attenuation( att );
        }

        rs_smb100a_set_frequency( rs_smb100a.freq );
    }

    return vars_push( FLOAT_VAR, rs_smb100a.freq );
}


/*---------------------------------------------------------------------*
 * Function decrements the frequency by a single sweep-step-sized step
 *---------------------------------------------------------------------*/

Var_T *
synthesizer_sweep_down( Var_T * v  UNUSED_ARG )
{
    Var_T *nv;


    rs_smb100a.step_freq *= -1.0;
    nv = synthesizer_sweep_up( NULL );
    rs_smb100a.step_freq *= -1.0;
    return nv;
}


/*----------------------------------------------------*
 * Function resets the frequency to the initial value
 *----------------------------------------------------*/

Var_T *
synthesizer_reset_frequency( Var_T * v  UNUSED_ARG )
{
    if ( ! rs_smb100a.start_freq_is_set )
    {
        print( FATAL, "No RF frequency has been set yet, so can't do a "
               "frequency reset.\n" );
        THROW( EXCEPTION );
    }

    /* Check that the modulation amplitude isn't too high for the new
       frequency */

    rs_smb100a_check_mod_ampl( rs_smb100a.start_freq );

    if ( FSC2_MODE == TEST )
        rs_smb100a.freq = rs_smb100a.start_freq;
    else
        rs_smb100a.freq = rs_smb100a_set_frequency( rs_smb100a.start_freq );

    return vars_push( FLOAT_VAR, rs_smb100a.freq );
}


/*---------------------------------------------------------------------------*
 * Function for reading in a table file of  frequency-dependend attenuations
 *---------------------------------------------------------------------------*/

Var_T *
synthesizer_use_table( Var_T * v )
{
    FILE *tfp = NULL;
    char *tfname;


    CLOBBER_PROTECT( tfp );

    /* Try to figure out the name of the table file - if no argument is given
       use the default table file, otherwise use the user supplied file name */

    if ( v == NULL )
    {
        if ( DEFAULT_TABLE_FILE[ 0 ] ==  '/' )
            rs_smb100a.table_file = T_strdup( DEFAULT_TABLE_FILE );
        else
            rs_smb100a.table_file = get_string( "%s%s%s", libdir,
                                               slash( libdir ),
                                               DEFAULT_TABLE_FILE );

        if ( ( tfp = rs_smb100a_open_table( rs_smb100a.table_file ) ) == NULL )
        {
            print( FATAL, "Default table file '%s' not found.\n",
                   rs_smb100a.table_file );
            rs_smb100a.table_file = T_free( rs_smb100a.table_file );
            THROW( EXCEPTION );
        }
    }
    else
    {
        vars_check( v, STR_VAR );

        tfname = T_strdup( v->val.sptr );

        too_many_arguments( v );

        TRY
        {
            tfp = rs_smb100a_find_table( &tfname );
            rs_smb100a.table_file = tfname;
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( tfname );
            RETHROW( );
        }
    }

    /* Now try to read in the table file */

    TRY
    {
        rs_smb100a_read_table( tfp );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( tfp );
        rs_smb100a.table_file = T_free( rs_smb100a.table_file );
        RETHROW( );
    }

    fclose( tfp );
    rs_smb100a.table_file = T_free( rs_smb100a.table_file );
    rs_smb100a.use_table = SET;

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

Var_T *
synthesizer_att_ref_freq( Var_T * v )
{
    double freq;


    /* Without an argument just return the reference frequency setting */

    if ( v == NULL )
        return vars_push( FLOAT_VAR, rs_smb100a.att_ref_freq );

    /* Otherwise check the supplied variable */

    freq = get_double( v, "RF attenuation reference frequency" );

    /* Check that the frequency is within the synthesizers range */

    if ( freq > MAX_FREQ || freq < MIN_FREQ )
    {
        print( FATAL, "Reference frequency for attenuation settings of %g MHz "
               "is out of synthesizer range (%f kHz - %f MHz).\n",
               rs_smb100a.att_ref_freq * 1.0e-6,
               MIN_FREQ * 1.0e-3, MAX_FREQ * 1.0e-6 );
        if ( FSC2_MODE == EXPERIMENT )
            return vars_push( FLOAT_VAR, rs_smb100a.freq );
        else
            THROW( EXCEPTION );
    }

    too_many_arguments( v );

    rs_smb100a.att_ref_freq = freq;

    /* If a table has already been loaded calculate the attenuation at the
       reference frequency */

    if ( rs_smb100a.use_table )
    {
        if (    rs_smb100a.att_ref_freq < rs_smb100a.min_table_freq
             || rs_smb100a.att_ref_freq > rs_smb100a.max_table_freq )
        {
            print( FATAL, "Reference frequency for attenuation settings of "
                   "%g MHz is not covered by the table.\n",
                   rs_smb100a.att_ref_freq );
            THROW( EXCEPTION );
        }

        rs_smb100a.att_at_ref_freq = rs_smb100a_get_att_from_table( freq );
    }

    return vars_push( FLOAT_VAR, freq );
}


/*-----------------------------------------------------------------*
 * Function for setting some or all modulation parameters at once.
 * The sequence the parameters are set in don't matter. If the
 * function succeeds 1 (as variable) is returned, otherwise an
 * exception is thrown.
 *-----------------------------------------------------------------*/

Var_T *
synthesizer_modulation( Var_T * v )
{
    unsigned int set = 0;
    int res;
    const char *str[ ] = { "amplitude", "type", "source" };
    const char *mod_source[ ] = { "EXT AC", "EXT DC", "INT" };
    double val;
    double ampl = 0.0;
    double freq = 0.0;
    int what;
    int type = UNDEFINED,
        source = UNDEFINED;
    bool ampl_is_set = UNSET;
    Var_T *func_ptr;
    int acc;


    if ( v == NULL )
    {
        print( FATAL, "Use the functions "
               "'synthesizer_mod_(type|source|freq|ampl)' to determine "
               "modulation settings.\n" );
        THROW( EXCEPTION );
    }

    while ( v )
    {
        if ( ( 1 << ( res = rs_smb100a_get_mod_param( &v, &val, &what ) ) )
             & set )
            print( SEVERE, "Parameter for modulation %s set more than once in "
                   "call of 'synthesizer_modulation'.\n", str[ res ] );
        else
        {
            switch ( res )
            {
                case 0 :                /* setting modulation amplitude */
                    ampl_is_set = SET;
                    ampl = val;
                    break;

                case 1 :                /* setting modulation type */
                    type = what;
                    break;

                case 2 :                /* setting modulation source */
                    source = what;
                    freq = val;
                    break;

                default :                 /* this definitely can't happen... */
                    fsc2_impossible( );
            }
        }
        set |= ( 1 << res );
        v = vars_pop( v );
    }

    if ( type != UNDEFINED )
    {
        func_ptr = func_get( "synthesizer_mod_type", &acc );
        vars_push( INT_VAR, ( long ) type );
        vars_pop( func_call( func_ptr ) );
    }

    if ( ampl_is_set )
    {
        func_ptr = func_get( "synthesizer_mod_ampl", &acc );
        vars_push( FLOAT_VAR, ampl );
        vars_pop( func_call( func_ptr ) );
    }

    if ( source != UNDEFINED )
    {
        func_ptr = func_get( "synthesizer_mod_source", &acc );
        vars_push( STR_VAR, mod_source[ source ] );
        if ( source == MOD_SOURCE_INT )
            vars_push( FLOAT_VAR, freq );
        vars_pop( func_call( func_ptr ) );
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

Var_T *
synthesizer_mod_type( Var_T * v )
{
    int res;


    if ( v == NULL )
    {
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs_smb100a.mod_type_is_set )
                {
                    print( FATAL, "Modulation type hasn't been set yet.\n" );
                    THROW( EXCEPTION );
                }
                break;

            case TEST :
                if ( ! rs_smb100a.mod_type_is_set )
                {
                    rs_smb100a.mod_type = MOD_TYPE_FM;
                    rs_smb100a.mod_type_is_set = SET;
                }
                break;

            case EXPERIMENT :
                rs_smb100a.mod_type = rs_smb100a_get_mod_type( );
        }

        return vars_push( INT_VAR, rs_smb100a.mod_type != UNDEFINED ?
                                                  rs_smb100a.mod_type : - 1L );
    }

    vars_check( v, STR_VAR | INT_VAR );

    if ( v->type == INT_VAR )
    {
        res = ( int ) v->val.lval;

        if ( res < 0 || res >= NUM_MOD_TYPES )
        {
            print( FATAL, "Invalid modulation type %d.\n", res );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( ( res = is_in( v->val.sptr, mod_types, 4 ) ) == UNDEFINED )
        {
            print( FATAL, "Invalid modulation type '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    rs_smb100a.mod_type = res;
    rs_smb100a.mod_type_is_set = SET;

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_mod_type( res );

    return vars_push( INT_VAR, ( long ) res );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

Var_T *
synthesizer_mod_source( Var_T * v )
{
    int source = 0;
    const char *sources[ ] = { "EXT AC", "AC", "EXT DC", "DC", "INT" };
    double freq = 0.0;


    if ( v == NULL )
    {
        if ( ! rs_smb100a.mod_type_is_set )
        {
            print( FATAL, "Can't determine modulation source as long as "
                   "modulation type isn't set.\n" );
            THROW( EXCEPTION );
        }

        if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
        {
            print( FATAL, "Can't determine modulation source when modulation "
                   "is off.\n" );
            THROW( EXCEPTION );
        }

        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] )
                {
                    print( FATAL, "Modulation source for %s modulation "
                           "hasn't been set yet.\n",
                           mod_types[ rs_smb100a.mod_type ] );
                    THROW( EXCEPTION );
                }
                break;

            case TEST :
                if ( ! rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] )
                {
                    rs_smb100a.mod_source[ rs_smb100a.mod_type ] =
                                                                 MOD_SOURCE_AC;
                    rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] = SET;
                }
                break;

            case EXPERIMENT :
                rs_smb100a.mod_source[ rs_smb100a.mod_type ] =
                           rs_smb100a_get_mod_source( rs_smb100a.mod_type, &freq );
                break;
        }

        return vars_push( INT_VAR,
                          rs_smb100a.mod_source[ rs_smb100a.mod_type ] );
    }

    if ( ! rs_smb100a.mod_type_is_set )
    {
        print( FATAL, "Can't set modulation source as long as modulation type "
               "isn't set.\n" );
        THROW( EXCEPTION );
    }

    if ( v->type & ( INT_VAR | FLOAT_VAR ))
    {
        if ( v->type == INT_VAR )
            print( WARN, "Integer value used as internal modulation "
                   "frequency.\n" );
        freq = get_double( v, "modulation frequency" );
        source = MOD_SOURCE_INT;
    }
    else
    {
        switch ( is_in( v->val.sptr, sources, NUM_ELEMS( sources ) ) )
        {
            case 0 : case 1 :
                source = MOD_SOURCE_AC;
                break;

            case 2 : case 3 :
                source = MOD_SOURCE_DC;
                break;

            case 4 :
                source = MOD_SOURCE_INT;
                break;

            default :
                print( FATAL, "Invalid modulation source '%s'.\n",
                       v->val.sptr );
                THROW( EXCEPTION );
        }
    }

    if ( source == MOD_SOURCE_INT )
    {
        if (    ( v = vars_pop( v ) ) == NULL
             || ! ( v->type & ( INT_VAR | FLOAT_VAR ) ) )
        {
            print( FATAL, "Argument setting to internal modulation must "
                   "be immediately followed by the modulation frequency.\n" );
            THROW( EXCEPTION );
        }

        if ( v->type == INT_VAR )
            print( WARN, "Integer value used as internal modulation "
                   "frequency.\n" );

        freq = get_double( v, "modulation frequency" );

        if ( freq < MIN_INT_MOD_FREQ )
        {
            if ( 0.9999 * MIN_INT_MOD_FREQ - freq > 0.0 )
                freq = MIN_INT_MOD_FREQ;
            else
            {
                print( FATAL, "Internal modulation frequency of %f Hz is too "
                       "low, minimum is %f Hz.\n", freq, MIN_INT_MOD_FREQ );
                THROW( EXCEPTION );
            }
        }

        if ( freq > MAX_INT_MOD_FREQ )
        {
            if ( 1.0001 * MIN_INT_MOD_FREQ - freq > 0.0 )
                freq = MAX_INT_MOD_FREQ;
            else
            {
                print( FATAL, "Internal modulation frequency of %f kHz is too "
                       "high, maximum is %f kHz.\n", freq * 1.0e-3,
                       MIN_INT_MOD_FREQ * 1.0e-3 );
                THROW( EXCEPTION );
            }
        }
    }

    too_many_arguments( v );

    if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
    {
        print( FATAL, "Can't set modulation source while modulation is "
               "off.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
        rs_smb100a_set_mod_source( rs_smb100a.mod_type, source, freq );

    rs_smb100a.mod_source[ rs_smb100a.mod_type ] = source;
    rs_smb100a.mod_source_is_set[ rs_smb100a.mod_type ] = SET;

    if ( source == MOD_SOURCE_INT )
    {
        rs_smb100a.mod_freq[ rs_smb100a.mod_type ] = freq;
        rs_smb100a.mod_freq_is_set[ rs_smb100a.mod_type ] = SET;
    }

    return vars_push( INT_VAR, rs_smb100a.mod_source );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

Var_T *
synthesizer_mod_freq( Var_T * v )
{
    double freq;


    if ( v == NULL )
    {
        if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
        {
            print( FATAL, "Can't determine modulation frequency, modulation "
                   "is off.\n" );
            THROW( EXCEPTION );
        }

        if ( rs_smb100a.mod_source[ rs_smb100a.mod_type ] != MOD_SOURCE_INT )
        {
            print( FATAL, "Can't determine modulation frequency, "
                   "synthesizer uses external modulation.\n" );
            THROW( EXCEPTION );
        }

        if (    FSC2_MODE == PREPARATION
             && ! rs_smb100a.mod_freq_is_set[ rs_smb100a.mod_type ] )
            print( FATAL, "Can't determine modulation frequency, it has "
                   "not been set yet.\n" );

        return vars_push( FLOAT_VAR,
                          rs_smb100a.mod_freq[ rs_smb100a.mod_type ] );
    }

    if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
    {
        print( SEVERE, "Can't set modulation frequency, modulation "
               "is off.\n" );
        return vars_push( FLOAT_VAR, 0.0 );
    }

    if ( rs_smb100a.mod_source[ rs_smb100a.mod_type ] != MOD_SOURCE_INT )
    {
        print( SEVERE, "Can't set modulation frequency, synthesizer uses "
               "external modulation.\n" );
        return vars_push( FLOAT_VAR, 0.0 );
    }

    freq = get_double( v, "modulation frequency" );

    if ( freq < MIN_INT_MOD_FREQ )
    {
        if ( 0.9999 * MIN_INT_MOD_FREQ - freq > 0.0 )
            freq = MIN_INT_MOD_FREQ;
        else
        {
            print( FATAL, "Internal modulation frequency of %f Hz is too "
                   "low, minimum is %f Hz.\n", freq, MIN_INT_MOD_FREQ );
            THROW( EXCEPTION );
        }
    }

    if ( freq > MAX_INT_MOD_FREQ )
    {
        if ( 1.0001 * MIN_INT_MOD_FREQ - freq > 0.0 )
            freq = MAX_INT_MOD_FREQ;
        else
        {
            print( FATAL, "Internal modulation frequency of %f kHz is too "
                   "high, maximum is %f kHz.\n", freq * 1.0e-3,
                   MIN_INT_MOD_FREQ * 1.0e-3 );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    if ( FSC2_MODE != EXPERIMENT )
        rs_smb100a_set_mod_source( rs_smb100a.mod_type, MOD_SOURCE_INT, freq );

    rs_smb100a.mod_freq[ rs_smb100a.mod_type ] = freq;
    rs_smb100a.mod_freq_is_set[ rs_smb100a.mod_type ] = SET;

    return vars_push( FLOAT_VAR, freq );
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

Var_T *
synthesizer_mod_ampl( Var_T * v )
{
    double ampl;
    size_t i;


    if ( v == NULL )
    {
        if ( ! rs_smb100a.mod_type_is_set )
        {
            print( FATAL, "Can't determine modulation amplitude as long as "
                   "modulation type isn't set.\n" );
            THROW( EXCEPTION );
        }

        if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
        {
            print( FATAL, "Can't determine modulation amplitude when "
                   "modulation is off.\n" );
            THROW( EXCEPTION );
        }

        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! rs_smb100a.mod_ampl_is_set[ rs_smb100a.mod_type ] )
                {
                    print( FATAL, "Modulation amplitude for %s modulation "
                           "hasn't been set yet.\n",
                           mod_types[ rs_smb100a.mod_type ] );
                    THROW( EXCEPTION );
                }
                break;

            case EXPERIMENT :
                rs_smb100a.mod_ampl[ rs_smb100a.mod_type ] =
                                rs_smb100a_get_mod_ampl( rs_smb100a.mod_type );
                break;
        }

        return vars_push( FLOAT_VAR,
                          rs_smb100a.mod_ampl[ rs_smb100a.mod_type ] );
    }

    if ( ! rs_smb100a.mod_type_is_set )
    {
        print( FATAL, "Can't set modulation amplitude as long as modulation "
               "type isn't set.\n" );
        THROW( EXCEPTION );
    }

    if ( rs_smb100a.mod_type == MOD_TYPE_OFF )
    {
        print( FATAL, "Can't set modulation amplitude while modulation is "
               "off.\n" );
        THROW( EXCEPTION );
    }

    if ( rs_smb100a.mod_type != MOD_TYPE_AM && ! rs_smb100a.freq_is_set )
    {
        print( FATAL, "Can't set modulation amplitued while RF frequency "
               "hasn't been set.\n" );
        THROW( EXCEPTION );
    }

    ampl = get_double( v, "modulation amplitude" );

    too_many_arguments( v );

    if ( ampl < 0.0 )
    {
        print( FATAL, "Invalid negative %s modulation amplitude of %g %s.\n",
               mod_types[ rs_smb100a.mod_type ],
               rs_smb100a.mod_type == MOD_TYPE_FM ? "kHz" :
                       ( rs_smb100a.mod_type == MOD_TYPE_AM ? "%%" : "rad" ) );
        THROW( EXCEPTION );
    }

    switch ( rs_smb100a.mod_type )
    {
        case MOD_TYPE_FM :
            for ( i = 0; i < num_fm_mod_ranges; i++ )
                if ( rs_smb100a.freq <= fm_mod_ranges[ i ].upper_limit_freq )
                    break;

            fsc2_assert( i < num_fm_mod_ranges );

            if ( ampl > fm_mod_ranges[ i ].upper_limit )
            {
                print( FATAL, "FM modulation amplitude of %.1f kHz is too "
                       "large, valid range is 0 - %.1f kHz for the current "
                       "RF frequency of %.4f MHz.\n",
                       ampl * 1.0e-3, fm_mod_ranges[ i ].upper_limit * 1.0e-3,
                       rs_smb100a.freq * 1.0e-6 );
                THROW( EXCEPTION );
            }
            break;

        case MOD_TYPE_AM :
            if ( ampl > MAX_AM_AMPL )
            {
                print( FATAL, "AM modulation amplitude of %.1f %% is too "
                       "large, valid range is 0 - %.2f %%.\n",
                       ampl, ( double ) MAX_AM_AMPL );
                THROW( EXCEPTION );
            }
            break;

        case MOD_TYPE_PM :
            for ( i = 0; i < num_pm_mod_ranges; i++ )
                if ( rs_smb100a.freq <= pm_mod_ranges[ i ].upper_limit_freq )
                    break;

            fsc2_assert( i < num_pm_mod_ranges );

            if ( ampl > pm_mod_ranges[ i ].upper_limit )
            {
                print( FATAL, "PM modulation amplitude of %.2f rad is too "
                       "large, valid range is 0 - %.2f rad for the current "
                       "RF frequency of %.4f MHz.\n",
                       ampl, pm_mod_ranges[ i ].upper_limit,
                       rs_smb100a.freq * 1.0e-6 );
                THROW( EXCEPTION );
            }
            break;

        default :                         /* this can never happen... */
            fsc2_impossible( );
    }

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_mod_ampl( rs_smb100a.mod_type, ampl );

    rs_smb100a.mod_ampl[ rs_smb100a.mod_type ] = ampl;
    rs_smb100a.mod_ampl_is_set[ rs_smb100a.mod_type ] = SET;

    return vars_push( FLOAT_VAR, ampl );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

#if defined WITH_PULSE_MODULATION

Var_T *
synthesizer_pulse_state( Var_T * v )
{
    bool state;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && ! rs_smb100a.pulse_mode_state_is_set )
        {
            print( FATAL, "Pulse mode state hasn't been set yet.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( INT_VAR, ( int ) rs_smb100a.pulse_mode_state );
    }

    state = get_boolean( v );

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_pulse_state( state );

    rs_smb100a.pulse_mode_state = state;
    rs_smb100a.pulse_mode_state_is_set = SET;

    return vars_push( INT_VAR, ( int ) rs_smb100a.pulse_mode_state );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_pulse_trigger_slope( Var_T * v )
{
    bool state;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && ! rs_smb100a.pulse_trig_slope_is_set )
        {
            print( FATAL, "Pulse trigger slope hasn't been set yet.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( INT_VAR, ( int ) rs_smb100a.pulse_trig_slope );
    }

    vars_check( v, STR_VAR );

    if (    ! strcasecmp( v->val.sptr, "POS" )
         || ! strcasecmp( v->val.sptr, "POSITIVE" ) )
        state = SLOPE_RAISE;
    else if (    ! strcasecmp( v->val.sptr, "NEG" )
              || ! strcasecmp( v->val.sptr, "NEGATIVE" ) )
        state = SLOPE_FALL;
    else
    {
        print( FATAL, "Invalid argument.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_pulse_trig_slope( state );

    rs_smb100a.pulse_trig_slope = state;
    rs_smb100a.pulse_trig_slope_is_set = SET;

    return vars_push( INT_VAR, ( int ) rs_smb100a.pulse_trig_slope );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_pulse_width( Var_T * v )
{
    double width;
    long ticks;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && ! rs_smb100a.pulse_width_is_set )
        {
            print( FATAL, "Pulse width hasn't been set yet.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( INT_VAR, ( int ) rs_smb100a.pulse_width );
    }

    width = get_double( v, "pulse width" );

    if ( width < 0.0 )
    {
        print( FATAL, "Invalid negative pulse width.\n" );
        THROW( EXCEPTION );
    }

    ticks = lrnd( width / MIN_PULSE_WIDTH );

    if ( ticks == 0 || ticks > lrnd( MAX_PULSE_WIDTH / MIN_PULSE_WIDTH ) )
    {
        print( FATAL, "Invalid pulse width of %s, allowed range is %ld ns "
               "to %.1f s\n", rs_smb100a_pretty_print( width ),
               lrnd( MIN_PULSE_WIDTH * 1.0e9 ), MAX_PULSE_WIDTH );
        THROW( EXCEPTION );
    }

    if ( fabs( ticks * MIN_PULSE_WIDTH - width ) > 0.01 * MIN_PULSE_WIDTH )
    {
        char *t = T_strdup( rs_smb100a_pretty_print( width ) );
        print( SEVERE, "Pulse width of %s isn't an integer multiple of %d ns, "
               "changing it to %s\n", t, irnd( MIN_PULSE_WIDTH * 1.0e9 ),
               rs_smb100a_pretty_print( ticks * MIN_PULSE_WIDTH ) );
        T_free( t );
        width = ticks * MIN_PULSE_WIDTH;
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_pulse_width( width );

    rs_smb100a.pulse_width = width;
    rs_smb100a.pulse_width_is_set = SET;

    return vars_push( FLOAT_VAR, width );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_pulse_delay( Var_T * v )
{
    double delay;
    long ticks;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && ! rs_smb100a.pulse_delay_is_set )
        {
            print( FATAL, "Pulse delay hasn't been set yet.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR, rs_smb100a.pulse_delay );
    }

    delay = get_double( v, "pulse delay" );

    if ( delay < 0.0 )
    {
        print( FATAL, "Invalid negative pulse delay.\n" );
        THROW( EXCEPTION );
    }

    ticks = lrnd( delay / MIN_PULSE_DELAY );

    if ( ticks == 0 || ticks > lrnd( MAX_PULSE_DELAY / MIN_PULSE_DELAY ) )
    {
        print( FATAL, "Invalid pulse delay of %s, allowed range is %d ns "
               "to %.1f s\n", rs_smb100a_pretty_print( delay ),
               irnd( MIN_PULSE_DELAY * 1.0e9 ), MIN_PULSE_DELAY );
        THROW( EXCEPTION );
    }

    if ( fabs( ticks * MIN_PULSE_DELAY - delay ) > 0.01 * MIN_PULSE_DELAY )
    {
        char *t = T_strdup( rs_smb100a_pretty_print( delay ) );
        print( SEVERE, "Pulse delay of %s isn't an integer multiple of %d ns, "
               "changing it to %s\n", t, irnd( MIN_PULSE_DELAY * 1.0e9 ),
               rs_smb100a_pretty_print( ticks * MIN_PULSE_DELAY ) );
        T_free( t );
        delay = ticks * MIN_PULSE_DELAY;
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_pulse_delay( delay );

    rs_smb100a.pulse_delay = delay;
    rs_smb100a.pulse_delay_is_set = SET;

    return vars_push( FLOAT_VAR, delay );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_double_pulse_mode( Var_T * v )
{
    bool state;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && ! rs_smb100a.double_pulse_mode_is_set )
        {
            print( FATAL, "Double pulse mode hasn't been set.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( INT_VAR, ( long ) rs_smb100a.double_pulse_mode );
    }

    state = get_boolean( v );

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        rs_smb100a_set_double_pulse_mode( state );

    rs_smb100a.double_pulse_mode = state;
    rs_smb100a.double_pulse_mode_is_set = SET;

    return vars_push( INT_VAR, ( long ) state );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_double_pulse_delay( Var_T * v )
{
    double delay;
    long ticks;


    if ( v == NULL )
    {
        if (    FSC2_MODE == PREPARATION
             && ! rs_smb100a.double_pulse_delay_is_set )
        {
            print( FATAL, "Double pulse delay hasn't been set.\n" );
            THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR, rs_smb100a.double_pulse_delay );
    }

    delay = get_double( v, "double pulse delay" );

    ticks = lrnd( delay / MIN_PULSE_WIDTH );

    if (    ticks < lrnd( MIN_DOUBLE_PULSE_DELAY / MIN_PULSE_WIDTH )
         || ticks > lrnd( MAX_DOUBLE_PULSE_DELAY / MIN_PULSE_WIDTH ) )
    {
        print( FATAL, "Invalid double pulse delay of %s, allowed range is "
               "%ld ns to %.1f s\n", rs_smb100a_pretty_print( delay ),
               lrnd( MIN_DOUBLE_PULSE_DELAY * 1.0e9 ), MAX_DOUBLE_PULSE_DELAY );
        THROW( EXCEPTION );
    }

    if ( fabs( ticks * MIN_PULSE_WIDTH - delay ) > 0.01 * MIN_PULSE_WIDTH )
    {
        char *t = T_strdup( rs_smb100a_pretty_print( delay ) );
        print( SEVERE, "Pulse width of %s isn't an integer multiple of %d ns, "
               "changing it to %s\n", t, irnd( MIN_PULSE_WIDTH * 1.0e9 ),
               rs_smb100a_pretty_print( ticks * MIN_PULSE_WIDTH ) );
        T_free( t );
        delay = ticks * MIN_PULSE_WIDTH;
    }

    rs_smb100a.double_pulse_delay = delay;
    rs_smb100a.double_pulse_delay_is_set = SET;

    if ( FSC2_MODE == EXPERIMENT )
      rs_smb100a_set_double_pulse_delay( delay );

    return vars_push( FLOAT_VAR, delay );
}

#endif /* WITH_PULSE_MODULATION */


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_freq_change_delay( Var_T * v )
{
    double delay;


    if ( v )
    {
        delay = get_double( v, "frequency change delay" );

        too_many_arguments( v );

        if ( delay < 1.0e-3 )
            rs_smb100a.freq_change_delay = 0.0;
        else
            rs_smb100a.freq_change_delay = delay;
    }

    return vars_push( FLOAT_VAR, rs_smb100a.freq_change_delay );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
synthesizer_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            rs_smb100a_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW( );
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
