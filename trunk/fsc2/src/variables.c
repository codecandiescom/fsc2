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


/* local variables */

static bool is_sorted = UNSET;
static size_t num_vars = 0;


/* locally used functions */

static int comp_vars_1( const void *a, const void *b );
static int comp_vars_2( const void *a, const void *b );
static Var *vars_str_comp( int comp_type, Var *v1, Var *v2 );
static void free_vars( void );
static void vars_warn_new( Var *v );
static Var *vars_get_lhs_pointer( Var *v, int dim );
static long vars_calc_index( Var *a, Var *v );
static Var *vars_setup_new_array( Var *v, int dim );
static void vars_ass_from_var( Var *src, Var *dest );
static void vars_ass_from_ptr( Var *src, Var *dest );
static void vars_ass_from_trans_ptr( Var *src, Var *dest );


/*
  Variables and arrays (and also functions) are described by a structure

  typedef struct Var_
  {
	char *name;                         // name of the variable
	int  type;                          // type of the variable
	union
	{
		long   lval;                                // for integer values
		double dval;                                // for float values
		long   *lpnt;                               // for integer arrays
		double *dpnt;                               // for double arrays/
		char   *sptr;                               // for strings
		struct Var_ * ( * fnct )( struct Var_ * );  // for functions
		struct Var_ *vptr;                          // for array references
		void   *gptr;                               // generic pointer
	} val;
	int dim;                 // dimension of array
	unsigned int *sizes;     // array of sizes of dimensions
	size_t len;              // total len of array
	unsigned long flags;

	struct Var_ *from;
	struct Var_ *next;       // next variable in list or stack
	struct Var_ *prev;       // previous variable in list or stack
	bool is_on_stack;        // indicates where to look for the variable
  }

  For each variable and array such a structure is created and appended to
  the head of a linked list of all variables. When a variable is accessed,
  either to get its value or to assign a new value to it, this list is
  searched for a structure that contains an entry with the same name.

  Each permanent variable (transient variables are only used on the variable
  stack, see below) has a name assigned to it, starting with a character and
  followed by as many characters, numbers and underscores as the user wants
  it to have.

  Non-transient variables have one of the following types: They're either an
  integer or float variable (INT_VAR or FLOAT_VAR, which translates to C's
  long and double type) or an integer or float array (INT_CONT_ARR or
  FLOAT_CONT_ARR) of as many dimensions as the user thinks it should
  have. Since at the moment of creation of a variable its type can not be
  determined there is also a further type, UNDEF_VAR which is assigned to the
  variable until it is possible to find out if the variable is just a simple
  variable or an array.

  What kind of type a variable has, i.e. integer or float, is controlled via
  the function VAR_TYPE(), defined as macro in variables.h, which gets the
  passed the variable's name - if the function returns INT_VAR the variable
  is an integer (or the array is an integer array) otherwise its type is
  FLOAT. So, changing VAR_TYPE() and recompiling will change the behaviour of
  the program in this respect. Currently, as agreed with Axel and Thomas,
  VAR_TYPE returns INT_VAR for variables starting with a capital letters,
  thus making the variable an integer. But this is easily changed...

  Now, when the input file is read in, lines like

           A = B + 3.25;

  are found. While this form is convenient for to read a human, a reverse
  polish notation (RPN) for the right hand side of the assignment of the form

           A B 3.25 + =

  is much easier to handle for a computer. For this reason there exists the
  variable stack (which actually isn't a stack but a linked list).

  So, if the lexer finds an identifier like 'A', it first tries to get a
  pointer to the variable named 'A' in the variables list by calling
  vars_get(). If this fails (and we're just parsing the VARIABLES section of
  the EDL file, otherwise it would utter an error message) a new variable is
  created instead by a call to vars_new(). The resulting pointer is passed
  to the parser.

  Now, the parser sees the '=' bit of text and realizes it has to do an
  assignment and thus branches to evaluate the right hand side of the
  expression. In this branch, the parser sees the 'B' and pushes a copy of the
  variable 'B' onto the variables stack, containing just the necessary
  information, i.e. its type and value. It then finds the '+' and branches
  recursively to evaluate the expression to the right of the '+'. Here, the
  parser sees just the numerical value '3.25' and pushes it onto the variables
  stack, thus creating another transient stack variable with the value 3.25
  (and type FLOAT_VAL). Now the copy of 'B' and the variable with the value of
  3.25 are on the variables stack and the parser can continue by adding the
  values of both these variables. It does so by calling vars_add(), creating
  another transient stack variable for the result and removing both the
  variables used as arguments. It finally returns to the state it started
  from, the place where it found the 'A =' bit, with a stack variable holding
  the result of the evaluation of the right hand side. All left to be done now
  is to call vars_assign() which assigns the value of the stack variable to
  'A'. The stack variable with the right hand side result is than removed from
  the stack. If we're a bit paranoid we can make sure everything worked out
  fine by checking that the variable stack is now empty. Quite simple, isn't
  it?

  What looks like a lot of complicated work to do something rather simple
  has the advantage that, due to its recursive nature, it can be used
  without any changes for much more complicated situations. Instead of the
  value 3.25 we could have a complicated expression to add to 'B', which
  will be handled auto-magically by a deeper recursion, thus breaking up the
  complicated task into small, simple tasks, that can be handled easily.
  Also, 'B' could be a more complicated expression instead which would be
  handled in the same way.

  Now, what about arrays? If the lexer finds an identifier (it doesn't know
  about the difference between variables and arrays) it searches the variables
  list and if it doesn't find an entry with the same name it creates a new one
  (again, as long as we're in the VARIABLES section where defining new
  variables and array is allowed). The parser in turn realizes that the user
  wants an array when it finds a string of tokens of the form

            variable_identifier [

  where 'variable_identifier' is a array name. It calls vars_arr_start()
  where, if the array is still completely new, the type of the array is set to
  INT_CONT_ARR or FLOAT_CONT_ARR (depending on the result of the macro
  VAR_TYPE(), see above). Finally, it pushes a transient variable onto the
  stack of type ARR_PTR with the 'from' element in the variable structure
  pointing to the original array. This transient variable serves as a kind of
  marker since the next thing the parser is going to do is to read all indices
  and push them onto the stack.

  The next tokens have to be numbers - either simple numbers or computed
  numbers (i.e. results of function calls or elements of arrays). These are
  the indices of the array. We've reached the end of the list of indices when
  the the closing bracket ']' is found in the input. Now the stack looks like
  this:

               last index ->    number      <- top of stack
                                number
                                number
               first index ->   number
                                ARR_PTR     <- bottom of stack

  i.e. on the top we've the indices (in reverse order) followed by the pointer
  to the array.  The next step depends if this is an access to an array
  element (i.e. it's found on the right hand side of an assignment) or if an
  array element is to be set (i.e. its on the left hand side). In the first
  case the function vars_arr_rhs() is called.

  Basically, what vars_arr_rhs() does is to take the indices and the pointer
  to the array from the stake, determine the value of the accessed array
  element and push its value as a variable onto the stack.

  If, on the other hand, the array is found on the left hand side of an
  assignment, vars_arr_lhs() is called. Again, from the indices the element of
  the array to be set is calculated, but, since the the right and side of the
  assignment is not known yet, again an ARR_PTR transient variable is pushed
  onto the stack with the generic pointer 'gptr' in the union in the variables
  structure pointing to the accessed element and the 'from' field pointing to
  the array. After the calculation of the right hand side, i.e. the value to
  be assigned to the array element, vars_assign() is called with both the
  calculated value and the ARR_PTR on the top of the stack. All vars_assign()
  has to do is to stuff the value into the location of the accessed element
  stored in the ARR_PTR and remove both transient variables from the stack.

  Things get a bit more complicated if the array on the left hand side is
  completely new (and we're still in the VARIABLES section where defining
  new variables and arrays is allowed). In this case, the indices aren't
  supposed to mean a certain element but are the sizes of the dimensions of
  the array.  If the array is new, vars_arr_lhs() calls vars_setup_new_array()
  instead of doing the normal element lookup. In this routine the indices
  are interpreted as sizes for the dimensions of the array and memory is
  allocated for the elements of the array. It returns a ARR_PTR to the array
  with the NEED_INIT flag in the 'flags' element of the variables structure
  (of the transient variable) is set, a list of data (in curly brackets,
  i.e. '{' and '}' following as an assignment will be interpreted as data
  for initializing the array.

  But beside these fixed sized arrays there are also variable sized arrays.
  These are needed e.g. for storing data sets received from the transient
  recorder where it is sometimes impossible to know the length of the data set
  in advance. Only the very last dimension of an array may be variable sized
  and making it variable sized is indicated by writing a star ('*') as the
  size of this dimension. In contrast to fixed sized arrays, variable sized
  arrays cannot be initialized and the very first assignment to such an array
  must be an array slice, i.e. an one-dimensional array. This is done by
  either assigning an existing array or by assigning the data from an
  array-returning function.

*/


/*----------------------------------------------------------------------*/
/* vars_get() is called when a token is found that might be a variable  */
/* identifier. The function checks if it is an already defined variable */
/* and in this case returns a pointer to the corresponding structure,   */
/* otherwise it returns NULL.                                           */
/* ->                                                                   */
/*   * string with name of variable                                     */
/* <-                                                                   */
/*   * pointer to VAR structure or NULL                                 */
/*----------------------------------------------------------------------*/

