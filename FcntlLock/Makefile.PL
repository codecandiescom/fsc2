# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Copyright (C) 2002-2011 Jens Thoms Toerring <jt@toerring.de>


use ExtUtils::MakeMaker;
use Config;


# Check if there's a C compiler we can use.

open my $fh, '>cc_test.c' or die "Failed to open a file for writing: $!\n";
print $fh "int main(void)\n{\nreturn 0;\n}\n";
close $fh;

if ( system "$Config{cc} -o cc_test cc_test.c" ) {
    unlink 'cc_test.c';
    die "Can't run C compiler '$Config{cc}'\n";
}
unlink 'cc_test';
unlink 'cc_test.c';


# Check if using fcntl() works - if the this fails the system may not
# have a fcntl(2) system call at all.

open $fh, '>fcntl_test.c' or die "Failed to open a file for writing: $!\n";
print $fh <<EOF;
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
int main( void ) {
	int fd = fileno( fopen( "./fcntl_test.c", "r" ) );
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


# Finally create the Makefile

WriteMakefile(
    NAME              => 'File::FcntlLock',
    ( $] >= 5.005 ?
      ( VERSION_FROM  => 'lib/File/FcntlLock.pm',
        ABSTRACT_FROM => 'lib/File/FcntlLock.pm',
        AUTHOR        => 'Jens Thoms Toerring <jt@toerring.de>' ) :
      ( ) ),
    PREREQ_PM         => { POSIX      => 0,
                           Errno      => 0,
                           Carp       => 0,
                           Exporter   => 0,
                           DynaLoader => 0 },
    PERL_MALLOC_OK    => TRUE,
    C                 => [ 'FcntlLock.xs' ],
    test              => { TESTS => 't/*.t' },
    clean             => { FILES => 'FcntlLock.h' }
);
