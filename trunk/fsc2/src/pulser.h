/*
  $Id$
*/


#if ! defined PULSER_HEADER
#define PULSER_HEADER

#include "fsc2.h"


typedef struct {
	int var[ 4 ][ 2 ];
	bool is_var[ 4 ][ 2 ];
} PHS;


typedef struct {

	char *name;
	bool is_trigger_out;
	long trigger_out_channel_number;

	bool ( *assign_function )( int function, long connector );
	bool ( *assign_channel_to_function )( int function, long channel );
	bool ( *invert_function )( int function );
	bool ( *set_delay_function )( int function, double delay );
	bool ( *set_function_high_level )( int function, double high_voltage );
	bool ( *set_function_low_level )( int function, double low_voltage );

	bool ( *set_timebase )( double timebase );
	bool ( *set_trigger_mode )( int mode );
	bool ( *set_repeat_time )( double time );
	bool ( *set_trig_in_level )( double voltage );
	bool ( *set_trig_in_slope )( int slope );

	bool ( *set_phase_reference )( int phase, int function );

	bool ( *new_pulse )( long pulse_number );
	bool ( *set_pulse_function )( long pulse_number, int function );
	bool ( *set_pulse_position )( long pulse_number, double time );
	bool ( *set_pulse_length )( long pulse_number, double time );
	bool ( *set_pulse_position_change )( long pulse_number, double time );
	bool ( *set_pulse_length_change )( long pulse_number, double time );
	bool ( *set_pulse_phase_cycle )( long pulse_number, int cycle );
	bool ( *set_pulse_maxlen )( long pulse_number, double time );
	bool ( *set_pulse_replacements )( long pulse_number,
									  long number_of_replacement_pulses,
									  long *list_of_replacement_pulses );

	bool ( *get_pulse_function )( long pulse_number, int *function );
	bool ( *get_pulse_position )( long pulse_number, double *time );
	bool ( *get_pulse_length )( long pulse_number, double *time );
	bool ( *get_pulse_position_change )( long pulse_number, double *time );
	bool ( *get_pulse_length_change )( long pulse_number, double *time );
	bool ( *get_pulse_phase_cycle )( long pulse_number, int *cycle );
	bool ( *get_pulse_maxlen )( long pulse_number, double *time );

	bool ( *setup_phase )( int function, PHS phs );

} Pulser_Struct;



enum {
	P_FUNC    = ( 1 << 0 ),
	P_POS     = ( 1 << 1 ),
	P_LEN     = ( 1 << 2 ),
	P_DPOS    = ( 1 << 3 ),
	P_DLEN    = ( 1 << 4 ),
	P_PHASE   = ( 1 << 5 ),
	P_MAXLEN  = ( 1 << 6 ),
	P_REPL    = ( 1 << 7 )
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
void p_set_rep_time( Var *v );
void p_set_rep_freq( Var *v );
void p_phase_ref( long function, int ref );

long p_num( char *txt );
void is_pulser_driver( void );
void is_pulser_func( void *func, const char *text );
double is_mult_ns( double val, const char * text );

long p_new( long pnum );
void p_set( long pnum, int type, Var *v );
Var *p_get( char *txt, int type );

void p_phs_setup( int func, int type, int pod, long val );
void p_phs_end( int func );


#endif  /* ! PULSER_HEADER */
