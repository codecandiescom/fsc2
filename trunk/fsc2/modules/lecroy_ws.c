/*
 *  $Id$
 *
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#define LECROY_WS_MAIN_
#include "lecroy_ws.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

LECROY_WS_T lecroy_ws;

const char *LECROY_WS_Channel_Names[ LECROY_WS_TOTAL_CHANNELS ] =
            { "CH1",  "CH2", "CH3", "CH4",     /* measurement channels */
              "MATH",                          /* math channel */
              "Z1",   "Z2",  "Z3",  "Z4",      /* zoom channels */
              "M1",   "M2",  "M3",  "M4"       /* memory channels */
              "LINE", "EXT", "EXT10" };        /* trigger channels */

/* List of fixed sensivity settings where the range of the offset changes */

double fixed_sens[ ] = { 5.0e-2, 5.0e-1, 10.0 };

/* List of offset factors for the different sensitivity ranges */

int max_offsets[ ] = { 1.0, 10.0, 100.0 };

static LECROY_WS_T lecroy_ws_stored;



/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int lecroy_ws_init_hook( void )
{
    size_t i;


    /* Set global variable to indicate that LAN is needed */

    Need_LAN = SET;

    /* Initialize some variables in the digitizers structure */

    lecroy_ws.num_used_channels = 0;

    lecroy_ws.w                 = NULL;
    lecroy_ws.num_windows       = 0;

    lecroy_ws.is_timebase       = UNSET;
    lecroy_ws.is_interleaved    = UNSET;
    lecroy_ws.is_trigger_source = UNSET;
    lecroy_ws.is_trigger_mode   = UNSET;
    lecroy_ws.is_trigger_delay  = UNSET;

    lecroy_ws.tb_index       = LECROY_WS_TEST_TB_INDEX;
    lecroy_ws.timebase       = TBAS( lecroy_ws.tb_index );
    lecroy_ws.mem_size       = LECROY_WS_LONG_MEM_SIZE;
    lecroy_ws.interleaved    = LECROY_WS_TEST_ILVD_MODE;
    lecroy_ws.trigger_source = LECROY_WS_TEST_TRIG_SOURCE;
    lecroy_ws.trigger_mode   = LECROY_WS_TEST_TRIG_MODE;
    lecroy_ws.trigger_delay  = LECROY_WS_TEST_TRIG_DELAY;
    lecroy_ws.cur_hres      =  hres[ 1 ] + lecroy_ws.tb_index;

    for ( i = LECROY_WS_CH1; i < LECROY_WS_CH_MAX; i++ )
    {
        lecroy_ws.is_sens[ i ]              = UNSET;
        lecroy_ws.sens[ i ]                 = LECROY_WS_TEST_SENSITIVITY;
        lecroy_ws.is_offset[ i ]            = UNSET;
        lecroy_ws.offset[ i ]               = LECROY_WS_TEST_OFFSET;
        lecroy_ws.is_coupling[ i ]          = UNSET;
        lecroy_ws.coupling[ i ]             = LECROY_WS_TEST_COUPLING;
        lecroy_ws.need_high_imp[ i ]        = UNSET;
        lecroy_ws.is_trigger_slope[ i ]     = UNSET;
        lecroy_ws.trigger_slope[ i ]        = LECROY_WS_TEST_TRIG_SLOPE;
        lecroy_ws.is_trigger_coupling[ i ]  = UNSET;
        lecroy_ws.trigger_coupling[ i ]     = LECROY_WS_TEST_TRIG_COUP;
        lecroy_ws.is_trigger_level[ i ]     = UNSET;
        lecroy_ws.trigger_level[ i ]        = LECROY_WS_TEST_TRIG_LEVEL;
        lecroy_ws.is_bandwidth_limiter[ i ] = UNSET;
        lecroy_ws.bandwidth_limiter[ i ]    = LECROY_WS_TEST_BWL;
    }

    for ( i = LECROY_WS_Z1; i <= LECROY_WS_Z4; i++ )
    {
        lecroy_ws.channels_in_use[ i ] = UNSET;
        lecroy_ws.source_ch[ i ]       = LECROY_WS_CH1;
        lecroy_ws.is_avg_setup[ i ]    = UNSET;
    }

    for ( i = LECROY_WS_M1; i <= LECROY_WS_M4; i++ )
        lecroy_ws.channels_in_use[ i ] = UNSET;

    lecroy_ws.lock_state = SET;

    lecroy_ws_stored.w = NULL;

    return 1;
}


/*-----------------------------------*
 * Test hook function for the module
 *-----------------------------------*/

