/*
 *  Copyright (C) 2018 Максим Александрович Рыбаков
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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "lakeshore340.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define SAMPLE_CHANNEL_A       0
#define SAMPLE_CHANNEL_B       1
#define SAMPLE_CHANNEL_C       2
#define SAMPLE_CHANNEL_D       3

#define KELVIN_UNITS           1
#define CELSIUS_UNITS          2

#define DEFAULT_LOOP           1

#define LIMIT_SETPOINT_K       300   // I was told to set this limit to protect
#define LIMIT_SETPOINT_C       27    // cryostat from students

#define LOCK_STATE_LOCAL       1
#define LOCK_STATE_REMOTE      2
#define LOCK_STATE_REMOTE_LLO  3

int lakeshore340_init_hook(       void );
int lakeshore340_exp_hook(        void );
int lakeshore340_end_of_exp_hook( void );


Var_T * temp_contr_name(         Var_T * v );
Var_T * temp_contr_loop_conf(    Var_T * v );
Var_T * temp_contr_setpoint(     Var_T * v );
Var_T * temp_contr_heater_power( Var_T * v );
Var_T * temp_contr_temperature(  Var_T * v );


static bool lakeshore340_init(                    const char * name );
static int lakeshore340_get_loop_control(                  int loop );
static void lakeshore340_set_heater(                      int range );
static int lakeshore340_get_heater(                            void );
static void lakeshore340_set_setpoint(                 double point );
static double lakeshore340_get_setpoint(                   int loop );
static double lakeshore340_sens_data(         char channel, int deg );
static void lakeshore340_loop_conf( int loop, char input, int unit,
		                                                  int state );
static void lakeshore340_lock(                            int state );
static bool lakeshore340_command(                  const char * cmd );
static void lakeshore340_gpib_failure(                         void );
static bool lakeshore340_talk( const char * cmd, char * reply,
		                                              long * length );


static struct {
    int device;
    int lock_state;
    int loop;
    int unit;

    struct {
        char input;
        int unit;
        int state;
    } loop_params[2];
} lakeshore340;


/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore340_init_hook( void )
{
    Need_GPIB = SET;

    lakeshore340.device     = -1;
    lakeshore340.lock_state = LOCK_STATE_REMOTE_LLO;

    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore340_exp_hook( void )
{
    if ( ! lakeshore340_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed.\n" );
        THROW( EXCEPTION );
    }
    return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore340_end_of_exp_hook( void )
{
    lakeshore340_lock( LOCK_STATE_LOCAL );
    return 1;
}


/**************************************/
/*                                    */
/*          EDL functions             */
/*                                    */
/**************************************/


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
temp_contr_name( Var_T * v UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------*
 * Configures control parameters for selected loop.
 * To change one of the parameters the function expects
 * two arguments, the name of parameter to be changed (as
 * a string) and its new value.
 *
 * The parameters are:
 * "input": "A", "B", "C" or "D" (the latter only for devices with sample
            channels C and D);
 * "units": "K" (for Kelvin), "C" (for Celsius), "S" (for Sensor units(
 * "state": "on", "off";
 * "loop" (number) = 1 or 2. If no loop number is given function defaults to 1.
 * Function can get any number of arguments and in any order.
 * For example:
 *
 * temp_contr_loop_conf("loop", 1, "input", "D", "state", "on");
 * (enables control loop 1 and sets input D as working one)
 *
 * temp_contr_loop_conf("loop", 2, "units", "K");
 * (sets Kelvin units for control loop 2)
 *---------------------------------------------------------*/

Var_T *
temp_contr_loop_conf(Var_T * v)
{
	char input = 'N';
	int unit = -1;
	int loop = -1;
	int state = -1;

	const char *state_str[] = { "off", "on" };

    if (v == NULL) {
        print(FATAL, "Missing arguments.\n");
        THROW(EXCEPTION);
    }

    do
    {
        // get name of parameter...

        if (v->type != STR_VAR) {
            print(FATAL, "Unexpected argument that's not a string.\n");
            THROW(EXCEPTION);
        }

        int par = -1;
        const char *par_str[] =	{ "loop", "input", "units", "state" };

        for (size_t i = 0; i < NUM_ELEMS(par_str); i++)
            if (! strcasecmp(v->val.sptr, par_str[i]))
            {
                par = i;
                break;
            }

        if (par == -1) {
            print(FATAL, "Invalid parameter name '%s'.\n", v->val.sptr);
            THROW(EXCEPTION);
        }

        // ...and its value

        if ((v = vars_pop(v)) == NULL)
        {
            print(FATAL, "Missing value for '%s'.\n", par_str[par]);
            THROW(EXCEPTION);
        }

        switch (par)
        {
			case 0: // loop
                loop = get_strict_long(v, "loop number");
				if (loop != 1 && loop != 2)
				{
					print(FATAL, "Invalid loop argument (\"%ld\").\n", loop);
					THROW(EXCEPTION);
				}
				break;

			case 1: // input
                if (v->type != STR_VAR) {
                    print(FATAL, "Expected string for channel.\n");
                    THROW(EXCEPTION);
                }

                if (   (   *v->val.sptr != 'A' && *v->val.sptr != 'B'
#if defined HAS_SAMPLE_CHANNEL_CHAS_SAMPLE_CHANNEL_D
                        && *v->val.sptr != 'C'
#endif
#if defined HAS_SAMPLE_CHANNEL_CHAS_SAMPLE_CHANNEL_D
                        && *v->val.sptr != 'D'
#endif
					   )
                    || strlen(v->val.sptr) != 1)
                {
                    print(FATAL, "Invalid input channel (\"%s\").\n",
                          v->val.sptr);
                    THROW(EXCEPTION);
                }

                input = *v->val.sptr;
				break;

			case 2: // units
                if (v->type != STR_VAR) {
                    print(FATAL, "Expected string for unit.\n");
                    THROW(EXCEPTION);
                }

				if (*v->val.sptr == 'K')
					unit = 1;
				else if (*v->val.sptr == 'C')
					unit = 2;
				else if (*v->val.sptr == 'S')
					unit = 3;
				else
				{
					print(FATAL, "Invalid input units (\"%s\").\n",
                          v->val.sptr);
					THROW(EXCEPTION);
				}
				break;

			case 3: // state
                if (v->type != STR_VAR) {
                    print(FATAL, "Expected string for state.\n");
                    THROW(EXCEPTION);
                }

                for (size_t i = 0; i < NUM_ELEMS(state_str); i++)
                    if (!strcasecmp(v->val.sptr, state_str[i]))
                    {
                        state = i;
                        break;
                    }

                if (state == -1)
                {
                    print(FATAL, "Invalid loop state (\"%s\").\n", v->val.sptr);
                    THROW(EXCEPTION);
                }
				break;
        }
    } while ((v = vars_pop(v)) != NULL);

    /* Use loop 1 per default if none was specified in the arguments
    * (and warn once during the test run) */

    if (loop != 1 && loop != 2) {
        loop = DEFAULT_LOOP;
        if (FSC2_MODE == TEST)
            print(WARN, "No loop argument found, using default loop 1.\n");
    }

    if (input == 'A' || input == 'B' || input == 'C' || input == 'D')
        lakeshore340.loop_params[loop - 1].input = input;
    if (unit == 'K' || unit == 'C' || unit == 'S')
        lakeshore340.loop_params[loop - 1].unit = unit;
    if (state == 0 || state == 1)
        lakeshore340.loop_params[loop - 1].state = state;

    if (FSC2_MODE == EXPERIMENT)
        lakeshore340_loop_conf(loop,
                               lakeshore340.loop_params[loop - 1].input,
                               lakeshore340.loop_params[loop - 1].unit,
                               lakeshore340.loop_params[loop - 1].state);

    return vars_push(INT_VAR, 1);
}


/*-----------------------------------------------------------*
 * Sets the temperature setpoint as first argument for
 * selected loop as second argument. When no loop argument
 * uses loop 1 as default. Temperature can be in Kelvin or
 * Celsius, it depends on what units loop has been configured
 *-----------------------------------------------------------*/

Var_T *
temp_contr_setpoint( Var_T * v )
{
	double setpoint;
    long loop = DEFAULT_LOOP;


    if (v == NULL)
        return vars_push(FLOAT_VAR,
                         FSC2_MODE == TEST ?
                         1.0 : lakeshore340_get_setpoint(DEFAULT_LOOP));

    setpoint = get_double(v, "setpoint temperature");

    if ((v = vars_pop(v)))
    {
        loop = get_strict_long(v, "loop number");
        if (loop != 1 && loop != 2)
        {
            print(FATAL, "Invalid loop argument (\"%ld\").\n", v->val.lval);
            THROW(EXCEPTION);
        }
    }

    if (   (   lakeshore340.loop_params[loop - 1].unit == 1
            && setpoint > LIMIT_SETPOINT_K)
        || (   lakeshore340.loop_params[loop - 1].unit == 2
               && setpoint > LIMIT_SETPOINT_C))
    {
        print(FATAL, "Requestted temperature too high, there would be a "
              "big risk of damage.\n");
        THROW(EXCEPTION);
    }

    lakeshore340.loop = loop;

	if (FSC2_MODE == EXPERIMENT)
		lakeshore340_set_setpoint(setpoint);

    return vars_push(FLOAT_VAR, setpoint);
}


/*------------------------------------------------*
 * Just sets heater range. 0 for turning
 * off and 1-5 for range itself. Returns heater
 * range if no argument
 *------------------------------------------------*/

Var_T *
temp_contr_heater_power( Var_T * v )
{
    if ( v == NULL )
        return vars_push( INT_VAR,
                          FSC2_MODE == TEST ? 1 : lakeshore340_get_heater( ) );

    long range = get_strict_long(v, "heater power range");
    if ( range < 0 || range > 5 )
    {
        print( FATAL, "Invalid heater power range argument %ld.\n", range);
        THROW( EXCEPTION );
    }

	if ( FSC2_MODE == EXPERIMENT )
		lakeshore340_set_heater( range );

    return vars_push( INT_VAR, range );
}

/*----------------------------------------------------*
 * Reads temperature from selected input of controller.
 * First argument is an input A, B, C or D. Second is
 * units K = Kelvin, C = Celsius, S = sensor units.
 * If no units argument uses Kelvin as default.
 *----------------------------------------------------*/

Var_T *
temp_contr_temperature( Var_T * v )
{
    long channel;
    char input;


    if ( v == NULL ) {
        print( FATAL, "Missing argument.\n");
        THROW( EXCEPTION );
    }

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
    {
        channel = get_long( v, "channel number" ) - 1;

        if (   channel != SAMPLE_CHANNEL_A
            && channel != SAMPLE_CHANNEL_B
#if defined HAS_SAMPLE_CHANNEL_C
            && channel != SAMPLE_CHANNEL_C
#endif
#if defined HAS_SAMPLE_CHANNEL_D
            && channel != SAMPLE_CHANNEL_D
#endif
            )
        {
            print( FATAL, "Invalid sample channel number (%ld).\n", channel );
            THROW( EXCEPTION );
        }

        input = channel + 'A';
    }
    else
    {
        if (   (   *v->val.sptr != 'A' && *v->val.sptr != 'B'
#if defined HAS_SAMPLE_CHANNEL_C 
                && *v->val.sptr != 'C'
#endif
#if defined HAS_SAMPLE_CHANNEL_D
                && *v->val.sptr != 'D'
#endif
               )
               || strlen( v->val.sptr ) != 1 )
        {
            print( FATAL, "Invalid sample channel (\"%s\").\n", v->val.sptr );
            THROW(EXCEPTION);
        }

        input = *v->val.sptr;
    }

    if ((v = vars_pop(v)) == NULL)
        return vars_push(FLOAT_VAR,
                         FSC2_MODE == TEST ?
                         123.0 :
                         lakeshore340_sens_data(input, lakeshore340.unit));

    if (*v->val.sptr == 'K')
        return vars_push(FLOAT_VAR, lakeshore340_sens_data(input, 1));
    else if (*v->val.sptr == 'C')
        return vars_push(FLOAT_VAR, lakeshore340_sens_data(input, 2));
    else if (*v->val.sptr == 'S')
        return vars_push(FLOAT_VAR, lakeshore340_sens_data(input, 3));

    print(FATAL, "Invalid unit argument (\"%s\").\n", v->val.sptr);
    THROW(EXCEPTION);
}




/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static bool
lakeshore340_init( const char * name )
{
    char buf[ 20 ];
    long len = sizeof buf - 1;

    /* Initialize GPIB communication with the temperature controller */

    if ( gpib_init_device( name, &lakeshore340.device ) == FAILURE )
        return FAIL;

    lakeshore340_talk("*STB?\n", buf, &len);
    buf[ len ] = '\0';

    /* Switch device to remote state with local lockout */

    lakeshore340_command("MODE 3\n");

    lakeshore340.loop = DEFAULT_LOOP;
    lakeshore340.unit = KELVIN_UNITS;

    /* get control parameters from each loop */

    lakeshore340_get_loop_control(1);
    lakeshore340_get_loop_control(2);

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static int
lakeshore340_get_loop_control( int loop )
{
	char buf[15];
	char cmd[10];
	long len = sizeof buf - 1;

	sprintf(cmd, "CSET? %d\n", loop);
	lakeshore340_talk(cmd, buf, &len);
	buf[len] = '\0';

    fsc2_assert(loop == 1 || loop == 2);

    lakeshore340.loop_params[loop - 1].input = buf[0];
    lakeshore340.loop_params[loop - 1].unit  = buf[2] - '0';
    lakeshore340.loop_params[loop - 1].state = buf[4] - '0';

	return 1;
}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore340_set_heater( int range )
{
	char cmd[ 15 ];

	sprintf( cmd, "RANGE %d\n", range );
	lakeshore340_command( cmd );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static int
lakeshore340_get_heater( void )
{
	char buf[ 50 ];
	char cmd[ 15 ];
	long len = sizeof buf - 1;
	int heater = 0;

	sprintf(cmd, "RANGE?\n");
	lakeshore340_talk(cmd, buf, &len);
	buf[len] = '\0';
	sscanf(buf, "%d", &heater);
	return heater;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore340_set_setpoint( double point)
{
	char cmd[ 35 ];

	sprintf( cmd, "SETP %d, %lf\n", lakeshore340.loop, point );
	lakeshore340_command( cmd );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore340_get_setpoint( int loop )
{
	char buf[ 50 ];
	char cmd[ 10 ];
	long len = sizeof buf - 1;
	double setpoint = 0.0;

	sprintf( cmd, "SETP? %d\n", loop );
	lakeshore340_talk( cmd, buf, &len );
	buf[ len ] = '\0';

	if ( *buf != '-' && *buf != '+' && ! isdigit( ( unsigned char ) *buf ) )
	    {
	        print( FATAL, "Loop Setpoint Error.\n" );
	        THROW( EXCEPTION );
	    }

	    sscanf( buf, "%lf\n", &setpoint );
	    return setpoint;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore340_loop_conf( int loop, char input, int unit, int state )
{
	char cmd[ 30 ];

	sprintf( cmd, "CSET %d, %c, %d, %d\n", loop, input, unit, state );
	lakeshore340_command( cmd );

}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore340_sens_data( char channel, int deg )
{
    char buf[ 50 ];
    char cmd[ 10 ];
    long len = sizeof buf - 1;
    double temp = 0.0;

    if ( deg == 1) //Kelvin
    {
    	sprintf(cmd, "KRDG? %c\n", channel);
    	lakeshore340_talk( cmd, buf, &len );
    	buf[ len ] = '\0';
    }

    if ( deg == 2) //Celsius
    {
        sprintf(cmd, "CRDG? %c\n", channel);
        lakeshore340_talk( cmd, buf, &len );
        buf[ len ] = '\0';
    }

    if (deg == 3) //Sensor units
	{
		sprintf(cmd, "SRDG? %c\n", channel);
		lakeshore340_talk(cmd, buf, &len);
		buf[len] = '\0';
	}

    if ( *buf != '-' && *buf != '+' && ! isdigit( ( unsigned char ) *buf ) )
    {
        print( FATAL, "Error reading temperature.\n" );
        THROW( EXCEPTION );
    }

    sscanf( buf, "%lf", &temp );

    return temp;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore340_lock( int state )
{
    char cmd[ 20 ];


    fsc2_assert( state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO );

    sprintf( cmd, "MODE %d\n", state );
    lakeshore340_command( cmd );
    lakeshore340.lock_state = state;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/


static bool
lakeshore340_command( const char * cmd )
{
    if ( gpib_write( lakeshore340.device, cmd, strlen( cmd ) - 1 ) == FAILURE )
        lakeshore340_gpib_failure( );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore340_talk( const char * cmd,
                   char *       reply,
                   long *       length )
{
    if ( gpib_write( lakeshore340.device, cmd, strlen( cmd ) - 1 ) == FAILURE
         || gpib_read( lakeshore340.device, reply, length ) == FAILURE )
        lakeshore340_gpib_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore340_gpib_failure( void )
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
