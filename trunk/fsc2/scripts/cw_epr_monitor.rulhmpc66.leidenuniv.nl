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
Cur_Acq_Rate, Pause, DWS, Clear;
Sweep_State = 0;
Acq_Rate, New_Acq_Rate;
Draw_While_Stopped = 0;
Pause_State = 0;
TB_Changed;
I, J;
st = 0.0;

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

/*Stop magnet if necessary */

IF magnet_sweep( ) != STOPPED {
	magnet_sweep( STOPPED );
}

/* Get current field and write it into the corresponding output field */

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

/* Initialize auto-acquisition of the lock-in */

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
DWS = button_create( "PUSH_BUTTON", "Display while stopped" );
button_state( DWS, Draw_While_Stopped );
Pause = button_create( "PUSH_BUTTON", "Pause display" );
button_state( Pause, Pause_State );
Clear = button_create( "NORMAL_BUTTON", "Clear screen" );
hide_toolbox( "OFF" );

I = 0;
delta_time( );

FOREVER {

	/* Update the output field with the current field */

	current_field = get_field( );
	output_value( Current_Field, current_field );

	/* Check if the magnet reached the upper or lower limit in which case
	   a running sweep must be stopped */

	IF Sweep_State != STOPPED & magnet_sweep( ) == STOPPED {
		Sweep_State = STOPPED;
		button_state( Sweep_Stop, "ON" );
		draw_marker( I, "RED" );

		IF ! Pause_State & ! Draw_While_Stopped {	
			lockin_auto_acquisition( "OFF" );
		}
	}

	TB_Changed = toolbox_changed( );

	/* If the display is stopped and no user interaction was detected wait a
	   short time and restart the main loop */

	IF ( Pause_State | ( Sweep_State == STOPPED & ! Draw_While_Stopped ) )
	   & ! TB_Changed {
		wait( 0.1 s );
		NEXT;
	}

	/* If displaying data isn't stopped get as many points as there are in
	   the lock-ins buffer and display them. */

	IF ! Pause_State & ( Sweep_State != STOPPED | Draw_While_Stopped ) {

    	st += delta_time( );

		WHILE st >= 1 / acq_rate {
			I += 1;
			display( I, lockin_get_data( 1 ) * 1.0e6 );
			st -= 1 / acq_rate;
		}
	}

	/* If there was no user interaction we're already done */

	IF ! TB_Changed {
		NEXT;
	}

	/* Check if "Sweep up" button has been pressed */

	IF button_state( Sweep_Up ) & Sweep_State != UP {

		IF ! Pause_State {
			IF ( Sweep_State == STOPPED & Draw_While_Stopped ) |
			   Sweep_State == DOWN {
				lockin_auto_acquisition( "OFF" );
			}

			lockin_auto_acquisition( "ON" );
			delta_time( );
			st = 0.0 s;
		}

		magnet_sweep( UP );
		Sweep_State = UP;
		draw_marker( I + 1, "YELLOW" );
	}

	/* Check if "Sweep Down" button has been pressed */

	IF button_state( Sweep_Down ) & Sweep_State != DOWN {

		IF ! Pause_State {
			IF ( Sweep_State == STOPPED & Draw_While_Stopped ) |
			   Sweep_State == UP {
				lockin_auto_acquisition( "OFF" );
			}

			lockin_auto_acquisition( "ON" );
			delta_time( );
			st = 0.0 s;
		}

		magnet_sweep( DOWN );
		Sweep_State = DOWN;
		draw_marker( I + 1, "BLUE" );
	}

	/* Check if "Stop Sweep" button has been pressed */

	IF button_state( Sweep_Stop ) & Sweep_State != STOPPED {

		IF ! Pause_State {
			lockin_auto_acquisition( "OFF" );

			IF Draw_While_Stopped {
				lockin_auto_acquisition( "ON" );
				delta_time( );
				st = 0.0 s;
			}
		}

		magnet_sweep( STOPPED );
		Sweep_State = STOPPED;
		draw_marker( I + 1, "RED" );
	}

	/* Check if a new field value has been entered (this will automatically
	   stop a running sweep) */

	IF input_changed( New_Field ) {

		new_set_field = input_value( New_Field );

		IF new_set_field <= 114304 G & new_set_field >= 0 G &
		   abs( new_set_field - new_field ) >= 0.14288 G {

			new_field = set_field( new_set_field );

			IF Sweep_State != STOPPED {
				IF ! Pause_State {
					lockin_auto_acquisition( "OFF" );

					IF Draw_While_Stopped {
						lockin_auto_acquisition( "ON" );
						delta_time( );
						st = 0.0 s;
					}
				}

				Sweep_State = STOPPED;
				button_state( Sweep_Stop, "ON" );
				draw_marker( I + 1, "RED" );
			}
		}

		input_value( New_Field, new_field );
	}

	/* Check if a new sweep rate has been set (this will automatically
	   stop a running sweep) */

	IF input_changed( Sweep_Rate ) {

		new_sweep_rate = abs( input_value( Sweep_Rate ) );

		IF  new_sweep_rate <= 33.1 & new_sweep_rate >= 0.23814 &
		    abs( sweep_rate - new_sweep_rate ) >= 0.01 {

			sweep_rate = magnet_sweep_rate( new_sweep_rate );

			IF Sweep_State != STOPPED {
				IF ! Pause_State {
					lockin_auto_acquisition( "OFF" );

					IF Draw_While_Stopped {
						lockin_auto_acquisition( "ON" );
						delta_time( );
						st = 0.0 s;
					}
				}

				Sweep_State = STOPPED;
				button_state( Sweep_Stop, "ON" );
				draw_marker( I + 1, "RED" );
			}
		}

		input_value( Sweep_Rate, sweep_rate );
	}

	/* Check if a new acquisition rate for the lock-in has been selected (this
	   will automatically stop a running sweep) */

	IF menu_changed( Acq_Rate ) {

		New_Acq_Rate = menu_choice( Acq_Rate );

		IF New_Acq_Rate != Cur_Acq_Rate {
			Cur_Acq_Rate = New_Acq_Rate;
			acq_rate = acq_rates_list[ Cur_Acq_Rate ];
			lockin_auto_setup( 1.0 / acq_rate, 1 );

			IF Sweep_State != STOPPED {
				IF ! Pause_State {
					lockin_auto_acquisition( "OFF" );

					IF Draw_While_Stopped {
						lockin_auto_acquisition( "ON" );
						delta_time( );
						st = 0.0 s;
					}
				}

				Sweep_State = STOPPED;
				button_state( Sweep_Stop, "ON" );
				new_field = new_set_field;
				draw_marker( I + 1, "RED" );
			}
		}
	}

	/* Check if the clear curve button has been pressed - if yes also clear
	   the lock-ins internal buffer. */

	IF button_state( Clear ) {
		clear_curve( );
		clear_marker( );
		rescale( 64 );
		I = 0;
		IF ! Pause_State & ( Sweep_State != STOPPED | Draw_While_Stopped ) {
			lockin_auto_acquisition( "OFF" );
			lockin_auto_acquisition( "ON" );
			delta_time( );
			st = 0.0 s;
		}
	}

	/* Check if the "Pause" button got changed */

	IF button_changed( Pause ) {
		IF ! Pause_State {
			IF Sweep_State != STOPPED | Draw_While_Stopped {
				lockin_auto_acquisition( "OFF" );
			}
		} ELSE IF Sweep_State != STOPPED | Draw_While_Stopped {
			lockin_auto_acquisition( "ON" );
			delta_time( );
			st = 0.0 s;
		}

		Pause_State = button_state( Pause );
	}

	Draw_While_Stopped = button_state( DWS );
}


ON_STOP:

/* At the and of the experiment make sure the sweep is stopped and the
   lock-ins auto-acquisition gets stopped. */

IF Sweep_State != STOPPED {
	magnet_sweep( STOPPED );
	lockin_auto_acquisition( "OFF" );
}