int lecroy_ws_test_hook( void )
{
    lecroy_ws_store_state( &lecroy_ws_stored, &lecroy_ws );
    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int lecroy_ws_exp_hook( void )
{
    /* Reset structure describing the state of the digitizer to what it
       was before the test run */

    lecroy_ws_store_state( &lecroy_ws, &lecroy_ws_stored );

    if ( ! lecroy_ws_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed.\n" );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int lecroy_ws_end_of_exp_hook( void )
{
    lecroy_ws_finished( );
    return 1;
}


/*------------------------------------------*
 * For final work before module is unloaded
 *------------------------------------------*/


void lecroy_ws_exit_hook( void )
{
    lecroy_ws_delete_windows( &lecroy_ws );
    lecroy_ws_delete_windows( &lecroy_ws_stored );
}


/*-------------------------------------------------------*
 * Function returns a string with the name of the device
 *-------------------------------------------------------*/

Var_T *digitizer_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------*
 * Function for creating a new window
 *------------------------------------*/

Var_T *digitizer_define_window( Var_T * v )
{
    double win_start = 0,
           win_width = 0;
    Window_T *w;


    if ( v == NULL || v->next == NULL )
    {
        print( FATAL, "Missing argument(s), absolute window position "
               "(relative to trigger) and window width must be specified.\n" );
        THROW( EXCEPTION );
    }

    /* Get the start point of the window */

    win_start = get_double( v, "window start position" );
    v = vars_pop( v );
    win_width = get_double( v, "window width" );

    /* Allow window width to be zero in test run only... */
    
    if ( ( FSC2_MODE == TEST && win_width < 0.0 ) ||
         ( FSC2_MODE != TEST && win_width <= 0.0 ) )
    {
        print( FATAL, "Zero or negative window width.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    /* Create a new window structure and append it to the list of windows */

    if ( lecroy_ws.w == NULL )
    {
        lecroy_ws.w = w = WINDOW_P T_malloc( sizeof *w );
        w->prev = NULL;
    }
    else
    {
        w = lecroy_ws.w;
        while ( w->next != NULL )
            w = w->next;
        w->next = WINDOW_P T_malloc( sizeof *w->next );
        w->next->prev = w;
        w = w->next;
    }

    w->next = NULL;
    w->num = lecroy_ws.num_windows++ + WINDOW_START_NUMBER;

    w->start = win_start;
    w->width = win_width;

    lecroy_ws_window_check( w, UNSET );

    return vars_push( INT_VAR, w->num );
}


/*--------------------------------------------------------------------*
 * Function for changing the properties of an already existing window
 *--------------------------------------------------------------------*/

Var_T *digitizer_change_window( Var_T * v )
{
    Window_T *w;


    if ( lecroy_ws.w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        print( FATAL, "Missing window ID.\n" );
        THROW( EXCEPTION );
    }

    /* Figure out the window number and test if a window with this number
       exists at all */

    w = lecroy_ws_get_window_by_number( get_strict_long( v, "window ID" ) );
    v = vars_pop( v );

    if ( v == NULL )
    {
        print( FATAL, "Missing window start position argument.\n" );
        THROW( EXCEPTION );
    }

    w->start = get_double( v, "window start position" );
    v = vars_pop( v );

    if ( v == NULL )
    {
        print( FATAL, "Missing window width argument.\n" );
        THROW( EXCEPTION );
    }

    w->width = get_double(  v, "window with" );

    if ( w->width < 0.0 ) {
        print( FATAL, "Negative window width.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    lecroy_ws_window_check( w, UNSET );

    return vars_push( INT_VAR, w->num );
}


/*------------------------------------------------------------------------*
 * Function queries or changes the position of an already existing window
 *------------------------------------------------------------------------*/

Var_T *digitizer_window_position( Var_T * v )
{
    Window_T *w;


    if ( lecroy_ws.w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        print( FATAL, "Missing window ID.\n" );
        THROW( EXCEPTION );
    }

    /* Figure out the window number and test if a window with this number
       exists at all */

    w = lecroy_ws_get_window_by_number( get_strict_long( v, "window ID" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( FLOAT_VAR, w->start );

    w->start = get_double( v, "window start position" );

    too_many_arguments( v );

    lecroy_ws_window_check( w, UNSET );

    return vars_push( FLOAT_VAR, w->start );
}


/*--------------------------------------------------------------------*
 * Function queries or changes he width of an already existing window
 *--------------------------------------------------------------------*/

Var_T *digitizer_window_width( Var_T * v )
{
    Window_T *w;


    if ( lecroy_ws.w == NULL )
    {
        print( FATAL, "No windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    /* Figure out the window number and test if a window with this number
       exists at all */

    w = lecroy_ws_get_window_by_number( get_strict_long( v, "window ID" ) );

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( FLOAT_VAR, w->width );

    w->width = get_double( v, "window width" );

    too_many_arguments( v );

    lecroy_ws_window_check( w, UNSET );

    return vars_push( FLOAT_VAR, w->width );
}


/*-------------------------------------------------------------*
 * Function for determining or setting the tim base (in s/div)
 *-------------------------------------------------------------*/

Var_T *digitizer_timebase( Var_T * v )
{
    double timebase;
    int tb_index = -1;
    size_t i;
    char *t;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_timebase )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, lecroy_ws.timebase );

            case EXPERIMENT :
                lecroy_ws.timebase = lecroy_ws_get_timebase( );
                lecroy_ws.is_timebase = SET;
                return vars_push( FLOAT_VAR, lecroy_ws.timebase );
        }

    timebase = get_double( v, "time base" );

    if ( timebase <= 0 )
    {
        print( FATAL, "Invalid zero or negative time base: %s.\n",
               lecroy_ws_ptime( timebase ) );
        THROW( EXCEPTION );
    }

    /* Pick the allowed timebase nearest to the user requested value, tell
       the user about problems if there's a deviation of more than 1 % */

    for ( i = 0; i < NUM_TBAS - 1; i++ )
        if ( timebase >= TBAS( i ) && timebase <= TBAS( i + 1 ) )
        {
            tb_index = i + ( ( TBAS( i ) / timebase >
                               timebase / TBAS( i + 1 ) ) ? 0 : 1 );
            break;
        }

    if ( tb_index >= 0 &&                                   /* value found ? */
         fabs( timebase - TBAS( tb_index ) ) > timebase * 1.0e-2 )
    {
        t = T_strdup( lecroy_ws_ptime( timebase ) );
        print( WARN, "Can't set timebase to %s, using %s instead.\n",
               t, lecroy_ws_ptime( TBAS( tb_index ) ) );
        T_free( t );
    }

    if ( tb_index < 0 )                                   /* not found yet ? */
    {
        t = T_strdup( lecroy_ws_ptime( timebase ) );

        if ( timebase < TBAS( 0 ) )
        {
            tb_index = 0;
            print( WARN, "Timebase of %s is too short, using %s instead.\n",
                   t, lecroy_ws_ptime( TBAS( tb_index ) ) );
        }
        else
        {
            tb_index = NUM_TBAS - 1;
            print( WARN, "Timebase of %s is too long, using %s instead.\n",
                   t, lecroy_ws_ptime( TBAS( tb_index ) ) );
        }

        T_free( t );
    }

    lecroy_ws.timebase = TBAS( tb_index );
    lecroy_ws.tb_index = tb_index;
    lecroy_ws.is_timebase = SET;
    if ( lecroy_ws.mem_size == LECROY_WS_SHORT_MEM_SIZE )
        lecroy_ws.cur_hres = hres[ 0 ] + lecroy_ws.tb_index;
    else
        lecroy_ws.cur_hres = hres[ 1 ] + lecroy_ws.tb_index;
 
    /* Now check if the trigger delay (in case it's set) fits with the new
       timebase setting, and, based on this, the window positions and widths */

    lecroy_ws.trigger_delay = lecroy_ws_trigger_delay_check( );
    lecroy_ws_all_windows_check( );

    /* In the experiment set the timebase and also the trigger delay, at least
       if it was already set */

    if ( FSC2_MODE == EXPERIMENT )
    {
        lecroy_ws_set_timebase( lecroy_ws.timebase );

        if ( lecroy_ws.is_trigger_delay )
            lecroy_ws_set_trigger_delay( lecroy_ws.trigger_delay );
    }

    return vars_push( FLOAT_VAR, lecroy_ws.timebase );
}


/*--------------------------------------------------------------------*
 * Function to determine or set the measurement mode (i.e. RIS or SS)
 *-------------------------------------------------------------------*/

Var_T *digitizer_interleave_mode( Var_T * v )
{
    bool ilvd;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_interleaved )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) lecroy_ws.interleaved );

            case EXPERIMENT :
                lecroy_ws.interleaved = lecroy_ws_get_interleaved( );
                lecroy_ws.is_interleaved = SET;
                return vars_push( INT_VAR, ( long ) lecroy_ws.interleaved );
        }

    ilvd = get_boolean( v );

    too_many_arguments( v );

    if ( ilvd && lecroy_ws.cur_hres->ppd_ris < 0 )
    {
        print( FATAL, "Can't switch to RIS mode for timebase of %s.\n",
               lecroy_ws_ptime( lecroy_ws.timebase ) );
        THROW( EXCEPTION );
    }

    lecroy_ws.interleaved = ilvd;
    lecroy_ws.is_interleaved = SET;
    lecroy_ws.is_timebase = SET;            /* both must be set to be able */

    lecroy_ws.trigger_delay = lecroy_ws_trigger_delay_check( );
    lecroy_ws_all_windows_check( );

    if ( FSC2_MODE == EXPERIMENT )
    {
        lecroy_ws_set_interleaved( ilvd );

        if ( lecroy_ws.is_trigger_delay )
            lecroy_ws_set_trigger_delay( lecroy_ws.trigger_delay );
    }

    return vars_push( INT_VAR, ( long ) ilvd );
}


/*-------------------------------------------------*
 * Function for setting or quering the memory size
 *-------------------------------------------------*/

Var_T *digitizer_memory_size( Var_T *v )
{
    long mem_size;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
            case TEST :
                return vars_push( INT_VAR, lecroy_ws.mem_size );

            case EXPERIMENT :
                return vars_push( INT_VAR, lecroy_ws_get_memory_size( ) );
        }

    mem_size = get_long( v, "memory size" );

    too_many_arguments( v );

    if ( mem_size <= 0 )
    {
        print( FATAL, "Negative or zero memory size\n" );
        THROW( EXCEPTION );
    }

    if ( mem_size != LECROY_WS_SHORT_MEM_SIZE &&
         mem_size != LECROY_WS_LONG_MEM_SIZE )
    {
        if ( mem_size < LECROY_WS_SHORT_MEM_SIZE )
            lecroy_ws.mem_size = LECROY_WS_SHORT_MEM_SIZE;
        else
            lecroy_ws.mem_size = LECROY_WS_LONG_MEM_SIZE;

        print( SEVERE, "Requested memory size of %ld can't be set, using "
               "%ld instead.\n", mem_size, lecroy_ws.mem_size );
    }

    lecroy_ws.mem_size = LECROY_WS_LONG_MEM_SIZE;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_memory_size( lecroy_ws.mem_size );

    lecroy_ws.trigger_delay = lecroy_ws_trigger_delay_check( );
    lecroy_ws_all_windows_check( );

    return vars_push( INT_VAR, lecroy_ws.mem_size );
}


/*------------------------------------------------------------------*
 * Function returns the current record length, which depends on the
 * timebase and mode (SS or RIS) on the one hand, the memory size
 * on the other.
 *------------------------------------------------------------------*/

Var_T *digitizer_record_length( Var_T *v UNUSED_ARG )
{
    if ( v != NULL )
    {
        print( FATAL, "Record length can only be queried\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    return vars_push( INT_VAR, lecroy_ws_curve_length( ) );
}


/*----------------------------------------------------------------*
 * Returns the time difference between two points for the current
 * timebase and mode (i.e. RIS or SS)
 *----------------------------------------------------------------*/

Var_T *digitizer_time_per_point( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == PREPARATION &&
         ( ! lecroy_ws.is_timebase || ! lecroy_ws.is_interleaved ) )
        no_query_possible( );

    return vars_push( FLOAT_VAR, lecroy_ws.interleaved ?
                                 lecroy_ws.cur_hres->tpp_ris :
                                 lecroy_ws.cur_hres->tpp );
}


/*----------------------------------------------------------------*
 * Function for setting or determining the sensitivity (in V/div)
 *----------------------------------------------------------------*/

Var_T *digitizer_sensitivity( Var_T * v )
{
    long channel;
    double sens;


    if ( v == NULL )
    {
        print( FATAL, "No channel specified.\n" );
        THROW( EXCEPTION );
    }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX )
    {
        print( FATAL, "Can't set or obtain sensitivity for channel %s.\n",
               LECROY_WS_Channel_Names[ channel ] );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_sens[ channel ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, lecroy_ws.sens[ channel ] );

            case EXPERIMENT :
                lecroy_ws.sens[ channel ] = lecroy_ws_get_sens( channel );
                return vars_push( FLOAT_VAR, lecroy_ws.sens[ channel ] );
        }

    sens = get_double( v, "sensitivity" );

    too_many_arguments( v );

    if ( sens < 0.0 )
    {
        print( FATAL, "Invalid negative or zero sensitivity.\n" );
        THROW( EXCEPTION );
    }

    if ( sens < 0.9999 * LECROY_WS_MAX_SENS ||
         sens > 1.0001 * LECROY_WS_MIN_SENS_1MOHM )
    {
        print( FATAL, "Requested sensitivity setting is out of range.\n" );
        THROW( EXCEPTION );
    }

    if ( sens < LECROY_WS_MAX_SENS )
        sens = LECROY_WS_MAX_SENS;
    if ( sens > 1.0001 * LECROY_WS_MIN_SENS_1MOHM )
        sens = LECROY_WS_MIN_SENS_1MOHM;

    if ( sens > 1.0001 * LECROY_WS_MIN_SENS_50OHM )
    {
        if ( ( FSC2_MODE == EXPERIMENT || lecroy_ws.is_coupling[ channel ] ) &&
             lecroy_ws.coupling[ channel ] == LECROY_WS_CPL_DC_50_OHM )
        {
            print( FATAL, "Requested sensitivity is out of range for channels "
                   "input coupling.\n" );
            THROW( EXCEPTION );
        }

        if ( sens <= 1.0001 * LECROY_WS_MIN_SENS_50OHM )
            sens = LECROY_WS_MIN_SENS_50OHM;
        else
            lecroy_ws.need_high_imp[ channel ] = SET;
    }                       

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_sens( channel, sens );

    lecroy_ws.sens[ channel ] = sens;
    lecroy_ws.is_sens[ channel ] = SET;

    return vars_push( FLOAT_VAR, lecroy_ws.sens[ channel ] );
}


/*----------------------------------------------------------------*
 * Function for setting or determining the vertical offset (in V)
 *----------------------------------------------------------------*/

Var_T *digitizer_offset( Var_T * v )
{
    long channel;
    double offset;
    ssize_t i = -1;
    size_t fs_index = 0;



    if ( v == NULL )
    {
        print( FATAL, "No channel specified.\n" );
        THROW( EXCEPTION );
    }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX )
    {
        print( FATAL, "Can't set or obtain offset for channel %s.\n",
               LECROY_WS_Channel_Names[ channel ] );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_offset[ channel ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, lecroy_ws.offset[ channel ] );

            case EXPERIMENT :
                lecroy_ws.offset[ channel ] =
                                              lecroy_ws_get_offset( channel );
                return vars_push( FLOAT_VAR, lecroy_ws.offset[ channel ] );
        }

    offset = get_double( v, "offset" );

    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION && ! lecroy_ws.is_sens[ channel ] )
    {
        print( FATAL, "Can't set offset unless the channels sensitivity "
               "has been set.\n" );
        THROW( EXCEPTION );
    }

    for ( i = 1; i < ( ssize_t ) NUM_ELEMS( fixed_sens ); i++ )
        if ( lecroy_ws.sens[ channel ] <= 0.9999 * fixed_sens[ i ] )
        {
            fs_index = i - 1;
            break;
        }

    if ( i < 0 )
        fs_index = NUM_ELEMS( fixed_sens ) - 1;

    if ( fabs( offset ) >= 1.0001 * max_offsets[ fs_index ] )
    {
        print( FATAL, "Offset too large for the currently set channel "
               "sensitivity.\n" );
        THROW( EXCEPTION );
    }

    lecroy_ws.offset[ channel ] = offset;
    lecroy_ws.is_offset[ channel ] = SET;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_offset( channel, offset );

    return vars_push( FLOAT_VAR, lecroy_ws.offset[ channel ] );
}


/*--------------------------------------------------------------*
 * Function for setting the coupling of a (measurement) channel
 *--------------------------------------------------------------*/

Var_T *digitizer_coupling( Var_T * v )
{
    long channel;
    long cpl = LECROY_WS_CPL_INVALID;
    const char *cpl_str[ ] = { "A1M", "D1M", "D50", "GND" };
    size_t i;


    if ( v == NULL )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX )
    {
        print( FATAL, "Can't set or obtain coupling for channel %s.\n",
               LECROY_WS_Channel_Names[ channel ] );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( v == NULL )
        switch( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_coupling[ channel ] )
                    no_query_possible( );
                /* Fall through */
        
            case TEST:
                return vars_push( INT_VAR,
                                  ( long ) lecroy_ws.coupling[ channel ] );

            case EXPERIMENT :
                lecroy_ws.coupling[ channel ] =
                                             lecroy_ws_get_coupling( channel );
                return vars_push( INT_VAR,
                                  ( long ) lecroy_ws.coupling[ channel ] );
        }

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type == STR_VAR )
    {
        for ( i = 0; i < NUM_ELEMS( cpl_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, cpl_str[ i ] ) )
            {
                cpl = i;
                break;
            }
    }
    else
        cpl = get_long( v, "coupling type" );

    if ( cpl < LECROY_WS_CPL_AC_1_MOHM || cpl > LECROY_WS_CPL_GND )
    {
        print( FATAL, "Invalid coupling type.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_coupling( channel, cpl );

    lecroy_ws.is_coupling[ channel ] = SET;
    lecroy_ws.coupling[ channel ] = cpl;

    return vars_push( INT_VAR, cpl );
}


/*---------------------------------------------------*
 * Function sets or determines the bandwidth limiter
 *---------------------------------------------------*/

Var_T *digitizer_bandwidth_limiter( Var_T * v )
{
    long channel;
    long bwl = LECROY_WS_BWL_OFF;


    if ( v == NULL )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX )
    {
        print( FATAL, "Can't set or obtain offset for channel %s.\n",
               LECROY_WS_Channel_Names[ channel ] );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_bandwidth_limiter[ channel] )
                    no_query_possible( );
                /* Fall through */

            case TEST:
                return vars_push( INT_VAR,
                            ( long ) lecroy_ws.bandwidth_limiter[ channel ] );

            case EXPERIMENT :
                lecroy_ws.bandwidth_limiter[ channel ] =
                                   lecroy_ws_get_bandwidth_limiter( channel );
                return vars_push( INT_VAR,
                            ( long ) lecroy_ws.bandwidth_limiter[ channel ] );
        }

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        bwl = get_long( v, "banndwith limiter type" );
        if ( bwl < LECROY_WS_BWL_OFF ||
#if defined LECROY_WS_BWL_200MHZ
             bwl > LECROY_WS_BWL_200MHZ )
