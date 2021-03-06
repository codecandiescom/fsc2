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


#pragma once
#if ! defined LECROY_WR_MODELS_HEADER
#define LECROY_WR_MODELS_HEADER


/* In this file values which differ for the different Waverunner oscilloscope
 * are defined.
 *
 * The values were assembled from the Waverunner and Waverunner-2
 * Operation's Manuals (LTXXX-OM-E Rev B and WR2-OM-E Rev C), from data
 * from LeCroy's web site and other sources found on the internet.
 * Unfortunately, it is already difficult to find out exactly which
 * models exist at all and then the information in the specifictations
 * sections of the manuals are often far from being clear or complete
 * and sometimes even contradictions exist between the information from
 * the manuals and the diverse web pages. Thus the data below sometimes
 * just represent a guess of which of the data to be found in different
 * sources are correct.
 *
 * To add possibly missing models create another set of data consisting
 * of defines for:
 *
 * LECROY_WR_CH_MAX               channel number of the highest existing
 *                                channel, this is LECROY_WR_CH2 for 2-channels
 *                                scopes and LECROY_WR_CH4 for 4-channel scopes
 * LECROY_WR_MAX_BW_IS_200MHz     define this only if the device has a
 *                                bandwidth of only 200 MHz
 * LECROY_WR_HAS_GLOBAL_BW        define this only if the device has global
 *                                bandwidth limiting capability
 * LECROY_WR_NUM_TBAS             total number of existing timebases
 * LECROY_WR_NUM_SS_TBAS          number of timebases in SINGLE SHOT mode
 * LECROY_WR_NUM_RIS_TBAS         number of timebases in RIS mode
 * LECROY_WR_SS_SAMPLE_RATE       (maximum) sample rate in SINGLE SHOT mode
 * LECROY_WR_RIS_SAMPLE_RATE      sample rate in RIS mode
 * LECROY_WR_MAX_TIMEBASE         maximum timebase setting in s/div (this seems
 *                                to be 1 ks/div for all models but on LeCroy's
 *                                web pages one also can find that it's only
 *                                100 s/div for some of the Waverunner-2 models)
 * LECROY_WR_MAX_USED_CHANNELS    maximum number of channels that can be
 *                                displayed at once
 * LECROY_WR_MAX_MEMORY_SIZE      maximum number of points per channel (single
 *                                channel, not combined channels)
 * LECROY_WR_TRG_MAX_LEVEL_CH_FAC factor by which trigger level can be higher
 *                                than the current sensitivity setting
 * LECROY_WR_TRG_MAX_LEVEL_EXT    max. trigger level for EXT trigger input
 * LECROY_WR_TRG_MAX_LEVEL_EXT10  max. trigger level for EXT10 trigger input
 * LECROY_WR_IS_XSTREAM           define this if this is a X-Stream device
 */


/* Waverunner models */

#if defined LT224
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channnels */
#define LECROY_WR_MAX_BW_IS_200MHz                   /* restricted BW */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     35    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  33    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 10    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              200    /* 200 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              10    /* 10 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         100000L    /* 100 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT322
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     35    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  33    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 10    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              200    /* 200 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              10    /* 10 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             6
#define LECROY_WR_MAX_MEMORY_SIZE         100000L    /* 100 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT342
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  34    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              500    /* 500 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 25 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             6
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT342L
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  34    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              500    /* 500 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 25 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             6
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT344
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  34    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              500    /* 500 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 25 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT344L
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  34    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE              500    /* 500 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 125 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT364
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  35    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 25 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         500000L    /* 500 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT364L
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channnels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     37    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  35    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 12    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              25    /* 25 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        2000000L    /* 2 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


/* Waverunner-2 models */

#elif defined LT262
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  37    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 10    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         100000L    /* 100 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT264
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  37    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 10    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         100000L    /* 100 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT264M
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  37    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 10    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT354
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT354M
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT354ML
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             1000    /* 1000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        2000000L    /* 2 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT372
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT372M
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT374
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT374M
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT374L
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        4000000L    /* 4 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT584
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE         250000L    /* 250 kpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT584M
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/div */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        1000000L    /* 1 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined LT584L
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_HAS_GLOBAL_BW
#define LECROY_WR_NUM_TBAS                     38    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  38    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                 11    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             2000    /* 2000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE              50    /* 50 Gs/s */
#define LECROY_WR_MAX_TIMEBASE               1000    /* 1 ks/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE        4000000L    /* 4 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES               4000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        5.0    /* 5 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.5    /* 500 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         5.0    /* 5 V */


#elif defined _44Xi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _62Xi
#define LECROY_WR_CH_MAX            LECROY_WR_CH2    /* 2 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _64Xi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _104Xi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _204Xi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _44MXi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _64MXi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _104MXi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#elif defined _204MXi
#define LECROY_WR_CH_MAX            LECROY_WR_CH4    /* 4 channels */
#define LECROY_WR_NUM_TBAS                     31    /* # of timebases */
#define LECROY_WR_NUM_SS_TBAS                  31    /* # of SS timebases */
#define LECROY_WR_NUM_RIS_TBAS                  4    /* # of RIS timebases */
#define LECROY_WR_SS_SAMPLE_RATE             5000    /* 5000 MS/s */
#define LECROY_WR_RIS_SAMPLE_RATE             200    /* 200 Gs/s */
#define LECROY_WR_MAX_TIMEBASE                 10    /* 10 s/dev */
#define LECROY_WR_MAX_USED_CHANNELS             8
#define LECROY_WR_MAX_MEMORY_SIZE       12500000L    /* 12.5 Mpts/channel */
#if ! defined LECROY_WR_MAX_AVERAGES
#define LECROY_WR_MAX_AVERAGES            1000000
#endif
#define LECROY_WR_TRG_MAX_LEVEL_CH_FAC        4.1    /* 4.1 times sensitivity */
#define LECROY_WR_TRG_MAX_LEVEL_EXT           0.4    /* 400 mV */
#define LECROY_WR_TRG_MAX_LEVEL_EXT10         4.0    /* 4 V */
#define LECROY_WR_IS_XSTREAM


#else
#error "Unknown model of LeCroy WaveRunner/X-Stream"
#endif


#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
