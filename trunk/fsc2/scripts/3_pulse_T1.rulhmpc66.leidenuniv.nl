#!/usr/bin/perl
# -*- cperl -*-
# Generated by fsc2_guify from 3_pulse_T1.EDL on Fri Dec 19 00:06:38 CET 2003

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

my $fsc2_how_to_run = "Test program";
my @fsc2_how_to_run = ( "Start experiment",
                        "Test program",
                        "Load into fsc2" );
my $fsc2_main_window = MainWindow->new( );
my $fsc2_main_frame = $fsc2_main_window->Frame( -relief => "ridge",
                                                -borderwidth => "1m" );
my $fsc2_balloon = $fsc2_main_frame->Balloon( );
my $fsc2_apply_frame = $fsc2_main_window->Frame( );
my $fsc2_apply_button = $fsc2_apply_frame->Button( -text => "Apply",
                                                   -command => \&write_out );
$fsc2_apply_button->bind( "all", "<Alt-a>" => \&write_out );
my $fsc2_quit_button = $fsc2_apply_frame->Button( -text => "Quit",
                 -command => sub { &store_defs; $fsc2_main_window->destroy } );
$fsc2_quit_button->bind( "all",
                         "<Alt-q>" =>
                         sub { &store_defs; $fsc2_main_window->destroy } );
$fsc2_apply_frame->pack( -side => "bottom",
                         -fill => "x",
                         -padx => "4m" );

