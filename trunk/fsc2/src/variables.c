/* 
   $Id$

   $Log$
   Revision 1.25  1999/07/27 16:19:25  jens
   *** empty log message ***

   Revision 1.24  1999/07/23 23:43:59  jens
   *** empty log message ***

   Revision 1.23  1999/07/22 22:07:21  jens
   *** empty log message ***

   Revision 1.22  1999/07/22 16:37:00  jens
   *** empty log message ***

   Revision 1.21  1999/07/22 07:47:47  jens
   *** empty log message ***

   Revision 1.20  1999/07/21 23:01:23  jens
   *** empty log message ***

   Revision 1.19  1999/07/21 15:26:32  jens
   *** empty log message ***

   Revision 1.18  1999/07/21 07:32:24  jens
   *** empty log message ***

   Revision 1.17  1999/07/20 23:30:32  jens
   Quite some changes: vars_push() has now a variable arguments list, thus we
   don't have to pass it a void pointer to the data but, depending on the type,
   it can find out what type of data the variable data is. Makes things a lot
   easier...

   Revision 1.16  1999/07/20 12:01:39  jens
   *** empty log message ***

   Revision 1.15  1999/07/20 11:23:33  jens
   ars

   Revision 1.14  1999/07/19 22:25:09  jens
   *** empty log message ***

   Revision 1.13  1999/07/19 07:40:08  jens
   *** empty log message ***

   Revision 1.12  1999/07/17 15:58:23  jens
   Bug fix.

   Revision 1.11  1999/07/17 15:45:01  jens
   Changed vars_push to reflect new treatment of start of print statements.

   Revision 1.10  1999/07/17 15:03:12  jens
   Fixed a memory leak & changes to make lint more happy.

   Revision 1.9  1999/07/17 13:32:18  jens
   *** empty log message ***

   Revision 1.8  1999/07/17 12:49:21  jens
   Diverse corrections to make lint more happy...

   Revision 1.7  1999/07/16 19:31:39  jens
   *** empty log message ***

*/

/* Thinks to be careful about:

   1. Currently, arrays are never pushed onto the variables stack, only
      `normal' variables and functions (i.e. pointers to functions).
	  But neither in vars_push() nor in  vars_pop() it is checked if the
	  variable passed to it is not an array.

   2. In vars_push_astack() we pop the array variable passed to the function
      although this should never be a variable on the variables stack.
*/


#include "fsc2.h"


typedef Var *VarretFnct( Var * );
typedef VarretFnct *FnctPtr;


