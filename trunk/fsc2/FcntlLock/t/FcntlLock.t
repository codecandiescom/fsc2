# -*- cperl -*-
#
# $Id$
#
#  This program is free software; you can redistribute it and/or modify it
#  under the same terms as Perl itself.
#
#  Copyright (C) 2002-2008 Jens Thoms Toerring <jt@toerring.de>
#
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl FcntlLock.t'

#########################

use Test;
use strict;
use warnings;
BEGIN { plan tests => 11 };
use POSIX;
use File::FcntlLock;


##############################################
# 1. Most basic test: create an object

my $fs = new File::FcntlLock;
ok( defined $fs and $fs->isa( 'File::FcntlLock' ) );

##############################################
# 2. Also basic: create an object with initalization and check thet the
#    properties of the created object are what they are supposed to be

$fs = new File::FcntlLock l_type   => F_RDLCK,
                          l_whence => SEEK_CUR,
                          l_start  => 123,
                          l_len    => 234;
ok(     defined $fs
    and $fs->isa( 'File::FcntlLock' )
    and $fs->l_type   == F_RDLCK 
    and $fs->l_whence == SEEK_CUR
    and $fs->l_start  == 123
    and $fs->l_len    == 234      );

##############################################
# 3. Change l_type property to F_UNLCK and check

$fs->l_type( F_UNLCK );
ok( $fs->l_type, F_UNLCK );

##############################################
# 4. Change l_type property to F_WRLCK and check

$fs->l_type( F_WRLCK );
ok( $fs->l_type, F_WRLCK );

##############################################
# 5. Change l_whence property to SEEK_END and check

$fs->l_whence( SEEK_END );
ok( $fs->l_whence, SEEK_END );

##############################################
# 6. Change l_whence property to SEEK_SET and check

$fs->l_whence( SEEK_SET );
ok( $fs->l_whence, SEEK_SET );

##############################################
# 7. Change l_start property and check

$fs->l_start( 20 );
ok( $fs->l_start, 20 );

##############################################
# 8. Change l_len property and check

$fs->l_len( 3 );
ok( $fs->l_len, 3 );

##############################################
# 9. Test if we can get a read lock on a file and release it again

my $fh;
if ( defined open $fh, '>', './fcntllock_test' ) {
    close $fh;
    if ( defined open $fh, '<', './fcntllock_test' ) {
        $fs->l_type( F_RDLCK );
        my $res = $fs->lock( $fh, F_SETLK );
        unlink './fcntllock_test';
        if ( defined $res ) {
            $fs->l_type( F_UNLCK );
            $res = $fs->lock( $fh, F_SETLK );
            print "# Dropping read lock failed: $! (" . $fs->lock_errno . ")\n"
                unless defined $res;
        } else {
            print "# Read lock failed: $! (" . $fs->lock_errno . ")\n";
        }
        close $fh;
        ok( defined $res );
    } else {
        print "# Can't open a file for reading: $!\n";
        ok( 0 );
    }
} else {
    print "# Can't create a test file: $!\n";
    ok( 0 );
}

##############################################
# 10. Test if we can get an write lock on a test file and release it again

if ( defined open $fh, '>', './fcntllock_test' ) {
    unlink './fcntllock_test';
    $fs->l_type( F_WRLCK );
    my $res = $fs->lock( $fh, F_SETLK );
    if ( defined $res ) {
        $fs->l_type( F_UNLCK );
        $res = $fs->lock( $fh, F_SETLK );
        print "# Dropping write lock failed: $! (" . $fs->lock_errno . ")\n"
            unless defined $res;
        close( $fh );
    } else {
        print "# Write lock failed: $! (" . $fs->lock_errno . ")\n";
    }
    ok( defined $res );
} else {
    print "# Can't open a file for writing: $!\n";
    ok( 0 );
}

##############################################
# 11. Now a "real" test: the child process grabs a write lock on a test file
#     for 2 secs while the parent repeatedly tests if it could get the lock.
#     After the child finally releases the lock the parent should be able to
#     obtain and again release it.


if ( defined open $fh, '>', './fcntllock_test' ) {
    unlink './fcntllock_test';
    $fs = $fs->new( l_type   => F_WRLCK,
                    l_whence => SEEK_SET,
                    l_start  => 0,
                    l_len    => 0         );
    if ( my $pid = fork ) {
        sleep 1;
        my $failed = 1;

        while ( 1 ) {
            last if $pid == waitpid( $pid, WNOHANG ) and $?;
            last unless defined $fs->lock( $fh, F_GETLK );
            last if $fs->l_type == F_WRLCK and $fs->l_pid != $pid;
            if ( $fs->l_type == F_UNLCK ) {
                $failed = 0;
                last;
            }
            select undef, undef, undef, 0.25;
        }

        if ( ! $failed ) {
            $fs->l_type( F_WRLCK );
            ok(     $fs->lock( $fh, F_SETLK )
                    and $fs->l_type( F_UNLCK ), $fs->lock( $fh, F_SETLK ) );
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
        print "# Can't fork: $!\n";
        ok( 0 );
    }
} else {
    print "# Can't open a file for writing: $!\n";
    ok( 0 );
}


# Local variables:
# tab-width: 4
# indent-tabs-mode: nil
# End:
