#!/usr/bin/perl
# -*- cperl -*-
# Generated by fsc2_guify from 3_pulse_endor.EDL on Thu Apr 22 15:02:31 CEST 2004

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
my $fsc2_main_window = MainWindow->new( );
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

# === REP_TIME float [ 0.001 : ] [ 50 ] "Repetition time" "ms"

my %REP_TIME;
$REP_TIME{ tk_frame } = $fsc2_main_frame->Frame( );
$REP_TIME{ tk_label } = $REP_TIME{ tk_frame }->Label( -text => "Repetition time",
-width => 20,
-anchor => 'w' );
$REP_TIME{ value } = 50;
$REP_TIME{ min } = 0.001;
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
$REP_TIME{ tk_unit  }->pack( %up );

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
$P1_LEN{ tk_unit  }->pack( %up );

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
$P2_LEN{ tk_unit  }->pack( %up );

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
$P3_LEN{ tk_unit  }->pack( %up );

# === P1_P2_DIST int [ 100 : 167772150 ] [ 400 ] "P1-P2 distance" "ns"

my %P1_P2_DIST;
$P1_P2_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P1_P2_DIST{ tk_label } = $P1_P2_DIST{ tk_frame }->Label( -text => "P1-P2 distance",
-width => 20,
-anchor => 'w' );
$P1_P2_DIST{ value } = 400;
$P1_P2_DIST{ min } = 100;
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
$P1_P2_DIST{ tk_unit  }->pack( %up );

# === P2_P3_DIST int [ 100 : 167772150 ] [ 2000 ] "P2-P3 distance" "ns"

my %P2_P3_DIST;
$P2_P3_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_P3_DIST{ tk_label } = $P2_P3_DIST{ tk_frame }->Label( -text => "P2-P3 distance",
-width => 20,
-anchor => 'w' );
$P2_P3_DIST{ value } = 2000;
$P2_P3_DIST{ min } = 100;
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
$P2_P3_DIST{ tk_unit  }->pack( %up );

# === P2_RF_DEL int [ 0 : 167772150 ] [ 100 ] "P2-RF pulse delay" "ns"

my %P2_RF_DEL;
$P2_RF_DEL{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_RF_DEL{ tk_label } = $P2_RF_DEL{ tk_frame }->Label( -text => "P2-RF pulse delay",
-width => 20,
-anchor => 'w' );
$P2_RF_DEL{ value } = 100;
$P2_RF_DEL{ min } = 0;
$P2_RF_DEL{ max } = 167772150;
$P2_RF_DEL{ tk_entry } = $P2_RF_DEL{ tk_frame }->Entry( -textvariable => \$P2_RF_DEL{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P2_RF_DEL{ min } ? $P2_RF_DEL{ min } : undef ),
( defined $P2_RF_DEL{ max } ? $P2_RF_DEL{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P2_RF_DEL{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P2_RF_DEL{ min } ? $P2_RF_DEL{ min } : '-inf' ) .
" : " . ( defined $P2_RF_DEL{ max } ? $P2_RF_DEL{ max } : '+inf' ) . " ]" );
$P2_RF_DEL{ tk_unit } = $P2_RF_DEL{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P2_RF_DEL{ tk_frame }->pack( %fp );
$P2_RF_DEL{ tk_label }->pack( %wp );
$P2_RF_DEL{ tk_entry }->pack( %wp );
$P2_RF_DEL{ tk_unit  }->pack( %up );

# === RF_P3_DEL int [ 0 : 167772150 ] [ 100 ] "RF-P3 pulse delay" "ns"

my %RF_P3_DEL;
$RF_P3_DEL{ tk_frame } = $fsc2_main_frame->Frame( );
$RF_P3_DEL{ tk_label } = $RF_P3_DEL{ tk_frame }->Label( -text => "RF-P3 pulse delay",
-width => 20,
-anchor => 'w' );
$RF_P3_DEL{ value } = 100;
$RF_P3_DEL{ min } = 0;
$RF_P3_DEL{ max } = 167772150;
$RF_P3_DEL{ tk_entry } = $RF_P3_DEL{ tk_frame }->Entry( -textvariable => \$RF_P3_DEL{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $RF_P3_DEL{ min } ? $RF_P3_DEL{ min } : undef ),
( defined $RF_P3_DEL{ max } ? $RF_P3_DEL{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $RF_P3_DEL{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $RF_P3_DEL{ min } ? $RF_P3_DEL{ min } : '-inf' ) .
" : " . ( defined $RF_P3_DEL{ max } ? $RF_P3_DEL{ max } : '+inf' ) . " ]" );
$RF_P3_DEL{ tk_unit } = $RF_P3_DEL{ tk_frame }->Label( -text => "ns",
-width => 5 );
$RF_P3_DEL{ tk_frame }->pack( %fp );
$RF_P3_DEL{ tk_label }->pack( %wp );
$RF_P3_DEL{ tk_entry }->pack( %wp );
$RF_P3_DEL{ tk_unit  }->pack( %up );

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
$DET_OFFSET{ tk_unit  }->pack( %up );

# === N_AVG menu [ "1", "3", "10", "30", "100", "300", "1000",  "3000", "10000" ] [ 3 ] "Number of averages"

my %N_AVG;
$N_AVG{ tk_frame } = $fsc2_main_frame->Frame( );
$N_AVG{ tk_label } = $N_AVG{ tk_frame }->Label( -text => "Number of averages",
-width => 20,
-anchor => 'w' );
$N_AVG{ value } = "10";
my @N_AVG = ( "1", "3", "10", "30", "100", "300", "1000", "3000", "10000" );
$N_AVG{ tk_entry } = $N_AVG{ tk_frame }->Optionmenu( -options => \@N_AVG,
-width => 10,
-textvariable => \$N_AVG{ value } );
$N_AVG{ tk_unit } = $N_AVG{ tk_frame }->Label( -text => "",
-width => 5 );
$N_AVG{ tk_frame }->pack( %fp );
$N_AVG{ tk_label }->pack( %wp );
$N_AVG{ tk_entry }->pack( %wp );
$N_AVG{ tk_unit  }->pack( %up );

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
$FIELD{ tk_unit  }->pack( %up );

# === START_FREQ float [ 0.009 : 1100 ] [ 25 ] "Start frequency" "MHz"

my %START_FREQ;
$START_FREQ{ tk_frame } = $fsc2_main_frame->Frame( );
$START_FREQ{ tk_label } = $START_FREQ{ tk_frame }->Label( -text => "Start frequency",
-width => 20,
-anchor => 'w' );
$START_FREQ{ value } = 25;
$START_FREQ{ min } = 0.009;
$START_FREQ{ max } = 1100;
$START_FREQ{ tk_entry } = $START_FREQ{ tk_frame }->Entry( -textvariable => \$START_FREQ{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $START_FREQ{ min } ? $START_FREQ{ min } : undef ),
( defined $START_FREQ{ max } ? $START_FREQ{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $START_FREQ{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $START_FREQ{ min } ? $START_FREQ{ min } : '-inf' ) .
" : " . ( defined $START_FREQ{ max } ? $START_FREQ{ max } : '+inf' ) . " ]" );
$START_FREQ{ tk_unit } = $START_FREQ{ tk_frame }->Label( -text => "MHz",
-width => 5 );
$START_FREQ{ tk_frame }->pack( %fp );
$START_FREQ{ tk_label }->pack( %wp );
$START_FREQ{ tk_entry }->pack( %wp );
$START_FREQ{ tk_unit  }->pack( %up );

# === END_FREQ float [ 0.009 : 1100 ] [ 50 ] "End frequency" "MHz"

my %END_FREQ;
$END_FREQ{ tk_frame } = $fsc2_main_frame->Frame( );
$END_FREQ{ tk_label } = $END_FREQ{ tk_frame }->Label( -text => "End frequency",
-width => 20,
-anchor => 'w' );
$END_FREQ{ value } = 50;
$END_FREQ{ min } = 0.009;
$END_FREQ{ max } = 1100;
$END_FREQ{ tk_entry } = $END_FREQ{ tk_frame }->Entry( -textvariable => \$END_FREQ{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $END_FREQ{ min } ? $END_FREQ{ min } : undef ),
( defined $END_FREQ{ max } ? $END_FREQ{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $END_FREQ{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $END_FREQ{ min } ? $END_FREQ{ min } : '-inf' ) .
" : " . ( defined $END_FREQ{ max } ? $END_FREQ{ max } : '+inf' ) . " ]" );
$END_FREQ{ tk_unit } = $END_FREQ{ tk_frame }->Label( -text => "MHz",
-width => 5 );
$END_FREQ{ tk_frame }->pack( %fp );
$END_FREQ{ tk_label }->pack( %wp );
$END_FREQ{ tk_entry }->pack( %wp );
$END_FREQ{ tk_unit  }->pack( %up );

# === FREQ_STEP float [ 0.001 : 1000 ] [ 0.25 ] "Step frequency" "MHz"

my %FREQ_STEP;
$FREQ_STEP{ tk_frame } = $fsc2_main_frame->Frame( );
$FREQ_STEP{ tk_label } = $FREQ_STEP{ tk_frame }->Label( -text => "Step frequency",
-width => 20,
-anchor => 'w' );
$FREQ_STEP{ value } = 0.25;
$FREQ_STEP{ min } = 0.001;
$FREQ_STEP{ max } = 1000;
$FREQ_STEP{ tk_entry } = $FREQ_STEP{ tk_frame }->Entry( -textvariable => \$FREQ_STEP{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $FREQ_STEP{ min } ? $FREQ_STEP{ min } : undef ),
( defined $FREQ_STEP{ max } ? $FREQ_STEP{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $FREQ_STEP{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $FREQ_STEP{ min } ? $FREQ_STEP{ min } : '-inf' ) .
" : " . ( defined $FREQ_STEP{ max } ? $FREQ_STEP{ max } : '+inf' ) . " ]" );
$FREQ_STEP{ tk_unit } = $FREQ_STEP{ tk_frame }->Label( -text => "MHz",
-width => 5 );
$FREQ_STEP{ tk_frame }->pack( %fp );
$FREQ_STEP{ tk_label }->pack( %wp );
$FREQ_STEP{ tk_entry }->pack( %wp );
$FREQ_STEP{ tk_unit  }->pack( %up );

# === RF_ATT float [ -140 : 13 ] [ -13 ] "Attenuation" "dB"

my %RF_ATT;
$RF_ATT{ tk_frame } = $fsc2_main_frame->Frame( );
$RF_ATT{ tk_label } = $RF_ATT{ tk_frame }->Label( -text => "Attenuation",
-width => 20,
-anchor => 'w' );
$RF_ATT{ value } = -13;
$RF_ATT{ min } = -140;
$RF_ATT{ max } = 13;
$RF_ATT{ tk_entry } = $RF_ATT{ tk_frame }->Entry( -textvariable => \$RF_ATT{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $RF_ATT{ min } ? $RF_ATT{ min } : undef ),
( defined $RF_ATT{ max } ? $RF_ATT{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $RF_ATT{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $RF_ATT{ min } ? $RF_ATT{ min } : '-inf' ) .
" : " . ( defined $RF_ATT{ max } ? $RF_ATT{ max } : '+inf' ) . " ]" );
$RF_ATT{ tk_unit } = $RF_ATT{ tk_frame }->Label( -text => "dB",
-width => 5 );
$RF_ATT{ tk_frame }->pack( %fp );
$RF_ATT{ tk_label }->pack( %wp );
$RF_ATT{ tk_entry }->pack( %wp );
$RF_ATT{ tk_unit  }->pack( %up );

# === SHOW_PREV button [ ON ] "Show pulse preview"

my %SHOW_PREV;
$SHOW_PREV{ tk_frame } = $fsc2_main_frame->Frame( );
$SHOW_PREV{ tk_label } = $SHOW_PREV{ tk_frame }->Label( -text => "Show pulse preview",
-width => 20,
-anchor => 'w' );
$SHOW_PREV{ value } = 1;
$SHOW_PREV{ tk_entry } = $SHOW_PREV{ tk_frame }->Checkbutton( -variable => \$SHOW_PREV{ value },
-width => 10 );
$SHOW_PREV{ tk_unit } = $SHOW_PREV{ tk_frame }->Label( -text => "",
-width => 5 );
$SHOW_PREV{ tk_frame }->pack( %fp );
$SHOW_PREV{ tk_label }->pack( %wp );
$SHOW_PREV{ tk_entry }->pack( %wp );
$SHOW_PREV{ tk_unit  }->pack( %up );

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

    my $REP_TIME = $REP_TIME{ value };
    my $P1_LEN = $P1_LEN{ value };
    my $P2_LEN = $P2_LEN{ value };
    my $P3_LEN = $P3_LEN{ value };
    my $P1_P2_DIST = $P1_P2_DIST{ value };
    my $P2_P3_DIST = $P2_P3_DIST{ value };
    my $P2_RF_DEL = $P2_RF_DEL{ value };
    my $RF_P3_DEL = $RF_P3_DEL{ value };
    my $DET_OFFSET = $DET_OFFSET{ value };
    my $N_AVG = $N_AVG{ value };
    my $FIELD = $FIELD{ value };
    my $START_FREQ = $START_FREQ{ value };
    my $END_FREQ = $END_FREQ{ value };
    my $FREQ_STEP = $FREQ_STEP{ value };
    my $RF_ATT = $RF_ATT{ value };
    my $SHOW_PREV = $SHOW_PREV{ value };

    print $fh "DEVICES:

ips120_10;
rs_sml01;
rb8509;
rb_pulser;


VARIABLES:

repeat_time   = $REP_TIME ms;
p1_to_p2_dist = $P1_P2_DIST ns;
p2_to_p3_dist = $P2_P3_DIST ns;
p1_len        = $P1_LEN ns;
p2_len        = $P2_LEN ns;
p3_len        = $P3_LEN ns;
p2_rf_delay   = $P2_RF_DEL ns;
rf_p3_delay   = $RF_P3_DEL ns;
det_offset    = $DET_OFFSET ns;

start_freq    = $START_FREQ MHz;
end_freq      = $END_FREQ MHz;
";
# === if ( START_FREQ <= END_FREQ )
    if ( eval { ( $START_FREQ <= $END_FREQ ) } ) {
        print $fh "freq_step     = $FREQ_STEP MHz;
";
# === else
    } else {
        print $fh "freq_step     = - $FREQ_STEP MHz;
";
# === endif
    }

    print $fh "att           = $RF_ATT;
freq;

field         = $FIELD G;
N_Avg         = $N_AVG;
N_Points      = ceil( ( end_freq - start_freq ) / freq_step ) + 1;
I, J = 0, K;
data[ *, *];
avg[ N_Points ];
File;


ASSIGNMENTS:

TRIGGER_MODE: INTERNAL, REPEAT_TIME = repeat_time;


PREPARATIONS:

P1:  FUNCTION = MICROWAVE,
	 START    = 120 ns,
	 LENGTH   = p1_len;

P2:  FUNCTION = MICROWAVE,
	 START    = P1.START + p1_to_p2_dist + 0.5 * ( P1.LENGTH - p2_len ),
	 LENGTH   = p2_len;

P3:  FUNCTION = MICROWAVE,
	 START    = P2.START + p2_to_p3_dist + 0.5 * ( P2.LENGTH - p3_len ),
	 LENGTH   = p3_len;

P4:  FUNCTION = RF,
	 START    = P2.START + P2.LENGTH + p2_rf_delay,
	 LENGTH   = p2_to_p3_dist - 0.5 * ( P2.LENGTH + P3.LENGTH )
				- p2_rf_delay - rf_p3_delay;

P5:  FUNCTION = DETECTION,
	 START    = P3.START + det_offset + p1_to_p2_dist + 0.5 * P3.LENGTH,
	 LENGTH   = 100 ns;

";
# === if SHOW_PREV
    if ( eval { $SHOW_PREV } ) {
        print $fh "pulser_show_pulses( );
";
# === endif
    }

    print $fh "";
# === if ( START_FREQ <= END_FREQ )
    if ( eval { ( $START_FREQ <= $END_FREQ ) } ) {
        print $fh "init_1d( 3, N_Points, start_freq * 1.0e-6, freq_step * 1.0e-6,
		 \"RF frequency [MHz]\", \"Echo amplitude [a.u.]\" );
";
# === else
    } else {
        print $fh "init_1d( 3, N_Points, end_freq * 1.0e-6, - freq_step * 1.0e-6,
		 \"RF frequency [MHz]\", \"Echo amplitude [a.u.]\" );
";
# === endif
    }

    print $fh "

EXPERIMENT:

/* Go to the start field */

field = set_field( field );

/* Open the data file */

File = get_file( );

synthesizer_attenuation( att );
freq = synthesizer_frequency( start_freq );
synthesizer_state( \"ON\" );
pulser_state( \"ON\" );
daq_gain( 4 );

FOREVER {
	freq = synthesizer_frequency( start_freq );
	J += 1;
	print( \"Starting #. sweep\\n\", J );

	FOR I = 1 : N_Points {
		wait( 1.1 * repeat_time * N_Avg );
";
# === if ( START_FREQ <= END_FREQ )
    if ( eval { ( $START_FREQ <= $END_FREQ ) } ) {
        print $fh "		data[ J, I ] = daq_get_voltage( CH0 );
		display_1d( I, data[ J, I ], 1,
					I, ( avg[ I ]  + data[ J, I ] ) / J, 2 );
";
# === else
    } else {
        print $fh "		data[ J, N_Points - I + 1 ] = daq_get_voltage( CH0 );
		display_1d( N_Points - I + 1, data[ J, N_Points - I + 1 ], 1,
					N_Points - I + 1, ( avg[ N_Points - I + 1 ]
					+ data[ J, N_Points - I + 1 ] ) / J, 2 );
";
# === endif
    }

    print $fh "		freq += freq_step;
		synthesizer_frequency( freq );
	}

	avg += data[ J ];
	clear_curve_1d( 1, 3 );
	display_1d( 1, data[ J ], 3 );
}

ON_STOP:

synthesizer_state( \"OFF\" );

IF J == 1 {
";
# === if ( START_FREQ <= END_FREQ )
    if ( eval { ( $START_FREQ <= $END_FREQ ) } ) {
        print $fh "    FOR K = 1 : I - 1 {
	    fsave( File, \"#,#\\n\", ( start_freq + ( K - 1 ) * freq_step ) * 1.0e-6,
			   data[ 1, K ] );
    }
";
# === else
    } else {
        print $fh "    FOR K = I + 1 : N_Points {
	    fsave( File, \"#,#\\n\",
			   ( start_freq + ( N_Points - K ) * freq_step ) * 1.0e-6,
			   data[ 1, K ] );
    }
";
# === endif
    }

    print $fh "	end_freq = freq - freq_step;
} ELSE {
	IF I < N_Points {
	   J -= 1;
    }

    FOR I = 1 : N_Points {
	    fsave( File, \"#\", ( start_freq + ( I - 1 ) * freq_step ) * 1.0e-6 );
	    FOR K = 1 : J {
		    fsave( File, \",#\", data[ K, I ] );
	    }
	    fsave( File, \"\\n\" );
	}

	IF J > 1 {
		fsave( File, \"\\n\" );
		FOR I = 1 : N_Points {
			fsave( File, \"#,#\\n\", ( start_freq + ( I - 1 ) * freq_step )
				   * 1.0e-6, avg[ I ] / J );
		}
    }
}

fsave( File,
	   \"% Date:                   # #\\n\"
	   \"% Script:                 3_pulse_endor\\n\"
	   \"% Field:                  # G\\n\"
	   \"% Start frequency:        # MHz\\n\"
	   \"% End frequency:          # MHz\\n\"
	   \"% Frequency step:         # MHz\\n\"
	   \"% Attenuation:            # dB\\n\"
	   \"% Repetition time:        # ms\\n\"
	   \"% Length of 1st MW pulse: # ns\\n\"
	   \"% Length of 2st MW pulse: # ns\\n\"
	   \"% Length of 3st MW pulse: # ns\\n\"
	   \"% P1-P2 separation:		  # ns\\n\"
	   \"% P2-P3 separation:		  # ns\\n\"
	   \"% RF pulse position:      # ns\\n\"
	   \"% RF pulse length:        # ns\\n\"
	   \"% Number of averages:     #\\n\",
	   date( ), time( ), field, start_freq * 1.0e-6, end_freq * 1.0e-6,
	   freq_step * 1.0e-6, att, repeat_time * 1.0e3, P1.LENGTH * 1.0e9,
	   P2.LENGTH * 1.0e9, P3.LENGTH * 1.0e9, p1_to_p2_dist * 1.0e9,
	   p2_to_p3_dist * 1.0e9, P4.START * 1.0e9, P4.LENGTH * 1.0e9, N_Avg );

save_comment( File, \"%\" );
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
        print $fh "50\n";
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
        print $fh "2000\n";
    }

    if ( $P2_RF_DEL{ value } =~ /^[+-]?\d+$/o and
         ( defined $P2_RF_DEL{ max } ? $P2_RF_DEL{ max } >= $P2_RF_DEL{ value } : 1 ) and
         ( defined $P2_RF_DEL{ min } ? $P2_RF_DEL{ min } <= $P2_RF_DEL{ value } : 1 ) ) {
        print $fh "$P2_RF_DEL{ value }\n";
    } else {
        print $fh "100\n";
    }

    if ( $RF_P3_DEL{ value } =~ /^[+-]?\d+$/o and
         ( defined $RF_P3_DEL{ max } ? $RF_P3_DEL{ max } >= $RF_P3_DEL{ value } : 1 ) and
         ( defined $RF_P3_DEL{ min } ? $RF_P3_DEL{ min } <= $RF_P3_DEL{ value } : 1 ) ) {
        print $fh "$RF_P3_DEL{ value }\n";
    } else {
        print $fh "100\n";
    }

    if ( $DET_OFFSET{ value } =~ /^[+-]?\d+$/o and
         ( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } >= $DET_OFFSET{ value } : 1 ) and
         ( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } <= $DET_OFFSET{ value } : 1 ) ) {
        print $fh "$DET_OFFSET{ value }\n";
    } else {
        print $fh "0\n";
    }

    print $fh "$N_AVG{ value }\n";

    if ( $FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FIELD{ max } ? $FIELD{ max } >= $FIELD{ value } : 1 ) and
         ( defined $FIELD{ min } ? $FIELD{ min } <= $FIELD{ value } : 1 ) ) {
        print $fh "$FIELD{ value }\n";
    } else {
        print $fh "8000\n";
    }

    if ( $START_FREQ{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $START_FREQ{ max } ? $START_FREQ{ max } >= $START_FREQ{ value } : 1 ) and
         ( defined $START_FREQ{ min } ? $START_FREQ{ min } <= $START_FREQ{ value } : 1 ) ) {
        print $fh "$START_FREQ{ value }\n";
    } else {
        print $fh "25\n";
    }

    if ( $END_FREQ{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $END_FREQ{ max } ? $END_FREQ{ max } >= $END_FREQ{ value } : 1 ) and
         ( defined $END_FREQ{ min } ? $END_FREQ{ min } <= $END_FREQ{ value } : 1 ) ) {
        print $fh "$END_FREQ{ value }\n";
    } else {
        print $fh "50\n";
    }

    if ( $FREQ_STEP{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FREQ_STEP{ max } ? $FREQ_STEP{ max } >= $FREQ_STEP{ value } : 1 ) and
         ( defined $FREQ_STEP{ min } ? $FREQ_STEP{ min } <= $FREQ_STEP{ value } : 1 ) ) {
        print $fh "$FREQ_STEP{ value }\n";
    } else {
        print $fh "0.25\n";
    }

    if ( $RF_ATT{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $RF_ATT{ max } ? $RF_ATT{ max } >= $RF_ATT{ value } : 1 ) and
         ( defined $RF_ATT{ min } ? $RF_ATT{ min } <= $RF_ATT{ value } : 1 ) ) {
        print $fh "$RF_ATT{ value }\n";
    } else {
        print $fh "-13\n";
    }

    print $fh "$SHOW_PREV{ value }\n";

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
    goto done_reading if ( defined $P2_RF_DEL{ max } and $ne > $P2_RF_DEL{ max } ) or
                         ( defined $P2_RF_DEL{ min } and $ne < $P2_RF_DEL{ min } );
    $P2_RF_DEL{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $RF_P3_DEL{ max } and $ne > $RF_P3_DEL{ max } ) or
                         ( defined $RF_P3_DEL{ min } and $ne < $RF_P3_DEL{ min } );
    $RF_P3_DEL{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if ( defined $DET_OFFSET{ max } and $ne > $DET_OFFSET{ max } ) or
                         ( defined $DET_OFFSET{ min } and $ne < $DET_OFFSET{ min } );
    $DET_OFFSET{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @N_AVG ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $N_AVG{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $FIELD{ max } and $ne > $FIELD{ max } ) or
                         ( defined $FIELD{ min } and $ne < $FIELD{ min } );
    $FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $START_FREQ{ max } and $ne > $START_FREQ{ max } ) or
                         ( defined $START_FREQ{ min } and $ne < $START_FREQ{ min } );
    $START_FREQ{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $END_FREQ{ max } and $ne > $END_FREQ{ max } ) or
                         ( defined $END_FREQ{ min } and $ne < $END_FREQ{ min } );
    $END_FREQ{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $FREQ_STEP{ max } and $ne > $FREQ_STEP{ max } ) or
                         ( defined $FREQ_STEP{ min } and $ne < $FREQ_STEP{ min } );
    $FREQ_STEP{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $RF_ATT{ max } and $ne > $RF_ATT{ max } ) or
                         ( defined $RF_ATT{ min } and $ne < $RF_ATT{ min } );
    $RF_ATT{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^1|0$/o;
    chomp $ne;
    $SHOW_PREV{ value } = $ne;

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
