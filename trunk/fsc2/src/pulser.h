/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#if ! defined PULSER_HEADER
#define PULSER_HEADER

#include "fsc2.h"


typedef struct {

	const char *name;
	bool is_trigger_out;
	long trigger_out_channel_number;
	bool has_phase_switch;


	bool ( *assign_function )( int function, long connector );
	bool ( *assign_channel_to_function )( int function, long channel );
	bool ( *invert_function )( int function );
	bool ( *set_function_delay )( int function, double delay );
	bool ( *set_function_high_level )( int function, double high_voltage );
	bool ( *set_function_low_level )( int function, double low_voltage );

	bool ( *set_timebase )( double timebase );
	bool ( *set_trigger_mode )( int mode );
	bool ( *set_repeat_time )( double rep_time );
	bool ( *set_trig_in_level )( double voltage );
	bool ( *set_trig_in_slope )( int slope );
	bool ( *set_trig_in_impedance )( int state );
	bool ( *set_max_seq_len )( double seq_len );

	bool ( *set_phase_reference )( int phase, int function );

	bool ( *new_pulse )( long pulse_number );
	bool ( *set_pulse_function )( long pulse_number, int function );
	bool ( *set_pulse_position )( long pulse_number, double ptime );
	bool ( *set_pulse_length )( long pulse_number, double ptime );
	bool ( *set_pulse_position_change )( long pulse_number, double ptime );
	bool ( *set_pulse_length_change )( long pulse_number, double ptime );
	bool ( *set_pulse_phase_cycle )( long pulse_number, long cycle );

	bool ( *get_pulse_function )( long pulse_number, int *function );
	bool ( *get_pulse_position )( long pulse_number, double *ptime );
	bool ( *get_pulse_length )( long pulse_number, double *ptime );
	bool ( *get_pulse_position_change )( long pulse_number, double *ptime );
	bool ( *get_pulse_length_change )( long pulse_number, double *ptime );
	bool ( *get_pulse_phase_cycle )( long pulse_number, long *cycle );

	bool ( *phase_setup_prep )( int func, int type, int pod, long val );
	bool ( *phase_setup )( int function );
	bool ( *set_phase_switch_delay )( int function, double del_time );
	bool ( *set_grace_period )( double gp_time );
	bool ( *keep_all_pulses )( void );

} Pulser_Struct;


enum {
	P_FUNC    = ( 1 << 0 ),
	P_POS     = ( 1 << 1 ),
	P_LEN     = ( 1 << 2 ),
	P_DPOS    = ( 1 << 3 ),
	P_DLEN    = ( 1 << 4 ),
	P_PHASE   = ( 1 << 5 )
};


void pulser_struct_init( void );
void p_assign_pod( long func, Var *v );
void p_assign_channel( long func, Var *v );
void p_set_delay( long func, Var *v );
void p_inv( long func );
void p_set_v_high( long func, Var *v );
void p_set_v_low( long func, Var *v );
void p_set_timebase( Var *v );
void p_set_trigger_mode( Var *v);
void p_set_trigger_slope( Var *v );
void p_set_trigger_level( Var *v );
void p_set_trigger_impedance( Var *v );
void p_set_rep_time( Var *v );
void p_set_rep_freq( Var *v );
void p_set_max_seq_len( Var *v );
void p_phase_ref( int function, int ref );

long p_num( char *txt );
void is_pulser_driver( void );
void is_pulser_func( void *func, const char *text );
double is_mult_ns( double val, const char * text );

long p_new( long pnum );
void p_set( long pnum, int type, Var *v );
Var *p_get( char *txt, int type );
Var *p_get_by_num( long pnum, int type );

void p_phs_setup( int func, int type, int pod, long val );
void p_phs_end( int func );

void p_set_psd( int func, Var *v );
void p_set_gp( Var *v );

void keep_all_pulses( void );

void p_exists_function( int function );


#endif  /* ! PULSER_HEADER */
