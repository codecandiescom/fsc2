/* 
   $Id$
*/


#include "fsc2.h"


/* some typedefs needed due to the limitations of va_arg() (also see the
   C-FAQ 15.11 on this point): FnctPtr is a pointer to a Var pointer returning
   function with a Var pointer as argument. */

typedef Var *VarretFnct( Var * );
typedef VarretFnct *FnctPtr;


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
	int    dim;              // dimension of array
	int    *sizes;           // array of sizes of dimensions
	long   len;              // total len of array
	long   flags;
	struct Var_ *from;
	struct Var_ *next;       // next variable in list or stack
	struct Var_ *prev;       // previous variable in list or stack
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
  long and double type) or an integer or float array (INT_ARR or FLOAT_ARR) of
  as many dimensions as the user thinks it should have. Since at the moment of
  creation of a variable its type can not be determined there is also a
  further type, UNDEF_VAR which is assigned to the variable until it is
  possible to find out if the variable is just a simple variable or an array.

  What kind of type a variable has, i.e. integer or float, is controlled via
  the function IF_FUNC(), defined as macro in variables.h, which gets the
  passed the variable's name - if the function returns TRUE the variable is an
  integer (or the array is an integer array) otherwise its type is FLOAT. So,
  changing IF_FUNC() and recompiling will change the behaviour of the program
  in this respect. Currently, as agreed with Axel and Thomas, IF_FUNC returns
  TRUE for variables starting with a capital letters, thus making the variable
  an integer. But this is easily changed...

  Now, when the input file is read in, lines like

           A = B + 3.25;

  are found. While this form is convenient for to read a human, a reverse
  polish notation (RPN) for the right hand side of the assignment of the form

           A B 3.25 + =

  is much easier to handle for a computer. For this reason there exists the
  variable stack (which actually isn't a stack but a linked list).

  So, if the lexer finds an identifier like `A', it first tries to get a
  pointer to the variable named `A' in the variables list by calling
  vars_get(). If this fails (and we're just parsing the VARIABLES section of
  the EDL file, otherwise it would utter an error message) a new variable is
  created instead by a call to vars_new(). The resulting pointer is passed
  to the parser.

  Now, the parser sees the `=' bit of text and realizes it has to do an
  assignment and thus branches to evaluate the right hand side of the
  expression. In this branch, the parser sees the `B' and pushes a copy of the
  variable `B' onto the variables stack, containing just the necessary
  information, i.e. its type and value. It then finds the `+' and branches
  recursively to evaluate the expression to the right of the `+'. Here, the
  parser sees just the numerical value `3.25' and pushes it onto the variables
  stack, thus creating another transient stack variable with the value 3.25
  (and type FLOAT_VAL). Now the copy of `B' and the variable with the value of
  3.25 are on the variables stack and the parser can continue by adding the
  values of both these variables. It does so by calling vars_add(), creating
  another transient stack variable for the result and removing both the
  variables used as arguments. It finally returns to the state it started
  from, the place where it found the `A =' bit, with a stack variable holding
  the result of the evaluation of the right hand side. All left to be done now
  is to call vars_assign() which assigns the value of the stack variable to
  `A'. The stack variable with the right hand side result is than removed from
  the stack. If we're a bit paranoid we can make sure everything worked out
  fine by checking that the variabe stack is now empty. Quite simple, isn't
  it?

  What looks like a lot of complicated work to do something rather simple
  has the advantage that, due to its recursive nature, it can be used
  without any changes for much more complicated situations. Instead of the
  value 3.25 we could have a complicated expression to add to `B', which
  will be handled automagically by a deeper recursion, thus breaking up the
  complicated task into small, simple tasks, that can be handled easily.
  Also, `B' could be a more complicated expression instead which would be
  handled in the same way.

  Now, what about arrays? If the lexer finds an identifier (it doesn't know
  about the difference between variables and arrays) it searches the variables
  list and if it doesn't find an entry with the same name it creates a new one
  (again, as long as we're in the VARIABLES section where defining new
  variables and array is allowed). The parser in turn realizes that the user
  wants an array when it finds a string of tokens of the form

            variable_identifier [ 

  where `variable_identifier' is a array name. It calls vars_arr_start()
  where, if the array is still completetly new, the type of the array is set
  to INT_ARR or FLOAT_ARR (depending on the result of the macro IF_FUNC(), see
  above). Finally, it pushes a transient variable onto the stack of type
  ARR_PTR with the `from' element in the variable structure pointing to the
  original array. This transient variable serves as a kind of marker since
  next the parser is going to read all indices and push them onto the stack.

  The next tokens have to be numbers - either simple numbers or computed
  numbers (i.e. results of function calls or elements of arrays). These are
  the indices of the array. We've reached the end of the list of indices when
  the the closing bracket `]' is found in the input. Now the stack looks like
  this:

               last index ->    number      <- top of stack
                                number
                                number
               first index ->   number
                                ARR_PTR     <- bottom of stack

  i.e. on the top we've the indices (in reverse order) followed by the pointer
  to the array.  The next step depends if this is an access to an array
  element (i.e. it's found on the right hand side of an assignment) or if an
  array element is to be set (i.e. its on thre left hand side). In the first
  case the function vars_arr_rhs() is called.

  Basically, what vars_arr_rhs() does is to take the indices and the pointer
  to the array from the stake, determine the value of the accessed array
  element and push its value as a variable onto the stack.

  If, on the other hand, the array is found on the left hand side of an
  assignment, vars_arr_lhs() is called. Again, from the indices the element of
  the array to be set is calculated, but, since the the right and side of the
  assignment is not known yet, again an ARR_PTR transient variable is pushed
  onto the stack with the generic pointer `gptr' in the union in the variables
  structure pointing to the accessed element and the `from' field pointing to
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
  are interpreted as sizes for the dimesions of the array and memory is
  allocated for the elements of the array. It returns a ARR_PTR to the array
  with the NEED_INIT flag in the `flags' element of the variables structure
  (of the transient variable) is set, a list of data (in curly brackets,
  i.e. `{' and `}' following as an assignment will be interpreted as data
  for initializing the array.

  But beside these fixed sized arrays there are also variable sized arrays.
  These are needed e.g. for storing data sets received from the transient
  recorder where it is sometimes impossible to know the length of the data set
  in advance. Only the very last dimension of an array may be variable sized
  and making it variable sized is indicated by writing a star (`*') as the
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


	/* try to find the variable with the name passed to the function */

	for ( ptr = var_list; ptr != NULL; ptr = ptr->next )
		if ( ! strcmp( ptr->name, name ) )
			return ptr;

	return NULL;
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
	vp->name = NULL;
	vp->sizes = NULL;
	vp->name = get_string_copy( name );

	/* Set relevant entries in the new structure and make it the very first
	   element in the list of variables */

	vp->flags = NEW_VARIABLE;    /* set flag to indicate it's new */
	vp->type = UNDEF_VAR;        /* set type to still undefined */

	vp->next = var_list;         /* set pointer to it's successor */
	if ( var_list != NULL )      /* set previous pointer in successor */
		var_list->prev = vp;     /* (if this isn't the very first) */ 
    var_list = vp;               /* make it the head of the list */

	return vp;                   /* return pointer to the structure */
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

	return new_var;
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

	return new_var;
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

	return new_var;
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
		THROW( EXCEPTION );
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

	return new_var;
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
		THROW( EXCEPTION );
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

	return new_var;
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
	Var  *new_var;
	long ires,
		 i;


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
		eprint( FATAL, "%s:%ld: Negative base while exponent not an integer "
				"value.\n", Fname, Lc );
		THROW( EXCEPTION );
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
					  i < labs( v2->val.lval ); i++ )
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

	return new_var;
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

	return v;
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


	/* make sure that `v1' and `v2' exist, are integers or float values
	   and have an value assigned to it */

	vars_check( v1, INT_VAR | FLOAT_VAR );
	vars_check( v2, INT_VAR | FLOAT_VAR );

	switch ( comp_type )
	{
		case COMP_EQUAL :
			new_var = vars_push( INT_VAR, VALUE( v1 ) == VALUE( v2 ) );
			break;

		case COMP_LESS :
			new_var = vars_push( INT_VAR, VALUE( v1 ) < VALUE( v2 ) );
			break;

		case COMP_LESS_EQUAL :
			new_var = vars_push( INT_VAR, VALUE( v1 ) <= VALUE( v2 ) );
			break;

		default:               /* this should never happen... */
			assert( 1 == 0 );
	}

	/* pop the variables from the stack */

	vars_pop( v1 );
	vars_pop( v2 );

	return new_var;
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
			/* understanding about the function pointer seems to be too
			   complicated for va_arg() when written directly thus `FnctPtr'
			   is a typedef (see start of file) */
			new_stack_var->val.fnct = va_arg( ap, FnctPtr );
			break;

		case ARR_PTR :
			new_stack_var->val.gptr = va_arg( ap, void * );
			new_stack_var->from = va_arg( ap, Var * );
			break;

		case INT_TRANS_ARR :
			new_stack_var->val.lpnt = va_arg( ap, long * );
			new_stack_var->len = va_arg( ap, long );
			new_stack_var->val.lpnt =
				get_memcpy( new_stack_var->val.lpnt,
							new_stack_var->len * sizeof( long ) );
			break;

		case FLOAT_TRANS_ARR :
			new_stack_var->val.dpnt = va_arg( ap, double * );
			new_stack_var->len = va_arg( ap, long );
			new_stack_var->val.dpnt =
				get_memcpy( new_stack_var->val.dpnt,
							new_stack_var->len * sizeof( double ) );
			break;

		case ARR_REF :
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

	return new_stack_var;
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

		switch( stack->type )
		{
			case STR_VAR :
				T_free( stack->val.sptr );
				break;

			case FUNC :
				T_free( stack->name );
				break;

			case INT_TRANS_ARR :
				T_free( stack->val.lpnt );
				break;

			case FLOAT_TRANS_ARR :
				T_free( stack->val.dpnt );
				break;
		}

		T_free( stack );
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
		if ( var_list->name != NULL )
			T_free( var_list->name );

		switch( var_list->type )
		{
			case INT_ARR :
				if ( var_list->sizes != NULL )
					T_free( var_list->sizes );
				if ( ! ( var_list->flags & NEED_ALLOC ) &&
					 var_list->val.lpnt != NULL )
					T_free( var_list->val.lpnt );
				break;

			case FLOAT_ARR :
				if ( var_list->sizes != NULL )
					T_free( var_list->sizes );
				if ( ! ( var_list->flags & NEED_ALLOC ) &&
					 var_list->val.dpnt != NULL )
					T_free( var_list->val.dpnt );
				break;

			case STR_VAR :
				if ( var_list->val.sptr != NULL )
					T_free( var_list->val.sptr );
				break;
		}

		vp = var_list->next;
		T_free( var_list );
		var_list = vp;
	}
}


void vars_check( Var *v, int type )
{
	int i;
	int t = v->type;
	const char *types[ ] = { "INTEGER", "FLOAT", "STRING", "INTEGER ARRAY",
							 "FLOAT ARRAY", "FUNCTION", "ARRAY POINTER",
							 "INTEGER ARRAY SLICE", "FLOAT ARRAY SLICE",
	                         "ARRAY REFERENCE" };
	
	/* being paranoid we first check that the variable exists at all -
	   probably this can be left out later. */

	assert( vars_exist( v ) );

	/* check that the variable has a value assigned to it */

	if ( v->type == UNDEF_VAR )
	{
		if ( v->name != NULL )
		{
			eprint( FATAL, "%s:%ld: The accessed variable `%s' has not been "
					"assigned a value.\n", Fname, Lc, v->name );
			THROW( EXCEPTION );
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
		i = 1;
		t = v->type;
		while ( ! ( ( t >>= 1 ) & 1 ) )
			i++;
		eprint( FATAL, "%s:%ld: Variable of type %s cannot be used in this "
				"context.\n", Fname, Lc, types[ i ] );
		THROW( EXCEPTION );
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
			eprint( WARN, "%s:%ld: WARNING: Variable `%s' has not been "
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


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

bool vars_exist( Var *v )
{
	Var *lp;


	for ( lp = var_list; lp != NULL; lp = lp->next )
		if ( lp == v )
			return OK;

	for ( lp = Var_Stack; lp != NULL; lp = lp->next )
		if ( lp == v )
			return OK;

	eprint( FATAL, "fsc2: INTERNAL ERROR: Use of non-existing "
			"variable detected at %s:%d.\n", __FILE__, __LINE__ );
	exit( EXIT_FAILURE );
}



/*-------------------------------------------------------------------*/
/* This function is called when a `VAR_TOKEN [' combination is found */
/* in the input. For a new array it sets its type. Everything else   */
/* it does is pushing a variable with a pointer to the array onto    */
/* the stack.                                                        */
/*-------------------------------------------------------------------*/


Var *vars_arr_start( Var *v )
{
	Var *ret;

	/* test if variable exists */

	assert( vars_exist( v ) );

	/* Check if the array is completely new (type is still UNDEF_VAR). In this
	   case set its type and zero the pointer to the data so we know no memory
	   has been allocated yet. Otherwise check if its really an array. */

	if ( v->type == UNDEF_VAR )
	{
		v->type = IF_FUNC( v->name ) ? INT_ARR : FLOAT_ARR;
		if ( v->type == INT_ARR )
			v->val.lpnt = NULL;
		else
			v->val.dpnt = NULL;
	}
	else
		vars_check( v, INT_ARR | FLOAT_ARR );

	/* push variable with generic pointer to array onto the stack */

	ret = vars_push( ARR_PTR, NULL, v );
	return ret;
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
	Var *a,
		*cv;


	while ( v->type != ARR_PTR )
		v = v->prev;

	a = v->from;

	/* count the variable below v on the stack */

	for ( dim = 0, cv = v->next; cv != 0; cv = cv->next )
		if ( cv->type != UNDEF_VAR )
			dim++;

	/* if the array is new we need to set it up */

	if ( a->flags & NEW_VARIABLE )
		return vars_setup_new_array( a, dim, v );

	/* push a pointer to the indexed element onto the stack */

	return vars_get_lhs_pointer( a, v, dim );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *vars_get_lhs_pointer( Var *a, Var *v, int dim )
{
	Var  *ret;
	long index;


	/* check that there are enough indices on the stack (since variable sized
       arrays allow assignment of array slices there needs to be only one
       index less than the dimension of the array) */

	if ( dim < a->dim - 1 )
	{
		eprint( FATAL, "%s:%ld: Not enough indices found for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Too many indices found for %d-dimensional "
				"array `%s'.\n", Fname, Lc, a->dim, a->name );
		THROW( EXCEPTION );
	}

	/* for arrays that still are variable sized we need assignment of a
       complete slice in order to determine the missing size of the last
       dimension */

	if ( a->flags & NEED_ALLOC && dim != a->dim - 1 )
	{
		if ( a->dim != 1 )
			eprint( FATAL, "%s:%ld: Size of array `%s' is still unknown, "
					"only %d ind%s allowed here.\n",
					Fname, Lc, a->name, a->dim - 1,
					( a->dim == 2 ) ? "ex is" : "ices are" );
		else
			eprint( FATAL, "%s:%ld: Size of array `%s' is still unknown, "
					"no indices are allowed here.\n", Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* calculate the pointer to the indexed array element (or slice) */

	index = vars_calc_index( a, v->next );

	/* pop the variable with the array pointer */

	vars_pop( v );

	/* If the array is still variable sized and thus needs memory allocated we
	 push a pointer to the array onto the stack and store the indexed slice in
	 the variables structure `len' element. Otherwise we push a variable onto
	 the stack with a pointer to the indexed element or slice.*/

	if ( a->flags & NEED_ALLOC )
	{
		ret = vars_push( ARR_PTR, NULL, a );
		ret->len = index;
	}
	else
	{
		if ( a->type == INT_ARR )
			ret = vars_push( ARR_PTR, a->val.lpnt + index , a );
		else
			ret = vars_push( ARR_PTR, a->val.dpnt + index , a );
	}

	/* Set a flag if a slice is indexed */

	if ( dim == a->dim - 1 )
		ret->flags |= NEED_SLICE;

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

long vars_calc_index( Var *a, Var *v )
{
	Var  *vn;
	int  i, cur;
	long index;



	/* Run through all the indices on the variable stack */

	for ( i = 0, index = 0; v != NULL; i++, v = vn )
	{
		/* We can't use still undefined variables as index...*/

		if ( v->type == UNDEF_VAR )
		{
			vn = v->next;
			vars_pop( v );
			break;
		}

		/* Check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Get current index and warn if it's a float variable */

		if ( v->type == INT_VAR )
			cur = v->val.lval - ARRAY_OFFSET;
		else
		{
			eprint( WARN, "%s:%ld: WARNING: Float variable used as index #%d "
					"for array `%s'.\n", Fname, Lc, i + 1, a->name );
			cur = ( int ) v->val.dval - ARRAY_OFFSET;
		}

		/* Check that the index a number not a `*' */

		if ( cur == - ARRAY_OFFSET && v->flags & VARIABLE_SIZED )
		{
			eprint( FATAL, "%s:%ld: A `*' as index is only allowed in the "
					"declaration of an array, not in an assignment.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}

		/* Check that the index is not too small or too large */

		if ( cur < 0 )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
			"array `%s', minimum is %d.\n", Fname, Lc, i + 1,
					cur + ARRAY_OFFSET, a->name, ARRAY_OFFSET );
			THROW( EXCEPTION );
		}

		if ( cur >= a->sizes[ i ] )
		{
			eprint( FATAL, "%s:%ld: Invalid array index #%d (value=%d) for "
					"array `%s', maximum is %d.\n",
					Fname, Lc, i + 1, cur + ARRAY_OFFSET, a->name,
					a->sizes[ i ] - 1 + ARRAY_OFFSET );
			THROW( EXCEPTION );
		}

		/* Update the index */

		index = index * a->sizes[ i ] + cur;

		/* Pop the variable with the index */

		vn = v->next;
		vars_pop( v );
	}

	if ( vn != NULL )
	{
		eprint( FATAL, "%s:%ld: Missing array index for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* For slices we need another update of the index */

	if ( i != a->dim )
		index = index * a->sizes[ i ];

	return index;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *vars_setup_new_array( Var *a, int dim, Var *v )
{
	int i,
		cur;
	Var *vn,
		*ret;


	if ( v->next->type == UNDEF_VAR )
	{
		eprint( FATAL, "%s:%ld: Missing indices in declaration of array "
				"`%s'.\n", Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* Set array's dimension and allocate memory for their sizes */

	a->dim = dim;
	a->sizes = NULL;
	a->sizes = T_malloc( dim * sizeof( int ) );
	a->flags &= ~NEW_VARIABLE;
	a->len = 1;

	/* Run through the variables with the sizes after popping the variable
       with the array pointer */

	vn = v->next;
	vars_pop( v );
	v = vn;

	for ( i = 0; v != NULL; i++ )
	{
		/* Check the variable with the size */

		vars_check( v, INT_VAR | FLOAT_VAR );

		/* Check the value of variable with the size and set the corresponding
		   entry in the array's field for sizes */

		if ( v->type == INT_VAR )
		{
			/* If the the very last variable with the sizes has the flag
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
					THROW( EXCEPTION );
				}

				vars_pop( v );

				a->flags &= ~NEW_VARIABLE;
				a->sizes[ i ] = 0;
				a->flags |= NEED_ALLOC;
				ret = vars_push( ARR_PTR, NULL, a );
				return ret;
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
			THROW( EXCEPTION );
		}

		a->sizes[ i ] = cur;
		a->len *= cur;

		/* Pop the variable with the size */

		vn = v->next;
		vars_pop( v );
		v = vn;
	}

	/* Allocate memory */

	if ( a->type == INT_ARR )
		a->val.lpnt = ( long * ) T_calloc( ( size_t ) a->len, sizeof( long ) );
	else
		a->val.dpnt = ( double * ) T_calloc( ( size_t ) a->len,
											 sizeof( double ) );

	a->flags &= ~NEW_VARIABLE;
	ret = vars_push( ARR_PTR, NULL, a );
	ret->flags |= NEED_INIT;

	return ret;
}


/*----------------------------------------------------------------------*/
/* Function is called if an element or a slice of an array is accessed  */
/* after the indices are read in. Prior to the call of this function    */
/* vars_arr_start() has been called and pushed a generic pointer to the */
/* array (i.e. of type ARR_PTR) */
/*----------------------------------------------------------------------*/

Var *vars_arr_rhs( Var *v )
{
	int  dim;
	Var  *a, *cv;
	long index;


	/* The variable pointer this function gets passed is a pointer to the very
       last index on the variable stack. Now we've got to work our way up in
       the stack until we find the first non-index variable which has to be a
       pointer to an array. */

	while ( v->type != ARR_PTR )
		v = v->prev;

	a = v->from;                      /* Get array the pointer refers to */

	/* If the right hand side array is still variable sizes it never has been
       assigned values to it and it makes no sense to use its elements */

	if ( a->flags & NEED_ALLOC )
	{
		eprint( FATAL, "%s:%ld: The array `%s' is dynamically sized and has "
				"not been assigned data yet.\n", Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* Count the indices on the stack */

	for ( dim = 0, cv = v->next; cv != 0; dim++, cv = cv->next )
		;

	/* Check that the number of indices is not less than the dimension of the
       array minus one - we allow slice access for the very last dimension */

	if ( dim < a->dim - 1 )
	{
		eprint( FATAL, "%s:%ld: Not enough indices found for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* Check that there are not too many indices */

	if ( dim > a->dim )
	{
		eprint( FATAL, "%s:%ld: Too many indices found for %d-dimensional "
				"array `%s'.\n", Fname, Lc, a->dim, a->name );
		THROW( EXCEPTION );
	}

	/* Calculate the position of the indexed array element (or slice) */

	index = vars_calc_index( a, v->next );

	/* Pop the array pointer variable */

	vars_pop( v );

 	/* If this is an element access return a variable with the value of the
	   accessed element, otherwise return a pointer to the first element of
	   the slice. */

	if ( dim == a->dim )
	{
		if ( a->type == INT_ARR )
			return vars_push( INT_VAR, *( a->val.lpnt + index ) );
		else
			return vars_push( FLOAT_VAR, *( a->val.dpnt + index ) );
	}
	else
	{
		if ( a->type == INT_ARR )
			return vars_push( ARR_PTR, a->val.lpnt + index, a );
		else
			return vars_push( ARR_PTR, a->val.dpnt + index, a );
	}

	assert( 1 == 0 );       /* we never should end here... */
}


/*----------------------------------------------------------------------*/
/* This is the central function for assigning new values. It allows the */
/* assignment of simple values as well has whole slices of arrays.      */
/*----------------------------------------------------------------------*/

void vars_assign( Var *src, Var *dest )
{
	/* <PARANOID> check that both variables really exist </PARANOID> */

	assert( vars_exist( src ) && vars_exist( dest ) );

	switch ( src->type )
	{
		case INT_VAR : case FLOAT_VAR :              /* variable */
			vars_ass_from_var( src, dest );
		   break;

		case ARR_PTR :                               /* array slice */
			vars_ass_from_ptr( src, dest );
			break;

		case INT_TRANS_ARR : case FLOAT_TRANS_ARR :  /* transient array */
			vars_ass_from_trans_ptr( src, dest );
			break;
/*
		case INT_ARR : case FLOAT_ARR :
			eprint( FATAL, "%s:%ld: A complete array can't be used in an "
					"assignment (try `%s[]' instead).\n",
					Fname, Lc, src->name );
			THROW( EXCEPTION );
*/
		default :
			assert( 1 == 0 );           /* we never should end here... */
	}

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
				"variable is used that has not been assigned a value.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* stop if left hand side needs an array slice but all we have is
	   a variable */

	if ( dest->flags & NEED_SLICE )
	{
		eprint( FATAL, "%s:%ld: In assignment to array `%s' an array "
				"slice is needed on the right hand side.\n",
				Fname, Lc, dest->from->name );
		THROW( EXCEPTION );
	}

	/* if the destination variable is still completely new set its type */

	if ( dest->type == UNDEF_VAR )
	{
		dest->type = IF_FUNC( dest->name ) ? INT_VAR : FLOAT_VAR;
		dest->flags &= ~NEW_VARIABLE;
	}

	/* Now do the assignment - take care: the left hand side variable can be
	   either a real variable or an array pointer with the void pointer
	   `val.gptr' in the variable structure pointing to the data to be set in
	   an array.
	*/

	switch ( dest->type )
	{
		case INT_VAR : case FLOAT_VAR :
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
			break;

		case ARR_PTR :
			if ( dest->from->type == INT_ARR )
			{
				if ( src->type == INT_VAR )
					*( ( long * ) dest->val.gptr ) = src->val.lval;
				else
				{
					eprint( WARN, "%s:%ld: WARNING: Using float value in "
							"assignment to integer variable.\n", Fname, Lc );
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

		default :                    /* we never should end here... */
			assert ( 1 == 0 );
	}
}
 

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_ass_from_ptr( Var *src, Var *dest )
{
	Var *d,
		*s;
	int i;


	/* may be that's paranoid, but... */

	assert( src->from->type & ( INT_ARR | FLOAT_ARR ) );

	/* we can't assign from an array slice to a variable */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) )
	{
		eprint( FATAL, "%s:%ld: Left hand side of assignment is a variable "
				"while right hand side is an array (slice).\n", Fname, Lc ); 
		THROW( EXCEPTION );
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
			eprint( FATAL, "%s:%ld: Arrays (or slices of) `%s' and `%s' have "
					"different size.\n", Fname, Lc, d->name, s->name );
			THROW( EXCEPTION );
		}

		if ( d->type == INT_ARR )
			dest->val.lpnt = ( long * ) dest->val.gptr;
		else
			dest->val.dpnt = ( double * ) dest->val.gptr;
	}

	/* Warn on float to integer assignment */

	if ( d->type == INT_ARR && s->type == FLOAT_ARR )
		eprint( WARN, "%s:%ld: Assignment of float array (slice) `%s' to "
				"integer array `%s'.\n", Fname, Lc, s->name, d->name );

	/* get also the correct type of pointer for the data source - the pointer
	   to the start of the slice is always stored as a void pointer */

	if ( s->type == INT_ARR )
		src->val.lpnt = ( long * ) src->val.gptr;
	else
		src->val.dpnt = ( double * ) src->val.gptr;

	/* Now copy the array slice */

	for ( i = 0; i < d->sizes[ d->dim - 1 ]; i++ )
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


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_ass_from_trans_ptr( Var *src, Var *dest )
{
	Var    *d;
	int    i;
	double *sdptr;
	long   *slptr;


	/* we can't assign from a transient array to a variable */

	if ( dest->type & ( INT_VAR | FLOAT_VAR ) )
	{
		eprint( FATAL, "%s:%ld: Left hand side of assignment is a variable "
				"while right hand side is an array (slice).\n", Fname, Lc ); 
		THROW( EXCEPTION );
	}

	/* Again being paranoid... */

	assert( dest->flags & NEED_SLICE );

	d = dest->from;

	/* Do allocation of memory (set size of missing dimension to the one of
	   the transient array) if the destination array needs it, otherwise check
	   that the sizes of the destinationarray size is equal to the length of
	   the transient array. */

	if ( d->flags & NEED_ALLOC )
	{
		d->sizes[ d->dim - 1 ] = src->len;

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
		if ( d->sizes[ d->dim - 1 ] != src->len )
		{
			eprint( FATAL, "%s:%ld: Array slice assigned to array `%s' does "
					"not fit its length.\n", Fname, Lc, d->name );
			THROW( EXCEPTION );
		}

		if ( d->type == INT_ARR )
			dest->val.lpnt = ( long * ) dest->val.gptr;
		else
			dest->val.dpnt = ( double * ) dest->val.gptr;
	}

	/* Warn on float to integer assignment */

	if ( d->type == INT_ARR && src->type == FLOAT_TRANS_ARR )
		eprint( WARN, "%s:%ld: Assignment of float array (or slice) to "
				"integer array `%s'.\n", Fname, Lc, d->name );

	/* Now copy the transient array as slice to the destination */

	if ( src->type == INT_TRANS_ARR )          /* set auxiliary variables */
		slptr = src->val.lpnt;
	else
		sdptr = src->val.dpnt;

	for ( i = 0; i < d->sizes[ d->dim - 1 ]; i++ )
	{
		if ( d->type == INT_ARR )
		{
			if ( src->type == INT_TRANS_ARR )
				*dest->val.lpnt++ = *slptr++;
			else
				*dest->val.lpnt++ = ( long ) *sdptr++;
		}
		else
		{
			if ( src->type == INT_TRANS_ARR )
				*dest->val.dpnt++ = ( double ) *slptr++;
			else
				*dest->val.dpnt++ = *sdptr++;
		}
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

void vars_arr_init( Var *v )
{
	long ni;
	Var  *p1,
		 *p2,
		 *a;


	/* if there are no initialization data this is indicated by a variable
	   of type UNDEF_VAR - just pop it as well as the array pointer */

	if ( v->type == UNDEF_VAR )
	{
		vars_pop( v->prev );
		vars_pop( v );
		return;
	}

	/* The variable pointer this function gets passed is a pointer to the very
       last initialization datum on the variable stack. Now we've got to work
       our way up in the variable stack until we find the first non-data
       variable, indicated by having the IS_META property set in its flags,
	   which should be a pointer to an array. */

	while ( v->type != ARR_PTR )
		v = v->prev;
	a = v->from;
	vars_check( a, INT_ARR | FLOAT_ARR );


	/* Variable sized arays can't be initialized, they need the assignment of
	   an array slice to determine the still unknown size of their very last
	   dimension */

	if ( a->flags & NEED_ALLOC )
	{
		eprint( FATAL, "%s:%ld: Variable sized array `%s' can not be "
				"initialized.\n", Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* If the array isn't newly declared we can't do an assignment */

	if ( ! ( v->flags & NEED_INIT ) )
	{
		eprint( FATAL, "%s:%ld: Initialization of array `%s' only allowed "
				"immediately after declaration.\n", Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* count number of initializers and check that it fits the number of
       elements the array */

	for ( p1 = v->next, ni = 0; p1 != NULL; ni++, p1 = p1->next )
		;

	if ( ni < a->len )
		eprint( WARN, "%s:%ld: Less initializers for array `%s' than it has "
				"elements.\n", Fname, Lc, a->name );

	if ( ni > a->len )
	{
		eprint( FATAL, "%s:%ld: Too many initializers for array `%s'.\n",
				Fname, Lc, a->name );
		THROW( EXCEPTION );
	}

	/* Finaly do the assignments - walk through the data and copy them into
	   the area for data in the array */

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


/*--------------------------------------------------------------------------*/
/* This function is called from the parsers for variable or function tokens */
/* possibly followed by a unit. If there was no unit the second argument is */
/* NULL and nothing has to be done. If it isn't NULL we've got top check    */
/* that the token is really a simple number and multiply it then with the   */
/* unit. If the variable or function token is an array we have to drop out  */
/* of parsing and print an error message instead.                           */
/*--------------------------------------------------------------------------*/

Var *apply_unit( Var *var, Var *unit ) 
{
	if ( unit == NULL )
	{
		if ( var->type & ( INT_VAR | FLOAT_VAR ) )
			return vars_mult( var, vars_push( INT_VAR, 1 ) );
		if ( var->type & ( INT_ARR | FLOAT_ARR ) )
			return vars_push( ARR_REF, var );

		assert( 1 == 0 );
	}
	else
	{
		if ( var->type & ( INT_VAR | FLOAT_VAR ) )
		    return vars_mult( var, unit );
		else
		{
			eprint( FATAL, "%s:%ld: Syntax error: A unit is applied to a "
					"non-number.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}
}