Var *vars_get( char *name )
{
	Var *ptr;


	/* Try to find the variable with the name passed to the function */

	if ( is_sorted )
		return bsearch( name, EDL.Var_List, num_vars, sizeof *EDL.Var_List,
						comp_vars_2 );
	else
		for ( ptr = EDL.Var_List; ptr != NULL; ptr = ptr->next )
			if ( ! strcmp( ptr->name, name ) )
				return ptr;

	return NULL;
}


/*----------------------------------------------------------------*/
/* Function is called after the VARIABLES section has been parsed */
/* to sort the list of variables just created so that a variable  */
/* can be found using a binary search instead of running through  */
/* the whole linked list each time (that this is much faster is   */
/* probably just wishful thinking...).                            */
/*----------------------------------------------------------------*/

void vars_sort( void )
{
	Var *ptr,
		*next_ptr,
		*new_var_list;
	size_t i;


	/* Find out how many variables exist */

	for ( num_vars = 0, ptr = EDL.Var_List; ptr != NULL;
		  ptr = ptr->next, num_vars++ )
		/* empty */ ;

	/* Nothing to sort if there are less than two variables */

	if ( num_vars < 2 )
	{
		is_sorted = SET;
		return;
	}

	/* Get the variables into a continous block, then sort them according
	   to the variable names */

	new_var_list = T_malloc( num_vars * sizeof *new_var_list );
	for ( i = 0, ptr = EDL.Var_List; i < num_vars; i++, ptr = next_ptr )
	{
		memcpy( new_var_list + i, ptr, sizeof *new_var_list );
		next_ptr = ptr->next;
		T_free( ptr );
	}

	EDL.Var_List = new_var_list;

	qsort( EDL.Var_List, num_vars, sizeof *EDL.Var_List, comp_vars_1 );

	/* Reset the next and prev pointers */

	for ( i = 0, ptr = EDL.Var_List; i < num_vars; ptr++, i++ )
	{
		ptr->next = ptr + 1;
		ptr->prev = ptr - 1;
	}

	EDL.Var_List->prev = NULL;
	( --ptr )->next = NULL;

	/* Set a flag that indicates that the list has been sorted */

	is_sorted = SET;
}


/*----------------------------------------------------*/
/* Function used for qsort'ing the list of variables. */
/*----------------------------------------------------*/

static int comp_vars_1( const void *a, const void *b )
{
	return strcmp( ( ( const Func * ) a )->name,
				   ( ( const Func * ) b )->name );
}


/*------------------------------------------*/
/* Function used in bsearch for a variable. */
/*------------------------------------------*/

static int comp_vars_2( const void *a, const void *b )
{
	return strcmp( ( const char * ) a , ( ( const Func * ) b )->name );
}


/*----------------------------------------------------------*/
/* vars_new() sets up a new variable by getting memory for  */
/* a variable structure and setting the important elements. */
/* ->                                                       */
/*   * string with variable name                            */
/* <-                                                       */
/*   * pointer to variable structure                        */
/*----------------------------------------------------------*/

Var *vars_new( char *name )
{
	Var *vp;


	/* Get memory for a new structure and for storing the name */

	vp = T_malloc( sizeof *vp );
	vp->name        = NULL;
	vp->is_on_stack = UNSET;
	vp->sizes       = NULL;
	vp->name        = T_strdup( name );

	/* Set relevant entries in the new structure and make it the very first
	   element in the list of variables */

	vp->flags = NEW_VARIABLE;        /* set flag to indicate it's new */
	vp->type = UNDEF_VAR;            /* set type to still undefined */

	vp->next = EDL.Var_List;         /* set pointer to it's successor */
	if ( EDL.Var_List != NULL )      /* set previous pointer in successor */
		EDL.Var_List->prev = vp;     /* (if this isn't the very first) */
    EDL.Var_List = vp;               /* make it the head of the list */

	return vp;                       /* return pointer to the structure */
}


/*---------------------------------------------------------------------*/
/* vars_add() adds two variables or array slices and pushes the result */
/* onto the stack.                                                     */
/* ->                                                                  */
/*    * pointers to two variable structures                            */
/* <-                                                                  */
/*    * pointer to a new transient variable on the variables stack     */
/*---------------------------------------------------------------------*/

