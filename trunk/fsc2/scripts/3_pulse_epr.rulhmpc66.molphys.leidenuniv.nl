#!/usr/bin/perl
# -*- cperl -*-

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

# === P2_P3_DIST int [ 100 : 167772150 ] [ 400 ] "P2-P3 distance" "ns"

my %P2_P3_DIST;
$P2_P3_DIST{ tk_frame } = $fsc2_main_frame->Frame( );
$P2_P3_DIST{ tk_label } = $P2_P3_DIST{ tk_frame }->Label( -text => "P2-P3 distance",
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

# === START_FIELD float [ 0 : 114304 ] [ 8000 ] "Start field" "G"

my %START_FIELD;
$START_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$START_FIELD{ tk_label } = $START_FIELD{ tk_frame }->Label( -text => "Start field",
-width => 20,
-anchor => 'w' );
$START_FIELD{ value } = 8000;
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

# === END_FIELD float [ 0 : 114304 ] [ 9000 ] "End field" "G"

my %END_FIELD;
$END_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$END_FIELD{ tk_label } = $END_FIELD{ tk_frame }->Label( -text => "End field",
-width => 20,
-anchor => 'w' );
$END_FIELD{ value } = 9000;
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

# === WAIT_TIME float [ 0.0 : ] [ 5.0 ] "Magnet wait time" "s"

my %WAIT_TIME;
$WAIT_TIME{ tk_frame } = $fsc2_main_frame->Frame( );
$WAIT_TIME{ tk_label } = $WAIT_TIME{ tk_frame }->Label( -text => "Magnet wait time",
-width => 20,
-anchor => 'w' );
$WAIT_TIME{ value } = 5.0;
$WAIT_TIME{ min } = 0;
$WAIT_TIME{ tk_entry } = $WAIT_TIME{ tk_frame }->Entry( -textvariable => \$WAIT_TIME{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } : undef ),
( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $WAIT_TIME{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } : '-inf' ) .
" : " . ( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } : '+inf' ) . " ]" );
$WAIT_TIME{ tk_unit } = $WAIT_TIME{ tk_frame }->Label( -text => "s",
-width => 5 );
$WAIT_TIME{ tk_frame }->pack( %fp );
$WAIT_TIME{ tk_label }->pack( %wp );
$WAIT_TIME{ tk_entry }->pack( %wp );
$WAIT_TIME{ tk_unit  }->pack( %up );

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

    my $TRIG_MODE = $TRIG_MODE{ value };
    my $REP_TIME = $REP_TIME{ value };
    my $P1_POS = $P1_POS{ value };
    my $P1_LEN = $P1_LEN{ value };
    my $P2_LEN = $P2_LEN{ value };
    my $P3_LEN = $P3_LEN{ value };
    my $P1_P2_DIST = $P1_P2_DIST{ value };
    my $P2_P3_DIST = $P2_P3_DIST{ value };
    my $DET_OFFSET = $DET_OFFSET{ value };
    my $N_AVG = $N_AVG{ value };
    my $START_FIELD = $START_FIELD{ value };
    my $END_FIELD = $END_FIELD{ value };
    my $FIELD_STEP = $FIELD_STEP{ value };
    my $SHOW_PREV = $SHOW_PREV{ value };
	my $WAIT_TIME = $WAIT_TIME{ value };

    print $fh "DEVICES:

ips120_10;
itc503;
rb8509;
rb_pulser;


VARIABLES:

repeat_time   = $REP_TIME ms;
p1_pos        = $P1_POS ns;
p1_to_p2_dist = $P1_P2_DIST ns;
p2_to_p3_dist = $P2_P3_DIST ns;
p1_len        = $P1_LEN ns;
p2_len        = $P2_LEN ns;
p3_len        = $P3_LEN ns;
det_offset    = $DET_OFFSET ns;

start_field   = $START_FIELD G;
end_field     = $END_FIELD G;
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "field_step    = $FIELD_STEP G;
";
# === else
    } else {
        print $fh "field_step    = - $FIELD_STEP G;
";
# === endif
    }

    print $fh "field;

