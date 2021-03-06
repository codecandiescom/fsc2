/*
 *  Library for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2014 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#if ! defined NI_DAQ_PUBLIC_INTERFACE_HEADER
#define NI_DAQ_PUBLIC_INTERFACE_HEADER


#ifdef __cplusplus
extern "C" {
#endif


#include <math.h>
#include <ni_daq_drv.h>


extern const char *ni_daq_errlist[ ];
extern const int ni_daq_nerr;


/* Basic function for opening and closing the board */

int ni_daq_open( const char * /* name */,
                 int          /* flag */ );

int ni_daq_close( int /* board */ );


/* Error handling utility functions */

int ni_daq_perror( const char * /* s */ );

const char *ni_daq_strerror( void );


/* Functions for setting the clock speeds and the output at the FREQ_OUT pin
   as well as for determining the boards current state in this respect */

int ni_daq_msc_set_clock_speed( int                      /* board   */,
                                NI_DAQ_CLOCK_SPEED_VALUE /* speed   */, 
                                int                      /* divider */ );

int ni_daq_msc_set_clock_output( int               /* board     */,
                                 NI_DAQ_CLOCK_TYPE /* daq_clock */,
                                 NI_DAQ_STATE      /* on_off    */ );

int ni_daq_msc_get_clock_state( int                        /* board     */,
                                NI_DAQ_CLOCK_TYPE *        /* daq_clock */,
                                NI_DAQ_STATE *             /* on_off    */,
                                NI_DAQ_CLOCK_SPEED_VALUE * /* speed     */,
                                int *                      /* divider   */ );

int ni_daq_msc_set_trigger( int              /* board        */,
                            NI_DAQ_TRIG_TYPE /* trigger_type */,
                            double           /* trigger_high */,
                            double           /* trigger_low  */ );


/* Functions for the AI subsystem */

int ni_daq_ai_set_speed( int                      /* board */,
                         NI_DAQ_CLOCK_SPEED_VALUE /* speed */ );

int ni_daq_ai_get_speed( int                        /* board */,
                         NI_DAQ_CLOCK_SPEED_VALUE * /* speed */ );

int ni_daq_ai_channel_configuration( int                  /* board         */,
                                     int                  /* num_channels  */,
                                     int *                /* channels      */,
                                     NI_DAQ_AI_TYPE *     /* types         */,
                                     NI_DAQ_BU_POLARITY * /* polarities    */,
                                     double               * /* ranges      */,
                                     NI_DAQ_STATE *       /* dither_enable */ );

int ni_daq_ai_acq_setup( int             /* board          */,
                         NI_DAQ_INPUT    /* start          */,
                         NI_DAQ_POLARITY /* start_polarity */,
                         NI_DAQ_INPUT /* scan_start        */,
                         NI_DAQ_POLARITY /* scan_polarity  */,
                         double          /* scan_duration  */,
                         NI_DAQ_INPUT    /* conv_start     */,
                         NI_DAQ_POLARITY /* conv_polarity  */,
                         double          /* conv_duration  */,
                         size_t          /* num_scans      */ );

int ni_daq_ai_start_acq( int /* board */ );

int ni_daq_ai_stop_acq( int /* board */ );

ssize_t ni_daq_ai_get_acq_data( int      /* board                */,
                                double * /* volts                */ [ ],
                                size_t   /* offset               */,
                                size_t   /* num_data_per_channel */,
                                int      /* wait_for_end         */ );

/* Functions for the AO subsystem */

int ni_daq_ao_channel_configuration( int                  /* board         */,
                                     int                  /* num_channels  */,
                                     int *                /* channels      */,
                                     NI_DAQ_STATE *       /* ext_reference */,
                                     NI_DAQ_BU_POLARITY * /* polarity      */ );

int ni_daq_ao( int      /* board        */,
               int      /* num_channels */,
               int *    /* channels     */,
               double * /* values       */ );


/* Functions for the GPCT subsystem */

int ni_daq_gpct_set_speed( int                      /* board */,
                           NI_DAQ_CLOCK_SPEED_VALUE /* speed */ );

int ni_daq_gpct_get_speed( int                        /* board */,
                           NI_DAQ_CLOCK_SPEED_VALUE * /* speed */ );

int ni_daq_gpct_start_counter( int          /* board   */,
                               int          /* counter */,
                               NI_DAQ_INPUT /* source  */ );

int ni_daq_gpct_start_gated_counter( int          /* board       */,
                                     int          /* counter     */,
                                     double       /* gate_length */,
                                     NI_DAQ_INPUT /* source      */ );

int ni_daq_gpct_stop_counter( int /* board */,
                              int /* counter */ );

int ni_daq_gpct_get_count( int             /* board        */,
                           int             /* counter      */,
                           int             /* wait_for_end */,
                           unsigned long * /* count        */,
                           int *           /* state        */ );

int ni_daq_gpct_single_pulse( int      /* board      */,
                              int      /* counter    */,
                              double   /* duration   */,
                              double * /* delay      */,
                              int      /* dont_start */ );

int ni_daq_gpct_continuous_pulses( int      /* board      */,
                                   int      /* counter    */,
                                   double   /* high_phase */,
                                   double   /* low_phase  */,
                                   double * /* delay      */,
                                   int      /* dont_start */ );

int ni_daq_gpct_start_pulses( int /* board   */,
                              int /* counter */ );

int ni_daq_gpct_stop_pulses( int /* board   */,
                             int /* counter */ );


/* Functions for the DIO subsystem */

int ni_daq_dio_write( int /* board */,
                      unsigned char /* value */,
                      unsigned char /* mask  */ );

int ni_daq_dio_read( int             /* board */,
                     unsigned char * /* value */,
                     unsigned char   /* mask  */ );


enum {
    NI_DAQ_IDLE = 0,
    NI_DAQ_BUSY,
    NI_DAQ_PULSER_RUNNING,
    NI_DAQ_CONT_PULSER_RUNNING,
    NI_DAQ_COUNTER_RUNNING,
    NI_DAQ_CONT_COUNTER_RUNNING
};


#define NI_DAQ_OK         0
#define NI_DAQ_ERR_NSB   -1
#define NI_DAQ_ERR_CBS   -2
#define NI_DAQ_ERR_IVA   -3
#define NI_DAQ_ERR_WFC   -4
#define NI_DAQ_ERR_BBS   -5
#define NI_DAQ_ERR_IVS   -6
#define NI_DAQ_ERR_BNO   -7
#define NI_DAQ_ERR_NDV   -8
#define NI_DAQ_ERR_NCB   -9
#define NI_DAQ_ERR_ITR  -10
#define NI_DAQ_ERR_ACS  -11
#define NI_DAQ_ERR_DFM  -12
#define NI_DAQ_ERR_DFP  -13
#define NI_DAQ_ERR_INT  -14
#define NI_DAQ_ERR_NAO  -15
#define NI_DAQ_ERR_NCS  -16
#define NI_DAQ_ERR_NAS  -17
#define NI_DAQ_ERR_NEM  -18
#define NI_DAQ_ERR_NPT  -19
#define NI_DAQ_ERR_SIG  -20
#define NI_DAQ_ERR_NER  -21
#define NI_DAQ_ERR_UAO  -22
#define NI_DAQ_ERR_NAT  -23
#define NI_DAQ_ERR_PNI  -24


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! NI_DAQ_PUBLIC_INTERFACE_HEADER */


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
