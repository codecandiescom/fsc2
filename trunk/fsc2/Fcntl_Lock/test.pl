# -*- cperl -*-
#
# $Id$
#
#  This program is free software; you can redistribute it and/or modify it
#  under the same terms as Perl itself.
#
#  Copyright (C) 2002-2006 Jens Thoms Toerring <jt@toerring.de>
#
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test;
BEGIN { plan tests => 17 };
use POSIX;
use Fcntl_Lock;

ok(1); # If we made it this far, we're in business...

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

##############################################
# 2. Most basic test: create an object

ok( $fs = Fcntl_Lock->new );

##############################################
# 3. Also basic: create an object with initalization

ok( $fs = $fs->new( 'l_type'   => F_RDLCK,
                    'l_whence' => SEEK_CUR,
                    'l_start'  => 123,
                    'l_len'    => 234,
                    'l_pid'    => $$ ) );

##############################################
# 4. Check if properties of created objects are what they are supposed to be

ok( $fs->l_type == F_RDLCK and $fs->l_whence == SEEK_CUR and
    $fs->l_start == 123 and $fs->l_len == 234 and $fs->l_pid == $$ );

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
# 11. Change l_pid property and check

$fs->l_pid( 1234 );
ok( $fs->l_pid, 1234 );

##############################################
# 12. Test if we can get an write lock on STDOUT

ok( defined $fs->fcntl_lock( STDOUT_FILENO, F_SETLK ) );

##############################################
# 13. Test if we can release the lock on STDOUT

$fs->l_type( F_UNLCK );
ok( defined $fs->fcntl_lock( STDOUT_FILENO, F_SETLK ) );

##############################################
# 14. Test if we can get a read lock on the script we're just running

$fs->l_type( F_RDLCK );
open( $fh, "./test.pl" );
ok( defined $fs->fcntl_lock( $fh, F_SETLK ) );

##############################################
# 15. Test if we can release this lock

$fs->l_type( F_UNLCK );
ok( defined $fs->fcntl_lock( $fh, F_SETLK ) );
close $fh;

##############################################
# 16. Now a real test: the child process grabs a write lock on a test file
#     for 2 secs while the parent repeatedly tries to get the lock. After
#     the child finally releases the lock the parent should be able to
#     obtain it (this test isn't real clean because under extremely high
#     system load it might not work as expected...)

$fs = $fs->new( 'l_type'   => F_WRLCK,
                'l_whence' => SEEK_SET,
                'l_start'  => 0,
                'l_len'    => 0 );
if ( open( $fh, ">./fcntl_locking_test" ) ) {
    unlink( "./fcntl_locking_test" );
    if ( my $pid = fork ) {
        sleep 1;
        $i = 13;
        for ( 1..12 ) {
            $i = $_;
            unless ( $fs->fcntl_lock( $fh, F_GETLK ) and 
                     $fs->l_type == F_UNLCK or $fs->l_pid == $pid ) {
                $i = 13;
                last;
            }
            last if $fs->l_type == F_UNLCK;
            select undef, undef, undef, 0.25;
        }
        if ( $i < 13 ) {
            $fs->l_type( F_WRLCK );
            ok( $state = $fs->fcntl_lock( $fh, F_SETLK ) );
            $fs->l_type( F_UNLCK );
            $fs->fcntl_lock( $fh, F_SETLK );
        } else {
            ok( 0 );
        }
        close $fh;
    } elsif ( defined $pid ) {
        $fs->fcntl_lock( $fh, F_SETLKW );
        sleep 2;
        $fs->l_type( F_UNLCK );
        $fs->fcntl_lock( $fh, F_SETLK );
        exit 0;
    } else {
        ok( 0 );
        print STDERR "Can't fork: $!\n";
    }
} else {
    print STDERR "Can't open a file for writing: $!\n";
    ok( 0 );
}

##############################################
# 17. Now another real test: basically the same as the previous one
#     but instead of locking a file both processes try to lock STDOUT

$fs = $fs->new( 'l_type'   => F_WRLCK,
                'l_whence' => SEEK_SET,
                'l_start'  => 0,
                'l_len'    => 0 );
if ( my $pid = fork ) {
    sleep 1;
    $i = 13;
    for ( 1..12 ) {
        $i = $_;
        unless ( $fs->fcntl_lock( STDOUT_FILENO, F_GETLK ) and
                 $fs->l_type == F_UNLCK or $fs->l_pid == $pid ) {
            $i = 13;
            last;
        }
        last if $fs->l_type == F_UNLCK;
        select undef, undef, undef, 0.25;
    }
    if ( $i < 13 ) {
        $fs->l_type( F_WRLCK );
        ok( $fs->fcntl_lock( STDOUT_FILENO, F_SETLK ) );
        $fs->l_type( F_UNLCK );
        $fs->fcntl_lock( STDOUT_FILENO, F_SETLK );
    } else {
        ok( 0 );
    }
    close STDOUT_FILENO;
} elsif ( defined $pid ) {
    $fs->fcntl_lock( STDOUT_FILENO, F_SETLKW );
    sleep 2;
    $fs->l_type( F_UNLCK );
    $fs->fcntl_lock( STDOUT_FILENO, F_SETLK );
    exit 0;
} else {
    print STDERR "Can't fork: $!\n";
    ok( 0 );
}


# Local variables:
# tab-width: 4
# indent-tabs-mode: nil
# End:
