#!/usr/bin/perl
# -*- cperl -*-
#
# $Id$
#
# Copyright (C) 1999-2005 Jens Thoms Toerring
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
use warnings;
use Tk;


my @version = split /\./, $Tk::VERSION;
die "Installed Perl-Tk version is $Tk::VERSION but Tk800.022 is required.\n"
    if $version[ 0 ] + 0.001 * $version[ 1 ] < 800.022;

my ( @f, @b, @p );
my %fp = ( '-side' => 'top',
		   '-fill' => 'x',
		   '-padx' => '2m',
		   '-pady' => '2m' );
my $use_data = 1;

@p = ( 200, 50, 400, 100, 600, 50, 300, 50, 575, 10  );

my $mw = MainWindow->new( );
$mw->title( "Pulsed Experiments" );

$f[ 0 ] = $mw->Frame( );
$b[ 0 ] = $f[ 0 ]->Button( '-text' => "Pulse Monitor" );
$b[ 1 ] = $f[ 0 ]->Checkbutton( '-text' => "Use monitor data" );

$f[ 1 ] = $mw->Frame(  );
$b[ 2 ] = $f[ 1 ]->Button( '-text' => "2 Pulse EPR" );
$b[ 3 ] = $f[ 1 ]->Button( '-text' => "2 Pulse T2" );
$b[ 4 ] = $f[ 1 ]->Button( '-text' => "3 Pulse EPR" );
$b[ 5 ] = $f[ 1 ]->Button( '-text' => "3 Pulse T1" );
$b[ 6 ] = $f[ 1 ]->Button( '-text' => "3 Pulse EM" );
$b[ 7 ] = $f[ 1 ]->Button( '-text' => "3 Pulse ENDOR" );

$f[ 2 ] = $mw->Frame(  );
$b[ 8 ] = $f[ 2 ]->Button( '-text' => "Quit",
								   '-command' => sub { $mw->destroy } );


$b[ 0 ]->configure( '-command' => sub { &run_monitor } );
$b[ 1 ]->configure( '-state' => "disabled",
				    '-variable' => \$use_data );

$b[ 2 ]->configure( '-command' => sub { &run_2_pulse_epr } );
$b[ 3 ]->configure( '-command' => sub { &run_2_pulse_t2 } );
$b[ 4 ]->configure( '-command' => sub { &run_3_pulse_epr } );
$b[ 5 ]->configure( '-command' => sub { &run_3_pulse_t1 } );
$b[ 6 ]->configure( '-command' => sub { &run_3_pulse_em } );
$b[ 7 ]->configure( '-command' => sub { &run_3_pulse_endor } );


$_->pack( %fp ) foreach @f;
$_->pack( %fp ) foreach @b;

MainLoop;


sub run_monitor {
	$mw->withdraw;
	open my $f, "pulse_monitor|" or die "Can't start pulse monitor script.\n";
	while ( <$f> ) {
		/^(\d+)\s+(\d+)$/;
		push @p, $1;
		push @p, $2;
	}
	close $f;
	$b[ 1 ]->configure( 'state' => "normal" ) if @p;
	$mw->deiconify;
}

sub run_2_pulse_epr {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $det_offset = $p[ 8 ] - $p[ 2 ] - $dist - 0.5 * $p[ 3 ];
		open $f, "2_pulse_epr $p[ 1 ] $p[ 3 ] $dist $det_offset|"
			or die "Can't start 2 pulse EPR script.\n";
	} else {
		open $f, "2_pulse_epr|" or die "Can't start 2 pulse EPR script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_2_pulse_t2 {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $det_offset = $p[ 8 ] - $p[ 2 ] - $dist - 0.5 * $p[ 3 ];
		open $f, "2_pulse_T2 $p[ 1 ] $p[ 3 ] $dist $det_offset|"
			or die "Can't start 2 pulse T2 script.\n";
	} else {
		open $f, "2_pulse_T2|" or die "Can't start 2 pulse T2 script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_3_pulse_epr {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist12 = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $dist23 = $p[ 4 ] - $p[ 2 ] - 0.5 * ( $p[ 3 ] - $p[ 5 ] );
		my $det_offset = $p[ 8 ] - $p[ 4 ] - $dist12 - 0.5 * $p[ 5 ];
		open $f, "3_pulse_epr $p[ 1 ] $p[ 3 ] $p[ 5 ] $dist12 $dist23 " .
			     "$det_offset|"
			or die "Can't start 3 pulse EPR script.\n";
	} else {
		open $f, "3_pulse_epr|" or die "Can't start 3 pulse EPR script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_3_pulse_t1 {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist12 = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $dist23 = $p[ 4 ] - $p[ 2 ] - 0.5 * ( $p[ 3 ] - $p[ 5 ] );
		my $det_offset = $p[ 8 ] - $p[ 4 ] - $dist23 - 0.5 * $p[ 5 ];
		open $f, "3_pulse T1 $p[ 1 ] $p[ 3 ] $p[ 5 ] $dist12 $dist23 " .
			     "$det_offset|"
			or die "Can't start 3 pulse T1 script.\n";
	} else {
		open $f, "3_pulse_T1|" or die "Can't start 3 pulse T1 script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_3_pulse_em {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist12 = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $dist23 = $p[ 4 ] - $p[ 2 ] - 0.5 * ( $p[ 3 ] - $p[ 5 ] );
		my $det_offset = $p[ 8 ] - $p[ 4 ] - $dist12 - 0.5 * $p[ 5 ];
		open $f, "3_pulse EM $p[ 1 ] $p[ 3 ] $p[ 5 ] $dist12 $dist23 " .
			     "$det_offset|"
			or die "Can't start 3 pulse EM script.\n";
	} else {
		open $f, "3_pulse_EM|" or die "Can't start 3 pulse EM script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_3_pulse_endor {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		my $dist12 = $p[ 2 ] - $p[ 0 ] - 0.5 * ( $p[ 1 ] - $p[ 3 ] );
		my $dist23 = $p[ 4 ] - $p[ 2 ] - 0.5 * ( $p[ 3 ] - $p[ 5 ] );
		my $dist2rf = $p[ 6 ] - $p[ 2 ] - $p[ 3 ];
		my $distrf3 = $dist23 - $p[ 7 ] - 0.5 * ( $p[ 3 ] + $p[ 5 ] )
			          - $dist2rf;
		my $det_offset = $p[ 8 ]- $p[ 4 ] - $dist12 - 0.5 * $p[ 5 ];
		open $f, "3_pulse_endor $p[ 1 ] $p[ 3 ] $p[ 5 ] $dist12 $dist23 " .
			     "$dist2rf $distrf3 $det_offset|"
			or die "Can't start 3 pulse ENDOR script.\n";
	} else {
		open $f, "3_pulse_endor|" or die "Can't start 3 pulse ENDOR script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}
