#!/usr/bin/perl
# -*- cperl -*-
# Generated by fsc2_guify from 3_pulse_EM_w.EDL on Tue Apr  8 22:56:09 CEST 2008

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

# === TRIG_MODE menu [ "EXTERNAL", "INTERNAL" ] [ 1 ] "Trigger mode"

my %TRIG_MODE;
$TRIG_MODE{ tk_frame } = $fsc2_main_frame->Frame( );
$TRIG_MODE{ tk_label } = $TRIG_MODE{ tk_frame }->Label( -text => "Trigger mode",
-width => 20,
-anchor => 'w' );
$TRIG_MODE{ value } = "EXTERNAL";
my @TRIG_MODE = ( "EXTERNAL", "INTERNAL" );
$TRIG_MODE{ tk_entry } = $TRIG_MODE{ tk_frame }->Optionmenu( -options => \@TRIG_MODE,
-width => 10,
-textvariable => \$TRIG_MODE{ value } );
$TRIG_MODE{ tk_unit } = $TRIG_MODE{ tk_frame }->Label( -text => "",
-width => 5 );
$TRIG_MODE{ tk_frame }->pack( %fp );
$TRIG_MODE{ tk_label }->pack( %wp );
$TRIG_MODE{ tk_entry }->pack( %wp );
$TRIG_MODE{ tk_unit  }->pack( %up );

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

# === P1_POS int [ 200 : 167772150 ] [ 200 ] "Position of 1st MW pulse" "ns"

my %P1_POS;
$P1_POS{ tk_frame } = $fsc2_main_frame->Frame( );
$P1_POS{ tk_label } = $P1_POS{ tk_frame }->Label( -text => "Position of 1st MW pulse",
-width => 20,
-anchor => 'w' );
$P1_POS{ value } = 200;
$P1_POS{ min } = 200;
$P1_POS{ max } = 167772150;
$P1_POS{ tk_entry } = $P1_POS{ tk_frame }->Entry( -textvariable => \$P1_POS{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ int_check( shift,
( defined $P1_POS{ min } ? $P1_POS{ min } : undef ),
( defined $P1_POS{ max } ? $P1_POS{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $P1_POS{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $P1_POS{ min } ? $P1_POS{ min } : '-inf' ) .
" : " . ( defined $P1_POS{ max } ? $P1_POS{ max } : '+inf' ) . " ]" );
$P1_POS{ tk_unit } = $P1_POS{ tk_frame }->Label( -text => "ns",
-width => 5 );
$P1_POS{ tk_frame }->pack( %fp );
$P1_POS{ tk_label }->pack( %wp );
$P1_POS{ tk_entry }->pack( %wp );
$P1_POS{ tk_unit  }->pack( %up );

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

# === P2_LEN int [ 10 : 167772150 ] [ 200 ] "Length of 2nd MW pulse" "ns"

my %P2_LEN;
$P2_LEN{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_LEN{ tk_label } = $P2_LEN{ tk_frame }->Label( -text => "Length of 2nd MW pulse",
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

# === P2_P3_DIST int [ 100 : 167772150 ] [ 400 ] "Init. P2-P3 distance" "ns"

my %P2_P3_DIST;
$P2_P3_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_P3_DIST{ tk_label } = $P2_P3_DIST{ tk_frame }->Label( -text => "Init. P2-P3 distance",
-width => 20,
-anchor => 'w' );
$P2_P3_DIST{ value } = 400;
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
$P2_P3_INCR{ tk_unit  }->pack( %up );

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

# === FIELD float [ 0 : 48370.8 ] [ 35000 ] "Field" "G"

my %FIELD;
$FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$FIELD{ tk_label } = $FIELD{ tk_frame }->Label( -text => "Field",
-width => 20,
-anchor => 'w' );
$FIELD{ value } = 35000;
$FIELD{ min } = 0;
$FIELD{ max } = 48370.8;
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

# === ADC_CHANNELS menu [ "0", "1", "0 and 1" ] "ADC channel(s) to use"

my %ADC_CHANNELS;
$ADC_CHANNELS{ tk_frame } = $fsc2_main_frame->Frame( );
$ADC_CHANNELS{ tk_label } = $ADC_CHANNELS{ tk_frame }->Label( -text => "ADC channel(s) to use",
-width => 20,
-anchor => 'w' );
$ADC_CHANNELS{ value } = "0";
my @ADC_CHANNELS = ( "0", "1", "0 and 1" );
$ADC_CHANNELS{ tk_entry } = $ADC_CHANNELS{ tk_frame }->Optionmenu( -options => \@ADC_CHANNELS,
-width => 10,
-textvariable => \$ADC_CHANNELS{ value } );
$ADC_CHANNELS{ tk_unit } = $ADC_CHANNELS{ tk_frame }->Label( -text => "",
-width => 5 );
$ADC_CHANNELS{ tk_frame }->pack( %fp );
$ADC_CHANNELS{ tk_label }->pack( %wp );
$ADC_CHANNELS{ tk_entry }->pack( %wp );
$ADC_CHANNELS{ tk_unit  }->pack( %up );

# === SHOW_PREV button [ OFF ] "Show pulse preview"

my %SHOW_PREV;
$SHOW_PREV{ tk_frame } = $fsc2_main_frame->Frame( );
$SHOW_PREV{ tk_label } = $SHOW_PREV{ tk_frame }->Label( -text => "Show pulse preview",
-width => 20,
-anchor => 'w' );
$SHOW_PREV{ value } = 0;
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

    my $TRIG_MODE = $TRIG_MODE{ value };
    my $REP_TIME = $REP_TIME{ value };
    my $P1_POS = $P1_POS{ value };
    my $P1_LEN = $P1_LEN{ value };
    my $P2_LEN = $P2_LEN{ value };
    my $P3_LEN = $P3_LEN{ value };
    my $P1_P2_DIST = $P1_P2_DIST{ value };
    my $P2_P3_DIST = $P2_P3_DIST{ value };
    my $P2_P3_INCR = $P2_P3_INCR{ value };
    my $DET_OFFSET = $DET_OFFSET{ value };
    my $CREATE_DEFENSE = $CREATE_DEFENSE{ value };
    my $PULSE_2_DEFENSE = $PULSE_2_DEFENSE{ value };
    my $N_AVG = $N_AVG{ value };
    my $FIELD = $FIELD{ value };
    my $ADC_CHANNELS = $ADC_CHANNELS{ value };
    my $SHOW_PREV = $SHOW_PREV{ value };

    print $fh "/* \$Id: 3_pulse_EM_w.EDL 8051 2007-07-08 23:09:47Z jens \$ */

DEVICES:

ips120_10;
rb8509;
rb_pulser_w;


VARIABLES:

repeat_time   = $REP_TIME ms;
p1_pos        = $P1_POS ns;
p1_to_p2_dist = $P1_P2_DIST ns;
p2_to_p3_dist = $P2_P3_DIST ns;
p2_to_p3_incr = $P2_P3_INCR ns;
p1_len        = $P1_LEN ns;
p2_len        = $P2_LEN ns;
p3_len        = $P3_LEN ns;
det_offset    = $DET_OFFSET ns;
field         = $FIELD G;
N_Avg         = $N_AVG;

I;
";
# === if ( ADC_CHANNELS eq "0 and 1" )
    if ( eval { ( $ADC_CHANNELS eq "0 and 1" ) } ) {
        print $fh "data[ 2 ];
";
# === else
    } else {
        print $fh "data;
";
# === endif
    }

    print $fh "File;
B1, B2;


ASSIGNMENTS:

TIMEBASE: 10 ns;
";
# === if ( TRIG_MODE eq "EXTERNAL" )
    if ( eval { ( $TRIG_MODE eq "EXTERNAL" ) } ) {
        print $fh "TRIGGER_MODE: EXTERNAL;
";
# === else
    } else {
        print $fh "TRIGGER_MODE: $TRIG_MODE, REPEAT_TIME = repeat_time;
";
# === endif
    }

    print $fh "

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
P1:  FUNCTION    = MICROWAVE,
	 START       = p1_pos,
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
	 LENGTH      = 10 ns;

";
# === if SHOW_PREV
    if ( eval { $SHOW_PREV } ) {
        print $fh "pulser_show_pulses( );
";
# === endif
    }

    print $fh "
";
# === if ( ADC_CHANNELS eq "0 and 1" )
    if ( eval { ( $ADC_CHANNELS eq "0 and 1" ) } ) {
        print $fh "init_1d( 2, 0, p2_to_p3_dist / 1ns, p2_to_p3_incr / 1 ns,
		 \"Pulse separation [ns]\", \"Echo amplitude [a.u.]\" );
";
# === else
    } else {
        print $fh "init_1d( 1, 0, p2_to_p3_dist / 1ns, p2_to_p3_incr / 1 ns,
		 \"Pulse separation [ns]\", \"Echo amplitude [a.u.]\" );
";
# === endif
    }

    print $fh "

EXPERIMENT:
";
# === if ( TRIG_MODE eq "EXTERNAL" )
    if ( eval { ( $TRIG_MODE eq "EXTERNAL" ) } ) {
        print $fh "print( \"Please make sure the repetition time is set correctly!\\n\" );
";
# === endif
    }

    print $fh "
magnet_field( field );

File = get_file( );

hide_toolbox( \"ON\" );
B1 = output_create( \"INT_OUTPUT\", \"Current P2-P3 separation [ns]\" );
B2 = output_create( \"FLOAT_OUTPUT\", \"Field [G]\", \"%.3f\" );
hide_toolbox( \"OFF\" );

output_value( B1, int( p2_to_p3_dist / 1 ns ) );
output_value( B2, field );

I = 1;
pulser_state( \"ON\" );
daq_gain( 4 );

FOREVER {
	wait( 1.1 * repeat_time * N_Avg );
";
# === if ( ADC_CHANNELS eq "0 and 1" )
    if ( eval { ( $ADC_CHANNELS eq "0 and 1" ) } ) {
        print $fh "	data[ 1 ] = daq_get_voltage( CH0 );
	data[ 2 ] = daq_get_voltage( CH1 );
	display( I, data[ 1 ], 1, I, data[ 2 ], 2 );
	fsave( File, \"#,#,#\\n\",
		   ( p2_to_p3_dist + p2_to_p3_incr * ( I - 1 ) ) / 1 ns,
		   data[ 1 ], data[ 2 ] );
";
# === else
    } else {
        print $fh "";
# === if ( ADC_CHANNELS eq "0" )
    if ( eval { ( $ADC_CHANNELS eq "0" ) } ) {
        print $fh "	data = daq_get_voltage( CH0 );
";
# === else
    } else {
        print $fh "	data = daq_get_voltage( CH1 );
";
# === endif
    }

    print $fh "	display( I, data );
	fsave( File, \"#,#\\n\",
		   ( p2_to_p3_dist + p2_to_p3_incr * ( I - 1 ) ) / 1 ns, data );
";
# === endif
    }

    print $fh "	pulser_shift( );
	pulser_update( );	
    output_value( B1, int( ( p2_to_p3_dist + I * p2_to_p3_incr ) / 1 ns ) );
	I += 1;
}


ON_STOP:

fsave( File,
	   \"\\n\"
	   \"% Date:                   # #\\n\"
	   \"% Script:                 3_pulse_EM (J-band, Leiden)\\n\"
	   \"% Field:                  # G\\n\"
	   \"% Repetition time:        # ms\\n\"
	   \"% Length of 1st MW pulse: # ns\\n\"
	   \"% Length of 2nd MW pulse: # ns\\n\"
	   \"% Length of 3rd MW pulse: # ns\\n\"
	   \"% P1-P2 separation:       # ns\\n\"
	   \"% Init. P2-P3 separation: # ns\\n\"
	   \"% P2-P3 increment:        # ns\\n\"
	   \"% Number of averages:     >= #\\n\"
	   \"% ADC gain:               4\\n\"
	   \"% ADC channel(s):         $ADC_CHANNELS\\n\",
	   date( ), time( ), field,  repeat_time / 1 ms, int( P1.LENGTH / 1ns ),
	   int( P2.LENGTH / 1 ns ), int( P3.LENGTH / 1 ns ),
	   int( p1_to_p2_dist / 1 ns ), int( p2_to_p3_dist / 1 ns ),
	   int( p2_to_p3_incr / 1 ns ), N_Avg );

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

    print $fh "$TRIG_MODE{ value }\n";

    if (     $REP_TIME{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o
         and (     defined $REP_TIME{ max } ? $REP_TIME{ max } >= $REP_TIME{ value } : 1 )
         and ( defined $REP_TIME{ min } ? $REP_TIME{ min } <= $REP_TIME{ value } : 1 ) ) {
        print $fh "$REP_TIME{ value }\n";
    } else {
        print $fh "50\n";
    }

    if (     $P1_POS{ value } =~ /^[+-]?\d+$/o
         and ( defined $P1_POS{ max } ? $P1_POS{ max } >= $P1_POS{ value } : 1 )
         and ( defined $P1_POS{ min } ? $P1_POS{ min } <= $P1_POS{ value } : 1 ) ) {
        print $fh "$P1_POS{ value }\n";
    } else {
        print $fh "200\n";
    }

    if (     $P1_LEN{ value } =~ /^[+-]?\d+$/o
         and ( defined $P1_LEN{ max } ? $P1_LEN{ max } >= $P1_LEN{ value } : 1 )
         and ( defined $P1_LEN{ min } ? $P1_LEN{ min } <= $P1_LEN{ value } : 1 ) ) {
        print $fh "$P1_LEN{ value }\n";
    } else {
        print $fh "100\n";
    }

    if (     $P2_LEN{ value } =~ /^[+-]?\d+$/o
         and ( defined $P2_LEN{ max } ? $P2_LEN{ max } >= $P2_LEN{ value } : 1 )
         and ( defined $P2_LEN{ min } ? $P2_LEN{ min } <= $P2_LEN{ value } : 1 ) ) {
        print $fh "$P2_LEN{ value }\n";
    } else {
        print $fh "200\n";
    }

    if (     $P3_LEN{ value } =~ /^[+-]?\d+$/o
         and ( defined $P3_LEN{ max } ? $P3_LEN{ max } >= $P3_LEN{ value } : 1 )
         and ( defined $P3_LEN{ min } ? $P3_LEN{ min } <= $P3_LEN{ value } : 1 ) ) {
        print $fh "$P3_LEN{ value }\n";
    } else {
        print $fh "200\n";
    }

    if (     $P1_P2_DIST{ value } =~ /^[+-]?\d+$/o
         and ( defined $P1_P2_DIST{ max } ? $P1_P2_DIST{ max } >= $P1_P2_DIST{ value } : 1 )
         and ( defined $P1_P2_DIST{ min } ? $P1_P2_DIST{ min } <= $P1_P2_DIST{ value } : 1 ) ) {
        print $fh "$P1_P2_DIST{ value }\n";
    } else {
        print $fh "400\n";
    }

    if (     $P2_P3_DIST{ value } =~ /^[+-]?\d+$/o
         and ( defined $P2_P3_DIST{ max } ? $P2_P3_DIST{ max } >= $P2_P3_DIST{ value } : 1 )
         and ( defined $P2_P3_DIST{ min } ? $P2_P3_DIST{ min } <= $P2_P3_DIST{ value } : 1 ) ) {
        print $fh "$P2_P3_DIST{ value }\n";
    } else {
        print $fh "400\n";
    }

    if (     $P2_P3_INCR{ value } =~ /^[+-]?\d+$/o
         and ( defined $P2_P3_INCR{ max } ? $P2_P3_INCR{ max } >= $P2_P3_INCR{ value } : 1 )
         and ( defined $P2_P3_INCR{ min } ? $P2_P3_INCR{ min } <= $P2_P3_INCR{ value } : 1 ) ) {
        print $fh "$P2_P3_INCR{ value }\n";
    } else {
        print $fh "20\n";
    }

    if (     $DET_OFFSET{ value } =~ /^[+-]?\d+$/o
         and ( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } >= $DET_OFFSET{ value } : 1 )
         and ( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } <= $DET_OFFSET{ value } : 1 ) ) {
        print $fh "$DET_OFFSET{ value }\n";
    } else {
        print $fh "0\n";
    }

    print $fh "$CREATE_DEFENSE{ value }\n";

    if (     $PULSE_2_DEFENSE{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o
         and (     defined $PULSE_2_DEFENSE{ max } ? $PULSE_2_DEFENSE{ max } >= $PULSE_2_DEFENSE{ value } : 1 )
         and ( defined $PULSE_2_DEFENSE{ min } ? $PULSE_2_DEFENSE{ min } <= $PULSE_2_DEFENSE{ value } : 1 ) ) {
        print $fh "$PULSE_2_DEFENSE{ value }\n";
    } else {
        print $fh "50\n";
    }

    print $fh "$N_AVG{ value }\n";

    if (     $FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o
         and (     defined $FIELD{ max } ? $FIELD{ max } >= $FIELD{ value } : 1 )
         and ( defined $FIELD{ min } ? $FIELD{ min } <= $FIELD{ value } : 1 ) ) {
        print $fh "$FIELD{ value }\n";
    } else {
        print $fh "35000\n";
    }

    print $fh "$ADC_CHANNELS{ value }\n";

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

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @TRIG_MODE ) {
        if ( $ne eq $_ ) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $TRIG_MODE{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if    ( defined $REP_TIME{ max } and $ne > $REP_TIME{ max } )
                         or ( defined $REP_TIME{ min } and $ne < $REP_TIME{ min } );
    $REP_TIME{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P1_POS{ max } and $ne > $P1_POS{ max } )
                         or ( defined $P1_POS{ min } and $ne < $P1_POS{ min } );
    $P1_POS{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P1_LEN{ max } and $ne > $P1_LEN{ max } )
                         or ( defined $P1_LEN{ min } and $ne < $P1_LEN{ min } );
    $P1_LEN{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P2_LEN{ max } and $ne > $P2_LEN{ max } )
                         or ( defined $P2_LEN{ min } and $ne < $P2_LEN{ min } );
    $P2_LEN{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P3_LEN{ max } and $ne > $P3_LEN{ max } )
                         or ( defined $P3_LEN{ min } and $ne < $P3_LEN{ min } );
    $P3_LEN{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P1_P2_DIST{ max } and $ne > $P1_P2_DIST{ max } )
                         or ( defined $P1_P2_DIST{ min } and $ne < $P1_P2_DIST{ min } );
    $P1_P2_DIST{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P2_P3_DIST{ max } and $ne > $P2_P3_DIST{ max } )
                         or ( defined $P2_P3_DIST{ min } and $ne < $P2_P3_DIST{ min } );
    $P2_P3_DIST{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $P2_P3_INCR{ max } and $ne > $P2_P3_INCR{ max } )
                         or ( defined $P2_P3_INCR{ min } and $ne < $P2_P3_INCR{ min } );
    $P2_P3_INCR{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
    goto done_reading if    ( defined $DET_OFFSET{ max } and $ne > $DET_OFFSET{ max } )
                         or ( defined $DET_OFFSET{ min } and $ne < $DET_OFFSET{ min } );
    $DET_OFFSET{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^1|0$/o;
    chomp $ne;
    $CREATE_DEFENSE{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if    ( defined $PULSE_2_DEFENSE{ max } and $ne > $PULSE_2_DEFENSE{ max } )
                         or ( defined $PULSE_2_DEFENSE{ min } and $ne < $PULSE_2_DEFENSE{ min } );
    $PULSE_2_DEFENSE{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @N_AVG ) {
        if ( $ne eq $_ ) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $N_AVG{ value } = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if    ( defined $FIELD{ max } and $ne > $FIELD{ max } )
                         or ( defined $FIELD{ min } and $ne < $FIELD{ min } );
    $FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @ADC_CHANNELS ) {
        if ( $ne eq $_ ) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $ADC_CHANNELS{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^1|0$/o;
    chomp $ne;
    $SHOW_PREV{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @fsc2_how_to_run ) {
        if ( $ne eq $_ ) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $fsc2_how_to_run = $ne;

    goto done_reading unless     defined( $ne = <$fh> )
                             and $ne =~ /^\s*([+-]\d+[+-]\d+)\s*$/;
    $geom = $1;

  done_reading:
    close $fh;
};
