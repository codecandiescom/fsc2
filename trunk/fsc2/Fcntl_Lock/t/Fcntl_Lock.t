#!perl
# -*- cperl -*-
#
# $Id$
#
#  This program is free software; you can redistribute it and/or modify it
#  under the same terms as Perl itself.
#
#  Copyright (C) 2002-2007 Jens Thoms Toerring <jt@toerring.de>
#
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test;
use strict;
use warnings;
BEGIN { plan tests => 16 };
use POSIX;
use File::Fcntl_Lock;


##############################################
# 1. If we made it this far we're in business...

ok( 1 ); 

##############################################
# 2. Most basic test: create an object

ok( my $fs = new File::Fcntl_Lock );

##############################################
# 3. Also basic: create an object with initalization

ok( $fs = $fs->new( l_type   => F_RDLCK,
                    l_whence => SEEK_CUR,
                    l_start  => 123,
                    l_len    => 234       ) );

##############################################
# 4. Check if properties of the created object are what they are supposed to be

ok( $fs->l_type   == F_RDLCK  and
    $fs->l_whence == SEEK_CUR and
    $fs->l_start  == 123      and
    $fs->l_len    == 234          );

##############################################
# 5. Change l_type property to F_UNLCK and check

$fs->l_type( F_UNLCK );
ok( $fs->l_type, F_UNLCK );

##############################################
# 6. Change l_type property to F_WRLCK and check

$fs->l_type( F_WRLCK );
ok( $fs->l_type, F_WRLCK );

##############################################
# 7. Change l_whence property to SEEK_END and check

$fs->l_whence( SEEK_END );
ok( $fs->l_whence, SEEK_END );

##############################################
# 8. Change l_whence property to SEEK_SET and check

$fs->l_whence( SEEK_SET );
ok( $fs->l_whence, SEEK_SET );

##############################################
# 9. Change l_start property and check

$fs->l_start( 20 );
ok( $fs->l_start, 20 );

##############################################
# 10. Change l_len property and check

$fs->l_len( 3 );
ok( $fs->l_len, 3 );

##############################################
# 11. Test if we can get an write lock on STDOUT

ok( defined $fs->lock( STDOUT_FILENO, F_SETLK ) );

##############################################
# 12. Test if we can release the lock on STDOUT

$fs->l_type( F_UNLCK );
ok( defined $fs->lock( STDOUT_FILENO, F_SETLK ) );

##############################################
# 13. Test if we can get a read lock on the script we're just running

$fs->l_type( F_RDLCK );
my $fh;
unless ( open $fh, 't/Fcntl_Lock.t' ) {
    print STDERR "Can't open a file for reading: $!\n";
    ok( 0 );
}
ok( defined $fs->lock( $fh, F_SETLK ) );

##############################################
# 14. Test if we can release the lock

$fs->l_type( F_UNLCK );
ok( defined $fs->lock( $fh, F_SETLK ) );
close $fh;

##############################################
# 15. Now a "real" test: the child process grabs a write lock on a test file
#     for 2 secs while the parent repeatedly tests if it could get the lock.
#     After the child finally releases the lock the parent should be able to
#     obtain and again release it.

$fs = $fs->new( l_type   => F_WRLCK,
                l_whence => SEEK_SET,
                l_start  => 0,
                l_len    => 0         );
unless ( open( $fh, ">./fcntl_locking_test" ) ) {
    print STDERR "Can't open a file for writing: $!\n";
    ok( 0 );
}
unlink( "./fcntl_locking_test" );
if ( my $pid = fork ) {
    sleep 1;
    my $failed = 1;
    while ( 1 ) {
        last if $pid == waitpid( $pid, WNOHANG ) and $?;
        last unless $fs->lock( $fh, F_GETLK );
        if ( $fs->l_type == F_UNLCK ) {
            $failed = 0;
            last;
        }
        select undef, undef, undef, 0.25;
    }
    if ( ! $failed ) {
        $fs->l_type( F_WRLCK );
        ok( $fs->lock( $fh, F_SETLK ) and 
            $fs->l_type( F_UNLCK ), $fs->lock( $fh, F_SETLK ) );
    } else {
        ok( 0 );
    }
    close $fh;
} elsif ( defined $pid ) {
    $fs->lock( $fh, F_SETLKW ) or exit 1;
    sleep 2;
    $fs->l_type( F_UNLCK );
    $fs->lock( $fh, F_SETLK ) or exit 1;
    exit 0;
} else {
    print STDERR "Can't fork: $!\n";
    ok( 0 );
}

##############################################
# 16. Finally another "real" test: basically the same as the previous one
#     but instead of locking a file both processes try to lock STDOUT

$fs = $fs->new( l_type   => F_WRLCK,
                l_whence => SEEK_SET,
                l_start  => 0,
                l_len    => 0         );
if ( my $pid = fork ) {
    sleep 1;
    my $failed = 1;
    while ( 1 ) {
        last if $pid == waitpid( $pid, WNOHANG ) and $?;
        last unless $fs->lock( STDOUT_FILENO, F_GETLK );
        if ( $fs->l_type == F_UNLCK ) {
            $failed = 0;
            last;
        }
        select undef, undef, undef, 0.25;
    }
    if ( ! $failed ) {
        $fs->l_type( F_WRLCK );
        ok( $fs->lock( STDOUT_FILENO, F_SETLK ) and
            $fs->l_type( F_UNLCK ), $fs->lock( STDOUT_FILENO, F_SETLK ) );
    } else {
        ok( 0 );
    }
} elsif ( defined $pid ) {
    $fs->lock( STDOUT_FILENO, F_SETLKW ) or exit 1;
    sleep 2;
    $fs->l_type( F_UNLCK );
    $fs->lock( STDOUT_FILENO, F_SETLK ) or exit 1;
    exit 0;
} else {
    print STDERR "Can't fork: $!\n";
    ok( 0 );
}


# Local variables:
# tab-width: 4
# indent-tabs-mode: nil
# End:
