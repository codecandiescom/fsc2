#!/usr/bin/perl

# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Copyright (C) 2002-2014 Jens Thoms Toerring <jt@toerring.de>


use strict;
use warnings;
use ExtUtils::MakeMaker;
use Config;


check_for_compiler( );
check_for_fcntl( );
assemble_pure( );


# Finally create the Makefile

WriteMakefile(
    NAME          => 'File::FcntlLock',
    VERSION_FROM  => 'lib/File/FcntlLock.pm',
    ABSTRACT_FROM => 'lib/File/FcntlLock.pod',
    LICENSE       => 'perl',
    AUTHOR        => 'Jens Thoms Toerring jt@toerring.de',
    PREREQ_PM     => { POSIX      => 0,
                       Errno      => 0,
                       Carp       => 0,
                       Exporter   => 0,
                       DynaLoader => 0,
                       Config     => 0 },
    test          => { TESTS => 't/*.t' },
    clean         => { FILES => 'lib/File/FcntlLock/Pure.pm' }
);


###########################################################
# Function for testing if the C compiler used for buildimg Perl is
# available on the system, otherwise there's no chance of building
# the module

sub check_for_compiler {
    open my $fh, '>', 'cc_test.c'
        or die "Failed to open a file for writing: $!\n";
    print $fh "int main(void)\n{\nreturn 0;\n}\n";
    close $fh;

    if ( system "$Config{cc} -o cc_test cc_test.c" ) {
        unlink 'cc_test.c';
        die "Can't run C compiler '$Config{cc}'\n";
    }
    unlink 'cc_test';
    unlink 'cc_test.c';
}


###########################################################
# Function for testing if the system has a fcntl(2) function,
# without it this module makes no sense at all.

sub check_for_fcntl {
    open my $fh, '>', 'fcntl_test.c'
        or die "Failed to open a file for writing: $!\n";
    print $fh <<EOF;
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
int main( void ) {
    int fd = fileno( fopen( "fcntl_test.c", "r" ) );
    struct flock f;
    f.l_type   = F_RDLCK;
    f.l_whence = SEEK_SET;
    f.l_start  = 0;
    f.l_len    = 0;
    return fcntl( fd, F_SETLK, &f ) != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
EOF
    close $fh;

    if ( system "$Config{cc} -o fcntl_test fcntl_test.c" ) {
        unlink 'fcntl_test.c';
        die "Failed to compile a program that uses fcntl(). Does your system " .
            "have a fcntl(2) system call?\n";
    }
    unlink 'fcntl_test';
    unlink 'fcntl_test.c';
}


###########################################################
# Function for assembling a "pure Perl" version of the module.
# For that we need to determine the layout of the C flock struct
# used by fcntl(2) and create some Perl code that can fill in such
# a structure via pack() and retrieve its values using unpack().
# This code then is combined with the template file 'Pure/Pure.tmpl'
# to make up the required module. (Failure is not a big issue,
# there are two other ways of attempting to use fcntl(2), one via
# an XS based module and one that attempts to obtain the same kind
# of information in its BEGIN block.)

sub assemble_pure {
    use File::Copy;

    return unless chdir 'Pure_build';

    goto FAIL if system "$Config{cc} -o builder builder.c";
    goto FAIL unless copy 'Pure.tmpl', 'Pure.pm';
    goto FAIL unless open my $out, '>>', 'Pure.pm';
    goto FAIL unless open my $in, "-|", './builder';
    print $out $_ for <$in>;
    close $in;

    print $out "\n\n1;\n";
    close $out;

    move 'Pure.pm', '../lib/File/FcntlLock';

  FAIL:
    unlink 'builder';
    chdir '..';
}


# Local variables:
# tab-width: 4
# indent-tabs-mode: nil
# End:
