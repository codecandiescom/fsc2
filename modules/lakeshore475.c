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

#include "lakeshore475.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

#define LOCK_STATE_LOCAL         0
#define LOCK_STATE_REMOTE        1
#define LOCK_STATE_REMOTE_LLO    2

#define MEASURE_DC               1
#define MEASURE_RMS              2
#define MEASURE_PEAK             3

#define ANALOG_OUTPUT_OFF        0
#define ANALOG_OUTPUT_DEFAULT    1
#define ANALOG_OUTPUT_USER_DEF   2
#define ANALOG_OUTPUT_MANUAL     3
#define ANALOG_OUTPUT_CONTROL    4

#define UNIPOLAR                 1
#define BIPOLAR                  2


int lakeshore475_init_hook(       void );
int lakeshore475_exp_hook(        void );
int lakeshore475_end_of_exp_hook( void );


Var_T * gaussmeter_field(           Var_T * v );
Var_T * gaussmeter_field_parameter( Var_T * v );
Var_T * gaussmeter_range(           Var_T * v );
Var_T * gaussmeter_measure_mode(    Var_T * v );
Var_T * gaussmeter_unit(            Var_T * v );
Var_T * gaussmeter_setpoint(        Var_T * v );
Var_T * gaussmeter_analog_mode(     Var_T * v );
Var_T * gaussmeter_digits(          Var_T * v );


static void lakeshore475_get_field_control(    void );
static void lakeshore475_get_analog_output(    void );
static double lakeshore475_query_field(        void );
static void lakeshore475_set_range(       int range );
static void lakeshore475_set_unit(             void );
static void lakeshore475_set_setpoint( double point );
static double lakeshore475_get_setpoint(       void );
static void lakeshore475_measure_mode(         void );
static void lakeshore475_analog_output(        void );
static void lakeshore475_field_control(        void );
static int lakeshore475_get_range(             void );


static bool lakeshore475_init(   const char * name );
static void lakeshore475_lock(           int state );
static bool lakeshore475_command( const char * cmd );
static void lakeshore475_gpib_failure(        void );
static bool lakeshore475_talk( const char * cmd,
                       char * reply, long * length );


static struct
{
	int device;
	int lock_state;
	int measure_mode;
	int digits;
	int unit;
	int analog_output_mode;
	int analog_volt_limit;
	double P;
	double I;
	double ramp;
	double slope;

} lakeshore475;