Var *vars_add( Var *v1, Var *v2 )
{
	Var *new_var = NULL;
	char *new_str;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    STR_VAR | INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | STR_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Add the values while taking care to get the type right */

	if ( v1->type & ( INT_CONT_ARR | FLOAT_CONT_ARR | INT_ARR |
					  FLOAT_ARR | ARR_REF ) )
		v1 = vars_array_check( v1, v2 );

	switch ( v1->type )
	{
		/* Concatenate two strings with the '+ operator */

		case STR_VAR :
			if ( v2->type != STR_VAR )
			{
				print( FATAL, "Can't add a string and a number.\n" );
				THROW( EXCEPTION );
			}

			new_str = T_malloc( strlen( v1->val.sptr )
								+ strlen( v2->val.sptr ) + 1 );
			strcpy( new_str, v1->val.sptr );
			strcat( new_str, v2->val.sptr );
			new_var = vars_push( STR_VAR, new_str );
			T_free( new_str );
			break;

		case INT_VAR :
			new_var = vars_add_to_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_add_to_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			vars_array_check( v1, v2 );
			new_var = vars_add_to_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			new_var = vars_add_to_float_arr( v1, v2 );
			break;

		case ARR_REF :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_add_to_int_arr( v1->from, v2 );
			else
				new_var = vars_add_to_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_add_to_int_arr( v1, v2 );
			else
				new_var = vars_add_to_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-------------------------------------------------------------------*/
/* vars_sub() subtracts two variables or array slices and pushes the */
/* result onto the stack.                                            */
/* ->                                                                */
/*    * pointers to two variable structures                          */
/* <-                                                                */
/*    * pointer to a new transient variable on the variables stack   */
/*-------------------------------------------------------------------*/

Var *vars_sub( Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Subtract the values while taking care to get the type right */

	switch ( v1->type )
	{
		case INT_VAR :
			new_var = vars_sub_from_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_sub_from_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_sub_from_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_sub_from_float_arr( v1, v2 );
			break;

		case ARR_REF :
			v1 = vars_array_check( v1, v2 );
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_sub_from_int_arr( v1->from, v2 );
			else
				new_var = vars_sub_from_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_sub_from_int_arr( v1, v2 );
			else
				new_var = vars_sub_from_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-----------------------------------------------------------------*/
/* vars_mult() multiplies two variables or array slices and pushes */
/* the result onto the stack.                                      */
/* ->                                                              */
/*    * pointers to two variable structures                        */
/* <-                                                              */
/*    * pointer to a new transient variable on the variables stack */
/*-----------------------------------------------------------------*/

Var *vars_mult( Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Multiply the values while taking care to get the type right */

	switch ( v1->type )
	{
		case INT_VAR :
			new_var = vars_mult_by_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_mult_by_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_mult_by_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_mult_by_float_arr( v1, v2 );
			break;

		case ARR_REF :
			v1 = vars_array_check( v1, v2 );
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_mult_by_int_arr( v1->from, v2 );
			else
				new_var = vars_mult_by_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_mult_by_int_arr( v1, v2 );
			else
				new_var = vars_mult_by_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-----------------------------------------------------------------*/
/* vars_div() divides two variables or array slices and pushes the */
/* result onto the stack.                                          */
/* ->                                                              */
/*    * pointers to two variable structures                        */
/* <-                                                              */
/*    * pointer to a new transient variable on the variables stack */
/*-----------------------------------------------------------------*/

Var *vars_div( Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Divide the values while taking care to get the type right */

	switch ( v1->type )
	{
		case INT_VAR :
			new_var = vars_div_of_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_div_of_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_div_of_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_div_of_float_arr( v1, v2 );
			break;

		case ARR_REF :
			v1 = vars_array_check( v1, v2 );
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_div_of_int_arr( v1->from, v2 );
			else
				new_var = vars_div_of_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_div_of_int_arr( v1, v2 );
			else
				new_var = vars_div_of_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-------------------------------------------------------------------*/
/* vars_mod() pushes the modulo of the two variables or array slices */
/* onto the stack.                                                   */
/* ->                                                                */
/*    * pointers to two variable structures                          */
/* <-                                                                */
/*    * pointer to a new transient variable on the variables stack   */
/*-------------------------------------------------------------------*/

Var *vars_mod( Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Divide the values while taking care to get the type right */

	switch ( v1->type )
	{
		case INT_VAR :
			new_var = vars_mod_of_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_mod_of_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_mod_of_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_mod_of_float_arr( v1, v2 );
			break;

		case ARR_REF :
			v1 = vars_array_check( v1, v2 );
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_mod_of_int_arr( v1->from, v2 );
			else
				new_var = vars_mod_of_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_mod_of_int_arr( v1, v2 );
			else
				new_var = vars_mod_of_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*------------------------------------------------------------------*/
/* vars_pow() takes 'v1' to the power of 'v2' and pushes the result */
/* onto the stack.                                                  */
/* ->                                                               */
/*    * pointers to two variable structures                         */
/* <-                                                               */
/*    * pointer to a new transient variable on the variables stack  */
/*------------------------------------------------------------------*/

Var *vars_pow( Var *v1, Var *v2 )
{
	Var  *new_var = NULL;


	/* Make sure that 'v1' and 'v2' exist, are integers or float values or
	   arrays (or pointers thereto) */

	vars_check( v1, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				    INT_ARR | FLOAT_ARR | ARR_REF | ARR_PTR );
	vars_check( v2, INT_VAR | FLOAT_VAR | ARR_REF | ARR_PTR |
				    INT_ARR | FLOAT_ARR );


	/* Divide the values while taking care to get the type right */

	switch ( v1->type )
	{
		case INT_VAR :
			new_var = vars_pow_of_int_var( v1, v2 );
			break;

		case FLOAT_VAR :
			new_var = vars_pow_of_float_var( v1, v2 );
			break;

		case INT_CONT_ARR : case INT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_pow_of_int_arr( v1, v2 );
			break;

		case FLOAT_CONT_ARR : case FLOAT_ARR :
			v1 = vars_array_check( v1, v2 );
			new_var = vars_pow_of_float_arr( v1, v2 );
			break;

		case ARR_REF :
			v1 = vars_array_check( v1, v2 );
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_pow_of_int_arr( v1->from, v2 );
			else
				new_var = vars_pow_of_float_arr( v1->from, v2 );
			break;

		case ARR_PTR :
			if ( v1->from->type == INT_CONT_ARR )
				new_var = vars_pow_of_int_arr( v1, v2 );
			else
				new_var = vars_pow_of_float_arr( v1, v2 );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-----------------------------------------------------------------*/
/* vars_negate() negates the value of a variable or an array slice */
/* ->                                                              */
/*    * pointer a variable                                         */
/* <-                                                              */
/*    * pointer to the same variable but with its value negated    */
/*      (for array slices its a new transient array!)              */
/*-----------------------------------------------------------------*/

Var *vars_negate( Var *v )
{
	Var *new_var;
	size_t i;
	size_t len = 0;
	long *rlp;
	double *rdp;
	long *ilp = NULL;
	double *idp = NULL;


	/* Make sure that 'v' exists, has integer or float type or is an array
	   and has an value assigned to it */

	vars_check( v, INT_VAR | FLOAT_VAR | INT_CONT_ARR | FLOAT_CONT_ARR |
				   ARR_REF | ARR_PTR | INT_ARR | FLOAT_ARR );

	switch( v->type )
	{
		case INT_VAR :
			v->val.lval = - v->val.lval;
			return v;

		case FLOAT_VAR :
			v->val.dval = - v->val.dval;
			return v;

		case INT_CONT_ARR :
			if ( v->dim != 1 )
			{
				print( FATAL, "Arithmetic can be only done on array "
					   "slices.\n" );
				THROW( EXCEPTION );
			}
			len = v->len;
			ilp = v->val.lpnt;
			break;

		case FLOAT_CONT_ARR :
			if ( v->dim != 1 )
			{
				print( FATAL, "Arithmetic can be only done on array "
					   "slices.\n" );
				THROW( EXCEPTION );
			}
			len = v->len;
			idp = v->val.dpnt;
			break;

		case ARR_PTR :
			len = v->from->sizes[ v->from->dim - 1 ];
			if ( v->from->type == INT_CONT_ARR )
				ilp = ( long * ) v->val.gptr;
			else
				idp = ( double * ) v->val.gptr;
			break;

		case ARR_REF :
			if ( v->from->dim != 1 )
			{
				print( FATAL, "Argument of function 'int()' is neither a "
					   "number nor a 1-dimensional array.\n" );
				THROW( EXCEPTION );
			}

			len = v->from->sizes[ 0 ];
			if ( v->from->type == INT_CONT_ARR )
				ilp = v->from->val.lpnt;
			else
				idp = v->from->val.dpnt;
			break;

		case INT_ARR :
			len = v->len;
			ilp = v->val.lpnt;
			break;

		case FLOAT_ARR :
			len = v->len;
			idp = v->val.dpnt;
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	if ( ilp != NULL )
	{
		rlp = T_malloc( len * sizeof *rlp );
		for ( i = 0; i < len; ilp++, i++ )
			rlp[ i ] = - *ilp;
		new_var = vars_push( INT_ARR, rlp, len );
		new_var->flags |= v->flags & IS_DYNAMIC;
		T_free( rlp );
	}
	else
	{
		rdp = T_malloc( len * sizeof *rdp );
		for ( i = 0; i < len; idp++, i++ )
			rdp[ i ] = - *idp;
		new_var = vars_push( FLOAT_ARR, rdp, len );
		new_var->flags |= v->flags & IS_DYNAMIC;
		T_free( rdp );
	}

	vars_pop( v );
	return new_var;
}


/*--------------------------------------------------------------------------*/
/* vars_comp() is used for comparing the values of two variables. There are */
/* three types of comparison - it can be tested if two variables are equal, */
/* if the first one is less than the second or if the first is less or      */
/* equal than the second variable (tests for greater or greater or equal    */
/* can be done simply by switching the arguments).                          */
/* In comparisons between floating point numbers not only the numbers are   */
/* compared but, in order to reduce problems due to rounding errors, also   */
/* the numbers when the last significant bit is changed (if there's a       */
/* function in libc that allow us to do this...).                           */
/* ->                                                                       */
/*    * type of comparison (COMP_EQUAL, COMP_UNEQUAL, COMP_LESS,            */
/*      COMP_LESS_EQUAL, COMP_AND, COMP_OR or COMP_XOR)                     */
/*    * pointers to the two variables                                       */
/* <-                                                                       */
/*    * integer variable with value of 1 (true) or 0 (false) depending on   */
/*      the result of the comparison                                        */
/*--------------------------------------------------------------------------*/

Var *vars_comp( int comp_type, Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	/* If both variables are strings we can also do some kind of comparisons */

	if ( v1 && v1->type == STR_VAR && v2 && v2->type == STR_VAR )
		return vars_str_comp( comp_type, v1, v2 );

	/* Make sure that 'v1' and 'v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	switch ( comp_type )
	{
#if ! defined IS_STILL_LIBC1     /* libc2 *has* nextafter() */
		case COMP_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT == v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) == VALUE( v2 ) ||
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 == VALUE( v2 ) );
			break;

		case COMP_UNEQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != VALUE( v2 ) &&
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 != VALUE( v2 ) );
			break;

		case COMP_LESS :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT < v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) < VALUE( v2 ) &&
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 < VALUE( v2 ) );
			break;

		case COMP_LESS_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT <= v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) <= VALUE( v2 ) ||
									 nextafter( VALUE( v1 ), VALUE( v2 ) )
									 <= VALUE( v2 ) );
			break;
#else
		case COMP_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT == v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) == VALUE( v2 ) );
			break;

		case COMP_UNEQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != VALUE( v2 ) );

			break;

		case COMP_LESS :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT < v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) < VALUE( v2 ) );
			break;

		case COMP_LESS_EQUAL :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT <= v2->INT );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) <= VALUE( v2 ) );
			break;
