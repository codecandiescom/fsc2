/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#if ! defined LECROY93XX_MODELS_HEADER
#define LECROY93XX_MODELS_HEADER


/* In this file values which differ for the different models of the
 * 93xx series of oscilloscopes are defined.
 *
 * The values were assembled mostly from data from LeCroy's web site.
 * Unfortunately, it is already difficult to find out exactly which
 * models exist at all and then the information in the specifictations
 * sections of the manuals (as far as I had access to them) are often
 * far from being clear or complete and, moreover, contradictions exist
 * between the information from the manuals and the diverse web pages.
 * Thus the data below sometimes just represent a guess at which of the
 * data to be found in different sources are correct.
 */


/*-------- Settings for all models --------*/

/* Maximum sensitivity setting */

#define LECROY93XX_MAX_SENS                  2.0e-3     /* 2 mV */


/* RIS (Random Interleaved Sampling) sample rate (except 9361/9362 which
   don't seem to have this capability at all) */

#define LECROY93XX_RIS_SAMPLE_RATE               10     /* 10 Gs/s */


/* Slowest timebase before switching to roll mode */

#define LECROY93XX_MAX_TIMEBASE                 0.2     /* 200 ms */


/* Maximum number of traces that can be displayed at once */

#define LECROY93XX_MAX_USED_CHANNELS              4


/* Maximum trigger level as factor of sensitivity of trigger channel */

#define LECROY93XX_TRG_MAX_LEVEL_CH_FAC    5.0    /* 5 div */


/* Maximum trigger levels for EXT and EXT10 */

#define LECROY93XX_TRG_MAX_LEVEL_EXT       0.5    /* 500 mV */
#define LECROY93XX_TRG_MAX_LEVEL_EXT10     5.0    /* 5 V */


/* Minimum memory size that can be set */

#define LECROY93XX_MIN_MEMORY_SIZE    500


/* The trigger delay can be set with a resolution of 1/10 of the timebase */

#define LECROY93XX_TRIG_DELAY_RESOLUTION 0.1



/*-------- Model specific settings --------*/

/*
 * To add possibly missing models create another set of data consisting
 * of defines for:
 *
 * LECROY93XX_CH_MAX            channel number of the highest existing channel,
 *                              this is LECROY93XX_CH2 for 2-channels scopes
 *                              and LECROY93XX_CH4 for 4-channel scopes
 * LECROY93XX_MAX_MEMORY_SIZE   maximum number of points per channel (single
 *                              channel, not combined channels)
 * LECROY93XX_HAS_200MHz_BW_LIMITER   define this only if the device has a
 *                              200 MHz bandwidth limiter
 * LECROY93XX_NUM_TBAS          total number of existing timebases, counting
 *                              all from the fastest up to the one before the
 *                              the device switches to roll mde
 * LECROY93XX_NUM_SS_TBAS       number of timebases in SINGLE SHOT mode
 * LECROY93XX_NUM_RIS_TBAS      number of timebases in RIS mode
 * LECROY93XX_SS_SAMPLE_RATE    (maximum) sample rate in SINGLE SHOT mode
 * LECROY93XX_MIN_TIMEBASE      minimum timebase
 * LECROY93XX_1_MOHM_MIN_SENS   lowest sensitivity at 1 MOhm input impedance
 * LECROY93XX_50_OHM_MIN_SENS   lowest sensitivity at 50 Ohm input impedance
 *
 * If LECROY93XX_MAIN is defined also define two arrays of doubles, called
 * 'fixed_sens' and 'max_offset', the first initialized to the sensitivity
 * settings at which the maximum offset for a channel changes, the second
 * to the corresponding maximum offsets.
 */


/*-------- 9304 models --------*/

/* Lecroy 9304 */

#if defined LECROY9304
#define LECROY93xx_MODEL                     "9304"
#define LECROY93XX_MAX_MEMORY_SIZE           10000L     /* 10 kpts/channel */
#endif

/* Lecroy 9304A */

#if defined LECROY9304A
#define LECROY93xx_MODEL                    "9304A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9304AM */

#if defined LECROY9304AM
#define LECROY93xx_MODEL                   "9304AM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9304AL */

#if defined LECROY9304AL
#define LECROY93xx_MODEL                   "9304AL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


/* Lecroy 9304C */

#if defined LECROY9304C
#define LECROY93xx_MODEL                    "9304C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9304CM */

#if defined LECROY9304CM
#define LECROY93xx_MODEL                   "9304CM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9304CL */

#if defined LECROY9304CL
#define LECROY93xx_MODEL                   "9304CL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


#if    defined LECROY9304    \
    || defined LECROY9304A   \
    || defined LECROY9304AM  \
    || defined LECROY9304AL  \
    || defined LECROY9304C   \
    || defined LECROY9304CM  \
    || defined LECROY9304CL
#define LECROY93XX_CH_MAX            LECROY93XX_CH4     /* 4 channnels */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   21     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  12     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               100     /* 100 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 1.0e-2, 0.2, 5.0 };
	double max_offsets[ ] = { 0.12, 1.2, 24.0 };
#endif
#endif


/*-------- 9310 models --------*/

/* Lecroy 9310 */

#if defined LECROY9310
#define LECROY93xx_MODEL                     "9310"
#define LECROY93XX_MAX_MEMORY_SIZE           10000L     /* 10 kpts/channel */
#endif

/* Lecroy 9310A */

#if defined LECROY9310A
#define LECROY93xx_MODEL                    "9310A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9310AM */

#if defined LECROY9310AM
#define LECROY93xx_MODEL                   "9310AM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9310AL */

#if defined LECROY9310AL
#define LECROY93xx_MODEL                   "9310AL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


/* Lecroy 9310C */

#if defined LECROY9310C
#define LECROY93xx_MODEL                    "9310C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9310CM */

#if defined LECROY9310CM
#define LECROY93xx_MODEL                   "9310CM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9310CL */

#if defined LECROY9310CL
#define LECROY93xx_MODEL                   "9310CL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


#if    defined LECROY9310    \
    || defined LECROY9310A   \
    || defined LECROY9310AM  \
    || defined LECROY9310AL  \
    || defined LECROY9310C   \
    || defined LECROY9310CM  \
    || defined LECROY9310CL
#define LECROY93XX_CH_MAX            LECROY93XX_CH2     /* 2 channnels */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   21     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  13     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               100     /* 100 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 1.0e-2, 0.2, 5.0 };
	double max_offsets[ ] = { 0.12, 1.2, 24.0 };
#endif
#endif


/*-------- 9314 models --------*/

/* Lecroy 9314 */

#if defined LECROY9314
#define LECROY93xx_MODEL                     "9314"
#define LECROY93XX_MAX_MEMORY_SIZE           10000L     /* 10 kpts/channel */
#endif

/* Lecroy 9314A */

#if defined LECROY9314A
#define LECROY93xx_MODEL                    "9314A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9314AM */

#if defined LECROY9314AM
#define LECROY93xx_MODEL                   "9314AM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9314AL */

#if defined LECROY9314AL
#define LECROY93xx_MODEL                   "9314AL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


/* Lecroy 9314C */

#if defined LECROY9314C
#define LECROY93xx_MODEL                    "9314C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9314CM */

#if defined LECROY9314CM
#define LECROY93xx_MODEL                   "9314CM"
#define LECROY93XX_MAX_MEMORY_SIZE          200000L     /* 200 kpts/channel */
#endif

/* Lecroy 9314CL */

#if defined LECROY9314CL
#define LECROY93xx_MODEL                   "9314CL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


#if    defined LECROY9314    \
    || defined LECROY9314A   \
    || defined LECROY9314AM  \
    || defined LECROY9314AL  \
    || defined LECROY9314C   \
    || defined LECROY9314CM  \
    || defined LECROY9314CL
#define LECROY93XX_CH_MAX            LECROY93XX_CH4     /* 4 channnels */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   21     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  13     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               100     /* 100 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 1.0e-2, 0.2, 5.0 };
	double max_offsets[ ] = { 0.12, 1.2, 24.0 };
#endif
#endif


/*-------- 9350 models --------*/

/* Lecroy 9350 */

#if defined LECROY9350
#define LECROY93xx_MODEL                     "9350"
#define LECROY93XX_MAX_MEMORY_SIZE           25000L     /* 25 kpts/channel */
#endif

/* Lecroy 9350A */

