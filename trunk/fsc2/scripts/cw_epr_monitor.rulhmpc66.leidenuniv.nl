#!/usr/local/bin/fsc2

/*
  $Id$

  Copyright (C) 2003-2004 Jens Thoms Toerring

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


DEVICES:

ips120_10;
sr810;


VARIABLES:

current_field;
new_field;
new_set_field;
sweep_rate = 1.0;
new_sweep_rate;
acq_rate;
Current_Field, New_Field, Sweep_Rate, Sweep_Up, Sweep_Stop, Sweep_Down,
Cur_Acq_Rate, Pause, Clear;
Sweep_State = 0;
Acq_Rate, New_Acq_Rate;
Pause_State = 0;
Old_Pause_State;
MAX_POINTS = 8191;            // maximum number of data points
POnS;
I, J;
dt, new_dt, st = 0.0;
Auto_Acq = 0;

/* Some variables to make the program  easier to read */

STOPPED = 0;
UP = 1;
DOWN = -1;

/* List of all useful acquisition rates */

acq_rates_list[ 10 ] = { 32 Hz, 16 Hz, 8 Hz, 4 Hz, 2 Hz, 1 Hz, 500 mHz,
						 250 mHz, 125 mHz, 62.5 mHz };


PREPARATIONS:

init_1d( "Points", "EPR signal [uV]" );

EXPERIMENT:

IF magnet_sweep( ) != STOPPED {
	magnet_sweep( STOPPED );
}

current_field = get_field( );
new_field = current_field;

sweep_rate = magnet_sweep_rate( sweep_rate );

/* Now ask the lock-in for the current setting of the acquisition rate for
   auto-acquisition, then figure out the index in the array of allowed
   values (which is used later to set up the menu), if necessary reduce
   the maximum acquisition rate to 32 Hz and finally tell the lock-in to
   use this acquisition rate for auto-acqusition of channel 1. */

acq_rate = 1.0 / lockin_get_sample_time( );

IF acq_rate >= acq_rates_list[ 1 ] {
	acq_rate = acq_rates_list[ 1 ];
	Cur_Acq_Rate = 1;
} ELSE {
	FOR I = 2 : size( acq_rates_list )
	{
		IF abs( acq_rates_list[ I ] - acq_rate ) < 0.0001 Hz
		{
			Cur_Acq_Rate = I;
			BREAK;
		}
	}
}

lockin_auto_setup( 1.0 / acq_rates_list[ Cur_Acq_Rate ], 1 );

/* Unlock the lock-ins keyboard */

lockin_lock_keyboard( "OFF" );

/* Finally, create the form with the buttons for sweeping etc. */

hide_toolbox( "ON" );
Current_Field = output_create( "FLOAT_OUTPUT", current_field,
							   "Current field [G]" );
New_Field  = input_create( "FLOAT_INPUT", current_field, "New field [G]" );
Sweep_Rate = input_create( "FLOAT_INPUT", sweep_rate, "Sweep rate [G/s]" );
Sweep_Up   = button_create( "RADIO_BUTTON", "Sweep up" );
Sweep_Stop = button_create( "RADIO_BUTTON", Sweep_Up, "Stop sweep" );
Sweep_Down = button_create( "RADIO_BUTTON", Sweep_Up, "Sweep Down" );
button_state( Sweep_Stop, "ON" );
Acq_Rate   = menu_create( "Lock-in acquisition rate", "32 Hz", "16 Hz", "8 Hz",
						   "4 Hz", "2 Hz", "1 Hz", "1/2 Hz", "1/4 Hz",
						   "1/8 Hz", "1/16 Hz" );
menu_choice( Acq_Rate, Cur_Acq_Rate );
POnS       = button_create( "PUSH_BUTTON", "Pause On Stop" );
button_state( POnS, "ON" );
Pause = button_create( "PUSH_BUTTON", "Pause display" );
button_state( Pause, 1 );
Clear = button_create( "NORMAL_BUTTON", "Clear screen" );
hide_toolbox( "OFF" );

I = 0;

dt = delta_time( );

