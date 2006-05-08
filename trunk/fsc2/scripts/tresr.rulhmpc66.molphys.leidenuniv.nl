#!/usr/bin/perl
# -*- cperl -*-
# Generated by fsc2_guify from tresr_j.EDL on Sun May  7 12:00:45 CEST 2006

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

# === START_FIELD float [ 0 : 114304 ] [ 80000 ] "Start field" "G"

my %START_FIELD;
$START_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$START_FIELD{ tk_label } = $START_FIELD{ tk_frame }->Label( -text => "Start field",
-width => 20,
-anchor => 'w' );
$START_FIELD{ value } = 80000;
$START_FIELD{ min } = 0;
$START_FIELD{ max } = 114304;
$START_FIELD{ tk_entry } = $START_FIELD{ tk_frame }->Entry( -textvariable => \$START_FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $START_FIELD{ min } ? $START_FIELD{ min } : undef ),
( defined $START_FIELD{ max } ? $START_FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $START_FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $START_FIELD{ min } ? $START_FIELD{ min } : '-inf' ) .
" : " . ( defined $START_FIELD{ max } ? $START_FIELD{ max } : '+inf' ) . " ]" );
$START_FIELD{ tk_unit } = $START_FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$START_FIELD{ tk_frame }->pack( %fp );
$START_FIELD{ tk_label }->pack( %wp );
$START_FIELD{ tk_entry }->pack( %wp );
$START_FIELD{ tk_unit  }->pack( %up );

# === END_FIELD float [ 0 : 114304 ] [ 80500 ] "End field" "G"

my %END_FIELD;
$END_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$END_FIELD{ tk_label } = $END_FIELD{ tk_frame }->Label( -text => "End field",
-width => 20,
-anchor => 'w' );
$END_FIELD{ value } = 80500;
$END_FIELD{ min } = 0;
$END_FIELD{ max } = 114304;
$END_FIELD{ tk_entry } = $END_FIELD{ tk_frame }->Entry( -textvariable => \$END_FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $END_FIELD{ min } ? $END_FIELD{ min } : undef ),
( defined $END_FIELD{ max } ? $END_FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $END_FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $END_FIELD{ min } ? $END_FIELD{ min } : '-inf' ) .
" : " . ( defined $END_FIELD{ max } ? $END_FIELD{ max } : '+inf' ) . " ]" );
$END_FIELD{ tk_unit } = $END_FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$END_FIELD{ tk_frame }->pack( %fp );
$END_FIELD{ tk_label }->pack( %wp );
$END_FIELD{ tk_entry }->pack( %wp );
$END_FIELD{ tk_unit  }->pack( %up );

# === FIELD_STEP float [ 0.14288 :  ] [ 1 ] "Field step" "G"

my %FIELD_STEP;
$FIELD_STEP{ tk_frame } = $fsc2_main_frame->Frame( );
$FIELD_STEP{ tk_label } = $FIELD_STEP{ tk_frame }->Label( -text => "Field step",
-width => 20,
-anchor => 'w' );
$FIELD_STEP{ value } = 1;
$FIELD_STEP{ min } = 0.14288;
$FIELD_STEP{ max } = undef;
$FIELD_STEP{ tk_entry } = $FIELD_STEP{ tk_frame }->Entry( -textvariable => \$FIELD_STEP{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } : undef ),
( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $FIELD_STEP{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } : '-inf' ) .
" : " . ( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } : '+inf' ) . " ]" );
$FIELD_STEP{ tk_unit } = $FIELD_STEP{ tk_frame }->Label( -text => "G",
-width => 5 );
$FIELD_STEP{ tk_frame }->pack( %fp );
$FIELD_STEP{ tk_label }->pack( %wp );
$FIELD_STEP{ tk_entry }->pack( %wp );
$FIELD_STEP{ tk_unit  }->pack( %up );

# === TRACE_LEN menu [ "500", "1000", "2500", "5000", "10000", "25000",  "50000", "100000" ] [ 2 ] "Trace length" "pts"