#endif

		case COMP_AND :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != 0 && v2->INT != 0 );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != 0.0 &&
									          VALUE( v2 ) != 0.0 );
			break;

		case COMP_OR :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR, v1->INT != 0 || v2->INT != 0 );
			else
				new_var = vars_push( INT_VAR, VALUE( v1 ) != 0.0 ||
									          VALUE( v2 ) != 0.0 );
			break;

		case COMP_XOR :
			if ( v1->type == INT_VAR && v2->type == INT_VAR )
				new_var = vars_push( INT_VAR,
									 ( v1->INT != 0 && v2->INT == 0 ) ||
									 ( v1->INT == 0 && v2->INT != 0 ) );
			else
				new_var = vars_push( INT_VAR,
								( VALUE( v1 ) != 0.0 && VALUE( v2 ) == 0.0 ) ||
								( VALUE( v1 ) == 0.0 && VALUE( v2 ) != 0.0 ) );
			break;

		default:               /* this should never happen... */
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static Var *vars_str_comp( int comp_type, Var *v1, Var *v2 )
{
	Var *new_var = NULL;


	vars_check( v1, STR_VAR );
	vars_check( v2, STR_VAR );

	switch ( comp_type )
	{
		case COMP_EQUAL :
			vars_push( INT_VAR, strcmp( v1->val.sptr, v2->val.sptr ) ? 0 : 1 );
			break;
			
		case COMP_UNEQUAL :
			vars_push( INT_VAR, strcmp( v1->val.sptr, v2->val.sptr ) ? 1 : 0 );
			break;

		case COMP_LESS :
			vars_push( INT_VAR,
					   strcmp( v1->val.sptr, v2->val.sptr ) < 0 ? 1 : 0 );
			break;

		case COMP_LESS_EQUAL :
			vars_push( INT_VAR,
					   strcmp( v1->val.sptr, v2->val.sptr ) <= 0 ? 1 : 0 );
			break;

		case COMP_AND :
		case COMP_OR  :
		case COMP_XOR :
			print( FATAL, "Logical and, or and xor operators can't be used "
				   "with string variables.\n" );
			THROW( EXCEPTION );

		default:               /* this should never happen... */
			fsc2_assert( 1 == 0 );
			break;
	}

	/* Pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
}


/*-----------------------------------------------------------------------*/
/* vars_lnegate() does a logical negate of an integer or float variable, */
/* i.e. if the variables value is zero a value of 1 is returned in a new */
/* variable, otherwise 0.                                                */
/*-----------------------------------------------------------------------*/

Var *vars_lnegate( Var *v )
{
	Var *new_var;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( ( v->type == INT_VAR && v->INT == 0 ) ||
		 ( v->type == FLOAT_VAR && v->FLOAT == 0.0 ) )
		new_var = vars_push( INT_VAR, 1 );
	else
		new_var = vars_push( INT_VAR, 0 );

	vars_pop( v );

	return new_var;
}


/*-----------------------------------------------------------------------*/
/* vars_push() creates a new entry on the variable stack (which actually */
/* is not really a stack but a linked list) and sets its type and value. */
/* When the type of the variable is undefined and data is non-zero this  */
/* signifies that this is the start of a print statement - in this case  */
/* data points to the format string and has to be copied to the new      */
/* variables name.                                                       */
/*-----------------------------------------------------------------------*/

Var *vars_push( int type, ... )
{
	Var *new_stack_var, *stack;
	va_list ap;


	/* Get memory for the new variable to be appended to the stack, set its
	   type and initialize some fields */

	new_stack_var              = T_malloc( sizeof *new_stack_var );
	new_stack_var->is_on_stack = SET;
	new_stack_var->type        = type;
	new_stack_var->name        = NULL;
	new_stack_var->next        = NULL;
	new_stack_var->flags       = 0;

	/* Get the data for the new variable */

	va_start( ap, type );

	switch ( type )
	{
		case UNDEF_VAR :
			break;

		case INT_VAR :
			new_stack_var->val.lval = va_arg( ap, long );
			break;

		case FLOAT_VAR :
			new_stack_var->val.dval = va_arg( ap, double );
			break;

		case STR_VAR :
			new_stack_var->val.sptr = T_strdup( va_arg( ap, char * ) );
			break;

		case FUNC :
			new_stack_var->val.fnct = va_arg( ap, struct Func_ * );
			break;

		case ARR_PTR :
			new_stack_var->val.gptr = va_arg( ap, void * );
			new_stack_var->from = va_arg( ap, Var * );
			new_stack_var->flags |= new_stack_var->from->flags & IS_DYNAMIC;
			break;

		case INT_ARR :
			new_stack_var->val.lpnt = va_arg( ap, long * );
			new_stack_var->len = va_arg( ap, size_t );
			if ( new_stack_var->val.lpnt != NULL )
				new_stack_var->val.lpnt =
					         get_memcpy( new_stack_var->val.lpnt,
										 new_stack_var->len
										 * sizeof *new_stack_var->val.lpnt );
			else
				new_stack_var->val.lpnt =
								   T_calloc( new_stack_var->len,
											 sizeof *new_stack_var->val.lpnt );
			break;

		case FLOAT_ARR :
			new_stack_var->val.dpnt = va_arg( ap, double * );
			new_stack_var->len = va_arg( ap, size_t );
			if ( new_stack_var->val.dpnt != NULL )
				new_stack_var->val.dpnt =
							get_memcpy( new_stack_var->val.dpnt,
										new_stack_var->len
										*sizeof *new_stack_var->val.dpnt );
			else
				new_stack_var->val.dpnt =
								   T_calloc( new_stack_var->len,
											 sizeof *new_stack_var->val.dpnt );
			break;

		case ARR_REF :
			new_stack_var->from = va_arg( ap, Var * );
			break;

		default :
			fsc2_assert( 1 == 0 );
			break;
	}

	va_end( ap );

	/* Finally append the new variable to the stack */

	if ( ( stack = EDL.Var_Stack ) == NULL )
	{
		EDL.Var_Stack = new_stack_var;
		new_stack_var->prev = NULL;
	}
	else
	{
		while ( stack->next != NULL )
			stack = stack->next;
		stack->next = new_stack_var;
		new_stack_var->prev = stack;
	}

	/* Return the address of the new stack variable */

	return new_stack_var;
}


/*-----------------------------------------------------------------*/
/* vars_pop() checks if a variable is on the variable stack and    */
/* if it does removes it from the linked list making up the stack  */
/* To make running through a list of variables easier for some     */
/* functions, this function returns a pointer to the next variable */
/* on the stack (if the popped variable was on the stack and had a */
/* successor).                                                     */
/*-----------------------------------------------------------------*/

Var *vars_pop( Var *v )
{
	Var *ret = NULL;
#ifndef NDEBUG
	Var *stack,
		*prev = NULL;
#endif


	/* Check thast this is a variable that can be popped */

	if ( v == NULL || ! v->is_on_stack )
		return NULL;

#ifndef NDEBUG
	/* Figure out if 'v' is really on the stack - otherwise we have found
	   a new bug */

	for ( stack = EDL.Var_Stack; stack && stack != v; stack = stack->next )
		prev = stack;

	if ( stack == NULL )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
#endif

	/* Now get rid of the variable */

	ret = v->next;

	if ( v->prev != NULL )
	{
		v->prev->next = v->next;
		if ( v->next != NULL )
			v->next->prev = v->prev;
	}
	else
	{
		EDL.Var_Stack = v->next;
		if ( v->next != NULL )
			v->next->prev = NULL;
	}

	switch( v->type )
	{
		case STR_VAR :
			T_free( v->val.sptr );
			break;

		case FUNC :
			T_free( v->name );
			break;

		case INT_ARR :
			T_free( v->val.lpnt );
			break;

		case FLOAT_ARR :
			T_free( v->val.dpnt );
			break;
	}

	T_free( v );
	return ret;
}


/*----------------------------------------------------------------*/
/* vars_del_stack() deletes all entries from the variables stack. */
/*----------------------------------------------------------------*/

void vars_del_stack( void )
{
	while ( vars_pop( EDL.Var_Stack ) )
		/* empty */ ;
}


/*----------------------------------------------------------*/
/* vars_clean_up() deletes the variable and array stack and */
/* removes all variables from the list of variables         */
/*----------------------------------------------------------*/

void vars_clean_up( void )
{
	is_sorted = UNSET;
	num_vars = 0;
	vars_del_stack( );
	free_vars( );
}


/*---------------------------------------------------------------*/
/* free_vars() removes all variables from the list of variables. */
/*---------------------------------------------------------------*/

static void free_vars( void )
{
	Var *ptr;


	for ( ptr = EDL.Var_List; ptr != NULL; ptr = ptr->next )
	{
		if ( ptr->name != NULL )
			T_free( ptr->name );

		switch( ptr->type )
		{
			case INT_CONT_ARR :
				if ( ptr->sizes != NULL )
					T_free( ptr->sizes );
				if ( ! ( ptr->flags & NEED_ALLOC ) && ptr->val.lpnt != NULL )
					T_free( ptr->val.lpnt );
				break;

			case FLOAT_CONT_ARR :
				if ( ptr->sizes != NULL )
					T_free( ptr->sizes );
				if ( ! ( ptr->flags & NEED_ALLOC ) && ptr->val.dpnt != NULL )
					T_free( ptr->val.dpnt );
				break;

			case STR_VAR :
				if ( ptr->val.sptr != NULL )
					T_free( ptr->val.sptr );
				break;
		}
	}

	EDL.Var_List = T_free( EDL.Var_List );
}


/*-------------------------------------------------------------------*/
/* vars_check() first checks that the variable passed to it as the   */
/* first argument really exists and then tests if the variable has   */
/* the type passed as the second argument. The type to be tested for */
/* can not only be a simple type but tests for different types at    */
/* once are possible by specifying the types logically ored, i.e. to */
/* test if a variable has integer or floating point type use         */
/*               INT_VAR | FLOAT_VAR                                 */
/* as the second argument.                                           */
/*-------------------------------------------------------------------*/

void vars_check( Var *v, int type )
{
	int i;
	int t;
	const char *types[ ] = { "INTEGER", "FLOAT", "STRING", "INTEGER ARRAY",
							 "FLOAT ARRAY", "FUNCTION", "ARRAY POINTER",
							 "INTEGER ARRAY SLICE", "FLOAT ARRAY SLICE",
	                         "ARRAY REFERENCE" };


	/* Someone might call the function with a NULL pointer - handle this
	   gracefully, i.e. by throwing an exception and don't crash (eventhough
	   this clearly is a bug) */

	if ( v == NULL )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
				__FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	/* Being real paranoid we check that the variable exists at all -
	   probably this can vanish later. */

	fsc2_assert( vars_exist( v ) );

	/* Check that the variable has a value assigned to it */

	if ( v->type == UNDEF_VAR )
	{
		fsc2_assert( v->name != NULL );         /* just a bit paranoid ? */

		print( FATAL, "The accessed variable '%s' has not been assigned a "
			   "value.\n", v->name );
		THROW( EXCEPTION );
	}

	/* Check that the variable has the correct type */

	if ( ! ( v->type & type ) )
	{
		i = 1;
		t = v->type;
		while ( ! ( ( t >>= 1 ) & 1 ) )
			i++;
		print( FATAL, "Variable of type %s can't be used in this context.\n",
			   types[ i ] );
		THROW( EXCEPTION );
	}

	vars_warn_new( v );
}


/*-------------------------------------------------------------------*/
/* vars_warn_new() prints out a warning if a variable never has been */
/* assigned a value                                                  */
/*-------------------------------------------------------------------*/

static void vars_warn_new( Var *v )
{
 	if ( v->flags & NEW_VARIABLE )
	{
		fsc2_assert( v->name != NULL );            /* just a bit paranoid ? */
		print( WARN, "Variable '%s' has not been assigned a value.\n",
			   v->name );
	}
}


/*---------------------------------------------------------------*/
/* vars_exist() checks if a variable really exists by looking it */
/* up in the variable list or on the variable stack (depending   */
/* on what type of variable it is).                              */
/*---------------------------------------------------------------*/

bool vars_exist( Var *v )
{
	Var *lp;


	if ( v->is_on_stack )
		for ( lp = EDL.Var_Stack; lp != NULL && lp != v; lp = lp->next )
			/* empty */ ;
	else
	{
		if ( is_sorted )
			lp = bsearch( v->name, EDL.Var_List, num_vars,
						  sizeof *EDL.Var_List, comp_vars_2 );
		else
			for ( lp = EDL.Var_List; lp != NULL && lp != v; lp = lp->next )
				/* empty */ ;
	}

	/* If the variable can't be found in the lists we've got a problem... */

	fsc2_assert( lp == v );
	return lp == v;
}


/*-------------------------------------------------------------------*/
/* This function is called when a 'VAR_TOKEN [' combination is found */
/* in the input. For a new array it sets its type. Everything else   */
/* it does is pushing a variable with a pointer to the array onto    */
/* the stack.                                                        */
/*-------------------------------------------------------------------*/


Var *vars_arr_start( Var *v )
{
	Var *ret;

	/* <PARANOIA> Test if variable exists </PARANOIA> */

	fsc2_assert( vars_exist( v ) );

	/* Check if the array is completely new (type is still UNDEF_VAR). In this
	   case set its type and zero the pointer to the data so we know no memory
	   has been allocated yet. Otherwise check if it's really an array. */

	if ( v->type == UNDEF_VAR )
	{
		v->type = ( VAR_TYPE( v->name ) == INT_VAR ) ?
					INT_CONT_ARR : FLOAT_CONT_ARR;
		if ( v->type == INT_CONT_ARR )
			v->val.lpnt = NULL;
		else
			v->val.dpnt = NULL;
		v->flags |= NEED_INIT;
	}
	else
		vars_check( v, INT_CONT_ARR | FLOAT_CONT_ARR );

	/* Push variable with generic pointer to an array onto the stack */

	ret = vars_push( ARR_PTR, NULL, v );
	return ret;
}


/*----------------------------------------------------------------------*/
/* The function is called when the end of an array access (indicated by */
/* the ']') is found on the left hand side of an assignment. If it is   */
/* called for a new array the indices found on the stack are the sizes  */
/* for the different dimensions of the array and are used to setup the  */
/* the array.                                                           */
/*----------------------------------------------------------------------*/

Var *vars_arr_lhs( Var *v )
{
	int n;
	Var *cv;


	while ( v->type != ARR_PTR )
		v = v->prev;

	/* Count the variables below v on the stack */

	for ( n = 0, cv = v->next; cv != 0; cv = cv->next )
		if ( cv->type != UNDEF_VAR )
			n++;

	/* If the array is new we need to set it up */

	if ( v->from->flags & NEW_VARIABLE )
		return vars_setup_new_array( v, n );

	/* Push a pointer to the indexed element onto the stack */

	return vars_get_lhs_pointer( v, n );
}


/*----------------------------------------------------------------------*/
/* The function is called when after an array variable and the list of  */
/* indices the final closing bracket ']' is found in the input file. It */
/* evaluates the index list and returns an ARR_PTR variable pointing to */
/* the indexed array element (or slice, if there is one index less than */
/* the array has dimensions).                                           */
/* ->                                                                   */
/*    1. The 'from'-field in 'v' is a pointer to to the array and the   */
/*       following variables on the stack are the indices of the        */
/*       element (or slice) to be accessed                              */
/*    2. Number of indices on the stack                                 */
/*----------------------------------------------------------------------*/

static Var *vars_get_lhs_pointer( Var *v, int n )
{
	Var *ret;
	Var *a = v->from;
	long a_index;


	/* Check that there are enough indices on the stack (since variable sized
       arrays allow assignment of array slices there needs to be only one
       index less than the dimension of the array) */

	if ( n < a->dim - 1 )
	{
		print( FATAL, "Not enough indices found for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	/* Check that there are not too many indices */

	if ( n > a->dim )
	{
		print( FATAL, "Too many indices (%d) found for %d-dimensional "
			   "array '%s'.\n", n, a->dim, a->name );
		THROW( EXCEPTION );
	}

	/* For arrays that still are variable sized we need assignment of a
       complete slice in order to determine the missing size of the last
       dimension */

	if ( a->flags & NEED_ALLOC && n != a->dim - 1 )
	{
		if ( a->dim != 1 )
			print( FATAL, "Size of array '%s' is still unknown, only %d ind%s "
				   "allowed here.\n", a->name, a->dim - 1,
				   ( a->dim == 2 ) ? "ex is" : "ices are" );
		else
			print( FATAL, "Size of array '%s' is still unknown, no indices "
				   "are allowed here.\n", a->name );
		THROW( EXCEPTION );
	}

	/* Calculate the pointer to the indexed array element (or slice) and,
       while doing so, pop all the indices from the stack */

	a_index = vars_calc_index( a, v->next );

	/* Pop the variable with the array pointer */

	vars_pop( v );

	/* If the array is still variable sized and thus needs memory allocated we
	   push a pointer to the array onto the stack and store the indexed slice
	   in the variables structure 'len' element. Otherwise we push a variable
	   onto the stack with a pointer to the indexed element or slice.*/

	if ( a->flags & NEED_ALLOC )
	{
		ret = vars_push( ARR_PTR, NULL, a );
		ret->len = a_index;
	}
	else
	{
		if ( a->type == INT_CONT_ARR )
			ret = vars_push( ARR_PTR, a->val.lpnt + a_index, a );
		else
			ret = vars_push( ARR_PTR, a->val.dpnt + a_index, a );
	}

	/* Set necessary flags if a slice is indexed */

	if ( n == a->dim - 1 )
		ret->flags |= NEED_SLICE;

	return ret;
}


/*----------------------------------------------------------------------*/
/* The functions calculates the pointer to an element (or slice) of the */
/* array 'a' from a list of indices on the stack, starting at 'v'.      */
/*----------------------------------------------------------------------*/

static long vars_calc_index( Var *a, Var *v )
{
	int i, cur;
	long a_index;


	/* Run through all the indices on the variable stack */

	for ( i = 0, a_index = 0; v != NULL; i++, v = vars_pop( v ) )
	{
		/* We may use an undefined variable as the very last index...*/

		if ( v->type == UNDEF_VAR )
			break;

		/* Check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Get current index and warn if it's a float variable */

		if ( v->type == INT_VAR )
			cur = v->val.lval - ARRAY_OFFSET;
		else
		{
			print( WARN, "Float variable used as index #%d for array '%s'.\n",
				   i + 1, a->name );
			cur = ( int ) v->val.dval - ARRAY_OFFSET;
		}

		/* Check that the index is a number and not a '*' */

		if ( cur == - ARRAY_OFFSET && v->flags & VARIABLE_SIZED )
		{
			print( FATAL, "A '*' as index is only allowed in the declaration "
				   "of an array, not in an assignment.\n" );
			THROW( EXCEPTION );
		}

		/* Check that the index is not too small or too large */

		if ( cur < 0 )
		{
			print( FATAL, "Invalid array index #%d (value=%d) for array '%s', "
				   "minimum is %d.\n",
				   i + 1, cur + ARRAY_OFFSET, a->name, ARRAY_OFFSET );
			THROW( EXCEPTION );
		}

		/* Here we must be careful: If the array is dynamically sized and
		   we're still in the test phase, the array slice size might be a
		   dummy value that get's corrected in the real measurement. In this
		   case (i.e. test phase and array is dynamically sized and it's the
		   last index into the array) we accept even an index that's too large
		   and readjust it to the currenty possible maximum value hoping that
		   everthing will work out well in the experiment. */

		if ( cur >= ( int ) a->sizes[ i ] )
		{
			if ( ! ( ( a->flags & IS_DYNAMIC ) && Internals.mode == TEST )
				 || v->next != NULL || i != a->dim - 1 )
			{
				print( FATAL, "Invalid array index #%d (value=%d) for array "
					   "'%s', maximum is %d.\n", i + 1, cur + ARRAY_OFFSET,
					   a->name, a->sizes[ i ] - 1 + ARRAY_OFFSET );
				THROW( EXCEPTION );
			}
			else
				cur = a->sizes[ i ] - 1;
		}

		/* Update the index */

		a_index = a_index * a->sizes[ i ] + cur;
	}

	if ( v != NULL )
	{
		if ( v->next != NULL )        /* i.e. UNDEF_VAR not as last index */
		{
			print( FATAL, "Missing array index for array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}
		else
			vars_pop( v );
	}

	/* For slices we need another update of the index */

	if ( i != a->dim && ! ( a->flags & NEED_ALLOC ) )
		a_index = a_index * a->sizes[ i ];

	return a_index;
}


/*-------------------------------------------------------------------------*/
/* The function sets up a new array, pointed to by the 'from'-field in 'v' */
/* and dimension 'dim' with the sizes of the dimensions specified by a     */
/* list of indices on the stack, starting directly after 'v'. If the list  */
/* contains only one index less than the dimension, a variable sized array */
/* is created.                                                             */
/*-------------------------------------------------------------------------*/

static Var *vars_setup_new_array( Var *v, int dim )
{
	int i,
		cur;
	Var *ret,
		*a = v->from;


	if ( v->next->type == UNDEF_VAR )
	{
		print( FATAL, "Missing indices in declaration of array '%s'.\n",
			   a->name );
		THROW( EXCEPTION );
	}

	/* Set arrays dimension and allocate memory for their sizes */

	a->dim = dim;
	a->sizes = NULL;
	a->sizes = T_malloc( dim * sizeof *a->sizes );
	a->len = 1;

	a->flags &= ~NEW_VARIABLE;

	/* Run through the variables with the sizes after popping the variable
       with the array pointer */

	for ( v = vars_pop( v ), i = 0; v != NULL; i++, v = vars_pop( v ) )
	{
		/* Check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Check the value of variable with the size and set the corresponding
		   entry in the arrays field for sizes */

		if ( v->type == INT_VAR )
			cur = ( int ) v->val.lval;
		else
		{
			print( WARN, "FLOAT value (%f) used as size in definition of "
				   "array '%s'.\n", v->val.dval, a->name );
			cur = irnd( v->val.dval );
		}

		/* If the the very last variable with the sizes has the VARIABLE_SIZED
		   flag set this is going to be a dynamically sized array - set the
		   corresponding flag in the array variable, don't reset its
		   NEW_VARIABLE flag and don't allocate memory and return a variable
		   with a generic pointer to the array. */

		if ( v->flags & VARIABLE_SIZED )
		{

			if ( i != dim - 1 )
			{
				print( FATAL, "Only the very last dimension of an array can "
					   "be set dynamically.\n" );
				THROW( EXCEPTION );
			}

			vars_pop( v );

			a->sizes[ i ] = 0;
			a->flags |= NEED_ALLOC;
			ret = vars_push( ARR_PTR, NULL, a );
			ret->len = 0;
			return ret;
		}

		if ( cur < 2 )
		{
			print( FATAL, "Invalid size (%d) used in definition of array "
				   "'%s', minimum is 2.\n", cur, a->name );
			THROW( EXCEPTION );
		}

		a->sizes[ i ] = ( unsigned int ) cur;
		a->len *= cur;
	}

	/* Allocate memory */

	if ( a->type == INT_CONT_ARR )
		a->val.lpnt = T_calloc( a->len, sizeof *a->val.lpnt );
	else
		a->val.dpnt = T_calloc( a->len, sizeof *a->val.dpnt );

	return vars_push( ARR_PTR, a->type == INT_CONT_ARR ?
					  ( void * ) a->val.lpnt : ( void * ) a->val.dpnt, a );
}


/*----------------------------------------------------------------------*/
/* Function is called if an element or a slice of an array is accessed  */
/* after the indices are read in. Prior to the call of this function    */
/* vars_arr_start() has been called and pushed a generic pointer to the */
/* array (i.e. of type ARR_PTR)                                         */
/*----------------------------------------------------------------------*/

Var *vars_arr_rhs( Var *v )
{
	int  dim;
	Var  *a;
	long a_index;


	/* The variable pointer this function gets passed is a pointer to the very
	last index on the variable stack. Now we've got to work our way up in the
	stack until we find the first non-index variable which has to be a pointer
	to an array. While doing so we also count the number of indices on the
	stack, 'dim'. If the last entry on the stack is an undefined variable it
	means we found a reference to an 1-dimensional array, i.e. something like
	'j[ ]'. */

	dim = 0;

	if ( v->type == UNDEF_VAR && v->prev->type == ARR_PTR )
		v = v->prev;
	else
		while ( v->type != ARR_PTR )
		{
			v = v->prev;
			dim++;
		}

	a = v->from;                      /* Get array the pointer refers to */

	/* If the right hand side array is still variable sized it never has been
       assigned values to it and it makes no sense to use its elements */

	if ( a->flags & NEED_ALLOC )
	{
		print( FATAL, "Array '%s' is dynamically sized and its size is still "
			   "unknown.\n", a->name );
		THROW( EXCEPTION );
	}

	/* Check that the number of indices is not less than the dimension of the
       array minus one - we allow slice access for the very last dimension */

	if ( dim < a->dim - 1 )
	{
		print( FATAL, "Not enough indices supplied for array '%s'.\n",
			   a->name );
		THROW( EXCEPTION );
	}

	/* Check that there are not too many indices */

	if ( dim > a->dim )
	{
		print( FATAL, "Too many indices supplied for %d-dimensional array "
			   "'%s'.\n", a->dim, a->name );
		THROW( EXCEPTION );
	}

	/* Calculate the position of the indexed array element (or slice) */

	a_index = vars_calc_index( a, v->next );

	/* Pop the array pointer variable */

	vars_pop( v );

 	/* If this is an element access return a variable with the value of the
	   accessed element, otherwise return a pointer to the first element of
	   the slice. */

	if ( dim == a->dim )
	{
		if ( a->type == INT_CONT_ARR )
			return vars_push( INT_VAR, *( a->val.lpnt + a_index ) );
		else
			return vars_push( FLOAT_VAR, *( a->val.dpnt + a_index ) );
	}

	if ( a->type == INT_CONT_ARR )
		return vars_push( ARR_PTR, a->val.lpnt + a_index, a );
	else
		return vars_push( ARR_PTR, a->val.dpnt + a_index, a );
}


/*----------------------------------------------------------------------*/
/* This is the central function for assigning new values. It allows the */
/* assignment of simple values as well has whole slices of arrays.      */
/*----------------------------------------------------------------------*/

void vars_assign( Var *src, Var *dest )
{
	/* <PARANOID> check that both variables exist </PARANOID> */

	fsc2_assert( vars_exist( src ) && vars_exist( dest ) );

	switch ( src->type )
	{
		case INT_VAR : case FLOAT_VAR :              /* variable */
			vars_ass_from_var( src, dest );
		   break;

		case ARR_PTR : case ARR_REF :                /* array slice */
			vars_ass_from_ptr( src, dest );
			break;

		case INT_ARR : case FLOAT_ARR :              /* transient array */
			vars_ass_from_trans_ptr( src, dest );
			break;

		default :
			fsc2_assert( 1 == 0 );        /* we never should end up here... */
			break;
	}

	dest->flags &= ~ NEED_INIT;

	if ( ! ( dest->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) ) )
		vars_pop( dest );

	vars_pop( src );
}


/*------------------------------------------------------------------------*/
/* Function assigns a value to a variable or an element of an array, thus */
/* only INT_VAR, FLOAT_VAR and ARR_PTR are allowed as types of the des-   */
/* tination variable 'dest'. The source has to be a simple variable, i.e. */
/* must be of type INT_VAR or FLOAT_VAR.                                  */
/*------------------------------------------------------------------------*/

static void vars_ass_from_var( Var *src, Var *dest )
{
	size_t i;
	long lval;
	double dval;
	long *lp;
	double *dp;


	/* Don't do an assignment if right hand side has no value */

	if ( src->flags & NEW_VARIABLE )
	{
		print( FATAL, "On the right hand side of the assignment a variable is "
			   "used that has not been assigned a value.\n" );
		THROW( EXCEPTION );
	}

	/* Stop if left hand side needs an array slice but all we have is
	   a variable */

	if ( dest->flags & NEED_SLICE )
	{
		print( FATAL, "In assignment to array '%s' an array slice is needed "
			   "on the right hand side.\n", dest->from->name );
		THROW( EXCEPTION );
	}

	if ( dest->flags & NEED_ALLOC )
	{
		print( FATAL, "Assignment to dynamic array '%s' that has a still "
			   "undefined size.\n", dest->name );
		THROW( EXCEPTION );
	}

	/* If the destination variable is still completely new set its type */

	if ( dest->type == UNDEF_VAR )
	{
		dest->type = VAR_TYPE( dest->name );
		dest->flags &= ~NEW_VARIABLE;
	}

	/* Do the assignment - take care: the left hand side variable can be
	   either a real variable or an array pointer with the void pointer
	   'val.gptr' in the variable structure pointing to the data to be set
	   in an array.  */

	switch ( dest->type )
	{
		case INT_VAR : case FLOAT_VAR :
			if ( dest->type == INT_VAR )
			{
				if ( src->type == INT_VAR )
					dest->val.lval = src->val.lval;
				else
				{
					print( WARN, "Floating point value used in assignment to "
						   "integer variable.\n" );
					dest->val.lval = ( long ) src->val.dval;
				}
			}
			else
			{
				if ( src->type == INT_VAR )
					dest->val.dval = ( double ) src->val.lval;
				else
					dest->val.dval = src->val.dval;
			}
			break;

		case ARR_PTR :
			if ( dest->from->type == INT_CONT_ARR )
			{
				if ( src->type == INT_VAR )
					*( ( long * ) dest->val.gptr ) = src->val.lval;
				else
				{
					print( WARN, "Floating point value used in assignment to "
						   "integer variable.\n" );
					*( ( long * ) dest->val.gptr ) = ( long ) src->val.dval;
				}
			}
			else
			{
				if ( src->type == INT_VAR )
					*( ( double * ) dest->val.gptr )
						                            = ( double ) src->val.lval;
				else
					*( ( double * ) dest->val.gptr ) = src->val.dval;
			}
			break;

		case INT_CONT_ARR :
			if ( dest->dim != 1 )
			{
				print( FATAL, "Can't assign value to array with more than one "
					   "dimension.\n" );
				THROW( EXCEPTION );
			}

			if ( src->type == FLOAT_VAR )
			{
				print( WARN, "Assigning float value to integer array.\n" );
				lval = ( long ) src->val.dval;
			}
			else
				lval = src->val.lval;

			for ( i = 0, lp = dest->val.lpnt; i < dest->len; i++ )
				*lp++ = lval;
			break;

		case FLOAT_CONT_ARR :
			if ( dest->dim != 1 )
			{
				print( FATAL, "Can't assign value to array with more than one "
					   "dimension.\n" );
				THROW( EXCEPTION );
			}

			dval = VALUE( src );
			for ( i = 0, dp = dest->val.dpnt; i < dest->len; i++ )
				*dp++ = dval;
			break;

		default :                    /* we never should end up here... */
			fsc2_assert ( 1 == 0 );
			break;
	}
}


/*------------------------------------------------------------------------*/
/* The function assigns complete array slices. The source type must be    */
/* an ARR_PTR while the destination can either be an array or an ARR_PTR  */
/* (in the later case the array pointed to must expect to be assigned an  */
/* array slice).                                                          */
/*------------------------------------------------------------------------*/

static void vars_ass_from_ptr( Var *src, Var *dest )
{
	Var *d,
		*s;
	unsigned int i;


	/* May be that's paranoid, but... */

	fsc2_assert( src->from->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) );

	/* We can't assign from an array slice to a variable */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) )
	{
		print( FATAL, "Left hand side of assignment is a variable while right "
			   "hand side is an array (slice).\n" );
		THROW( EXCEPTION );
	}

	if ( src->type == ARR_REF )
	{
		if ( src->from->type == INT_CONT_ARR )
			src->val.gptr = src->from->val.lpnt;
		else if ( src->from->type == FLOAT_CONT_ARR )
			src->val.gptr = src->from->val.dpnt;
		else
		{
			fsc2_assert( 1 == 0 );
		}
	}

	if ( dest->type == ARR_PTR )
	{
		fsc2_assert( dest->flags & NEED_SLICE ||
					 dest->from->flags & NEED_INIT );

		d = dest->from;
		s = src->from;

		/* Allocate memory (set size of missing dimension to the last one of
		   the source array) if the destination array needs it, otherwise
		   check that size of both the arrays is equal. */

		if ( d->flags & NEED_ALLOC )
		{
			d->sizes[ d->dim - 1 ] = s->sizes[ s->dim - 1 ];

			d->len *= d->sizes[ d->dim - 1 ];

			if ( d->type == INT_CONT_ARR )
			{
				d->val.lpnt = T_calloc( d->len, sizeof *d->val.lpnt );
				dest->val.lpnt = d->val.lpnt
					             + dest->len * d->sizes[ d->dim - 1 ];
			}
			else
			{
				d->val.dpnt = T_calloc( d->len, sizeof *d->val.dpnt );
				dest->val.dpnt = d->val.dpnt
					             + dest->len * d->sizes[ d->dim - 1 ];
			}

			d->flags &= ~ NEED_ALLOC;
		}
		else
		{
			if ( d->sizes[ d->dim - 1 ] != s->sizes[ s->dim - 1 ] )
			{
				print( FATAL, "Arrays (or slices of) '%s' and '%s' have "
					   "different sizes (%ld and %ld, respectively).\n",
					   d->name, s->name,
					   d->sizes[ d->dim - 1 ], s->sizes[ s->dim - 1 ] );
				THROW( EXCEPTION );
			}

			if ( d->type == INT_CONT_ARR )
				dest->val.lpnt = ( long * ) dest->val.gptr;
			else
				dest->val.dpnt = ( double * ) dest->val.gptr;
		}
	}
	else
	{
		fsc2_assert( dest->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) );

		d = dest;
		s = src->from;

		if ( dest->flags & NEED_ALLOC )
		{
			d->len = d->sizes[ 0 ] = s->sizes[ s->dim - 1 ];
			if ( d->type == INT_CONT_ARR )
				d->val.lpnt = T_calloc( d->len, sizeof *d->val.lpnt );
			else
				d->val.dpnt = T_calloc( d->len, sizeof *d->val.dpnt );

			d->flags &= ~ NEED_ALLOC;
		}
		else if ( d->len != s->sizes[ s->dim - 1 ] )
		{
			print( FATAL, "Arrays (or slices of) '%s' and '%s' have different "
				   "sizes (%ld and %ld, respectively).\n",
				   d->name, s->name, d->len, s->sizes[ s->dim - 1 ] );
			THROW( EXCEPTION );
		}
	}

	/* Warn on float to integer assignment */

	if ( d->type == INT_CONT_ARR && s->type == FLOAT_CONT_ARR )
		print( WARN, "Assignment of float array (slice) '%s' to integer array "
			   "'%s'.\n", s->name, d->name );

	/* Now do the actual copying - if both types are identical a fast memcpy()
	   will do the job, otherwise we need to do it 'by hand' */

	if ( s->type == d->type )
	{
		if ( s->type == INT_CONT_ARR )
			memcpy( dest->val.lpnt, src->val.gptr,
					d->sizes[ d->dim - 1 ] * sizeof *dest->val.lpnt );
		else
			memcpy( dest->val.dpnt, src->val.gptr,
					d->sizes[ d->dim - 1 ] * sizeof *dest->val.dpnt );
	}
	else
	{
		/* Get also the correct type of pointer for the data source - the
		   pointer to the start of the slice is always stored as a void
		   pointer */

		if ( s->type == INT_CONT_ARR )
			src->val.lpnt = ( long * ) src->val.gptr;
		else
			src->val.dpnt = ( double * ) src->val.gptr;

		/* Finally copy the array slice element by element */

		if ( d->type == INT_CONT_ARR )
			for ( i = 0; i < d->sizes[ d->dim - 1 ]; i++ )
				*dest->val.lpnt++ = ( long ) *src->val.dpnt++;
		else
			for ( i = 0; i < d->sizes[ d->dim - 1 ]; i++ )
				*dest->val.dpnt++ = ( double ) *src->val.lpnt++;
	}

	/* The size of the destination array is now fixed if the source array has
	   a fixed size */

	d->flags &= d->flags & src->from->flags & IS_DYNAMIC;
}