#if defined LECROY9350A
#define LECROY93xx_MODEL                    "9350A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9350AM */

#if defined LECROY9350AM
#define LECROY93xx_MODEL                   "9350AM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9350AL */

#if defined LECROY9350AL
#define LECROY93xx_MODEL                   "9350AL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


/* Lecroy 9350C */

#if defined LECROY9350C
#define LECROY93xx_MODEL                    "9350C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9350CM */

#if defined LECROY9350CM
#define LECROY93xx_MODEL                   "9350CM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9350CL */

#if defined LECROY9350CL
#define LECROY93xx_MODEL                   "9350CL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


#if    defined LECROY9350    \
    || defined LECROY9350A   \
    || defined LECROY9350AM  \
    || defined LECROY9350AL  \
    || defined LECROY9350C   \
    || defined LECROY9350CM  \
    || defined LECROY9350CL
#define LECROY93XX_CH_MAX            LECROY93XX_CH2     /* 2 channnels */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   23     /* # of SS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               500     /* 500 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 1.0e-2, 0.2, 5.0 };
	double max_offsets[ ] = { 0.12, 1.2, 24.0 };
#endif
#endif


#if    defined LECROY9350    \
    || defined LECROY9350A   \
    || defined LECROY9350C
#define LECROY93XX_NUM_RIS_TBAS                  11     /* # of RIS timebases */
#endif

#if    defined LECROY9350AM  \
    || defined LECROY9350AL  \
    || defined LECROY9350CM  \
    || defined LECROY9350CL
#define LECROY93XX_NUM_RIS_TBAS                  12     /* # of RIS timebases */
#endif


/*-------- 9354 models --------*/

/* Lecroy 9354 */

#if defined LECROY9354
#define LECROY93xx_MODEL                     "9354"
#define LECROY93XX_MAX_MEMORY_SIZE           25000L     /* 25 kpts/channel */
#endif

/* Lecroy 9354A */

#if defined LECROY9354A
#define LECROY93xx_MODEL                    "9354A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9354AM */

#if defined LECROY9354AM
#define LECROY93xx_MODEL                   "9354AM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9354AL */

#if defined LECROY9354AL
#define LECROY93xx_MODEL                   "9354AL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


/* Lecroy 9354C */

#if defined LECROY9354C
#define LECROY93xx_MODEL                    "9354C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9354CM */

#if defined LECROY9354CM
#define LECROY93xx_MODEL                   "9354CM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9354CL */

#if defined LECROY9354CL
#define LECROY93xx_MODEL                   "9354CL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif

/* Lecroy 9354TM */

#if defined LECROY9354CM
#define LECROY93xx_MODEL                   "9354TM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif

/* Lecroy 9354CTM */

#if defined LECROY9354CTM
#define LECROY93xx_MODEL                  "9354CTM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif


#if    defined LECROY9354    \
    || defined LECROY9354A   \
    || defined LECROY9354AM  \
    || defined LECROY9354AL  \
    || defined LECROY9354C   \
    || defined LECROY9354CM  \
    || defined LECROY9354CL  \
    || defined LECROY9354TM  \
    || defined LECROY9354CTM
#define LECROY93XX_CH_MAX            LECROY93XX_CH4     /* 4 channnels */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   23     /* # of SS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               500     /* 500 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 1.0e-2, 0.2, 5.0 };
	double max_offsets[ ] = { 0.12, 1.2, 24.0 };
#endif
#endif


#if    defined LECROY9354    \
    || defined LECROY9354A   \
    || defined LECROY9354C
#define LECROY93XX_NUM_RIS_TBAS                  11     /* # of RIS timebases */
#endif

#if    defined LECROY9354AM  \
    || defined LECROY9354AL  \
    || defined LECROY9354CM  \
    || defined LECROY9354CL  \
    || defined LECROY9354TM  \
    || defined LECROY9354CTM
#define LECROY93XX_NUM_RIS_TBAS                  12     /* # of RIS timebases */
#endif


/* 9361/62 models (they don't seem to have a RIS mode) */

/* Lecroy 9361 */