/*
  Each variable and array (and also each function) is described by a structure

  typedef struct Var_
  {
    char *name;                             // name of variable
	int type;                               // type of variable
	union
	{
		long lval;                          // for integer values
		double dval;                        // for float values
		long *lpnt;                         // for integer arrays
		double *dpnt;                       // for double arrays
		double ( *fnct )( long, double * ); // for functions
	} val;
	long dim;             // dimension of array / number of args of function
	long *sizes;          // sizes of dimensions of arrays
	long len;             // total len of array / position in function list
	bool new_flag;        // set when variable is just created
	struct Var_ *next;
  }

  For each variable and array such a structure is created and appended to
  the head of a linked list of all variables. When a variable is accessed,
  either to get its value or to assign a new value to it, this list is
  searched for a structure that contains an entry with the same name.

  Each permanent variable (transient variables are only used on the variable
  stack, see below) has a name assigned to it, starting with a character and
  followed by as many characters, numbers and underscores as the user wants
  it to have.

  Each variable has a certain type: It's either an integer or float variable
  (INT_VAR or FLOAT_VAR, which translates to C's long and double type) or it
  is an integer or float array (INT_ARR or FLOAT_ARR) of as many dimensions
  as the user thinks it should have. Functions are just a funny kind of
  variables of type FUNC, with the value being the result of the calculation
  of the function value (plus possible other side effects). While a variable
  still hasn't assigned a value to it it's type remains undefined, i.e.
  UNDEF_VAR.

  What kind of type a variable has is controlled via the function IF_FUNC(),
  defined as macro in variables.h, which just gets to know about the very
  first character of the variable's name - if the function returns TRUE the
  variable is an integer (or the array is an integer array) otherwise its
  type is FLOAT. So, changing IF_FUNC() and recompiling will change the
  behaviour of the program in this respect. Currently, as agreed with Axel
  and Thomas, IF_FUNC returns TRUE for variables starting with a capital
  letters, thus making the variable an integer. But this is easily
  changed...

  When the input file is read in, lines like

           A = B + 3.25;

  are found. While this is rather convenient for a human a reverse polish
  notation (RPN) for the right hand side of the assignment of the form

           B 3.25 +

  is much easier to handle for a computer. For this reason there exists a
  variable stack (which actually isn't a stack but a linked list).

  So, if the lexer finds an identifier like `A', it first tries to get a
  pointer to the variable named `A' in the variables list by calling
  vars_get(). If this fails (and we're just parsing the VARIABLES section of
  an EDL file, otherwise it would utter an error message) a new variable is
  created instead by a call to vars_new(). The resulting pointer is passed
  to the parser.

  Now, the parser sees the `=' bit of text and realizes it has to do an
  assignment and thus branches to evaluate the right hand side of the
  expression. In this branch, the parser sees the `B' and pushes a pointer
  to a copy of the variable `B' onto the variables stack. It then finds the
  `+' and branches recursively to evaluate the expression to the right of
  the `+'. Here, the parser sees just the numerical value `3.25' and pushes
  it onto the variables stack, thus creating another transient variable on
  the stack with the value 3.25. It returns from the branch with the pointer
  to this transient variable. Now the (transient) copy of `B' and the
  transient variable with the value of 3.25 are on the variables stack and
  the parser can continue by adding the values of both these variables. It
  does so by calling vars_add(), creating another transient variable on the
  stack for the result and removing both the variables used as arguments. It
  finally returns to the state it started from, the place where it found the
  `A =' bit, with a pointer to the transient variable with the result of the
  evaluation of the right hand side. All left to be done now is to call
  vars_new_assign() or vars_assign(), depending if we're still in the
  VARIABLES section, which assigns the value of the transient variable to
  the variable `A'. The transient variable is than removed from the
  variables stack. If we're a bit paranoid we can make sure everything
  worked out fine by checking that the variabe stack is now empty. Quite
  simple, isn't it?
  
  What looks like a lot of complicated work to do something rather simple
  has the advantage that, due to its recursive nature, it can be used
  without any changes for much more complicated situations. Instead of the
  value 3.25 we could have a complicated expression to add to `B', which
  will be handled automagically by a deeper recursion, thus breaking up the
  complicated task into small, simple tasks, that can be handled easily.
  Also, `B' could be a more complicated expression instead which would be
  handled in the same way.

  Now, what about arrays? Again, if the lexer finds an identifier (it
  doesn't know about the difference between variables and arrays) it
  searches the variables list and if it doesn't find an entry with the same
  name it creates a new one (again, as long as we're in the VARIABLES
  section where defining new variables and array is allowed). The parser in
  turn realizes that the user wants an array when it finds a string of
  tokens of the form

            variable_identifier [ 

  where `variable_identifier' is a variable or array name. It calls
  vars_arr_start() where the type of the array is set to INT_ARR or
  FLOAT_ARR (depending on the result of the macro IF_FUNC(), see above) and
  its dimension is set to zero.

  The next token has to be an expression (see below for an exception). By a
  call to vars_arr_extend() the dimension of the array is set to one and the
  list of sizes for the dimensions is updated to reflect the value of the
  expression, i.e. the size for the very first dimension of the
  array. Finally, the transient variable with the size of this first
  dimension is popped from tha variable stack. The next token to be found by
  the parser has to be either a comma followed by another expression or a
  closing bracket. In the first case, i.e. if we have a two-dimensional
  array, var_arr_extend() is called again. Here the dimension of the array
  is incremented and the list for the sizes of the dimensions is extended
  and updated.  Finally, the transient variable with the size of the new
  dimension is removed. If instead a closing bracket, `]', is found, the
  parser has to do nothing, since everything relevant for the array is
  already set. Instead, the parser now expects an initialization of the
  array, i.e an equal sign `=' followed by a list of data (these can also
  have the form of an expression) in curly braces, `{' and `}'. Again, we
  may check that the variables stack is empty, otherwise something went
  horribly wrong.

  The one exception from this scheme is that the very last dimension of an
  array can be dynamically set. This is achieved by not giving an expression
  but a star (`*') as the very last dimension. But there are special rules
  for dynamically sized arrays: Since their purpose is to allow storing of
  complete curves obtained from a device (where the exact length of the data
  is not known in advance) the still undetermined size is only finally
  determined by such an operation. Previous to this operation, it is
  forbidden to either initialize arrays of this type or to assign data to
  its elements.

  Arrays make evaluating expressions a bit more complicated - the expression
  could be an array element with the indices in turn being array elements
  with their indices being... So, if in the evaluation of an expression an
  array element is detected, a pointer to a new entry of the form

  typedef struct AStack_
  {
    Var *var;
	long act_entry;
	long *entries;
	struct AStack_ *next;
  }

  is pushed onto the array stack by a call to the function vars_push_astack().
  The structure's element `var' points to the variable of the arrray,
  `act_entry' is zero at first (we're just dealing with the very first index
  of the arrray) and the first slot in `entries' is set to the value of the
  index (`entries' has as many slots as te array has dimensions).

  If the parser finds another index, the function vars_update_astack()
  increments `act_entry' and stores the value of the new index in the next
  free slot of `entries' (of the topmost element of the array stack). Of
  course, some checking has to be done to make sure, that there are not more
  indices then there are dimensions in the array.

  Finally, when the parser finds a closing bracket, it calls vars_pop_astack()
  which pushes the array element indexed by the data in `entries' as a
  transient variable onto the variables stack and removes the entry for the
  array from the array stack.

  Again, after completely evaluating an assignment, one should make sure
  everything work out fine by checking that the array stack is empty.

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


	/* try to find the variable with the name passed to the function */

	for ( ptr = var_list; ptr != ( Var * ) NULL; ptr = ptr->next )
		if ( ! strcmp( ptr->name, name ) )
			return( ptr );

	return( NULL );
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

	vp = T_malloc( sizeof( Var ) );
	vp->name = get_string_copy( name );

	/* Set relevant entries in the new structure and make it the very first
	   element in the list of variables */

	vp->flags = NEW_VARIABLE;    /* set flag to indicate it's new */
	vp->type = UNDEF_VAR;        /* set type to still undefined */

	vp->next = var_list;         /* set pointer to it's successor */
    var_list = vp;               /* make it the head of the list */

	return( vp );                /* return pointer to the structure */
}