/*----------------------------------------------------------------------*/
/* Function also assigns an array slice, but here the source must be an */
/* transient integer or float array while the destination type is again */
/* an ARR_PTR.                                                          */
/*----------------------------------------------------------------------*/

static void vars_ass_from_trans_ptr( Var *src, Var *dest )
{
	Var *d;
	unsigned int i;
	double *sdptr = NULL;
	long *slptr = NULL;
	bool dest_needs_pop = UNSET;


	/* We can't assign from a transient array to a variable */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) )
	{
		print( FATAL, "Left hand side of assignment is a variable while right "
			   "hand side is an array (slice).\n" );
		THROW( EXCEPTION );
	}

	vars_check( dest, ARR_PTR | INT_CONT_ARR | FLOAT_CONT_ARR );

	/* This is for assignments to one-dimensional arrays that are specified
	   on the LHS just with their names and without any brackets */

	if ( dest->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) )
	{
		if ( dest->dim != 1 )
		{
			print( FATAL, "Left hand side of assignment isn't an array "
				   "slice.\n" );
			THROW( EXCEPTION );
		}

		dest = vars_push( ARR_PTR, ( dest->type == INT_CONT_ARR ) ?
						  ( void * ) dest->val.lpnt :
						  ( void * ) dest->val.dpnt, dest );
		dest->len = 0;
		dest->flags |= NEED_SLICE;

		dest_needs_pop = SET;
	}

	d = dest->from;

	/* Again being paranoid... */

	fsc2_assert( dest->flags & NEED_SLICE || d->flags & NEED_ALLOC );

	/* Do allocation of memory (set size of missing dimension to the one of
	   the transient array) if the destination array needs it, otherwise check
	   that the sizes of the destination array size is equal to the length of
	   the transient array. */

	if ( d->flags & NEED_ALLOC )
	{
		d->sizes[ d->dim - 1 ] = src->len;

		d->len *= d->sizes[ d->dim - 1 ];

		if ( d->type == INT_CONT_ARR )
		{
			d->val.lpnt = T_calloc( d->len, sizeof *d->val.lpnt );
			dest->val.lpnt = d->val.lpnt + dest->len * d->sizes[ d->dim - 1 ];
		}
		else
		{
			d->val.dpnt = T_calloc( d->len, sizeof *d->val.dpnt );
			dest->val.dpnt = d->val.dpnt + dest->len * d->sizes[ d->dim - 1 ];
		}

		d->flags &= ~ NEED_ALLOC;
	}
	else
	{
		if ( d->sizes[ d->dim - 1 ] != src->len )
		{
			print( FATAL, "Array slice assigned to array '%s' does not fit "
				   "its length, its size is %ld while the slice has %ld "
				   "elements.\n", d->name, d->sizes[ d->dim - 1 ], src->len );
			THROW( EXCEPTION );
		}

		if ( d->type == INT_CONT_ARR )
			dest->val.lpnt = ( long * ) dest->val.gptr;
		else
			dest->val.dpnt = ( double * ) dest->val.gptr;
	}

	/* Warn on float to integer assignment */

	if ( d->type == INT_CONT_ARR && src->type == FLOAT_ARR )
		print( WARN, "Assigning float array (or slice) to integer array "
			   "'%s'.\n", d->name );

	/* Now copy the transient array as slice to the destination - if both
	   variable types fit a fast memcpy() will do the job while in the other
	   case the copying has to be done 'by hand' */

	if ( ( src->type == INT_ARR && d->type == INT_CONT_ARR ) ||
		 ( src->type == FLOAT_ARR && d->type == FLOAT_CONT_ARR ) )
	{
		if ( src->type == INT_ARR )
			memcpy( dest->val.lpnt, src->val.lpnt,
					d->sizes[ d->dim - 1 ] * sizeof *dest->val.lpnt );
		else
			memcpy( dest->val.dpnt, src->val.dpnt,
					d->sizes[ d->dim - 1 ] * sizeof *dest->val.dpnt );
	}
	else
	{
		if ( src->type == INT_ARR )
			slptr = src->val.lpnt;
		else
			sdptr = src->val.dpnt;

		for ( i = 0; i < d->sizes[ d->dim - 1 ]; i++ )
		{
			if ( d->type == INT_CONT_ARR )
				*dest->val.lpnt++ = ( long ) *sdptr++;
			else
				*dest->val.dpnt++ = ( double ) *slptr++;
		}
	}

	d->flags &= d->flags & src->flags & IS_DYNAMIC;

	if ( dest_needs_pop )
		vars_pop( dest );
}