#if defined LECROY9361
#define LECROY93xx_MODEL                     "9361"
#define LECROY93XX_SS_SAMPLE_RATE              2500     /* 2500 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   26     /* # of SS timebases */
#define LECROY93XX_1_MOHM_MIN_SENS              5.0     /* 5 V */
#define LECROY93XX_50_OHM_MIN_SENS              5.0     /* 5 V */
#endif

/* Lecroy 9362 */

#if defined LECROY9362
#define LECROY93xx_MODEL                     "9362"
#define LECROY93XX_HAS_NO_BW_LIMITER
#define LECROY93XX_SS_SAMPLE_RATE              5000     /* 5000 MS/s */
#define LECROY93XX_MIN_TIMEBASE             5.0e-10     /* 500 ps */
#define LECROY93XX_NUM_TBAS                      27     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   27     /* # of SS timebases */
#define LECROY93XX_1_MOHM_MIN_SENS              1.0     /* 1 V */
#define LECROY93XX_50_OHM_MIN_SENS              1.0     /* 1 V */
#endif

#if    defined LECROY9361    \
    || defined LECROY9362
#define LECROY93XX_CH_MAX            LECROY93XX_CH2     /* 2 channnels */
#define LECROY93XX_NUM_RIS_TBAS                   0     /* # of RIS timebases */
#define LECROY93XX_OFFSET_FACTOR                8.0
#endif


/*-------- 9370 models --------*/

/* Lecroy 9370 */

#if defined LECROY9350
#define LECROY93xx_MODEL                     "9370"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9370A */

#if defined LECROY9370A
#define LECROY93xx_MODEL                    "9370A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9370AM */

#if defined LECROY9370AM
#define LECROY93xx_MODEL                   "9370AM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9370AL */

#if defined LECROY9370AL
#define LECROY93xx_MODEL                   "9350AL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


/* Lecroy 9370C */

#if defined LECROY9370C
#define LECROY93xx_MODEL                    "9370C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9370CM */

#if defined LECROY9370CM
#define LECROY93xx_MODEL                   "9370CM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 250 kpts/channel */
#endif

/* Lecroy 9370CL */

#if defined LECROY9370CL
#define LECROY93xx_MODEL                   "9370CL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


#if    defined LECROY9370    \
    || defined LECROY9370A   \
    || defined LECROY9370AM  \
    || defined LECROY9370AL  \
    || defined LECROY9370C   \
    || defined LECROY9370CM  \
    || defined LECROY9370CL
#define LECROY93XX_CH_MAX            LECROY93XX_CH2     /* 2 channnels */
#define LECROY93XX_HAS_200MHz_BW_LIMITER                /* restricted BW */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   23     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  12     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               500     /* 500 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS             10.0     /* 10 V */
#define LECROY93XX_50_OHM_MIN_SENS              1.0     /* 1 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 5.0e-3, 0.1, 1.0, 10.0 };
	double max_offsets[ ] = { 0.4, 1.0, 10.0, 100.0 };
#endif
#endif


/*-------- 9374 models --------*/

/* Lecroy 9374 */

#if defined LECROY9374
#define LECROY93xx_MODEL                     "9374"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9374A */

#if defined LECROY9374A
#define LECROY93xx_MODEL                    "9374A"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9374AM */

#if defined LECROY9374AM
#define LECROY93xx_MODEL                   "9374AM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 200 kpts/channel */
#endif

/* Lecroy 9374AL */

#if defined LECROY9374AL
#define LECROY93xx_MODEL                   "9374AL"
#define LECROY93XX_MAX_MEMORY_SIZE         1000000L     /* 1 Mpts/channel */
#endif


/* Lecroy 9374C */

#if defined LECROY9374C
#define LECROY93xx_MODEL                    "9374C"
#define LECROY93XX_MAX_MEMORY_SIZE           50000L     /* 50 kpts/channel */
#endif

/* Lecroy 9374CM */

#if defined LECROY9374CM
#define LECROY93xx_MODEL                   "9374CM"
#define LECROY93XX_MAX_MEMORY_SIZE          250000L     /* 200 kpts/channel */
#endif

/* Lecroy 9374CL */

#if defined LECROY9374CL
#define LECROY93xx_MODEL                   "9374CL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 1 Mpts/channel */
#endif

/* Lecroy 9374TM */

#if defined LECROY9374CM
#define LECROY93xx_MODEL                   "9374TM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif

/* Lecroy 9374CTM */

#if defined LECROY9374CTM
#define LECROY93xx_MODEL                  "9374CTM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif


#if    defined LECROY9374    \
    || defined LECROY9374A   \
    || defined LECROY9374AM  \
    || defined LECROY9374AL  \
    || defined LECROY9374C   \
    || defined LECROY9374CM  \
    || defined LECROY9374CL  \
    || defined LECROY9374TM  \
    || defined LECROY9374CTM
#define LECROY93XX_CH_MAX            LECROY93XX_CH4     /* 4 channnels */
#define LECROY93XX_HAS_200MHz_BW_LIMITER                /* restricted BW */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   23     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  12     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE               500     /* 500 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS             10.0     /* 10 V */
#define LECROY93XX_50_OHM_MIN_SENS              1.0     /* 1 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 5.0e-3, 0.1, 1.0, 10.0 };
	double max_offsets[ ] = { 0.4, 1.0, 10.0, 100.0 };
#endif
#endif


/*-------- 9384 models --------*/

/* Lecroy 9374 */

#if defined LECROY9384
#define LECROY93xx_MODEL                     "9384"
#define LECROY93XX_MAX_MEMORY_SIZE          100000L     /* 100 kpts/channel */
#endif

/* Lecroy 9384A */

#if defined LECROY9384A
#define LECROY93xx_MODEL                    "9384A"
#define LECROY93XX_MAX_MEMORY_SIZE          100000L     /* 100 kpts/channel */
#endif

/* Lecroy 9384AM */

#if defined LECROY9384AM
#define LECROY93xx_MODEL                   "9384AM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif

/* Lecroy 9384AL */

#if defined LECROY9384AL
#define LECROY93xx_MODEL                   "9384AL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif


/* Lecroy 9384C */

#if defined LECROY9384C
#define LECROY93xx_MODEL                    "9384C"
#define LECROY93XX_MAX_MEMORY_SIZE          100000L     /* 100 kpts/channel */
#endif

/* Lecroy 9384CM */

#if defined LECROY9384CM
#define LECROY93xx_MODEL                   "9384CM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif

/* Lecroy 9384CL */

#if defined LECROY9384CL
#define LECROY93xx_MODEL                   "9384CL"
#define LECROY93XX_MAX_MEMORY_SIZE         2000000L     /* 2 Mpts/channel */
#endif

/* Lecroy 9384TM */

#if defined LECROY9384TM
#define LECROY93xx_MODEL                   "9384TM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif

/* Lecroy 9384CTM */

#if defined LECROY9384CTM
#define LECROY93xx_MODEL                  "9384CTM"
#define LECROY93XX_MAX_MEMORY_SIZE          500000L     /* 500 kpts/channel */
#endif


#if    defined LECROY9384    \
    || defined LECROY9384A   \
    || defined LECROY9384AM  \
    || defined LECROY9384AL  \
    || defined LECROY9384C   \
    || defined LECROY9384CM  \
    || defined LECROY9384CL  \
    || defined LECROY9384TM  \
    || defined LECROY9384CTM
#define LECROY93XX_CH_MAX            LECROY93XX_CH4     /* 4 channnels */
#define LECROY93XX_HAS_200MHz_BW_LIMITER                /* restricted BW */
#define LECROY93XX_NUM_TBAS                      26     /* # of timebases */
#define LECROY93XX_NUM_SS_TBAS                   25     /* # of SS timebases */
#define LECROY93XX_NUM_RIS_TBAS                  11     /* # of RIS timebases */
#define LECROY93XX_SS_SAMPLE_RATE              1000     /* 1000 MS/s */
#define LECROY93XX_MIN_TIMEBASE              1.0e-9     /* 1 ns */
#define LECROY93XX_1_MOHM_MIN_SENS             10.0     /* 10 V */
#define LECROY93XX_50_OHM_MIN_SENS              1.0     /* 1 V */
#if defined LECROY93XX_MAIN_
	double fixed_sens[ ] = { 5.0e-3, 0.1, 1.0, 10.0 };
	double max_offsets[ ] = { 0.4, 1.0, 10.0, 100.0 };
#endif
#endif


#endif /* ! defined LECROY93XX_MODELS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
