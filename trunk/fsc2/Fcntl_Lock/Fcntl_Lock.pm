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
use Carp;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default.

our @EXPORT = qw( F_GETLK F_SETLK F_SETLKW
				  F_RDLCK F_WRLCK F_UNLCK
				  SEEK_SET SEEK_CUR SEEK_END
);

our $VERSION = '0.03';

bootstrap Fcntl_Lock $VERSION;

# Preloaded methods go here.

###########################################################
#

sub new {
	my $inv = shift;
    my $pkg = ref( $inv ) || $inv;
    my $self = { 'l_type'   => F_RDLCK,
				 'l_whence' => SEEK_SET,
                 'l_start'  => 0,
				 'l_len'    => 0,
				 'l_pid'    => 0,
			   };

	croak "Missing value in key-value initializer list" if @_ % 2;
	while ( @_ ) {
		no strict 'refs';
		my $key = shift;
		croak "Flock structure has no \'$key\' member" unless defined &$key;
		&$key( $self, shift );
	}

    bless $self, $pkg;
}


###########################################################
#

sub l_type {
    my $flock_struct = shift;
    return $flock_struct->{ l_type } unless @_;
    my $l_type = shift;
    croak "Invalid value for l_type member"
        unless $l_type == F_RDLCK or $l_type == F_WRLCK or $l_type == F_UNLCK;
    $flock_struct->{ 'l_type' } = $l_type;
}


###########################################################
#

sub l_whence {
    my $flock_struct = shift;
    return $flock_struct->{ l_whence } unless @_;
    my $l_whence = shift;
    croak "Invalid value for l_whence member" unless $l_whence == SEEK_SET or
											         $l_whence == SEEK_CUR or
											         $l_whence == SEEK_END;
    $flock_struct->{ l_whence } = $l_whence;
}


###########################################################
#

sub l_start {
    my $flock_struct = shift;
    return $flock_struct->{ l_start } unless @_;
    $flock_struct->{ l_start } = shift;
}


###########################################################
#

sub l_len {
    my $flock_struct = shift;
    return $flock_struct->{ l_len } unless @_;
    my $l_len = shift;
    croak "Invalid value for l_end member" if $l_len < 0;
    $flock_struct->{ l_len } = $l_len;
}


###########################################################
#

sub l_pid {
    my $flock_struct = shift;
    return $flock_struct->{ l_pid } unless @_;
    my $l_pid = shift;
    croak "Invalid value for l_pid member" unless $l_pid >= 0;
    $flock_struct->{ l_pid } = $l_pid;
}


###########################################################
#

sub fcntl_lock {
	my ( $flock_struct, $fh, $function ) = @_;
	croak "Missing arguments to fcntl_lock()"
		unless defined $flock_struct and defined $fh and defined $function;
	croak "Invalid action argument" unless $function == F_GETLK or
										   $function == F_SETLK or
										   $function == F_SETLKW;
	$fh = fileno( $fh ) if ref( $fh );
	C_fcntl_lock( $fh, $function, $flock_struct );
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
  $fs->fcntl_lock( $fh, F_SETLK ) or die "Can't obtain lock: $!\n";

=head1 DESCRIPTION

File locking is usually done using the flock() function. Unfortunately, this
allows only locks on whole files and is often implemented in terms of
flock(2), which has some shortcomings.

Using this module file locking via fcntl(2) can be done (obviously, this
restricts the use of the module to systems that have a fcntl() system
call). Before a file (or parts of a file) can be locked, an object simulating
a flock structure must be created and its properties set. Afterwards, by
calling the function fcntl_lock() a lock can be set or determined which
process currently holds the lock.

To create a new flock structure object simple call new():

  fs = Fcntl_Lock->new;

You also can pass the new() function a set of key-value pairs to initialize
the members of the flock structure, e.g.

  $fs = Fcntl_Lock->new( 'l_type'   => F_WRLCK,
                         'l_whence' => SEEK_SET,
                         'l_start'  => 0,
                         'l_len'    => 100 );

if you plan to obtain a write lock for the first 100 bytes of a file.

The following functions are available to set or query the properties of
the object simulating the flock structure:

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

On success the function returns the string "0 but true". Should you ever get
a return value of 'undef' with errno being set to B<EINVAL> chances are high
that you found a bug in the module.

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
