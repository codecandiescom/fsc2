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
my $use_data = 0;

my $mw = MainWindow->new( );
$mw->title( "Pulsed Experiments" );

$f[ 0 ] = $mw->Frame( );
$b[ 0 ] = $f[ 0 ]->Button( '-text' => "Pulse Monitor" );
$b[ 1 ] = $f[ 0 ]->Checkbutton( '-text' => "Use monitor data" );

$f[ 1 ] = $mw->Frame(  );
$b[ 2 ] = $f[ 1 ]->Button( '-text' => "2 Pulse T2" );
$b[ 3 ] = $f[ 1 ]->Button( '-text' => "2 Pulse EM" );
$b[ 4 ] = $f[ 1 ]->Button( '-text' => "2 Pulse ENDOR" );
$b[ 5 ] = $f[ 1 ]->Button( '-text' => "3 Pulse EPR" );
$b[ 6 ] = $f[ 1 ]->Button( '-text' => "3 Pulse T1" );
$b[ 7 ] = $f[ 1 ]->Button( '-text' => "3 Pulse EM" );
$b[ 8 ] = $f[ 1 ]->Button( '-text' => "3 Pulse ENDOR" );

$f[ 2 ] = $mw->Frame(  );
$b[ 9 ] = $f[ 2 ]->Button( '-text' => "Quit",
								   '-command' => sub { $mw->destroy } );


$b[ 0 ]->configure( '-command' => sub { &run_monitor } );
$b[ 1 ]->configure( '-state' => "disabled" );

$b[ 2 ]->configure( '-command' => sub { &run_2_pulse_t2 } );


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

sub run_2_pulse_t2 {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		open $f, "2_pulse_T2 $p[ 1 ] $p[ 2 ] $p[ 3 ] $p[ 4 ]|"
			or die "Can't start 2 pulse T2 script.\n";
	} else {
		open $f, "2_pulse_T2|" or die "Can't start 2 pulse T2 script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

sub run_2_pulse_em {
	my $f;
	$mw->withdraw;
	if ( @p and $use_data ) {
		open $f, "2_pulse_EM $p[ 1 ] $p[ 2 ] $p[ 3 ] $p[ 4 ]|"
			or die "Can't start 2 pulse EM script.\n";
	} else {
		open $f, "2_pulse_EM|" or die "Can't start 2 pulse EM script.\n";
	}
	while ( <$f> ) { }
	close $f;
	$mw->deiconify;
}