my %TRACE_LEN;
$TRACE_LEN{ tk_frame } = $fsc2_main_frame->Frame( );
$TRACE_LEN{ tk_label } = $TRACE_LEN{ tk_frame }->Label( -text => "Trace length",
-width => 20,
-anchor => 'w' );
$TRACE_LEN{ value } = "1000";
my @TRACE_LEN = ( "500", "1000", "2500", "5000", "10000", "25000", "50000", "100000" );
$TRACE_LEN{ tk_entry } = $TRACE_LEN{ tk_frame }->Optionmenu( -options => \@TRACE_LEN,
-width => 10,
-textvariable => \$TRACE_LEN{ value } );
$TRACE_LEN{ tk_unit } = $TRACE_LEN{ tk_frame }->Label( -text => "pts",
-width => 5 );
$TRACE_LEN{ tk_frame }->pack( %fp );
$TRACE_LEN{ tk_label }->pack( %wp );
$TRACE_LEN{ tk_entry }->pack( %wp );
$TRACE_LEN{ tk_unit  }->pack( %up );

# === SIGNAL_FIELD float [ 0 : 114304 ] [ 80050 ]  "Signal start field\n(for laser correction)" "G"

my %SIGNAL_FIELD;
$SIGNAL_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$SIGNAL_FIELD{ tk_label } = $SIGNAL_FIELD{ tk_frame }->Label( -text => "Signal start field\n(for laser correction)",
-width => 20,
-anchor => 'w' );
$SIGNAL_FIELD{ value } = 80050;
$SIGNAL_FIELD{ min } = 0;
$SIGNAL_FIELD{ max } = 114304;
$SIGNAL_FIELD{ tk_entry } = $SIGNAL_FIELD{ tk_frame }->Entry( -textvariable => \$SIGNAL_FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $SIGNAL_FIELD{ min } ? $SIGNAL_FIELD{ min } : undef ),
( defined $SIGNAL_FIELD{ max } ? $SIGNAL_FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $SIGNAL_FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $SIGNAL_FIELD{ min } ? $SIGNAL_FIELD{ min } : '-inf' ) .
" : " . ( defined $SIGNAL_FIELD{ max } ? $SIGNAL_FIELD{ max } : '+inf' ) . " ]" );
$SIGNAL_FIELD{ tk_unit } = $SIGNAL_FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$SIGNAL_FIELD{ tk_frame }->pack( %fp );
$SIGNAL_FIELD{ tk_label }->pack( %wp );
$SIGNAL_FIELD{ tk_entry }->pack( %wp );
$SIGNAL_FIELD{ tk_unit  }->pack( %up );

# === PRETRIGGER  float [ 0.0 : 100.0 ] [ 10.0 ] "Pre-trigger" "in %"