#else
             bwl > LECROY_WS_BWL_ON )
#endif
        {
#if defined LECROY_WS_BWL_200MHZ
            print( FATAL, "Invalid argument, bandwidth limiter can only be "
                   "set to 'OFF' (%d), 'ON' (%d) or '200MHZ' "
                   "(%d).\n", LECROY_WS_BWL_OFF, LECROY_WS_BWL_ON,
                   LECROY_WS_BWL_200MHZ );
#else
            print( FATAL, "Invalid argument, bandwidth limiter can only be "
                   "set to 'OFF' (%d) or 'ON' (%d).\n",
                   LECROY_WS_BWL_OFF, LECROY_WS_BWL_ON );
#endif
            THROW( EXCEPTION );
        }
    }
    else if ( v->type == STR_VAR )
    {
        if ( strcasecmp( v->val.sptr, "OFF" ) )
            bwl = LECROY_WS_BWL_OFF;
        else if ( strcasecmp( v->val.sptr, "ON" ) )
            bwl = LECROY_WS_BWL_ON;
#if defined LECROY_WS_BWL_200MHZ
        else if ( strcasecmp( v->val.sptr, "200MHZ" ) )
            bwl = LECROY_WS_BWL_200MHZ;
#endif
        else
#if defined LECROY_WS_BWL_200MHZ
            print( FATAL, "Invalid argument, bandwidth limiter can only be "
                   "set to 'OFF' (%d), 'ON' (%d) or '200MHZ' "
                   "(%d).\n", LECROY_WS_BWL_OFF, LECROY_WS_BWL_ON,
                   LECROY_WS_BWL_200MHZ );
#else
            print( FATAL, "Invalid argument, bandwidth limiter can only be "
                   "set to 'OFF' (%d) or 'ON' (%d).\n",
                   LECROY_WS_BWL_OFF, LECROY_WS_BWL_ON );
#endif
    }
    else
    {
        print( FATAL, "Invalid second argument of function call.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    lecroy_ws.is_bandwidth_limiter[ channel ] = SET;
    lecroy_ws.bandwidth_limiter[ channel ] = bwl;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_bandwidth_limiter( channel, bwl );

    return vars_push( INT_VAR, ( long ) bwl );
}


/*-------------------------------------------------------------------*
 * digitizer_trigger_channel() sets or determines the channel that is
 * used for triggering.
 *-------------------------------------------------------------------*/

Var_T *digitizer_trigger_channel( Var_T * v )
{
    long channel;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_source )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR,
                                  lecroy_ws_translate_channel(
                                                     LECROY_WS_TO_GENERAL,
                                                     lecroy_ws.trigger_source,
                                                     UNSET ) );

            case EXPERIMENT :
                lecroy_ws.trigger_source = lecroy_ws_get_trigger_source( );
                return vars_push( INT_VAR,
                                  lecroy_ws_translate_channel(
                                                     LECROY_WS_TO_GENERAL,
                                                     lecroy_ws.trigger_source,
                                                     UNSET ) );
        }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    switch ( channel )
    {
        case LECROY_WS_CH1 : case LECROY_WS_CH2 :
#if defined LECROY_WS_CH3
        case LECROY_WS_CH3 :
#endif
#if defined LECROY_WS_CH4
        case LECROY_WS_CH4 :
#endif
        case LECROY_WS_LIN :
        case LECROY_WS_EXT :
        case LECROY_WS_EXT10 :
            if ( FSC2_MODE == EXPERIMENT )
                lecroy_ws_set_trigger_source( channel );
            lecroy_ws.trigger_source = channel;
            lecroy_ws.is_trigger_source = SET;
            break;

        default :
            print( FATAL, "Channel %s can't be used as trigger channel.\n",
                   LECROY_WS_Channel_Names[ channel ] );
            THROW( EXCEPTION );
    }

    too_many_arguments( v );

    return vars_push( INT_VAR,
                      lecroy_ws_translate_channel( LECROY_WS_TO_GENERAL,
                                                    lecroy_ws.trigger_source,
                                                    UNSET ) );
}


