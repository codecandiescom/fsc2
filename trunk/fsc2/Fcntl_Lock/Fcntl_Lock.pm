# -*- cperl -*-
#
# This program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Copyright (C) 2002 Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>
#
# $Id$


package Fcntl_Lock;

use 5.006;
use strict;
use warnings;
use POSIX;
use Errno;
use Carp;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default.

our @EXPORT = qw( F_GETLK F_SETLK F_SETLKW
				  F_RDLCK F_WRLCK F_UNLCK
				  SEEK_SET SEEK_CUR SEEK_END
);

our $VERSION = '0.05';

# Set up an hash with the error messages, but only for errno's that Errno
# knows about. The texts represent what's written in the man pages for
# Linux and TRUE64.

my %fcntl_error_texts;

BEGIN {
	my $err;

	if ( $err = eval { &Errno::EACCES } ) {
		$fcntl_error_texts{ $err } = "File or segment already locked or " .
			                         "memory-mapped by other process(es)";
	}
	if ( $err = eval { &Errno::EAGAIN } ) {
		$fcntl_error_texts{ $err } = "File or segment already locked or " .
			                         "memory-mapped by other process(es)";
	}
	if ( $err = eval { &Errno::EBADF } ) {
		$fcntl_error_texts{ $err } = "Not an open file handle or descriptor " .
			                         "or not open or writing (with F_WRLCK)";
	}
	if ( $err = eval { &Errno::EDEADLK } ) {
		$fcntl_error_texts{ $err } = "Operation would cause a deadlock";
	}
	if ( $err = eval { &Errno::EFAULT } ) {
		$fcntl_error_texts{ $err } = "Lock outside accessible address space";
	}
	if ( $err = eval { &Errno::EINTR } ) {
		$fcntl_error_texts{ $err } = "Operation interrupted by a signal";
	}
	if ( $err = eval { &Errno::ENOLCK } ) {
		$fcntl_error_texts{ $err } = "Too many segment locks open, lock " .
			                         "table full or remote locking protocol " .
									 "failure (e.g. NFS)";
	}
	if ( $err = eval { &Errno::EINVAL } ) {
		$fcntl_error_texts{ $err } = "Illegal parameter or file does not " .
			                         "support locking";
	}
	if ( $err = eval { &Errno::EOVERFLOW } ) {
		$fcntl_error_texts{ $err } = "On of the parameters to be returned " .
			                         "can not be represented correctly";
	}
	if ( $err = eval { &Errno::ENETUNREACH } ) {
		$fcntl_error_texts{ $err } = "File is on remote machine that can " .
			                         "not be reached anymore";
	}
}


bootstrap Fcntl_Lock $VERSION;


# Preloaded methods go here.

###########################################################
#

sub new {
	my $inv = shift;
    my $pkg = ref( $inv ) || $inv;

    my $self = { 'l_type'        => F_RDLCK,
				 'l_whence'      => SEEK_SET,
                 'l_start'       => 0,
				 'l_len'         => 0,
				 'l_pid'         => 0,
				 'errno'         => undef,
				 'error_message' => undef
			   };

	croak "Missing value in key-value initializer list" if @_ % 2;
	while ( @_ ) {
		my $key = shift;
		no strict 'refs';
		croak "Flock structure has no \'$key\' member" unless defined &$key;
		&$key( $self, shift );
	}

    bless $self, $pkg;
}


###########################################################
#

sub l_type {
    my $flock_struct = shift;

	if ( @_ ) {
		my $l_type = shift;
		croak "Invalid value for l_type member"
		 unless $l_type == F_RDLCK or $l_type == F_WRLCK or $l_type == F_UNLCK;
		$flock_struct->{ 'l_type' } = $l_type;
	}
    return $flock_struct->{ l_type };
}


###########################################################
#

sub l_whence {
    my $flock_struct = shift;

	if ( @_ ) {
		my $l_whence = shift;
		croak "Invalid value for l_whence member"
			unless $l_whence == SEEK_SET or
				   $l_whence == SEEK_CUR or
				   $l_whence == SEEK_END;
		$flock_struct->{ l_whence } = $l_whence;
	}
    return $flock_struct->{ l_whence };
}


