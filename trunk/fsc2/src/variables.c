/* 
   $Id$

   $Log$
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


/*---------------------------------------------------------------------------*/
/* vars_new_assign() assigns a value to a new integer or float variable.     */
/* ATTENTION: This routine is only to be used for assignments for new vari-  */
/*            ables, i.e. in the VARIABLES section - in other parts of the   */
/*            program we'll have to use a modified version of this routine!  */
/* ->                                                                        */
/*    * source variable - this will always be a transient variable from the  */
/*      variables stack!                                                     */
/*    * variable the value is to be assigned to                              */
/* <-                                                                        */
/*    * pointer to `dest', i.e. the variable the value is assigned to        */
/*---------------------------------------------------------------------------*/

Var *vars_new_assign( Var *src, Var *dest )
{
	/* Make real sure we assign to a variable that does exist - just being
       paranoid... */

	assert( dest != NULL );
	assert( src != NULL );
	assert( src->type == INT_VAR || src->type == FLOAT_VAR );

	/* We're still in the definition part - so assigning a new value to a
	   variable that already has a value assigned to it is a syntax error */
	   
	if ( dest->type != UNDEF_VAR )
	{
		eprint( FATAL, "%s:%ld: Variable `%s' already exists.\n",
				Fname, Lc, dest->name );
		THROW( MULTIPLE_VARIABLE_DEFINITION_EXCEPTION );
	}

	/* after this test we can set the variable type - if the name starts we a
	   capital letter it's an INT_VAL otherwise a FLOAT_VAR */

	dest->type = IF_FUNC( dest->name[ 0 ] ) ? INT_VAR : FLOAT_VAR;

	if ( ! ( src->flags & NEW_VARIABLE ) )
	{
		if ( dest->type == INT_VAR )
		{
			if ( src->type == INT_VAR )
				dest->val.lval = src->val.lval;
			else
			{
				eprint( WARN, "%s:%ld: Assigning float value to integer "
						"variable `%s'.\n", Fname, Lc, dest->name );
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

		dest->flags &= ~NEW_VARIABLE;
	}
	else
	{
		/* warn (and don't use it) if `src' exists but has never been assigned
		   a value, instead assign `dest' a value of zero */

		if ( dest->type == INT_VAR )
			dest->val.lval = 0;
		else
			dest->val.dval = 0.0;
	}

	/* pop source variable from variables stack and return the new variable */

	vars_pop( src );
	return( dest );
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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v1 );
	vars_check( v2 );

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

	vars_check( v );

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

	vars_check( v1 );
	vars_check( v2 );

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


/*--------------------------------------------------------------------------*/
/* vars_push_simple() creates a new entry on the variable stack as a copy   */
/* of an already existing variable (this is checked).                       */
/*--------------------------------------------------------------------------*/

Var *vars_push_copy( Var *v )
{
	/* Make sure it's really a simple variable */

	if ( v->type == INT_VAR && v->type == FLOAT_VAR )
	{
		eprint( FATAL, "%s:%ld: `%s' is an array and can't be used in this "
				"context.\n", Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	if ( v->type == INT_VAR && v->type == FLOAT_VAR )
	{
		eprint( FATAL, "%s:%ld: `%s' is an array and can't be used in this "
				"context.\n", Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that it exists etc. */

	vars_check( v );

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
			assert( 1 == 0 );
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
		Var_Stack = new_stack_var;
	else
	{
		while ( stack->next != NULL )
			stack = stack->next;
		stack->next = new_stack_var;
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
			prev->next = stack->next;
		else
			Var_Stack = stack->next;

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


#if 0

/*-----------------------------------------------------------*/
/* vars_arr_start() starts the declaration of an array       */
/* ->                                                        */
/*    * pointer to the new variable to be used for the array */
/*-----------------------------------------------------------*/

void qvars_arr_start( Var *a )
{
	if ( a->type != UNDEF_VAR )
	{
		eprint( FATAL, "%s:%ld: Variable or array `%s' already exists.\n",
				Fname, Lc, a->name );
		THROW( MULTIPLE_VARIABLE_DEFINITION_EXCEPTION );
	}

	/* do clean up of former array declarations */

	vars_arr_init( NULL, NULL );

	/* set up the array for one dimension */

	a->type = IF_FUNC( a->name[ 0 ] ) ? INT_ARR : FLOAT_ARR;
	a->dim = 0;
	a->sizes = NULL;
}


/*----------------------------------------------------------*/
/* vars_arr_extend() is called in the declaration of arrays */
/* with more than one dimension for each new dimension      */
/* ->                                                       */
/*    * pointer to an array variable                        */
/*    * pointer to variable with size of the next dimension */
/*----------------------------------------------------------*/

void qvars_arr_extend( Var *a, Var *s )
{
	long size;


	/* Get array dimension and make sure its larger than zero, but there's
	   also a special case: when the pointer to the variable `s' is NULL this
	   means we need an array of variable size. Variable sizes are only
	   allowed for the very last dimension of the array. */

	if ( a->dim >= 1 && is_variable_array( a ) )
	{
		eprint( FATAL, "%s:%ld: Only the very last dimension of an array "
				"can be set dynamically.\n", Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
	}

	if ( s == NULL )
		size = 0;
	else
	{
		/* make sure that the variable with the size exists, has integer or
		   float type and has a value assigned to it */

		vars_check( s );

		size = ( s->type == INT_VAR ) ? s->val.lval : ( long ) s->val.dval;
		if ( size <= 0 )
		{
			eprint( FATAL, "%s:%ld: Zero or negative array size #%ld "
					"(value=%ld) for array `%s'.\n",
					Fname, Lc, a->dim + 1, size, a->name );
			THROW( VARIABLES_EXCEPTION );
		}
	}

	/* extend the `sizes' entry and store the size for this dimension */

	if ( a->sizes == NULL )
		a->sizes = T_malloc( sizeof( int ) );
	else
		a->sizes = T_realloc( ( void * ) a->sizes,
							  ( a->dim + 1 )* sizeof( int ) );
	a->sizes[ a->dim++ ] = size;

	if ( s )                 /* only for non-variable-size arrays */
		vars_pop( s );
}


/*----------------------------------------------------------------------*/
/* vars_arr_init() is called in the initialisation of arrays for each   */
/* of the initializers - if called with `a' set to NULL the static      */
/* variable `cur_elem' is reset (this is automatically done by          */
/* `vars_arr_start()).                                                  */
/* ->                                                                   */
/*    * pointer to the array to be initialized (the static variable     */
/*      `cur_elem' contains the number of the element to be initialized */
/*      and that's the reason why it got to be reset before initiali-   */
/*      zing another array)                                             */
/*    * pointer to variable with the value to be assigned to the array  */
/*      element                                                         */
/*----------------------------------------------------------------------*/

void vars_arr_init( Var *a, Var *d )
{
	static long cur_elem = 0;
	long i;


	/* after the last value the function has to be called with the array
	   pointer set to NULL to do the clean up - same is done after an error */

	if ( a == NULL )
	{
		cur_elem = 0;
		vars_pop( d );
		return;
	}
	
	/* make sure we don't initialize a variable sized array */

	if ( is_variable_array( a ) )
	{
		eprint( FATAL, "%s:%ld: Dynamically sized arrays can't be "
				"initialized.\n", Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that there are not more initializers than elements in the array */

	if ( ! ( a->flags & NEW_VARIABLE ) && cur_elem == a->len )
	{
		eprint( FATAL, "%s:%ld: Too many initializers for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* make sure that `d' exists, has integer or float type and has
	   an value assigned to it */

	vars_check( d );

	/* if the array has the `new_flag' still set, i.e. this is the very first
	   asssignment, allocate memory for the data */

	if ( a->flags & NEW_VARIABLE )
	{
		for ( a->len = 1, i = 0; i < a->dim; ++i )
			a->len *= a->sizes[ i ];
		
		if ( a->type == INT_VAR )
			a->val.lpnt = ( long * ) T_calloc( ( size_t ) a->len,
											   sizeof( long ) );
		else
			a->val.dpnt = ( double * ) T_calloc( ( size_t ) a->len,
												 sizeof( double ) );

		a->flags &= NEW_VARIABLE;
	}

	/* assign the new data to the array - if the array is an integer array but
	   the data is a float print out a warning */

	if ( a->type == INT_ARR )
	{
		if ( d->type == INT_VAR )
			a->val.lpnt[ cur_elem++ ] = d->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Assigning float value to element of "
					"integer array `%s'.\n", Fname, Lc, a->name );
			a->val.lpnt[ cur_elem++ ] = ( long ) d->val.dval;
		}
	}
	else
		a->val.dpnt[ cur_elem++ ] = ( d->type == INT_VAR ) ?
				                          ( double ) d->val.lval : d->val.dval;

	/* pop the data from the stack */

	vars_pop( d );
}


/*------------------------------------------------------------------------*/
/* vars_push_astack() pushes an array variable onto the array stack for   */
/* the evaluation of the following indices. In order to be able to have   */
/* indices made up from elements of other arrays we need a kind of stack, */
/* i.e. the array stack. A new entry in the stack will be put on top of   */
/* the stack.                                                             */
/*------------------------------------------------------------------------*/

void vars_push_astack( Var *v )
{
	AStack *tmp;


	/* check that array is really an array */

	if ( v->type == UNDEF_VAR )
	{
		eprint( FATAL, "%s:%ld: There is no array named `%s'.\n",
				Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	if ( v->type == INT_VAR || v->type == FLOAT_VAR )
	{
		eprint( FATAL, "%s:%ld: Numerical variable `%s' isn't an array.\n", 
				Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	assert( v->type == INT_ARR || v->type == FLOAT_ARR );

	/* you can't assign to or use elements of a dynamically sized array */

	if ( is_variable_array( v ) )
	{
		eprint( FATAL, "%s:%ld: Accessing elements of dynamically sized array "
				"`%s' is not possible.\n", Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* get memory on the array stack */

	tmp = T_malloc( sizeof( AStack ) );

	/* put `tmp' on top of stack */

	if ( Arr_Stack != NULL )
	{
		tmp->next = Arr_Stack;
		Arr_Stack = tmp;
	}
	else
	{
		Arr_Stack = tmp;
		Arr_Stack->next = NULL;
	}

	/* set all the other data */

	Arr_Stack->var = v;
	Arr_Stack->act_entry = 0;
	Arr_Stack->entries = T_malloc( v->dim * sizeof( long ) );

	vars_pop( v );    /* this isn't be needed ? */
}


/*-------------------------------------------------------------------------*/
/* vars_pop_astack() is called when all the indices are read in. It pushes */
/* the value of the accessed array element on the variable stack and the   */
/* array variable is popped from the top of the array stack.               */
/* <-                                                                      */
/*    * variable with the value of the accessed array element              */
/*-------------------------------------------------------------------------*/

Var *vars_pop_astack( void )
{
	long i, index;
	Var *ret;


	/* check that there are enough indices */

	if ( Arr_Stack->act_entry != Arr_Stack->var->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %ld-dimensional but only %ld "
				"indices are given.\n", Fname, Lc, Arr_Stack->var->name,
				 Arr_Stack->var->dim, Arr_Stack->act_entry );
		THROW( VARIABLES_EXCEPTION );
	}

	/* Calculate the index of the specified element */

	for ( index = Arr_Stack->entries[ 0 ], i = 1; 
		  i < Arr_Stack->var->dim; ++i )
		index =   index * ( Arr_Stack->var->sizes[ i - 1 ] - 1 )
			    + Arr_Stack->entries[ i ];

	/* make sure we did it right */

	assert( index < Arr_Stack->var->len );

	/* create the resulting data by pushing it on the stack */

	if ( Arr_Stack->var->type == INT_ARR )
		ret = vars_push( INT_VAR, Arr_Stack->var->val.lpnt[ index ] );
	else
		ret = vars_push( FLOAT_VAR, Arr_Stack->var->val.dpnt[ index ] );

	/* finally delete the current Arr_Stack entry */

	vars_del_top_astack( );

	return( ret );
}


/*-----------------------------------------------------------------------*/
/* vars_arr_assign() is called in the assignment of a value to an array  */
/* element when all the indices are read in and the value to be assigned */
/* to the array element is known. The indices of the accessed array      */
/* element are collected in the topmost element of the array stack and   */
/* the value to be assigned to the array element is in a transient       */
/* variable on variable stack. This routine is rather similar to the one */
/* used in the access of an array element, vars_pop_astack().            */
/* ->                                                                    */
/*    * pointer to the array with the element to be set                  */
/*    * pointer to the transient variable with the value to be assigned  */
/*-----------------------------------------------------------------------*/

void vars_arr_assign( Var *a, Var *v )
{
	long i, index, len;


	/* allocate memory if the array has its `new_flag' still set */

	if ( Arr_Stack->var->flags & NEW_VARIABLE )
	{
		for ( len = Arr_Stack->var->sizes[ 0 ], i = 1;
			  i < Arr_Stack->var->dim; ++i )
			len *= Arr_Stack->var->sizes[ i ];

		if ( Arr_Stack->var->type == INT_ARR )
			Arr_Stack->var->val.lpnt = T_malloc( len * sizeof( long ) );
		else
			Arr_Stack->var->val.dpnt = T_malloc( len * sizeof( double ) );
		Arr_Stack->var->len = len;
		Arr_Stack->var->flags &= ~NEW_VARIABLE;
	}

	/* check that there are enough indices */

	if ( Arr_Stack->act_entry != Arr_Stack->var->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %ld-dimensional but only %ld "
				"indices are given.\n", Fname, Lc, Arr_Stack->var->name,
				 Arr_Stack->var->dim, Arr_Stack->act_entry );
		THROW( VARIABLES_EXCEPTION );
	}

	/* Calculate the index of the specified element */

	for ( index = Arr_Stack->entries[ 0 ], i = 1; 
		  i < Arr_Stack->var->dim; ++i )
		index =   index * ( Arr_Stack->var->sizes[ i - 1 ] - 1 )
			    + Arr_Stack->entries[ i ];

	/* make sure we did it right */

	assert( index < Arr_Stack->var->len );

	/* set the indexed array element to the value passed in the transient
	   variable on the variables stack and if a float value is assigned to an
	   integer array print a warning */

	if ( a->type == INT_ARR )
	{
		if ( v->type == INT_VAR )
			a->val.lpnt[ index ] = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Assigning float value to element of "
					"integer array `%s'.\n", Fname, Lc, a->name );
			a->val.lpnt[ index ] = ( long ) v->val.dval;
		}
	}
	else
		a->val.dpnt[ index ] = v->type == INT_VAR ?
			                              ( double ) v->val.lval : v->val.dval;

	/* finally delete the current Arr_Stack entry and pop the transient
	   variable from the variables stack */

	vars_del_top_astack( );
	vars_pop( v );
}


/*-----------------------------------------------------------------------*/
/* vars_update_astack() is called for each array index when accessing an */
/* array element. The indices are successively stored in the array stack */
/* variable (in the AStack structure entry `entries') on the array stack */
/* for calculation of the position of the array element later on (in the */
/* function `vars_pop_astack()').                                        */
/* ->                                                                    */
/*    * variable with next array index                                   */
/*-----------------------------------------------------------------------*/

void vars_update_astack( Var *v )
{
	long index;


	/* make sure there are not more indices than dimensions */

	if ( Arr_Stack->act_entry >= Arr_Stack->var->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %ld-dimensional but more "
				"indices are given.\n", Fname, Lc, Arr_Stack->var->name,
				 Arr_Stack->var->dim );
		THROW( VARIABLES_EXCEPTION );
	}

	/* make sure that `v' exists, has integer or float type and has
	   an value assigned to it - warn if it's a float variable */

	if ( v->type == INT_ARR || v->type == FLOAT_ARR )
	{
		eprint( FATAL, "%s:%ld: Array `%s' can't be used as array index.\n",
				Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	vars_check( v );

	if ( v->type == FLOAT_VAR )
		eprint( WARN, "%s:%ld: WARNING: FLOAT value (value=%f) used as array "
				"index.\n", Fname, Lc, v->val.dval );

	/* check that the index isn't negative or too large (take care to
	   handle differences between C- and FORTRAN-style indexing right) */

	if ( v->type == INT_VAR )
		index = v->val.lval;
	else
		index = ( long ) v->val.dval;

	if ( index - ARRAY_OFFSET < 0 )
	{
		eprint( FATAL, "%s:%ld: Invalid array index #%ld (value=%ld) for "
				"array `%s', minimum is %d.\n", Fname, Lc,
				Arr_Stack->act_entry + 1, index, Arr_Stack->var->name,
				ARRAY_OFFSET );
		THROW( VARIABLES_EXCEPTION );
	}

	if ( index - ARRAY_OFFSET >= 
		 Arr_Stack->var->sizes[ Arr_Stack->act_entry ] )
	{
		eprint( FATAL, "%s:%ld: Invalid array index #%ld (value=%ld) for "
				"array `%s', maximum is %ld.\n", Fname, Lc,
				Arr_Stack->act_entry + 1, index, Arr_Stack->var->name, 
				Arr_Stack->var->sizes[ Arr_Stack->act_entry ] - 1 
				+ ARRAY_OFFSET );
		THROW( VARIABLES_EXCEPTION );
	}

	/* finally store the index and pop the variable with the index */

	Arr_Stack->entries[ Arr_Stack->act_entry++ ] = index - ARRAY_OFFSET;

	vars_pop( v );
}


/*------------------------------------------------------------------------*/
/* vars_del_top_astack() removes the topmost element from the array stack */
/*------------------------------------------------------------------------*/

void vars_del_top_astack( void )
{
	AStack *tmp;


	if ( Arr_Stack == NULL )
		return;

	free( Arr_Stack->entries );
	tmp = Arr_Stack->next;
	free( Arr_Stack );
	Arr_Stack = tmp;
}


/*-------------------------------------------------------*/
/* vars_del_astack() completely destroys the array stack */
/*-------------------------------------------------------*/

void vars_del_astack( void )
{
	while ( Arr_Stack != NULL )
		vars_del_top_astack( );
}


#endif

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


void vars_check2( Var *v, int type )
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


/*-----------------------------------------------------------------------*/
/* vars_check() checks that a variable exists, has integer or float type */
/* and warns if it hasn't been assigned a value                          */
/*-----------------------------------------------------------------------*/

void vars_check( Var *v )
{
	if ( v->type == UNDEF_VAR )
	{
		if ( v->name != NULL )
		{
			eprint( FATAL, "%s:%ld: There is no variable named `%s'.\n",
					Fname, Lc, v->name );
			THROW( VARIABLES_EXCEPTION );
		}
		else
		{
			/* ERROR: undefined transient variables shouldn't have a name
			   and also never should have type UNDEF_VAR... */ 

			eprint( FATAL, "fsc2: INTERNAL ERROR detected at %s.\n",
					__FILE__, __LINE__);
			exit( EXIT_FAILURE );
		}
	}

	if ( v->type != INT_VAR && v->type != FLOAT_VAR )
	{
		if ( v->name != NULL )
			eprint( FATAL, "%s:%ld: `%s' is neither an integer nor a float "
					"variable.\n", Fname, Lc, v->name );
		else
			eprint( FATAL, "%s:%ld: Transient variable is neither an integer "
					"nor a float variable.\n", Fname, Lc );
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


/*--------------------------------------------------------------------*/
/* vars_assign() assigns a value to an already existing variable.     */
/* ->                                                                 */
/*    * source variable                                               */
/*    * variable the value is to be assigned to                       */
/* <-                                                                 */
/*    * pointer to `dest', i.e. the variable the value is assigned to */
/*--------------------------------------------------------------------*/

Var *vars_assign( Var *src, Var *dest )
{
	/* Make real sure we assign to a variable that does exist and also from a
	   variable that exists - just being paranoid... */

	assert( dest != NULL && src != NULL );

	/* make sure `src' has integer or float type - still being paranoid */

	assert( src->type == INT_VAR || src->type == FLOAT_VAR );
	assert( dest->type == INT_VAR || dest->type == FLOAT_VAR );

	/* assign the new value, warn if a float value is assigned to an integer
	   variable */

	if ( dest->type == INT_VAR )
	{
		if ( src->type == INT_VAR )
			dest->val.lval = src->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: Assigning float value to integer variable "
					"`%s'.\n", Fname, Lc, src->name );
			dest->val.lval = ( long ) src->val.dval;
		}
	}
	else
		dest->val.dval = ( src->type == INT_VAR ) ?
			                          ( double ) src->val.lval : src->val.dval;

	dest->flags &= ~NEW_VARIABLE;

	/* pop source variable from variables stack and return the new variable */

	vars_pop( src );
	return( dest );
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
/* it does is pushing a variable with a pointer to the array onto    */
/* the stack.                                                        */
/*-------------------------------------------------------------------*/


Var *vars_arr_start( Var *v )
{
	/* test if variable exists */

	assert( vars_exist( v ) );

	/* check if the array is completely new (type is still UNDEF_VAR) or
	   otherwise if its really an array */

	if ( v->type == UNDEF_VAR )
		v->type = IF_FUNC( v->name[ 0 ] ) ? INT_ARR : FLOAT_ARR;
	else
		vars_check2( v, INT_ARR | FLOAT_ARR );

	/* push variable with generic pointer to array onto the stack */

	return( vars_push( ARR_PTR, v ) );
}


/*---------------------------------------------------------------------*/
/* This function is called when on the left hand side of an assignment */
/* the end of the list of array indices is found (indicated by a `]'). */
/* It gets passed a pointer to a variable that points to the array.    */
/* All the variables below this variable are indices for the array     */
/* element. The only exception is the case that the array is still new */
/* - then these variables indicate the sizes of the dimensions.        */
/*
   So, what's returned?

   1. when called in VARIABLES section for a new array
     a. for a non-variable array a pointer to the first element of the
	    array is returned
     b. for a variable sized array an array pointer variable pointing to
	    NULL is returned (thus making initialisation of this kind of arrays
		impossible)

   2. normally
     a. for new, variable sized arrays a generic pointer to the array is
	    returned (actually the variable passed to the function) and the
		stack remains untouched
     b. normally a pointer to the element top be set - for variable sized
	    arrays when less indices than the dimensions of the array were
		on the stack the NEED_ARRAY_SLICE is set.

*/
/*---------------------------------------------------------------------*/

Var *vars_arr_set( Var *v )
{
	int dim;
	Var *a, *cv;


	/* check that it's really a variable with a generic array pointer */

	vars_check2( v, ARR_PTR );
	a = v->val.vptr;

	/* count the variable below v on the stack */

	for ( dim = 0, cv = v->next; cv != 0; ++dim, cv = cv->next )
		;

	if ( dim == 0 && ! ( a->flags & VARIABLE_SIZED ) )
	{
		eprint( FATAL, "%s:%ld: Missing array dimensions.\n", Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
	}

	/* If the referenced array has the NEW_VARIABLE flag still set it means
	   either that its really completely new or that it is dynamically sized
	   and the missing dimension could not be determined yet. In the first
	   case we try to set its sizes and allocate memory for it via calling
	   vars_setup_new_array().

	   If the completely new array is a normal fixed size array this will
	   return a variable on the stack pointing to the very first element of
	   the array which can be used in the following to initialize the new
	   array. If, on the other hand, vars_setup_new_array() just returns a
	   NULL array pointer which means it's an dynamically sized array and
	   makes sure we can't assign data to it. We now pop the array pointer
	   and return the variable pointer we got from vars_setup_new_array().

	   `New' dynamically sized arrays have already been set up as far as
	   possible by an earlier call to vars_setup_new_array() (otherwise the
	   VARIABLE_SIZED flag couldn't be set). We now expect an assignment of an
	   array slice which will finally determine the still missing size. The
	   function doing this assignment will have to do all things normally done
	   here, i.e. after determining the size of the last dimension the
	   allocation memory and the calculation of the location for storing the
	   data, all by itself. Thus we simply leave the pointer to the array on
	   the stack untouched and return.  */

	if ( a->flags & NEW_VARIABLE )
	{
		if ( ! ( a->flags & VARIABLE_SIZED ) )  /* in VARIABLES section only */
		{
			ret = vars_setup_new_array( a, dim, v->next );
			vars_pop( v );
			return( ret );
		}
		else
			return( v );
	}

	/* push a pointer to the indexed element onto the stack */

	return( vars_get_element_pointer( a, v, dim ) );
}


Var *vars_get_element_pointer( Var *a, Var *v, int dim )
{
	Var *ret;
	long index;


	/* check that there are enough indices on the stack (since variable sized
       arrays allow assignment of array slices there needs to be only one
       index less than the dimension of the array) */

	if ( ( ! ( a->flags & VARIABLE_SIZED ) && dim < a->dim ) ||
		 ( a->flags & VARIABLE_SIZED && dim < a->dim - 1 ) )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %d-dimensional but only "
				"indices are given.\n", Fname, Lc, a->name, a->dim, dim );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %d-dimensional but more "
				"indices are given.\n", Fname, Lc, a->name, a->dim );
		THROW( VARIABLES_EXCEPTION );
	}

	/* calculate the pointer to the indexed array element and push it onto
	   the stack */

	index = vars_calc_index( a, v->next );

	/* pop the variable with the array pointer */

	vars_pop( v );

	/* push a pointer to the indexed element onto the stack */

	if ( a->type == INT_ARR )
		ret = vars_push( INT_PTR, a->val.lpnt + index, a );
	else
		ret = vars_push( FLOAT_PTR, a->val.dpnt + index, a );

	if ( a->flags & VARIABLE_SIZED && dim == a->dim - 1 )
		ret->flags |= NEED_ARRAY_SLICE;

	return( ret );
}


long vars_calc_index( Var *a, Var *v )
{
	Var *vn;
	int i, cur;
	long index;


	for ( i = 0, index = 0; v != NULL; ++i, v = vn )
	{
		/* check the variable with the size */

		vars_check2( v, INT_VAR | FLOAT_VAR );

		/* get current size and warn if the index is a float variable */

		if ( v->type == INT_VAR )
			cur = v->val.lval - ARRAY_OFFSET;
		else
		{
			eprint( WARN, "%s:%ld: WARNING: Float variable used as index #%d "
					"for array `%s'.\n", Fname, Lc, i + 1, a->name );
			cur = ( int ) v->val.dval - ARRAY_OFFSET;
		}

		/* check that the index is not too small or too large */

		if ( cur < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
					"array `%s', minimum is %d.\n",
					Fname, Lc, i + 1, cur + ARRAY_OFFSET, ARRAY_OFFSET );
		THROW( VARIABLES_EXCEPTION );
		}

		if ( cur >= a->sizes[ i ] )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
					"array `%s', maximum is %d.\n",
					Fname, Lc, i + 1, cur + ARRAY_OFFSET,
					a->sizes[ i ] - 1 + ARRAY_OFFSET );
			THROW( VARIABLES_EXCEPTION );
		}

		/* update the index */

		index += index * a->sizes[ i ] + cur;

		/* pop the variable with the index */

		vn = v->next;
		vars_pop( v );
	}

	return( index );
}


Var *vars_setup_new_array( Var *a, int dim, Var *v )
{
	int i, cur;
	Var *vn;


	/* set array's dimension and allocate memory for their sizes */

	a->dim = dim;
	a->sizes = T_malloc( dim * sizeof( int ) );

	/* run through the variables with the sizes */

	for ( i = 0; v != NULL; ++i, v = vn )
	{
		/* check the variable with the size */

		vars_check2( v, INT_VAR | FLOAT_VAR );

		/* check the value of variable with the size and set the corresponding
		   entry in the array's field for sizes */

		if ( v->type == INT_ARR )
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

				a->flags |= VARIABLE_SIZED;
				if ( a->type == INT_ARR )
					return( vars_push( INT_PTR, NULL ) );
				else
					return( vars_push( INT_PTR, NULL ) );
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

		/* pop the variable with the size */

		vn = v->next;
		vars_pop( v );
	}

	/* calculate number of elements and allocate memory */

	for ( a->len = 1, i = 0; i < a->dim; ++i )
		a->len *= a->sizes[ i ];
	
	if ( a->type == INT_VAR )
		a->val.lpnt = ( long * ) T_calloc( ( size_t ) a->len, sizeof( long ) );
	else
		a->val.dpnt = ( double * ) T_calloc( ( size_t ) a->len,
											 sizeof( double ) );

	a->flags &= ~NEW_VARIABLE;

	if ( a->type == INT_VAR )	
		return( vars_push( INT_PTR, a->val.lpnt, a ) );
	else
		return( vars_push( FLOAT_PTR, a->val.dpnt, a ) );
}


Var *vars_arr_get( Var *v )
{
	int dim, i;
	Var *a, *cv, *ret;
	long index;


	/* check that variable is an array pointer */

	vars_check2( v, ARR_PTR );
	a = v->val.vptr;

	/* check that array already has data assigned to it */

	if ( a->flags & NEW_VARIABLE )
	{
		assert( a->flags & VARIABLE_SIZED );

		eprint( FATAL, "%s:%ld: The array `%s' is dynamically siezd and has "
				"not been assigned data yet.\n", Fname, Lc, a->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* count the variable below v on the stack */

	for ( dim = 0, cv = v->next; cv != 0; ++dim, cv = cv->next )
		;

	if ( dim == 0 & ! ( a->flags & VARIABLE_SIZED ) )
	{
		eprint( FATAL, "%s:%ld: Missing array dimensions.\n", Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
	}

	
	/* check that for fixed size arrays there are as many indices on the
	   stack as the array has dimensions, for dynamically sized arrays we
	   also allow acess to array slices */

	if ( ( ! ( a->flags & VARIABLE_SIZED ) && dim < a->dim ) ||
		 ( a->flags & VARIABLE_SIZED && dim < a->dim - 1 ) )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %d-dimensional but only "
				"indices are given.\n", Fname, Lc, a->name, a->dim, dim );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Array `%s' is %d-dimensional but more "
				"indices are given.\n", Fname, Lc, a->name, a->dim );
		THROW( VARIABLES_EXCEPTION );
	}

	/* calculate the pointer to the indexed array element and push it onto
	   the stack */

	index = vars_calc_index( a, v->next );

	/* pop the array pointer variable and return a variable with the value
	   from the accessed element */

	vars_pop( v );

	if ( a->flags & VARIABLE_SIZED && dim == a->dim - 1 )
	{
		if ( a->type == INT_ARR )
			ret = vars_push( INT_PTR, a->val.lpnt + index, a );
		else
			ret = vars_push( FLOAT_PTR, a->val.dpnt + index, a );
	
		ret->flags = IS_ARRAY_SLICE;
		return( ret );
	}
	
	if ( a->type == INT_ARR )
		return( vars_push( INT_VAR, *( a->val.lpnt + index ) ) );
	else
		return( vars_push( FLOAT_VAR, *( a->val.dpnt + index ) ) );

	assert( 1 == 0 );
}


void vars_assign( Var *src, Var *dest )
{
	/* <PARANOID> check that both variables really exist </PARANOID> */

	assert( vars_exist( src ) && vars_exist( dest ) );

	/* 1. both variables are simple variables (*/

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) &&
		 src->type & ( INT_VAR | FLOAT_VAR ) )
	{
		vars_ass_var_to_var( src, dest );
		return;
	}

	/* 2. dest is simple variable, src is an array slice */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) &&
		 src->type & ( INT_PTR | FLOAT_PTR ) )
	{
		vars_ass_slice_to_var( src, dest );
		return;
	}

	/* 3. dest is an */

	if ( dest->type == ARR_PTR &&
		 src->type & ( INT_VAR | FLOAT_VAR ) )
	{
		

}


void vars_ass_var_to_var( Var *src, Var *dest )
{
	Var null;


	if ( src->flags & NEW_VARIABLE )
	{
		eprint( WARN, "%s:%ld: WARNING: Variable used in assignment that "
				"never has been assigned a value.\n", Fname, Lc );
		vars_pop( src );
		null.type = INT_VAR;
		null.val.lval = 0;
		src = &null;
	}

	if ( dest->type == INT_VAR )
	{
		if ( src->type == INT_VAR )
			dest->val.lval = src->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: WARNING: Assigning to integer "
					"variable from float value.\n", Fname, Lc );
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
	
	dest->flags &= ~NEW_VARIABLE;
	vars_pop( src );
}


void vars_ass_arrelem_to_var( Var *src, Var *dest )
{
	assert( src->flags & IS_ARRAY_SLICE );

	eprint( FATAL, "%s:%ld: Array slice can't be assigned to a variable.\n",
			Fname, Lc );
	THROW( VARIABLES_EXCEPTION );
}





		case INT_PTR :

		if ( dest_type & ( INT_VAR | FLOAT_VAR ) )
		{












	if ( src->type & INT_VAR | FLOAT_VAR )
	{
		if ( src->flags & NEW_VARIABLE )
			eprint( WARN, "%s:%ld: WARNING: Variable used in assignment that "
					"never has been assigned a value.\n", Fname, Lc );

		if ( dest->type & ( INT_VAR | FLOAT_VAR ) ||
			 dest->flags & NEW_VARIABLE )
		{
			if ( src->flags & NEW_VARIABLE )
				if ( dest_type & INT_VAR || )
					dest->val.



	vars_check( src, INT_PTR | FLOAT_PTR );

	

	if ( src->flags & IS_ARRAY_SLICE )
	{
		switch ( dest->type )
		{
			case INT_PTR : case FLOAT_PTR :
				if ( ! ( dest->flags & NEED_ARRAY_SLICE ) )
			

