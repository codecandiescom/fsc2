#!/usr/bin/perl

use strict;
use warnings;
use Fcntl ':mode';

die "No files passed as arguments\n" unless @ARGV > 0;

for my $fn ( @ARGV ) {
	next unless -f $fn && ! -l $fn;
	open my $fi, '<', $fn or next;
	if ( open my $fo, '>', $fn . '.new' ) {
		my $found = 0;
		while ( <$fi> ) {
			if ( /\s+Copyright\s+\(C\)\s+(\d{4}-)?2011\s+/ ) {
				s/2011/2012/;
				$found = 1;
			}
			print $fo $_;
		}
		close $fo;
		close $fi;
		if ( $found ) {
			my $mode = S_IMODE( ( stat( $fn ) )[ 2 ] );
			rename $fn . ".new", $fn;
			chmod $mode, $fn;
		} else {
			unlink $fn . ".new";
		}
	}
}
