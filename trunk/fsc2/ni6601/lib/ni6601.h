/*
 *  $Id$
 * 
 *  Copyright (C) 2002-2008 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#if ! defined NI6601_LIBRARY_HEADER
#define NI6601_LIBRARY_HEADER


#ifdef __cplusplus
extern "C" {
#endif


#include <ni6601_drv.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>


typedef struct {
    int is_init;              /* has board been used so far? */
    int fd;                   /* file descriptor for board */
    int is_used[ 4 ];
    int state[ 4 ];
} NI6601_Device_Info;


enum {
    NI6601_IDLE = 0,
    NI6601_BUSY,
    NI6601_PULSER_RUNNING,
    NI6601_CONT_PULSER_RUNNING,
    NI6601_COUNTER_RUNNING,
    NI6601_CONT_COUNTER_RUNNING,
    NI6601_BUFF_COUNTER_RUNNING
};


int ni6601_close( int /* board */ );

int ni6601_start_counter( int /* board   */,
                          int /* counter */,
                          int /* source  */ );

int ni6601_start_gated_counter( int    /* board       */,
                                int    /* counter     */,
                                double /* gate_length */,
                                int    /* source      */ );

int ni6601_start_buffered_counter( int           /* board       */,
                                   int           /* counter     */,
                                   double        /* gate_length */,
                                   int           /* source      */,
                                   unsigned long /* num_points  */,
                                   int           /* continuous  */ );

ssize_t ni6601_get_buffered_available( int /* board */ );

ssize_t ni6601_get_buffered_counts( int             /* board          */,
                                    unsigned long * /* counts         */,
                                    size_t          /* num_points     */,
                                    double          /* wait_secs      */,
                                    int *           /* quit_on_signal */,
                                    int *           /* timed_out      */,
                                    int *           /* end_of_data    */ );

int ni6601_stop_counter( int /* board   */,
                         int /* counter */ );

int ni6601_get_count( int             /* board        */,
                      int             /* counter      */,
                      int             /* wait_for_end */,
                      int             /* do_poll      */,
                      unsigned long * /* count        */,
                      int *           /* state        */ );

int ni6601_generate_continuous_pulses( int    /* board      */,
                                       int    /* counter    */,
                                       double /* high_phase */,
                                       double /* low_phase  */ );
int ni6601_stop_pulses( int /* board */, int  /* counter    */ );

int ni6601_generate_single_pulse( int    /* board    */,
                                  int    /* counter  */,
                                  double /* duration */ );

int ni6601_dio_write( int           /* board */,
                      unsigned char /* bits  */,
                      unsigned char /* mask  */ );

int ni6601_dio_read( int             /* board */,
                     unsigned char * /* bits  */,
                     unsigned char   /* mask  */ );

int ni6601_is_counter_armed( int   /* board   */,
                             int   /* counter */,
                             int * /* state   */ );

int ni6601_perror( const char * /* s */ );

const char *ni6601_strerror( void );


extern const char *ni6601_errlist[ ];
extern const int ni6601_nerr;


#define NI6601_OK        0
#define NI6601_ERR_NSB  -1
#define NI6601_ERR_NSC  -2
#define NI6601_ERR_CBS  -3
#define NI6601_ERR_IVA  -4
#define NI6601_ERR_WFC  -5
#define NI6601_ERR_BBS  -6
#define NI6601_ERR_IVS  -7
#define NI6601_ERR_BNO  -8
#define NI6601_ERR_NDV  -9
#define NI6601_ERR_NCB -10
#define NI6601_ERR_ITR -11
#define NI6601_ERR_ACS -12
#define NI6601_ERR_DFM -13
#define NI6601_ERR_DFP -14
#define NI6601_ERR_INT -15
#define NI6601_ERR_TMB -16
#define NI6601_ERR_MEM -17
#define NI6601_ERR_OFL -18
#define NI6601_ERR_TFS -19
#define NI6601_ERR_NBC -20


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif  /* NI6601_LIBRARY_HEADER */


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
