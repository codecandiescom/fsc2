/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"


#define INCH         25.4         /* inch to mm conversion factor */
#define S2P          1            /* send to printer mode */
#define P2F          2            /* print to file mode */

#define A6_PAPER     0
#define A5_PAPER     1
#define A4_PAPER     2
#define A3_PAPER     3
#define Letter_PAPER 4
#define Legal_PAPER  5


#define GUESS_NUM_LINES 10


static FD_print *print_form;

static char *cmd = NULL;             /* for storing the last print command */
static int print_type = S2P;         /* the way to print: S2P or P2F */

static int paper_type = A4_PAPER;
static double paper_width;
static double paper_height;

static bool print_with_color = UNSET;
static bool print_with_comment = UNSET;

static double x_0, y_0, w, h;        /* position and size of print area */
static double margin = 25.0;         /* margin (in mm) to leave on all sides */

static char *pc_string = NULL;

static bool get_print_file( FILE **fp, char **name, long data );
static void get_print_comm( void );
static void start_printing( FILE *fp, char *name, long what );
static void print_header( FILE *fp, char *name );
static void do_1d_printing( FILE *fp, long what );
static void do_2d_printing( FILE *fp );
static void eps_make_scale( FILE *fp, void *cv, int coord, long dim );
static void eps_color_scale( FILE *fp );
static void eps_draw_curve_1d( FILE *fp, Curve_1d *cv, int i, long dir );
static void eps_draw_surface( FILE *fp, int cn );
static void eps_draw_contour( FILE *fp, int cn );
static void print_comm( FILE *fp );
static char **split_into_lines( int *num_lines );
static char *paren_replace( const char *str );
static void print_markers( FILE *fp );


/*-------------------------------------------------------------------------*/
/* This function gets called as the callback routine for the print button  */
/* It shows a form that lets the user select if (s)he wants to "print" the */
/* resulting PostScript file (using whatever command the user specifies in */
/* the form) or to save it in as a file. Other things the user can choose  */
/* in this form is the paper size, either A4, A3, Legal, Letter or A5 or   */
/* A6 (the later two as size reduced version created for an A4 printer),   */
/* and b/w or color output.                                                */
/* For drawing the curves from the main 1D display window the parameter    */
/* 'data' is expected to be 1, for 2D display to be 2 and for cross        */
/* section curves 'data' must be 0 for cross sections through the x-axis   */
/* and -1 for the y-axis!                                                  */
/*-------------------------------------------------------------------------*/

void print_it( FL_OBJECT *obj, long data )
{
	FILE *fp = NULL;
	char *name = NULL;


	fl_deactivate_object( obj );

	if ( data == 2 && G.active_curve == -1 )
	{
		fl_show_alert( "Error", "Can't print - no curve is shown.", NULL, 1 );
		fl_activate_object( obj );
		return;
	}

	lower_permissions( );     /* not really needed,just my usual paranoia... */

	/* Find out about the way to print and get a file if needed */

	if ( get_print_file( &fp, &name, data ) )
	{
		get_print_comm( );

		if ( data == 2 && G.active_curve == -1 )
		{
			fl_show_alert( "Error", "Can't print - no curve is shown.",
						   NULL, 1 );
			fl_activate_object( obj );
			return;
		}

		if ( data == 2 )
			print_with_color = SET;

		start_printing( fp, name, data );

		if ( fp != NULL )
			fclose( fp );
		name = CHAR_P T_free( name );
	}

	fl_activate_object( obj );
}


/*--------------------------------------------------------------*/
/* Shows a form that allows the user to select the way to print */
/* and, for printing to file mode, select the file              */
/*--------------------------------------------------------------*/

static bool get_print_file( FILE **fp, char **name, long data )
{
	static FL_OBJECT *obj;
	struct stat stat_buf;


	/* Create the form for print setup */

	print_form = GUI.G_Funcs.create_form_print( );

	/* There's no good way to draw 2D in black and white so make the b&w
	   button invisible and activate the color button */

	if ( data == 2 )
	{
		fl_hide_object( print_form->col_button );
		fl_hide_object( print_form->bw_button );
	}

	/* If a printer command has already been set put it into the input object,
	   otherwise set default command */

	if ( cmd != NULL )
	{
		fl_set_input( print_form->s2p_input, cmd );
		cmd = CHAR_P T_free( cmd );
	}
	else
		fl_set_input( print_form->s2p_input, "lpr -h" );

	fl_set_button( print_form->add_comment, ( int ) print_with_comment );

	/* Set the paper type to the last one used (or A4 at the start) */

	switch ( paper_type )
	{
		case A6_PAPER :
			fl_set_button( print_form->A6, 1 );
			paper_width = 210.0;
			paper_height = 297.0;
			break;

		case A5_PAPER :
			fl_set_button( print_form->A5, 1 );
			paper_width = 210.0;
			paper_height = 297.0;
			break;

		case A4_PAPER :
			fl_set_button( print_form->A4, 1 );
			paper_width = 210.0;
			paper_height = 297.0;
			break;

		case A3_PAPER :
			fl_set_button( print_form->A3, 1 );
			paper_width = 297.0;
			paper_height = 420.2;
			break;

		case Letter_PAPER :
			fl_set_button( print_form->Letter, 1 );
			paper_width = 215.9;
			paper_height = 279.4;
			break;

		case Legal_PAPER :
			fl_set_button( print_form->Legal, 1 );
			paper_width = 215.9;
			paper_height = 355.6;
			break;
	}

	if ( print_type == S2P )
	{
		fl_set_button( print_form->s2p_button, 1 );
		fl_set_button( print_form->p2f_button, 0 );
		fl_set_object_lcol( print_form->s2p_input, FL_BLACK );
		fl_set_object_lcol( print_form->p2f_input, FL_INACTIVE_COL );
		fl_set_object_lcol( print_form->p2f_browse, FL_INACTIVE_COL );
		fl_deactivate_object( print_form->p2f_group );
	}
	else
	{
		fl_set_button( print_form->s2p_button, 0 );
		fl_set_button( print_form->p2f_button, 1 );
		fl_set_object_lcol( print_form->s2p_input, FL_INACTIVE_COL );
		fl_set_object_lcol( print_form->p2f_input, FL_BLACK );
		fl_set_object_lcol( print_form->p2f_browse, FL_BLACK );
		fl_deactivate_object( print_form->s2p_input );
	}

	if ( print_with_color )
	{
		fl_set_button( print_form->bw_button, 0 );
		fl_set_button( print_form->col_button, 1 );
	}
	else
	{
		fl_set_button( print_form->bw_button, 1 );
		fl_set_button( print_form->col_button, 0 );
	}

	fl_show_form( print_form->print, FL_PLACE_MOUSE, FL_TRANSIENT,
				  "fsc2: Print" );

	/* Let the user fill in the form */

	do
		obj = fl_do_forms( );
	while ( obj != print_form->print_button &&
			obj != print_form->cancel_button );

	/* Store state of form */

	TRY
	{
		if ( fl_get_button( print_form->s2p_button ) )
		{
			print_type = S2P;
			cmd = T_strdup( fl_get_input( print_form->s2p_input ) );
		}
		else
		{
			print_type = P2F;
			*name = T_strdup( fl_get_input( print_form->p2f_input ) );
		}
		TRY_SUCCESS;
	}
	CATCH ( OUT_OF_MEMORY_EXCEPTION )
		obj = print_form->cancel_button;

	if ( 1 == fl_get_button( print_form->A6 ) )
		 paper_type = A6_PAPER;
	if ( 1 == fl_get_button( print_form->A5 ) )
		 paper_type = A5_PAPER;
	if ( 1 == fl_get_button( print_form->A4 ) )
		 paper_type = A4_PAPER;
	if ( 1 == fl_get_button( print_form->A3 ) )
		 paper_type = A3_PAPER;
	if ( 1 == fl_get_button( print_form->Letter ) )
		 paper_type = Letter_PAPER;
	if ( 1 == fl_get_button( print_form->Legal ) )
		 paper_type = Legal_PAPER;

	fl_hide_form( print_form->print );
	fl_free_form( print_form->print );

	/* If cancel button was pressed (or an error happened) return now */

	if ( obj == print_form->cancel_button )
	{
		*name = CHAR_P T_free( *name );
		return FAIL;
	}

	/* In send-to-printer mode we're already done */

	if ( print_type == S2P )
		return OK;

	/* In print-to-file mode ask for confirmation if the file already exists
	   and try to open it for writing */

	if  ( 0 == stat( *name, &stat_buf ) &&
		  1 != show_choices( "The selected file does already exist:\n"
							 "Are you sure you want to overwrite it?",
							 2, "Yes", "No", NULL, 2 ) )
	{
		*name = CHAR_P T_free( *name );
		return FAIL;
	}

	if ( ( *fp = fopen( *name, "w" ) ) == NULL )
	{
		fl_show_alert( "Error", "Sorry, can't open file:", *name, 1 );
		*name = CHAR_P T_free( *name );
		return FAIL;
	}

	return OK;
}


/*------------------------------------------------------*/
/* Callback function for the objects in the print form. */
/*------------------------------------------------------*/

void print_callback( FL_OBJECT *obj, long data )
{
	const char *fn;


	data = data;

	if ( obj == print_form->s2p_button )
	{
		fl_set_object_lcol( print_form->p2f_input, FL_INACTIVE_COL );
		fl_set_object_lcol( print_form->p2f_browse, FL_INACTIVE_COL );
		fl_set_object_lcol( print_form->s2p_input, FL_BLACK );
		fl_activate_object( print_form->s2p_input );
		fl_deactivate_object( print_form->p2f_group );
	}
	else if ( obj == print_form->p2f_button )
	{
		fl_set_object_lcol( print_form->p2f_input, FL_BLACK );
		fl_set_object_lcol( print_form->p2f_browse, FL_BLACK );
		fl_set_object_lcol( print_form->s2p_input, FL_INACTIVE_COL );
		fl_deactivate_object( print_form->s2p_input );
		fl_activate_object( print_form->p2f_group );
	}
	else if ( obj == print_form->p2f_browse )
	{
		fn = fl_show_fselector( "Select output EPS file:", NULL,
								"*.eps", NULL );
		if ( fn != NULL )
			fl_set_input( print_form->p2f_input, fn );
	}
	else if ( obj == print_form->A6 )
	{
		paper_width = 210.0;
		paper_height = 297.0;
		paper_type = A6_PAPER;
	}
	else if ( obj == print_form->A5 )
	{
		paper_width = 210.0;
		paper_height = 297.0;
		paper_type = A4_PAPER;
	}
	else if ( obj == print_form->A4 )
	{
		paper_width = 210.0;
		paper_height = 297.0;
		paper_type = A4_PAPER;
	}
	else if ( obj == print_form->A3 )
	{
		paper_width = 297.0;
		paper_height = 420.2;
		paper_type = A3_PAPER;
	}
	else if ( obj == print_form->Letter )
	{
		paper_width = 215.9;
		paper_height = 279.4;
		paper_type = Letter_PAPER;
	}
	else if ( obj == print_form->Legal )
	{
		paper_width = 215.9;
		paper_height = 355.6;
		paper_type = Legal_PAPER;
	}
	else if ( obj == print_form->bw_button )
		print_with_color = UNSET;
	else if ( obj == print_form->col_button )
		print_with_color = SET;
	else if ( obj == print_form->add_comment )
		print_with_comment =
			fl_get_button( print_form->add_comment ) ? SET : UNSET;
}


/*---------------------------------------------------------------*/
/* Asks the user for a comment to print into the EPS file - the  */
/* resulting string is stored in the global variable 'pc_string' */
/*---------------------------------------------------------------*/

static void get_print_comm( void )
{
	FL_OBJECT *obj;
	const char *res;


	if ( ! print_with_comment )
		return;

	if ( pc_string != NULL )
		fl_set_input( GUI.print_comment->pc_input, pc_string );

	pc_string = CHAR_P T_free( pc_string );

	fl_show_form( GUI.print_comment->print_comment, FL_PLACE_MOUSE,
                  FL_TRANSIENT, "fsc2: Print Text" );

	while ( ( obj = fl_do_forms( ) ) != GUI.print_comment->pc_done )
	{
		if ( obj == GUI.print_comment->pc_clear )
			fl_set_input( GUI.print_comment->pc_input, NULL );
	}

	res = fl_get_input( GUI.print_comment->pc_input );
	if ( res != NULL && *res != '\0' )
		pc_string = T_strdup( res );

	if ( fl_form_is_visible( GUI.print_comment->print_comment ) )
		fl_hide_form( GUI.print_comment->print_comment );
}


/*--------------------------------------------------------------*/
/* Function forks of a child for doing the actually printing,   */
/* while the main program can continue taking care of new data. */
/*--------------------------------------------------------------*/

static void start_printing( FILE *fp, char *name, long what )
{
	pid_t pid;
	char filename[ ] = P_tmpdir "/fsc2.print.XXXXXX";
	int tmp_fd = -1;
	char *args[ 4 ];


	/* Do a minimal sanity check of the print command */

	if ( print_type == S2P && ( cmd == NULL || *cmd == '\0' ) )
	{
		fl_show_alert( "Error", "Sorry, missing print command.", NULL, 1 );
		return;
	}

	if ( ( pid = fork( ) ) < 0 )
	{
		fl_show_alert( "Error", "Sorry, can't print.",
					   "Running out of system resources.", 1 );
		return;
	}

	/* fsc2's main process can now continue, the spawned child process
	   takes care of all the rest */

	if ( pid > 0 )
		return;

	if ( GUI.is_init )
		close( ConnectionNumber( GUI.d ) );

	/* Some programs only work when the input is a seekable stream, so we
	   first create a temporary file (that gets unlink()'ed immediately,
	   so that its automatically gone when the process exits) */

	if ( print_type == S2P )
	{
		if ( ( tmp_fd = mkstemp( filename ) ) < 0 ||
			 ( fp = fdopen( tmp_fd, "w" ) ) == NULL )
		{
			if ( tmp_fd >= 0 )
			{
				unlink( filename );
				close( tmp_fd );
				_exit( EXIT_FAILURE );
			}
		}

		unlink( filename );
	}

	print_header( fp, name );

	if ( what == 2 )
		do_2d_printing( fp );
	else
		do_1d_printing( fp, what );

	fprintf( fp, "showpage\n"
			 	 "%%%%Trailer\n"
			 	 "end %% fsc2Dict\n"
				 "%%%%EOF\n" );

	fflush( fp );

	if ( print_type != S2P )
	{
		fclose( fp );
		_exit( EXIT_SUCCESS );
	}

	/* If we have to print to a command it gets execv()'ed with the
	   standard input redirected to read from the temporary file */

	lseek( tmp_fd, 0, SEEK_SET );

	args[ 0 ] = ( char * ) "sh";
	args[ 1 ] = ( char * ) "-c";
	args[ 2 ] = ( char * ) cmd;
	args[ 3 ] = NULL;

	dup2( tmp_fd, STDIN_FILENO );
	close( tmp_fd );

	execv( "/bin/sh", args );
	_exit( EXIT_FAILURE );
}


/*------------------------------------------------------------------------*/
/* Prints the header of the EPS-file as well as date, user and fsc2 logo. */
/*------------------------------------------------------------------------*/

