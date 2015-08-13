/*
 *  Copyright (C) 2015 Jens Thoms Toerring
 *
 *  This file is part of Fsc3.
 *
 *  Fsc3 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  Fsc3 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


/* File to be included by programs using the C version of the library */


#if ! defined rs_rto_c_h_
#define rs_rto_c_h_

#include <stdbool.h>


typedef struct RS_RTO_no_namespace rs_rto_t;

typedef struct {
    double start;
    double end;
    size_t length;
} Data_Header_t;


enum
{
    FSC3_SUCCESS,
    FSC3_INVALID_ARG,
    FSC3_COMM_FAILURE,
    FSC3_BAD_DATA,
    FSC3_OP_ERROR,
    FSC3_OUT_OF_MEMORY,
    FSC3_OTHER_ERROR
};

enum
{
    Log_Level_None,              // no logging at all (and no file opened)
    Log_Level_Low,               // logs errors only
    Log_Level_Normal,            // logs function calls and errors
    Log_Level_High               // logs function calls, all data and errors
};

enum
{
    Model_RTO1002,
    Model_RTO1004,
    Model_RTO1012,
    Model_RTO1014,
    Model_RTO1022,
    Model_RTO1024,
    Model_RTO1044
};

enum
{
    Option_B1,
    Option_B4,
    Option_B10,
    Option_B18,
    Option_B19,
    Option_B101,
    Option_B102,
    Option_B103,
    Option_B104,
    Option_B200,
    Option_B201,
    Option_B202,
    Option_B203,
    Option_B205,
    Option_K1,
    Option_K2,
    Option_K3,
    Option_K4,
    Option_K5,
    Option_K6,
    Option_K7,
    Option_K8,
    Option_K9,
    Option_K11,
    Option_K12,
    Option_K13,
    Option_K17,
    Option_K21,
    Option_K22,
    Option_K23,
    Option_K24,
    Option_K26,
    Option_K31,
    Option_K40,
    Option_K50,
    Option_K52,
    Option_K55,
    Option_K60,
    Option_U1
};

enum
{
    Channel_Ext,
    Channel_Ch1,
    Channel_Ch2,
    Channel_Ch3,
    Channel_Ch4,
    Channel_Math1,
    Channel_Math2,
    Channel_Math3,
    Channel_Math4,
};

enum
{
    Trig_Slope_Negative,
    Trig_Slope_Positive,
    Trig_Slope_Either
};

enum
{
    Trig_Mode_Auto,
    Trig_Mode_Normal,
    Trig_Mode_Free_Running
};

enum
{
    Coupling_AC,
    Coupling_DC1M,
    Coupling_DC50
};

enum
{
    Bandwidth_Full,
    Bandwidth_MHz20,
    Bandwidth_MHz200,
    Bandwidth_MHz800
};

enum
{
    Polarity_Positive,
    Polarity_Negative,
};

enum
{
    Acq_Mode_Normal,
    Acq_Mode_Average,
    Acq_Mode_Segmented
};

enum
{
    Filter_Type_Off,
    Filter_Type_Low_Pass,
    Filter_Type_High_Pass
};

enum
{
    Filter_Cut_Off_kHz5,
    Filter_Cut_Off_kHz50,
    Filter_Cut_Off_MHz50
};



rs_rto_t *
rs_rto_open( char   const  * ip,
			 int             log_level,
             char         ** error_str );

int
rs_rto_close(  rs_rto_t * rs );


int
rs_rto_model( rs_rto_t const * rs,
			  int            * model );

int
rs_rto_options( rs_rto_t const  * rs,
				int            ** options,
				size_t          * len );

int
rs_rto_has_option( rs_rto_t const * rs,
				   int              option,
				   bool           * is_available );

int
rs_rto_name(  rs_rto_t const  * rs,
              char     const ** name );

int
rs_rto_num_channels( rs_rto_t const * rs,
                     int            * num );

