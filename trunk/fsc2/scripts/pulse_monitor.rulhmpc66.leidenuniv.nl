#!/usr/local/bin/fsc2 -T

/***************************************************************************/
/*                                                                         */
/*  EDL monitor script pulses experiments with the 275 GHz spectrometer.   */
/*                                                                         */
/* $Id$ */
/*                                                                         */
/***************************************************************************/

DEVICES:

rs_sml01;
rb_pulser;
rb8509;
ips120_10;


ASSIGNMENTS:

TRIGGER_MODE: INTERNAL, REPEAT_TIME = 10 ms;


VARIABLES:

current_field;
new_field;
new_set_field;
sweep_rate = 1.0;
new_sweep_rate;
freq = 10 MHz;
Sweep_State = 0;


PS[ 5 ];
PL[ 5 ];
PSV[ 5 ];
PLV[ 5 ];
PMS[ 5 ];
Freq, Current_Field, New_Field, Sweep_Rate, Sweep_Up,
Sweep_Stop, Sweep_Down, Pause, Clear;
Acq_Rate, New_Acq_Rate;
Pause_Display = 1;
STOPPED = 0;
UP = 1;
DOWN = -1;
I = 0;
NP, NL;
np, nl;
MAX_LEN = 16777215;


PREPARATIONS:

P1:   FUNCTION = MW,
	  START    = 120 ns,
	  LENGTH   = 60 ns;

P2:   FUNCTION = MW,
	  START    = 400 ns,
	  LENGTH   = 60 ns;

P3:   FUNCTION = MW,
	  START    = 700 ns,
	  LENGTH   = 100 ns;

P4:   FUNCTION = RF,
	  START    = 190 ns,
	  LENGTH   = 100 ns;

P5:   FUNCTION = DETECTION,
	  START    = 180 ns,
	  LENGTH   = 100 ns;

init_1d( );


EXPERIMENT:

IF magnet_sweep( ) != STOPPED {
    magnet_sweep( STOPPED );
}

current_field = get_field( );
new_field = current_field;

hide_toolbox( "ON" );
PSV[ 1 ] = int( P1.START / 10 ns );
PS[ 1 ] = input_create( "INT_INPUT", PSV[ 1 ] * 10, "Position of MW pulse 1" );
PLV[ 1 ] = int( P1.LENGTH / 10 ns );
PL[ 1 ] = input_create( "INT_INPUT", PLV[ 1 ] * 10, "Length of MW pulse 1" );

PSV[ 2 ] = int( P2.START / 10 ns );
PS[ 2 ] = input_create( "INT_INPUT", PSV[ 2] * 10, "Position MW of pulse 2" );
PLV[ 2 ] = int( P2.LENGTH / 10 ns );
PL[ 2 ] = input_create( "INT_INPUT", PLV[ 2 ] * 10, "Length of MW pulse 2" );

PSV[ 3 ] = int( P3.START / 10 ns );
PS[ 3 ] = input_create( "INT_INPUT", PSV[ 3 ] * 10, "Position of MW pulse 3" );
PLV[ 3 ] = int( P3.LENGTH / 10 ns );
PL[ 3 ] = input_create( "INT_INPUT", PLV[ 3 ] * 10, "Length of MW pulse 3" );

PSV[ 4 ] = int( P4.START / 10 ns );
PS[ 4 ] = input_create( "INT_INPUT", PSV[ 4 ] * 10, "Position of RF pulse" );
PLV[ 4 ] = int( P4.LENGTH / 10 ns );
PL[ 4 ] = input_create( "INT_INPUT", PLV[ 4 ] * 10, "Length of RF pulse" );

PSV[ 5 ] = int( P5.START / 10 ns );
PS[ 5 ] = input_create( "INT_INPUT", PSV[ 5 ] * 10, "Position of DET pulse" );
PLV[ 5 ] = int( P5.LENGTH / 10 ns );
PL[ 5 ] = input_create( "INT_INPUT", PLV[ 5 ] * 10, "Length of DET pulse" );

