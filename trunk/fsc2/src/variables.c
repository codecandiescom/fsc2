/* 
   $Id$

   $Log$
   Revision 1.7  1999/07/16 19:31:39  jens
   *** empty log message ***

*/

#include "fsc2.h"


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

	vp = ( Var * ) T_malloc(   sizeof( Var )
							 + ( strlen( name ) + 1 ) * sizeof( char ) );

	vp->name = ( char * ) ( vp + 1 );

	/* Set relevant entries in the new structure and make it the very first
	   element in the list of variables */

	strcpy( vp->name, name );    /* set its name */
	vp->new_flag = SET;          /* set flag to indicate it's new */
	vp->type = UNDEF_VAR;        /* set type to still undefined */

	vp->next = var_list;         /* set pointer to it's successor */
    var_list = vp;               /* make it the head of the list */

	return( vp );                /* return pointer to the structure */
}


/*---------------------------------------------------------------------------*/
/* vars_new_assign() assigns a value to a new variable - if the variable     */
/* pointer with the value is NULL use integer type and a value of zero as    */
/* default.                                                                  */
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

	if ( src != ( Var * ) NULL && ! src->new_flag )
	{
		/* make sure `src' has integer or float type */

		assert( src->type == INT_VAR || src->type == FLOAT_VAR );

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

		dest->new_flag = UNSET;
	}
	else
	{
		/* warn (and don't use it) if `src' exists but has never been assigned
		   a value, instead assign `dest' a value of zero */

		if ( src != NULL )
			vars_warn_new( src );

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
	int ires;
	double dres;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1 );
	vars_check( v2 );

	/* now add the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
		{
			ires = v1->val.lval + v2->val.lval;
			new_var = vars_push( INT_VAR, ( void * ) &ires );
		}
		else
		{
			dres = ( double ) v1->val.lval + v2->val.dval;
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = v1->val.dval + ( double ) v2->val.lval;
		else
			dres = v1->val.dval + v2->val.dval;

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
	int ires;
	double dres;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1 );
	vars_check( v2 );

	/* now subtract the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
		{
			ires = v1->val.lval - v2->val.lval;
			new_var = vars_push( INT_VAR, ( void * ) &ires );
		}
		else
		{
			dres = ( double ) v1->val.lval - v2->val.dval;
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = v1->val.dval - ( double ) v2->val.lval;
		else
			dres = v1->val.dval - v2->val.dval;

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
	int ires;
	double dres;


	/* make sure that `v1' and `v2' exist, are integers or float values 
	   and have an value assigned to it */

	vars_check( v1 );
	vars_check( v2 );

	/* now multiply the values while taking care to get the type right */

	if ( v1->type == INT_VAR )
	{
		if ( v2->type == INT_VAR )
		{
			ires = v1->val.lval * v2->val.lval;
			new_var = vars_push( INT_VAR, ( void * ) &ires );
		}
		else
		{
			dres = ( double ) v1->val.lval * v2->val.dval;
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = v1->val.dval * ( double ) v2->val.lval;
		else
			dres = v1->val.dval * v2->val.dval;

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
	int ires;
	double dres;


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
		{
			ires = v1->val.lval / v2->val.lval;
			new_var = vars_push( INT_VAR, ( void * ) &ires );
		}
		else
		{
			dres = ( double ) v1->val.lval / v2->val.dval;
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = v1->val.dval / ( double ) v2->val.lval;
		else
			dres = v1->val.dval / v2->val.dval;

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
	int ires;
	double dres;


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
		{
			ires = v1->val.lval % v2->val.lval;
			new_var = vars_push( INT_VAR, ( void * ) &ires );
		}
		else
		{
			dres = fmod( ( double ) v1->val.lval, v2->val.dval );
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = fmod( v1->val.dval, ( double ) v2->val.lval );
		else
			dres = fmod( v1->val.dval, v2->val.dval );

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
	int ires;
	double dres;


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
		if ( v2->type == INT_VAR )
		{
			if ( v2->val.lval >= 0 )
			{
				ires = ( long ) pow( v1->val.lval, v2->val.lval );
				new_var = vars_push( INT_VAR, ( void * ) &ires );
			}
			else
			{
				dres = pow( v1->val.lval, v2->val.lval );
				new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
			}
		}
		else
		{
			dres = pow( ( double ) v1->val.lval, v2->val.dval );
			new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
		}
	}
	else
	{
		if ( v2->type == INT_VAR )
			dres = pow( v1->val.dval, ( double ) v2->val.lval );
		else
			dres = pow( v1->val.dval, v2->val.dval );

		new_var = vars_push( FLOAT_VAR, ( void * ) &dres );
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
/*    * type of comparision (EQUAL, LESS or LESS_EQUAL)                     */
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
		case EQUAL :
			res = (    ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    == ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, ( void * ) &res );
			break;

		case LESS :
			res = (   ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    < ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, ( void * ) &res );
			break;

		case LESS_EQUAL :
			res = (    ( v1->type == INT_VAR ? v1->val.lval : v1->val.dval )
				    <= ( v2->type == INT_VAR ? v2->val.lval : v2->val.dval ) )
				  ? 1 : 0;
			new_var = vars_push( INT_VAR, ( void * ) &res );
			break;
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return( new_var );
}


/*--------------------------------------------------------------------------*/
/* vars_push_simple() creates a new entry on the variable stack for an      */
/* already existing variable (this is checked) and sets its type and value. */
/*--------------------------------------------------------------------------*/

Var *vars_push_simple( Var *v )
{
	/* Make sure it's really a simple variable */

	if ( v->type == INT_ARR || v->type == FLOAT_ARR )
	{
		eprint( FATAL, "%s:%ld: `%s' is an array and can't be used in this "
				"context.\n", Fname, Lc, v->name );
		THROW( VARIABLES_EXCEPTION );
	}

	/* check that it exists etc. */

	vars_check( v );

	/* push a transient variable onto the stack with the relevant data set */

	return( vars_push( v->type,
					   v->type == INT_VAR ?
					   ( void * ) &v->val.lval : ( void * ) &v->val.dval ) );
}


/*-----------------------------------------------------------------------*/
/* vars_push() creates a new entry on the variable stack (which actually */
/* is not really a stack but a linked list) and sets its type and value. */
/*-----------------------------------------------------------------------*/

Var *vars_push( int type, void *data )
{
	Var *new_stack_var, *stack;


	/* get memory for the new variable to be apppended to the stack */

	new_stack_var = ( Var * ) T_malloc( sizeof( Var ) );

	/* set its type and clear the `new__flag' */

	new_stack_var->type = type;
	new_stack_var->new_flag = UNSET;

	/* set its value and the `next' entry to NULL */

	if ( type == INT_VAR )
		new_stack_var->val.lval = *( ( int * ) data );
	if ( type == FLOAT_VAR )
		new_stack_var->val.dval = *( ( double * ) data );

	new_stack_var->name = NULL;
	new_stack_var->next = NULL;

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


/*-------------------------------------------------------------------*/
/* vars_pop() checks if a variable belongs to the variable stack and */
/* if it does removes it from the linked list making up the stack    */
/*-------------------------------------------------------------------*/

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

		if ( stack->type == INT_ARR && stack->val.lpnt != NULL )
			free( stack->val.lpnt );
		if ( stack->type == FLOAT_ARR && stack->val.dpnt != NULL )
			free( stack->val.dpnt );
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


/*-----------------------------------------------------------*/
/* vars_arr_start() starts the declaration of an array       */
/* ->                                                        */
/*    * pointer to the new variable to be used for the array */
/*-----------------------------------------------------------*/

void vars_arr_start( Var *a )
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

void vars_arr_extend( Var *a, Var *s )
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
		a->sizes = ( long * ) T_malloc( sizeof( long ) );
	else
		a->sizes = ( long * ) T_realloc( ( void * ) a->sizes,
										 ( a->dim + 1 )* sizeof( long ) );
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

	if ( ! a->new_flag && cur_elem == a->len )
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

	if ( a->new_flag )
	{
		for ( a->len = 1, i = 0; i < a->dim; ++i )
			a->len *= a->sizes[ i ];
		
		if ( a->type == INT_VAR )
			a->val.lpnt = ( long * ) T_calloc( a->len, sizeof( long ) );
		else
			a->val.dpnt = ( double * ) T_calloc( a->len, sizeof( double ) );

		a->new_flag = UNSET;
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

	tmp = ( AStack * ) T_malloc( sizeof( AStack ) );

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
	Arr_Stack->entries = ( long * ) T_malloc( v->dim * sizeof( long ) );

	vars_pop( v );
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
		ret = vars_push( INT_VAR, &Arr_Stack->var->val.lpnt[ index ] );
	else
		ret = vars_push( FLOAT_VAR, &Arr_Stack->var->val.dpnt[ index ] );

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

Var *vars_arr_assign( Var *a, Var *v )
{
	long i, index, len;
	Var *ret;


	/* allocate memory if the array has its `new_flag' still set */

	if ( Arr_Stack->var->new_flag )
	{
		for ( len = Arr_Stack->var->sizes[ 0 ], i = 1;
			  i < Arr_Stack->var->dim; ++i )
			len *= Arr_Stack->var->sizes[ i ];

		if ( Arr_Stack->var->type == INT_ARR )
			Arr_Stack->var->val.lpnt =
				( long * ) T_malloc( len * sizeof( long ) );
		else
			Arr_Stack->var->val.dpnt =
				( double * ) T_malloc( len * sizeof( double ) );
		Arr_Stack->var->len = len;
		Arr_Stack->var->new_flag = UNSET;
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

	return( ret );
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


/*----------------------------------------------------------*/
/* vars_clean_up() deletes the variable and array stack and */
/* removes all variables from the list of variables         */
/*----------------------------------------------------------*/

void vars_clean_up( void )
{
	vars_del_stack( );
	vars_del_astack( );
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


/*-----------------------------------------------------------------------*/
/* vars_check() checks that a variable exists, has integer or float type */
/* and warns if it hasn't been assigned a value                          */
/*-----------------------------------------------------------------------*/

void vars_check( Var *v )
{
	if ( v->type == UNDEF_VAR )
	{
		if ( v->name != NULL )
			eprint( FATAL, "%s:%ld: There is no variable named `%s'.\n",
					Fname, Lc, v->name );
		else
			eprint( FATAL, "%s:%ld: INTERNAL ERROR: Transient variable does "
					"not exist.\n", Fname, Lc );
		THROW( VARIABLES_EXCEPTION );
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
 	if ( v->new_flag )
	{
		if ( v->name != NULL )
			eprint( WARN, "%s:%ld: WARNING: Variable `%s' has never been "
					"assigned a value.\n", Fname, Lc, v->name );
		else
			eprint( WARN, "%s:%ld: INTERNAL WARNING: Transient variable has "
					"`new_flag' set.\n", Fname, Lc );
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

	dest->new_flag = UNSET;

	/* pop source variable from variables stack and return the new variable */

	vars_pop( src );
	return( dest );
}