# === REP_TIME float [ 1.0e-3 : ] [ 10 ] "Repetition time" "ms"
my %REP_TIME;
$REP_TIME{ tk_frame } = $fsc2_main_frame->Frame( );
$REP_TIME{ tk_label } = $REP_TIME{ tk_frame }->Label( -text => "Repetition time",
-width => 20,
-anchor => 'w' );
$REP_TIME{ value } = 10;
$REP_TIME{ min } = 1.0e-3;
$REP_TIME{ max } = undef;
$REP_TIME{ tk_entry } = $REP_TIME{ tk_frame }->Entry( -textvariable => \$REP_TIME{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $REP_TIME{ min } ? $REP_TIME{ min } : undef ),
( defined $REP_TIME{ max } ? $REP_TIME{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $REP_TIME{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $REP_TIME{ min } ? $REP_TIME{ min } : '-inf' ) .
" : " . ( defined $REP_TIME{ max } ? $REP_TIME{ max } : '+inf' ) . " ]" );
$REP_TIME{ tk_unit } = $REP_TIME{ tk_frame }->Label( -text => "ms",
-width => 5 );
$REP_TIME{ tk_frame }->pack( %fp );
$REP_TIME{ tk_label }->pack( %wp );
$REP_TIME{ tk_entry }->pack( %wp );
$REP_TIME{ tk_unit }->pack( %up );

# === P1_LEN int [ 10 : 167772150 ] [ 100 ] "Length of 1st MW pulse" "ns"
my %P1_LEN;
$P1_LEN{ tk_frame } = $fsc2_main_frame->Frame( );
$P1_LEN{ tk_label } = $P1_LEN{ tk_frame }->Label( -text => "Length of 1st MW pulse",
-width => 20,
-anchor => 'w' );
$P1_LEN{ value } = 100;
$P1_LEN{ min } = 10;
$P1_LEN{ max } = 167772150;
$P1_LEN{ tk_entry } = $P1_LEN{ tk_frame }->Entry( -textvariable => \$P1_LEN{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P1_LEN{ min } ? $P1_LEN{ min } : undef ),
( defined $P1_LEN{ max } ? $P1_LEN{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P1_LEN{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P1_LEN{ min } ? $P1_LEN{ min } : '-inf' ) .
" : " . ( defined $P1_LEN{ max } ? $P1_LEN{ max } : '+inf' ) . " ]" );
$P1_LEN{ tk_unit } = $P1_LEN{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P1_LEN{ tk_frame }->pack( %fp );
$P1_LEN{ tk_label }->pack( %wp );
$P1_LEN{ tk_entry }->pack( %wp );
$P1_LEN{ tk_unit }->pack( %up );

# === P2_LEN int [ 10 : 167772150 ] [ 200 ] "Length of 2st MW pulse" "ns"
my %P2_LEN;
$P2_LEN{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_LEN{ tk_label } = $P2_LEN{ tk_frame }->Label( -text => "Length of 2st MW pulse",
-width => 20,
-anchor => 'w' );
$P2_LEN{ value } = 200;
$P2_LEN{ min } = 10;
$P2_LEN{ max } = 167772150;
$P2_LEN{ tk_entry } = $P2_LEN{ tk_frame }->Entry( -textvariable => \$P2_LEN{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P2_LEN{ min } ? $P2_LEN{ min } : undef ),
( defined $P2_LEN{ max } ? $P2_LEN{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P2_LEN{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P2_LEN{ min } ? $P2_LEN{ min } : '-inf' ) .
" : " . ( defined $P2_LEN{ max } ? $P2_LEN{ max } : '+inf' ) . " ]" );
$P2_LEN{ tk_unit } = $P2_LEN{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P2_LEN{ tk_frame }->pack( %fp );
$P2_LEN{ tk_label }->pack( %wp );
$P2_LEN{ tk_entry }->pack( %wp );
$P2_LEN{ tk_unit }->pack( %up );

# === P3_LEN int [ 10 : 167772150 ] [ 200 ] "Length of 3rd MW pulse" "ns"
my %P3_LEN;
$P3_LEN{ tk_frame } = $fsc2_main_frame->Frame( );
$P3_LEN{ tk_label } = $P3_LEN{ tk_frame }->Label( -text => "Length of 3rd MW pulse",
-width => 20,
-anchor => 'w' );
$P3_LEN{ value } = 200;
$P3_LEN{ min } = 10;
$P3_LEN{ max } = 167772150;
$P3_LEN{ tk_entry } = $P3_LEN{ tk_frame }->Entry( -textvariable => \$P3_LEN{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P3_LEN{ min } ? $P3_LEN{ min } : undef ),
( defined $P3_LEN{ max } ? $P3_LEN{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P3_LEN{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P3_LEN{ min } ? $P3_LEN{ min } : '-inf' ) .
" : " . ( defined $P3_LEN{ max } ? $P3_LEN{ max } : '+inf' ) . " ]" );
$P3_LEN{ tk_unit } = $P3_LEN{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P3_LEN{ tk_frame }->pack( %fp );
$P3_LEN{ tk_label }->pack( %wp );
$P3_LEN{ tk_entry }->pack( %wp );
$P3_LEN{ tk_unit }->pack( %up );

# === P1_P2_DIST int [ 60 : 167772150 ] [ 400 ] "P1-P2 distance" "ns"
my %P1_P2_DIST;
$P1_P2_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P1_P2_DIST{ tk_label } = $P1_P2_DIST{ tk_frame }->Label( -text => "P1-P2 distance",
-width => 20,
-anchor => 'w' );
$P1_P2_DIST{ value } = 400;
$P1_P2_DIST{ min } = 60;
$P1_P2_DIST{ max } = 167772150;
$P1_P2_DIST{ tk_entry } = $P1_P2_DIST{ tk_frame }->Entry( -textvariable => \$P1_P2_DIST{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P1_P2_DIST{ min } ? $P1_P2_DIST{ min } : undef ),
( defined $P1_P2_DIST{ max } ? $P1_P2_DIST{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P1_P2_DIST{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P1_P2_DIST{ min } ? $P1_P2_DIST{ min } : '-inf' ) .
" : " . ( defined $P1_P2_DIST{ max } ? $P1_P2_DIST{ max } : '+inf' ) . " ]" );
$P1_P2_DIST{ tk_unit } = $P1_P2_DIST{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P1_P2_DIST{ tk_frame }->pack( %fp );
$P1_P2_DIST{ tk_label }->pack( %wp );
$P1_P2_DIST{ tk_entry }->pack( %wp );
$P1_P2_DIST{ tk_unit }->pack( %up );

# === P2_P3_DIST int [ 60 : 167772150 ] [ 400 ] "Init. P2-P3 distance" "ns"
my %P2_P3_DIST;
$P2_P3_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_P3_DIST{ tk_label } = $P2_P3_DIST{ tk_frame }->Label( -text => "Init. P2-P3 distance",
-width => 20,
-anchor => 'w' );
$P2_P3_DIST{ value } = 400;
$P2_P3_DIST{ min } = 60;
$P2_P3_DIST{ max } = 167772150;
$P2_P3_DIST{ tk_entry } = $P2_P3_DIST{ tk_frame }->Entry( -textvariable => \$P2_P3_DIST{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P2_P3_DIST{ min } ? $P2_P3_DIST{ min } : undef ),
( defined $P2_P3_DIST{ max } ? $P2_P3_DIST{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P2_P3_DIST{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P2_P3_DIST{ min } ? $P2_P3_DIST{ min } : '-inf' ) .
" : " . ( defined $P2_P3_DIST{ max } ? $P2_P3_DIST{ max } : '+inf' ) . " ]" );
$P2_P3_DIST{ tk_unit } = $P2_P3_DIST{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P2_P3_DIST{ tk_frame }->pack( %fp );
$P2_P3_DIST{ tk_label }->pack( %wp );
$P2_P3_DIST{ tk_entry }->pack( %wp );
$P2_P3_DIST{ tk_unit }->pack( %up );

# === P2_P3_INCR int [ 10 : ] [ 20 ] "P2-P3 dist. increment" "ns"
my %P2_P3_INCR;
$P2_P3_INCR{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_P3_INCR{ tk_label } = $P2_P3_INCR{ tk_frame }->Label( -text => "P2-P3 dist. increment",
-width => 20,
-anchor => 'w' );
$P2_P3_INCR{ value } = 20;
$P2_P3_INCR{ min } = 10;
$P2_P3_INCR{ max } = undef;
$P2_P3_INCR{ tk_entry } = $P2_P3_INCR{ tk_frame }->Entry( -textvariable => \$P2_P3_INCR{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P2_P3_INCR{ min } ? $P2_P3_INCR{ min } : undef ),
( defined $P2_P3_INCR{ max } ? $P2_P3_INCR{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P2_P3_INCR{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P2_P3_INCR{ min } ? $P2_P3_INCR{ min } : '-inf' ) .
" : " . ( defined $P2_P3_INCR{ max } ? $P2_P3_INCR{ max } : '+inf' ) . " ]" );
$P2_P3_INCR{ tk_unit } = $P2_P3_INCR{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P2_P3_INCR{ tk_frame }->pack( %fp );
$P2_P3_INCR{ tk_label }->pack( %wp );
$P2_P3_INCR{ tk_entry }->pack( %wp );
$P2_P3_INCR{ tk_unit }->pack( %up );

# === DET_OFFSET int [ -167772150 : 167772150 ] [ 0 ] "Detection offset" "ns"
my %DET_OFFSET;
$DET_OFFSET{ tk_frame } = $fsc2_main_frame->Frame( );
$DET_OFFSET{ tk_label } = $DET_OFFSET{ tk_frame }->Label( -text => "Detection offset",
-width => 20,
-anchor => 'w' );
$DET_OFFSET{ value } = 0;
$DET_OFFSET{ min } = -167772150;
$DET_OFFSET{ max } = 167772150;
$DET_OFFSET{ tk_entry } = $DET_OFFSET{ tk_frame }->Entry( -textvariable => \$DET_OFFSET{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } : undef ),
( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $DET_OFFSET{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } : '-inf' ) .
" : " . ( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } : '+inf' ) . " ]" );
$DET_OFFSET{ tk_unit } = $DET_OFFSET{ tk_frame }->Label( -text => "ns",
-width => 5 );
$DET_OFFSET{ tk_frame }->pack( %fp );
$DET_OFFSET{ tk_label }->pack( %wp );
$DET_OFFSET{ tk_entry }->pack( %wp );
$DET_OFFSET{ tk_unit }->pack( %up );

# === NUM_AVG int [ 1 : 10000 ] [ 10 ] "Number of averages"
my %NUM_AVG;
$NUM_AVG{ tk_frame } = $fsc2_main_frame->Frame( );
$NUM_AVG{ tk_label } = $NUM_AVG{ tk_frame }->Label( -text => "Number of averages",
-width => 20,
-anchor => 'w' );
$NUM_AVG{ value } = 10;
$NUM_AVG{ min } = 1;
$NUM_AVG{ max } = 10000;
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
$NUM_AVG{ tk_unit }->pack( %up );

# === FIELD float [ 0 : 114304 ] [ 8000 ] "Field" "G"
my %FIELD;
$FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$FIELD{ tk_label } = $FIELD{ tk_frame }->Label( -text => "Field",
-width => 20,
-anchor => 'w' );
$FIELD{ value } = 8000;
$FIELD{ min } = 0;
$FIELD{ max } = 114304;
$FIELD{ tk_entry } = $FIELD{ tk_frame }->Entry( -textvariable => \$FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $FIELD{ min } ? $FIELD{ min } : undef ),
( defined $FIELD{ max } ? $FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $FIELD{ min } ? $FIELD{ min } : '-inf' ) .
" : " . ( defined $FIELD{ max } ? $FIELD{ max } : '+inf' ) . " ]" );
$FIELD{ tk_unit } = $FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$FIELD{ tk_frame }->pack( %fp );
$FIELD{ tk_label }->pack( %wp );
$FIELD{ tk_entry }->pack( %wp );
$FIELD{ tk_unit }->pack( %up );

$fsc2_main_frame->pack( %fp, -pady => '1m' );
$fsc2_main_window->Optionmenu( -options => \@fsc2_how_to_run,
                                -textvariable => \$fsc2_how_to_run,
                              )->pack( -padx => '3m',
                                       -pady => '3m' );

$fsc2_apply_button->pack( %wp, padx => '5m', -pady => '3m' );
$fsc2_quit_button->pack( %wp, padx => '5m', -pady => '3m' );

load_defs( );
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

    my $REP_TIME = $REP_TIME{ value };
    my $P1_LEN = $P1_LEN{ value };
    my $P2_LEN = $P2_LEN{ value };
    my $P3_LEN = $P3_LEN{ value };
    my $P1_P2_DIST = $P1_P2_DIST{ value };
    my $P2_P3_DIST = $P2_P3_DIST{ value };
    my $P2_P3_INCR = $P2_P3_INCR{ value };
    my $DET_OFFSET = $DET_OFFSET{ value };
    my $NUM_AVG = $NUM_AVG{ value };
    my $FIELD = $FIELD{ value };

    print $fh "DEVICES:

ips120_10;
rb8509;
rb_pulser;


VARIABLES:

repeat_time = $REP_TIME ms;
p1_to_p2_dist = $P1_P2_DIST ns;
p2_to_p3_dist = $P2_P3_DIST ns;
p2_to_p3_incr = $P2_P3_INCR ns;
p1_len = $P1_LEN ns;
p2_len = $P2_LEN ns;
p3_len = $P3_LEN ns;
det_offset = $DET_OFFSET ns;

field = $FIELD G;

N_Avg  = $NUM_AVG;
I;
data;
File;


ASSIGNMENTS:

TRIGGER_MODE: INTERNAL, REPEAT_TIME = repeat_time;


PREPARATIONS:

P1:  FUNCTION    = MICROWAVE,
	 START       = 120 ns,
	 LENGTH      = p1_len;

P2:  FUNCTION    = MICROWAVE,
	 START       = P1.START + p1_to_p2_dist + 0.5 * ( P1.LENGTH - p2_len ),
	 LENGTH      = p2_len;

P3:  FUNCTION    = MICROWAVE,
	 START       = P2.START + p2_to_p3_dist + 0.5 * ( P2.LENGTH - p3_len ),
	 DELTA_START = p2_to_p3_incr,
	 LENGTH      = p3_len;

P4:  FUNCTION    = DETECTION,
	 START       = P3.START + det_offset + p1_to_p2_dist + 0.5 * P3.LENGTH,
	 DELTA_START = p2_to_p3_incr,
	 LENGTH      = 100 ns;

init_1d( 1, 0, p2_to_p3_dist * 1.0e9, p2_to_p3_incr * 1.0e9,
		 \"Pulse separation [ns]\", \"Echo amplitude [a.u.]\" );


EXPERIMENT:

/* Go to the field */

set_field( field );

/* Open the data file, ask user for comment to be written to it and the
   output the parameter to it */

File = get_file( );
save_comment( File, \"%\" );

fsave( File,
	   \"% Date:                   # #\\n\"
	   \"% Field:                  # G\\n\"
	   \"% Repetition time:        # ms\\n\"
	   \"% Length of 1st MW pulse: # ns\\n\"
	   \"% Length of 2st MW pulse: # ns\\n\"
	   \"% Length of 3st MW pulse: # ns\\n\"
	   \"% P1-P2 separation:       # ns\\n\"
	   \"% Init. P2-P3 separation: # ns\\n\"
	   \"% P2-P3 increment:        # ns\\n\",
	   date( ), time( ), field,  repeat_time * 1.0e3, P1.LENGTH * 1.0e9,
	   P2.LENGTH * 1.0e9, P3.LENGTH * 1.0e9, p1_to_p2_dist * 1.0e9,
	   p2_to_p3_dist * 1.0e9, p2_to_p3_incr * 1.0e9 );

I = 1;

FOREVER {
	pulser_state( \"ON\" );
	wait( 1.1 * repeat_time * N_Avg );
	data = daq_get_voltage( CH0 );
	pulser_state( \"OFF\" );
	display( I, data );
	fsave( File, \"#,#\\n\", p2_to_p3_dist * ( I - 1 ) * 1.0e9, data );
	pulser_shift( );
	pulser_update( );	
	I += 1;
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

    if ( $REP_TIME{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $REP_TIME{ max } ? $REP_TIME{ max } >= $REP_TIME{ value } : 1 ) and
         ( defined $REP_TIME{ min } ? $REP_TIME{ min } <= $REP_TIME{ value } : 1 ) ) {
        print $fh "$REP_TIME{ value }\n";
    } else {
        print $fh "10\n";
    }

    if ( $P1_LEN{ value } =~ /^[+-]?\d+$/o and
         ( defined $P1_LEN{ max } ? $P1_LEN{ max } >= $P1_LEN{ value } : 1 ) and
         ( defined $P1_LEN{ min } ? $P1_LEN{ min } <= $P1_LEN{ value } : 1 ) ) {
        print $fh "$P1_LEN{ value }\n";
    } else {
        print $fh "100\n";
    }

    if ( $P2_LEN{ value } =~ /^[+-]?\d+$/o and
         ( defined $P2_LEN{ max } ? $P2_LEN{ max } >= $P2_LEN{ value } : 1 ) and
         ( defined $P2_LEN{ min } ? $P2_LEN{ min } <= $P2_LEN{ value } : 1 ) ) {
        print $fh "$P2_LEN{ value }\n";
    } else {
        print $fh "200\n";
    }

    if ( $P3_LEN{ value } =~ /^[+-]?\d+$/o and
         ( defined $P3_LEN{ max } ? $P3_LEN{ max } >= $P3_LEN{ value } : 1 ) and
         ( defined $P3_LEN{ min } ? $P3_LEN{ min } <= $P3_LEN{ value } : 1 ) ) {
        print $fh "$P3_LEN{ value }\n";
    } else {
        print $fh "200\n";
    }

    if ( $P1_P2_DIST{ value } =~ /^[+-]?\d+$/o and
         ( defined $P1_P2_DIST{ max } ? $P1_P2_DIST{ max } >= $P1_P2_DIST{ value } : 1 ) and
         ( defined $P1_P2_DIST{ min } ? $P1_P2_DIST{ min } <= $P1_P2_DIST{ value } : 1 ) ) {
        print $fh "$P1_P2_DIST{ value }\n";
    } else {
        print $fh "400\n";
    }

    if ( $P2_P3_DIST{ value } =~ /^[+-]?\d+$/o and
         ( defined $P2_P3_DIST{ max } ? $P2_P3_DIST{ max } >= $P2_P3_DIST{ value } : 1 ) and
         ( defined $P2_P3_DIST{ min } ? $P2_P3_DIST{ min } <= $P2_P3_DIST{ value } : 1 ) ) {
        print $fh "$P2_P3_DIST{ value }\n";
    } else {
        print $fh "400\n";
    }

    if ( $P2_P3_INCR{ value } =~ /^[+-]?\d+$/o and
         ( defined $P2_P3_INCR{ max } ? $P2_P3_INCR{ max } >= $P2_P3_INCR{ value } : 1 ) and
         ( defined $P2_P3_INCR{ min } ? $P2_P3_INCR{ min } <= $P2_P3_INCR{ value } : 1 ) ) {
        print $fh "$P2_P3_INCR{ value }\n";
    } else {
        print $fh "20\n";
    }

    if ( $DET_OFFSET{ value } =~ /^[+-]?\d+$/o and
         ( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } >= $DET_OFFSET{ value } : 1 ) and
         ( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } <= $DET_OFFSET{ value } : 1 ) ) {
        print $fh "$DET_OFFSET{ value }\n";
    } else {
        print $fh "0\n";
    }

    if ( $NUM_AVG{ value } =~ /^[+-]?\d+$/o and
         ( defined $NUM_AVG{ max } ? $NUM_AVG{ max } >= $NUM_AVG{ value } : 1 ) and
         ( defined $NUM_AVG{ min } ? $NUM_AVG{ min } <= $NUM_AVG{ value } : 1 ) ) {
        print $fh "$NUM_AVG{ value }\n";
    } else {
        print $fh "10\n";
    }

    if ( $FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FIELD{ max } ? $FIELD{ max } >= $FIELD{ value } : 1 ) and
         ( defined $FIELD{ min } ? $FIELD{ min } <= $FIELD{ value } : 1 ) ) {
        print $fh "$FIELD{ value }\n";
    } else {
        print $fh "8000\n";
    }

    print $fh "$fsc2_how_to_run\n";

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
    goto done_reading if ( defined $REP_TIME{ max } and $ne > $REP_TIME{ max } ) or
                         ( defined $REP_TIME{ min } and $ne < $REP_TIME{ min } );
    $REP_TIME{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P1_LEN{ max } and $ne > $P1_LEN{ max } ) or
                         ( defined $P1_LEN{ min } and $ne < $P1_LEN{ min } );
    $P1_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P2_LEN{ max } and $ne > $P2_LEN{ max } ) or
                         ( defined $P2_LEN{ min } and $ne < $P2_LEN{ min } );
    $P2_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P3_LEN{ max } and $ne > $P3_LEN{ max } ) or
                         ( defined $P3_LEN{ min } and $ne < $P3_LEN{ min } );
    $P3_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P1_P2_DIST{ max } and $ne > $P1_P2_DIST{ max } ) or
                         ( defined $P1_P2_DIST{ min } and $ne < $P1_P2_DIST{ min } );
    $P1_P2_DIST{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P2_P3_DIST{ max } and $ne > $P2_P3_DIST{ max } ) or
                         ( defined $P2_P3_DIST{ min } and $ne < $P2_P3_DIST{ min } );
    $P2_P3_DIST{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $P2_P3_INCR{ max } and $ne > $P2_P3_INCR{ max } ) or
                         ( defined $P2_P3_INCR{ min } and $ne < $P2_P3_INCR{ min } );
    $P2_P3_INCR{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $DET_OFFSET{ max } and $ne > $DET_OFFSET{ max } ) or
                         ( defined $DET_OFFSET{ min } and $ne < $DET_OFFSET{ min } );
    $DET_OFFSET{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $NUM_AVG{ max } and $ne > $NUM_AVG{ max } ) or
                         ( defined $NUM_AVG{ min } and $ne < $NUM_AVG{ min } );
    $NUM_AVG{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $FIELD{ max } and $ne > $FIELD{ max } ) or
                         ( defined $FIELD{ min } and $ne < $FIELD{ min } );
    $FIELD{ value } = $ne;

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

  done_reading:
    close $fh;
};
