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

#include "lakeshore455.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

#define LOCK_STATE_LOCAL         0
#define LOCK_STATE_REMOTE        1
#define LOCK_STATE_REMOTE_LLO    2

#define MEASURE_DC               1
#define MEASURE_RMS              2
#define MEASURE_PEAK             3


int lakeshore455_init_hook(       void );
int lakeshore455_exp_hook(        void );
int lakeshore455_end_of_exp_hook( void );


Var_T * gaussmeter_field(        Var_T * v );
Var_T * gaussmeter_range(        Var_T * v );
Var_T * gaussmeter_measure_mode( Var_T * v );
Var_T * gaussmeter_unit(         Var_T * v );
Var_T * gaussmeter_digits(       Var_T * v );


static double lakeshore455_query_field(       void );
static void lakeshore455_set_range(      int range );
static void lakeshore455_set_unit(            void );
static void lakeshore455_measure_mode(        void );
static int lakeshore455_get_range(            void );


static bool lakeshore455_init(   const char * name );
static void lakeshore455_lock(           int state );
static bool lakeshore455_command( const char * cmd );
static void lakeshore455_gpib_failure(        void );
static bool lakeshore455_talk( const char * cmd,
                       char * reply, long * length );


static struct
{
	int device;
	int lock_state;
	int measure_mode;
	int digits;
	int unit;
} lakeshore455;

/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore455_init_hook(void)
{
	Need_GPIB = SET;

	lakeshore455.device = -1;
	lakeshore455.lock_state = LOCK_STATE_REMOTE_LLO;

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore455_exp_hook(void)
{
	if (! lakeshore455_init(DEVICE_NAME))
	{
		print(FATAL, "Initialization of device failed.\n");
		THROW(EXCEPTION);
	}

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
lakeshore455_end_of_exp_hook(void)
{
	lakeshore455_lock(LOCK_STATE_LOCAL);
	return 1;
}


/**************************************/
/*                                    */
/*          EDL functions             */
/*                                    */
/**************************************/

/*----------------------------------------------------*
 * Function only returns field reading
 *----------------------------------------------------*/

Var_T *
gaussmeter_field(Var_T * v  UNUSED_ARG)
{
    return vars_push(FLOAT_VAR,
                     FSC2_MODE == TEST ? 3124.0 : lakeshore455_query_field());
}


/*----------------------------------------------------*
 * Sets fiels range, need a number from 1 to 5,
 * returns range if no argument
 *----------------------------------------------------*/

Var_T *
gaussmeter_range(Var_T * v)
{
    if (v == NULL)
        return vars_push(INT_VAR,
                         FSC2_MODE == TEST ? 1 : lakeshore455_get_range());


    long range = get_strict_long(v, "range");
    if (range < 1 || range > 5)
    {
        print(FATAL, "Invalid heater argument\n");
        THROW(EXCEPTION);
    }

    if (FSC2_MODE == EXPERIMENT)
        lakeshore455_set_range(range);

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

	long mode;

    if (v->type == STR_VAR) {
        const char * mode_str[] = { "DC", "RMS", "peak" };
        mode = is_in(v->val.sptr, mode_str, NUM_ELEMS(mode_str));
    }
    else
        mode = get_strict_long(v, "mode") + 1;

    if (mode < MEASURE_DC || mode > MEASURE_PEAK)
    {
        print(FATAL, "Invalid mode argument\n");
        THROW(EXCEPTION);
    }

    lakeshore455.measure_mode = mode;

    if (FSC2_MODE == EXPERIMENT)
        lakeshore455_measure_mode();

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

	long unit;

    if (v->type == STR_VAR)
    {
        const char *unit_str[] = { "G", "T", "O", "A/m" };
        unit = is_in(v->val.sptr, unit_str, NUM_ELEMS(unit_str)) + 1;
    }
    else
        unit = get_strict_long(v, "unit");

    if (unit < 1 || unit > 4)
    {
        print(FATAL, "Invalid unit argument\n");
        THROW(EXCEPTION);
    }

    lakeshore455.unit = unit;
    if (FSC2_MODE == EXPERIMENT)
		lakeshore455_set_unit();

    return vars_push(INT_VAR, unit);
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

	long digits = get_strict_long(v, "number of digits");
    if (digits < 3 || digits > 5)
    {
        print(FATAL, "Invalid digits argument\n");
        THROW(EXCEPTION);
    }

    lakeshore455.digits = digits;
    if (FSC2_MODE == EXPERIMENT)
        lakeshore455_measure_mode();

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
lakeshore455_init(const char * name)
{
	char buf[20];
	long len = sizeof buf - 1;

	/* Initialize GPIB communication with the temperature controller */

	if (gpib_init_device(name, &lakeshore455.device) == FAILURE)
		return FAIL;

	lakeshore455_talk("*STB?\n", buf, &len);
	buf[len] = '\0';

	/* Switch device to remote state with local lockout */

	lakeshore455_command("MODE 2\n");

	/* Default configuration */

	lakeshore455.measure_mode = MEASURE_DC;
	lakeshore455.unit = 1;
	lakeshore455.digits = 3;


	return OK;
}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static double
lakeshore455_query_field( void )
{
	char buf[50];
	char cmd[15];
	long len = sizeof buf - 1;
	double field = 0.0;

	sprintf(cmd, "RDGFIELD?\n");
	lakeshore455_talk(cmd, buf, &len);
	buf[len] = '\0';

	if (*buf != '-' && *buf != '+' && !isdigit((unsigned char) *buf))
	{
		print(FATAL, "Error.\n");
		THROW(EXCEPTION);
	}

	sscanf(buf, "%lf", &field);
	return field;

}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore455_set_range(int range)
{
	char cmd[15];

	sprintf(cmd, "RANGE %d\n", range);
	lakeshore455_command(cmd);

}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore455_set_unit( void )
{
	char cmd[15];

	sprintf(cmd, "UNIT %d\n", lakeshore455.unit);
	lakeshore455_command(cmd);

}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore455_measure_mode( void )
{
	char cmd[25];

	sprintf(cmd, "RDGMODE %d, %d\n", lakeshore455.measure_mode,
			lakeshore455.digits - 2);
	lakeshore455_command(cmd);

}



/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static int
lakeshore455_get_range( void )
{
	char buf[50];
	char cmd[15];
	long len = sizeof buf - 1;
	int range = 0;

	sprintf(cmd, "RANGE?\n");
	lakeshore455_talk(cmd, buf, &len);
	buf[len] = '\0';

	sscanf(buf, "%d", &range);
	return range;

}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore455_lock(int state)
{
	char cmd[20];

	fsc2_assert(state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO);

	sprintf(cmd, "MODE %d\n", state);
	lakeshore455_command(cmd);
	lakeshore455.lock_state = state;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore455_command(const char * cmd)
{
	if (gpib_write(lakeshore455.device, cmd, strlen(cmd) - 1) == FAILURE)
		lakeshore455_gpib_failure();

	return OK;
}

/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lakeshore455_talk(const char * cmd, char * reply, long * length)
{
	if (gpib_write(lakeshore455.device, cmd, strlen(cmd) - 1) == FAILURE
			|| gpib_read(lakeshore455.device, reply, length) == FAILURE)
		lakeshore455_gpib_failure();

	return OK;
}

/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lakeshore455_gpib_failure(void)
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