Freq    = input_create( "FLOAT_INPUT", freq / 1 MHz, "RF frequency (MHz)" );
Current_Field = output_create( "FLOAT_OUTPUT", current_field,
                               "Current field [G]" );
New_Field  = input_create( "FLOAT_INPUT", current_field, "New field [G]" );
Sweep_Rate = input_create( "FLOAT_INPUT", sweep_rate, "Sweep rate [G/s]" );
Sweep_Up   = button_create( "RADIO_BUTTON", "Sweep up" );
Sweep_Stop = button_create( "RADIO_BUTTON", Sweep_Up, "Stop sweep" );
Sweep_Down = button_create( "RADIO_BUTTON", Sweep_Up, "Sweep Down" );
Pause      = button_create( "PUSH_BUTTON", "Pause display" );
Clear      = button_create( "NORMAL_BUTTON", "Clear screen" );
button_state( Sweep_Stop, "ON" );
button_state( Pause, "ON" );
hide_toolbox( "OFF" );

PMS[ 1 ] = int( pulser_pulse_minimum_specs( P1 ) / 10 ns );
PMS[ 2 ] = int( pulser_pulse_minimum_specs( P2 ) / 10 ns );
PMS[ 3 ] = int( pulser_pulse_minimum_specs( P3 ) / 10 ns );
PMS[ 4 ] = int( pulser_pulse_minimum_specs( P4 ) / 10 ns );
PMS[ 5 ] = int( pulser_pulse_minimum_specs( P5 ) / 10 ns );


