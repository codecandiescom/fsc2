#!/usr/bin/perl -w
# -*- cperl -*-
#
# $Id$
#
# Copyright (C) 1999-2002 Jens Thoms Toerring
#
# This file is part of fsc2.
#
# Fsc2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# Fsc2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with fsc2; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


use strict;
use Tk;
use Tk::Balloon;

my @version = split /\./, $Tk::VERSION;
die "Installed Perl-Tk version is $Tk::VERSION but Tk800.022 is required.\n"
	if $version[ 0 ] + 0.001 * $version[ 1 ] < 800.022;

my $no_ranges = 1;
my $old_no_ranges = 0;
my $how = 'Start experiment';
my $num_averages = 50;
my ( @start_field, @end_field, $field_step, @fs, @fe, @fr, @rsf, @ref,
	 $min_field );


my %fp = ( '-side' => 'top',
		   '-fill' => 'x',
		   '-padx' => '2m',
		   '-pady' => '2m' );

my %wp = ( '-side' => 'left',
		   '-fill' => 'x',
		   '-padx' => '3m',
		   '-expand' => 1 );

# Get defaults

&get_defs;

# Create all the graphic stuff

my $mw = MainWindow->new( );
$mw->title( "tresr" );
my $balloon = $mw->Balloon( );

my $f0 = $mw->Frame( );
my $f0_l1 = $f0->Label( '-text'           => 'Number of field ranges:',
						'-width'          => '20',
						'anchor'          => 'w' );
my $f0_v = $f0->Scale( '-variable'        => \$no_ranges,
                       '-orient'          => 'horizontal',
                       '-from'            => 1,
                       '-to'              => 10,
                       '-resolution'      => 1,
					   '-width'           => 8 );
$balloon->attach( $f0_v, '-balloonmsg'    =>
				         "Adjusts the number of field ranges to measure" );
my $f0_l2 = $f0->Label( '-text'           => ' ',
						'-width'          => 1 );

$f0->pack( %fp );
$f0_l1->pack( %wp );
$f0_v->pack( %wp );
$f0_l2->pack( %wp );

$f0_v->configure( '-command' => \&make_rest );

MainLoop;


#############################################

sub get_defs {

local *F;
my $ne;

if ( $ARGV[ 0 ] ) {
	open( F, "<$ARGV[ 0 ]" ) or return;
} else {
	open( F, "<$ENV{ HOME }/.fsc2/tresr" ) or return;
}

goto done_reading unless defined( $ne = <F> ) and $ne =~ /^#/;

goto done_reading unless defined( $ne = <F> ) and $ne =~ /^\d+$/;
chomp $ne;
$no_ranges = $ne;

for ( my $i = 0; $i < 10; $i++ ) {
    goto done_reading unless defined( $ne = <F> ) and $ne =~ /^\d+$/;
    chomp $ne;
    $start_field[ $i ] = $ne if $ne != 0;

    goto done_reading unless defined( $ne = <F> ) and $ne =~ /^\d+$/;
    chomp $ne;
    $end_field[ $i ] = $ne if $ne != 0;
}

goto done_reading unless defined( $ne = <F> ) and is_numeric( $ne );
chomp $ne;
$field_step = $ne if $ne eq "." or $ne != 0;

goto done_reading unless defined( $ne = <F> ) and $ne =~ /^\d+$/;
chomp $ne;
$num_averages = $ne if $ne != 0;

done_reading:
close F;
}


#############################################

sub store_defs {
	local *F;

	mkdir "$ENV{ HOME }/.fsc2", 0777 unless -e "$ENV{ HOME }/.fsc2";
	open( F, ">$ENV{ HOME }/.fsc2/tresr" ) or return;

	print F "# Do not edit - created automatically!\n";

	print F "$no_ranges\n";
    for ( my $i = 0; $i < 10; $i++ ) {
        if ( defined( $start_field[ $i ] ) ) {
            print F "$start_field[ $i ]\n";
        } else {
            print F "0\n";
        }
        if ( defined ( $end_field[ $i ] ) ) {
            print F "$end_field[ $i ]\n";
        } else {
            print F "0\n";
        }
    }

    if ( defined ( $field_step ) ) {
        print F "$field_step\n";
    } else {
        print F "0\n";
    }

    print F "$num_averages\n";

	close F;
}


#############################################

sub is_numeric {

	my $new_val = shift;
	$new_val =~ /^((\d+\.?(\d+)?)|(\.(\d+)?))?$/;
}


#############################################

sub write_out {

return if &do_checks( ) != 0;

local *F;
my $text;

open( F, "|fsc2_" . lc $how )
	or die "Can't find utility fsc2_" . lc $how . ".\n";

print F
"DEVICES:\n\n";
print F "lecroy9400;
er032m;


VARIABLES:

";

if ( $no_ranges > 1 ) {
    print F "start_field[ $no_ranges ] = { ";
    for ( my $i = 0; $ i < $no_ranges - 1; $i++ ) {
        print F $rsf[ $i ] . " G, ";
    }
    print F $rsf[ $no_ranges - 1 ] . " G };\n";

    print F "end_field[ $no_ranges ]   = { ";
    for ( my $i = 0; $ i < $no_ranges - 1; $i++ ) {
        print F $ref[ $i ] . " G, ";
    }
    print F $ref[ $no_ranges - 1 ] . " G };\n";
} else {
    print F "start_field = $rsf[ 0 ] G;
end_field   = $ref[ 0 ] G;\n";
}

print F "field_step  = " . abs( $field_step ) . " G;

field";
print F " = start_field" if $no_ranges == 1;
print F ";\n";
print F "min_field = $min_field G;\n" if $no_ranges > 1;
print F "data[ * ];\n";
print F "I = 1;\n" if $no_ranges == 1;
print F "J, K, F1, F2;


PREPARATIONS:

init_2d( 1, 0, 0, 0, 0, ";
if ( $no_ranges > 1 ) {
	print F "min_field" ;
} else {
	print F "start_field";
}
print F ", field_step, \"Time [us]\", \"Field [G]\" );

magnet_setup( ";
if ( $no_ranges > 1 ) {
	print F "min_field";
} else {
	print F "start_field";
}
print F ", field_step );
digitizer_averaging( FUNC_E, CH1, $num_averages );


EXPERIMENT:

change_scale( 0.0, 1.0e6 * digitizer_time_per_point( ) );

F1 = get_file( \"Enter data file name:\", \"*.dat\", \"\", \"\", \"dat\" );
F2 = clone_file( F1, \"dat\", \"par\" );

save_comment( F2 );
";

if ( $no_ranges == 1 ) {
    print F "fsave( F2, \"Start field = # G\\n\", start_field );
fsave( F2, \"End field   = # G\\n\", end_field );\n";
} else {
    print F "FOR K = 1 : $no_ranges {
	fsave( F2, \"Start field[ # ] = # G\\n\", K, start_field[ K ] );
	fsave( F2, \"End field[ # ]   = # G\\n\", K, end_field[ K ] );
}\n";
}
print F "fsave( F2, \"Field step  = # G\\n\", field_step );
fsave( F2, \"Time scale  = # us\\n\", digitizer_timebase( ) * 1.0e6 );
save_program( F2 );\n\n";


if ( $no_ranges > 1 ) {
    print F "FOR K = 1 : $no_ranges {
	set_field( start_field[ K ] );
	field = start_field[ K ];
	WHILE field <= end_field[ K ] {
		digitizer_start_acquisition( );
		data = digitizer_get_curve( FUNC_E );
		display( 1, round( ( field - min_field ) / field_step ) + 1,
			     data );
	    field = sweep_up( );
		FOR J = 1 : size( data ) {
		    fsave( F1, \"# \", data[ J ] );
		}
		fsave( F1, \"\\n\" );
	}
}\n";
} else {
    print F "WHILE field <= end_field {
	digitizer_start_acquisition( );
	data = digitizer_get_curve( FUNC_E );
	field = sweep_up( );
	FOR J = 1 : size( data ) {
		fsave( F1, \"# \", data[ J ] );
	}
	fsave( F1, \"\\n\" );
	display( 1, I, data );
	I +=1;
}\n";
}

	close F;

	# Notify the user if sending the program to fsc2 failed for some reasons

	if ( $? != 0 ) {
		if ( $? >> 8 == 255 ) {
			$text = "Internal error.";
		} elsif ( $? >> 8 == 1 ) {
			$text = "Someone else is running fsc2.";
		} elsif ( $? >> 8 == 2 ) {
			$text = "fsc2 is currently busy.";
		} elsif ( $? >> 8 == 3 ) {
			$text = "Internal error of fsc2.";
		} elsif ( $? >> 8 == 4 ) {
			$text = "Could not start fsc2.";
		} else {
			$text = "Something strange is going on here.";
		}

		&show_message( $text ) if $? >> 8 != 0;
	}
}


#############################################

sub do_checks {

	$min_field = 1e9;

    if ( ! defined( $field_step ) or $field_step =~ /^\.?$/ ) {
        &show_message( "Field step size hasn't been set." );
        return -1;
    }

    for ( my $i = 0; $i < $no_ranges; $i++ ) {

        if ( ! defined( $start_field[ $i ] ) or
			 $start_field[ $i ] =~ /^\.?$/ ) {
            &show_message( "Start field #" . ( $i + 1 ) .
                           " hasn't been set." );
            return -1;
        }

        if ( $start_field[ $i ]  < 1460 ) {
            &show_message( "Start field #" . ( $i + 1 ) . " is too low." );
            return -1;
        }

        if ( $start_field[ $i ] > 19900 ) {
            &show_message( "Start field #" . ( $i + 1 ) . " is too high." );
        }

        if ( ! defined( $end_field[ $i ] ) or
			 $end_field[ $i ] =~ /^\.?$/ ) {
            &show_message( "End field #" . ( $i + 1 ) . " hasn't been set." );
            return -1;
        }

        if ( $end_field[ $i ] < 1460 ) {
            &show_message( "End field #" . ( $i + 1 ) . " is too low." );
            return -1;
        }

        if ( $end_field[ $i ] > 19900 ) {
            &show_message( "End field #" . ( $i + 1 ) . " is too high." );
            return -1;
        }

		if ( $start_field[ $i ] <= $end_field[ $i ] ) {
			$rsf[ $i ] = $start_field[ $i ];
			$ref[ $i ] = $end_field[ $i ];
		} else {
			$ref[ $i ] = $start_field[ $i ];
			$rsf[ $i ] = $end_field[ $i ];
		}

		if ( abs( $field_step ) > $ref[ $i ] - $rsf[ $i ] ) {
			if ( $no_ranges == 1 ) {
				&show_message( "Field step size larger than\n" .
							   "difference between start and\n" .
							   "end field." );
			} else {
				&show_message( "Field step size larger than\n" .
							   "difference between start and\n" .
							   "end of " . ( $i + 1 ) . ". field range." );
			}
			return -1;
		}

		$min_field = $rsf[ $i ] if $rsf[ $i ] < $min_field;
	}

    for ( my $i = 0; $i < $no_ranges; $i++ ) {
        for ( my $j = 0; $j < $no_ranges; $j++ ) {
            next if $i == $j;

			if ( $rsf[ $i ] > $rsf[ $j ] and $rsf[ $i ] < $ref[ $j ] ) {
				&show_message( "Field ranges " . ( $i + 1 ) . " and " .
							   ( $j + 1 ) . " overlap." );
				return -1;
			}

			if ( $ref[ $i ] > $rsf[ $j ] and $ref[ $i ] < $ref[ $j ] ) {
				&show_message( "Field ranges " . ( $i + 1 ) . " and " .
							   ( $j + 1 ) . " overlap." );
				return -1;
			}
		}

    }

	return 0;
}


#############################################

sub show_message {
	my $text = shift;

	$mw->messageBox( '-icon' => 'error',
					 '-type' => 'Ok',
					 '-title' => 'Error',
					 '-message' => $text );
}


#############################################

sub make_rest {

    for ( my $i = 9; $i >= 0; $i-- ) {
        $fr[ $i ]->destroy if defined $fr[ $i ];
    }

    for ( my $i = $old_no_ranges - 1; $i >= 0; $i-- ) {
        $fe[ $i ]->destroy if defined $fe[ $i ];
        $fs[ $i ]->destroy if defined $fs[ $i ];
    }

    for ( my $i = 0; $i < $no_ranges; $i++ ) {
        $fs[ $i ] = $mw->Frame( );
        my $fs_l1 = $fs[ $i ]->Label( '-text'           =>
                                      "Start field " . ($i + 1) . " :",
                                      '-width'          => '20',
                                      'anchor'          => 'w' );
        my $fs_v = $fs[ $i ]->Entry( '-textvariable'    => \$start_field[ $i ],
                                     '-width'           => '8',
                                     '-validate'        => 'key',
                                     '-validatecommand' => \&is_numeric,
                                     '-relief'          => 'sunken' );
        $balloon->attach( $fs_v, '-balloonmsg'    =>
                          "Enter start field of ". ($i + 1) .
                          ". range (in Gauss)" );
        my $fs_l2 = $fs[ $i ]->Label( '-text'           => 'G',
                                      '-width'          => 1 );

        $fs[ $i ]->pack( %fp );
        $fs_l1->pack( %wp );
        $fs_v->pack( %wp );
        $fs_l2->pack( %wp );

        $fe[ $i ] = $mw->Frame( );
        my $fe_l1 = $fe[ $i ]->Label( '-text'           => 
                                      "End field " . ($i + 1) . ":",
                                      '-width'          => '20',
                                      'anchor'          => 'w' );
        my $fe_v = $fe[ $i ]->Entry( '-textvariable'    => \$end_field[ $i ],
                                     '-width'           => '8',
                                     '-validate'        => 'key',
                                     '-validatecommand' => \&is_numeric,
                                     '-relief'          => 'sunken' );
        $balloon->attach( $fe_v, '-balloonmsg'    =>
                          "Enter end field of ". ($i + 1) .
                          ". range (in Gauss)" );
        my $fe_l2 = $fe[ $i ]->Label( '-text'           => 'G',
                                      '-width'          => 1 );

        $fe[ $i ]->pack( %fp );
        $fe_l1->pack( %wp );
        $fe_v->pack( %wp );
        $fe_l2->pack( %wp );
    }

    $old_no_ranges = $no_ranges;

    $fr[ 0 ] = $mw->Frame( );
    $fr[ 1 ] = $fr[ 0 ]->Label( '-text'           => 'Field step size:',
                            '-width'          => '20',
                            'anchor'          => 'w' );
    $fr[ 2 ] = $fr[ 0 ]->Entry( '-textvariable'    => \$field_step,
                                '-width'           => '8',
                                '-validate'        => 'key',
                                '-validatecommand' => \&is_numeric,
                                '-relief'          => 'sunken' );
    $balloon->attach( $fr[ 2 ], '-balloonmsg'    =>
                      "Enter the size of the field steps (in Gauss)" );
    $fr[ 3 ] = $fr[ 0 ]->Label( '-text'           => 'G',
                                '-width'          => 1 );

    $fr[ 0 ]->pack( %fp );
    $fr[ 1 ]->pack( %wp );
    $fr[ 2 ]->pack( %wp );
    $fr[ 3 ]->pack( %wp );

    $fr[ 4 ] = $mw->Frame( );
    my $fr4_l = $fr[ 4 ]->Label( '-text'      => 'Number of averages:',
                                 '-width'     => '20',
                                 'anchor'     => 'w' );
    my $fr4_m = $fr[ 4 ]->Optionmenu( '-options' => 
                                      [ ( 10, 20, 50, 100, 200, 500,
                                          1000, 2000, 5000,
                                          10000, 20000, 50000,
                                          100000, 200000, 500000,
                                          1000000 ) ],
                                      '-textvariable' => \$num_averages );
    $balloon->attach( $fr4_m, '-balloonmsg'    =>
                      "Adjusts the number of averages\n" .
                      "done during the experiment" );
    $fr[ 4 ]->pack( %fp );
    $fr4_l->pack( %wp );
    $fr4_m->pack( %wp );

    $fr[ 5 ] = $mw->Optionmenu( '-options' => [ ( 'Start experiment',
                                                  'Test program',
                                                  'Load into fsc2' ) ],
                                '-textvariable' => \$how,
                              )->pack( '-padx' => '3m',
                                       '-pady' => '3m' );

    $fr[ 6 ] = $mw->Frame( );
    $fr[ 7 ] = $mw->Button( '-text' => 'Apply',
                             '-command' => \&write_out );
    $fr[ 8 ] = $mw->Button( '-text' => 'Quit',
                            '-command' => sub { &store_defs; $mw->destroy } );
    $fr[ 6 ]->pack( '-side' => 'top',
                    '-fill' => 'x',
                    '-padx' => '4m' );
    $fr[ 7 ]->pack( %wp, 'padx' => '5m', '-pady' => '3m' );
    $fr[ 8 ]->pack( %wp, 'padx' => '5m', '-pady' => '3m' );
}