/*--------------------------------------------------------------------------*/
/* Function initializes a new array. When called the stack at 'v' contains  */
/* all the data for the initialization (the last data on top of the stack)  */
/* and, directly below the data, an ARR_PTR to the array to be initialized. */
/* If 'v' is an UNDEF_VAR no initialization has to be done.                 */
/*--------------------------------------------------------------------------*/

void vars_arr_init( Var *v )
{
	size_t num_init;
	Var  *p1,
		 *p2,
		 *a;


	/* If there are no initialization data this is indicated by a variable
	   of type UNDEF_VAR - just pop it as well as the array pointer */

	if ( v->type == UNDEF_VAR )
	{
		v->prev->from->flags &= ~ NEED_INIT;
		vars_pop( v->prev );
		vars_pop( v );
		return;
	}

	/* The variable pointer this function gets passed is a pointer to the very
       last initialization data on the variable stack. Now we've got to work
       our way up the variable stack until we find the first non-data variable
       which must be of ARR_PTR type. While doing so, we count the number
       of initializers 'num_init' on the stack. */

	num_init = 0;
	while ( v->type != ARR_PTR )
	{
		v = v->prev;
		num_init++;
	}
	a = v->from;
	vars_check( a, INT_CONT_ARR | FLOAT_CONT_ARR );

	/* If the array isn't newly declared we can't do an assignment */

	if ( ! ( a->flags & NEED_INIT ) )
	{
		print( FATAL, "Initialization of array '%s' only allowed "
			   "immediately after declaration.\n", a->name );
		THROW( EXCEPTION );
	}

	/* Only 1-dimensional variable sized arrays can be initialized which
	   automatically fixes the size of the array */

	if ( a->flags & NEED_ALLOC )
	{
		if ( a->dim != 1 )
		{
			print( FATAL, "Only 1-dimensional variable sized arrays can be "
				   "initialized, but '%s' is %d-dimensional.\n",
				   a->name, a->dim );
			THROW( EXCEPTION );
		}

		if ( num_init < 2 )
		{
			print( FATAL, "Got only one value as initializer for "
				   "1-dimensional, variable sized array '%s'.\n", a->name );
			THROW( EXCEPTION );
		}

		a->len = a->sizes[ 0 ] = num_init;

		if ( a->type == INT_CONT_ARR )
			a->val.lpnt = T_malloc( a->len * sizeof *a->val.lpnt );
		else
			a->val.dpnt = T_malloc( a->len * sizeof *a->val.dpnt );

		a->flags &= ~NEED_ALLOC;
	}

	/* Check that the number of initializers fits the number of elements of
       the array */

	if ( num_init < a->len )
		print( WARN, "Less initializers for array '%s' than it has "
			   "elements.\n", a->name );

	if ( num_init > a->len )
	{
		print( FATAL, "Too many initializers for array '%s'.\n", a->name );
		THROW( EXCEPTION );
	}

	/* Finally do the assignments - walk through the data and copy them into
	   the area for data in the array */

	if ( a->type == INT_CONT_ARR )
		v->val.lpnt = a->val.lpnt;
	else
		v->val.dpnt = a->val.dpnt;

	for ( p1 = v->next; p1 != NULL; p1 = p2 )
	{
		fsc2_assert( p1->type & ( INT_VAR | FLOAT_VAR ) );

		if ( a->type == INT_CONT_ARR )
		{
			if ( p1->type == INT_VAR )
				*v->val.lpnt++ = p1->val.lval;
			else
			{
				print( WARN, "Floating point value used in initialization of "
					   "integer array '%s'.\n", a->name );
				*v->val.lpnt++ = ( long ) p1->val.dval;
			}
		}
		else
		{
			if ( p1->type == INT_VAR )
				*v->val.dpnt++ = ( double ) p1->val.lval;
			else
				*v->val.dpnt++ = p1->val.dval;
		}

		p2 = p1->next;
		vars_pop( p1 );
	}

	a->flags &= ~ NEED_INIT;
	vars_pop( v );
}


