/*
  $Id$
*/


#include "fsc2.h"


#define INCH         25.4         /* mm in an inch */
#define S2P          1            /* send to printer */
#define P2F          2            /* print to file */

#define A4_PAPER     0
#define A3_PAPER     1
#define Letter_PAPER 2
#define Legal_PAPER  3


FD_print *print_form;

char *cmd = NULL;                 /* for storing the last print command */
int print_type = S2P;             /* the way to print: S2P or P2F */

int paper_type = A4_PAPER;
double paper_width;
double paper_height;

bool print_with_color = UNSET;

double x_0, y_0, w, h;            /* position and size of print area */
double margin = 25.0;             /* margin (in mm) to leave on all sides */


static int get_print_file( FILE **fp, char **name );
static void print_header( FILE *fp, char *name );
static void eps_make_scale( FILE *fp, void *cv, int coord );
static void eps_draw_curve_1d( FILE *fp, int i );
static void eps_draw_contour( FILE *fp, int cn );
static void do_print( char *name, const char *command );
static int start_printing( char **argv, char *name );



/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

void print_1d( FL_OBJECT *obj, long data )
{
	int type;
	FILE *fp;
	char *name = NULL;
	int i;


	data = data;

	fl_deactivate_object( obj );

	/* Find out about the way to print and get a file, then print the header */

   if ( ( type = get_print_file( &fp, &name ) ) == 0 )
   {
	   fl_activate_object( obj );
	   return;
   }

	print_header( fp, name );

	/* Set the area for the graph, leave a margin on all sides and,
       additionally, 20 mm (x-axis) / 25 (y-axis) for labels and tick marks */

	x_0 = margin + 25.0;              /* margin & 25 mm for the y-axis */
	y_0 = margin + 20.0;              /* margin & 20 mm for the x-axis */
	w = paper_height - margin - x_0;
	h = paper_width - margin - y_0;

	/* Draw the frame and the scales (just 0.5 mm outside the the area for the
       graph) */

	fprintf( fp, "0.05 slw\n" );
	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp s\n",
			 x_0 - 0.5, y_0 - 0.5, h + 1.0, w + 1.0, - ( h + 1.0 ) );

	for ( i = 0; i < G.nc; i++ )
	{
		if ( G.curve[ i ]->active )
			break;
	}

	if ( i != G.nc )
	{
		eps_make_scale( fp, ( void * ) G.curve[ i ], X );
		eps_make_scale( fp, ( void * ) G.curve[ i ], Y );
	}

	/* Restrict drawings to the frame */

	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp clip newpath\n",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	for ( i = G.nc - 1; i >= 0; i-- )
		eps_draw_curve_1d( fp, i );

	fprintf( fp, "showpage\n" );
	fclose( fp );

	if ( print_type == S2P )
		do_print( name, cmd );

	if ( name != NULL )
	{
		T_free( name );
		name = NULL;
	}

	fl_activate_object( obj );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

void print_2d( FL_OBJECT *obj, long data )
{
	int type;
	FILE *fp;
	char *name = NULL;


	data = data;

	fl_deactivate_object( obj );

	if ( G.active_curve == -1 )
	{
		fl_show_alert( "Error", "Can't print - no curve is shown.", NULL, 1 );
		fl_activate_object( obj );
		return;
	}

	/* Find out about the way to print and get a file, then print the header */

	if ( ( type = get_print_file( &fp, &name ) ) == 0 )
	{
		fl_activate_object( obj );
		return;
	}

	print_header( fp, name );

	/* Set the area for the graph, leave a margin on all sides and,
       additionally, 20 mm (x-axis) / 25 (y-axis) for labels and tick marks */

	x_0 = margin + 25.0;
	y_0 = margin + 20.0;
	w = paper_height - margin - x_0;
	h = paper_width - margin - y_0;

	/* Draw the frame and the scales */

	fprintf( fp, "0.05 slw\n" );
	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp s\n",
			 x_0 - 0.5, y_0 - 0.5, h + 1.0, w + 1.0, - ( h + 1.0 ) );

	eps_make_scale( fp, G.curve_2d[ G.active_curve ], X );
	eps_make_scale( fp, G.curve_2d[ G.active_curve ], Y );

	/* Restrict drawings to the frame */

	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp clip newpath\n",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	eps_draw_contour( fp, G.active_curve );

	fprintf( fp, "showpage\n" );
	fclose( fp );

	if ( print_type == S2P )
		do_print( name, cmd );

	if ( name != NULL )
	{
		T_free( name );
		name = NULL;
	}

	fl_activate_object( obj );
}