/*------------------------------------------------------------------*/
/* vars_add() adds two variables and pushes the result on the stack */
/* ->                                                               */
/*    * pointers to two variable structures                         */
/* <-                                                               */
/*    * pointer to a new transient variable on the variables stack  */
/*------------------------------------------------------------------*/

Var *vars_add( Var *v1, Var *v2 )
{
	Var *new_var;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* now add the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( INT_VAR, v1->val.lval + v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v1->val.lval + v2->val.dval );
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR,
								 v1->val.dval + ( double ) v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR, v1->val.dval + v2->val.dval );
	}

	/* pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*-----------------------------------------------------------------------*/
/* vars_sub() subtracts two variables and pushes the result on the stack */
/* ->                                                                    */
/*    * pointers to two variable structures                              */
/* <-                                                                    */
/*    * pointer to a new transient variable on the variables stack       */
/*-----------------------------------------------------------------------*/

Var *vars_sub( Var *v1, Var *v2 )
{
	Var *new_var;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* now subtract the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( INT_VAR, v1->val.lval - v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v1->val.lval - v2->val.dval );
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR,
								 v1->val.dval - ( double ) v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR, v1->val.dval - v2->val.dval );
	}

	/* pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*-------------------------------------------------------------------------*/
/* vars_mult() multiplies two variables and pushes the result on the stack */
/* ->                                                                      */
/*    * pointers to two variable structures                                */
/* <-                                                                      */
/*    * pointer to a new transient variable on the variables stack         */
/*-------------------------------------------------------------------------*/

Var *vars_mult( Var *v1, Var *v2 )
{
	Var *new_var;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* now multiply the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( INT_VAR, v1->val.lval * v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v1->val.lval * v2->val.dval );
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR,
								 v1->val.dval * ( double ) v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR, v1->val.dval * v2->val.dval );
	}

	/* pop the variables from the variable stack (if they belong to it) */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*---------------------------------------------------------------------*/
/* vars_div() divides two variables and pushes the result on the stack */
/* ->                                                                  */
/*    * pointers to two variable structures                            */
/* <-                                                                  */
/*    * pointer to a new transient variable on the variables stack     */
/*---------------------------------------------------------------------*/

Var *vars_div( Var *v1, Var *v2 )
{
	Var *new_var;


	/* make sure that `v1' and `v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* make sure the denominator is not zero */

	if ( ( v2->type == INT_VAR && v2->val.lval == 0 ) ||
		 ( v2->type == FLOAT_VAR && v2->val.dval == 0.0 ) )
	{
		eprint( FATAL, "%s:%ld: Division by zero.\n", Fname, Lc );
		THROW( FLOATING_POINT_EXCEPTION );
	}

	/* calculate the result and it push onto the stack */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( INT_VAR, v1->val.lval / v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR,
								 ( double ) v1->val.lval / v2->val.dval );
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR,
								 v1->val.dval / ( double ) v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR, v1->val.dval / v2->val.dval );
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*-----------------------------------------------------------------*/
/* vars_mod() pushes the modulo of the two variables on the stack  */
/* ->                                                              */
/*    * pointers to two variable structures                        */
/* <-                                                              */
/*    * pointer to a new transient variable on the variables stack */
/*-----------------------------------------------------------------*/

Var *vars_mod( Var *v1, Var *v2 )
{
	Var *new_var;


	/* make sure that `v1' and `v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* make sure the second value is not zero */

	if ( ( v2->type == INT_VAR && v2->val.lval == 0 ) ||
		 ( v2->type == FLOAT_VAR && v2->val.dval == 0.0 ) )
	{
		eprint( FATAL, "%s:%ld: Modulo by zero.\n", Fname, Lc );
		THROW( FLOATING_POINT_EXCEPTION );
	}

	/* calculate the result and it push onto the stack */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( INT_VAR, v1->val.lval % v2->val.lval );
		else
			new_var = vars_push( FLOAT_VAR, fmod( ( double ) v1->val.lval,
												  v2->val.dval ) );
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR, fmod( v1->val.dval,
												  ( double ) v2->val.lval ) );
		else
			new_var = vars_push( FLOAT_VAR,
								 fmod( v1->val.dval, v2->val.dval ) );
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*------------------------------------------------------------------*/
/* vars_pow() takes `v1' to the power of `v2' and pushes the result */
/* on the stack                                                     */
/* ->                                                               */
/*    * pointers to two variable structures                         */
/* <-                                                               */
/*    * pointer to a new transient variable on the variables stack  */
/*------------------------------------------------------------------*/