my %PRETRIGGER;
$PRETRIGGER{ tk_frame } = $fsc2_main_frame->Frame( );
$PRETRIGGER{ tk_label } = $PRETRIGGER{ tk_frame }->Label( -text => "Pre-trigger",
-width => 20,
-anchor => 'w' );
$PRETRIGGER{ value } = 10.0;
$PRETRIGGER{ min } = 0.0;
$PRETRIGGER{ max } = 100.0;
$PRETRIGGER{ tk_entry } = $PRETRIGGER{ tk_frame }->Entry( -textvariable => \$PRETRIGGER{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $PRETRIGGER{ min } ? $PRETRIGGER{ min } : undef ),
( defined $PRETRIGGER{ max } ? $PRETRIGGER{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $PRETRIGGER{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $PRETRIGGER{ min } ? $PRETRIGGER{ min } : '-inf' ) .
" : " . ( defined $PRETRIGGER{ max } ? $PRETRIGGER{ max } : '+inf' ) . " ]" );
$PRETRIGGER{ tk_unit } = $PRETRIGGER{ tk_frame }->Label( -text => "in %",
-width => 5 );
$PRETRIGGER{ tk_frame }->pack( %fp );
$PRETRIGGER{ tk_label }->pack( %wp );
$PRETRIGGER{ tk_entry }->pack( %wp );
$PRETRIGGER{ tk_unit  }->pack( %up );

# === NUM_AVG     int [ 1 : 1000000 ] [ 10 ] "Number of averages"

my %NUM_AVG;
$NUM_AVG{ tk_frame } = $fsc2_main_frame->Frame( );
$NUM_AVG{ tk_label } = $NUM_AVG{ tk_frame }->Label( -text => "Number of averages",
-width => 20,
-anchor => 'w' );
$NUM_AVG{ value } = 10;
$NUM_AVG{ min } = 1;
$NUM_AVG{ max } = 1000000;
$NUM_AVG{ tk_entry } = $NUM_AVG{ tk_frame }->Entry( -textvariable => \$NUM_AVG{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $NUM_AVG{ min } ? $NUM_AVG{ min } : undef ),
( defined $NUM_AVG{ max } ? $NUM_AVG{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $NUM_AVG{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $NUM_AVG{ min } ? $NUM_AVG{ min } : '-inf' ) .
" : " . ( defined $NUM_AVG{ max } ? $NUM_AVG{ max } : '+inf' ) . " ]" );
$NUM_AVG{ tk_unit } = $NUM_AVG{ tk_frame }->Label( -text => "",
-width => 5 );
$NUM_AVG{ tk_frame }->pack( %fp );
$NUM_AVG{ tk_label }->pack( %wp );
$NUM_AVG{ tk_entry }->pack( %wp );
$NUM_AVG{ tk_unit  }->pack( %up );

# === ACQ_CH menu [ "CH1", "CH2" ] [ 1 ] "Acquisition channel"

my %ACQ_CH;
$ACQ_CH{ tk_frame } = $fsc2_main_frame->Frame( );
$ACQ_CH{ tk_label } = $ACQ_CH{ tk_frame }->Label( -text => "Acquisition channel",
-width => 20,
-anchor => 'w' );
$ACQ_CH{ value } = "CH1";
my @ACQ_CH = ( "CH1", "CH2" );
$ACQ_CH{ tk_entry } = $ACQ_CH{ tk_frame }->Optionmenu( -options => \@ACQ_CH,
-width => 10,
-textvariable => \$ACQ_CH{ value } );
$ACQ_CH{ tk_unit } = $ACQ_CH{ tk_frame }->Label( -text => "",
-width => 5 );
$ACQ_CH{ tk_frame }->pack( %fp );
$ACQ_CH{ tk_label }->pack( %wp );
$ACQ_CH{ tk_entry }->pack( %wp );
$ACQ_CH{ tk_unit  }->pack( %up );

# === AVG_CH menu [ "TRACE_A", "TRACE_B", "TRACE_C", "TRACE_D" ] [ 1 ]  "Averaging channel"

my %AVG_CH;
$AVG_CH{ tk_frame } = $fsc2_main_frame->Frame( );
$AVG_CH{ tk_label } = $AVG_CH{ tk_frame }->Label( -text => "Averaging channel",
-width => 20,
-anchor => 'w' );
$AVG_CH{ value } = "TRACE_A";
my @AVG_CH = ( "TRACE_A", "TRACE_B", "TRACE_C", "TRACE_D" );
$AVG_CH{ tk_entry } = $AVG_CH{ tk_frame }->Optionmenu( -options => \@AVG_CH,
-width => 10,
-textvariable => \$AVG_CH{ value } );
$AVG_CH{ tk_unit } = $AVG_CH{ tk_frame }->Label( -text => "",
-width => 5 );
$AVG_CH{ tk_frame }->pack( %fp );
$AVG_CH{ tk_label }->pack( %wp );
$AVG_CH{ tk_entry }->pack( %wp );
$AVG_CH{ tk_unit  }->pack( %up );

$fsc2_main_frame->pack( %fp, -pady => '1m' );
$fsc2_main_window->Optionmenu( -options => \@fsc2_how_to_run,
                                -textvariable => \$fsc2_how_to_run,
                              )->pack( -padx => '3m',
                                       -pady => '3m' );

$fsc2_apply_button->pack( %wp, padx => '5m', -pady => '3m' );
$fsc2_quit_button->pack(  %wp, padx => '5m', -pady => '3m' );

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

    my $START_FIELD = $START_FIELD{ value };
    my $END_FIELD = $END_FIELD{ value };
    my $FIELD_STEP = $FIELD_STEP{ value };
    my $TRACE_LEN = $TRACE_LEN{ value };
    my $SIGNAL_FIELD = $SIGNAL_FIELD{ value };
    my $PRETRIGGER = $PRETRIGGER{ value };
    my $NUM_AVG = $NUM_AVG{ value };
    my $ACQ_CH = $ACQ_CH{ value };
    my $AVG_CH = $AVG_CH{ value };

    print $fh "DEVICES:

ips120_10;
itc503;
lecroy_wr2;


VARIABLES:

start_field = $START_FIELD G;             // Start field
end_field   = $END_FIELD G;               // End field
field_step  = $FIELD_STEP G;              // Field step size
field = start_field;                     // Current field value
Num_Traces  = ceil( abs( start_field - end_field ) / abs( field_step ) ) + 1;
                                         // Number of field points
no_signal_max_field = $SIGNAL_FIELD G;    // Start of field region with signals
Num_No_Signal_Traces = floor( abs( start_field - no_signal_max_field )
					   / abs( field_step ) );
                                         // No. of traces without signal

Num_Avg = $NUM_AVG;                       // Number of averages per trace

Trace_Len = $TRACE_LEN;                   // Digitizer trace length
timebase;                                // Digitizer timebase
time_res;                                // Digitizer time resolution
sens;                                    // Digitizer sensitivity
pretrigger  = 0.01 * $PRETRIGGER;         // Amount of pre-trigger (rel. to
			                             // digitizer trace length)
wait_time = 5 s;                         // Time to wait for magnet to settle
data[ Num_Traces, * ];                   // Data of single scan
mdata[ Num_Traces, * ];                  // Averaged data of all scans
mdata_g[ Num_Traces, * ];
new_mdata[ * ];                          // New (scan averaged) trace
new_mdata_g[ * ];                        // New (scan averaged and baseline
			                             // corrected) trace
background[ * ];                         // averaged laser background signal
Pretrigger_Points;                       // No. of points before trigger
ground[ Num_Traces ];                    // baseline offsets for each trace

start_temp;

K = 0;                                   // Counter for scans
I;                                       // Counter for traces
J;                                       // Misc counter
BG_Count = 0;                            // No. of averaged backgrounds
B1, B2, B3, B4;                          // Toolbox variables
File;                                    // File handle
Scans_Done = 0;                          // Number of finished scans


PREPARATIONS:

/* Initialize graphics */

init_2d( 3, 0, Num_Traces, 0, 1, start_field, field_step, \"Time [�s]\",
         \"Field [G]\", \"Signal [mV]\" );


EXPERIMENT:

/* Set up the digitizer - first set the length of the digitizers traces and
   then set up the lengths of a few arrays accordingly */

digitizer_memory_size( Trace_Len );
background = float_slice( Trace_Len );
mdata[ 1 ] = float_slice( Trace_Len );
mdata_g[ 1 ] = mdata[ 1 ];
FOR I = 2 : Num_Traces {
	mdata[ I ] = mdata[ 1 ];
	mdata_g[ I ] = mdata_g[ 1 ];
}

/* Now get the timebase and the corresponding time resolution */

timebase = digitizer_timebase( );
time_res = digitizer_time_per_point( );

/* Using the time resolution calculate and set the amount of pre-trigger
   (which has to be adjusted to 1/10 of the timebase) and determine the
   number of points before the trigger */

pretrigger = 0.1 * timebase * round( pretrigger * time_res
						             * Trace_Len * 10.0 / timebase );
digitizer_trigger_delay( pretrigger );
Pretrigger_Points = int( pretrigger / time_res );

/* Correct the x-axis for to the amount of pre-trigger and the timebase */

change_scale_2d( - pretrigger / 1 us, time_res / 1 us );

/* Figure out the digitizers sensitivity */

sens = digitizer_sensitivity( CH1 );

/* Set up trigger mode and averaging if the number of averages is larger
   than 1 */

digitizer_trigger_coupling( EXT, \"DC\" );
digitizer_trigger_channel( EXT );

IF Num_Avg > 1 {
	digitizer_averaging( $AVG_CH, $ACQ_CH, Num_Avg );
}

/* Create the toolbox with two output field, one for the current scan number
   and one for the current field as well as a push button for stopping the
   experiment after the end of a scan */

hide_toolbox( \"ON\" );
B1 = output_create( \"INT_OUTPUT\", \"Current scan\" );
B2 = output_create( \"FLOAT_OUTPUT\", \"Current field [G]\", \"%.2f\" );
B3 = output_create( \\\"FLOAT_OUTPUT\\\", \\\"Current temperature [K]\\\", \\\"%.1f\\\" );
B4 = button_create( \"PUSH_BUTTON\", \"Stop after end of scan\" );
hide_toolbox( \"OFF\" );

set_field( start_field );
IF wait_time > 1 us {
	wait( wait_time );
}

File = get_file( );

start_temp = temp_contr_temperature( );

/* Now we need two nested loops, the outer loop over the scans and the inner
   loop over the field positions */

FOREVER {

	K += 1;
	output_value( B1, K );	              // Update the scan count display

	FOR I = 1 : Num_Traces {

		output_value( B2, field );        // Update the field value display
		output_value( B3, temp_contr_temperature( ) );

		/* Start an acquisition and fetch the resulting curve, then
		   immediately go to the next field position */
		
		digitizer_start_acquisition( );
		IF Num_Avg > 1 {
			data[ I ] = digitizer_get_curve( $AVG_CH );
		} ELSE {
			data[ I ] = digitizer_get_curve( $ACQ_CH );
	    }

		field += field_step;
		set_field( field );

		/* Calculate the mean of the data points before the trigger for the
		   elimination of baseline drifts */

		IF Pretrigger_Points > 0 {
		    ground[ I ] = mean( data[ I ], 1, Pretrigger_Points );
		}

	    /* While we're still in the field region where no signal is to be
	       expected add up the trace for background correction, then draw
		   the curves. */

		new_mdata = add_to_average( mdata[ I ], data[ I ], K );
		mdata_g[ I ] = add_to_average( mdata_g[ I ],
									   data[ I ] - ground[ I ], K );

		IF I <= Num_No_Signal_Traces {
			BG_Count += 1;
			background = add_to_average( background, data[ I ] - ground[ I ],
										 BG_Count );

			display_2d( 1, I, new_mdata / 1 mV, 1,
		            	1, I, mdata_g[ I ] / 1 mV, 2 );

			FOR J = 1 : I {
				IF Pretrigger_Points > 0 {
				    new_mdata = mdata_g[ J ] - background;
				    display_2d( 1, J, new_mdata / 1 mV, 3 );
				} ELSE {
					new_mdata = mdata[ J ] - background;
				    display_2d( 1, J, new_mdata / 1 mV, 3 );
				}
			}
		}

	    /* When we're in the field region where signals are to be expected draw
    	   the raw data, the data corrected for baseline drifts and the data
		   with both baseline drift and background correction as well as the
		   fully corrected and integrated data */

		IF I > Num_No_Signal_Traces  {
			display_2d( 1, I, new_mdata / 1 mV, 1,
		    	        1, I, mdata_g[ I ] / 1 mV, 2,
		        	    1, I, ( mdata_g[ I ] - background ) / 1 mV, 3 );
		}
	}

	/* Now that we're done with the scan recalculate the new mean of the
	   data and reset the field to the start position */

	mdata = add_to_average( mdata, data, K );

	field = start_field;
	set_field( field );

	Scans_Done += 1;                      // Update the number of scans done

	IF button_state( B4 ) {               // Stop on user request
		BREAK;
	}

	field = start_field;
	set_field( start_field );
	IF wait_time > 1 us {
		wait( wait_time );
	}
}

ON_STOP:

/* Write out the experimental parameters - make sure to get it right also in
   the case that a scan was aborted while under way */

fsave( File, \"\\# Date                 : # #\\n\"
	         \"\\# Start field          = # G\\n\"
             \"\\# End field            = # G\\n\"
             \"\\# Field step size      = # G\\n\"
			 \"\\# No. of scans         = #\\n\"
             \"\\# No. of averages      = #\\n\"
			 \"\\# Sensitivity          = # mV\\n\"
			 \"\\# Timebase             = # us\\n\"
             \"\\# Time resolution      = # ns\\n\"
             \"\\# Trigger position     = # us\\n\"
			 \"\\# No. of traces        = #\\n\"
             \"\\# Trace length         = #\\n\"
			 \"\\# No signal traces     = #\\n\"
	         \"\\# Temperature at start = # K\\n\"
    	     \"\\# Temperature at end   = # K\\n\",
       date( ), time( ), start_field,
	   Scans_Done == 0 ? start_field + ( I - 1 ) * field_step : end_field,
	   field_step, Scans_Done == 0 ? 1 : Scans_Done, Num_Avg, sens / 1 mV,
	   timebase / 1 us, time_res / 1 ns, pretrigger / 1 us,
	   Scans_Done == 0 ? I : Num_Traces, Trace_Len, Num_No_Signal_Traces,
	   start_temp, temp_contr_temperature( ) );

/* Ask the user for a comment and write it into the file */

save_comment( File, \"# \" );

/* Finally write all (scan-averaged if applicable) raw data to the file */

IF Scans_Done == 0 {
    FOR J = 1 : I {
		fsave( File, \"\\# Field = # G\\n\",
		       start_field + ( J - 1 ) * field_step );
		save( File, data[ J ] );
	}
} ELSE {
    FOR J = 1 : Num_Traces {
		fsave( File, \"\\# Field = # G\\n\",
		       start_field + ( J - 1 ) * field_step );
		save( File, mdata[ J ] );
	}
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

    if ( $START_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $START_FIELD{ max } ? $START_FIELD{ max } >= $START_FIELD{ value } : 1 ) and
         ( defined $START_FIELD{ min } ? $START_FIELD{ min } <= $START_FIELD{ value } : 1 ) ) {
        print $fh "$START_FIELD{ value }\n";
    } else {
        print $fh "80000\n";
    }

    if ( $END_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $END_FIELD{ max } ? $END_FIELD{ max } >= $END_FIELD{ value } : 1 ) and
         ( defined $END_FIELD{ min } ? $END_FIELD{ min } <= $END_FIELD{ value } : 1 ) ) {
        print $fh "$END_FIELD{ value }\n";
    } else {
        print $fh "80500\n";
    }

    if ( $FIELD_STEP{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } >= $FIELD_STEP{ value } : 1 ) and
         ( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } <= $FIELD_STEP{ value } : 1 ) ) {
        print $fh "$FIELD_STEP{ value }\n";
    } else {
        print $fh "1\n";
    }

    print $fh "$TRACE_LEN{ value }\n";

    if ( $SIGNAL_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $SIGNAL_FIELD{ max } ? $SIGNAL_FIELD{ max } >= $SIGNAL_FIELD{ value } : 1 ) and
         ( defined $SIGNAL_FIELD{ min } ? $SIGNAL_FIELD{ min } <= $SIGNAL_FIELD{ value } : 1 ) ) {
        print $fh "$SIGNAL_FIELD{ value }\n";
    } else {
        print $fh "80050\n";
    }

    if ( $PRETRIGGER{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $PRETRIGGER{ max } ? $PRETRIGGER{ max } >= $PRETRIGGER{ value } : 1 ) and
         ( defined $PRETRIGGER{ min } ? $PRETRIGGER{ min } <= $PRETRIGGER{ value } : 1 ) ) {
        print $fh "$PRETRIGGER{ value }\n";
    } else {
        print $fh "10.0\n";
    }

    if ( $NUM_AVG{ value } =~ /^[+-]?\d+$/o and
         ( defined $NUM_AVG{ max } ? $NUM_AVG{ max } >= $NUM_AVG{ value } : 1 ) and
         ( defined $NUM_AVG{ min } ? $NUM_AVG{ min } <= $NUM_AVG{ value } : 1 ) ) {
        print $fh "$NUM_AVG{ value }\n";
    } else {
        print $fh "10\n";
    }

    print $fh "$ACQ_CH{ value }\n";

    print $fh "$AVG_CH{ value }\n";

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

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $START_FIELD{ max } and $ne > $START_FIELD{ max } ) or
                         ( defined $START_FIELD{ min } and $ne < $START_FIELD{ min } );
    $START_FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $END_FIELD{ max } and $ne > $END_FIELD{ max } ) or
                         ( defined $END_FIELD{ min } and $ne < $END_FIELD{ min } );
    $END_FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $FIELD_STEP{ max } and $ne > $FIELD_STEP{ max } ) or
                         ( defined $FIELD_STEP{ min } and $ne < $FIELD_STEP{ min } );
    $FIELD_STEP{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @TRACE_LEN ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $TRACE_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $SIGNAL_FIELD{ max } and $ne > $SIGNAL_FIELD{ max } ) or
                         ( defined $SIGNAL_FIELD{ min } and $ne < $SIGNAL_FIELD{ min } );
    $SIGNAL_FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $PRETRIGGER{ max } and $ne > $PRETRIGGER{ max } ) or
                         ( defined $PRETRIGGER{ min } and $ne < $PRETRIGGER{ min } );
    $PRETRIGGER{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $NUM_AVG{ max } and $ne > $NUM_AVG{ max } ) or
                         ( defined $NUM_AVG{ min } and $ne < $NUM_AVG{ min } );
    $NUM_AVG{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @ACQ_CH ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $ACQ_CH{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @AVG_CH ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $AVG_CH{ value } = $ne;

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