N_Avg    = $N_AVG;
N_Points = ceil( ( end_field - start_field ) / field_step ) + 1;
wait_time = $WAIT_TIME s;
I;
J = 0;
K;
data[ *, * ];
avg[ N_Points ] = float_slice( N_Points );
File1, File2;
B1, B2, B3, B4;
start_temp;


ASSIGNMENTS:
";
	if ( $TRIG_MODE{ value } eq "EXTERNAL" ) {
	    print $fh "TRIGGER_MODE: EXTERNAL;\n\n";
	} else {
		print $fh "TRIGGER_MODE: $TRIG_MODE, REPEAT_TIME = repeat_time;\n\n";
	}

	print $fh "
PREPARATIONS:

P1:  FUNCTION = MICROWAVE,
	 START    = p1_pos,
	 LENGTH   = p1_len;

P2:  FUNCTION = MICROWAVE,
	 START    = P1.START + p1_to_p2_dist + 0.5 * ( P1.LENGTH - p2_len ),
	 LENGTH   = p2_len;

P3:  FUNCTION = MICROWAVE,
	 START    = P2.START + p2_to_p3_dist + 0.5 * ( P2.LENGTH - p3_len ),
	 LENGTH   = p3_len;

P4:  FUNCTION = DETECTION,
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
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "init_1d( 2, N_Points, start_field, field_step,
		 \"Field [G]\", \"Echo amplitude [a.u.]\" );
";
# === else
    } else {
        print $fh "init_1d( 2, N_Points, end_field, - field_step,
		 \"Field [G]\", \"Echo amplitude [a.u.]\" );
";
# === endif
    }

    print $fh "

EXPERIMENT:
";

	if ( $TRIG_MODE{ value } eq "EXTERNAL" ) {
		print $fh "print( \"Please make sure the repetition time is set correctly!\\n\\n\" );
"
	}

	print $fh "