/*--------------------------------------------------------------*/
/* Shows a form that allows the user to select the way to print */
/* and, for printing to file mode, select the file              */
/*--------------------------------------------------------------*/

static int get_print_file( FILE **fp, char **name )
{
	FL_OBJECT *obj;
	char filename[ ] = P_tmpdir "/fsc2.eps.XXXXXX";
	struct stat stat_buf;


	/* Create the form for print setup */

	print_form = create_form_print( );

	/* If a printer command has already been set put it into the input object,
	   otherwise set default command */

	if ( cmd != NULL )
	{
		fl_set_input( print_form->s2p_input, cmd );
		T_free( cmd );
		cmd = NULL;
	}
	else
		fl_set_input( print_form->s2p_input, "lpr -h" );

	/* Set the paper type to the last one used (or A4 at the start) */

	switch ( paper_type )
	{
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
		fl_deactivate_object( print_form->p2f_group );
	}
	else
	{
		fl_set_button( print_form->s2p_button, 0 );
		fl_set_button( print_form->p2f_button, 1 );
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
			cmd = get_string_copy( fl_get_input( print_form->s2p_input ) );
		}
		else
		{
			print_type = P2F;
			*name = get_string_copy( fl_get_input( print_form->p2f_input ) );
		}
		TRY_SUCCESS;
	}
	CATCH (EXCEPTION )
		obj = print_form->cancel_button;

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
		if ( *name != NULL )
		{
			T_free( *name );
			*name = NULL;
		}
		return 0;
	}

	/* In send-to-printer mode try to create and open a temporary file */

	if ( print_type == S2P ) 
	{
		/* Create a temporary file */

		if ( mkstemp( filename ) < 0 ||
			 ( *fp = fopen( filename, "w" ) ) == NULL )
		{
			fl_show_alert( "Error", "Sorry, can't open a temporary file.",
						   NULL, 1 );
			return 0;
		}

		/* ... and store its name */

		TRY
		{
			*name = get_string_copy( filename );
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
			return 0;

		return S2P;          /* print mode is 'send to printer' */
	}

	/* In print-to-file mode ask for confirmation if the file already exists
	   and try to open it for writing */

	if  ( ( 0 == stat( *name, &stat_buf ) &&
			1 != show_choices( "The selected file does already exist:\n"
							   " You're sure you want to overwrite it?",
							   2, "Yes", "No", NULL, 2 ) ) ||
		  ( *fp = fopen( *name, "w" ) ) == NULL )
	{
		if ( *name != NULL )
		{
			T_free( *name );
			*name = NULL;
		}
		return 0;
	}

	return P2F;              /* print mode is 'print to file' */
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
		fl_activate_object( print_form->s2p_input );
		fl_deactivate_object( print_form->p2f_group );
		return;
	}

	if ( obj == print_form->p2f_button )
	{
		fl_deactivate_object( print_form->s2p_input );
		fl_activate_object( print_form->p2f_group );
		return;
	}

	if ( obj == print_form->p2f_browse )
	{
		fl_invalidate_fselector_cache( );
		fn = fl_show_fselector( "Select output EPS file:", NULL,
								"*.eps", NULL );
		if ( fn != NULL )
			fl_set_input( print_form->p2f_input, fn );
		return;
	}

	if ( obj == print_form->A4 )
	{
		paper_width = 210.0;
		paper_height = 297.0;
		paper_type = A4_PAPER;
		return;
	}

	if ( obj == print_form->A3 )
	{
		paper_width = 297.0;
		paper_height = 420.2;
		paper_type = A3_PAPER;
		return;
	}

	if ( obj == print_form->Letter )
	{
		paper_width = 215.9;
		paper_height = 279.4;
		paper_type = Letter_PAPER;
		return;
	}

	if ( obj == print_form->Legal )
	{
		paper_width = 215.9;
		paper_height = 355.6;
		paper_type = Legal_PAPER;
		return;
	}

	if ( obj == print_form->bw_button )
		print_with_color = UNSET;

	if ( obj == print_form->col_button )
		print_with_color = SET;
}


