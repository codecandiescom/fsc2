# -*- cperl -*-
#
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.
#
# Copyright (C) 2002-2014 Jens Thoms Toerring <jt@toerring.de>


# Package for file locking with fcntl(2) in which the binary layout of
# the C flock struct has been determined via a C program on installation
# and appropriate Perl code been appended to the package.

package File::FcntlLock::Pure;

use 5.006;
use strict;
use warnings;
use base qw( File::FcntlLock::Core );


###########################################################
# Function for doing the actual fcntl() call: assembles the binary
# structure that must be passed to fcntl() from the File::FcntlLock
# object we get passed, calls it and then modifies the File::FcntlLock
# with the data from the flock structure

sub lock {
    my ( $flock_struct, $fh, $action ) = @_;
    my $buf = pack_flock( $flock_struct );
    my $ret = fcntl( $fh, $action, $buf );

    if ( $ret  ) {
		unpack_flock( $flock_struct, $buf );
        $flock_struct->{ errno } = $flock_struct->{ error } = undef;
    } else {
        $flock_struct->{ errno } = $! + 0;
        $flock_struct->{ error } = get_error( $! + 0 );
    }

    return $ret;
}


###########################################################

# Method created automatically while running 'perl Makefile.PL'
# (based on the the C 'struct flock' in <fcntl.h>) for packing
# the data from the 'flock_struct' into a binary blob to be
# passed to fcntl().

sub pack_flock {
    my $fs = shift;
    return pack( 'ssx4qqlx4',
                 $fs->{ l_type },
                 $fs->{ l_whence },
                 $fs->{ l_start },
                 $fs->{ l_len },
                 $fs->{ l_pid } );
}


###########################################################

# Method created automatically while running 'perl Makefile.PL'
# (based on the the C 'struct flock' in <fcntl.h>) for unpacking
# the binary blob received from a call of fcntl() into the
# 'flock_struct'.

sub unpack_flock {
     my ( $fs, $data ) = @_;
     ( $fs->{ l_type   },
       $fs->{ l_whence },
       $fs->{ l_start  },
       $fs->{ l_len    },
       $fs->{ l_pid    } ) = unpack( 'ssx4qqlx4', $data );
}


1;