pulser_state( \"ON\" );
start_temp = temp_contr_temperature( );

field = set_field( start_field );
IF wait_time > 1 us {
	wait( wait_time );
}

/* Open the file for averaged data */

File1 = get_file( \"\", \"*.avg\", \"\", \"\", \"avg\" );

/* Create the toolbox with two output field, one for the current scan number
   and one for the current field as well as a push button for stopping the
   experiment at the end of a scan */

hide_toolbox( \"ON\" );
B1 = output_create( \"INT_OUTPUT\", \"Current scan\" );
B2 = output_create( \"FLOAT_OUTPUT\", \"Current field [G]\", \"%.3f\" );
B3 = output_create( \"FLOAT_OUTPUT\", \"Current temperature [K]\", \"%.1f\" );
B4 = button_create( \"PUSH_BUTTON\", \"Stop after end of scan\" );
hide_toolbox( \"OFF\" );


FOREVER {

	J += 1;
	output_value( B1, J );	              // Update the scan count display

	FOR I = 1 : N_Points {
		output_value( B2, field );
		output_value( B3, temp_contr_temperature( ) );
		wait( 1.1 * repeat_time * N_Avg );
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "
		data[ J, I ] = - daq_get_voltage( CH0 );
		display_1d( I, data[ J, I ], 1,
                    I, ( ( J - 1 ) * avg[ I ] + data[ J, I ] ) / J, 2 );
";
# === else
    } else {
        print $fh "
		data[ J, N_Points - I + 1 ] = - daq_get_voltage( CH0 );
		display_1d( N_Points - I + 1, data[ J, N_Points - I + 1 ], 1,
                    N_Points - I + 1,
                    ( ( J - 1 ) * avg[ N_Points - I + 1 ]
                      + data[ J, N_Points - I + 1 ] ) / J, 2 );
";
# === endif
    }

# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "		IF I < N_Points {
";
# === else
    } else {
        print $fh "		IF I > 1 {
";
# === endif
    }

    print $fh "			field += field_step;
			set_field( field );
		}
	}

	avg = add_to_average( avg, data[ J ], J );

	IF button_state( B4 ) {               // Stop on user request
		BREAK;
	}

	field = set_field( start_field );
	IF wait_time > 1 us {
		wait( wait_time );
	}
}

ON_STOP:

IF I != 0 {
	IF J == 1 {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "		FOR K = 1 : I - 1 {
			fsave( File1, \"#,#\\n\", start_field + ( K - 1 ) * field_step,
				   data[ 1, K ] );
		}
";
# === else
    } else {
        print $fh "		FOR K = N_Points - I + 2 : N_Points {
			fsave( File1, \"#,#\\n\", end_field + ( K - 1 ) * field_step,
				   data[ 1, K ] );
		}
";
	}
# === endif
	print $fh "		fsave( File1, \"\\n\" );
	} ELSE {
		IF I <= N_Points {
			J -= 1;
		}

		IF J > 1 {
			File2 = clone_file( File1, \"avg\", \"scans\" );
			FOR I = 1 : N_Points {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "	 		fsave( File2, \"#\", start_field + ( I - 1 ) * field_step );
";
# === else
    } else {
		print $fh "	 		fsave( File2, \"#\", end_field + ( I - 1 ) * field_step );
";
	}
	print $fh "			FOR K = 1 : J {
					fsave( File2, \",#\", data[ K, I ] );
				}
				fsave( File2, \"\\n\" );
			}

			fsave( File2, \"\\n\" );
		}

		FOR I = 1 : N_Points {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "			fsave( File1, \"#,#\\n\", start_field + ( I - 1 ) * field_step, avg[ I ] );
";
# === else
    } else {
		print $fh "			fsave( File1, \"#,#\\n\", end_field + ( I - 1 ) * field_step, avg[ I ] );
";
	}
	print $fh "		}
		fsave( File1, \"\\n\" );
	}

	fsave( File1,
	       \"% Date:                   # #\\n\"
	       \"% Script:                 3_pulse_epr\\n\"
	       \"% Start field:            # G\\n\"
	       \"% End field:              # G\\n\"
	       \"% Field step:             # G\\n\"
	       \"% Repetition time:        # ms\\n\"
	       \"% Length of 1st MW pulse: # ns\\n\"
	       \"% Length of 2nd MW pulse: # ns\\n\"
	       \"% Length of 3rd MW pulse: # ns\\n\"
	       \"% P1-P2 separation:       # ns\\n\"
	       \"% P2-P3 separation:       # ns\\n\"
	       \"% Number of averages:     #\\n\"
		   \"% Number of scans:        #\\n\"
	       \"% ADC gain:               4\\n\"
	       \"% Temperature at start:   # K\\n\"
    	   \"% Temperature at end:     # K\\n\",
		   date( ), time( ), start_field, field, field_step,
		   repeat_time * 1.0e3, int( P1.LENGTH * 1.0e9 ), int( P2.LENGTH * 1.0e9 ),
		   int( P3.LENGTH * 1.0e9 ), int( p1_to_p2_dist * 1.0e9 ),
	       int( p2_to_p3_dist * 1.0e9 ), J, N_Avg, start_temp, temp_contr_temperature( ) );

	save_comment( File1, \"% \" );

	IF J > 1 {
		fsave( File2,
		       \"% Date:                   # #\\n\"
		       \"% Script:                 3_pulse_epr\\n\"
		       \"% Start field:            # G\\n\"
		       \"% End field:              # G\\n\"
		       \"% Field step:             # G\\n\"
		       \"% Repetition time:        # ms\\n\"
		       \"% Length of 1st MW pulse: # ns\\n\"
		       \"% Length of 2nd MW pulse: # ns\\n\"
		       \"% Length of 3rd MW pulse: # ns\\n\"
		       \"% P1-P2 separation:       # ns\\n\"
		       \"% P2-P3 separation:       # ns\\n\"
		       \"% Number of averages:     #\\n\"
			   \"% Number of scans:        #\\n\"
		       \"% ADC gain:               4\\n\"
		       \"% Temperature at start:   # K\\n\"
	    	   \"% Temperature at end:     # K\\n\",
			   date( ), time( ), start_field, field, field_step,
			   repeat_time * 1.0e3, int( P1.LENGTH * 1.0e9 ),
			   int( P2.LENGTH * 1.0e9 ), int( P3.LENGTH * 1.0e9 ),
			   int( p1_to_p2_dist * 1.0e9 ), int( p2_to_p3_dist * 1.0e9 ),
			   J, N_Avg, start_temp, temp_contr_temperature( ) );
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

    print $fh "$TRIG_MODE{ value }\n";

    if ( $REP_TIME{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $REP_TIME{ max } ? $REP_TIME{ max } >= $REP_TIME{ value } : 1 ) and
         ( defined $REP_TIME{ min } ? $REP_TIME{ min } <= $REP_TIME{ value } : 1 ) ) {
        print $fh "$REP_TIME{ value }\n";
    } else {
        print $fh "50\n";
    }

    if ( $P1_POS{ value } =~ /^[+-]?\d+$/o and
         ( defined $P1_POS{ max } ? $P1_POS{ max } >= $P1_POS{ value } : 1 ) and
         ( defined $P1_POS{ min } ? $P1_POS{ min } <= $P1_POS{ value } : 1 ) ) {
        print $fh "$P1_POS{ value }\n";
    } else {
        print $fh "200\n";
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

    if ( $DET_OFFSET{ value } =~ /^[+-]?\d+$/o and
         ( defined $DET_OFFSET{ max } ? $DET_OFFSET{ max } >= $DET_OFFSET{ value } : 1 ) and
         ( defined $DET_OFFSET{ min } ? $DET_OFFSET{ min } <= $DET_OFFSET{ value } : 1 ) ) {
        print $fh "$DET_OFFSET{ value }\n";
    } else {
        print $fh "0\n";
    }

    print $fh "$N_AVG{ value }\n";

    if ( $START_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $START_FIELD{ max } ? $START_FIELD{ max } >= $START_FIELD{ value } : 1 ) and
         ( defined $START_FIELD{ min } ? $START_FIELD{ min } <= $START_FIELD{ value } : 1 ) ) {
        print $fh "$START_FIELD{ value }\n";
    } else {
        print $fh "8000\n";
    }

    if ( $END_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $END_FIELD{ max } ? $END_FIELD{ max } >= $END_FIELD{ value } : 1 ) and
         ( defined $END_FIELD{ min } ? $END_FIELD{ min } <= $END_FIELD{ value } : 1 ) ) {
        print $fh "$END_FIELD{ value }\n";
    } else {
        print $fh "9000\n";
    }

    if ( $FIELD_STEP{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } >= $FIELD_STEP{ value } : 1 ) and
         ( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } <= $FIELD_STEP{ value } : 1 ) ) {
        print $fh "$FIELD_STEP{ value }\n";
    } else {
        print $fh "1\n";
    }

    print $fh "$SHOW_PREV{ value }\n";

    if ( $WAIT_TIME{ value } =~ /^((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } >= $WAIT_TIME{ value } : 1 ) and
         ( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } <= $WAIT_TIME{ value } : 1 ) ) {
        print $fh "$WAIT_TIME{ value }\n";
    } else {
        print $fh "5.0\n";
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
	my $got_args = 0;


	if ( @ARGV == 7 ) {
		$got_args = 1;
		foreach ( @ARGV ) {
			unless ( /^-?\d+$/ ) {
				$got_args = 0;
				last;
			}
		}
	}

    $name =~ s|^.*?([^/]+)$|$1|;

	if ( $got_args ) {
		unless ( open $fh, "<$ENV{ HOME }/.fsc2/$name" ) {
			$ne = $ARGV[ 0 ];
			return  if ( defined $P1_POS{ max } and $ne > $P1_POS{ max } ) or
				       ( defined $P1_POS{ min } and $ne < $P1_POS{ min } );
			$P1_POS{ value } = $ne;

			$ne = $ARGV[ 1 ];
			return  if ( defined $P1_LEN{ max } and $ne > $P1_LEN{ max } ) or
				       ( defined $P1_LEN{ min } and $ne < $P1_LEN{ min } );
			$P1_LEN{ value } = $ne;

			$ne = $ARGV[ 2 ];
			return if ( defined $P2_LEN{ max } and $ne > $P2_LEN{ max } ) or
				      ( defined $P2_LEN{ min } and $ne < $P2_LEN{ min } );
			$P2_LEN{ value } = $ne;

			$ne = $ARGV[ 3 ];
			return if ( defined $P3_LEN{ max } and $ne > $P3_LEN{ max } ) or
				      ( defined $P3_LEN{ min } and $ne < $P3_LEN{ min } );
			$P3_LEN{ value } = $ne;

			$ne = $ARGV[ 4 ];
			return if ( defined $P1_P2_DIST{ max } and $ne > $P1_P2_DIST{ max } ) or
                      ( defined $P1_P2_DIST{ min } and $ne < $P1_P2_DIST{ min } );
			$P1_P2_DIST{ value } = $ne;

			$ne = $ARGV[ 5 ];
			return if ( defined $P2_P3_DIST{ max } and $ne > $P2_P3_DIST{ max } ) or
                      ( defined $P2_P3_DIST{ min } and $ne < $P2_P3_DIST{ min } );
			$P2_P3_DIST{ value } = $ne;

			$ne = $ARGV[ 6 ];
			return if ( defined $DET_OFFSET{ max } and $ne > $DET_OFFSET{ max } ) or
                      ( defined $DET_OFFSET{ min } and $ne < $DET_OFFSET{ min } );
			$DET_OFFSET{ value } = $ne;
			return;
		}
	} else {
		if ( $ARGV[ 0 ] ) {
			open( $fh, "<$ARGV[ 0 ]" ) or return;
		} else {
			open( $fh, "<$ENV{ HOME }/.fsc2/$name" ) or return;
		}
	}

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^#/;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @TRIG_MODE ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $TRIG_MODE{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $REP_TIME{ max } and $ne > $REP_TIME{ max } ) or
                         ( defined $REP_TIME{ min } and $ne < $REP_TIME{ min } );
    $REP_TIME{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 0 ] if $got_args;
    goto done_reading if ( defined $P1_POS{ max } and $ne > $P1_POS{ max } ) or
                         ( defined $P1_POS{ min } and $ne < $P1_POS{ min } );
    $P1_POS{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 0 ] if $got_args;
    goto done_reading if ( defined $P1_LEN{ max } and $ne > $P1_LEN{ max } ) or
                         ( defined $P1_LEN{ min } and $ne < $P1_LEN{ min } );
    $P1_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 1 ] if $got_args;
    goto done_reading if ( defined $P2_LEN{ max } and $ne > $P2_LEN{ max } ) or
                         ( defined $P2_LEN{ min } and $ne < $P2_LEN{ min } );
    $P2_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 2 ] if $got_args;
    goto done_reading if ( defined $P3_LEN{ max } and $ne > $P3_LEN{ max } ) or
                         ( defined $P3_LEN{ min } and $ne < $P3_LEN{ min } );
    $P3_LEN{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 3 ] if $got_args;
    goto done_reading if ( defined $P1_P2_DIST{ max } and $ne > $P1_P2_DIST{ max } ) or
                         ( defined $P1_P2_DIST{ min } and $ne < $P1_P2_DIST{ min } );
    $P1_P2_DIST{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 4 ] if $got_args;
    goto done_reading if ( defined $P2_P3_DIST{ max } and $ne > $P2_P3_DIST{ max } ) or
                         ( defined $P2_P3_DIST{ min } and $ne < $P2_P3_DIST{ min } );
    $P2_P3_DIST{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?\d+$/;
    chomp $ne;
	$ne = $ARGV[ 5 ] if $got_args;
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

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^1|0$/o;
    chomp $ne;
    $SHOW_PREV{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $WAIT_TIME{ max } and $ne > $WAIT_TIME{ max } ) or
                         ( defined $WAIT_TIME{ min } and $ne < $WAIT_TIME{ min } );
    $WAIT_TIME{ value } = $ne;

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