FOREVER {

	/* Most of the following only needs to be run when the user changed
	   something in the toolbox... */

	IF toolbox_changed( ) {

		/* The first part of the loop is dealing with the pulse settings. It
		   gets a bit complicated because we have to check the new settings for
		   each pulse so it won't make the pulser kill the experiment just
		   because the pulse can't be set... */

		/* Checks for first MW pulse */

		IF input_changed( PS[ 1 ] ) {
			NP = input_value( PS[ 1 ] );
			IF NP % 10 != 0 {
				input_value( PS[ 1 ], PSV[ 1 ] * 10 );
			} ELSE {
				NP /= 10;
				IF NP < PMS[ 1 ] | NP + PLV[ 1 ] + PMS[ 2 ] >= PSV[ 2 ] {
					input_value( PS[ 1 ], PSV[ 1 ] * 10 );
				} ELSE {
					PSV[ 1 ] = NP;
					P1.START = NP * 10 ns;
					pulser_update( P1 );
				}
			}
		}

		IF input_changed( PL[ 1 ] ) {
			NL = input_value( PL[ 1 ] );
			IF NL % 10 != 0 {
				input_value( PL[ 1 ], PLV[ 1 ] * 10 );
			} ELSE {
				NL /= 10;
				IF NL < 0 | PSV[ 1 ] + NL + PMS[ 2 ] >= PSV[ 2 ] {
					input_value( PL[ 1 ], PLV[ 1 ] * 10 );
				} ELSE {
					PLV[ 1 ] = NL;
					P1.LENGTH = NL * 10 ns;
					pulser_update( );
				}
			}
		}

		/* Checks for second MW pulse */

		IF input_changed( PS[ 2 ] ) {
			NP = input_value( PS[ 2 ] );
			IF NP % 10 != 0 {
				input_value( PS[ 2 ], PSV[ 2 ] * 10 );
			} ELSE {
				NP /= 10;
				IF NP <= PSV[ 1 ] + PLV[ 1 ] + PMS[ 2 ] |
				   NP + PLV[ 2 ] + PMS[ 3 ] >= PSV[ 3 ] {
					input_value( PS[ 2 ], PSV[ 2 ] * 10 );
				} ELSE {
					PSV[ 2 ] = NP;
					P2.START = NP * 10 ns;
					pulser_update( );
				}
			}
		}

		IF input_changed( PL[ 2 ] ) {
			NL = input_value( PL[ 2 ] );
			IF NL % 10 != 0 {
				input_value( PL[ 2 ], PLV[ 2 ] * 10 );
			} ELSE {
				NL /= 10;
				IF NL < 0 | PSV[ 2 ] + NL + PMS[ 3 ] >= PSV[ 3 ] {
					input_value( PL[ 2 ], PLV[ 2 ] * 10 );
				} ELSE {
					PLV[ 2 ] = NL;
					P2.LENGTH = NL * 10 ns;
					pulser_update( );
				}
			}
		}

		/* Checks for third MW pulse */

		IF input_changed( PS[ 3 ] ) {
			NP = input_value( PS[ 3 ] );
			IF NP % 10 != 0 {
				input_value( PS[ 3 ], PSV[ 3 ] * 10 );
			} ELSE {
				NP /= 10;
				IF NP <= PSV[ 2 ] + PLV[ 2 ] + PMS[ 3 ] |
				   NP + PLV[ 3 ] > MAX_LEN {
					input_value( PS[ 3 ], PSV[ 3 ] * 10 );
				} ELSE {
					PSV[ 3 ] = NP;
					P3.START = NP * 10 ns;
					pulser_update( );
				}
			}
		}

		IF input_changed( PL[ 3 ] ) {
			NL = input_value( PL[ 3 ] );
			IF NL % 10 != 0 {
				input_value( PL[ 3 ], PLV[ 3 ] * 10 );
			} ELSE {
				NL /= 10;
				IF NL < 0 | PSV[ 3 ] + NL > MAX_LEN {
					input_value( PL[ 3 ], PLV[ 3 ] * 10 );
				} ELSE {
					PLV[ 3 ] = NL;
					P3.LENGTH = NL * 10 ns;
					pulser_update( );
				}
			}
		}

		/* Checks for the RF Pulse */

		IF input_changed( PS[ 4 ] ) {
			NP = input_value( PS[ 4 ] );
			IF NP % 10 != 0 {
				input_value( PS[ 4 ], PSV[ 4 ] * 10 );
			} ELSE {
				NP /= 10;
				IF NP <= PMS[ 4 ] | NP > MAX_LEN {
					input_value( PS[ 4 ], PSV[ 4 ] * 10 );
				} ELSE {
					PSV[ 4 ] = NP;
					P4.START = NP * 10 ns;
					pulser_update( );
				}
			}
		}

		IF input_changed( PL[ 4 ] ) {
			NL = input_value( PL[ 4 ] );
			IF NL % 10 != 0 {
				input_value( PL[ 4 ], PLV[ 4 ] * 10 );
			} ELSE {
				NL /= 10;
				IF NL < 0 | NL > MAX_LEN {
					input_value( PL[ 4 ], PLV[ 4 ] * 10 );
				} ELSE {
					PLV[ 4 ] = NL;
					P4.LENGTH = NL * 10 ns;
					pulser_update( );
				}
			}
		}

		/* Checks for the detection Pulse */

		IF input_changed( PS[ 5 ] ) {
			NP = input_value( PS[ 5 ] );
			IF NP % 10 != 0 {
				input_value( PS[ 5 ], PSV[ 5 ] * 10 );
			} ELSE {
				NP /= 10;
				IF NP <= PMS[ 5 ] | NP + PLV[ 5 ] > MAX_LEN {
					input_value( PS[ 5 ], PSV[ 5 ] * 10 );
				} ELSE {
					PSV[ 5 ] = NP;
					P5.START = NP * 10 ns;
					pulser_update( );
				}
			}
		}

		IF input_changed( PL[ 5 ] ) {
			NL = input_value( PL[ 5 ] );
			IF NL % 10 != 0 {
				input_value( PL[ 5 ], PLV[ 5 ] * 10 );
			} ELSE {
				NL /= 10;
				IF NL < 0 | PSV[ 5 ] + NL > MAX_LEN {
					input_value( PL[ 5 ], PLV[ 5 ] * 10 );
				} ELSE {
					PLV[ 5 ] = NL;
					P5.LENGTH = NL * 10 ns;
					pulser_update( );
				}
			}
		}

		/* The second section of the loop deals with the buttons and entry
		   fields for field control.... */

		/* Check if the sweep up button has been pressed and start sweeping up
		   (unless we're already sweeping up) */

		IF  Sweep_State != UP & button_state( Sweep_Up ) {
			IF Sweep_State == STOPPED {
				magnet_sweep( UP );
				Sweep_State = UP;
			}
	
			IF Sweep_State == DOWN {
				magnet_sweep( UP );
				Sweep_State = UP;
			}
	
			draw_marker( I + 1, "YELLOW" );
			button_state( Pause, "OFF" );
		}

		/* Check if the sweep down button has been pressed and start sweeping
		   down (unless we're already sweeping down) */

		IF Sweep_State != DOWN & button_state( Sweep_Down ) {
			IF Sweep_State == STOPPED {
				magnet_sweep( DOWN );
				Sweep_State = DOWN;
			}

			IF Sweep_State == UP {
				magnet_sweep( DOWN );
				Sweep_State = DOWN;
			}

			draw_marker( I + 1, "BLUE" );
			button_state( Pause, "OFF" );
		}

		/* Check if the sweep stop button has been pressed while we're
		   sweeping */

		IF Sweep_State != STOPPED & button_state( Sweep_Stop ) {
			magnet_sweep( STOPPED );
			Sweep_State = STOPPED;
			draw_marker( I + 1, "RED" );
		}

		/* Check if a new field has been set - if yes go to the new field
		   (which automatically stops a sweep) after checking that it's within
		   the allowed limits */

		new_set_field = input_value( New_Field );
		IF abs( new_set_field - new_field ) > 0.149 G {
			IF new_set_field > 114304 G | new_set_field < 0 G {
				input_value( New_Field, new_field );
			} ELSE {
				Sweep_State = STOPPED;
				new_field = new_set_field;
				button_state( Sweep_Stop, "ON" );
				current_field = set_field( new_field );
				draw_marker( I + 1, "RED" );
			}
			output_value( Current_Field, current_field );
		}

		/* Check if a new sweep rate has been set - if yes set the new rate
		   after checking that it's within the allowed limits */

		new_sweep_rate = abs( input_value( Sweep_Rate ) );
		IF abs( sweep_rate - new_sweep_rate ) > 0.01 {
			IF new_sweep_rate <= 33.1 & new_sweep_rate >= 0.23814 {
				sweep_rate = magnet_sweep_rate( new_sweep_rate );
				IF Sweep_State != STOPPED {
					draw_marker( I + 1, "GREEN" )
				}
			}
			input_value( Sweep_Rate, sweep_rate );
		}

		/* Check if the clear curve button has been pressed */

		IF button_state( Clear ) {
			clear_curve( );
			clear_marker( );
			rescale( 64 );
			I = 0;
		}

		Pause_Display = button_state( Pause );
	}

	/* The fourth and final part of the loop are the things that need to be
	   done each time through the loop and mostly deals with getting data from
	   the ADC (that gets its input from the boxcar integrator) */

	/* Update the output field with the current field if necessary */

	UNLESS Sweep_State == STOPPED {
		current_field = get_field( );
		output_value( Current_Field, current_field );
	}

	IF ! Pause_Display {
		I += 1;
		display( I, daq_get_voltage( CH0 ) );
	} ELSE {
		wait( 0.2 s );
	}
}


ON_STOP:

/* At the and of the experiment make sure the sweep is stopped */

IF Sweep_State != STOPPED {
	magnet_sweep( STOPPED );
}