/*--------------------------------------------------------------------*
 * digitizer_trigger_level() sets or determines the trigger level for
 * one of the possible trigger channels
 *--------------------------------------------------------------------*/

Var_T *digitizer_trigger_level( Var_T * v )
{
    int channel;
    double level;


    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );
    v = vars_pop( v );

    if ( channel == LECROY_WS_LIN ) {
        print( FATAL, "Trigger level for %s can't be determined nor "
               "changed.\n", LECROY_WS_Channel_Names[ LECROY_WS_LIN ] );
        THROW( EXCEPTION );
    }

    if ( ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX ) &&
         channel != LECROY_WS_EXT && channel != LECROY_WS_EXT10 )
    {
        print( FATAL, "Invalid trigger channel.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_level[ channel ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, 
                                  lecroy_ws.trigger_level[ channel ] );

            case EXPERIMENT :
                lecroy_ws.trigger_level[ channel ] =
                                       lecroy_ws_get_trigger_level( channel );
                return vars_push( FLOAT_VAR,
                                  lecroy_ws.trigger_level[ channel ] );
        }
    }

    level = get_double( v, "trigger level" );

    too_many_arguments( v );

    switch ( channel )
    {
        case LECROY_WS_CH1 : case LECROY_WS_CH2 :
#if defined LECROY_WS_CH3
        case LECROY_WS_CH3 :
#endif
#if defined LECROY_WS_CH4
        case LECROY_WS_CH4 :
#endif
            if ( FSC2_MODE == PREPARATION &&
                 ! lecroy_ws.is_sens[ channel ] )
            {
                print( FATAL, "Can't set trigger level in PREPARATION section "
                       "while sensitivity for the chaannel hasn't been "
                       "set.\n" );
                THROW( EXCEPTION );
            }

            if ( lrnd( fabs( 1.0e6 * level ) ) >
                      lrnd( 1.0e6 * LECROY_WS_TRG_MAX_LEVEL_CH_FAC *
                            lecroy_ws_get_sens( channel ) ) )
            {
                print( FATAL, "Trigger level is too large, maximum is %f "
                       "times the current channel sensitivity.\n",
                       LECROY_WS_TRG_MAX_LEVEL_CH_FAC );
                THROW( EXCEPTION );
            }
            break;

        case LECROY_WS_EXT :
            if ( lrnd( fabs( 1.0e6 * level ) ) >
                 lrnd( 1.0e6 * LECROY_WS_TRG_MAX_LEVEL_EXT ) )
            {
                print( FATAL, "Trigger level too large, maximum is %f V.\n",
                       LECROY_WS_TRG_MAX_LEVEL_EXT );
                THROW( EXCEPTION );
            }
            break;

        case LECROY_WS_EXT10 :
            if ( lrnd( fabs( 1.0e6 * level ) ) >
                 lrnd( 1.0e6 * LECROY_WS_TRG_MAX_LEVEL_EXT10 ) )
            {
                print( FATAL, "Trigger level too large, maximum is %f V.\n",
                       LECROY_WS_TRG_MAX_LEVEL_EXT10 );
                THROW( EXCEPTION );
            }
            break;
    }

    lecroy_ws.is_trigger_level[ channel ] = SET;
    lecroy_ws.trigger_level[ channel ] = level;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_trigger_level( channel, level );

    return vars_push( FLOAT_VAR, level );
}


/*---------------------------------------------------*
 * Function sets or determines the trigger slope for
 * one of the trigger channels
 *---------------------------------------------------*/

Var_T *digitizer_trigger_slope( Var_T * v )
{
    int channel;
    int slope;


    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );
    v = vars_pop( v );

    if ( channel == LECROY_WS_LIN ) {
        print( SEVERE, "Trigger slope for %s can't be determined or "
               "changed.\n", LECROY_WS_Channel_Names[ LECROY_WS_LIN ] );
        THROW( EXCEPTION );
    }

    if ( ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX ) &&
         channel != LECROY_WS_EXT && channel != LECROY_WS_EXT10 )
    {
        print( FATAL, "Invalid trigger channel.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
    {
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_slope[ channel ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR,
                                ( long ) lecroy_ws.trigger_slope[ channel ] );

            case EXPERIMENT :
                lecroy_ws.trigger_slope[ channel ] =
                                       lecroy_ws_get_trigger_slope( channel );
                return vars_push( INT_VAR,
                                ( long ) lecroy_ws.trigger_slope[ channel ] );
        }
    }

    if ( v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "POSITIVE" ) ||
             ! strcasecmp( v->val.sptr, "POS" ) )
            slope = 1;
        else if ( ! strcasecmp( v->val.sptr, "NEGATIVE" ) ||
                  ! strcasecmp( v->val.sptr, "NEG" ) ) 
            slope = 0;
        else
        {
            print( FATAL, "Invalid slope: \"%s\".\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }
    else
        slope = get_long( v, "trigger slope" );

    too_many_arguments( v );

    lecroy_ws.is_trigger_slope[ channel ] = SET;
    lecroy_ws.trigger_slope[ channel ] = slope ? POSITIVE : NEGATIVE;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_trigger_slope( channel, slope ? POSITIVE : NEGATIVE );

    return vars_push( INT_VAR, lecroy_ws.trigger_slope[ channel ] );
}


/*--------------------------------------------------*
 * Function to query of change the trigger coupling
 * of one of the trigger channels
 *--------------------------------------------------*/

Var_T *digitizer_trigger_coupling( Var_T * v )
{
    long channel;
    long cpl = -1;
    const char *cpl_str[ ] = { "AC", "DC", "LF REJ", "HF REJ", "HF" };
    size_t i;


    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    v = vars_pop( v );

    if ( channel == LECROY_WS_LIN ) {
        print( SEVERE, "Trigger coupling for LINE can't be determined or "
               "changed.\n" );
        return vars_push( FLOAT_VAR, 0.0 );
    }

    if ( ( channel < LECROY_WS_CH1 || channel > LECROY_WS_CH_MAX ) &&
         channel != LECROY_WS_EXT && channel != LECROY_WS_EXT10 )
    {
        print( FATAL, "Invalid trigger channel.\n" );
        THROW( EXCEPTION );
    }

    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_coupling[ channel ] )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR,
                             ( long ) lecroy_ws.trigger_coupling[ channel ] );

            case EXPERIMENT :
                lecroy_ws.trigger_coupling[ channel ] =
                                    lecroy_ws_get_trigger_coupling( channel );
                return vars_push( INT_VAR,
                             ( long )lecroy_ws.trigger_coupling[ channel ] );
        }

    if ( v->type == STR_VAR )
    {
        for ( i = 0; i < NUM_ELEMS( cpl_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, cpl_str[ i ] ) )
            {
                cpl = i;
                break;
            }
    }
    else
        cpl = get_long( v, "trigger coupling type" );

    if ( cpl < LECROY_WS_TRG_CPL_AC || cpl > LECROY_WS_TRG_CPL_HF )
    {
        print( FATAL, "Invalid trigger coupling type.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_trigger_coupling( channel, cpl );

    lecroy_ws.is_trigger_coupling[ channel ] = SET;
    lecroy_ws.trigger_coupling[ channel ] = cpl;

    /* Settting the HF trigger coupling automatically switches to positive
       trigger slope */

    if ( cpl == LECROY_WS_TRG_CPL_HF )
    {
        lecroy_ws.is_trigger_slope[ channel ] = SET;
        lecroy_ws.trigger_slope[ channel ] = POSITIVE;
    }

    return vars_push( INT_VAR, cpl );
}


/*-------------------------------------------------------*
 * Function to set or determine the current trigger mode
 *-------------------------------------------------------*/

Var_T *digitizer_trigger_mode( Var_T * v )
{
    long mode = -1;
    const char *mode_str[ ] = { "AUTO", "NORMAL", "SINGLE", "STOP" };
    size_t i;

    if ( v == 0 )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_mode )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) lecroy_ws.trigger_mode );

            case EXPERIMENT :
                lecroy_ws.trigger_mode = lecroy_ws_get_trigger_mode( );
                return vars_push( INT_VAR, ( long ) lecroy_ws.trigger_mode );
        }

    if ( v->type == STR_VAR )
    {
        for ( i = 0; i < NUM_ELEMS( mode_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, mode_str[ i ] ) )
            {
                mode = i;
                break;
            }
    }
    else
        mode = get_long( v, "trigger mode" );

    if ( mode < 0 || mode > ( long ) NUM_ELEMS( mode_str ) )
    {
        print( FATAL, "Invalid trigger mode.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_trigger_mode( mode );

    lecroy_ws.trigger_mode = ( int ) mode;
    lecroy_ws.is_trigger_mode = SET;

    return vars_push( INT_VAR, mode );
}


/*-----------------------------------------------------------------*
 * Function to set or determine the trigger delay, positive values
 * (up to the full horizontal width of the screen, i.e. 10 times
 * the time base) are for pretrigger while negative values (up to
 * 10,000 times the time base) are for starting the acquisition
 * after the trigger. Time resolution of the trigger delay is 1/10
 * of the timebase.
 *-----------------------------------------------------------------*/

Var_T *digitizer_trigger_delay( Var_T * v )
{
    double delay;


    if ( v == 0 )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! lecroy_ws.is_trigger_delay )
                    no_query_possible( );
                /* Fall through */

            case TEST :
                return vars_push( FLOAT_VAR, lecroy_ws.trigger_delay );

            case EXPERIMENT :
                lecroy_ws.trigger_delay = lecroy_ws_get_trigger_delay( );
                return vars_push( FLOAT_VAR, lecroy_ws.trigger_delay );
        }

    delay = get_double( v, "trigger delay" );

    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION && ! lecroy_ws.is_timebase )
    {
        print( FATAL, "Can't set trigger delay in PREPARATION "
               "section while tim base hasn't been set.\n" );
        THROW( EXCEPTION );
    }

    /* Check that the trigger delay is within the limits (taking rounding
       errors of the order of the current time resolution into account) */

    if ( delay < 0.0 && delay <   -10.0 * lecroy_ws.timebase
                                -   0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Pre-trigger delay is too long, can't be longer than "
               "10 times the time base.\n" );
        THROW( EXCEPTION );
    }

    if ( delay > 0.0 && delay >   1.0e4 * lecroy_ws.timebase
                                + 0.5 * lecroy_ws_time_per_point( ) )
    {
        print( FATAL, "Post-triger delay is too long, can't be longer that "
               "10,000 times the time base.\n" );
        THROW( EXCEPTION );
    }

    /* The delay can only be set in units of 1/50 of the timebase */

    delay = 0.02 * lrnd( 50.0 * delay / lecroy_ws.timebase )
            * lecroy_ws.timebase;

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_set_trigger_delay( delay );

    lecroy_ws.trigger_delay = delay;
    lecroy_ws.is_trigger_delay = SET;

    return vars_push( FLOAT_VAR, delay );
}


/*-----------------------------------------------------------------*
 * Function for setting up averaging:
 *  1st argument is the trace channels the average is displayed on
 *  2nd argument is the source channel (or "OFF")
 *  3rd argument is the number of averages to be done
 *-----------------------------------------------------------------*/

Var_T *digitizer_averaging( Var_T * v )
{
    long channel;
    long source_ch;
    long num_avg;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Get the channel to use for averaging */

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel != LECROY_WS_MATH )
    {
        print( FATAL, "Averaging can only be done with channel '%s'.\n",
               LECROY_WS_Channel_Names[ LECROY_WS_MATH ] );
        THROW( EXCEPTION );
    }

    /* Get the source channel */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing source channel argument.\n" );
        THROW( EXCEPTION );
    }

    /* If we get the string "OFF" it means we're supposed to switch off
       averaging (at least for this channel) */

    if ( v->type == STR_VAR && ! strcasecmp( v->val.sptr, "OFF" ) )
    {
        too_many_arguments( v );
        lecroy_ws.is_avg_setup[ channel ] = UNSET;
        return vars_push( INT_VAR, 0L );
    }

    source_ch = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( source_ch < LECROY_WS_CH1 || source_ch > LECROY_WS_CH_MAX )
    {
        print( FATAL, "Averaging can only be done on channels %s to %s as "
               "source channels.\n",
               LECROY_WS_Channel_Names[ LECROY_WS_CH1 ],
               LECROY_WS_Channel_Names[ LECROY_WS_CH_MAX ] );
        THROW( EXCEPTION );
    }

    /* Get the number of averages to use - adjust value if necessary to one
       of the possible numbers of averages as given by the array 'na' */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing number of averages.\n" );
        THROW( EXCEPTION );
    }

    num_avg = get_long( v, "number of averages" );

    if ( num_avg <= 0 )
    {
        print( FATAL, "Zero or negative number of averages (%ld).\n",
               num_avg );
        THROW( EXCEPTION );
    }

    if ( num_avg > LECROY_WS_MAX_AVERAGES )
    {
        print( FATAL, "Requested number of averages too large, maximum is "
               "%ld.\n", LECROY_WS_MAX_AVERAGES );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    lecroy_ws.is_avg_setup[ channel ] = SET;
    lecroy_ws.source_ch[ channel ]    = source_ch;
    lecroy_ws.num_avg[ channel ]      = num_avg;

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------------------*
 * Function for quering the number of averages for the math channel
 *------------------------------------------------------------------*/

Var_T *digitizer_num_averages( Var_T * v )
{
    long channel;
    long num_avg = 0;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if ( v->next != NULL )
    {
        print( FATAL, "Function can only be used for queries.\n" );
        THROW( EXCEPTION );
    }

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( channel != LECROY_WS_MATH )
    {
        print( FATAL, "Averaging can only be done using channel '%s'.\n",
               LECROY_WS_Channel_Names[ LECROY_WS_MATH ] );
        THROW( EXCEPTION );
    }

    if ( lecroy_ws.is_avg_setup[ channel ] )
        num_avg = lecroy_ws.num_avg[ channel ];
    else
    {
        print( WARN, "No averaging has been setup for channel %s.\n",
               LECROY_WS_Channel_Names[ channel ] );
        num_avg = 0;
    }

    return vars_push( INT_VAR, num_avg );
}


/*----------------------------------------------------------------------*
 * This is not a function that users should usually call but a function
 * that allows other functions to check if a certain number stands for
 * channel that can be used in measurements. Normally, an exception
 * gets thrown (and an error message gets printed) when the channel
 * number isn't ok. Only when the function gets called with a second
 * argument it returns with either 0 or 1, indicating false or true.
 *----------------------------------------------------------------------*/

Var_T *digitizer_meas_channel_ok( Var_T * v )
{
    long channel;
    bool flag;


    flag = v->next != NULL;

    channel = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                                get_strict_long( v, "channel number" ), flag );

    if ( channel >= LECROY_WS_CH1 && channel <= LECROY_WS_CH_MAX )
        return vars_push( INT_VAR, 1L );
    else
        return vars_push( INT_VAR, 0L );
}


