use ExtUtils::MakeMaker;

# Compile and run the program that creates the include file (first try the
# version that also tries 'long long', then the one that only uses for 'long').

if ( ( system( "make sizes1 >/dev/null 2>&1" ) or
	   system( "./sizes1 >/dev/null 2>&1" ) ) and
	 ( system( "make sizes2 >/dev/null 2>&1" ) or
	   system( "./sizes2 >/dev/null 2>&1" ) ) ) {
	unlink( "./sizes1" );
	unlink( "./sizes2" );
	die "Can't create the necessary header file.\n";
}
	
unlink( "./sizes1" );
unlink( "./sizes2" );

# Now create the Makefile

WriteMakefile(
    'NAME'			=> 'Fcntl_Lock',
    'VERSION_FROM'	=> 'Fcntl_Lock.pm',
    'PREREQ_PM'		=> {},
    ( $] >= 5.005 ?
      ( ABSTRACT_FROM => 'Fcntl_Lock.pm', # retrieve abstract from module
        AUTHOR     => 'Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>' ) :
	  ( ) ),
    'LIBS'		=> [''],
    'DEFINE'	=> '',
    'INC'		=> '',
	'PERL_MALLOC_OK' => 1,
	clean => { FILES => "Fcntl_Lock.h" }
);
