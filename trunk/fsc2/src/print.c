/*
  $Id$
*/


#include "fsc2.h"


#define INCH   25.4                    /* mm in an inch */

static double paper_width = 210.0;
static double paper_height = 297.0;

static double x_0, y_0, w, h;          /* position and size of the frame */

static int get_print_file( FILE **fp, char **name );
static void print_header( FILE *fp, char *name );
static void eps_make_scale( FILE *fp, void *cv, int coord );
static void eps_draw_curve_1d( FILE *fp, int i );



/* Some implementation details: If we want to send data directly to the
   printer it would seem to be the best idea simply create a pipe via popen()
   to lpr(1) and this way to write the data to be printed to lpr's standard
   input. Unfortunately, there seems to be a problem with this: Even though
   the program gets the SIGCHLD signal the exit status is already reaped by
   pclose(). Thus, if we're running an handler for SIGCHLD signals it will
   triggered but the handler will wait for the death of another child instead
   of the process that exited due to the pclose(). While this is ok as long as
   there are no other child processes while the measurement is running there
   is another child, the child we forked to do the measurement. Now, after the
   the pclose() call the parent process will hang in the SIGCHLD signal
   handler, waiting indefinitely for another child to exit, while to child
   process doing the measurement is also hanging, waiting for the parent
   process to send signals allowing the child process to send further data...
*/



void print_1d( FL_OBJECT *obj, long data )
{
	int print_type;
	FILE *fp;
	char *name;
	int i;


	obj = obj;
	data = data;

	/* Find out about the way to print and get a file, then print the header */

   if ( ( print_type = get_print_file( &fp, &name ) ) == 0 )
	   return;
	print_header( fp, name );

	x_0 = 50;
	y_0 = 45;
	w = paper_height - 75;
	h = paper_width - 70;

	/* Draw the frame and the scales */

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

	T_free( name );
}


void print_2d( FL_OBJECT *obj, long data )
{
	int print_type;
	FILE *fp;
	char *name;


	obj = obj;
	data = data;

	if ( G.active_curve == -1 )
	{
		fl_show_alert( "Error", "Can't print - no curve is shown.", NULL, 1 );
		return;
	}

	/* Find out about the way to print and get a file, then print the header */

	if ( ( print_type = get_print_file( &fp, &name ) ) == 0 )
		return;
	print_header( fp, name );

	x_0 = 55.0;
	y_0 = 45.0;
	w = paper_height - 25.0 - x_0;
	h = paper_width - 25.0 - y_0;

	/* Draw the frame and the scales */

	fprintf( fp, "0.05 slw\n" );
	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp s\n",
			 x_0 - 0.5, y_0 - 0.5, h + 1.0, w + 1.0, - ( h + 1.0 ) );

	eps_make_scale( fp, G.curve_2d[ G.active_curve ], X );
	eps_make_scale( fp, G.curve_2d[ G.active_curve ], Y );

	/* Restrict drawings to the frame */

	fprintf( fp, "%f %f m\n0 %f rl\n%f 0 rl\n0 %f rl cp clip newpath\n",
			 x_0 - 0.1, y_0 - 0.1, h + 0.2, w + 0.2, - ( h + 0.2 ) );

	fprintf( fp, "showpage\n" );
	fclose( fp );

	T_free( name );
}


int get_print_file( FILE **fp, char **name )
{
	if ( ( *fp = fopen( "xyz.eps", "w" ) ) == NULL )
		return 0;
	*name = get_string_copy( "xyz.eps" );
	return 1;
}


void print_header( FILE *fp, char *name )
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
			     "/slc { setlinecap } bind def\n"
			     "/ch { gs newpath 0 0 moveto\n"
			     "      false charpath flattenpath pathbbox\n"
                 "      exch pop 3 -1 roll pop exch pop gr } bind def\n"
				 "/cw { gs newpath 0 0 moveto\n"
	             "      false charpath flattenpath pathbbox\n"
	             "      pop exch pop exch pop gr } bind def\n"
			     "/fsc2 { gs m /Times-Roman 8 sf\n"
			     "        1 -0.025 0 { setgray gs (fsc2) show gr\n"
			     "        -0.025 0.025 rm } for\n"
			     "        1 setgray (fsc2) show gr } bind def\n"
			     "%%%%EndProlog\n" );

	/* Rotate and shift for landscape format, scale to mm units, set font
	   and draw logo, date and user name */

	fprintf( fp, "90 r\n0 %d t\n", ( int ) ( -72.0 * paper_width / INCH ) );
	fprintf( fp, "%f %f scale\n", 72.0 / INCH, 72.0 / INCH );
	fprintf( fp, "%f %f fsc2\n", paper_height - 18.0, paper_width - 12.0 );
	fprintf( fp, "/Times-Roman 4 sf\n" );
	fprintf( fp, "5 5 m (%s %s) show\n", ctime( &d ),
			 getpwuid( getuid( ) )->pw_name );
}


/*----------------------------------------------*/
/* Draws the scales for both 1D and 2D graphics */
/*----------------------------------------------*/

void eps_make_scale( FILE *fp, void *cv, int coord )
{
	double rwc_delta,          /* distance between small ticks (in rwc) */
		   order,              /* and its order of magitude */
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

	/* Now get a `smooth' value for the ticks distance, i.e. either 2, 2.5,
	   5 or 10 and convert it to real world coordinates */

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
	   value still shown in the canvas), `rwc_start_fine' the position of the
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
		if ( G.label[ X ] != NULL )
			fprintf( fp, "%f (%s) cw sub 25 m (%s) show\n",
					 paper_height - 25.0, G.label[ X ], G.label[ X ] );

		y = y_0;

		fprintf( fp, "-1000\n" );

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
		if ( G.label[ Y ] != NULL )
			fprintf( fp, "gs 25 (%s) ch add %f (%s) cw sub t 90 r 0 0 m (%s) "
					 "show gr\n", G.label[ Y ], paper_width - 25.0,
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


void eps_draw_curve_1d( FILE *fp, int i )
{
	Curve_1d *cv = G.curve[ i ];
	double s2d[ 2 ];
	long k;


	s2d[ X ] = w * cv->s2d[ X ] / G.canvas.w;
	s2d[ Y ] = h * cv->s2d[ Y ] / G.canvas.h;

	fprintf( fp, "gs 0.2 slw 1 slc\n" );

	/* Set the dash type for the different curves */

	switch ( i )
	{
		case 1 :
			fprintf( fp, "[ 1 0.8 ] 0 sd\n" );
			break;

		case 2 :
			fprintf( fp, "[ 0.001 0.5 ] 0 sd\n" );
			break;

		case 3 :
			fprintf( fp, "[ 0.5 0.5 0.0001 0.5 ] 0 sd\n" );
			break;
	}

	/* Find the very first point and move to it */

	k = 0;
	while ( ! cv->points[ k ].exist && k < G.nx )
		k++;

	if ( k >= G.nx )
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