/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore475_init_hook(void)
{
	Need_GPIB = SET;

	lakeshore475.device = -1;
	lakeshore475.lock_state = LOCK_STATE_REMOTE_LLO;

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore475_exp_hook(void)
{
	if (!lakeshore475_init(DEVICE_NAME))
	{
		print(FATAL, "Initialization of device failed.\n");
		THROW(EXCEPTION);
	}
	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore475_end_of_exp_hook(void)
{
	lakeshore475_lock(LOCK_STATE_LOCAL);
	return 1;
}


/**************************************/
/*                                    */
/*          EDL functions             */
/*                                    */
/**************************************/

/*----------------------------------------------------*
 * Function only returns field reading, need no argument
 *----------------------------------------------------*/

Var_T *
gaussmeter_field(Var_T * v  UNUSED_ARG )
{

    return vars_push(FLOAT_VAR,
                     FSC2_MODE == TEST ? 3124.0 : lakeshore475_query_field());
}


/*----------------------------------------------------*
 * Function sets field control parameters as P value,
 * I value, ramp rate and control slope limit. To change
 * one of parameters there should be two arguments: the
 * name of parameter to be changed and its value. Function
 * can get any number of arguments and in any order.
 * For example:
 *
 * gaussmeter_field_parameter("P", 15, "slope", 110);
 * (sets 15 for P and 110 for slope limit)
 *----------------------------------------------------*/

Var_T *
gaussmeter_field_parameter( Var_T * v )
{
    if (v == NULL)
    {
        print(FATAL, "Missing arguments.\n");
        THROW(EXCEPTION);
    }

    do
    {
        if (v->type != STR_VAR) {
            print(FATAL, "Expected string value.\n");
            THROW( EXCEPTION);
        }

        int par = -1;
        const char *par_str[] =	{ "P", "I", "ramp", "slope" };

        for (size_t i = 0; i < NUM_ELEMS(par_str); i++)
            if (!strcasecmp(v->val.sptr, par_str[i]))
            {
                par = i;
                break;
            }

        if (par < 0) {
            print(FATAL, "Invlaid parameter type argument '%s'.\n",
                  v->val.sptr);
            THROW(EXCEPTION);
        }

        if ((v = vars_pop(v)) == NULL)
        {
            print(FATAL, "Value for '%s' missing.\n", par_str[par]);
            THROW(EXCEPTION);
        }


        double val = get_double(v, par_str[par]);

        switch (par)
        {
			case 0:
				lakeshore475.P = val;
				break;

			case 1:
				lakeshore475.I = val;
				break;

			case 2:
				lakeshore475.ramp = val;
				break;

			case 3:
				lakeshore475.slope = val;
				break;
        }

    } while (v != NULL);

    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_field_control();

    return vars_push(INT_VAR, 1);
}


/*----------------------------------------------------*
 * Sets field range, need a number from 1 to 5,
 * returns range if no argument
 *----------------------------------------------------*/

Var_T *
gaussmeter_range(Var_T * v)
{
    if (v == NULL)
        return vars_push(INT_VAR,
                         FSC2_MODE == TEST ? 1 : lakeshore475_get_range());

	long range = get_strict_long(v, "range");
    if (range < 1 || range > 5)
    {
        print(FATAL, "Invalid heater argument.\n");
        THROW(EXCEPTION);
    }

    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_set_range(range);

    return vars_push(INT_VAR, range);
}


/*----------------------------------------------------*
 * Specifies the measurement mode:
 * 1 = DC, 2 = RMS, 3 = peak. Argument uses umbers or
 * names of modes
 *----------------------------------------------------*/

Var_T *
gaussmeter_measure_mode(Var_T * v)
{
    if (v == NULL)
    {
        print(FATAL, "Mode argument is missing\n");
        THROW(EXCEPTION);
    }

	long mode = -1;

    if (v->type == STR_VAR)
    {
        const char *mode_str[] = { "DC", "RMS", "peak" };
        
        for (size_t i = 0; i < NUM_ELEMS(mode_str); i++)
            if (!strcasecmp(v->val.sptr, mode_str[i]))
            {
                mode = i + 1;
                break;
            }
    }
    else
        mode = get_strict_long(v, "mode");

    if (mode < MEASURE_DC || mode > MEASURE_PEAK)
    {
        print(FATAL, "Invalid mode argument\n");
        THROW(EXCEPTION);
    }

    lakeshore475.measure_mode = mode;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_measure_mode();

    return vars_push(INT_VAR, mode);
}


/*----------------------------------------------------*
 * Configures gaussmeter for units:
 * 1 = Gauss, 2 = Tesla, 3 = Oersted, 4 = Amp/meter
 *----------------------------------------------------*/

Var_T *
gaussmeter_unit(Var_T * v)
{
    if (v == NULL)
    {
        print(FATAL, "Unit argument is missing\n");
        THROW(EXCEPTION);
    }


	long int unit = -1;

    if (v->type == STR_VAR) {
        const char *unit_str[] = { "G", "T", "O", "A/m" };

        for (size_t i = 0; i < NUM_ELEMS(unit_str); i++)
            if (! strcasecmp(v->val.sptr, unit_str[i]))
            {
                unit = i + 1;
                break;
            }
    }
    else
        unit = get_strict_long(v, "unit");

    if (unit < 1 || unit > 4)
    {
        print(FATAL, "Invalid unit argument\n");
        THROW(EXCEPTION);
    }

    lakeshore475.unit = unit;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_set_unit();

    return vars_push(INT_VAR, unit);
}


/*----------------------------------------------------*
 * Specifies the final field setpoint, returns setpoint
 * if no argument
 *----------------------------------------------------*/

Var_T *
gaussmeter_setpoint(Var_T * v)
{
    if (v == NULL)
        return vars_push(FLOAT_VAR,
                         FSC2_MODE == TEST ? 1.0 : lakeshore475_get_setpoint());

    lakeshore475_get_analog_output();
    if (lakeshore475.analog_output_mode == ANALOG_OUTPUT_OFF)
        print(WARN, "Control mode is off\n");

    double setpoint = get_double(v, "setpoint");

    if (FSC2_MODE == EXPERIMENT)
		lakeshore475_set_setpoint(setpoint);

    return vars_push(FLOAT_VAR, setpoint);
}


/*----------------------------------------------------*
 * Specifies analog output mode:
 * 0 = off, 1 = default, 2 = user defined, 3 = manual,
 * 4 = control ( can use numbers and pointed names )
 * and voltage limit from 1 to 10 V as second argument
 * (may not use second argument)
 *----------------------------------------------------*/

Var_T *
gaussmeter_analog_mode(Var_T * v)
{
	long mode = -1;
	long limit;
	const char *mode_str[] = { "off", "default", "user", "manual", "control" };

    if (v == NULL)
    {
        print(FATAL, "Mode argument is missing\n");
        THROW(EXCEPTION);
    }

    if (v->type == STR_VAR)
        for (size_t i = 0; i < NUM_ELEMS(mode_str); i++)
            if (! strcasecmp(v->val.sptr, mode_str[i]))
            {
                mode = i;
                break;
            }
    else
        mode = get_strict_long(v, "mode");

    if (mode < ANALOG_OUTPUT_OFF || mode > ANALOG_OUTPUT_CONTROL)
    {
        print(FATAL, "Invalid mode argument\n");
        THROW(EXCEPTION);
    }

    lakeshore475.analog_output_mode = mode;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_analog_output();

    if ((v = vars_pop(v)) == NULL)
        return vars_push(INT_VAR, 1);

    limit = get_long(v, "limit");
    if (limit < 1 || limit > 10)
    {
        print(FATAL, "Invalid limit argument\n");
        THROW(EXCEPTION);
    }

    lakeshore475.analog_volt_limit = limit;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_analog_output();

    return vars_push(INT_VAR, 1);
}

/*----------------------------------------------------*
 * Specifies number of digits from 3 to 5.
 *----------------------------------------------------*/

Var_T *
gaussmeter_digits(Var_T * v)
{
    if (v == NULL)
    {
        print(FATAL, "Digits argument is missing\n");
        THROW(EXCEPTION);
    }

	int digits = get_strict_long(v, "number of digits");

    if (digits < 3 || digits > 5)
    {
        print(FATAL, "Invalid digits argument\n");
        THROW(EXCEPTION);
    }

    lakeshore475.digits = digits;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore475_measure_mode();

    return vars_push(INT_VAR, digits);
}


/**********************************************************/
/*                                                        */
/*       Internal (non-exported) functions                */
/*                                                        */
/**********************************************************/

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static bool
lakeshore475_init(const char * name)
{
	char buf[20];
	long len = sizeof buf - 1;

	/* Initialize GPIB communication with the temperature controller */

	if (gpib_init_device(name, &lakeshore475.device) == FAILURE)
		return FAIL;

	lakeshore475_talk("*STB?\n", buf, &len);
	buf[len] = '\0';

	/* Switch device to remote state with local lockout */

	lakeshore475_command("MODE 2\n");

	/* Default configuration */

	lakeshore475.measure_mode      = MEASURE_DC;
	lakeshore475.analog_volt_limit = 5;
	lakeshore475.unit              = 1;
	lakeshore475.digits            = 3;

	lakeshore475_get_field_control();
	lakeshore475_get_analog_output();

	return OK;
}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_get_field_control(void)
{
	char buf[60];
	long len = sizeof buf - 1;

	lakeshore475_talk("CPARAM?\n", buf, &len);
	buf[len] = '\0';

    char * bp = buf;

    if (   sscanf(bp, "%lf", &lakeshore475.P) != 1
        || ! (bp = strchr(bp, ','))
        || sscanf(++bp, "%lf", &lakeshore475.I) != 1
        || ! (bp = strchr(bp, ','))
        || 	sscanf(++bp, "%lf", &lakeshore475.ramp) != 1
        || ! (bp = strchr(bp, ','))
        || sscanf(++bp, "%lf", &lakeshore475.slope) != 1) {
        print(FATAL, "Invalid reply from device.\n");
        THROW(EXCEPTION);
    }
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void lakeshore475_get_analog_output(void)
{
	char buf[70];
	long len = sizeof buf - 1;

	lakeshore475_talk("ANALOG?\n", buf, &len);
	buf[len] = '\0';

	lakeshore475.analog_output_mode = buf[0] - '0';
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore475_query_field(void)
{
	char buf[50];
	long len = sizeof buf - 1;
	double field = 0.0;

	lakeshore475_talk("RDGFIELD?\n", buf, &len);
	buf[len] = '\0';

	if (sscanf(buf, "%lf", &field) != 1) {
        print(FATAL, "Invalid reply from device.\n");
		THROW(EXCEPTION);
	}

	return field;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_set_range(int range)
{
	char cmd[15];

	sprintf(cmd, "RANGE %d\n", range);
	lakeshore475_command(cmd);

}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_set_unit(void)
{
	char cmd[15];

	sprintf(cmd, "UNIT %d\n", lakeshore475.unit);
	lakeshore475_command(cmd);

}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_set_setpoint(double point)
{
	char cmd[25];

	sprintf(cmd, "CSETP %lf\n", point);
	lakeshore475_command(cmd);
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore475_get_setpoint(void)
{
	char buf[50];
	long len = sizeof buf - 1;

	lakeshore475_talk("CSETP?\n", buf, &len);
	buf[len] = '\0';

	double setpoint;
	if (sscanf(buf, "%lf", &setpoint) != 1) {
        print(FATAL, "Invalid reply from device.\n");
		THROW(EXCEPTION);
	}

	return setpoint;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_measure_mode(void)
{
	char cmd[25];

	sprintf(cmd, "RDGMODE %d, %d\n", lakeshore475.measure_mode,
			lakeshore475.digits - 2);
	lakeshore475_command(cmd);
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void
lakeshore475_field_control(void)
{
	char cmd[70];

	sprintf(cmd, "CPARAM %lf, %lf, %lf, %lf\n", lakeshore475.P,
			lakeshore475.I, lakeshore475.ramp, lakeshore475.slope);
	lakeshore475_command(cmd);
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_analog_output(void)
{
	char cmd[30];

	sprintf(cmd, "ANALOG %d, %d, 0, 0, 0, %d\n",
            lakeshore475.analog_output_mode,
			UNIPOLAR, lakeshore475.analog_volt_limit);
	lakeshore475_command(cmd);

	if (lakeshore475.analog_output_mode == ANALOG_OUTPUT_CONTROL)
		lakeshore475_command("CMODE 1\n");
}



/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static int
lakeshore475_get_range(void)
{
	char buf[50];
	long len = sizeof buf - 1;
	int range = 0;

	lakeshore475_talk("RANGE?\n", buf, &len);
	buf[len] = '\0';

	if (sscanf(buf, "%d", &range) != 1) {
        print(FATAL, "Inva;id reply from device.\n");
        THROW(EXCEPTION);
    }

	return range;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_lock(int state)
{
	char cmd[20];

	fsc2_assert(state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO);

	sprintf(cmd, "MODE %d\n", state);
	lakeshore475_command(cmd);
	lakeshore475.lock_state = state;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore475_command(const char * cmd)
{
	if (gpib_write(lakeshore475.device, cmd, strlen(cmd) - 1) == FAILURE)
		lakeshore475_gpib_failure();

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore475_talk(const char * cmd, char * reply, long * length)
{
	if (   gpib_write(lakeshore475.device, cmd, strlen(cmd) - 1) == FAILURE
        || gpib_read(lakeshore475.device, reply, length) == FAILURE)
		lakeshore475_gpib_failure();

	return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore475_gpib_failure(void)
{
	print(FATAL, "Communication with device failed.\n");
	THROW(EXCEPTION);
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
