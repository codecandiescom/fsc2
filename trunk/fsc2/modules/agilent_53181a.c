/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#include "fsc2_module.h"

#include "agilent_53181a.conf"
#include "gpib_if.h"

/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


int agilent_53181a_init_hook(       void );
int agilent_53181a_test_hook(       void );
int agilent_53181a_exp_hook(        void );
int agilent_53181a_end_of_exp_hook( void );

Var_T * freq_counter_name(       Var_T * v );
Var_T * freq_counter_coupling(   Var_T * v );
Var_T * freq_counter_timebase(   Var_T * v );
Var_T * freq_counter_mode(       Var_T * v );
Var_T * freq_counter_digits(     Var_T * v );
Var_T * freq_counter_gate_time(  Var_T * v );
Var_T * freq_counter_measure(    Var_T * v );


static int agilent_53181a_init( void );
static void agilent_53181a_set_timebase( void );
static int agilent_53181a_get_timebase( void );
static void agilent_53181a_set_coupling( void );
static int agilent_53181a_get_coupling( void );
static void agilent_53181a_set_mode( void );
static int agilent_53181a_get_mode( void );
static void agilent_53181a_set_digits( void );
static int agilent_53181a_get_digits( void );
static void agilent_53181a_set_gate_time( void );
static double agilent_53181a_get_gate_time( void );
static double agilent_53181a_get_freq( int channel );
static bool agilent_53181a_talk( const char * cmd,
                                 char *       reply,
                                 long *       length );
static void agilent_53181a_comm_fail( void );


typedef struct {
    int device;
    int mode;
    bool is_mode;
    int digits;
    bool is_digits;
	double gate_time;
	bool is_gate_time;
	int coupling;
	bool is_coupling;
	int timebase;
	bool is_timebase;
} AGILENT_53181A;


static AGILENT_53181A agilent_53181a, agilent_53181a_stored;


#define TIMEBASE_INT  0
#define TIMEBASE_EXT  1
#define TIMEBASE_AUTO 2


#define COUPLING_A50  0
#define COUPLING_A1M  2
#define COUPLING_D50  1
#define COUPLING_D1M  3

#define MODE_AUTO     1
#define MODE_GATE     2
#define MODE_DIGITS   3
#define MODE_EXT      3

#define MIN_DIGITS    3
#define MAX_DIGITS   15

#define MIN_GATE_TIME  1.0e-3     /* 1 ms */
#define MAX_GATE_TIME  1000.0     /* 1 ks */

#define TEST_CH1_FREQUENCY   1.0e8    /* 100 MHz */
#define TEST_CH2_FREQUENCY   9.2e9    /* 9.2 GHz */

#define TEST_TIMEASE    TIMEBASE_INT
#define TEST_MODE       MODE_AUTO
#define TEST_COUPLING   COUPLING_A50

#define TEST_GATE_TIME  1         /* 1 s */
#define TEST_DIGITS     10


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
agilent_53181a_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

	agilent_53181a.device = -1;
    agilent_53181a.is_mode = UNSET;
	agilent_53181a.is_gate_time = UNSET;
	agilent_53181a.is_digits = UNSET;
	agilent_53181a.is_coupling = UNSET;
	agilent_53181a.is_timebase = UNSET;

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
agilent_53181a_test_hook( void )
{
	agilent_53181a_stored = agilent_53181a;
	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
agilent_53181a_exp_hook( void )
{
	agilent_53181a = agilent_53181a_stored;

	if ( ! agilent_53181a_init( ) )
    {
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_error_msg );
        THROW( EXCEPTION );
    }

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
agilent_53181a_end_of_exp_hook( void )
{
    /* Switch device to local mode */

    if ( agilent_53181a.device != -1 )
        gpib_local( agilent_53181a.device );

    agilent_53181a.device = -1;
    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_coupling( Var_T * v )
{
    long cpl;
    const char *cpl_str[ ] = { "A50", "D50", "A1M", "D1M" };


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( agilent_53181a.is_coupling )
                    no_query_possible( );
                /* fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) agilent_53181a.coupling );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) agilent_53181a_get_coupling( ) );
        }

    if ( v->type == STR_VAR )
    {
        size_t i;

        for ( i = 0; i < NUM_ELEMS( cpl_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, cpl_str[ i ] ) )
            {
                cpl = i;
                break;
            }
    }
    else
        cpl = get_long( v, "coupling type" );

    if ( cpl < COUPLING_A50 || cpl > COUPLING_D1M )
    {
        print( FATAL, "Invalid coupling type.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    agilent_53181a.coupling = cpl;
    agilent_53181a.is_coupling = SET;

    if ( FSC2_MODE == EXPERIMENT )
        agilent_53181a_set_coupling( );

    return vars_push( INT_VAR, cpl );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_timebase( Var_T * v )
{
    long tb = -1;
    const char *tb_str[ ] = { "INT", "EXT", "AUTO" };


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( agilent_53181a.is_timebase )
                    no_query_possible( );
                /* fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) agilent_53181a.timebase );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) agilent_53181a_get_timebase( ) );
        }

    if ( v->type == STR_VAR )
    {
        size_t i;

        for ( i = 0; i < NUM_ELEMS( tb_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, tb_str[ i ] ) )
            {
                tb = i;
                break;
            }
    }
    else
        tb = get_long( v, "timebase type" );

    if ( tb < TIMEBASE_INT || tb > TIMEBASE_AUTO )
    {
        print( FATAL, "Invalid timebase type.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    agilent_53181a.timebase = tb;
    agilent_53181a.is_timebase = SET;

    if ( FSC2_MODE == EXPERIMENT )
        agilent_53181a_set_timebase( );

    return vars_push( INT_VAR, tb );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_mode( Var_T * v )
{
    long mode = -1;
    const char *mode_str[ ] = { "AUTO", "GATE", "DIGITS" };


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( agilent_53181a.is_mode )
                    no_query_possible( );
                /* fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) agilent_53181a.mode );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) agilent_53181a_get_mode( ) );
        }

    if ( v->type == STR_VAR )
    {
        size_t i;

        for ( i = 0; i < NUM_ELEMS( mode_str ); i++ )
            if ( ! strcasecmp( v->val.sptr, mode_str[ i ] ) )
            {
                mode = i;
                break;
            }
    }
    else
        mode = get_long( v, "mode type" );

    if ( mode < MODE_AUTO || mode > MODE_DIGITS )
    {
        print( FATAL, "Invalid mode type.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    agilent_53181a.mode = mode;
    agilent_53181a.is_mode = SET;

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( mode == MODE_GATE && agilent_53181a.is_gate_time )
            agilent_53181a_set_gate_time( );
        else if ( mode == MODE_DIGITS && agilent_53181a.is_digits )
            agilent_53181a_set_digits( );
        agilent_53181a_set_mode( );
    }

    return vars_push( INT_VAR, mode );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_digits( Var_T * v )
{
    long digits;

    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( agilent_53181a.is_digits )
                    no_query_possible( );
                /* fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) agilent_53181a.digits );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) agilent_53181a_get_digits( ) );
        }

    digits = get_long( v, "number of digits" );
    too_many_arguments( v );

    if ( digits < MIN_DIGITS || digits > MAX_DIGITS )
    {
        print( FATAL, "Invalid number of digits, must be between %d and %d.\n",
               MIN_DIGITS, MAX_DIGITS );
        THROW( EXCEPTION );
    }

    agilent_53181a.digits = digits;
    agilent_53181a.is_digits = SET;

    if (    FSC2_MODE == EXPERIMENT
         && agilent_53181a.is_mode
         && agilent_53181a.mode == MODE_DIGITS )
        agilent_53181a_set_digits( );

    return vars_push( INT_VAR, digits );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_gate_time( Var_T * v )
{
    long gate_time;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( agilent_53181a.is_gate_time )
                    no_query_possible( );
                /* fall through */

            case TEST :
                return vars_push( INT_VAR, ( long ) agilent_53181a.gate_time );

            case EXPERIMENT :
                return vars_push( INT_VAR,
                                  ( long ) agilent_53181a_get_gate_time( ) );
        }

    gate_time = get_long( v, "gate_time" );
    too_many_arguments( v );

    if (    gate_time < 0.999 * MIN_GATE_TIME
         || gate_time > 1.001 * MAX_GATE_TIME )
    {
        print( FATAL, "Invalid gate_time, must be between %d and %d.\n",
               MIN_GATE_TIME, MAX_GATE_TIME );
        THROW( EXCEPTION );
    }

    if ( gate_time < MIN_GATE_TIME )
        gate_time = MIN_GATE_TIME;
    if ( gate_time > MAX_GATE_TIME )
        gate_time = MAX_GATE_TIME;

    agilent_53181a.gate_time = gate_time;
    agilent_53181a.is_gate_time = SET;

    if (    FSC2_MODE == EXPERIMENT
         && agilent_53181a.is_mode
         && agilent_53181a.mode == MODE_GATE )
        agilent_53181a_set_gate_time( );

    return vars_push( FLOAT_VAR, gate_time );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
freq_counter_measure( Var_T * v )
{
	long channel;


	if ( v == NULL )
	{
		print( FATAL, "Missing channel argument.\n" );
		THROW( EXCEPTION );
	}

	channel = get_long( v, "channel" );

	too_many_arguments( v );

	if (    channel != CHANNEL_CH1
#if defined HAS_SECOND_CHANNEL
		 && channel != CHANNEL_CH2
#endif
                                   )
	{
		print( FATAL, "Invalid channel specified.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR,
                          channel == CHANNEL_CH1 ?
                          TEST_CH1_FREQUENCY : TEST_CH2_FREQUENCY );

	retur vars_push( FLOAT_VAR,
                     agilent_53181a_get_freq( channel == CHANNEL_CH1 ?
                                              1 : 2 ) );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
agilent_53181a_init( void )
{
    if ( gpib_init_device( DEVICE_NAME, &agilent_53181a.device ) == FAILURE )
        return FAIL;

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_set_coupling( void )
{
    char cmd[ 100 ];


    switch ( agilent_53181a.coupling )
    {
        case COUPLING_A50 :
            strcpy( cmd, "INP1:COUP AC;IMP 50" );
            break;

        case COUPLING_D50 :
            strcpy( cmd, "INP1:COUP DC;IMP 50" );
            break;

        case COUPLING_A1M :
            strcpy( cmd, "INP1:COUP AC;IMP 1E6" );
            break;

        case COUPLING_D1M :
            strcpy( cmd, "INP1:COUP DC;IMP 1E6" );
            break;
    }

    if ( gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE )
        agilent_53181a_comm_fail( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
agilent_53181a_get_coupling( void )
{
    char reply[ 50 ];
    long length = sizeof reply;
    int coup = 0;
    double val = 0.0;


    agilent_53181a_talk( "INP1:COUP?", reply, &length );

    if ( ! strcmp( reply, "AC" ) )
        coup = 0;
    else if ( ! strcmp( reply, "DC" ) )
        coup = 2;
    else
        agilent_53181a_comm_fail( );
    
    length = sizeof reply;
    agilent_53181a_talk( "INP1:COUP?", reply, &length );

    TRY
    {
        val = T_atod( reply );
        TRY_SUCCESS;
    }
    OTHERWISE
        agilent_53181a_comm_fail( );

    if ( lrnd( val ) == 50 )
        coup += 0;
    else if ( lrnd( val ) == 1000000 )
        coup += 1;
    else
        agilent_53181a_comm_fail( );


    agilent_53181a.coupling = coup;
    agilent_53181a.is_coupling = SET;

    return coup;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_set_timebase( void )
{
    char cmd[ 100 ];

    if ( agilent_53181a.timebase == TIMEBASE_AUTO )
        strcpy( cmd, ":ROSC:SOUR:AUTO 1" );
    else if ( agilent_53181a.timebase == TIMEBASE_INT )
        strcpy( cmd, ":ROSC:SOUR:AUTO 0;SOUR INT" );
    else
        strcpy( cmd, ":ROSC:SOUR:AUTO 0;SOUR EXT" );

    if ( gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE )
        agilent_53181a_comm_fail( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
agilent_53181a_get_timebase( void )
{
    char reply[ 50 ];
    long length = sizeof reply;


    agilent_53181a_talk( ":ROSC:SOUR?", reply, &length );

    if ( ! strcmp( reply, "INT" ) )
        return TIMEBASE_INT;
    else if ( ! strcmp( reply, "EXT" ) )
        return TIMEBASE_EXT;

    agilent_53181a_comm_fail( );

    return -1;              /* we'll never end up here */
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_set_mode( void )
{
    char cmd[ 100 ] = ":FREQ:ARM:STAR:SOUR IMM;:FREQ:ARM:STOP:SOUR ";
    const char *modes[ ] = { "IMM", "TIM", "DIG" };


    strcat( cmd, modes[ agilent_53181a.mode ] );

    if ( gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE )
        agilent_53181a_comm_fail( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
agilent_53181a_get_mode( void )
{
    char reply[ 50 ];
    long length = sizeof reply;
    const char *modes[ ] = { "IMM", "TIM", "DIG" };
    size_t i;


    agilent_53181a_talk( ":FREQ:ARM:STAR?", reply, &length );

    if ( ! strcmp( reply, "EXT" ) )
        return MODE_EXT;
    else if ( strcmp( reply, "IMM" ) )
        agilent_53181a_comm_fail( );

    agilent_53181a_talk( ":FREQ:ARM:STOP?", reply, &length );

    for ( i = 0; i < NUM_ELEMS( modes ); i++ )
        if ( ! strcmp( reply, modes[ i ] ) )
        {
            agilent_53181a.mode = i;
            agilent_53181a.is_mode = SET;
            return i;
        }

    agilent_53181a_comm_fail( );

    return -1;    /* we'll never get here */
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_set_digits( void )
{
    char cmd[ 100 ];


    sprintf( cmd, ":FREQ:ARM:STOP:DIG %d", agilent_53181a.digits );
    if ( gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE )
        agilent_53181a_comm_fail( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static int
agilent_53181a_get_digits( void )
{
    char reply[ 50 ];
    long length = sizeof reply;
    long digits;


    agilent_53181a_talk( ":FREQ:ARM:STOP:DIG?", reply, &length );

    TRY
    {
        digits = T_atol( reply );
        TRY_SUCCESS;
    }
    OTHERWISE
        agilent_53181a_comm_fail( );

    agilent_53181a.digits = digits;
    agilent_53181a.is_digits = SET;

    return digits;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_set_gate_time( void )
{
    char cmd[ 100 ];

    sprintf( cmd, ":FREQ:ARM:STOP:TIM %g", agilent_53181a.gate_time );
    if ( gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE )
        agilent_53181a_comm_fail( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
agilent_53181a_get_gate_time( void )
{
    char reply[ 50 ];
    long length = sizeof reply;
    double gate_time;


    agilent_53181a_talk( ":FREQ:ARM:STOP:TIM?", reply, &length );

    TRY
    {
        gate_time = T_atod( reply );
        TRY_SUCCESS;
    }
    OTHERWISE
        agilent_53181a_comm_fail( );

    agilent_53181a.gate_time = gate_time;
    agilent_53181a.is_gate_time = SET;

    return gate_time;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static double
agilent_53181a_get_freq( int channel )
{
    char str[ 50 ];
    long length = sizeof reply;
    double val = 0.0;


    sprintf( str, ":CONF:FREQ (@%d);READ?", channel );
    agilent_53181a_talk( str, str, &length );

    TRY
    {
        val = T_atod( str );
        TRY_SUCCESS;
    }
    OTHERWISE 
        agilent_53181a_comm_fail( );

    return val;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
agilent_53181a_talk( const char * cmd,
                     char *       reply,
                     long *       length )
{
    if (    gpib_write( agilent_53181a.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( agilent_53181a.device, reply, length ) == FAILURE )
        agilent_53181a_comm_fail( );
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
agilent_53181a_comm_fail( void )
{
	print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