static void print_header( FILE *fp, char *name )
{
	time_t d;
	static char *tstr = NULL;


	/* Writes EPS header plus some routines into the file */

	d = time( NULL );

	fprintf( fp, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			     "%%%%BoundingBox: 0 0 %d %d\n"
			     "%%%%Creator: fsc2, Mar 2002\n"
				 "%%%%CreationDate: %s"
				 "%%%%Title: %s\n"
				 "%%%%For: %s (%s)\n"
			     "%%%%Orientation: Landscape\n"
			 	 "%%%%DocumentData: Clean8Bit\n"
				 "%%%%DocumentNeededResources: font Times-Roman\n"
			     "%%%%EndComments\n",
			 irnd( 72.0 * paper_width / INCH ),
			 irnd( 72.0 * paper_height / INCH ),
			 ctime( &d ), name ? name : "(none)",
			 getpwuid( getuid( ) )->pw_name,
			 getpwuid( getuid( ) )->pw_gecos );

	/* Create a dictonary with the commands used in all the following */

	fprintf( fp, "%%%%BeginProlog\n"
			 	 "%%%%BeginResource: procset fsc2Dict\n"
			     "/fsc2Dict 23 dict dup begin\n"
			     "/b { bind def } bind def\n"
			     "/s { stroke } b\n"
			     "/f { fill } b\n"
                 "/t { translate } b\n"
			     "/r { rotate} b\n"
				 "/m { moveto } b\n"
			     "/l { lineto } b\n"
			     "/rm { rmoveto } b\n"
			     "/rl { rlineto } b\n"
			     "/slw { setlinewidth } b\n"
			     "/sd { setdash } b\n"
			     "/srgb { setrgbcolor } b\n"
			     "/sgr { setgray } b\n"
			     "/sw { stringwidth pop } b\n"
				 "/sf { exch findfont exch scalefont setfont } b\n"
			     "/np { newpath } b\n"
			     "/cp { closepath } b\n"
			     "/gs { gsave } b\n"
			     "/gr { grestore } b\n"
			     "/slc { setlinecap } b\n"
			     "/ch { gs np 0 0 m\n"
			     "      false charpath flattenpath pathbbox\n"
                 "      exch pop 3 -1 roll pop exch pop gr } b\n"
				 "/cw { gs np 0 0 m\n"
	             "      false charpath flattenpath pathbbox\n"
			     "      pop exch pop exch pop gr } b\n" );

	if ( print_with_color )
		fprintf( fp,
			     "/fsc2 { gs /Times-Roman 6 sf\n"
			     "        (fsc2) ch sub 6 sub exch\n"
			     "        (fsc2) cw sub 4 sub exch m\n"
			     "        1 -0.025 0 { dup dup srgb gs (fsc2) show gr\n"
			     "        -0.025 0.025 rm } for\n"
			     "        1 1 1 srgb (fsc2) show gr } b\n" );
	else
		fprintf( fp,
			     "/fsc2 { gs /Times-Roman 6 sf\n"
			     "        (fsc2) ch sub 6 sub exch\n"
			     "        (fsc2) cw sub 4 sub exch m\n"
			     "        1 -0.025 0 { sgr gs (fsc2) show gr\n"
			     "        -0.025 0.025 rm } for\n"
			     "        1 setgray (fsc2) show gr } b\n" );

	/* End of dictonary, now open it for all of the following (don't forget
	   to close it at the end of the file !) */

	fprintf( fp, "end readonly def %% fsc2Dict\n"
			 	 "%%%%EndResource\n"
			     "%%%%EndProlog\n"
			 	 "%%%%BeginSetup\n"
			     "fsc2Dict begin\n"
			 	 "%%%%EndSetup\n" );

	/* Setup the font that also supports umlauts etc., thanks to Adobe Systems
	   Inc., "PostScript Language Reference", 3rd edition */

	fprintf( fp, "/Times-RomanISOLatin1 /Times-Roman findfont dup length\n"
			 	 "dict begin {\n"
			 	 "  1 index /FID ne\n { def } { pop pop } ifelse\n"
				 "} forall\n"
				 "/Encoding ISOLatin1Encoding def\n"
				 "currentdict end definefont pop\n" );

	/* Rotate and shift for landscape format, scale to mm units, draw logo,
	   set font and draw date and user name */

	fprintf( fp, "%.6f dup scale 90 r 0 %.2f t\n",
			 72.0 / INCH, - paper_width );

	if ( paper_type == A5_PAPER )
		fprintf( fp, "10 10 t\n0.5 sqrt dup scale\n" );

	if ( paper_type == A6_PAPER )
		fprintf( fp, "10 10 t\n0.5 dup scale\n" );

	fprintf( fp, "%.2f %.2f fsc2\n", paper_height, paper_width );
	fprintf( fp, "/Times-RomanISOLatin1 4 sf\n" );

	TRY
	{
		tstr = T_strdup( ctime( &d ) );
		tstr[ strlen( tstr ) - 1 ] = '\0';
		fprintf( fp, "5 5 m (%s %s) show\n", tstr,
				 getpwuid( getuid( ) )->pw_name );
		T_free( tstr );
		TRY_SUCCESS;
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
		fprintf( fp, "5 5 m (%s) show\n", getpwuid( getuid( ) )->pw_name );
}


/*------------------------------------------------------------*/
/* This function writes out the Postscript code for all kinds */
/* of 1D graphics, i.e. the normal 1D window as well as cross */
/* section windows. What it got to output it sees from the    */
/* last argument which is set to 1 for a normal 1D window, to */
/* 0 for a cross section in x-direction and -1 for a cross    */
/* section in y-direction.                                    */
/*------------------------------------------------------------*/

static void do_1d_printing( FILE *fp, long what )
{
	int i;


	/* Set the area for the graph, leave a margin on all sides and,
       additionally, 20 mm (x-axis) / 25 (y-axis) for labels and tick marks */

	x_0 = margin + 25.0;              /* margin & 25 mm for the y-axis */
	y_0 = margin + 20.0;              /* margin & 20 mm for the x-axis */
	w = paper_height - margin - x_0;
	h = paper_width - margin - y_0;

	fprintf( fp, "0.05 slw 0 sgr\n" );

	if ( print_with_comment )
		print_comm( fp );

	/* Draw the frame and the scales (just 0.5 mm outside the the area for the
       graph) */

	fprintf( fp, "%.2f %.2f m\n0 %.2f rl\n%.2f 0 rl\n0 %.2f rl cp s\n",
			 x_0 - 0.5, y_0 - 0.5, h + 1.0, w + 1.0, - ( h + 1.0 ) );

	if ( what == 1 )    /* normal 1D printing */
	{
		/* Draw the scale for the active curve (if there is an active curve) */

		for ( i = 0; i < G.nc; i++ )
		{
			if ( G.curve[ i ]->active )
				break;
		}

		if ( i != G.nc )
		{
			eps_make_scale( fp, ( void * ) G.curve[ i ], X, 1 );
			eps_make_scale( fp, ( void * ) G.curve[ i ], Y, 1 );
		}
	}
	else                /* when printing a cross section */
	{
		eps_make_scale( fp, ( void * ) &G.cut_curve, X, what );
		eps_make_scale( fp, ( void * ) &G.cut_curve, Y, what );
	}

	/* Restrict further drawings to the frame of the scales */

	fprintf( fp, "%.2f %.2f m\n0 %.2f rl\n%.2f 0 rl\n0 %.2f rl cp clip np\n",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	if ( what == 1 )
	{
		for ( i = G.nc - 1; i >= 0; i-- )
			eps_draw_curve_1d( fp, G.curve[ i ], i, 1 );
		print_markers( fp );
	}
	else
		eps_draw_curve_1d( fp, &G.cut_curve, 0, what );
}


/*--------------------------------------------------------------*/
/* This function writes out the Postscript code for 2D windows. */
/* Because I wasn't able to come up yet with any useful method  */
/* on how to draw a contour in b&w this part has currently been */
/* disabled (the user can't select b&w).                        */
/*--------------------------------------------------------------*/

static void do_2d_printing( FILE *fp )
{
	/* If the active curve has been switched off in the mean time nothing
	   remains to be printed... */

	if ( G.active_curve == -1 )
		return;

	/* Set the area for the graph, leave a margin on all sides and,
       additionally, 20 mm (x-axis) / 25 (y-axis) for labels and tick marks */

	x_0 = margin + 25.0;
	y_0 = margin + 20.0;

	if ( print_with_color )
		w = paper_height - margin - x_0 - 35.0;
	else
		w = paper_height - margin - x_0;

	h = paper_width - margin - y_0;

	fprintf( fp, "0.05 slw 0 0 0 srgb\n" );

	if ( print_with_comment )
		print_comm( fp );

	/* Draw the frame and the scales */

	fprintf( fp, "%.2f %.2f m\n0 %.2f rl\n%.2f 0 rl\n0 %.2f rl cp s\n",
			 x_0 - 0.5, y_0 - 0.5, h + 1.0, w + 1.0, - ( h + 1.0 ) );

	eps_make_scale( fp, G.curve_2d[ G.active_curve ], X, 2 );
	eps_make_scale( fp, G.curve_2d[ G.active_curve ], Y, 2 );

	if ( print_with_color )
		eps_make_scale( fp, G.curve_2d[ G.active_curve ], Z, 2 );

	/* Restrict further drawings to the frame of the scales */

	fprintf( fp, "%.2f %.2f m\n0 %.2f rl\n%.2f 0 rl\n0 %.2f rl cp clip np\n",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	/* The way the program is working currently only the first alternative
	   will happen (b&w support for 2D is currently switched off because
	   I didn't come up with a good way to draw it...) */

	if ( print_with_color )
		eps_draw_surface( fp, G.active_curve );
	else
		eps_draw_contour( fp, G.active_curve );
}


/*----------------------------------------------*/
/* Draws the scales for both 1D and 2D graphics */
/*----------------------------------------------*/

static void eps_make_scale( FILE *fp, void *cv, int coord, long dim )
{
	double rwc_delta,           /* distance between small ticks (in rwc) */
		   order,               /* and its order of magnitude */
		   mag;
	static double d_delta_fine, /* distance between small ticks (in points) */
		   d_start_fine,        /* position of first small tick (in points) */
		   d_start_medium,      /* position of first medium tick (in points) */
		   d_start_coarse,      /* position of first large tick (in points) */
		   cur_p;               /* loop variable with position */
	static int medium_factor,   /* number of small tick spaces between */
			   coarse_factor;   /* medium and large tick spaces */
	static int medium,          /* loop counters for medium and large ticks */
			   coarse;
	double rwc_start = 0,       /* rwc value of first point */
		   rwc_start_fine,      /* rwc value of first small tick */
		   rwc_start_medium,    /* rwc value of first medium tick */
		   rwc_start_coarse;    /* rwc value of first large tick */
	static double rwc_coarse;
	static double x, y;
	char lstr[ 128 ];
	double s2d[ 3 ];
	int r_coord;
	double rwcs, rwcd;
	char *label;


	if ( dim == 1 )
	{
		s2d[ X ] = w * ( ( Curve_1d * ) cv )->s2d[ X ] / G.canvas.w;
		s2d[ Y ] = h * ( ( Curve_1d * ) cv )->s2d[ Y ] / G.canvas.h;

		rwcs = G.rwc_start[ coord ];
		rwcd = G.rwc_delta[ coord ];
	}
	else if ( dim == 2 )
	{
		s2d[ X ] = w * ( ( Curve_2d * ) cv )->s2d[ X ] / G.canvas.w;
		s2d[ Y ] = h * ( ( Curve_2d * ) cv )->s2d[ Y ] / G.canvas.h;
		s2d[ Z ] = h * ( ( Curve_2d * ) cv )->s2d[ Z ] / G.z_axis.h;

		rwcs = ( ( Curve_2d * ) cv )->rwc_start[ coord ];
		rwcd = ( ( Curve_2d * ) cv )->rwc_delta[ coord ];
	}
	else
	{
		s2d[ X ] = w * ( ( Curve_1d * ) cv )->s2d[ X ] / G.cut_canvas.w;
		s2d[ Y ] = h * ( ( Curve_1d * ) cv )->s2d[ Y ] / G.cut_canvas.h;

		if ( coord == X )
			r_coord = ( dim < 0 ? X : Y );
		else
			r_coord = Z;

		rwcs = G.curve_2d[ G.active_curve ]->rwc_start[ r_coord ];
		rwcd = G.curve_2d[ G.active_curve ]->rwc_delta[ r_coord ];
	}

	/* The distance between the smallest ticks should be about 6 points -
	   calculate the corresponding delta in real word units */

	rwc_delta = 1.5 * fabs( rwcd ) / s2d[ coord ];

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Now get a 'smooth' value for the real world ticks distance, i.e. either
	   2, 2.5, 5 or 10 */

	if ( rwc_delta <= 2.0 )       /* in [ 1, 2 ] -> units of 2 */
	{
		medium_factor = 5;
		coarse_factor = 25;
		rwc_delta = 2.0 * order;
	}
	else if ( rwc_delta <= 3.0 )  /* in ] 2, 3 ] -> units of 2.5 */
	{
		medium_factor = 4;
		coarse_factor = 20;
		rwc_delta = 2.5 * order;
	}
	else if ( rwc_delta <= 6.0 )  /* in ] 3, 6 ] -> units of 5 */
	{
		medium_factor = 2;
		coarse_factor = 10;
		rwc_delta = 5.0 * order;
	}
	else                          /* in ] 6, 10 [ -> units of 10 */
	{
		medium_factor = 5;
		coarse_factor = 10;
		rwc_delta = 10.0 * order;
	}

	/* Calculate the final distance between the small ticks in points */

	d_delta_fine = s2d[ coord ] * rwc_delta / fabs( rwcd );

	/* 'rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the frame), 'rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   'd_start_fine' is the same position but in points */

	if ( dim <= 1 )
		rwc_start = rwcs - ( ( Curve_1d * ) cv )->shift[ coord ] * rwcd;
	else if ( dim == 2 )
		rwc_start = rwcs - ( ( Curve_2d * ) cv )->shift[ coord ] * rwcd;

	if ( rwcd < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = s2d[ coord ] * ( rwc_start_fine - rwc_start ) / rwcd;

	if ( lrnd( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = s2d[ coord ] * ( rwc_start_medium - rwc_start ) / rwcd;

	if ( lrnd( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lrnd( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = s2d[ coord ] * ( rwc_start_coarse - rwc_start ) / rwcd;

	if ( lrnd( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lrnd( ( d_start_fine - d_start_coarse ) / d_delta_fine );

	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

	if ( coord == X )
	{
		/* Draw the x-axis label string */

		TRY
		{
			if ( dim == 1 || dim == 2 )
				label = paren_replace( G.label[ X ] );
			else
				label = paren_replace( G.label[ dim < 0 ? X : Y ] );

			if ( label != NULL )
			{
				fprintf( fp, "(%s) dup cw %.2f exch sub %.2f m show\n",
						 label, x_0 + w, margin );
				T_free( label );
			}
			TRY_SUCCESS;
		}
		CATCH( OUT_OF_MEMORY_EXCEPTION ) { }

		y = y_0;

		/* Set the minimum start position of a x-axis tick label */

		fprintf( fp, "%.2f\n", d_start_fine - 45.0 );

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < w;
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = cur_p + x_0;

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				fprintf( fp, "%.2f %.2f m 0 -3.5 rl s\n", x, y - 0.5 );
				rwc_coarse += coarse_factor * rwc_delta;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				fprintf( fp, "dup %.2f (%s) cw 0.5 mul sub 3 sub lt {\n",
						 x, lstr );
				fprintf( fp, "%.2f (%s) cw 0.5 mul sub %.2f 5 sub (%s) ch sub "
						 "m\n(%s) show pop %.2f (%s) cw 0.5 mul add } if\n",
						 x, lstr, y - 1.0, lstr, lstr, x, lstr );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				fprintf( fp, "%.2f %.2f m 0 -2.5 rl s\n", x, y - 0.5 );
			else                                       /* short line */
				fprintf( fp, "%.2f %.2f m 0 -1.5 rl s\n", x, y - 0.5 );
		}

		fprintf( fp, "pop\n" );
	}
	else if ( coord == Y )
	{
		/* Draw the y-axis label string */

		TRY
		{
			if ( dim == 1 || dim == 2 )
				label = paren_replace( G.label[ Y ] );
			else
				label = paren_replace( G.label[ Z ] );

			if ( label != NULL )
				fprintf( fp, "gs (%s) dup dup ch %.2f add exch cw %.2f exch "
						 	 "sub t 90 r 0 0 m show gr\n",
						 label, margin, paper_width - margin );
			T_free( label );
			TRY_SUCCESS;
		}
		CATCH( OUT_OF_MEMORY_EXCEPTION ) { }

		x = x_0;

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < h;
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			y = cur_p + y_0;

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				fprintf( fp, "%.2f %.2f m -3.5 0 rl s\n", x - 0.5, y );
				rwc_coarse += coarse_factor * rwc_delta;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				fprintf( fp, "%.2f (%s) cw sub 5 sub %.2f (%s) ch 0.5 mul "
						 "sub m (%s) show\n", x_0 - 0.5, lstr, y, lstr, lstr );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				fprintf( fp, "%.2f %.2f m -2.5 0 rl s\n", x - 0.5, y );
			else                                      /* short line */
				fprintf( fp, "%.2f %.2f m -1.5 0 rl s\n", x - 0.5, y );

		}
	}
	else
	{
		x = x_0 + w + 17.0;

		if ( G.label[ Z ] != NULL )
		{
			TRY
			{
				label = paren_replace( G.label[ Z ] );

				if ( label != NULL )
				{
					fprintf( fp, "gs (%s) dup dup ch %.2f exch sub exch cw "
							 "%.2f exch sub t 90 r 0 0 m show gr\n",
							 label, x + 28.0, paper_width - margin );
					T_free( label );
				}
				TRY_SUCCESS;
			}
			CATCH( OUT_OF_MEMORY_EXCEPTION ) { }
		}

		/* Make the color scale */

		eps_color_scale( fp );

		/* Draw all the ticks and numbers */

		fprintf( fp, "%.2f %.2f m 0 %.2f rl s\n", x - 1.0, y_0, h );

		for ( cur_p = d_start_fine; cur_p < h;
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			y = cur_p + y_0;

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				fprintf( fp, "%.2f %.2f m -3.5 0 rl s\n", x - 0.5, y );
				rwc_coarse += coarse_factor * rwc_delta;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				fprintf( fp, "%.2f %.2f (%s) ch 0.5 mul "
						 "sub m (%s) show\n", x + 1, y, lstr, lstr );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				fprintf( fp, "%.2f %.2f m -2.5 0 rl s\n", x - 0.5, y );
			else                                      /* short line */
				fprintf( fp, "%.2f %.2f m -1.5 0 rl s\n", x - 0.5, y );

		}
	}
}


/*-------------------------------------------------------*/
/* Creates the scale for the color coding printed for 2D */
/* output in color mode.                                 */
/*-------------------------------------------------------*/

static void eps_color_scale( FILE *fp )
{
	double x = x_0 + w + 5.0;
	double y = y_0;
	double width = 6.0;
	double h_inc, c_inc;
	double c;
	int i;
	int rgb[ 3 ];


	fprintf( fp, "%.2f %.2f m 0 %.2f rl %.2f 0 rl 0 %.2f rl cp s\n",
			 x - 0.1, y_0 - 0.1, h + 0.2, width + 0.2, - ( h + 0.2 ) );


	h_inc = h / ( double ) NUM_COLORS;
	c_inc = 1.0 / ( double ) ( NUM_COLORS - 1 );
	for ( i = 0, c = 0.0; i < NUM_COLORS; y += h_inc, c += c_inc, i++ )
	{
		i2rgb( c, rgb );
		fprintf( fp, "%.6f %.6f %.6f srgb\n"
				     "%.2f %.2f m %.2f 0 rl 0 %.2f rl %.2f 0 rl cp f\n",
				 ( double ) rgb[ RED ] / 255.0,
				 ( double ) rgb[ GREEN ] / 255.0,
				 ( double ) rgb[ BLUE ] / 255.0,
				 x, y, width, h_inc, - width );
	}

	fprintf( fp, "0 0 0 srgb\n" );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void eps_draw_curve_1d( FILE *fp, Curve_1d *cv, int i, long dir )
{
	double s2d[ 2 ];
	long k;
	long max_points = ( dir == 1 || dir == -1 ) ? G.nx : G.ny;


	if ( dir == 1 )
	{
		s2d[ X ] = w * cv->s2d[ X ] / G.canvas.w;
		s2d[ Y ] = h * cv->s2d[ Y ] / G.canvas.h;
	}
	else
	{
		s2d[ X ] = w * cv->s2d[ X ] / G.cut_canvas.w;
		s2d[ Y ] = h * cv->s2d[ Y ] / G.cut_canvas.h;
	}

	fprintf( fp, "gs 0.2 slw 1 slc\n" );

	/* Set the either the colour or the dash type for the different curves */

	switch ( i )
	{
		case 0 :
			if ( print_with_color )
				fprintf( fp, "1 0 0 srgb\n" );          /* red */
			break;

		case 1 :
			if ( print_with_color )
				fprintf( fp, "0 1 0 srgb\n" );          /* green */
			else
				fprintf( fp, "[ 1 0.8 ] 0 sd\n" );
			break;

		case 2 :
			if ( print_with_color )
				fprintf( fp, "1 0.75 0 srgb\n" );       /* dark yellow */
			else
				fprintf( fp, "[ 0.001 0.5 ] 0 sd\n" );
			break;

		case 3 :
			if ( print_with_color )
				fprintf( fp, "0 0 1 srgb\n" );          /* blue */
			else
				fprintf( fp, "[ 0.5 0.5 0.0001 0.5 ] 0 sd\n" );
			break;
	}

	/* Find the very first point and move to it */

	for ( k = 0; ! cv->points[ k ].exist && k < max_points; k++ )
		/* empty */ ;

	if ( k >= max_points )                  /* is there only just one ? */
		return;

	fprintf( fp, "%.2f %.2f m\n", x_0 + s2d[ X ] * ( k + cv->shift[ X ] ),
			 y_0 + s2d[ Y ] * ( cv->points[ k ].v + cv->shift[ Y ] ) );
	k++;

	/* Draw all other points */

	for ( ; k < max_points; k++ )
		if (  cv->points[ k ].exist )
			fprintf( fp, "%.2f %.2f l\n",
					 x_0 + s2d[ X ] * ( k + cv->shift[ X ] ),
					 y_0 + s2d[ Y ] * ( cv->points[ k ].v + cv->shift[ Y ] ) );

	fprintf( fp, "s gr\n" );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void eps_draw_surface( FILE *fp, int cn )
{
	Curve_2d *cv = G.curve_2d[ cn ];
	double s2d[ 2 ];
	double dw,
		   dh;
	long i, j, k;
	int rgb[ 3 ];


	s2d[ X ] = w * cv->s2d[ X ] / G.canvas.w;
	s2d[ Y ] = h * cv->s2d[ Y ] / G.canvas.h;
	dw = s2d[ X ] / 2;
	dh = s2d[ Y ] / 2;

	fprintf( fp, "gs\n" );

	/* Print comlete area gray to coover all points for which there are
	   no data */

	fprintf( fp, "%s\n%.2f %.2f m 0 %.2f rl %.2f 0 rl 0 %.2f rl cp f\n",
			 print_with_color ? "0.5 0.5 0.5 srgb" : "0.5 sgr",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	/* Now draw points for which we have data */

	for ( k = 0, j = 0; j < G.ny; j++ )
		for ( i = 0; i < G.nx; k++, i++ )
		{
			if ( ! cv->points[ k ].exist )
				continue;

			i2rgb( cv->z_factor * ( cv->points[ k ].v + cv->shift[ Z ] ),
				   rgb );
			fprintf( fp,
					 "%.6f %.6f %.6f srgb\n"
					 "%.2f %.2f m %.2f 0 rl 0 %.2f rl %.2f 0 rl cp f\n",
					 ( double ) rgb[ RED ] / 255.0,
					 ( double ) rgb[ GREEN ] / 255.0,
					 ( double ) rgb[ BLUE ] / 255.0,
					 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) - dw,
					 y_0 + s2d[ Y ] * ( j + cv->shift[ Y ] ) - dh,
					 2.0 * dw, 2.0 * dh, - 2.0 * dw );
		}

	fprintf( fp, "gr\n" );
}


/*--------------------------------------------------------------------------*/
/* A rather useless try to draw a 2D curve in gray scale, using contours... */
/*--------------------------------------------------------------------------*/

static void eps_draw_contour( FILE *fp, int cn )
{
	Curve_2d *cv = G.curve_2d[ cn ];
	double s2d[ 2 ];
	double dw,
		   dh;
	double cur_p;
	double z;
	long i, j, k;
	double z1, z2, g;


	s2d[ X ] = w * cv->s2d[ X ] / G.canvas.w;
	s2d[ Y ] = h * cv->s2d[ Y ] / G.canvas.h;
	dw = s2d[ X ] / 2;
	dh = s2d[ Y ] / 2;

	fprintf( fp, "gs\n" );

	/* Print areas gray for which there are no data */

	cur_p = s2d[ X ] * cv->shift[ X ];
	if ( cur_p - dw > 0 )
		fprintf( fp,
				 "0.5 sgr %.2f %.2f m %.2f 0 rl 0 %.2f rl %.2f 0 rl cp f\n",
				 x_0, y_0, cur_p - dw, h, - ( cur_p - dw ) );

	cur_p = s2d[ X ] * ( G.nx - 1 + cv->shift[ X ] );
	if ( w > cur_p + dw )
		fprintf( fp,
				 "0.5 sgr %.2f %.2f m 0 %.2f rl %.2f 0 rl 0 %.2f rl cp f\n",
				 x_0 + cur_p + dw, y_0, h, w - ( cur_p + dw ), - h );

	cur_p = s2d[ Y ] * cv->shift[ Y ];
	if ( cur_p - dh > 0 )
		fprintf( fp,
				 "0.5 sgr %.2f %.2f m %.2f 0 rl 0 %.2f rl %.2f 0 rl cp f\n",
				 x_0, y_0, w, cur_p - dh, - w );

	cur_p = s2d[ Y ] * ( G.ny - 1 + cv->shift[ Y ] );
	if ( h > cur_p + dh )
		fprintf( fp,
				 "0.5 sgr %.2f %.2f m %.2f 0 rl 0 %.2f rl %.2f 0 rl cp f\n",
				 x_0, y_0 + cur_p + dh , w, h - ( cur_p + dh ), - w );

	/* Now draw the data */

	for ( g = 1.0, z = 0.0; z <= 1.0; g -= 0.045, z += 0.05 )
		for ( k = 0, j = 0; j < G.ny; j++ )
			for ( i = 0; i < G.nx; i++, k++ )
			{
				if ( ! cv->points[ k ].exist )
				{
					fprintf( fp, "0.5 sgr %.2f %.2f m 0 %.2f 2 copy rl "
							 	 "%.2f 0 rl neg rl cp f\n",
							 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) - dw,
							 y_0 + s2d[ Y ] * ( j + cv->shift[ Y ] ) - dh,
							 2.0 * dh, 2.0 * dw );
					continue;
				}

				if ( i < G.nx - 1 && cv->points[ k + 1 ].exist )
				{
					z1 = cv->z_factor * ( cv->points[ k ].v + cv->shift[ Z ] );
					z2 = cv->z_factor
						          * ( cv->points[ k + 1 ].v + cv->shift[ Z ] );

					if ( ( z1 >= z && z2 < z ) || ( z1 < z && z2 >= z ) )
					{
						fprintf( fp, "%.6f sgr %.2f %.2f m 0 %.2f rl s\n",
								 g,
								 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) + dw,
								 y_0 + s2d[ Y ] * ( j + cv->shift[ Y ] ) - dh,
								 2.0 * dh );
					}
				}

				if ( j < G.ny - 1 && cv->points[ ( j + 1 ) * G.nx + i ].exist )
				{
					z1 = cv->z_factor * ( cv->points[ k ].v + cv->shift[ Z ] );
					z2 = cv->z_factor * ( cv->points[ ( j + 1 ) * G.nx + i ].v
										  + cv->shift[ Z ] );

					if ( ( z1 >= z && z2 < z ) || ( z1 < z && z2 >= z ) )
					{
						fprintf( fp, "%.6f sgr %.2f %.2f m %.2f 0 rl s\n",
								 g,
								 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) - dw,
								 y_0 + s2d[ Y ] * ( j + cv->shift[ Y ] ) + dh,
								 2.0 * dw );
					}
				}
			}

	fprintf( fp, "gr\n" );
}


/*---------------------------------------------------------------*/
/* This routine prints a comment text into a field in the middle */
/* of the page below the x-axis.                                 */
/*---------------------------------------------------------------*/

static void print_comm( FILE *fp )
{
	char **lines = NULL;
	int num_lines;
	int i;


	if ( pc_string == NULL )
		return;

	if ( ( lines = split_into_lines( &num_lines ) ) == NULL )
		return;

	/* Calculate the length of the longest lines and push it on the stack */

	fprintf( fp, "(%s) dup sw dup\n", lines[ 0 ] );

	if ( num_lines > 1 )
		for ( i = 1; i < num_lines; i++ )
			  fprintf( fp, "(%s) dup sw exch 4 1 roll dup 4 1 roll lt\n"
						   "{ pop }{ exch pop } ifelse dup\n",
					   lines[ i ] );

	fprintf( fp, "pop dup %.2f exch sub 0.5 mul %.2f add\n", w, margin );
	fprintf( fp, "5 (X) ch 1.5 mul add gs t dup 0 0 m np\n"
				 "(m) cw neg (X) ch -1.5 mul m\n"
				 "(m) cw 2 mul add 0 rl\n"
				 "0 (X) ch %d 1.65 mul 4 add mul rl\n"
				 "(m) cw 2 mul add neg 0 rl cp\n"
				 "gs 0.95 %s f gr s\n",
			 num_lines - 1, print_with_color ? "dup dup srgb" : "sgr" );

	fprintf( fp, "0 0 m\n"
				 "count { dup show sw neg (X) ch 1.65 mul rm } "
				 "repeat gr\n" );


	for ( i = 0; i < num_lines; i++ )
		T_free( lines[ i ] );

	T_free( lines );

}


/*-------------------------------------------------------------------*/
/* The comment text to be printed is a single string with embedded   */
/* newline characters. This routine splits this string into an array */
/* of strings, each of them holding one line of text. The function   */
/* returns an array of strings (the calling routine must T_free()    */
/* both all the strings as well as the array of string pointers) and */
/* the number of lines to be printed (through the pointer argument). */
/*-------------------------------------------------------------------*/

static char **split_into_lines( int *num_lines )
{
	static char **lines;
	static char *cp;
	static int nl;
	static int cur_size;
	static int i = 0;
	int j;
	ptrdiff_t count;


	lines = NULL;
	cur_size = GUESS_NUM_LINES;

	TRY
	{
		lines = CHAR_PP T_malloc( cur_size * sizeof *lines );
		TRY_SUCCESS;
	}
	OTHERWISE
		return NULL;

	/* Count the number of lines by detecting the '\n' characters and set up
	   an array of pointers to the starts of the lines */

	TRY
	{
		for ( lines[ nl = 0 ] = cp = pc_string; *cp != '\0'; cp++ )
			if ( *cp == '\n' )
			{
				if ( nl++ == cur_size )
				{
					cur_size += GUESS_NUM_LINES;
					lines = CHAR_PP T_realloc( lines,
											   cur_size * sizeof *lines );
				}

				lines[ nl ] = cp + 1;
			}

		if ( *--cp == '\n' )
		{
			while ( nl != 0 && *--cp == '\n')
				nl--;

			if ( nl == 0 )
				THROW( EXCEPTION );

			lines = CHAR_PP T_realloc( lines, ( nl + 1 ) * sizeof *lines );
		}
		else
		{
			if ( nl++ >= cur_size )
				lines = CHAR_PP T_realloc( lines, ( nl + 1 ) * sizeof *lines );
			lines[ nl ] = cp + 2;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		T_free( lines );
		return NULL;
	}

	/* Now split the string into single lines and copy them into the array
	   of lines */

	TRY
	{
		for ( i = 0; i < nl; i++ )
		{
			cp = lines[ i ];
			count = lines[ i + 1 ] - cp;
			lines[ i ] = CHAR_P T_malloc( count );
			memcpy( lines[ i ], cp, count );
			lines[ i ][ count - 1 ] = '\0';

			TRY
			{
				cp = paren_replace( lines[ i ] );
				TRY_SUCCESS;
			}
			OTHERWISE
			{
				T_free( lines[ i ] );
				RETHROW( );
			}

			T_free( lines[ i ] );
			lines[ i ] = cp;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		for ( j = 0; j < i; j++ )
			T_free( lines[ i ] );
		T_free( lines );
		return NULL;
	}

	*num_lines = nl;
	return lines;
}


/*------------------------------------------------------------------*/
/* Routine returns a string that is basically the copy of the input */
/* string but with a backslash prepended to all parenthesis (which  */
/* the PostScript interpreter might otherwise choke on). At the     */
/* same time all characters that aren't Clean8Bit (i.e. all charac- */
/* ters below 0x1B (except TAB, LF and CR) and 0x7F) are replaced   */
/* by a space character.                                            */
/*------------------------------------------------------------------*/

static char *paren_replace( const char *str )
{
	char *sp;
	char *cp;
	long p_count, len, i;


	if ( str == NULL )
		return NULL;

	for ( p_count = 0, len = 1, sp = ( char * ) str; *sp != '\0'; sp++, len++ )
	{
		if ( *sp < 0x1B || *sp == 0x7F )
		{
			if ( *sp == 0x7F ||
				 ( *sp != 0x09 && *sp != 0x0A && *sp != 0x0D ) )
				*sp = ' ';
		}

		if ( *sp == '(' || *sp == ')' )
			p_count++;
	}

	if ( p_count == 0 )
		return T_strdup( str );

	cp = sp = CHAR_P T_malloc( len + p_count );
	strcpy( sp, str );

	for ( i = len; *cp != '\0'; i--, cp++ )
		if ( *cp == '(' || *cp == ')' )
		{
			memmove( cp + 1, cp, i );
			*cp++ = '\\';
		}

	return sp;
}


/*-----------------------------------------------------*/
/* Function for drawing the markers in 1D graphic mode */
/*-----------------------------------------------------*/

static void print_markers( FILE *fp )
{
	long i;
	Curve_1d *cv = NULL;
	Marker *m;
	double s2d;


	/* Find out the currently active curve (needed for scaling), return
	   immediately if there isn't one */

	for ( i = 0; i < G.nc; i++ )
		if ( G.curve[ i ]->active )
		{
			cv = G.curve[ i ];
			break;
		}

	if ( cv == NULL )
		return;

	/* Calculate x scale factor and then draw all markers */

	s2d = w * cv->s2d[ X ] / G.canvas.w;

	for ( m = G.marker; m != NULL; m = m->next )
	{
		if ( print_with_color )
			switch ( m->color )
			{
				case 0 :
					fprintf( fp, "0.8 0.8 0.8 srgb " );    /* "white" */
					break;

				case 1 :
					fprintf( fp, "1 0 0 srgb " );          /* red */
					break;

				case 2 :
					fprintf( fp, "0 1 0 srgb " );          /* green */
					break;

				case 3 :
					fprintf( fp, "1 0.75 0 srgb " );       /* dark yellow */
					break;

				case 4 :
					fprintf( fp, "0 0 1 srgb " );          /* blue */
					break;

				default :
					fprintf( fp, "0 0 0 srgb " );          /* black */
				break;
			}
		else
			fprintf( fp, "0 sgr " );

		fprintf( fp, "[ 1 1 ] 0 sd " );
		
		fprintf( fp, "%.2f %.2f m 0 %.2f rl s\n",
				 x_0 + s2d * ( m->position + cv->shift[ X ] ), y_0, h );
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