int
rs_rto_max_memory( rs_rto_t const * rs,
                   unsigned long  * size );

int
rs_rto_display_enabled( rs_rto_t const * rs,
                        bool           * state );

int
rs_rto_set_display_enabled( rs_rto_t * rs,
                            bool     * state );

int
rs_rto_keyboard_locked( rs_rto_t const * rs,
                        bool           * state );

int
rs_rto_set_keyboard_locked( rs_rto_t * rs,
                            bool     * state );

int
rs_rto_trigger_mode( rs_rto_t const * rs,
                     int            * mode );

int
rs_rto_trigger_set_mode( rs_rto_t * rs,
                         int      * mode );

int
rs_rto_trigger_source( rs_rto_t const * rs,
                       int            * soirce );

int
rs_rto_trigger_set_source( rs_rto_t * rs,
                           int      * source );

int
rs_rto_trigger_slope( rs_rto_t const * rs,
                      int            * slope );

int
rs_rto_trigger_set_slope( rs_rto_t * rs,
                          int      * slope );


int
rs_rto_trigger_channel_slope( rs_rto_t const * rs,
                              int              channel,
                              int            * slope );

int
rs_rto_trigger_set_channel_slope( rs_rto_t * rs,
                                  int        channel,
                                  int      * slope );

int
rs_rto_trigger_level( rs_rto_t * rs,
                      double   * level );

int
rs_rto_trigger_set_level( rs_rto_t * rs,
                          double   * level );

int
rs_rto_trigger_channel_level( rs_rto_t * rs,
                              int        channel,
                              double   * level );

int
rs_rto_trigger_set_channel_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * level );

int
rs_rto_trigger_min_level( rs_rto_t * rs,
                          double   * min_level );

int
rs_rto_trigger_channel_min_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * min_level );

int
rs_rto_trigger_max_level( rs_rto_t * rs,
                          double   * max_level );

int
rs_rto_trigger_channel_max_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * max_level );

int
rs_rto_trigger_is_overloaded( rs_rto_t * rs,
                              bool     * is_overloaded );

int
rs_rto_trigger_position( rs_rto_t * rs,
                         double   * pos );

int
rs_rto_trigger_set_position( rs_rto_t * rs,
                             double   * pos );

int
rs_rto_trigger_earliest_position( rs_rto_t * rs,
                                  double   * min_pos );

int
rs_rto_trigger_latest_position( rs_rto_t * rs,
                                double   * max_pos );

int
rs_rto_trigger_out_pulse_state( rs_rto_t const * rs,
                                bool           * trig_out_enabled );

int
rs_rto_trigger_set_out_pulse_state( rs_rto_t * rs,
                                    bool     * trig_out_enable );


int
rs_rto_trigger_out_pulse_polarity( rs_rto_t const * rs,
                                   int            * pol );

int
rs_rto_trigger_set_out_pulse_polarity( rs_rto_t * rs,
                                       int      * pol );

int
rs_rto_trigger_out_pulse_length( rs_rto_t const * rs,
                                 double         * len );

int
rs_rto_trigger_set_out_pulse_length( rs_rto_t * rs,
                                     double   * len );

int
rs_rto_trigger_out_pulse_delay( rs_rto_t * rs,
                                double   * delay );

int
rs_rto_trigger_set_out_pulse_delay( rs_rto_t * rs,
                                    double   * delay );

int
rs_rto_trigger_min_out_pulse_delay( rs_rto_t * rs,
                                    double   * min_delay );

int
rs_rto_trigger_max_out_pulse_delay( rs_rto_t * rs,
                                    double   * max_delay );

int
rs_rto_trigger_raise( rs_rto_t * rs );

int
rs_rto_acq_timebase( rs_rto_t * rs,
                     double   * timebase );

int
rs_rto_acq_set_timebase( rs_rto_t * rs,
                         double   * timebase );

