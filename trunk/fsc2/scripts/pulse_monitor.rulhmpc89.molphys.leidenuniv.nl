#!/usr/bin/perl
# -*- cperl -*-
# Generated by fsc2_guify from pulse_monitor_w.EDL on Wed Jul  4 10:26:34 CEST 2007

use strict;
use warnings;
use Tk;
use Tk::Balloon;

my @version = split /\./, $Tk::VERSION;
die "Installed Perl-Tk version is $Tk::VERSION but Tk800.022 is required.\n"
    if $version[ 0 ] + 0.001 * $version[ 1 ] < 800.022;

my %fp = ( -side => 'top',
           -fill => 'x',
           -padx => '2m',
           -pady => '2m' );
my %wp = ( -side => 'left',
           -fill => 'x',
           -expand => 1 );
my %up = ( -side => 'left' );
my $geom;

my $fsc2_how_to_run = "Test program";
my @fsc2_how_to_run = ( "Start experiment",
                        "Test program",
                        "Load into fsc2" );
my $fsc2_main_window = MainWindow->new( -title =>
                               ( split /\./, ( split /\//, $0 )[ -1 ] )[ 0 ] );
my $fsc2_main_frame = $fsc2_main_window->Frame( -relief => "ridge",
                                                -borderwidth => "1m" );
my $fsc2_balloon = $fsc2_main_frame->Balloon( );
my $fsc2_apply_frame = $fsc2_main_window->Frame( );
my $fsc2_apply_button = $fsc2_apply_frame->Button( -text => "Apply",
                                                   -command => \&write_out );
$fsc2_apply_button->bind( "all", "<Alt-a>" => \&write_out );
my $fsc2_quit_button = $fsc2_apply_frame->Button( -text => "Quit",
                 -command => sub { $fsc2_main_window->geometry =~
                                                   /^\d+x\d+([+-]\d+[+-]\d+)$/;
                                   $geom = $1;
                                   &store_defs;
                                   $fsc2_main_window->destroy } );
$fsc2_quit_button->bind( "all",
                         "<Alt-q>" =>
                                sub { $fsc2_main_window->geometry =~
                                                   /^\d+x\d+([+-]\d+[+-]\d+)$/;
                                      $geom = $1;
                                      &store_defs;
                                      $fsc2_main_window->destroy } );
$fsc2_apply_frame->pack( -side => "bottom",
                         -fill => "x",
                         -padx => "4m" );

# === CREATE_DEFENSE button [ ON ] "Create defense pulse"

my %CREATE_DEFENSE;
$CREATE_DEFENSE{ tk_frame } = $fsc2_main_frame->Frame( );
$CREATE_DEFENSE{ tk_label } = $CREATE_DEFENSE{ tk_frame }->Label( -text => "Create defense pulse",
-width => 20,
-anchor => 'w' );
$CREATE_DEFENSE{ value } = 1;
$CREATE_DEFENSE{ tk_entry } = $CREATE_DEFENSE{ tk_frame }->Checkbutton( -variable => \$CREATE_DEFENSE{ value },
-width => 10 );
$CREATE_DEFENSE{ tk_unit } = $CREATE_DEFENSE{ tk_frame }->Label( -text => "",
-width => 5 );
$CREATE_DEFENSE{ tk_frame }->pack( %fp );
$CREATE_DEFENSE{ tk_label }->pack( %wp );
$CREATE_DEFENSE{ tk_entry }->pack( %wp );
$CREATE_DEFENSE{ tk_unit  }->pack( %up );

# === PULSE_2_DEFENSE float [ 0 : ] [ 50 ] "End of MW pulse to\nend of defense\npulse distance" "ns"

my %PULSE_2_DEFENSE;
$PULSE_2_DEFENSE{ tk_frame } = $fsc2_main_frame->Frame( );
$PULSE_2_DEFENSE{ tk_label } = $PULSE_2_DEFENSE{ tk_frame }->Label( -text => "End of MW pulse to\nend of defense\npulse distance",
-width => 20,
-anchor => 'w' );
$PULSE_2_DEFENSE{ value } = 50;
$PULSE_2_DEFENSE{ min } = 0;
$PULSE_2_DEFENSE{ max } = undef;
$PULSE_2_DEFENSE{ tk_entry } = $PULSE_2_DEFENSE{ tk_frame }->Entry( -textvariable => \$PULSE_2_DEFENSE{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $PULSE_2_DEFENSE{ min } ? $PULSE_2_DEFENSE{ min } : undef ),
( defined $PULSE_2_DEFENSE{ max } ? $PULSE_2_DEFENSE{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $PULSE_2_DEFENSE{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $PULSE_2_DEFENSE{ min } ? $PULSE_2_DEFENSE{ min } : '-inf' ) .
" : " . ( defined $PULSE_2_DEFENSE{ max } ? $PULSE_2_DEFENSE{ max } : '+inf' ) . " ]" );
$PULSE_2_DEFENSE{ tk_unit } = $PULSE_2_DEFENSE{ tk_frame }->Label( -text => "ns",
-width => 5 );
$PULSE_2_DEFENSE{ tk_frame }->pack( %fp );
$PULSE_2_DEFENSE{ tk_label }->pack( %wp );
$PULSE_2_DEFENSE{ tk_entry }->pack( %wp );
$PULSE_2_DEFENSE{ tk_unit  }->pack( %up );

$fsc2_main_frame->pack( %fp, -pady => '1m' );
$fsc2_main_window->Optionmenu( -options => \@fsc2_how_to_run,
                                -textvariable => \$fsc2_how_to_run,
                              )->pack( -padx => '3m',
                                       -pady => '3m' );

$fsc2_apply_button->pack( %wp, -padx => '5m', -pady => '3m' );
$fsc2_quit_button->pack(  %wp, -padx => '5m', -pady => '3m' );

load_defs( );
$fsc2_main_window->geometry( $geom ) if defined $geom;
MainLoop;


################################################################

sub int_check {
    my ( $new, $min, $max ) = @_;

    return 0 if $new =~ /^\+?(\d+)?$/ and defined $max and $max < 0;
    return 0 if $new =~ /^-/ and defined $min and $min >= 0;
    if ( $new =~ /^[+-]?$/ ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
         return 1;
     }

    return 0 unless $new =~ /^[+-]?\d+?$/;

    if ( ( defined $min and $new < $min )
         or ( defined $max and $new > $max ) ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
     } else {
         $fsc2_apply_button->configure( -state => 'normal' );
     }
     return 1;
}


################################################################

sub float_check {
    my ( $new, $min, $max ) = @_;
    my $float_rep = '[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?';

    return 0 if $new =~ /^\+/ and defined $max and $max < 0;
    return 0 if $new =~ /^-/ and defined $min and $min > 0;

    if ( $new =~ /^$float_rep$/ ) {
        if ( ( defined $max and $max < $new ) or
             ( defined $min and $min > $new ) ) {
            $fsc2_apply_button->configure( -state => 'disabled' );
        } else {
            $fsc2_apply_button->configure( -state => 'normal' );
        }
        return 1;
    }

    if ( $new =~ /^[+-]?(\d+)?\.?(\d+)?([Ee][+-]?(\d+)?)?$/ ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
         return 1;
     }

     return 0;
}


################################################################

sub write_out {
    my $fh;

    open( $fh, "|fsc2_" . lc $fsc2_how_to_run )
        or die "Can't find utility fsc2_" . lc $fsc2_how_to_run . ".\n";

    my $CREATE_DEFENSE = $CREATE_DEFENSE{ value };
    my $PULSE_2_DEFENSE = $PULSE_2_DEFENSE{ value };

    print $fh "/* \$Id: pulse_monitor_w.EDL 8013 2007-01-02 00:26:16Z jens \$ */


DEVICES:

rs_sml01;
rb_pulser_w;
rb8509;
ips120_10;


ASSIGNMENTS:

TRIGGER_MODE: INTERNAL, REPEAT_TIME = 10 ms;

PHASES:

PHASE_SEQUENCE_1: +X, +X, +X, +X, +X, +X, +X, +X, +X,
                  +Y, +Y, +Y, +Y, +Y, +Y, +Y, +Y, +Y,
                  -X, -X, -X, -X, -X, -X, -X, -X, -X;

PHASE_SEQUENCE_2: +X, +X, +X, +Y, +Y, +Y, -X, -X, -X,
                  +X, +X, +X, +Y, +Y, +Y, -X, -X, -X,
				  +X, +X, +X, +Y, +Y, +Y, -X, -X, -X;

PHASE_SEQUENCE_3: +X, +Y, -X, +X, +Y, -X, +X, +Y, -X,
				  +X, +Y, -X, +X, +Y, -X, +X, +Y, -X,
				  +X, +Y, -X, +X, +Y, -X, +X, +Y, -X;

VARIABLES:

current_field;
new_field;
new_set_field;
sweep_rate = 1.0;
new_sweep_rate;
freq;
new_freq;
Sweep_State = 0;
att;
new_att;


PS[ 5 ];            /* pulse start position input fields */
PL[ 5 ];            /* pulse length input fields */
PSV[ 5 ];           /* start positions of pulses (in units of 10 ns) */
PLV[ 5 ];           /* pulse lengths (in units of 10 ns) */
PMS[ 5 ];           /* minimum values for pulse positions (in 10 ns units) */
Phase, Cur_Phase = 1, New_Phase;
ADC_Channel;
Freq, Current_Field, New_Field, RF_ON_OFF, Att, Sweep_Rate, Sweep_Up,
Sweep_Stop, Sweep_Down, ADC_Channels, Pause, Clear;
Acq_Rate, New_Acq_Rate;
Pause_Display = 1;
STOPPED = 0;
UP = 1;
DOWN = -1;
I = 0;
J;
NP, NL;
np, nl;
MAX_LEN = 16777215;     /* max. delay (in ticks) */


PREPARATIONS:

";
# === if ! CREATE_DEFENSE
    if ( eval { ! $CREATE_DEFENSE } ) {
        print $fh "pulser_defense_pulse_mode( \"OFF\" );
";
# === else
    } else {
        print $fh "pulser_minimum_defense_distance( $PULSE_2_DEFENSE ns );
";
# === endif
    }

    print $fh "
P1:   FUNCTION    = MW,
      START       = 200 ns,
      LENGTH      = 60 ns,
	  PHASE_CYCLE = PHASE_SEQUENCE_1;

P2:   FUNCTION    = MW,
      START       = 460 ns,
      LENGTH      = 60 ns,
	  PHASE_CYCLE = PHASE_SEQUENCE_2;

P3:   FUNCTION    = MW,
      START       = 760 ns,
      LENGTH      = 100 ns,
	  PHASE_CYCLE = PHASE_SEQUENCE_3;

P4:   FUNCTION = RF,
      START    = 380 ns,
      LENGTH   = 100 ns;

P5:   FUNCTION = DETECTION,
      START    = 310 ns,
      LENGTH   = 10 ns;

init_1d( 2, \"Points\", \"Signal\" );


EXPERIMENT:

/* Stop the magnet if it's sweeping */

IF magnet_sweep( ) != STOPPED {
    magnet_sweep( STOPPED );
}

/* Make sure the pulser is running */

pulser_state( \"ON\" );

/* Get the current field value and the synthesizer settings */

current_field = magnet_field( );
new_field = current_field;
att = synthesizer_attenuation( );
freq = synthesizer_frequency( );

/* Create the toolbox */

hide_toolbox( \"ON\" );
PSV[ 1 ] = round( P1.START / 10 ns );
PS[ 1 ] = input_create( \"INT_INPUT\", PSV[ 1 ] * 10, \"Position of MW pulse 1\" );
PLV[ 1 ] = round( P1.LENGTH / 10 ns );
PL[ 1 ] = input_create( \"INT_INPUT\", PLV[ 1 ] * 10, \"Length of MW pulse 1\" );

PSV[ 2 ] = round( P2.START / 10 ns );
PS[ 2 ] = input_create( \"INT_INPUT\", PSV[ 2] * 10, \"Position MW of pulse 2\" );
PLV[ 2 ] = round( P2.LENGTH / 10 ns );
PL[ 2 ] = input_create( \"INT_INPUT\", PLV[ 2 ] * 10, \"Length of MW pulse 2\" );

PSV[ 3 ] = round( P3.START / 10 ns );
PS[ 3 ] = input_create( \"INT_INPUT\", PSV[ 3 ] * 10, \"Position of MW pulse 3\" );
PLV[ 3 ] = round( P3.LENGTH / 10 ns );
PL[ 3 ] = input_create( \"INT_INPUT\", PLV[ 3 ] * 10, \"Length of MW pulse 3\" );

PSV[ 4 ] = round( P4.START / 10 ns );
PS[ 4 ] = input_create( \"INT_INPUT\", PSV[ 4 ] * 10, \"Position of RF pulse\" );
PLV[ 4 ] = round( P4.LENGTH / 10 ns );
PL[ 4 ] = input_create( \"INT_INPUT\", PLV[ 4 ] * 10, \"Length of RF pulse\" );

PSV[ 5 ] = round( P5.START / 10 ns );
PS[ 5 ] = input_create( \"INT_INPUT\", PSV[ 5 ] * 10, \"Position of DET pulse\" );
PLV[ 5 ] = round( P5.LENGTH / 10 ns );
PL[ 5 ] = input_create( \"INT_INPUT\", PLV[ 5 ] * 10, \"Length of DET pulse\" );

Phase = menu_create( \"Phase settings\",
                     \"+X  +X  +X\", \"+X  +X  +Y\", \"+X  +X  -X\",
                     \"+X  +Y  +X\", \"+X  +Y  +Y\", \"+X  +Y  -X\",
                     \"+X  -X  +X\", \"+X  -X  +Y\", \"+X  -X  -X\",
                     \"+Y  +X  +X\", \"+Y  +X  +Y\", \"+Y  +X  -X\",
                     \"+Y  +Y  +X\", \"+Y  +Y  +Y\", \"+Y  +Y  -X\",
                     \"+Y  -X  +X\", \"+Y  -X  +Y\", \"+Y  -X  -X\",
                     \"-X  +X  +X\", \"-X  +X  +Y\", \"-X  +X  -X\",
                     \"-X  +Y  +X\", \"-X  +Y  +Y\", \"-X  +Y  -X\",
                     \"-X  -X  +X\", \"-X  -X  +Y\", \"-X  -X  -X\" );

Freq    = input_create( \"FLOAT_INPUT\", freq / 1 MHz, \"RF frequency (MHz)\" );
Current_Field = output_create( \"FLOAT_OUTPUT\", current_field,
                               \"Current field [G]\" );
Att = input_create( \"FLOAT_INPUT\", att, \"RF attenuation (dB)\" );
RF_ON_OFF    = button_create( \"PUSH_BUTTON\", \"RF On/Off\" );
button_state( RF_ON_OFF, synthesizer_state( ) );
New_Field    = input_create( \"FLOAT_INPUT\", current_field, \"New field [G]\" );
Sweep_Rate   = input_create( \"FLOAT_INPUT\", sweep_rate, \"Sweep rate [G/s]\" );
Sweep_Up     = button_create( \"RADIO_BUTTON\", \"Sweep up\" );
Sweep_Stop   = button_create( \"RADIO_BUTTON\", Sweep_Up, \"Stop sweep\" );
Sweep_Down   = button_create( \"RADIO_BUTTON\", Sweep_Up, \"Sweep Down\" );
ADC_Channels = menu_create( \"ADC channel(s) to use\", \"0\", \"1\", \"0 and 1\" );
Pause        = button_create( \"PUSH_BUTTON\", \"Pause display\" );
Clear        = button_create( \"NORMAL_BUTTON\", \"Clear screen\" );
button_state( Sweep_Stop, \"ON\" );
button_state( Pause, \"ON\" );
hide_toolbox( \"OFF\" );

PMS[ 1 ] = round( pulser_pulse_minimum_specs( P1 ) / 10 ns );
PMS[ 2 ] = round( pulser_pulse_minimum_specs( P2 ) / 10 ns );
PMS[ 3 ] = round( pulser_pulse_minimum_specs( P3 ) / 10 ns );

synthesizer_frequency( freq );


FOREVER {

    /* Most of the following only needs to be run when the user changed
       something in the toolbox... */

    IF toolbox_changed( ) {

        /* The first part of the loop is dealing with the pulse settings. It
           gets a bit complicated because we have to check the new settings
	       for each pulse so it won't make the pulser kill the experiment
	       because a pulse can't be set to the requested value... */

        /* Checks for first MW pulse */

        IF input_changed( PS[ 1 ] ) {
            NP = input_value( PS[ 1 ] );
            IF NP % 10 != 0 OR NP < 0 {
                input_value( PS[ 1 ], PSV[ 1 ] * 10 );
            } ELSE {
                NP /= 10;
                IF NP < PMS[ 1 ] OR NP + PLV[ 1 ] + PMS[ 2 ] >= PSV[ 2 ] {
                    input_value( PS[ 1 ], PSV[ 1 ] * 10 );
                } ELSE {
                    PMS[ 4 ] = round( pulser_pulse_minimum_specs( P4 )
                                      / 10 ns ) - PSV[ 1 ] + NP;
                    PMS[ 5 ] = round( pulser_pulse_minimum_specs( P5 )
                                      / 10 ns ) - PSV[ 1 ] + NP;
                    PSV[ 1 ] = NP;
                    P1.START = NP * 10 ns;

                    IF PSV[ 4 ] < PMS[ 4 ] {
                        PSV[ 4 ] = PMS[ 4 ];
                        P4.START = PMS[ 4 ] * 10 ns;
                        input_value( PS[ 4 ], PSV[ 4 ] * 10 );
                    }

                    IF PSV[ 5 ] < PMS[ 5 ] {
                        PSV[ 5 ] = PMS[ 5 ];
                        P5.START = PMS[ 5 ] * 10 ns;
                        input_value( PS[ 5 ], PSV[ 5 ] * 10 );
                    }

                    pulser_update( );
                }
            }
        }

        IF input_changed( PL[ 1 ] ) {
            NL = input_value( PL[ 1 ] );
            IF NL % 10 != 0 OR NL <= 0 {
                input_value( PL[ 1 ], PLV[ 1 ] * 10 );
            } ELSE {
                NL /= 10;
                IF PSV[ 1 ] + NL + PMS[ 2 ] >= PSV[ 2 ] {
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
            IF NP % 10 != 0 OR NP < 0 {
                input_value( PS[ 2 ], PSV[ 2 ] * 10 );
            } ELSE {
                NP /= 10;
                IF NP < PSV[ 1 ] + PLV[ 1 ] + PMS[ 2 ] OR
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
            IF NL % 10 != 0 OR NL < 0 OR ( NL == 0 AND PLV[ 3 ] != 0 ) {
                input_value( PL[ 2 ], PLV[ 2 ] * 10 );
            } ELSE {
                NL /= 10;
                IF PSV[ 2 ] + NL + PMS[ 3 ] >= PSV[ 3 ] {
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
            IF NP % 10 != 0 OR NP < 0 {
                input_value( PS[ 3 ], PSV[ 3 ] * 10 );
            } ELSE {
                NP /= 10;
                IF NP < PSV[ 2 ] + PLV[ 2 ] + PMS[ 3 ] OR
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
            IF NL % 10 != 0 OR NL < 0 {
                input_value( PL[ 3 ], PLV[ 3 ] * 10 );
            } ELSE {
                NL /= 10;
                IF PSV[ 3 ] + NL > MAX_LEN {
                    input_value( PL[ 3 ], PLV[ 3 ] * 10 );
                } ELSE {
                    PLV[ 3 ] = NL;
                    P3.LENGTH = NL * 10 ns;
                    pulser_update( );
                }
            }
        }

		/* Check for change of phase settings */

		IF menu_changed( Phase ) {
		    New_Phase = menu_choice( Phase );
		    IF New_Phase > Cur_Phase {
		        FOR J = 1 : New_Phase - Cur_Phase {
				    pulser_next_phase( );
			    }
		    } ELSE IF New_Phase < Cur_Phase {
			    pulser_phase_reset( );
			    FOR J = 1 : New_Phase - 1 {
				    pulser_next_phase( );
			    }
            }
			Cur_Phase = New_Phase;
		}

        /* Checks for the RF pulse */

        IF input_changed( PS[ 4 ] ) {
            NP = input_value( PS[ 4 ] );
            IF NP % 10 != 0 OR NP < 0 {
                input_value( PS[ 4 ], PSV[ 4 ] * 10 );
            } ELSE {
                NP /= 10;
                PMS[ 4 ] = round( pulser_pulse_minimum_specs( P4 ) / 10 ns );
                IF NP < PMS[ 4 ] OR NP > MAX_LEN {
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
            IF NL % 10 != 0 OR NL < 0 {
                input_value( PL[ 4 ], PLV[ 4 ] * 10 );
            } ELSE {
                NL /= 10;
                IF NL > MAX_LEN {
                    input_value( PL[ 4 ], PLV[ 4 ] * 10 );
                } ELSE {
                    PLV[ 4 ] = NL;
                    P4.LENGTH = NL * 10 ns;
                    pulser_update( );
                }
            }
        }

        /* Checks for the detection pulse */

        IF input_changed( PS[ 5 ] ) {
            NP = input_value( PS[ 5 ] );
            IF NP % 10 != 0 OR NP < 0 {
                input_value( PS[ 5 ], PSV[ 5 ] * 10 );
            } ELSE {
                NP /= 10;
                PMS[ 5 ] = round( pulser_pulse_minimum_specs( P5 ) / 10 ns );
                IF NP < PMS[ 5 ] OR NP + PLV[ 5 ] > MAX_LEN {
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
            IF NL % 10 != 0 OR NL < 0 {
                input_value( PL[ 5 ], PLV[ 5 ] * 10 );
            } ELSE {
                NL /= 10;
                IF PSV[ 5 ] + NL > MAX_LEN {
                    input_value( PL[ 5 ], PLV[ 5 ] * 10 );
                } ELSE {
                    PLV[ 5 ] = NL;
                    P5.LENGTH = NL * 10 ns;
                    pulser_update( );
                }
            }
        }

        /* Deal with RF frequency changes */

        IF input_changed( Freq ) {
            new_freq = input_value( Freq ) * 1 MHz;
            IF new_freq < 9 kHz OR new_freq > 1100 MHz {
                input_value( Freq, freq );
            } ELSE {
                freq = new_freq;
                synthesizer_frequency( freq );
            }
        }

        /* Deal with RF attenuation changes */

        IF input_changed( Att ) {
            new_att = input_value( Att );
            IF new_att < -140 dB OR new_att > 13 dB {
                input_value( Att, att );
            } ELSE {
                att = new_att;
                synthesizer_attenuation( att );
            }
        }

        /* Button for switching RF power on or off */

        IF button_changed( RF_ON_OFF ) {
            synthesizer_state( button_state( RF_ON_OFF ) );
        }

        /* Check if the sweep up button has been pressed and start sweeping
           up (unless we're already sweeping up) */

        IF  Sweep_State != UP AND button_state( Sweep_Up ) {
            IF Sweep_State == STOPPED {
                magnet_sweep( UP );
                Sweep_State = UP;
            }

            IF Sweep_State == DOWN {
                magnet_sweep( UP );
                Sweep_State = UP;
            }

            draw_marker( I + 1, \"YELLOW\" );
            button_state( Pause, \"OFF\" );
        }

        /* Check if the sweep down button has been pressed and start sweeping
           down (unless we're already sweeping down) */

        IF Sweep_State != DOWN AND button_state( Sweep_Down ) {
            IF Sweep_State == STOPPED {
                magnet_sweep( DOWN );
                Sweep_State = DOWN;
            }

            IF Sweep_State == UP {
                magnet_sweep( DOWN );
                Sweep_State = DOWN;
            }

            draw_marker( I + 1, \"BLUE\" );
            button_state( Pause, \"OFF\" );
        }

        /* Check if the sweep stop button has been pressed while we're
           sweeping */

        IF Sweep_State != STOPPED AND button_state( Sweep_Stop ) {
            magnet_sweep( STOPPED );
            Sweep_State = STOPPED;
            draw_marker( I + 1, \"RED\" );
        }

        /* Check if a new field has been set - if yes go to the new field
           (which automatically stops a sweep) after checking that it's
	       within the allowed limits */

        new_set_field = input_value( New_Field );
        IF abs( new_set_field - new_field ) >= 0.04152 G {
            IF new_set_field > 48370.8 G OR new_set_field < 0.0 G {
                input_value( New_Field, new_field );
            } ELSE {
                Sweep_State = STOPPED;
                new_field = new_set_field;
                button_state( Sweep_Stop, \"ON\" );
                current_field = magnet_field( new_field );
                draw_marker( I + 1, \"RED\" );
            }
            output_value( Current_Field, current_field );
        }

        /* Check if a new sweep rate has been set - if yes set the new rate
           after checking that it's within the allowed limits */

        new_sweep_rate = abs( input_value( Sweep_Rate ) );
        IF abs( sweep_rate - new_sweep_rate ) > 0.01 {
            IF new_sweep_rate <= 33.1 AND new_sweep_rate >= 0.23814 {
                sweep_rate = magnet_sweep_rate( new_sweep_rate );
                IF Sweep_State != STOPPED {
                    draw_marker( I + 1, \"GREEN\" )
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

    /* The final part of the loop are the things that need to be done each
	   time through the loop and deals with getting data from the ADC (that
	   gets its input from the boxcar integrator) and updating the field
	   value display */

    /* Update the output field with the current field if necessary */

    UNLESS Sweep_State == STOPPED {
        current_field = magnet_field( );
        output_value( Current_Field, current_field );
    }

    IF ! Pause_Display {
        I += 1;
        wait( 0.2 s );
		ADC_Channel = menu_choice( ADC_Channels );
		IF ADC_Channel == 1 {
        	display( I, daq_get_voltage( CH0 ), 1 );
		} ELSE IF ADC_Channel == 2 {
        	display( I, daq_get_voltage( CH1 ), 2 );
		} ELSE {
        	display( I, daq_get_voltage( CH0 ), 1,
					 I, daq_get_voltage( CH1 ), 2 );
		}	 
    } ELSE {
        wait( 0.2 s );
    }
}


ON_STOP:

/* At the and of the experiment make sure the sweep is stopped */

IF Sweep_State != STOPPED {
    magnet_sweep( STOPPED );
}

/* Write out to (standord output) the current pulse settings */

FOR I = 1 : 5 {
	fsave( 1, \"# #\\n\", PSV[ I ] * 10, PLV[ I ] * 10 );
}
";
    close $fh;

    my $text;
    if ( $? != 0 ) {
        if ( $? >> 8 == 255 ) {
            $text = "Internal error.";
        } elsif ( $? >> 8 == 1 ) {
            $text = "Someone else is running fsc2.";
        } elsif ( $? >> 8 == 2 ) {
            $text = "fsc2 is already testing or\nrunning an experiment.";
        } elsif ( $? >> 8 == 3 ) {
            $text = "Internal error of fsc2.";
        } elsif ( $? >> 8 == 4 ) {
            $text = "Could not start fsc2.";
        } else {
            $text = "Something strange\nis going on here.";
        }

        &show_message( $text ) if $? >> 8 != 0;
    }
}


################################################################

sub show_message {
    my $text = shift;

    $fsc2_main_window->messageBox( -icon => 'error',
                                   -type => 'Ok',
                                   -title => 'Error',
                                   -message => $text );
}


################################################################

sub store_defs {
    my $fh;
    my $name = $0;

    $name =~ s|^.*?([^/]+)$|$1|;
    mkdir "$ENV{ HOME }/.fsc2", 0777 unless -e "$ENV{ HOME }/.fsc2";
    open( $fh, ">$ENV{ HOME }/.fsc2/$name" ) or return;
    print $fh "# Do not edit - created automatically!\n";

    print $fh "$CREATE_DEFENSE{ value }\n";

    if ( $PULSE_2_DEFENSE{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $PULSE_2_DEFENSE{ max } ? $PULSE_2_DEFENSE{ max } >= $PULSE_2_DEFENSE{ value } : 1 ) and
         ( defined $PULSE_2_DEFENSE{ min } ? $PULSE_2_DEFENSE{ min } <= $PULSE_2_DEFENSE{ value } : 1 ) ) {
        print $fh "$PULSE_2_DEFENSE{ value }\n";
    } else {
        print $fh "50\n";
    }

    print $fh "$fsc2_how_to_run\n";

    print $fh "$geom\n" if defined $geom;

    close $fh;
};


################################################################

sub load_defs {
    my $fh;
    my $name = $0;
    my $ne;
    my $found;

    $name =~ s|^.*?([^/]+)$|$1|;
    if ( $ARGV[ 0 ] ) {
        open( $fh, "<$ARGV[ 0 ]" ) or return;
    } else {
        open( $fh, "<$ENV{ HOME }/.fsc2/$name" ) or return;
    }

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^#/;

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^1|0$/o;
    chomp $ne;
    $CREATE_DEFENSE{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $PULSE_2_DEFENSE{ max } and $ne > $PULSE_2_DEFENSE{ max } ) or
                         ( defined $PULSE_2_DEFENSE{ min } and $ne < $PULSE_2_DEFENSE{ min } );
    $PULSE_2_DEFENSE{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @fsc2_how_to_run ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $fsc2_how_to_run = $ne;

    goto done_reading unless defined( $ne = <$fh> )
                             and $ne =~ /^\s*([+-]\d+[+-]\d+)\s*$/;
    $geom = $1;

  done_reading:
    close $fh;
};