/*-------------------------------------------------------------------*
 * Function for copying a curve from a normal or trae channel to one
 * of the memory channels
 *-------------------------------------------------------------------*/

Var_T *digitizer_copy_curve( Var_T * v )
{
    long src, dest;


    src = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                        get_strict_long( v, "source channel number" ), UNSET );

    if ( ! ( src >= LECROY_WS_CH1 && src <= LECROY_WS_CH_MAX ) )
    {
        print( FATAL, "Invalid source channel %s, must be one of the "
               "measurement channels.\n",
               LECROY_WS_Channel_Names[ src ] );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    dest = lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                   get_strict_long( v, "destination channel number" ), UNSET );

    if ( dest != LECROY_WS_M1 && dest != LECROY_WS_M2 &&
         dest != LECROY_WS_M3 && dest != LECROY_WS_M4 )
    {
        print( FATAL, "Invalid destination channel %s, must be one of the "
               "MEM channels\n", LECROY_WS_Channel_Names[ dest ] );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_copy_curve( src, dest );

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------*
 * Function for starting an acquisition
 *--------------------------------------*/

Var_T *digitizer_start_acquisition( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == EXPERIMENT )
        lecroy_ws_start_acquisition( );

    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*
 * Function for fetching a curve from one of the channels,
 * possibly using a window
 *---------------------------------------------------------*/

Var_T *digitizer_get_curve( Var_T * v )
{
    Window_T *w;
    int ch, i;
    double *array = NULL;
    long length;
    Var_T *nv;


    /* The first variable got to be a channel number */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    ch = ( int ) lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( ( ch < LECROY_WS_CH1 && ch > LECROY_WS_CH_MAX ) &&
         ( ch < LECROY_WS_M1  && ch > LECROY_WS_M4     ) &&
         ch != LECROY_WS_MATH )
    {
        print( FATAL, "Invalid channel specification.\n" );
        THROW( EXCEPTION );
    }

    /* Now check if there's a variable with a window number and check it */

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        long win_num;

        if ( lecroy_ws.w == NULL )
        {
            print( FATAL, "No measurement windows have been defined.\n" );
            THROW( EXCEPTION );
        }

        win_num = get_strict_long( v, "window number" );

        for ( w = lecroy_ws.w; w != NULL && w->num != win_num;
              w = w->next )
            /* empty */ ;

        if ( w == NULL )
        {
            print( FATAL, "Invalid measurement window number.\n" );
            THROW( EXCEPTION );
        }
    }
    else
        w = NULL;

    too_many_arguments( v );

    /* Talk to digitizer only in the real experiment, otherwise return a dummy
       array */

    if ( FSC2_MODE == EXPERIMENT )
    {
        lecroy_ws_get_curve( ch, w, &array, &length );
        nv = vars_push( FLOAT_ARR, array, length );
    }
    else
    {
        length = lecroy_ws_curve_length( );
        array = DOUBLE_P T_malloc( length * sizeof *array );
        for ( i = 0; i < length; i++ )
            array[ i ] = 1.0e-7 * sin( M_PI * i / 122.0 );
        nv = vars_push( FLOAT_ARR, array, length );
        nv->flags |= IS_DYNAMIC;
    }

    T_free( array );
    return nv;
}


/*------------------------------------------------------------------------*
 * Function for fetching the area under the curve of one of the channels,
 * possibly using one or more windows
 *------------------------------------------------------------------------*/

