use ExtUtils::MakeMaker;

use Config;


# Check if we can run the C compiler and if using fcntl() works - if the
# later fails the system may not have a fcntl() system call at all.

open( $fh, ">cc_test.c" ) or die "Can't open a file for writing: $!\n";
print $fh "int main(void)\n{\nreturn 0;\n}\n";
close $fh;

if ( system $Config{cc}, qw( -o cc_test cc_test.c ) ) {
	unlink "cc_test.c";
	die "Can't run C compiler $Config{cc}\n";
}
unlink "cc_test";
unlink "cc_test.c";

open( $fh, ">fcntl_test.c" ) or die "Can't open a file for writing: $!\n";
print $fh "#include <fcntl.h>\n" .
	      "#include <stdio.h>\n" .
	      "int main(void)\n" .
          "{\n" .
	      "    struct flock f;\n" .
	      "    f.l_type = F_RDLCK;\n" .
	      "    f.l_whence = SEEK_SET;\n" .
          "    f.l_start = 0;\n" .
	      "    f.l_len = 0;\n" .
          "    return fcntl( 0, F_SETLK, &f ) == -1;\n" .
          "}\n";
close $fh;

if ( system $Config{cc}, qw( -o fcntl_test fcntl_test.c ) ) {
	unlink "fcntl_test.c";
	die "Can't compile a program that uses fcntl(). Does your system " .
		"really has a fcntl() system call?\n";
}
unlink "fcntl_test";
unlink "fcntl_test.c";


# Compile and run the program that creates the include file (first try a
# version that also tries 'long long', then one that only uses 'long').

if ( ( system $Config{cc}, qw( -o sizes1 sizes1.c ) or system "./sizes1" ) and
	 ( system $Config{cc}, qw( -o sizes2 sizes2.c ) or system "./sizes2" ) ) {
	unlink( "sizes1" ) if -e "sizes1";
	unlink( "sizes2" ) if -e "sizes2";
	die "Can't create the necessary header file.\n";
}
unlink "sizes1" if -e "sizes1";
unlink "sizes2" if -e "sizes2";


# Now create the Makefile

WriteMakefile(
    'NAME'			=> 'Fcntl_Lock',
    'VERSION_FROM'	=> 'Fcntl_Lock.pm',
    'PREREQ_PM'		=> {},
    ( $] >= 5.005 ?
      ( ABSTRACT_FROM => 'Fcntl_Lock.pm', # retrieve abstract from module
        AUTHOR     => 'Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>' ) :
	  ( ) ),
    'LIBS'		=> [ '' ],
    'DEFINE'	=> '',
    'INC'		=> '',
	'PERL_MALLOC_OK' => 1,
	'C'              => [ qw( Fcntl_Lock.c ) ],
	clean => { FILES => "Fcntl_Lock.h" }
);