int
rs_rto_acq_shortest_timebase( rs_rto_t const * rs,
                              double         * shortest );

int
rs_rto_acq_longest_timebase( rs_rto_t const * rs,
                             double         * longest );

int
rs_rto_acq_shortest_timebase_const_resolution( rs_rto_t const * rs,
                                               double         * shortest );

int
rs_rto_acq_longest_timebase_const_resolution( rs_rto_t const * rs,
                                              double         * longest );

int
rs_rto_acq_record_length( rs_rto_t      * rs,
                          unsigned long * record_length );

int
rs_rto_acq_set_record_length( rs_rto_t      * rs,
                              unsigned long * record_length );

int
rs_rto_acq_min_record_length( rs_rto_t const * rs,
                              unsigned long  * min_record_length );

int
rs_rto_acq_max_record_length( rs_rto_t      * rs,
                              unsigned long * max_record_length );

int
rs_rto_acq_resolution( rs_rto_t * rs,
                       double   * resolution );

int
rs_rto_acq_set_resolution( rs_rto_t * rs,
                           double   * resolution );

int
rs_rto_acq_lowest_resolution( rs_rto_t * rs,
                              double   * lowest );

int
rs_rto_acq_highest_resolution( rs_rto_t * rs,
                               double   * highest );

int
rs_rto_acq_average_count( rs_rto_t const * rs,
                          unsigned long  * count );

int
rs_rto_acq_set_average_count( rs_rto_t      * rs,
                              unsigned long * count );

int
rs_rto_acq_max_average_count( rs_rto_t const * rs,
                              unsigned long  * max_count );

int
rs_rto_acq_segment_count( rs_rto_t const * rs,
                          unsigned long  * count );

int
rs_rto_acq_set_segment_count( rs_rto_t      * rs,
                              unsigned long * count );

int
rs_rto_acq_max_segment_count( rs_rto_t const * rs,
                              unsigned long  * max_count );

int
rs_rto_acq_reference_position( rs_rto_t * rs,
                               double   * pos );

int
rs_rto_acq_set_reference_position( rs_rto_t * rs,
                                   double   * pos );

int
rs_rto_acq_mode( rs_rto_t const * rs,
                 int            * mode );

int
rs_rto_acq_set_mode( rs_rto_t * rs,
                     int      * mode );

int
rs_rto_acq_available_segments( rs_rto_t      * rs,
                               unsigned long * count );

int
rs_rto_acq_is_running( rs_rto_t * rs,
                       bool     * is_running );

int
rs_rto_acq_is_waiting_for_trigger( rs_rto_t * rs,
                                   bool     * is_waiting );

int
rs_rto_acq_run( rs_rto_t * rs );

int
rs_rto_acq_run_single( rs_rto_t * rs );

int
rs_rto_acq_run_continuous( rs_rto_t * rs );

int
rs_rto_acq_stop( rs_rto_t * rs,
                 bool     * was_running );


int
rs_rto_acq_download_limits_enabled( rs_rto_t * rs,
                                    bool     * state );

int
rs_rto_acq_set_download_limits_enabled( rs_rto_t * rs,
                                        bool     * state );

int
rs_rto_acq_download_limits( rs_rto_t * rs,
                            double   * start,
                            double   * end );

int
rs_rto_acq_set_download_limits( rs_rto_t * rs,
                                double   * start,
                                double   * end );

int
rs_rto_acq_max_download_limits( rs_rto_t * rs,
                                double   * start,
                                double   * end );

int
rs_rto_channel_exists( rs_rto_t const  * rs,
                       int               ch,
                       bool            * exists );

int
rs_rto_channel_name( rs_rto_t const  * rs,
                     int               ch,
                     char           ** name );

int
rs_rto_channel_state( rs_rto_t * rs,
                      int        ch,
                      bool     * state );

int
rs_rto_channel_set_state( rs_rto_t * rs,
                          int        ch,
                          bool     * state );