Var *vars_pow( Var *v1, Var *v2 )
{
	Var *new_var;
	long ires, i;


	/* make sure that `v1' and `v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	/* make sure the base is not negative while the exponent 
	   is not an integer */

	if ( ( ( v1->type == INT_VAR && v1->val.lval < 0 ) ||
		   ( v1->type == FLOAT_VAR && v1->val.dval < 0.0 ) )
		 && v2->type != INT_VAR )
	{
		eprint( FATAL, "%s:%ld: Negative base while exponent not an integral "
				"value.\n", Fname, Lc );
		THROW( FLOATING_POINT_EXCEPTION );
	}

	/* calculate the result and it push onto the stack */

	if ( v1->type == INT_VAR )
	{
		if ( v1->val.lval == 0 )   /* powers of zero are always 1 */
			new_var = vars_push( INT_VAR, 1L );
		else
		{
			if ( v2->type == INT_VAR )
			{
				for ( ires = v1->val.lval, i = 1;
					  i < labs( v2->val.lval ); ++i )
					ires *= v1->val.lval;
				if ( v2->val.lval >= 0 )
					new_var = vars_push( INT_VAR, ires );
				else
					new_var = vars_push( FLOAT_VAR, 1.0 / ( double ) ires );
			}
			else
				new_var = vars_push( FLOAT_VAR, pow( ( double ) v1->val.lval,
													 v2->val.dval ) );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			new_var = vars_push( FLOAT_VAR, pow( v1->val.dval,
												 ( double ) v2->val.lval ) );
		else
			new_var = vars_push( FLOAT_VAR,
								 pow( v1->val.dval, v2->val.dval ) );
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*-----------------------------------------------*/
/* vars_negate() negates the value of a variable */
/* ->                                            */
/*    * pointer a variable                       */
/* <-                                            */
/*    * pointer to the same variable but with    */
/*      its value negated                        */
/*-----------------------------------------------*/

Var *vars_negate( Var *v )
{
	/* make sure that `v' exists, has integer or float type and has
	   an value assigned to it */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		v->val.lval = - v->val.lval;
	else
		v->val.dval = - v->val.dval;

	return( v );
}


/*--------------------------------------------------------------------------*/
/* vars_comp() is used for comparing the values of two variables. There are */
/* three types of comparision, it can be tested if two variables are equal, */
/* the first one is less than the second or if the first is less or equal   */
/* than the second variable (tests for greater or greater or equal can be   */
/* done simply by switching the arguments).                                 */
/* ->                                                                       */
/*    * type of comparision (COMP_EQUAL, COMP_LESS or COMP_LESS_EQUAL)      */
/*    * pointers to the two variables                                       */
/* <-                                                                       */
/*    * 1 (TRUE) or 0 (FALSE) depending on result of comparision            */
/*--------------------------------------------------------------------------*/

Var *vars_comp( int comp_type, Var *v1, Var *v2 )
{
	Var *new_var;
	long res;


	/* make sure that `v1' and `v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	switch ( comp_type )
	{
		case COMP_EQUAL :
			res = (    ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    == ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, res );
			break;

		case COMP_LESS :
			res = (   ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    < ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, res );
			break;

		case COMP_LESS_EQUAL :
			res = (    ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    <= ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, res );
			break;

		default:               /* this should never happen... */
			assert( 1 == 0 );
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*--------------------------------------------------------------*/
/* vars_push_copy() creates a new entry on the variable stack   */
/* as a copy of an already existing variable (this is checked). */
/*--------------------------------------------------------------*/

Var *vars_push_copy( Var *v )
{
	assert( vars_exist( v ) );

	if ( v->type == UNDEF_VAR )
	{
		if ( v->name != NULL )
		{
			eprint( FATAL, "%s:%ld: The accessed variable `%s' has never been "
					"assigned a value.\n", Fname, Lc );
			THROW( VARIABLES_EXCEPTION );
		}
		else
		{
			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s:%d.\n",
					__FILE__, __LINE__ );
			exit( EXIT_FAILURE );
		}
	}

	/* check that it exists and is a simple variable etc. */

	vars_check( v, INT_VAR | FLOAT_VAR );

	/* push a transient variable onto the stack with the relevant data set */

	if ( v->type == INT_VAR )
		return( vars_push( INT_VAR, v->val.lval ) );
	else
		return( vars_push( FLOAT_VAR, v->val.dval ) );
}


/*-----------------------------------------------------------------------*/
/* vars_push() creates a new entry on the variable stack (which actually */
/* is not really a stack but a linked list) and sets its type and value. */
/* When the type of the variable is undefined and datas is non-zero this */
/* signifies that this is the start of a print statement - in this case  */
/* data points to the format string and has to be copied to the new      */
/* variables name.                                                       */
/*-----------------------------------------------------------------------*/

Var *vars_push( int type, ... )
{
	Var *new_stack_var, *stack;
	va_list ap;


	/* get memory for the new variable to be apppended to the stack
	   and setits type */

	new_stack_var = T_malloc( sizeof( Var ) );
	new_stack_var->type = type;

	/* get the data for the new variable */

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
			new_stack_var->val.sptr = get_string_copy( va_arg( ap, char * ) );
			break;

		case FUNC :
			/* getting the function pointer seems to be to complicated for
			   va_arg when written directly thus `FnctPtr' is a typedef
			   (see start of file) */

			new_stack_var->val.fnct = va_arg( ap, FnctPtr );
			break;

		case ARR_PTR :
			new_stack_var->val.vptr = va_arg( ap, Var * );
			break;

		case INT_PTR :
			new_stack_var->val.lpnt = va_arg( ap, long * );
			new_stack_var->from = va_arg( ap, Var * );
			break;

		case FLOAT_PTR :
			new_stack_var->val.dpnt = va_arg( ap, double * );
			new_stack_var->from = va_arg( ap, Var * );
			break;

		default :
			assert( 1 == 0 );
	}

	va_end( ap );
	
	/* clear its `flag' and set the `name' and `next' entry to NULL */

	new_stack_var->name = NULL;
	new_stack_var->next = NULL;
	new_stack_var->flags = 0;

	/* and finally append it to the end of the stack */

	if ( ( stack = Var_Stack ) == NULL )
	{
		Var_Stack = new_stack_var;
		new_stack_var->prev = NULL;
	}
	else
	{
		while ( stack->next != NULL )
			stack = stack->next;
		stack->next = new_stack_var;
		new_stack_var->prev = stack;
	}

	/* return address of the new stack variable */

	return( new_stack_var );
}


/*----------------------------------------------------------------*/
/* vars_pop() checks if a variable is on the variable stack and   */
/* if it does removes it from the linked list making up the stack */
/*----------------------------------------------------------------*/

void vars_pop( Var *v )
{
	Var *stack = Var_Stack,
		*prev = NULL;


	if ( v == NULL )
		return;

	/* figure out if v is on the stack */

	while ( stack )
		if ( stack == v )
			break;
		else
		{
			prev = stack;
			stack = stack->next;
		}

	/* if v is on the stack remove it */

	if ( stack != NULL )
	{
		if ( prev != NULL )
		{
			prev->next = stack->next;
			if ( stack->next != NULL )
				stack->next->prev = prev;
		}
		else
		{
			Var_Stack = stack->next;
			if ( stack->next != NULL )
				stack->next->prev = NULL;
		}

		if ( stack->type == STR_VAR )
			free( stack->val.sptr );
		if ( stack->type == FUNC )
			free( stack->name );

		free( stack );
	}
}


/*-----------------------------------------------------*/
/* vars_del_stack() deletes all entries from the stack */
/*-----------------------------------------------------*/

void vars_del_stack( void )
{
	while ( Var_Stack != NULL )
		vars_pop( Var_Stack );
}


/*----------------------------------------------------------*/
/* vars_clean_up() deletes the variable and array stack and */
/* removes all variables from the list of variables         */
/*----------------------------------------------------------*/

void vars_clean_up( void )
{
	vars_del_stack( );
/*	vars_del_astack( );*/
	free_vars( );
}


/*--------------------------------------------------------------*/
/* free_vars() removes all variables from the list of variables */
/*--------------------------------------------------------------*/

void free_vars( void )
{
	Var *vp;


	while ( var_list != NULL )
	{
		vp = var_list->next;
		free( var_list );
		var_list = vp;
	}
}


void vars_check( Var *v, int type )
{
	/* being paranoid we first check that the variable exists at all -
	   probably this can be left out later. */

	assert( vars_exist( v ) );

	/* check that the variable has a value assigned to it */

	if ( v->type == UNDEF_VAR )
	{
		if ( v->name != NULL )
		{
			eprint( FATAL, "%s:%ld: The accessed variable `%s' has never been "
					"assigned a value.\n", Fname, Lc );
			THROW( VARIABLES_EXCEPTION );
		}
		else
		{
			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s:%d.\n",
					__FILE__, __LINE__ );
			exit( EXIT_FAILURE );
		}
	}
	
	/* check that the variable has the correct type */

	if ( ! ( v->type & type ) )
	{
		eprint( FATAL, "%s:%ld: Wrong type of variable.\n",
				Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	vars_warn_new( v );
}


/*-------------------------------------------------------------------*/
/* vars_warn_new() prints out a warning if a variable never has been */
/* assigned a value                                                  */
/*-------------------------------------------------------------------*/

void vars_warn_new( Var *v )
{
 	if ( v->flags & NEW_VARIABLE )
	{
		if ( v->name != NULL )
			eprint( WARN, "%s:%ld: WARNING: Variable `%s' has never been "
					"assigned a value.\n", Fname, Lc, v->name );
		else
		{
			/* ERROR: transient variables shouldn't have a name (with the
			   only exception for functions, but then we should never end
			   up here...) */ 

			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s.\n",
					__FILE__, __LINE__ );
			exit( EXIT_FAILURE );
		}
	}
}


bool vars_exist( Var *v )
{
	Var *lp;


	for ( lp = var_list; lp != NULL; lp = lp->next )
		if ( lp == v )
			return( OK );

	for ( lp = Var_Stack; lp != NULL; lp = lp->next )
		if ( lp == v )
			return( OK );

	eprint( FATAL, "fsc2: INTERNAL ERROR: Use of non-existing "
			"variable detected at %s:%d.\n", __FILE__, __LINE__ );
	return( FAIL );
}



/*-------------------------------------------------------------------*/
/* This function is called when a `VAR_TOKEN [' combination is found */
/* in the input. For a new array it sets its type. Everything else   */
/* it does is pushing a variable with a generic pointer to the array */
/* onto the stack.                                                   */
/*-------------------------------------------------------------------*/


Var *vars_arr_start( Var *v )
{
	Var *ret;

	/* test if variable exists */

	assert( vars_exist( v ) );

	/* check if the array is completely new (type is still UNDEF_VAR) or
	   otherwise if its really an array */

	if ( v->type == UNDEF_VAR )
		v->type = IF_FUNC( v->name[ 0 ] ) ? INT_ARR : FLOAT_ARR;
	else
		vars_check( v, INT_ARR | FLOAT_ARR );

	/* push variable with generic pointer to array onto the stack */

	ret = vars_push( ARR_PTR, v );
	ret->flags |= IS_META;
	return( ret );
}


/*----------------------------------------------------------------------*/
/* The function is called when the end of an array access (indicated by */
/* the `]') is found on the left hand side of an assignment. If it is   */
/* called for a new arrray the indices found on the stack are the sizes */
/* for the different dimensions of the array and are used to setup the  */
/* the array.                                                           */
/*----------------------------------------------------------------------*/

Var *vars_arr_lhs( Var *v )
{
	int dim;
	Var *a, *cv;


	while ( !( v->flags & IS_META ) )
		v = v->prev;

	/* check that it's really a variable with a generic array pointer */

	vars_check( v, ARR_PTR );
	a = v->val.vptr;

	/* count the variable below v on the stack */

	for ( dim = 0, cv = v->next; cv != 0; cv = cv->next )
		if ( cv->type != UNDEF_VAR )
			++dim;

	/* if the array is new we need to set it up */

	if ( a->flags & NEW_VARIABLE )
		return( vars_setup_new_array( a, dim, v ) );

	/* push a pointer to the indexed element onto the stack */

	return( vars_get_lhs_pointer( a, v, dim ) );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *vars_get_lhs_pointer( Var *a, Var *v, int dim )
{
	Var *ret;
	long index;


	/* check that there are enough indices on the stack (since variable sized
       arrays allow assignment of array slices there needs to be only one
       index less than the dimension of the array) */

	if ( dim < a->dim - 1 )
	{
		eprint( FATAL, "%s:%ld: Not enough indices found for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Too many indices found for %d-dimensional "
				"array `%s'.\n", Fname, Lc, a->dim, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* for arrays that still are variable sized we need assignment of a
       complete slice in order to determine the missing size of the last
       dimension */

	if ( need_alloc( a ) && dim != a->dim - 1 )
	{
		eprint( FATAL, "%s:%ld: Size of array `%s' is still unknown, only %d "
				"indices are allowed (plus assignment of an array slice).\n",
				Fname, Lc, a->name, a->dim - 1 );
		THROW( VARIABLES_EXCEPTION );
	}

	/* calculate the pointer to the indexed array element (or slice) */

	index = vars_calc_index( a, v->next );

	/* pop the variable with the array pointer */

	vars_pop( v );

	/* If the array is still variable sized and thus needs memory allocated we
	 push a pointer to the array onto the stack and store the indexed slice in
	 the variables structure `len' element. Otherwise we push a variable onto
	 the stack with a pointer to the indexed element or slice.*/

	if ( need_alloc( a ) )
	{
		ret = vars_push( a->type == INT_ARR ? INT_PTR : FLOAT_PTR, NULL, a );
		ret->len = index;
	}
	else
	{
		if ( a->type == INT_ARR )
			ret = vars_push( INT_PTR, a->val.lpnt + index , a );
		else
			ret = vars_push( FLOAT_PTR, a->val.dpnt + index , a );
	}

	/* Set a flag if a slice is indexed */

	if ( dim == a->dim - 1 )
		ret->flags |= NEED_SLICE;

	return( ret );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

long vars_calc_index( Var *a, Var *v )
{
	Var *vn;
	int i, cur;
	long index;



	/* run through all the indices on the variable stack */

	for ( i = 0, index = 0; v != NULL; ++i, v = vn )
	{
		if ( v->type == UNDEF_VAR )
		{
			vn = v->next;
			vars_pop( v );
			continue;
		}

		/* check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* get current index and warn if it's a float variable */

		if ( v->type == INT_VAR )
			cur = v->val.lval - ARRAY_OFFSET;
		else
		{
			eprint( WARN, "%s:%ld: WARNING: Float variable used as index #%d "
					"for array `%s'.\n", Fname, Lc, i + 1, a->name );
			cur = ( int ) v->val.dval - ARRAY_OFFSET;
		}

		/* check that the index is not too small or too large */

		if ( cur == - ARRAY_OFFSET && v->flags & VARIABLE_SIZED )
		{
			eprint( FATAL, "%s:%ld: A `*' as index is only allowed in "
					"declaration of an array, but not in assignment.\n",
					Fname, Lc );
			THROW( VARIABLES_EXCEPTION );
		}

		/* check that the index is not too small or too large */

		if ( cur < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
			"array `%s', minimum is %d.\n", Fname, Lc, i + 1,
					cur + ARRAY_OFFSET, a->name, ARRAY_OFFSET );
			THROW( VARIABLES_EXCEPTION );
		}

		if ( cur >= a->sizes[ i ] )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
					"array `%s', maximum is %d.\n",
					Fname, Lc, i + 1, cur + ARRAY_OFFSET, a->name,
					a->sizes[ i ] - 1 + ARRAY_OFFSET );
			THROW( VARIABLES_EXCEPTION );
		}

		/* update the index */

		index += index * a->sizes[ i ] + cur;

		/* pop the variable with the index */

		vn = v->next;
		vars_pop( v );
	}

	/* for slices we need another update of the index */

	if ( i != a->dim )
		index += index * a->sizes[ i ];

	return( index );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *vars_setup_new_array( Var *a, int dim, Var *v )
{
	int i, cur;
	Var *vn, *ret;


	if ( v->next->type == UNDEF_VAR )
	{
		eprint( FATAL, "%s:%ld: Missing indices in declaration of array "
				"`%s'.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* set array's dimension and allocate memory for their sizes */

	a->dim = dim;
	a->sizes = T_malloc( dim * sizeof( int ) );
	a->flags &= ~NEW_VARIABLE;
	a->len = 1;

	/* run through the variables with the sizes after ppopping the variable
       with the array pointer */

	vn = v->next;
	vars_pop( v );
	v = vn;

	for ( i = 0; v != NULL; ++i )
	{
		/* check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* check the value of variable with the size and set the corresponding
		   entry in the array's field for sizes */

		if ( v->type == INT_VAR )
		{
			/* if the the very last variable with the sizes has the flag
			   VARIABLE_SIZED set this is going to be a dynamically sized
			   array - set the corresponding flag in the array variable,
			   don't reset its NEW_VARIABLE flag and don't allocate memory
			   and return a variable with a generic pointer to the array. */

			if ( v->flags & VARIABLE_SIZED )
			{
				if ( i != dim - 1 )
				{
					eprint( FATAL, "%s:%ld: Only the very last dimension of "
							"an array can be set dynamically.\n", Fname, Lc );
					THROW( VARIABLES_EXCEPTION );
				}

				vars_pop( v );

				a->flags &= ~NEW_VARIABLE;
				a->sizes[ i ] = 0;
				a->flags |= NEED_ALLOC;
				ret = vars_push( a->type == INT_ARR ? INT_PTR: FLOAT_PTR,
								 NULL, a );
				ret->flags |= IS_META;
				return( ret );
			}

			cur = ( int ) v->val.lval;
		}
		else
		{
			eprint( WARN, "%s:%ld: WARNING: FLOAT value (value=%f) used as "
					"size in definition of array `%s'.\n",
					Fname, Lc, v->val.dval, a->name );
			cur = ( int ) v->val.dval;
		}

		if ( cur < 2 )
		{
			eprint( FATAL, "%s:%ld: Invalid size (value=%d) used in "
					"definition of array `%s', minimum is 2.\n",
					Fname, Lc, cur, a->name );
			THROW( VARIABLES_EXCEPTION );
		}

		a->sizes[ i ] = cur;
		a->len *= cur;

		/* pop the variable with the size */

		vn = v->next;
		vars_pop( v );
		v = vn;
	}

	/* allocate memory */

	if ( a->type == INT_ARR )
		a->val.lpnt = ( long * ) T_calloc( ( size_t ) a->len, sizeof( long ) );
	else
		a->val.dpnt = ( double * ) T_calloc( ( size_t ) a->len,
											 sizeof( double ) );

	a->flags &= ~NEW_VARIABLE;
	if ( a->type == INT_ARR )
		ret = vars_push( INT_PTR, NULL, a );
	else
		ret = vars_push( FLOAT_PTR, NULL, a );
	ret->flags |= NEED_INIT;
	ret->flags |= IS_META;

	return( ret );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *vars_arr_rhs( Var *v )
{
	int dim;
	Var *a, *cv;
	long index;


	/* The variable pointer this function gets passed is a pointer to the very
       last index on the variable stack. Now we've got to work our way up in
       the variable stack until we find the first non-index variable which
       should be a pointer to an array. */

	while ( ! ( v->flags & IS_META ) )
		v = v->prev;
	vars_check( v, ARR_PTR );         /* check it's really an array pointer */

	a = v->val.vptr;                  /* get array ther pointer points to */

	/* If the right hand side array is still variable sizes it never has been
       assigned values to it and it makes no sense to use its elements */

	if ( a->flags & NEED_ALLOC )
	{
		eprint( FATAL, "%s:%ld: The array `%s' is dynamically sized and has "
				"not been assigned data yet.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* count the indices on the stack */

	for ( dim = 0, cv = v->next; cv != 0; ++dim, cv = cv->next )
		;

	/* check that the number of indices is not less than the dimension of the
       array minus one - we allow slice access for the very last dimension */

	if ( dim < a->dim - 1 )
	{
		eprint( FATAL, "%s:%ld: Not enough indices found for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Too many indices found for %d-dimensional "
				"array `%s'.\n", Fname, Lc, a->dim, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* calculate the position of the indexed array element (or slice) */

	index = vars_calc_index( a, v->next );

	/* pop the array pointer variable */

	vars_pop( v );

 	/* If this is an element access return a variable with the value of the
	   accessed element, otherwise return a pointer to the first element of
	   the slice. */

	if ( dim == a->dim )
	{
		if ( a->type == INT_ARR )
			return( vars_push( INT_VAR, *( a->val.lpnt + index ) ) );
		else
			return( vars_push( FLOAT_VAR, *( a->val.dpnt + index ) ) );
	}
	else
	{
		if ( a->type == INT_ARR )
			return( vars_push( INT_PTR, a->val.lpnt + index, a ) );
		else
			return( vars_push( FLOAT_PTR, a->val.dpnt + index, a ) );
	}

	assert( 1 == 0 );       /* we never should end here... */
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_assign( Var *src, Var *dest )
{
	/* <PARANOID> check that both variables really exist </PARANOID> */

	assert( vars_exist( src ) && vars_exist( dest ) );

	if ( src->type & ( INT_VAR | FLOAT_VAR ) )  /* if src is simple variable */
		vars_ass_from_var( src, dest );
	else                                        /* if src is an array slice */
		vars_ass_from_ptr( src, dest );

	vars_pop( dest );
	vars_pop( src );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_ass_from_var( Var *src, Var *dest )
{
	/* don't do the assignment if the right hand side has no value */

	if ( src->flags & NEW_VARIABLE )
	{
		eprint( FATAL, "%s:%ld: On right hand side of assignment a "
				"variable is used that has never been assigned a value.\n",
				Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
	}

	/* stop if left hand side needs an array slice but all we have is
	   a variable */

	if ( dest->flags & NEED_SLICE )
	{
		eprint( FATAL, "%s:%ld: In assignment to array `%s' an array "
				"slice is needed on the right hand side.\n",
				Fname, Lc, dest->from->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* if the destination variable is still completely new set its type */

	if ( dest->type == UNDEF_VAR )
	{
		dest->type = IF_FUNC( dest->name[ 0 ] ) ? INT_VAR : FLOAT_VAR;
		dest->flags &= ~NEW_VARIABLE;
	}

	/* now do the assignment */

	if ( dest->type == INT_VAR )
	{
		if ( src->type == INT_VAR )
			dest->val.lval = src->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: WARNING: Using float value in "
					"assignment to integer variable.\n", Fname, Lc );
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
}
 

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_ass_from_ptr( Var *src, Var *dest )
{
	Var *d, *s;
	int i;


	/* may be that's paranoid, but... */

	assert( src->type & ( INT_PTR | FLOAT_PTR ) );

	/* we can't assign from an array slice to a variable */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) )
	{
		eprint( FATAL, "%s:%ld: Left hand side of assignment is a variable "
				"while right hand side is an array slice.\n", Fname, Lc ); 
		THROW( VARIABLES_EXCEPTION );
	}

	/* Again being paranoid... */

	assert( dest->flags & NEED_SLICE );

	d = dest->from;
	s = src->from;

	/* Do allocation of memory (set size of missing dimension to the last one
	   of the source array) if the destination array needs it, otherwise check
	   that sizes of the array sizes is equal. */

	if ( d->flags & NEED_ALLOC )
	{
		d->sizes[ d->dim - 1 ] = s->sizes[ s->dim - 1 ];

		d->len *= d->sizes[ d->dim - 1 ];

		if ( d->type == INT_ARR )
		{
			d->val.lpnt = T_calloc( d->len,  sizeof( long ) );
			dest->val.lpnt = d->val.lpnt + dest->len;
		}
		else
		{
			d->val.dpnt = T_calloc( d->len, sizeof( double ) );
			dest->val.dpnt = d->val.dpnt + dest->len;
		}

		d->flags &= ~NEED_ALLOC;
	}
	else
	{
		if ( d->sizes[ d->dim - 1 ] != s->sizes[ s->dim - 1 ] )
		{
			eprint( FATAL, "%s:%ld: Array slices of array `%s' and `%s' have "
					"different size.\n", Fname, Lc, d->name, s->name );
			THROW( VARIABLES_EXCEPTION );
		}
	}

	/* Warn on float to integer assignment */

	if ( d->type == INT_ARR && s->type == FLOAT_ARR )
		eprint( WARN, "%s:%ld: Assignment of slice of float array `%s' to "
				"integer array `%s'.\n", Fname, Lc, s->name, d->name );

	/* Now copy the array slice */

	for ( i = 0; i < d->sizes[ d->dim - 1 ]; ++i )
	{
		if ( d->type == INT_ARR )
		{
			if ( s->type == INT_ARR )
				*dest->val.lpnt++ = *src->val.lpnt++;
			else
				*dest->val.lpnt++ = ( long ) *src->val.dpnt++;
		}
		else
		{
			if ( s->type == INT_ARR )
				*dest->val.dpnt++ = ( double ) *src->val.lpnt++;
			else
				*dest->val.dpnt++ = *src->val.dpnt++;
		}
	}
}



void vars_arr_init( Var *v )
{
	long ni;
	Var *p1, *p2, *a;


	if ( v->type == UNDEF_VAR )      /* no initialization */
	{
		vars_pop( v->prev );
		vars_pop( v );
		return;
	}


	/* The variable pointer this function gets passed is a pointer to the very
       last intialisation data on the variable stack. Now we've got to work
       our way up in the variable stack until we find the first non-data
       variable which should be a pointer to an array. */

	while ( ! ( v->flags & IS_META ) )
		v = v->prev;
	vars_check( v, INT_PTR | FLOAT_PTR );

	a = v->from;

	/* variable sized arays can't be initialized, they need the assignment
	   of an array slice n order to determine the still unknown size of the
	   last dimension */

	if ( need_alloc( a ) )
	{
		eprint( FATAL, "%s:%ld: Variable sized array `%s' can not be "
				"initialized.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* If the array isn't just declared we can't do an assignment anymore */

	if ( ! ( v->flags & NEED_INIT ) )
	{
		eprint( FATAL, "%s:%ld: Initialization of array `%s' only allowed "
				"immediately after declaration.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}


	/* count number of initializers and check that it fits the length of
	   the array */

	for ( p1 = v->next, ni = 0; p1 != NULL; ++ni, p1 = p1->next )
		;

	if ( ni < a->len )
		eprint( WARN, "%s:%ld: There are less initializers for array `%s' "
				"than the array has elements.\n", Fname, Lc, a->name );

	if ( ni > a->len )
	{
		eprint( FATAL, "%s:%ld: there are more initializers for array `%s' "
				"than the array has elements.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* Finaly do the assignments */

	if ( a->type == INT_ARR )
		v->val.lpnt = a->val.lpnt;
	else
		v->val.dpnt = a->val.dpnt;

	for ( p1 = v->next; p1 != NULL; p1 = p2 )
	{
		assert( p1->type & ( INT_VAR | FLOAT_VAR ) );

		if ( a->type == INT_ARR )
		{
			if ( p1->type == INT_VAR )
				*v->val.lpnt++ = p1->val.lval;
			else
			{
				eprint( WARN, "%s:%ld: Float value used in initialization of "
						"integer array `%s'.\n", Fname, Lc, a->name );
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

	vars_pop( v );
}
