/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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


#if ! defined KEYTHLEY2600A_LL_H_
#define KEYTHLEY2600A_LL_H_


/* Internal functions from keithley2600a_ll.c */

bool keithley2600a_open( void );
bool keithley2600a_close( void );
bool keithley2600a_cmd( const char * cmd );
size_t keithley2600a_talk( const char * cmd,
                           char       * reply,
                           size_t       length,
                           bool         allow_abort );

void keithley2600a_get_state( void );
void keithley2600a_reset( void );
const char * keithley2600a_get_model( void );
void keithley2600a_show_errors( void );

int keithley2600a_get_sense( unsigned int ch );
int keithley2600a_set_sense( unsigned int ch,
                             int          sense );
double keithley2600a_get_line_frequency( void );

void keithley2600a_prep_lin_sweeps( void );
void keithley2600a_prep_list_sweeps( void );

bool keithley2600a_line_to_bool( const char * line );
int keithley2600a_line_to_int( const char * line );
double keithley2600a_line_to_double( const char * line );
double * keithley2600a_line_to_doubles( const char * line,
                                        double     * buf,
                                        int          cnt );
void keithley2600a_bad_data( void );
void keithley2600a_comm_failure( void );


/* If binary transfer is on in most cases we know exactly how many
   bytes to expect while with ASCII mode it's usually not that simple.
   To be to check when binary mode is on, but not with ASCII mode we
   use a macro which we can pass the expected number of bytes but
   which we simply disregard in ASCII mode. */

#if ! defined BINARY_TRANSFER
#define TALK( a, b,c, d, e )    keithley2600a_talk( a, b, c, d )
#else
#define TALK( a, b,c, d, e )                  \
if ( keithley2600a_talk( a, b, c, d ) != e )  \
    keithley2600a_bad_data( );
#endif

#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