Var_T *digitizer_get_area( Var_T * v )
{
    Window_T *w;
    int ch, i, j;
    Var_T *cv;
    Var_T *ret = NULL;
    int win_count = 0;


    /* The first variable got to be a channel number */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    ch = ( int ) lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( ( ch < LECROY_WS_CH1 && ch > LECROY_WS_CH_MAX ) &&
         ( ch < LECROY_WS_M1  && ch > LECROY_WS_M4     ) &&
         ch != LECROY_WS_MATH )
    {
        print( FATAL, "Invalid channel specification.\n" );
        THROW( EXCEPTION );
    }

    /* Now check the variables and count how many window handles we got */

    for ( cv = v = vars_pop( v ); cv != NULL; cv = cv->next )
    {
        vars_check( cv, INT_VAR | INT_ARR );

        if ( cv->type == INT_VAR )
            win_count++;
        else
        {
            if ( cv->len == 0 )
            {
                print( FATAL, "Length of array passed as argument #%d is "
                       "not known.\n", win_count + 1 );
                THROW( EXCEPTION );
            }
            win_count += cv->len;
        }
    }

    /* Complain when we got something that could be a window handle but no
       windows have been defined */

    if ( win_count > 0 && lecroy_ws.w == NULL )
    {
        print( FATAL, "No measurement windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    /* If there's more than one window handle we must return an array */

    if ( win_count > 1 )
        ret = vars_push( FLOAT_ARR, NULL, ( long ) win_count );


    /* When we're still in the test phase we got to return a dummy value */

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( win_count == 0 )
            return vars_push( FLOAT_VAR, 1.234e-8 );

        for ( i = 0; i < win_count; v = vars_pop( v ) )
        {
            if ( v->type == INT_VAR )
            {
                w = lecroy_ws_get_window_by_number(
                                       get_strict_long( v, "window number" ) );

                if ( win_count == 1 )
                    return vars_push( FLOAT_VAR, 1.234e-8 );

                ret->val.dpnt[ i++ ] = 1.234e-8;
                continue;
            }

            for ( j = 0; j < v->len; j++ )
            {
                w = lecroy_ws_get_window_by_number( v->val.lpnt[ j ] );

                /* Take care of the hopefully rather unlikely situation that
                   we've got an array of length 1 */

                if ( win_count == 1 )
                    return vars_push( FLOAT_VAR, 1.234e-8 );

                ret->val.dpnt[ i++ ] = 1.234e-8;
            }
        }

        return ret;
    }

    /* Now comes the part that gets run in a real experiment. */

    if ( win_count == 0 )
        return vars_push( FLOAT_VAR, lecroy_ws_get_area( ch, NULL ) );

    /* Otherwise loop over the window numbers and fill the array with areas
       for the different windows. */

    for ( i = 0; i < win_count; v = vars_pop( v ) )
    {
        if ( v->type == INT_VAR )
        {
            w = lecroy_ws_get_window_by_number(
                                       get_strict_long( v, "window number" ) );

            if ( win_count == 1 )
                return vars_push( FLOAT_VAR, lecroy_ws_get_area( ch, w ) );

            ret->val.dpnt[ i++ ] = lecroy_ws_get_area( ch, w );

            continue;
        }

        for ( j = 0; j < v->len; j++ )
        {
            w = lecroy_ws_get_window_by_number( v->val.lpnt[ j ] );

            if ( win_count == 1 )
                return vars_push( FLOAT_VAR, lecroy_ws_get_area( ch, w ) );

            ret->val.dpnt[ i++ ] = lecroy_ws_get_area( ch, w );

        }
    }

    return ret;
}


/*-----------------------------------------------------------*
 * Funcction for getting the maximum amplitude of a curve of
 * one of the channels, possibly using one or more windows
 *-----------------------------------------------------------*/

Var_T *digitizer_get_amplitude( Var_T * v )
{
    Window_T *w;
    int ch, i, j;
    Var_T *cv;
    Var_T *ret = NULL;
    int win_count = 0;


    /* The first variable got to be a channel number */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    ch = ( int ) lecroy_ws_translate_channel( GENERAL_TO_LECROY_WS,
                               get_strict_long( v, "channel number" ), UNSET );

    if ( ( ch < LECROY_WS_CH1 && ch > LECROY_WS_CH_MAX ) &&
         ( ch < LECROY_WS_M1  && ch > LECROY_WS_M4     ) &&
         ch != LECROY_WS_MATH )
    {
        print( FATAL, "Invalid channel specification.\n" );
        THROW( EXCEPTION );
    }

    /* Now check the variables and count how many window handles we got */

    for ( cv = v = vars_pop( v ); cv != NULL; cv = cv->next )
    {
        vars_check( cv, INT_VAR | INT_ARR );

        if ( cv->type == INT_VAR )
            win_count++;
        else
        {
            if ( cv->len == 0 )
            {
                print( FATAL, "Length of array passed as argument #%d is "
                       "not known.\n", win_count + 1 );
                THROW( EXCEPTION );
            }
            win_count += cv->len;
        }
    }

    /* Complain when we got something that could be a window handle but no
       windows have been defined */

    if ( win_count > 0 && lecroy_ws.w == NULL )
    {
        print( FATAL, "No measurement windows have been defined.\n" );
        THROW( EXCEPTION );
    }

    /* If there's more than one window handle we must return an array */

    if ( win_count > 1 )
        ret = vars_push( FLOAT_ARR, NULL, ( long ) win_count );


    /* When we're still in the test phase we got to return a dummy value */

    if ( FSC2_MODE != EXPERIMENT )
    {
        if ( win_count == 0 )
            return vars_push( FLOAT_VAR, 1.234e-7 );

        for ( i = 0; i < win_count; v = vars_pop( v ) )
        {
            if ( v->type == INT_VAR )
            {
                w = lecroy_ws_get_window_by_number(
                                       get_strict_long( v, "window number" ) );

                if ( win_count == 1 )
                    return vars_push( FLOAT_VAR, 1.234e-7 );

                ret->val.dpnt[ i++ ] = 1.234e-7;
                continue;
            }

            for ( j = 0; j < v->len; j++ )
            {
                w = lecroy_ws_get_window_by_number( v->val.lpnt[ j ] );

                /* Take care of the hopefully rather unlikely situation that
                   we've got an array of length 1 */

                if ( win_count == 1 )
                    return vars_push( FLOAT_VAR, 1.234e-7 );

                ret->val.dpnt[ i++ ] = 1.234e-7;
            }
        }

        return ret;
    }

    /* Now comes the part that gets run in a real experiment. */

    if ( win_count == 0 )
        return vars_push( FLOAT_VAR, lecroy_ws_get_amplitude( ch, NULL ) );

    /* Otherwise loop over the window numbers and fill the array with areas
       for the different windows. */

    for ( i = 0; i < win_count; v = vars_pop( v ) )
    {
        if ( v->type == INT_VAR )
        {
            w = lecroy_ws_get_window_by_number(
                                       get_strict_long( v, "window number" ) );

            if ( win_count == 1 )
                return vars_push( FLOAT_VAR,
                                  lecroy_ws_get_amplitude( ch, w ) );

            ret->val.dpnt[ i++ ] = lecroy_ws_get_amplitude( ch, w );

            continue;
        }

        for ( j = 0; j < v->len; j++ )
        {
            w = lecroy_ws_get_window_by_number( v->val.lpnt[ j ] );

            if ( win_count == 1 )
                return vars_push( FLOAT_VAR,
                                  lecroy_ws_get_amplitude( ch, w ) );

            ret->val.dpnt[ i++ ] = lecroy_ws_get_amplitude( ch, w );

        }
    }

    return ret;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *digitizer_lock_keyboard( Var_T * v )
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
        lecroy_ws_lock_state( lock );

    lecroy_ws.lock_state = lock;
    return vars_push( INT_VAR, lock ? 1L : 0L );
}


/*-----------------------------------------------------*
 * Function for sending a command string to the device
 *-----------------------------------------------------*/

Var_T *digitizer_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );
    
    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            lecroy_ws_command( cmd );
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