/*--------------------------------------------------------------------------*/
/* This function is called from the parsers for variable or function tokens */
/* possibly followed by a unit. If there was no unit the second argument is */
/* NULL and nothing has to be done. If it isn't NULL we've got to check     */
/* that the token is really a simple number and multiply it with the unit.  */
/* If the variable or function token is an array we have to drop out of     */
/* parsing and print an error message instead.                              */
/*--------------------------------------------------------------------------*/

Var *apply_unit( Var *var, Var *unit )
{
	if ( var->type == UNDEF_VAR )
	{
		fsc2_assert( var->name != NULL );         /* just a bit paranoid ? */

		print( FATAL, "The accessed variable '%s' has not been assigned a "
			   "value.\n", var->name );
		THROW( EXCEPTION );
	}

	if ( unit == NULL )
	{
		if ( var->type == STR_VAR )
			return var;
		if ( var->type & ( INT_VAR | FLOAT_VAR ) )
			return vars_mult( var, vars_push( INT_VAR, 1 ) );
		if ( var->type & ( INT_CONT_ARR | FLOAT_CONT_ARR ) )
			return vars_push( ARR_REF, var );
		return var;
	}

	if ( var->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_mult( var, unit );
	else
	{
		print( FATAL, "Syntax error: Unit is applied to something which isn't "
			   "a number.\n" );
		THROW( EXCEPTION );
	}

	return NULL;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

Var *vars_val( Var *v )
{
	Var *vn;


	vars_check( v, ARR_PTR );

	if ( v->flags & NEED_SLICE )
	{
		vars_check( v->from, INT_CONT_ARR | FLOAT_CONT_ARR );
		if ( ! ( v->from->flags & NEED_ALLOC ) )
		{
			if ( v->from->type == INT_CONT_ARR )
				vn = vars_push( INT_ARR, v->val.vptr,
								v->from->sizes[ v->from->dim - 1 ] );
			else
				vn = vars_push( FLOAT_ARR, v->val.vptr,
								v->from->sizes[ v->from->dim - 1 ] );
		}
		else
		{
			if ( v->from->type == INT_CONT_ARR )
				vn = vars_push( INT_ARR, NULL, 0 );
			else
				vn = vars_push( FLOAT_ARR, NULL, 0 );
			vn->flags |= NEED_ALLOC;
		}

		vn->flags |= v->from->flags & IS_DYNAMIC;
		return vn;
	}

	if ( v->from->type == INT_CONT_ARR )
		return vars_push( INT_VAR, *( ( long * ) v->val.gptr ) );
	else if ( v->from->type == FLOAT_CONT_ARR )
		return vars_push( FLOAT_VAR, *( ( double * ) v->val.gptr ) );

	fsc2_assert( 1 == 0 );
	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
