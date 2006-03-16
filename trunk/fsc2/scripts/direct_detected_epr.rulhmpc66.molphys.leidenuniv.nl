#!/usr/bin/perl
# -*- cperl -*-

use strict;
use warnings;
use Tk;
use Tk::Balloon;

my @version = split /\./, $Tk::VERSION;
die "Installed Perl-Tk version is $Tk::VERSION but Tk800.022 is required.\n"
    if $version[ 0 ] + 0.001 * $version[ 1 ] < 800.022;

my %fp = ( -side => 'top',
           -fill => 'x',
           -padx => '2m',
           -pady => '2m' );
my %wp = ( -side => 'left',
           -fill => 'x',
           -expand => 1 );
my %up = ( -side => 'left' );
my $geom;

my $fsc2_how_to_run = "Test program";
my @fsc2_how_to_run = ( "Start experiment",
                        "Test program",
                        "Load into fsc2" );
my $fsc2_main_window = MainWindow->new( -title =>
							   ( split /\./, ( split /\//, $0 )[ -1 ] )[ 0 ] );
my $fsc2_main_frame = $fsc2_main_window->Frame( -relief => "ridge",
                                                -borderwidth => "1m" );
my $fsc2_balloon = $fsc2_main_frame->Balloon( );
my $fsc2_apply_frame = $fsc2_main_window->Frame( );
my $fsc2_apply_button = $fsc2_apply_frame->Button( -text => "Apply",
                                                   -command => \&write_out );
$fsc2_apply_button->bind( "all", "<Alt-a>" => \&write_out );
my $fsc2_quit_button = $fsc2_apply_frame->Button( -text => "Quit",
                 -command => sub { $fsc2_main_window->geometry =~
							   					   /^\d+x\d+([+-]\d+[+-]\d+)$/;
							   	   $geom = $1;
							   	   &store_defs;
                                   $fsc2_main_window->destroy } );
$fsc2_quit_button->bind( "all",
                         "<Alt-q>" =>
                         		sub { $fsc2_main_window->geometry =~
							   					   /^\d+x\d+([+-]\d+[+-]\d+)$/;
							   		  $geom = $1;
							   		  &store_defs;
									  $fsc2_main_window->destroy } );
$fsc2_apply_frame->pack( -side => "bottom",
                         -fill => "x",
                         -padx => "4m" );

# === DET_TIME float [ 0.001 : ] [ 50 ] "Detection time" "ms"

my %DET_TIME;
$DET_TIME{ tk_frame } = $fsc2_main_frame->Frame( );
$DET_TIME{ tk_label } = $DET_TIME{ tk_frame }->Label( -text => "Detection time",
-width => 20,
-anchor => 'w' );
$DET_TIME{ value } = 50;
$DET_TIME{ min } = 0.001;
$DET_TIME{ max } = undef;
$DET_TIME{ tk_entry } = $DET_TIME{ tk_frame }->Entry( -textvariable => \$DET_TIME{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $DET_TIME{ min } ? $DET_TIME{ min } : undef ),
( defined $DET_TIME{ max } ? $DET_TIME{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $DET_TIME{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $DET_TIME{ min } ? $DET_TIME{ min } : '-inf' ) .
" : " . ( defined $DET_TIME{ max } ? $DET_TIME{ max } : '+inf' ) . " ]" );
$DET_TIME{ tk_unit } = $DET_TIME{ tk_frame }->Label( -text => "ms",
-width => 5 );
$DET_TIME{ tk_frame }->pack( %fp );
$DET_TIME{ tk_label }->pack( %wp );
$DET_TIME{ tk_entry }->pack( %wp );
$DET_TIME{ tk_unit  }->pack( %up );

# === START_FIELD float [ 0 : 114304 ] [ 8000 ] "Start field" "G"

my %START_FIELD;
$START_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$START_FIELD{ tk_label } = $START_FIELD{ tk_frame }->Label( -text => "Start field",
-width => 20,
-anchor => 'w' );
$START_FIELD{ value } = 8000;
$START_FIELD{ min } = 0;
$START_FIELD{ max } = 114304;
$START_FIELD{ tk_entry } = $START_FIELD{ tk_frame }->Entry( -textvariable => \$START_FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $START_FIELD{ min } ? $START_FIELD{ min } : undef ),
( defined $START_FIELD{ max } ? $START_FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $START_FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $START_FIELD{ min } ? $START_FIELD{ min } : '-inf' ) .
" : " . ( defined $START_FIELD{ max } ? $START_FIELD{ max } : '+inf' ) . " ]" );
$START_FIELD{ tk_unit } = $START_FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$START_FIELD{ tk_frame }->pack( %fp );
$START_FIELD{ tk_label }->pack( %wp );
$START_FIELD{ tk_entry }->pack( %wp );
$START_FIELD{ tk_unit  }->pack( %up );

# === END_FIELD float [ 0 : 114304 ] [ 9000 ] "End field" "G"

my %END_FIELD;
$END_FIELD{ tk_frame } = $fsc2_main_frame->Frame( );
$END_FIELD{ tk_label } = $END_FIELD{ tk_frame }->Label( -text => "End field",
-width => 20,
-anchor => 'w' );
$END_FIELD{ value } = 9000;
$END_FIELD{ min } = 0;
$END_FIELD{ max } = 114304;
$END_FIELD{ tk_entry } = $END_FIELD{ tk_frame }->Entry( -textvariable => \$END_FIELD{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $END_FIELD{ min } ? $END_FIELD{ min } : undef ),
( defined $END_FIELD{ max } ? $END_FIELD{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $END_FIELD{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $END_FIELD{ min } ? $END_FIELD{ min } : '-inf' ) .
" : " . ( defined $END_FIELD{ max } ? $END_FIELD{ max } : '+inf' ) . " ]" );
$END_FIELD{ tk_unit } = $END_FIELD{ tk_frame }->Label( -text => "G",
-width => 5 );
$END_FIELD{ tk_frame }->pack( %fp );
$END_FIELD{ tk_label }->pack( %wp );
$END_FIELD{ tk_entry }->pack( %wp );
$END_FIELD{ tk_unit  }->pack( %up );

# === FIELD_STEP float [ 0.14288 :  ] [ 1 ] "Field step" "G"

my %FIELD_STEP;
$FIELD_STEP{ tk_frame } = $fsc2_main_frame->Frame( );
$FIELD_STEP{ tk_label } = $FIELD_STEP{ tk_frame }->Label( -text => "Field step",
-width => 20,
-anchor => 'w' );
$FIELD_STEP{ value } = 1;
$FIELD_STEP{ min } = 0.14288;
$FIELD_STEP{ max } = undef;
$FIELD_STEP{ tk_entry } = $FIELD_STEP{ tk_frame }->Entry( -textvariable => \$FIELD_STEP{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } : undef ),
( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $FIELD_STEP{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } : '-inf' ) .
" : " . ( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } : '+inf' ) . " ]" );
$FIELD_STEP{ tk_unit } = $FIELD_STEP{ tk_frame }->Label( -text => "G",
-width => 5 );
$FIELD_STEP{ tk_frame }->pack( %fp );
$FIELD_STEP{ tk_label }->pack( %wp );
$FIELD_STEP{ tk_entry }->pack( %wp );
$FIELD_STEP{ tk_unit  }->pack( %up );

# === WAIT_TIME float [ 0.0 : ] [ 5.0 ] "Magnet wait time" "s"

my %WAIT_TIME;
$WAIT_TIME{ tk_frame } = $fsc2_main_frame->Frame( );
$WAIT_TIME{ tk_label } = $WAIT_TIME{ tk_frame }->Label( -text => "Magnet wait time",
-width => 20,
-anchor => 'w' );
$WAIT_TIME{ value } = 5.0;
$WAIT_TIME{ min } = 0;
$WAIT_TIME{ tk_entry } = $WAIT_TIME{ tk_frame }->Entry( -textvariable => \$WAIT_TIME{ value },
-width => 10,
-validate => 'key',
-validatecommand => sub{ float_check( shift,
( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } : undef ),
( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } : undef ) ); },
-relief => 'sunken' );
$fsc2_balloon->attach( $WAIT_TIME{ tk_entry },
-balloonmsg  => "Range: [ " . ( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } : '-inf' ) .
" : " . ( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } : '+inf' ) . " ]" );
$WAIT_TIME{ tk_unit } = $WAIT_TIME{ tk_frame }->Label( -text => "s",
-width => 5 );
$WAIT_TIME{ tk_frame }->pack( %fp );
$WAIT_TIME{ tk_label }->pack( %wp );
$WAIT_TIME{ tk_entry }->pack( %wp );
$WAIT_TIME{ tk_unit  }->pack( %up );

$fsc2_main_frame->pack( %fp, -pady => '1m' );
$fsc2_main_window->Optionmenu( -options => \@fsc2_how_to_run,
                                -textvariable => \$fsc2_how_to_run,
                              )->pack( -padx => '3m',
                                       -pady => '3m' );

$fsc2_apply_button->pack( %wp, padx => '5m', -pady => '3m' );
$fsc2_quit_button->pack(  %wp, padx => '5m', -pady => '3m' );

load_defs( );
$fsc2_main_window->geometry( $geom ) if defined $geom;
MainLoop;


################################################################

sub int_check {
    my ( $new, $min, $max ) = @_;

    return 0 if $new =~ /^\+?(\d+)?$/ and defined $max and $max < 0;
    return 0 if $new =~ /^-/ and defined $min and $min >= 0;
    if ( $new =~ /^[+-]?$/ ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
         return 1;
     }

    return 0 unless $new =~ /^[+-]?\d+?$/;

    if ( ( defined $min and $new < $min )
         or ( defined $max and $new > $max ) ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
     } else {
         $fsc2_apply_button->configure( -state => 'normal' );
     }
     return 1;
}


################################################################

sub float_check {
    my ( $new, $min, $max ) = @_;
    my $float_rep = '[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?';

    return 0 if $new =~ /^\+/ and defined $max and $max < 0;
    return 0 if $new =~ /^-/ and defined $min and $min > 0;

    if ( $new =~ /^$float_rep$/ ) {
        if ( ( defined $max and $max < $new ) or
             ( defined $min and $min > $new ) ) {
            $fsc2_apply_button->configure( -state => 'disabled' );
        } else {
            $fsc2_apply_button->configure( -state => 'normal' );
        }
        return 1;
    }

    if ( $new =~ /^[+-]?(\d+)?\.?(\d+)?([Ee][+-]?(\d+)?)?$/ ) {
         $fsc2_apply_button->configure( -state => 'disabled' );
         return 1;
     }

     return 0;
}


################################################################

sub write_out {
    my $fh;

    open( $fh, "|fsc2_" . lc $fsc2_how_to_run )
        or die "Can't find utility fsc2_" . lc $fsc2_how_to_run . ".\n";

    my $DET_TIME = $DET_TIME{ value };
    my $START_FIELD = $START_FIELD{ value };
    my $END_FIELD = $END_FIELD{ value };
    my $FIELD_STEP = $FIELD_STEP{ value };
    my $WAIT_TIME = $WAIT_TIME{ value };

    print $fh "DEVICES:

ips120_10;
itc503;
rb8509;


VARIABLES:

detection_time   = $DET_TIME ms;
start_field   = $START_FIELD G;
end_field     = $END_FIELD G;
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "field_step    = $FIELD_STEP G;
";
# === else
    } else {
        print $fh "field_step    = - $FIELD_STEP G;
";
# === endif
    }

    print $fh "field;

N_Points = ceil( ( end_field - start_field ) / field_step ) + 1;
wait_time = $WAIT_TIME s;
I = 0;
J = 0;
K;
data[ *, * ];
avg[ N_Points ] = float_slice( N_Points );
File1, File2;
B1, B2, B3, B4;
start_temp;


PREPARATIONS:

";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "init_1d( 2, N_Points, start_field, field_step,
		 \"Field [G]\", \"Echo amplitude [a.u.]\" );
";
# === else
    } else {
        print $fh "init_1d( 2, N_Points, end_field, - field_step,
		 \"Field [G]\", \"Echo amplitude [a.u.]\" );
";
# === endif
    }

    print $fh "
EXPERIMENT:
";

	print $fh "
daq_gain( 4 );
start_temp = temp_contr_temperature( );

field = set_field( start_field );
IF wait_time > 1 us {
	wait( wait_time );
}

/* Open the file for averaged data */

File1 = get_file( \"\", \"*.avg\", \"\", \"\", \"avg\" );

/* Create the toolbox with two output field, one for the current scan number
   and one for the current field as well as a push button for stopping the
   experiment at the end of a scan */

hide_toolbox( \"ON\" );
B1 = output_create( \"INT_OUTPUT\", \"Current scan\" );
B2 = output_create( \"FLOAT_OUTPUT\", \"Current field [G]\", \"%.3f\" );
B3 = output_create( \"FLOAT_OUTPUT\", \"Current temperature [K]\", \"%.1f\" );
B4 = button_create( \"PUSH_BUTTON\", \"Stop after end of scan\" );
hide_toolbox( \"OFF\" );


FOREVER {

	J += 1;
	output_value( B1, J );	              // Update the scan count display

	FOR I = 1 : N_Points {
		output_value( B2, field );
		output_value( B3, temp_contr_temperature( ) );
		wait( detection_time );
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "
		data[ J, I ] = - daq_get_voltage( CH0 );
		display_1d( I, data[ J, I ], 1,
                    I, ( ( J - 1 ) * avg[ I ] + data[ J, I ] ) / J, 2 );
";
# === else
    } else {
        print $fh "
		data[ J, N_Points - I + 1 ] = - daq_get_voltage( CH0 );
		display_1d( N_Points - I + 1, data[ J, N_Points - I + 1 ], 1,
                    N_Points - I + 1,
                    ( ( J - 1 ) * avg[ N_Points - I + 1 ]
                      + data[ J, N_Points - I + 1 ] ) / J, 2 );
";
# === endif
    }

# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
        print $fh "		IF I < N_Points {
";
# === else
    } else {
        print $fh "		IF I > 1 {
";
# === endif
    }

    print $fh "			field += field_step;
			set_field( field );
		}
	}

	avg = add_to_average( avg, data[ J ], J );

	IF button_state( B4 ) {               // Stop on user request
		BREAK;
	}

	field = set_field( start_field );
	IF wait_time > 1 us {
		wait( wait_time );
	}
}

ON_STOP:

IF I != 0 {
	IF J == 1 {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "		FOR K = 1 : I - 1 {
			fsave( File1, \"#,#\\n\", start_field + ( K - 1 ) * field_step,
				   data[ 1, K ] );
		}
";
# === else
    } else {
        print $fh "		FOR K = N_Points - I + 2 : N_Points {
			fsave( File1, \"#,#\\n\", end_field + ( K - 1 ) * field_step,
				   data[ 1, K ] );
		}
";
	}
# === endif
	print $fh "		fsave( File1, \"\\n\" );
	} ELSE {
		IF I <= N_Points {
			J -= 1;
		}

		IF J > 1 {
			File2 = clone_file( File1, \"avg\", \"scans\" );
			FOR I = 1 : N_Points {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "	 		fsave( File2, \"#\", start_field + ( I - 1 ) * field_step );
";
# === else
    } else {
		print $fh "	 		fsave( File2, \"#\", end_field + ( I - 1 ) * field_step );
";
	}
	print $fh "			FOR K = 1 : J {
					fsave( File2, \",#\", data[ K, I ] );
				}
				fsave( File2, \"\\n\" );
			}
		}

		FOR I = 1 : N_Points {
";
# === if ( START_FIELD <= END_FIELD )
    if ( eval { ( $START_FIELD <= $END_FIELD ) } ) {
		print $fh "			fsave( File1, \"#,#\\n\", start_field + ( I - 1 ) * field_step, avg[ I ] );
";
# === else
    } else {
		print $fh "			fsave( File1, \"#,#\\n\", end_field + ( I - 1 ) * field_step, avg[ I ] );
";
	}

	print $fh "		}
		fsave( File1, \"\\n\" );
	}

	fsave( File1,
	       \"% Date:                   # #\\n\"
	       \"% Script:                 direct_detected_epr\\n\"
	       \"% Start field:            # G\\n\"
	       \"% End field:              # G\\n\"
	       \"% Field step:             # G\\n\"
	       \"% Detection time:         # ms\\n\"
		   \"% Number of scans:        #\\n\"
	       \"% ADC gain:               4\\n\"
	       \"% Temperature at start:   # K\\n\"
    	   \"% Temperature at end:     # K\\n\",
		   date( ), time( ), start_field, field, field_step,
		   detection_time * 1.0e3, J, start_temp, temp_contr_temperature( ) );

	save_comment( File1, \"% \" );

	IF J > 1 {
		fsave( File2,
	    	   \"% Date:                   # #\\n\"
	       	   \"% Script:                 direct_detected_epr\\n\"
	    	   \"% Start field:            # G\\n\"
	    	   \"% End field:              # G\\n\"
	    	   \"% Field step:             # G\\n\"
	    	   \"% Detection time:         # ms\\n\"
			   \"% Number of scans:        #\\n\"
	    	   \"% ADC gain:               4\\n\"
		       \"% Temperature at start:   # K\\n\"
    		   \"% Temperature at end:     # K\\n\",
			   date( ), time( ), start_field, field, field_step,
			   detection_time * 1.0e3, J, start_temp, temp_contr_temperature( ) );
	}
}
";
    close $fh;

    my $text;
    if ( $? != 0 ) {
        if ( $? >> 8 == 255 ) {
            $text = "Internal error.";
        } elsif ( $? >> 8 == 1 ) {
            $text = "Someone else is running fsc2.";
        } elsif ( $? >> 8 == 2 ) {
            $text = "fsc2 is already testing or\nrunning an experiment.";
        } elsif ( $? >> 8 == 3 ) {
            $text = "Internal error of fsc2.";
        } elsif ( $? >> 8 == 4 ) {
            $text = "Could not start fsc2.";
        } else {
            $text = "Something strange\nis going on here.";
        }

        &show_message( $text ) if $? >> 8 != 0;
    }
}


################################################################

sub show_message {
    my $text = shift;

    $fsc2_main_window->messageBox( -icon => 'error',
                                   -type => 'Ok',
                                   -title => 'Error',
                                   -message => $text );
}


################################################################

sub store_defs {
    my $fh;
    my $name = $0;

    $name =~ s|^.*?([^/]+)$|$1|;
    mkdir "$ENV{ HOME }/.fsc2", 0777 unless -e "$ENV{ HOME }/.fsc2";
    open( $fh, ">$ENV{ HOME }/.fsc2/$name" ) or return;
    print $fh "# Do not edit - created automatically!\n";

    if ( $DET_TIME{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $DET_TIME{ max } ? $DET_TIME{ max } >= $DET_TIME{ value } : 1 ) and
         ( defined $DET_TIME{ min } ? $DET_TIME{ min } <= $DET_TIME{ value } : 1 ) ) {
        print $fh "$DET_TIME{ value }\n";
    } else {
        print $fh "50\n";
    }

    if ( $START_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $START_FIELD{ max } ? $START_FIELD{ max } >= $START_FIELD{ value } : 1 ) and
         ( defined $START_FIELD{ min } ? $START_FIELD{ min } <= $START_FIELD{ value } : 1 ) ) {
        print $fh "$START_FIELD{ value }\n";
    } else {
        print $fh "8000\n";
    }

    if ( $END_FIELD{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $END_FIELD{ max } ? $END_FIELD{ max } >= $END_FIELD{ value } : 1 ) and
         ( defined $END_FIELD{ min } ? $END_FIELD{ min } <= $END_FIELD{ value } : 1 ) ) {
        print $fh "$END_FIELD{ value }\n";
    } else {
        print $fh "9000\n";
    }

    if ( $FIELD_STEP{ value } =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $FIELD_STEP{ max } ? $FIELD_STEP{ max } >= $FIELD_STEP{ value } : 1 ) and
         ( defined $FIELD_STEP{ min } ? $FIELD_STEP{ min } <= $FIELD_STEP{ value } : 1 ) ) {
        print $fh "$FIELD_STEP{ value }\n";
    } else {
        print $fh "1\n";
    }

    if ( $WAIT_TIME{ value } =~ /^((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o and
         ( defined $WAIT_TIME{ max } ? $WAIT_TIME{ max } >= $WAIT_TIME{ value } : 1 ) and
         ( defined $WAIT_TIME{ min } ? $WAIT_TIME{ min } <= $WAIT_TIME{ value } : 1 ) ) {
        print $fh "$WAIT_TIME{ value }\n";
    } else {
        print $fh "5.0\n";
    }

    print $fh "$fsc2_how_to_run\n";

	print $fh "$geom\n" if defined $geom;

    close $fh;
};


################################################################

sub load_defs {
    my $fh;
    my $name = $0;
    my $ne;
    my $found;
	my $got_args = 0;


	$name =~ s|^.*?([^/]+)$|$1|;

	if ( $ARGV[ 0 ] ) {
		open( $fh, "<$ARGV[ 0 ]" ) or return;
	} else {
		open( $fh, "<$ENV{ HOME }/.fsc2/$name" ) or return;
	}

    goto done_reading unless defined( $ne = <$fh> ) and $ne =~ /^#/;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $DET_TIME{ max } and $ne > $DET_TIME{ max } ) or
                         ( defined $DET_TIME{ min } and $ne < $DET_TIME{ min } );
    $DET_TIME{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $START_FIELD{ max } and $ne > $START_FIELD{ max } ) or
                         ( defined $START_FIELD{ min } and $ne < $START_FIELD{ min } );
    $START_FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $END_FIELD{ max } and $ne > $END_FIELD{ max } ) or
                         ( defined $END_FIELD{ min } and $ne < $END_FIELD{ min } );
    $END_FIELD{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^[+-]?((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $FIELD_STEP{ max } and $ne > $FIELD_STEP{ max } ) or
                         ( defined $FIELD_STEP{ min } and $ne < $FIELD_STEP{ min } );
    $FIELD_STEP{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> )
        and $ne =~ /^((\d+(\.(\d+)?)?)|(\.\d+))([eE][+-]?\d+)?$/o;
    chomp $ne;
    goto done_reading if ( defined $WAIT_TIME{ max } and $ne > $WAIT_TIME{ max } ) or
                         ( defined $WAIT_TIME{ min } and $ne < $WAIT_TIME{ min } );
    $WAIT_TIME{ value } = $ne;

    goto done_reading unless defined( $ne = <$fh> );
    chomp $ne;
    $found = 0;
    for ( @fsc2_how_to_run ) {
        if ( $ne eq $_) {
            $found = 1;
            last;
        }
    }
    goto done_reading unless $found;
    $fsc2_how_to_run = $ne;

    goto done_reading unless defined( $ne = <$fh> )
							 and $ne =~ /^\s*([+-]\d+[+-]\d+)\s*$/;
	$geom = $1;

  done_reading:
    close $fh;
};