int
rs_rto_channel_is_overloaded( rs_rto_t * rs,
                              int        ch,
                              bool     * is_overloaded );

int
rs_rto_channel_coupling( rs_rto_t * rs,
                         int        ch,
                         int      * coup );

int
rs_rto_channel_set_coupling( rs_rto_t * rs,
                             int        ch,
                             int      * coup );

int
rs_rto_channel_scale( rs_rto_t * rs,
                      int        ch,
                      double   * scale );

int
rs_rto_channel_set_scale( rs_rto_t * rs,
                          int        ch,
                          double   * scale );

int
rs_rto_channel_min_scale( rs_rto_t * rs,
                          int        ch,
                          double   * min_scale );

int
rs_rto_channel_max_scale( rs_rto_t * rs,
                          int        ch,
                          double   * max_scale );

int
rs_rto_channel_offset( rs_rto_t * rs,
                       int        ch,
                       double   * offset );

int
rs_rto_channel_set_offset( rs_rto_t * rs,
                           int        ch,
                           double   * offset );

int
rs_rto_channel_min_offset( rs_rto_t * rs,
                           int        ch,
                           double   * min_offset );

int
rs_rto_channel_max_offset( rs_rto_t * rs,
                           int        ch,
                           double   * max_offset );

int
rs_rto_channel_position( rs_rto_t * rs,
                         int        ch,
                         double   * pos );

int
rs_rto_channel_set_position( rs_rto_t * rs,
                             int        ch,
                             double   * pos );

int
rs_rto_channel_impedance( rs_rto_t const * rs,
                          int              ch,
                          double         * imp );

int
rs_rto_channel_set_impedance( rs_rto_t * rs,
                              int        ch,
                              double   * imp );

int
rs_rto_channel_min_impedance( rs_rto_t const * rs,
                              int              ch,
                              double         * min_imp );

int
rs_rto_channel_max_impedance( rs_rto_t const * rs,
                              int              ch,
                              double         * max_imp );

int
rs_rto_channel_bandwidth( rs_rto_t const * rs,
                          int              ch,
                          int            * bw );

int
rs_rto_channel_set_bandwidth( rs_rto_t * rs,
                              int        ch,
                              int      * bw );

int
rs_rto_channel_filter_type( rs_rto_t const * rs,
                            int              ch,
                            int            * ft );

int
rs_rto_channel_set_filter_type( rs_rto_t * rs,
                                int        ch,
                                int      * ft );

int
rs_rto_channel_cut_off( rs_rto_t const * rs,
                        int              ch,
                        int            * fco );


int
rs_rto_channel_set_cut_off( rs_rto_t * rs,
                            int        ch,
                            int      * fco );

int
rs_rto_channel_header( rs_rto_t      * rs,
                       int             ch,
                       Data_Header_t * header );

int
rs_rto_channel_data( rs_rto_t  * rs,
                     int         ch,
                     double   ** data,
                     size_t    * length );

int
rs_rto_channel_segment_data( rs_rto_t   * rs,
                             int          ch,
                             double   *** data,
                             size_t     * num_segments,
                             size_t     * length );

int
rs_rto_channel_segment_data_subset( rs_rto_t        * rs,
                                    int               ch,
                                    unsigned long     start_index,
                                    unsigned long     count,
                                    double        *** data,
                                    size_t          * num_segments,
                                    size_t          * length );

int
rs_rto_channel_function( rs_rto_t  * rs,
                         int         ch,
                         char     ** func );

int
rs_rto_channel_set_function( rs_rto_t   * rs,
                             int          ch,
                             char const * func );

char const *
rs_rto_last_error( rs_rto_t const * rs );


#endif


/*                                                                              
 * Local variables:                                                             
 * tab-width: 4                                                                 
 * indent-tabs-mode: nil                                                        
 * End:                                                                         
 */