FOREVER {

	/* If we're sweeping update the output field with the current field */

	IF Sweep_State != STOPPED {
		current_field = get_field( );
		output_value( Current_Field, current_field );
	}

	/* Now test if one of the buttons or input fields has changed its state */

	IF  toolbox_changed( ) {

		/* Check if the sweep up button has been pressed and we're not already
		   sweeping up - distinguish between the start of a sweep and the
		   reversal of the sweep direction, in the later case also stop and
		   restart the lock-in's auto-acquisition to empty its internal data
		   buffer. */

		IF  Sweep_State != UP & button_state( Sweep_Up ) & I < MAX_POINTS {
			IF Sweep_State == STOPPED {
				magnet_sweep( UP );
				Sweep_State = UP;
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}

			IF Sweep_State == DOWN {
				lockin_auto_acquisition( "OFF" );
				magnet_sweep( UP );
				Sweep_State = UP;
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}

			draw_marker( I + 1, "YELLOW" );
			button_state( Pause, 0 );
		}

		/* Check if the sweep down button has been pressed and we're not
		   already sweeping down  - distinguish between the start of a sweep
		   and the reversal of the sweep direction, in the later case also
		   stop and restart the lock-in's auto-acquisition to empty its
		   internal data buffer. */

		IF Sweep_State != DOWN & button_state( Sweep_Down ) & I < MAX_POINTS {
			IF Sweep_State == STOPPED {
				magnet_sweep( DOWN );
				Sweep_State = DOWN;
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}

			IF Sweep_State == UP {
				lockin_auto_acquisition( "OFF" );
				magnet_sweep( DOWN );
				Sweep_State = DOWN;
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}

			draw_marker( I + 1, "BLUE" );
			button_state( Pause, 0 );
		}

		/* Check if the sweep stop button has been pressed while we're
		   sweeping */

		IF Sweep_State != STOPPED & button_state( Sweep_Stop ) {
			lockin_auto_acquisition( "OFF" );
			Auto_Acq = 0;
			magnet_sweep( STOPPED );
			Sweep_State = STOPPED;
			draw_marker( I + 1, "RED" );

			IF button_state( POnS ) {
				Pause_State = 1;
				button_state( Pause, 1 );
			} ELSE {
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}
		}

		/* Check if a new field has been set - if yes set it (which automati-
		   cally stops a sweep) after checking that it's within the allowed
		   limits */

		new_set_field = input_value( New_Field );
		IF abs( new_set_field - new_field ) > 0.149 G {
			IF new_set_field > 114304 G | new_set_field < 0 G {
				input_value( New_Field, new_field );
			} ELSE {
				Sweep_State = STOPPED;
				new_field = new_set_field;
				lockin_auto_acquisition( "OFF" );
				Auto_Acq = 0;
				button_state( Sweep_Stop, "ON" );
				current_field = set_field( new_field );
				draw_marker( I + 1, "RED" );
			}
			output_value( Current_Field, current_field );
		}

		/* Check if a new sweep rate has been set - if yes set the new rate
		   after checking that it's within the allowed limits. Because any
		   data stored in the lock-ins internal data buffer have become
		   meaningless stop and restart auto-acquisition to clean out the
		   buffer. */

		new_sweep_rate = abs( input_value( Sweep_Rate ) );
		IF abs( sweep_rate - new_sweep_rate ) > 0.01 {
			IF new_sweep_rate <= 33.1 & new_sweep_rate >= 0.23814 {
				sweep_rate = magnet_sweep_rate( new_sweep_rate );
				IF Sweep_State != STOPPED {
					lockin_auto_acquisition( "OFF" );
					lockin_auto_acquisition( "ON" );
					dt = delta_time( );
					st = 0.0 s;
					Auto_Acq = 1;
					draw_marker( I + 1, "GREEN" )
				}
			}
			input_value( Sweep_Rate, sweep_rate );
		}

		/* Check if a new acquisition rate for the lock-in has been selected -
		   also in this case the data in the lock-ins buffer aren't useful
		   any more so clean them by stopping and restarting the auto-
		   acquisition. */

		New_Acq_Rate = menu_choice( Acq_Rate );
		IF New_Acq_Rate != Cur_Acq_Rate {
			Cur_Acq_Rate = New_Acq_Rate;
			acq_rate = acq_rates_list[ Cur_Acq_Rate ];
			lockin_auto_setup( 1.0 / acq_rate, 1 );
			IF Sweep_State != STOPPED {
				lockin_auto_acquisition( "OFF" );
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}
		}

		Old_Pause_State = Pause_State;
		Pause_State = button_state( Pause );
		IF Pause_State != Old_Pause_State {
			lockin_auto_acquisition( ! Pause_State );
			Old_Pause_State = Pause_State;
			IF ! Pause_State {
				dt = delta_time( );
				st = 0.0 s;
			}
			Auto_Acq = ! Pause_State;
		}

		/* Check if the clear curve button has been pressed - if yes also clear
		   the lock-ins internal buffer. */

		IF button_state( Clear ) {
			clear_curve( );
			clear_marker( );
			rescale( 64 );
			I = 0;
			IF Sweep_State != STOPPED {
				lockin_auto_acquisition( "OFF" );
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				st = 0.0 s;
				Auto_Acq = 1;
			}
		}
	}

	/* If the magnet is sweeping the field get new data points, otherwise
	   wait a short moment to keep the program from using up the complete
	   computer time. If we already have acquired MAX_POINTS data points or the
	   magnet sweep has been haltet because one of the field limits has been
	   reached also stop the sweep without further user intervention. */

	IF ! Pause_State {

		/* If the lock-in is running in auto-acquisition mode do a guess on
		   how many points have been acquired since the last time we were
		   here and fetch them, otherwise just get a single one. */

		IF Auto_Acq {
		    new_dt = delta_time( );
		    st += new_dt - dt;
			dt = new_dt;
			WHILE st > 0.0 s {
				I += 1;
				display( I, lockin_get_data( 1 ) * 1.0e6 );
				st -= 1.0 / acq_rate;
			}
			if st < 0.0 s {
				st = 0 s;
			}
		} ELSE {
			I += 1;
			display( I, lockin_get_data( 1 ) * 1.0e6 );
		}

		IF ( Sweep_State != STOPPED & magnet_sweep( ) == STOPPED ) |
			 I >= MAX_POINTS {
			lockin_auto_acquisition( "OFF" );
			Auto_Acq = 0;
			magnet_sweep( STOPPED );
			Sweep_State = STOPPED;
			button_state( Sweep_Stop, "ON" );
			draw_marker( I, "RED" );
			IF button_state( POnS ) {
				Pause_State = 1;
			} ELSE {
				lockin_auto_acquisition( "ON" );
				dt = delta_time( );
				Auto_Acq = 1;
			}
		}
	} ELSE {
		wait( 0.1 s );
	}
}


ON_STOP:

/* At the and of the experiment make sure the sweep is stopped and the
   lock-ins auto-acquisition gets stopped. */

IF Sweep_State != STOPPED {
	magnet_sweep( STOPPED );
	lockin_auto_acquisition( "OFF" );
}