###########################################################
#

sub l_start {
    my $flock_struct = shift;

    if ( @_ ) { $flock_struct->{ l_start } = shift }
	return $flock_struct->{ l_start };
}


###########################################################
# Negative lengths may be allowed on some systems, e.g. on TRUE64 the man
# page says that when l_len is positive, the lock starts at l_start and ends
# at l_start + l_len, while for negative lengths the locked region is from
# l_start + l_len to l_start - 1.

sub l_len {
    my $flock_struct = shift;

	if ( @_ ) { $flock_struct->{ l_len } = shift }
    return $flock_struct->{ l_len };
}


###########################################################
#

sub l_pid {
    my $flock_struct = shift;

	if ( @_ ) {
		my $l_pid = shift;
		croak "Invalid value for l_pid member" unless $l_pid >= 0;
		$flock_struct->{ l_pid } = $l_pid;
	}
    return $flock_struct->{ l_pid };
}


###########################################################
#

sub fcntl_errno {
    my $flock_struct = shift;
	return $flock_struct->{ errno };
}


###########################################################
#

sub fcntl_error {
    my $flock_struct = shift;
	return $flock_struct->{ error };
}


###########################################################
#

sub fcntl_system_error {
	local $!;
	my $flock_struct = shift;
	return $flock_struct->{ errno } ? $! = $flock_struct->{ errno } : 'undef';
}


###########################################################
#

sub fcntl_lock {
	my ( $flock_struct, $fh, $action ) = @_;
	my $ret;
	my $err;

	croak "Missing arguments to fcntl_lock()"
		unless defined $flock_struct and defined $fh and defined $action;
	croak "Invalid action argument" unless $action == F_GETLK or
										   $action == F_SETLK or
										   $action == F_SETLKW;

	my $fd = ref( $fh ) ? fileno( $fh ) : $fh;
	if ( $ret = C_fcntl_lock( $fd, $action, $flock_struct, $err ) ) {
		$flock_struct->{ errno } = $flock_struct->{ error } = undef;
	} else {
		die "Internal error in Fcntl_Lock module detected" if $err;
		$flock_struct->{ errno } = $! + 0;
		$flock_struct->{ error } = defined $fcntl_error_texts{ $! + 0 } ?
			$fcntl_error_texts{ $! + 0 } : "Unexpected error: $!";
	}

	return $ret;
}


1;
__END__

=head1 NAME

Fcntl_Lock - Perl extension for file locking using fcntl()

=head1 SYNOPSIS

  use Fcntl_Lock;

  my $fs = Fcntl_Lock->new;
  $fs->l_type( F_RDLCK );
  $fs->l_whence( SEEK_CUR );
  $fs->l_start( 100 );
  $fs->l_len( 123 );

  my $fh;
  open( $fh, ">>file_name" ) or die "Can't open file: $!\n";
  unless ( $fs->fcntl_lock( $fh, F_SETLK ) ) {
      print "Locking failed: " . $fs->error . "\n";
  }

=head1 DESCRIPTION

File locking in Perl is usually done using the flock() function.
Unfortunately, this only allows locks on whole files and is often implemented
in terms of flock(2), which has some shortcomings.

Using this module file locking via fcntl(2) can be done (obviously, this
restricts the use of the module to systems that have a fcntl() system
call). Before a file (or parts of a file) can be locked, an object simulating
a flock structure must be created and its properties set. Afterwards, by
calling the function fcntl_lock() a lock can be set or determined which
process currently holds the lock.

To create a new flock structure object simple call B<new>:

  fs = Fcntl_Lock->new;

You also can pass the B<new> function a set of key-value pairs to initialize
the members of the flock structure, e.g.

  $fs = Fcntl_Lock->new( 'l_type'   => F_WRLCK,
                         'l_whence' => SEEK_SET,
                         'l_start'  => 0,
                         'l_len'    => 100 );

if you plan to obtain a write lock for the first 100 bytes of a file.

The following functions are for setting and quering the properties of the
object simulating the flock structure:

=over 4

=item B<l_type>

