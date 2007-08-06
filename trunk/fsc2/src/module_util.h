/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined MODULE_UTIL_HEADER
#define MODULE_UTIL_HEADER


int get_mode( void );

int get_check_state( void );

int get_batch_state( void );

bool check_user_request( void );

void stop_on_user_request( void );

void too_many_arguments( Var_T * /* v */ );

void no_query_possible( void );

long get_long( Var_T *      /* v       */,
               const char * /* snippet */  );

double get_double( Var_T *      /* v       */,
                   const char * /* snippet */  );

long get_strict_long( Var_T *      /* v       */,
                      const char * /* snippet */  );

bool get_boolean( Var_T * /* v */ );

Var_T *get_element( Var_T * /* v   */,
                    int     /* len */,
                    ...                );

double is_mult_ns( double       /* val  */,
                   const char * /* text */  );

char *translate_escape_sequences( char * /* str */ );

double experiment_time( void );

FILE *fsc2_fopen( const char * /* path */,
                  const char * /* mode */  );

int fsc2_fscanf( FILE *       /* stream */,
                 const char * /* format */,
                 ...                        );

size_t fsc2_fread( void  * /* ptr    */,
                   size_t  /* size   */,
                   size_t  /* nmemb  */,
                   FILE *  /* stream */  );

int fsc2_fprintf( FILE *       /* stream */,
                  const char * /* format */,
                  ...                        );

size_t fsc2_fwrite( void  * /* ptr    */,
                    size_t  /* size   */,
                    size_t  /* nmemb  */,
                    FILE *  /* stream */  );

int fsc2_fgetc( FILE * /* stream */ );

int fsc2_getc( FILE * /* stream */ );

char *fsc2_fgets( char * /* s      */,
                  int    /* size   */,
                  FILE * /* stream */  );

int fsc2_ungetc( int    /* c      */,
                 FILE * /* stream */  );

int fsc2_fseek( FILE * /* stream */,
                long   /* offset */,
                int    /* whence */  );

long fsc2_ftell( FILE * /* stream */ );

int fsc2_fputc( int    /* c      */,
                FILE * /* stream */  );

int fsc2_fputs( const char * /* s      */,
                FILE *       /* stream */  );

int fsc2_putc( int    /* c      */,
               FILE * /* stream */  );

int fsc2_fclose( FILE * /* stream */ );

const char *fsc2_config_dir( void );


#define MODULE_CALL_ESTIMATE   0.02   /* 20 ms per module function call -
                                         estimate for calculation of time in
                                         test run via experiment_time() */


#endif  /* ! MODULE_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