/*------------------------------------------------------------------------*/
/* Prints the header of the EPS-file as well as date, user and fsc2 logo. */
/*------------------------------------------------------------------------*/

static void print_header( FILE *fp, char *name )
{
	time_t d;


	/* Start writing the EPS header plus some routines into the file */

	d = time( NULL );

	fprintf( fp, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			     "%%%%Creator: fsc2, Dec 1999\n"
			     "%%%%CreationDate: %s", ctime( &d ) );

	/* Continue with rest of header */

	fprintf( fp, "%%%%Title: %s\n"
			     "%%%%BoundingBox: 0 0 %d %d\n"
			     "%%%%End Comments\n",
			     name,
			     ( int ) ( 72.0 * paper_width / INCH ),
			     ( int ) ( 72.0 * paper_height / INCH ) );

	/* Some (hopefully) helpful definitions */

	fprintf( fp, "%%%%BeginProlog\n"
			     "/s { stroke } bind def\n"
                 "/t { translate } bind def\n"
			     "/r { rotate} bind def\n"
			     "/cp { closepath } bind def\n"
				 "/m { moveto } bind def\n"
			     "/l { lineto } bind def\n"
			     "/rm { rmoveto } bind def\n"
			     "/rl { rlineto } bind def\n"
			     "/slw { setlinewidth } bind def\n"
			     "/srgb { setrgbcolor } bind def\n"
			     "/sd { setdash } bind def\n"
			     "/sw { stringwidth pop } bind def\n"
				 "/sf { exch findfont exch scalefont setfont } bind def\n"
			     "/gs { gsave } bind def\n"
			     "/gr { grestore } bind def\n"
			     "/sgr { setgray } bind def\n"
			     "/slc { setlinecap } bind def\n"
			     "/ch { gs newpath 0 0 moveto\n"
			     "      false charpath flattenpath pathbbox\n"
                 "      exch pop 3 -1 roll pop exch pop gr } bind def\n"
				 "/cw { gs newpath 0 0 moveto\n"
	             "      false charpath flattenpath pathbbox\n"
	             "      pop exch pop exch pop gr } bind def\n"
			     "/fsc2 { gs /Times-Roman 6 sf\n"
			     "        (fsc2) ch sub 6 sub exch\n"
			     "        (fsc2) cw sub 4 sub exch m\n"
			     "        1 -0.025 0 { sgr gs (fsc2) show gr\n"
			     "        -0.025 0.025 rm } for\n"
			     "        1 setgray (fsc2) show gr } bind def\n"
			     "%%%%EndProlog\n" );

	/* Rotate and shift for landscape format, scale to mm units, set font
	   and draw logo, date and user name */

	fprintf( fp, "%f dup scale 90 r 0 %f t\n", 72.0 / INCH, - paper_width );
	fprintf( fp, "%f %f fsc2\n", paper_height, paper_width );
	fprintf( fp, "/Times-Roman 4 sf\n" );
	fprintf( fp, "5 5 m (%s %s) show\n", ctime( &d ),
			 getpwuid( getuid( ) )->pw_name );
}


/*----------------------------------------------*/
/* Draws the scales for both 1D and 2D graphics */
/*----------------------------------------------*/

static void eps_make_scale( FILE *fp, void *cv, int coord )
{
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order,              /* and its order of magnitude */
		   mag;
	double d_delta_fine,       /* distance between small ticks (in points) */
		   d_start_fine,       /* position of first small tick (in points) */
		   d_start_medium,     /* position of first medium tick (in points) */
		   d_start_coarse,     /* position of first large tick (in points) */
		   cur_p;              /* loop variable with position */
	int medium_factor,         /* number of small tick spaces between medium */
		coarse_factor;         /* and large tick spaces */
	int medium,                /* loop counters for medium and large ticks */
		coarse;
	double rwc_start,          /* rwc value of first point */
		   rwc_start_fine,     /* rwc value of first small tick */
		   rwc_start_medium,   /* rwc value of first medium tick */
		   rwc_start_coarse;   /* rwc value of first large tick */
	double rwc_coarse;
	double x, y;
	char lstr[ 128 ];
	double s2d[ 2 ];


	if ( G.dim == 1 )
	{
		s2d[ X ] = w * ( ( Curve_1d * ) cv )->s2d[ X ] / G.canvas.w;
		s2d[ Y ] = h * ( ( Curve_1d * ) cv )->s2d[ Y ] / G.canvas.h;
	}
	else
	{
		s2d[ X ] = w * ( ( Curve_2d * ) cv )->s2d[ X ] / G.canvas.w;
		s2d[ Y ] = h * ( ( Curve_2d * ) cv )->s2d[ Y ] / G.canvas.h;
	}

	/* The distance between the smallest ticks should be about 6 points -
	   calculate the corresponding delta in real word units */

	rwc_delta = 1.5 * fabs( G.rwc_delta[ coord ] ) / s2d[ coord ];

	/* Now scale this distance to the interval [ 1, 10 [ */

	mag = floor( log10( rwc_delta ) );
	order = pow( 10.0, mag );
	modf( rwc_delta / order, &rwc_delta );

	/* Now get a `smooth' value for the real world ticks distance, i.e. either
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

	d_delta_fine = s2d[ coord ] * rwc_delta / fabs( G.rwc_delta[ coord ] );

	/* `rwc_start' is the first value in the display (i.e. the smallest x or y
	   value still shown in the frame), `rwc_start_fine' the position of the
	   first small tick (both in real world coordinates) and, finally,
	   `d_start_fine' is the same position but in points */

	if ( G.dim == 1 )
		rwc_start = G.rwc_start[ coord ]
		        - ( ( Curve_1d * ) cv )->shift[ coord ] * G.rwc_delta[ coord ];
	else
		rwc_start = G.rwc_start[ coord ]
		        - ( ( Curve_2d * ) cv )->shift[ coord ] * G.rwc_delta[ coord ];

	if ( G.rwc_delta[ coord ] < 0 )
		rwc_delta *= -1.0;

	modf( rwc_start / rwc_delta, &rwc_start_fine );
	rwc_start_fine *= rwc_delta;

	d_start_fine = s2d[ coord ] * ( rwc_start_fine - rwc_start ) /
		                                                  G.rwc_delta[ coord ];
	if ( lround( d_start_fine ) < 0 )
		d_start_fine += d_delta_fine;

	/* Calculate start index (in small tick counts) of first medium tick */

	modf( rwc_start / ( medium_factor * rwc_delta ), &rwc_start_medium );
	rwc_start_medium *= medium_factor * rwc_delta;

	d_start_medium = s2d[ coord ] * ( rwc_start_medium - rwc_start ) /
			                                              G.rwc_delta[ coord ];
	if ( lround( d_start_medium ) < 0 )
		d_start_medium += medium_factor * d_delta_fine;

	medium = lround( ( d_start_fine - d_start_medium ) / d_delta_fine );

	/* Calculate start index (in small tick counts) of first large tick */

	modf( rwc_start / ( coarse_factor * rwc_delta ), &rwc_start_coarse );
	rwc_start_coarse *= coarse_factor * rwc_delta;

	d_start_coarse = s2d[ coord ] * ( rwc_start_coarse - rwc_start ) /
			                                              G.rwc_delta[ coord ];
	if ( lround( d_start_coarse ) < 0 )
	{
		d_start_coarse += coarse_factor * d_delta_fine;
		rwc_start_coarse += coarse_factor * rwc_delta;
	}

	coarse = lround( ( d_start_fine - d_start_coarse ) / d_delta_fine );

	/* Now, finally we got everything we need to draw the axis... */

	rwc_coarse = rwc_start_coarse - coarse_factor * rwc_delta;

	if ( coord == X )
	{
		/* Draw the x-axis label string */

		if ( G.label[ X ] != NULL )
			fprintf( fp, "%f (%s) cw sub %f m (%s) show\n",
					 paper_height - margin, G.label[ X ], margin,
					 G.label[ X ] );

		y = y_0;

		/* Set the minimum start position of a x-axis tick label */

		fprintf( fp, "%f\n", d_start_fine - 45.0 );

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < w; 
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			x = cur_p + x_0;

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				fprintf( fp, "%f %f m\n 0 -3.5 rl s\n", x, y - 0.5 );
				rwc_coarse += coarse_factor * rwc_delta;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				fprintf( fp, "dup %f (%s) cw 0.5 mul sub 3 sub lt {\n",
						 x, lstr );
				fprintf( fp, "%f (%s) cw 0.5 mul sub %f 5 sub (%s) ch sub m\n"
						 "(%s) show pop %f (%s) cw 0.5 mul add } if\n",
						 x, lstr, y - 1.0, lstr, lstr, x, lstr );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				fprintf( fp, "%f %f m\n 0 -2.5 rl s\n", x, y - 0.5 );
			else                                       /* short line */
				fprintf( fp, "%f %f m\n 0 -1.5 rl s\n", x, y - 0.5 );
		}

		fprintf( fp, "pop\n" );
	}
	else
	{
		/* Draw the y-axis label string */

		if ( G.label[ Y ] != NULL )
			fprintf( fp, "gs %f (%s) ch add %f (%s) cw sub t 90 r 0 0 m (%s) "
					 "show gr\n", margin, G.label[ Y ], paper_width - margin,
					 G.label[ Y ], G.label[ Y ] );

		x = x_0;

		/* Draw all the ticks and numbers */

		for ( cur_p = d_start_fine; cur_p < h;
			  medium++, coarse++, cur_p += d_delta_fine )
		{
			y = cur_p + y_0;

			if ( coarse % coarse_factor == 0 )         /* long line */
			{
				fprintf( fp, "%f %f m\n-3.5 0 rl s\n", x - 0.5, y );
				rwc_coarse += coarse_factor * rwc_delta;

				make_label_string( lstr, rwc_coarse, ( int ) mag );
				fprintf( fp, "%f (%s) cw sub 5 sub %f (%s) ch 0.5 mul "
						 "sub m (%s) show\n", x_0 - 0.5, lstr, y, lstr, lstr );
			}
			else if ( medium % medium_factor == 0 )    /* medium line */
				fprintf( fp, "%f %f m\n-2.5 0 rl s\n", x - 0.5, y );
			else                                      /* short line */
				fprintf( fp, "%f %f m\n-1.5 0 rl s\n", x - 0.5, y );

		}
	}
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void eps_draw_curve_1d( FILE *fp, int i )
{
	Curve_1d *cv = G.curve[ i ];
	double s2d[ 2 ];
	long k;


	s2d[ X ] = w * cv->s2d[ X ] / G.canvas.w;
	s2d[ Y ] = h * cv->s2d[ Y ] / G.canvas.h;

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

	k = 0;
	while ( ! cv->points[ k ].exist && k < G.nx )
		k++;

	if ( k >= G.nx )                /* is there only just one ? */
		return;

	fprintf( fp, "%f %f m\n", x_0 + s2d[ X ] * ( k + cv->shift[ X ] ),
			 y_0 + s2d[ Y ] * ( cv->points[ k ].v + cv->shift[ Y ] ) );
	k++;

	/* Draw all other points */

	for ( ; k < G.nx; k++ )
	{
		if (  cv->points[ k ].exist )
			fprintf( fp, "%f %f l\n", x_0 + s2d[ X ] * ( k + cv->shift[ X ] ),
					 y_0 + s2d[ Y ] * ( cv->points[ k ].v + cv->shift[ Y ] ) );
	}

	fprintf( fp, "s gr\n" );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void eps_draw_contour( FILE *fp, int cn )
{
	Curve_2d *cv = G.curve_2d[ cn ];
	double s2d[ 2 ] = { w * cv->s2d[ X ] / G.canvas.w,
						h * cv->s2d[ Y ] / G.canvas.h };
	double dw = s2d[ X ] / 2,
		   dh = s2d[ Y ] / 2;
	double curp;
	double z;
	long i, j, k;
	double z1, z2, g;
	int rgb[ 3 ];


	fprintf( fp, "gs\n" );

	/* Print areas gray for which there are no data */

	curp = s2d[ X ] * cv->shift[ X ];
	if ( curp - dw > 0 )
		fprintf( fp, "0.5 sgr\n"
				 "%f %f m\n"
				 "%f 0 rl\n"
				 "0 %f rl\n"
				 "%f 0 rl\n"
				 "cp fill\n",
				 x_0, y_0, curp - dw, h, - ( curp - dw ) );

	curp = s2d[ X ] * ( G.nx - 1 + cv->shift[ X ] );
	if ( w > curp + dw )
		fprintf( fp, "0.5 sgr\n"
				 "%f %f m\n"
				 "0 %f rl\n"
				 "%f 0 rl\n"
				 "0 %f rl\n"
				 "cp fill\n",
				 x_0 + curp + dw , y_0, h, w - ( curp + dw ), - h );

	curp = s2d[ Y ] * cv->shift[ Y ];
	if ( curp - dh > 0 )
		fprintf( fp, "0.5 sgr\n"
				 "%f %f m\n"
				 "%f 0 rl\n"
				 "0 %f rl\n"
				 "%f 0 rl cp\n"
				 "fill\n",
				 x_0, y_0, w, curp - dh, - w );

	curp = s2d[ Y ] * ( G.ny - 1 + cv->shift[ Y ] );
	if ( h > curp + dh )
		fprintf( fp, "0.5 sgr\n"
				 "%f %f m\n"
				 "%f 0 rl\n"
				 "0 %f rl\n"
				 "%f 0 rl\n"
				 "cp fill\n",
				 x_0, y_0 + curp + dh , w, h - ( curp + dh ), - w );

	/* Now draw the data */

	for ( g = 1.0, z = 0.0; z <= 1.0; g -= 0.045, z += 0.05 )
		for ( k = 0, j = 0; j < G.ny; j++ )
			for ( i = 0; i < G.nx; i++, k++ ) 
			{
				if ( ! cv->points[ k ].exist )
				{
					fprintf( fp, "0.5 sgr\n"
							 "%f %f m\n"
							 "0 %f 2 copy rl\n"
							 "%f 0 rl\n"
							 "neg rl cp fill\n",
							 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) - dw,
							 y_0 + s2d[ Y ] * ( j + cv->shift[ X ] ) - dh,
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
						if ( print_with_color )
						{
							i2rgb( z, rgb );
							fprintf( fp, "%f %f %f srgb ",
									 ( double ) rgb[ RED ]   / 255.0,
									 ( double ) rgb[ GREEN ] / 255.0,
									 ( double ) rgb[ BLUE ]  / 255.0 );
						}
						else
							fprintf( fp, "%f sgr ", g );
						fprintf( fp, "%f %f m 0 %f rl s\n",
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
						if ( print_with_color )
						{
							i2rgb( z, rgb );
							fprintf( fp, "%f %f %f srgb ",
									 ( double ) rgb[ RED ]   / 255.0,
									 ( double ) rgb[ GREEN ] / 255.0,
									 ( double ) rgb[ BLUE ]  / 255.0 );
						}
						else
							fprintf( fp, "%f sgr ", g );
						fprintf( fp, "%f %f m %f 0 rl s\n",
								 x_0 + s2d[ X ] * ( i + cv->shift[ X ] ) - dw,
								 y_0 + s2d[ Y ] * ( j + cv->shift[ Y ] ) + dh,
								 2.0 * dw );
					}
				}
			}

	fprintf( fp, "gr\n" );
}


/* Some implementation details: If we want to send data directly to the
   printer the best way would seem to be to simply create a pipe via popen()
   to lpr (or whatever the appropriate print command is) and then write the
   data to be printed to lpr's standard input. Unfortunately, there's a
   problem with this approach: Even though the main program gets the SIGCHLD
   signal the exit status is already reaped by pclose(). Thus, if we're
   running an handler for SIGCHLD signals it will be triggered but the handler
   will wait for the death of another child instead of the process that exited
   due to the pclose(). While this might be OK as long as there are no other
   child processes, at least while the measurement is running there is at
   least one other child, the child we forked to do the measurement. Now,
   after the the pclose() call the parent process will hang in the SIGCHLD
   signal handler, waiting indefinitely for another child to exit, while the
   child process doing the measurement also hangs, waiting for the parent
   process to send signals allowing the measurement child process to send
   further data...

   So, instead we write all data to a temporary file and then fork another
   process. This process forks again and then execs lpr (or whatever printing
   command the user asked us to use). The first forked process waits until the
   printing process finishes and removes the temporary file before it exits.
   This double level of forking insures that the temporary file is really
   going to be deleted, even if the parent processes dies in the mean time -
   otherwise we would have to know the correct flags for the print command to
   make it delete this file by itself. */


static void do_print( char *name, const char *command )
{
	char *cptr;
	char *cmd_line = NULL;
	char **argv = NULL;
	int argc;
	

	/* Do a minimal sanity check of the print command */

	if ( command == NULL || *command == '\0' )
	{
		fl_show_alert( "Error", "Sorry, bad print command.", NULL, 1 );
		return;
	}

	/* Now let's try to assemble the command line, if this fails we can send
       an error message. */

	TRY
	{
		cmd_line = get_string_copy( command );

		argv = T_malloc( 3 * sizeof( char * ) );
		argv[ 0 ] = cptr = cmd_line;
		argc = 1;

		while ( ( cptr = strchr( cptr, ' ' ) ) != NULL )
		{
			if ( cptr < cmd_line + 2 )
			{
				fl_show_alert( "Error", "Sorry, bad print command.", NULL, 1 );
				THROW( EXCEPTION );
			}

			*cptr = '\0';
			while ( *++cptr == ' ' )
				;

			if ( *cptr != '\0' )
			{
				argv = T_realloc( argv, ( argc + 3 ) * sizeof( char * ) );
				argv[ argc++ ] = cptr;
			}
		};

		argv[ argc++ ] = name;          /* finally, append the file name */
		argv[ argc ] = NULL;            /* end of the argument list */

		/* Fork another process to do the printing */

		switch( fork( ) )
		{
			case -1 :              /* failure of fork() */
				fl_show_alert( "Error", "Sorry, can't print results.",
							   "Running out of resources.", 1 );
				THROW( EXCEPTION );

			case 0 :               /* the new child process */
				_exit( start_printing( argv, name ) );

			default :              /* the parent process */
			   break;
		}

		TRY_SUCCESS;
	}
	CATCH( EXCEPTION )
		unlink( name );              /* on failure delete the temporary file */

	if ( argv != NULL )
		T_free( argv );
	if ( cmd_line != NULL )
		T_free( cmd_line );
}


/*------------------------------------------------------------*/
/* Starts a `grand-parent' process to do the actual printing. */
/*------------------------------------------------------------*/

static int start_printing( char **argv, char *name )
{
	int status = EXIT_FAILURE;
	char *a;
	int i;


	signal( SIGCHLD, SIG_IGN );  /* we don't need SIGCHLD signals */

	switch( fork( ) )
	{
		case -1 :                /* failure of fork() */
			unlink( name );
			break;

		case 0 :                 /* the child finally execs....*/
			if ( execvp( argv[ 0 ], argv ) < 0 )
			{
				fprintf( stderr, "fsc2: print command failed:" );
				for ( i = 0, a = argv[ 0 ]; a != NULL; a = argv[ ++i ] )
					fprintf( stderr, " %s", a );
				fprintf( stderr, "\n" );
				_exit( EXIT_FAILURE );
			}

		default :                /* the parent waits for the child to finish */
			wait( &status );
			unlink( name );
	}

	return status;
}