If called without an argument returns the current setting of the lock type,
otherwise the lock type is set to the argument, which must be either
B<F_RDLCK>, B<F_WRLCK> or B<F_UNLCK> (for read lock, write lock or unlock).


=item B<l_whence>

Queries or sets the B<l_whence> member of the flock structure, determining
if the B<l_start> value is relative to the start of the file, to the current
position in the file or to the end of the file. The corresponding values are
B<SEEK_SET>, B<SEEK_CUR> and B<SEEK_END>. See also the man page for lseek(2);


=item B<l_start>

Queries or sets the start position (offset) of the lock in the file according
to the mode selected by the B<l_whence> member. See also the man page for
lseek(2).


=item B<l_len>

Queries or sets the length of the region (in bytes) in the file to be
locked. A value of 0 means a lock (starting at B<l_start>) to the very end of
the file.

=item B<l_pid>

This member of the structure queried or set by the function is only useful
when determining the current owner of a lock, in which case the PID of the
process holding the lock is returned in this member.

=back

When not initialized the flock structure entry B<l_type> is set to B<F_RDLCK>
by default, B<l_whence> to B<SEEK_SET>, and both B<l_start> and B<l_len> to 0,
i.e. the settings for a read lock on the whole file.


After having set up the flock structure object you now can determine the
current holder of a lock or try to obtain a lock by invoking the function
fcntl_lock() with two arguments, a file handle (or a file descriptor, the
module figures out automatically what it got) and a flag indicating the
action to be taken, i.e.

  $fs->fcntl_lock( $fh, F_SETLK );

There are three actions, either B<F_GETLK>, B<F_SETLK> or B<F_SETLKW>:

=over 4

=item

For B<F_GETLK> the function determines if and who currently is holding the
lock.  If no other process is holding the lock the B<l_type> field is set to
F_UNLCK. Otherwise the flock structure object is set to the values that
prevent us from obtaining a lock, with the B<l_pid> entry set to the
PID of the process holding the lock.

For B<F_SETLK> the function tries to obtain the lock (when B<l_type> is set to
either B<F_WRLCK> or B<F_RDLCK>) or releases the lock (if B<l_type> is set to
B<F_UNLCK>). If the lock is held by someone else the function call returns
'undef' and errno is set to B<EACCESS> or B<EAGAIN> (please see the the man
page for fcntl(2) for the details).

B<F_SETLKW> is similar to B<F_SETLK> but instead of returning an error if the
lock can't be obtained immediately it blocks until the lock is obtained. If a
signal is received while waiting for the lock the function returns 'undef' and
errno is set to B<EINTR>.

=back

On success the function returns the string "0 but true". If the function fails
(as indicated by an 'undef' return value) you can either immediately evaluate
the error number ($!, $ERRNO or $OS_ERROR) directly or check for it at some
later time. There exist three functions for this purpose:

=over 4

=item B<fcntl_errno>

This function returns the error number from the latest call of B<fcntl_lock>
for the flock structure object. If the last call did not result in an error
the function returns 'undef'.

=item B<fcntl_error>

The function returns a short description of the error that happened during the
latest call of B<fcntl_lock> with the flock structure object. Please take the
messages with a grain of salt, they represent what Linux and TRUE64 tell
what the error numbers mean, there could be differences (and additional error
numbers) on other systems. If there was no error the function returns 'undef'.

=item B<fcntl_system_error>

The previous function, B<fcntl_error>, tries to return a string with some
relevance to the locking operation (i.e. "Lock is held by other processes"
instead of "Permission denied"). If one instead wants to obtain the normal
system error message one gets when using $! B<fcntl_system_error> can be
called instead. Also this function returns 'undef' if there was no error.

==back

=head2 EXPORT

F_GETLK F_SETLK F_SETLKW
F_RDLCK F_WRLCK F_UNLCK
SEEK_SET SEEK_CUR SEEK_END

=head1 CREDITS

Thanks to Mark-Jason Dominus <mjd@plover.com> and Benjamin Goldberg
<goldbb2@earthlink.net> for discussions, code and encouragement.

=head1 AUTHOR

Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>

=head1 SEE ALSO

L<perl>, L<fcntl(2)>, L<lseek(2)>.

=cut
