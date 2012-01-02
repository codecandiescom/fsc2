/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"
#include "exp_parser_common.h"


/* Number of program token structures to be allocated as a chunk */

#define PRG_CHUNK_SIZE 16384

/* Number of tokens to be parsed before forms are rechecked for user input -
   too small a number slows down the program quite a lot (and even moderately
   short EDL files may produce an appreciable number of tokens to be parsed
   when e.g. running in loops), while making it too large makes it difficult
   for the user to stop the interpreter. */

#define CHECK_FORMS_AFTER   8192


Token_Val_T Exp_Val;                      /* also used by exp_lexer.l */


static bool In_for_lex = UNSET;           /* set while handling for loop
                                             condition part */
static int In_cond = 0;                   /* counts conditional construts in
                                             conditionals */
static long Token_count;

static struct CB_Stack {
    Prg_Token_T *where;
    struct CB_Stack *next;
} *Cb_stack = NULL;                        /* curly braces stack */

extern int exp_testparse( void );         /* from exp_parser.y */
extern void exp_test_init( void );

extern int exp_runparse( void );          /* from exp_run_parser.y */
extern int exp_runparser_init( void );    /* from exp_run_parser.y */
extern int conditionparse( void );        /* from condition_parser.y */
extern void conditionparser_init( void ); /* from condition_parser.y */

extern int explex( void );                /* from exp_lexer.flex */
extern FILE *expin;                       /* from exp_lexer.flex */

extern void exprestart( FILE * fp );

extern Token_Val_T exp_runlval;           /* from exp_run_parser.y */
extern Token_Val_T exp_testlval;          /* from exp_test_parser.y */
extern Token_Val_T conditionlval;         /* from condition_parser.y */

/* local functions */

static void get_and_store_tokens( long * parenthesis_count,
                                  long * square_brace_count );
static void push_curly_brace( Prg_Token_T * where );
static bool pop_curly_brace( void );
static void loop_setup( void );
static void setup_while_or_repeat( int   type,
                                   long * pos );
static void setup_if_else( long        * pos,
                           Prg_Token_T * cur_wr );
static void setup_else( Prg_Token_T * cur,
                        long          i,
                        bool          in_if,
                        bool        * dont_need_close_parens );
static void setup_open_brace_in_if_else( Prg_Token_T * cur,
                                         long          i,
                                         bool          in_if );
static bool setup_close_brace_in_if_else( long        * pos,
                                          Prg_Token_T * cur,
                                          long          i,
                                          bool        * in_if );
static void exp_syntax_check( void );
static void deal_with_token_in_test( void );
static const char *get_construct_name( int token_type );


/*---------------------------------------------------------------------------*
 * This routine stores the experiment section of an EDL file in the form
 * of tokens, together with their semantic values as returned by the
 * lexer. This is necessary for two reasons: First, the experiment section
 * may contain loops. Without having the EDL program stored internally it
 * would be rather difficult to run through the loops since we would have
 * to re-read and re-tokenise the input file again and again, which not
 * only would be painfully slow but also difficult since what we read is
 * not the real EDL file(s) but a pipe that passes to us what remains from
 * the input files (there could even be more than one when #include
 * directives had been used) after filtering through the program
 * 'fsc2_clean' (of course, beside storing the experiment section in
 * tokenised form as done by this function, we also need routines that
 * later pass the stored tokens to the parsers, exp_testlex() and
 * exp_runlex()). Second, we will have to run through the experiment
 * section at least three times, first for syntax and sanity checks, then
 * for the test run and finally for really doing the experiment. .
 *
 * The function does the following:
 * 1. It stores each token (with the semantic values it receives from the
 *    lexer) in the array of program tokens, prg_token.
 * 2. While doing so it also does a few preliminay tests - it checks that
 *    opening and closing parentheses and braces match, BREAK and NEXT
 *    statements only appear in within loops and that THE ON_STOP statement
 *    isn't in a block or within parentheses or braces.
 * 3. When done with storing the program it does everything needed to set up
 *    loops, if-else and unless-else constructs.
 * 4. Finally, a syntax check of the program is run. In this check the whole
 *    program is feed to a parser to test if syntactic errors are found.
 *---------------------------------------------------------------------------*/

void
store_exp( FILE * in )
{
    static bool is_restart = UNSET;
    long parenthesis_count = 0;
    long square_brace_count = 0;
    long i;


    /* Set input file */

    expin = in;

    /* Let's try to keep the lexer happy... */

    if ( is_restart )
        exprestart( expin );
    else
        is_restart = SET;

    EDL.prg_token = NULL;
    Cb_stack = NULL;

    /* Get and store all tokens (if there are no program tokens at all return
       immediately, then there's nothing left to be done or checked). */

    get_and_store_tokens( &parenthesis_count, &square_brace_count );

    if ( EDL.prg_length <= 0 )
        return;

    /* Make all the tokens a linked list (but that's not used yet) */

    EDL.prg_token->end = EDL.prg_token + 1;

    for ( i = 1; i < EDL.prg_length - 1; i++ )
        EDL.prg_token[ i ].end   = EDL.prg_token + i + 1;

    EDL.prg_token[ i ].end = NULL;

    /* Check that all parentheses and braces are balanced */

    if ( parenthesis_count > 0 )
    {
        eprint( FATAL, UNSET, "'(' without closing ')' detected at end of "
                "program.\n" );
        THROW( EXCEPTION );
    }

    if ( Cb_stack != NULL  )
    {
        eprint( FATAL, UNSET, "Block starting with '{' at %s:%ld has no "
                "closing '}'.\n",
                Cb_stack->where->Fname, Cb_stack->where->Lc );
        THROW( EXCEPTION );
    }

    if ( square_brace_count > 0 )
    {
        eprint( FATAL, UNSET, "'[' without closing ']' detected at end of "
                "program.\n" );
        THROW( EXCEPTION );
    }

    /* Check and initialize IF constructs and loops */

    loop_setup( );

    /* Now we have to do a syntax check - some syntax errors can not be found
       when running the test because some branches of IF or UNLESS conditions
       might never get triggered. It has also the advantage that syntax erors
       will be found immediately instead after a long test run. */

    exp_syntax_check( );
}


/*---------------------------------------------------------------------*
 * The function stores all token it receives from the lexer (together
 * with the semantic value) in the array of program tokens, prg_token.
 * While doing so it also performs some preliminary checks on the
 * balancedness of braces and parentheses.
 *---------------------------------------------------------------------*/

static void
get_and_store_tokens( long * parenthesis_count,
                      long * square_brace_count )
{
    int token;
    Prg_Token_T *cur;
    char *cur_Fname = NULL;
    long curly_brace_in_loop_count = 0;
    bool in_loop = UNSET;


    while ( ( token = explex( ) ) != 0 )
    {
        if ( token == ON_STOP_TOK )
        {
            if ( *parenthesis_count > 0 )
            {
                print( FATAL, "Unbalanced parentheses before ON_STOP "
                       "label.\n" );
                THROW( EXCEPTION );
            }

            if ( *square_brace_count > 0 )
            {
                print( FATAL, "Unbalanced square braces before ON_STOP "
                        "label.\n" );
                THROW( EXCEPTION );
            }

            if ( Cb_stack != NULL )
            {
                print( FATAL, "ON_STOP label found within a block.\n" );
                THROW( EXCEPTION );
            }

            EDL.On_Stop_Pos = EDL.prg_length;
            continue;
        }

        /* If necessary get or extend memory for storing the tokens, then
           store the token */

        if ( EDL.prg_length % PRG_CHUNK_SIZE == 0 )
            EDL.prg_token = T_realloc( EDL.prg_token,
                                         ( EDL.prg_length + PRG_CHUNK_SIZE )
                                       * sizeof *EDL.prg_token );

        cur = EDL.prg_token + EDL.prg_length;
        cur->token = token;

        /* If the file name changed get a copy of the new file name. Then set
           the pointer in the program token structure to the file name and
           copy the current line number. Don't free the name of the previous
           file - all the program token structures from the previous file
           point to the string, and it may only be free()ed when the program
           token structures get free()ed. */

        if ( cur_Fname == NULL || strcmp( EDL.Fname, cur_Fname ) )
        {
            cur_Fname = NULL;
            cur_Fname = T_strdup( EDL.Fname );
        }

        cur->Fname = cur_Fname;
        cur->Lc = EDL.Lc;

        /* Initialize pointers needed for flow control and the data entries
           used in repeat loops */

        cur->start = cur->end = NULL;
        cur->counter = 0;

        /* In most cases it's enough just to copy the union with the value.
           But there are a few exceptions: for strings we need also a copy of
           the string since it's not persistent. Function tokens and also
           variable references just push their data onto the variable stack,
           so we have to copy them from the stack into the token structure.
           Finally, we have to do some sanity checks on parentheses etc. */

        switch( cur->token )
        {
            case FOR_TOK : case WHILE_TOK : case UNLESS_TOK :
            case REPEAT_TOK : case FOREVER_TOK :
                in_loop = SET;
                break;

            case '(' :
                *parenthesis_count += 1;
                break;

            case ')' :
                *parenthesis_count -= 1;
                if ( *parenthesis_count < 0 )
                {
                    print( FATAL, "Found ')' without matching '('.\n" );
                    THROW( EXCEPTION );
                }
                break;

            case '{' :
                if ( *parenthesis_count != 0 )
                {
                    print( FATAL, "More '(' than ')' found before a '{'.\n" );
                    THROW( EXCEPTION );
                }

                if ( *square_brace_count != 0 )
                {
                    print( FATAL, "More '[' than ']' found before a '{'.\n" );
                    THROW( EXCEPTION );
                }

                push_curly_brace( cur );
                if ( in_loop )
                    curly_brace_in_loop_count++;
                break;

            case '}' :
                if ( *parenthesis_count != 0 )
                {
                    print( FATAL, "More '(' than ')' found before a '}'.\n" );
                    THROW( EXCEPTION );
                }

                if ( *square_brace_count != 0 )
                {
                    print( FATAL, "More '[' than ']' found before a '}'.\n" );
                    THROW( EXCEPTION );
                }

                if ( ! pop_curly_brace( ) )
                {
                    print( FATAL, "Found '}' without matching '{'.\n" );
                    THROW( EXCEPTION );
                }

                if ( in_loop && --curly_brace_in_loop_count == 0 )
                    in_loop = UNSET;

                break;

            case '[' :
                *square_brace_count += 1;
                break;

            case ']' :
                *square_brace_count -= 1;
                if ( *square_brace_count < 0 )
                {
                    print( FATAL, "Found ']' without matching '['.\n" );
                    THROW( EXCEPTION );
                }
                break;

            case E_STR_TOKEN :
                cur->tv.sptr = NULL;
                cur->tv.sptr = T_strdup( Exp_Val.sptr );
                break;

            case E_FUNC_TOKEN :
                cur->tv.vptr = NULL;
                cur->tv.vptr = T_malloc( sizeof *cur->tv.vptr );
                memcpy( cur->tv.vptr, EDL.Var_Stack, sizeof *EDL.Var_Stack );
                cur->tv.vptr->name = NULL;
                cur->tv.vptr->name = T_strdup( EDL.Var_Stack->name );
                vars_pop( EDL.Var_Stack );
                break;

            case E_VAR_REF :
                cur->tv.vptr = NULL;
                cur->tv.vptr = T_malloc( sizeof *cur->tv.vptr );
                memcpy( cur->tv.vptr, EDL.Var_Stack, sizeof *EDL.Var_Stack );
                vars_pop( EDL.Var_Stack );
                break;

            case ';' :
                if ( *parenthesis_count != 0 )
                {
                    print( FATAL, "More '(' than ')' found at end of "
                            "statement.\n" );
                    THROW( EXCEPTION );
                }

                if ( *square_brace_count != 0 )
                {
                    print( FATAL, "More '[' than ']' found at end of of "
                           "statement.\n" );
                    THROW( EXCEPTION );
                }
                break;

            case BREAK_TOK :
                if ( ! in_loop )
                {
                    print( FATAL, "BREAK statement not within a loop.\n" );
                    THROW( EXCEPTION );
                }
                break;

            case NEXT_TOK :
                if ( ! in_loop )
                {
                    print( FATAL, "NEXT statement not within a loop.\n" );
                    THROW( EXCEPTION );
                }
                break;

            default :
                memcpy( &cur->tv, &Exp_Val, sizeof cur->tv );
                break;
        }

        EDL.prg_length++;
    }

    /* Now that we know how many tokens there are cut back the length of the
       array for tokens to the required length */

    if ( EDL.prg_length > 0 )
        EDL.prg_token = T_realloc( EDL.prg_token,
                                   EDL.prg_length * sizeof *EDL.prg_token );
    else
        EDL.prg_token = T_free( EDL.prg_token );
}


/*-------------------------------------------------------------------------*
 * Function is called for each opening curly brace found in the input file
 * to store the current file name and line number and thus to be able to
 * print more informative error messages if the braces don't match up.
 *-------------------------------------------------------------------------*/

static void
push_curly_brace( Prg_Token_T * where )
{
    struct CB_Stack *new_cb;


    new_cb = T_malloc( sizeof *new_cb );
    new_cb->next = Cb_stack;
    new_cb->where = where;
    Cb_stack = new_cb;
}


/*---------------------------------------------------------------------------*
 * Function is called when a closing curly brace is found in the input file
 * to remove the entry on the curly brace stack for the corresponding
 * opening brace. It returns OK when there was a corresponding opening brace
 * (i.e. opening and closing braces aren't unbalanced in favour of closing
 * braces), otherwise FAIL is returned. This function is also used when
 * getting rid of the curly brace stack after an exception was thrown.
 *---------------------------------------------------------------------------*/

static bool
pop_curly_brace( void )
{
    struct CB_Stack *old_cb;


    if ( Cb_stack == NULL )
        return FAIL;

    old_cb = Cb_stack;
    Cb_stack = old_cb->next;
    T_free( old_cb );

    return OK;
}


/*----------------------------------------------------*
 * Deallocates all memory used for storing the tokens
 * (and their semantic values) of a program.
 *----------------------------------------------------*/

void
forget_prg( void )
{
    char *cur_Fname;
    long i;


    EDL.On_Stop_Pos = -1;

    /* Check if anything has to be done at all */

    if ( EDL.prg_token == NULL )
    {
        EDL.prg_length = 0;
        return;
    }

    /* Get the address in the first token where the file name is stored and
       free the string, then free memory allocated in the program token
       structures. */

    cur_Fname = EDL.prg_token[ 0 ].Fname;
    T_free( cur_Fname );

    for ( i = 0; i < EDL.prg_length; ++i )
    {
        switch( EDL.prg_token[ i ].token )
        {
            case E_STR_TOKEN :
                T_free( EDL.prg_token[ i ].tv.sptr );
                break;

            case E_FUNC_TOKEN :       /* get rid of string for function name */
                T_free( EDL.prg_token[ i ].tv.vptr->name );
                /* fall through */

            case E_VAR_REF :          /* get rid of copy of stack variable */
                T_free( EDL.prg_token[ i ].tv.vptr );
                break;
        }

        /* If the file name changed we store the pointer to the new name and
           free the string */

        if ( EDL.prg_token[ i ].Fname != cur_Fname )
            T_free( cur_Fname = EDL.prg_token[ i ].Fname );
    }

    /* Get rid of the memory used for storing the tokens */

    EDL.prg_token = T_free( EDL.prg_token );
    EDL.prg_length = 0;

    /* Get rid of structures for curly braces that may have survived when an
       exception got thrown */

    while ( pop_curly_brace( ) )
        /* empty */ ;

    Cb_stack = NULL;
}


/*----------------------------------------------------------------*
 * Sets up the blocks of WHILE, UNTIL, REPEAT and FOR loops and
 * IF-ELSE and UNLESS-ELSE constructs where a block is everything
 * between a matching pair of curly braces.
 *----------------------------------------------------------------*/

static void
loop_setup( void )
{
    long i;
    long cur_pos;


    for ( i = 0; i < EDL.prg_length; ++i )
    {
        switch( EDL.prg_token[ i ].token )
        {
            case FOREVER_TOK :
                EDL.prg_token[ i ].counter = 1;
                /* fall through */

            case WHILE_TOK : case REPEAT_TOK :
            case FOR_TOK   : case UNTIL_TOK :
                cur_pos = i;
                setup_while_or_repeat( EDL.prg_token[ i ].token, &i );
                fsc2_assert( ! (    cur_pos < EDL.On_Stop_Pos
                                 && i > EDL.On_Stop_Pos ) );
                break;

            case IF_TOK : case UNLESS_TOK :
                cur_pos = i;
                setup_if_else( &i, NULL );
                fsc2_assert( ! (    cur_pos < EDL.On_Stop_Pos
                                 && i > EDL.On_Stop_Pos ) );
                break;
        }
    }
}


/*----------------------------------------------------------------*
 * Does the real work for setting up WHILE, REPEAT and FOR loops.
 * Can be called recursively to allow nested loops.
 * ->
 *  1. Type of loop (WHILE_TOK, UNTIL_TOK, REPEAT_TOK or FOR_TOK)
 *  2. Pointer to number of token
 *----------------------------------------------------------------*/

static void
setup_while_or_repeat( int    type,
                       long * pos )
{
    Prg_Token_T *cur = EDL.prg_token + *pos;
    long i = *pos + 1;


    /* Start of with some sanity checks */

    if ( i == EDL.prg_length )
    {
        eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in EXPERIMENT "
                "section.\n", EDL.prg_token[ i - 1 ].Fname,
                EDL.prg_token[ i - 1 ].Lc );
        THROW( EXCEPTION );
    }

    if ( type != FOREVER_TOK && EDL.prg_token[ i ].token == '{' )
    {
        eprint( FATAL, UNSET, "%s:%ld: Missing loop condition.\n",
                EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
        THROW( EXCEPTION );
    }

    if ( type == FOREVER_TOK && EDL.prg_token[ i ].token != '{' )
    {
        eprint( FATAL, UNSET, "%s:%ld: No condition can be used for a FOREVER "
                "loop.\n", EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
        THROW( EXCEPTION );
    }

    /* Look for the start and end of the WHILE, UNTIL, REPEAT, FOR or FOREVER
       block beginning with the first token after the keyword. Handle nested
       WHILE, UNTIL, REPEAT, FOR, FOREVER, IF or UNLESS tokens by calling the
       appropriate setup functions recursively. BREAK and NEXT tokens store
       in 'start' a pointer to the block header, i.e. the WHILE, UNTIL, REPEAT,
       FOR or FOREVER token. */

    for ( ; i < EDL.prg_length; i++ )
    {
        switch( EDL.prg_token[ i ].token )
        {
            case WHILE_TOK : case REPEAT_TOK :
            case FOR_TOK : case FOREVER_TOK :
            case UNTIL_TOK :
                setup_while_or_repeat( EDL.prg_token[ i ].token, &i );
                break;

            case NEXT_TOK : case BREAK_TOK :
                EDL.prg_token[ i ].start = cur;
                break;

            case IF_TOK : case UNLESS_TOK :
                setup_if_else( &i, cur );
                break;

            case ELSE_TOK :
                eprint( FATAL, UNSET, "%s:%ld: ELSE without IF/UNLESS in "
                        "current block.\n", EDL.prg_token[ i ].Fname,
                        EDL.prg_token[ i ].Lc );
                THROW( EXCEPTION );

            case '{' :
                if ( i + 1 == EDL.prg_length )
                {
                    eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
                            "EXPERIMENT section.\n", EDL.prg_token[ i ].Fname,
                            EDL.prg_token[ i ].Lc );
                    THROW( EXCEPTION );
                }
                cur->start = EDL.prg_token + i + 1;
                break;

            case '}' :
                cur->end = EDL.prg_token + i + 1;
                EDL.prg_token[ i ].end = cur;
                *pos = i;
                return;
        }
    }

    eprint( FATAL, UNSET, "Missing '}' for %s starting at %s:%ld.\n",
            get_construct_name( type ), cur->Fname, cur->Lc );
    THROW( EXCEPTION );
}


/*----------------------------------------------------------------------*
 * Does the real work for setting up IF-ELSE or UNLESS-ELSE constructs.
 * Can be called recursively to allow nested IF and UNLESS tokens.
 * ->
 *    1. pointer to number of token
 *    2. Pointer to program token of enclosing while, repeat or for
 *       loop (needed for handling of 'break' and 'continue').
 *----------------------------------------------------------------------*/

static void
setup_if_else( long        *  pos,
               Prg_Token_T * cur_wr )
{
    Prg_Token_T *cur = EDL.prg_token + *pos;
    long i = *pos + 1;
    bool in_if = SET;
    bool dont_need_close_parens = UNSET;           /* set for IF-ELSE and
                                                      UNLESS-ELSE constructs */

    /* Start with some sanity checks */

    if ( i == EDL.prg_length )
    {
        eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in EXPERIMENT "
                "section.\n", EDL.prg_token[ i - 1 ].Fname,
                EDL.prg_token[ i - 1 ].Lc );
        THROW( EXCEPTION );
    }

    if ( EDL.prg_token[ i ].token == '{' )
    {
        eprint( FATAL, UNSET, "%s:%ld: Missing condition after IF/UNLESS.\n",
                EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
    }

    /* Now let's get things done... */

    for ( ; i < EDL.prg_length; i++ )
        switch( EDL.prg_token[ i ].token )
        {
            case WHILE_TOK : case REPEAT_TOK :
            case FOR_TOK :   case FOREVER_TOK :
            case UNTIL_TOK :
                setup_while_or_repeat( EDL.prg_token[ i ].token, &i );
                break;

            case NEXT_TOK : case BREAK_TOK :
                if ( cur_wr == NULL )
                {
                    eprint( FATAL, UNSET, "%s:%ld: %s statement not within "
                            "a loop.\n", EDL.prg_token[ i ].token == NEXT_TOK ?
                            "NEXT" : "BREAK",
                            EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
                    THROW( EXCEPTION );
                }
                EDL.prg_token[ i ].start = cur_wr;
                break;

            case IF_TOK : case UNLESS_TOK :
                setup_if_else( &i, cur_wr );
                if ( dont_need_close_parens )
                {
                    if ( cur->end != NULL )
                        ( cur->end - 1 )->end = EDL.prg_token + i + 1;
                    else
                        cur->end = EDL.prg_token + i + 1;
                    EDL.prg_token[ i ].end = EDL.prg_token + i + 1;
                    *pos = i;
                    return;
                }
                break;

            case ELSE_TOK :
                setup_else( cur, i, in_if, &dont_need_close_parens );
                break;

            case '{' :
                setup_open_brace_in_if_else( cur, i, in_if );
                break;

            case '}' :
                if ( setup_close_brace_in_if_else( pos, cur, i, &in_if ) )
                    return;
        }

    eprint( FATAL, UNSET, "Missing '}' for %s starting at %s:%ld.\n",
            in_if ? "IF/UNLESS" : "ELSE", cur->Fname, cur->Lc );
    THROW( EXCEPTION );
}


/*------------------------------------------------------*
 * Handling of 'ELSE' during the setup of IF-ELSE loops
 *------------------------------------------------------*/

static void
setup_else( Prg_Token_T * cur,
            long          i,
            bool          in_if,
            bool        * dont_need_close_parens )
{
    if ( i + 1 == EDL.prg_length )
    {
        eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
                "EXPERIMENT section.\n",
                EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
        THROW( EXCEPTION );
    }

    if (    EDL.prg_token[ i + 1 ].token != '{'
         && EDL.prg_token[ i + 1 ].token != IF_TOK
         && EDL.prg_token[ i + 1 ].token != UNLESS_TOK )
    {
        eprint( FATAL, UNSET, "%s:%ld: Missing '{' after ELSE.\n",
                EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
        THROW( EXCEPTION );
    }

    if ( in_if )
    {
        eprint( FATAL, UNSET, "Missing '}' for ELSE block "
                "belonging to IF-ELSE construct starting at "
                "%s:%ld.\n", cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    if (     EDL.prg_token[ i + 1 ].token == IF_TOK
          || EDL.prg_token[ i + 1 ].token == UNLESS_TOK )
    {
        cur->end = EDL.prg_token + i;
        *dont_need_close_parens = SET;
    }
}


/*---------------------------------------------------*
 * Handling of '{' during the setup of IF-ELSE loops
 *---------------------------------------------------*/

static void
setup_open_brace_in_if_else( Prg_Token_T * cur,
                             long          i,
                             bool          in_if )
{
    if ( i + 1 == EDL.prg_length )
    {
        eprint( FATAL, UNSET, "%s:%ld: Unexpected end of file in "
                "EXPERIMENT section.\n",
                EDL.prg_token[ i ].Fname, EDL.prg_token[ i ].Lc );
        THROW( EXCEPTION );
    }

    if ( in_if )
        cur->start = EDL.prg_token + i + 1;
    else
        cur->end = EDL.prg_token + i - 1;
}


/*---------------------------------------------------*
 * Handling of '}' during the setup of IF-ELSE loops
 *---------------------------------------------------*/

static bool
setup_close_brace_in_if_else( long        * pos,
                              Prg_Token_T * cur,
                              long          i,
                              bool        * in_if )
{
    if ( *in_if )
    {
        *in_if = UNSET;
        if (    i + 1 < EDL.prg_length
             && EDL.prg_token[ i + 1 ].token == ELSE_TOK )
            return FALSE;
    }

    if ( cur->end != NULL )
        ( cur->end - 1 )->end = EDL.prg_token + i + 1;
    else
        cur->end = EDL.prg_token + i + 1;

    EDL.prg_token[ i ].end = EDL.prg_token + i + 1;
    *pos = i;

    return TRUE;
}


/*-------------------------------------------------------------------*
 * This function is called to start the syntax test of the currently
 * loaded EDL program. After initializing the pointer to the first
 * program token all it does is calling the test parser defined in
 * exp_test_parser.y. On syntax errors the test parser throws an
 * exception, otherwise this routine returns normally.
 *-------------------------------------------------------------------*/

static void
exp_syntax_check( void )
{
    if ( EDL.prg_token == NULL )
        return;

    EDL.Fname = T_free( EDL.Fname );

    TRY
    {
        EDL.cur_prg_token = EDL.prg_token;
        exp_test_init( );
        exp_testparse( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        EDL.Fname = NULL;
        RETHROW( );
    }

    EDL.Fname = NULL;
}


/*-------------------------------------------------------------------*
 * This function just feeds the stored program token by token to the
 * syntax test parser exp_testparse(), defined in exp_test_parser.y.
 * No actions are associated with this parsing stage, all that is
 * done is a test if the syntax of the program is ok.
 *-------------------------------------------------------------------*/

int
exp_testlex( void )
{
    if (    EDL.cur_prg_token != NULL
         && EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
    {
        EDL.Fname = EDL.cur_prg_token->Fname;
        EDL.Lc = EDL.cur_prg_token->Lc;
        memcpy( &exp_testlval, &EDL.cur_prg_token->tv,
                sizeof exp_testlval );
        return EDL.cur_prg_token++->token;
    }

    return 0;
}


/*-------------------------------------------------------------------*
 * Function does the test run of the program. For most of the stored
 * tokens of the EDL program it simply invokes the parser defined in
 * exp_run_parser.y, which will then call the function exp_runlex()
 * to get further tokens. Exceptions are tokens that are related to
 * flow control that aren't handled by the parser but by this
 * function.
 *-------------------------------------------------------------------*/

void
exp_test_run( void )
{
    int old_FLL = EDL.File_List_Len;


    EDL.Fname = T_free( EDL.Fname );
    In_for_lex = UNSET;
    In_cond = 0;

    TRY
    {
        /* 1. Save the variables so we can reset them to their initial values
              when the test run is finished.
           2. Call the functions experiment_time( ) and f_dtime() to initialize
              the time functions.
           3. Run the test run hook functions of the modules.
           4. Do the test run.
        */

        Fsc2_Internals.mode = TEST;
        vars_save_restore( SET );

        experiment_time( );
        EDL.experiment_time = 0.0;
        vars_pop( f_dtime( NULL ) );

        run_test_hooks( );

        exp_runparser_init( );

        EDL.cur_prg_token = EDL.prg_token;
        Token_count = 0;

        while (    EDL.cur_prg_token != NULL
                && EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
        {
            Token_count++;

            /* Give the 'Stop Test' button a chance to get tested... */

            if (    ! ( Fsc2_Internals.cmdline_flags & TEST_ONLY )
                 && ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
                 && Token_count >= CHECK_FORMS_AFTER )
            {
                fl_check_only_forms( );
                Token_count %= CHECK_FORMS_AFTER;
            }

            /* This will be set by the end() function which simulates
               pressing the "Stop" button in the display window */

            if ( EDL.do_quit )
            {
                if ( EDL.On_Stop_Pos < 0 )            /* no ON_STOP part ? */
                {
                    EDL.cur_prg_token = NULL;         /* -> stop immediately */
                    break;
                }
                else                                  /* goto ON_STOP part */
                    EDL.cur_prg_token = EDL.prg_token + EDL.On_Stop_Pos;

                EDL.do_quit = UNSET;
            }

            /* Now deal with the token at hand - the function only returns
               when control structure tokens are found */

            deal_with_token_in_test( );
        }

        tools_clear( );
        run_end_of_test_hooks( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        tools_clear( );

        EDL.Fname = NULL;
        vars_del_stack( );
        delete_devices( );                       /* run the exit hooks ! */
        vars_save_restore( UNSET );

        EDL.File_List_Len = old_FLL;
        close_all_files( );

        Fsc2_Internals.mode = PREPARATION;
        RETHROW( );
    }

    Fsc2_Internals.mode = PREPARATION;
    EDL.Fname = NULL;
    vars_save_restore( UNSET );
    EDL.File_List_Len = old_FLL;
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

static void
deal_with_token_in_test( void )
{
    Prg_Token_T *cur = EDL.cur_prg_token;


    switch ( cur->token )
    {
        case '}' :
            EDL.cur_prg_token = cur->end;
            break;

        case WHILE_TOK :
            if ( test_condition( cur ) )
            {
                cur->counter = 1;
                EDL.cur_prg_token = cur->start;
            }
            else
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->end;
            }
            break;

        case UNTIL_TOK :
            if ( ! test_condition( cur ) )
            {
                cur->counter = 1;
                EDL.cur_prg_token = cur->start;
            }
            else
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->end;
            }
            break;

        case REPEAT_TOK :
            if ( cur->counter == 0 )
                get_max_repeat_count( cur );
            if ( ++cur->count.repl.act <= cur->count.repl.max )
            {
                cur->counter++;
                EDL.cur_prg_token = cur->start;
            }
            else
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->end;
            }
            break;

        case FOR_TOK :
            if ( cur->counter == 0 )
                get_for_cond( cur );
            if ( test_for_cond( cur ) )
            {
                cur->counter = 1;
                EDL.cur_prg_token = cur->start;
            }
            else
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->end;
            }
            break;

        case FOREVER_TOK :
            if ( cur->counter )
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->start;
            }
            else                            /* get out of infinite loop! */
            {
                cur->counter = 0;
                EDL.cur_prg_token = cur->end;
            }
            break;

        case BREAK_TOK :
            cur->start->counter = 0;
            EDL.cur_prg_token = cur->start->end;
            break;

        case NEXT_TOK :
            EDL.cur_prg_token = cur->start;
            break;

        case IF_TOK : case UNLESS_TOK :
            EDL.cur_prg_token = test_condition( cur ) ? cur->start : cur->end;
            break;

        case ELSE_TOK :
            if ( ( ++EDL.cur_prg_token )->token == '{' )
                EDL.cur_prg_token++;
            break;

        default :
            exp_runparse( );               /* (re)start the parser */
            break;
    }
}


/*----------------------------------------------------------------*
 * Routine works as a kind of virtual lexer by simply passing the
 * parser the tokens that got stored while running store_exp().
 * The only exceptions are tokens dealing with flow control - for
 * most of them the parser gets signaled an end of file, only the
 * '}' is handled by the parser itself (but also as an EOF but
 * also doing some sanity checks).
 *----------------------------------------------------------------*/

int
exp_runlex( void )
{
    Var_T *ret, *from, *next, *prev;


    if (    EDL.cur_prg_token != NULL
         && EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
    {
        Token_count++;

        EDL.Fname = EDL.cur_prg_token->Fname;
        EDL.Lc    = EDL.cur_prg_token->Lc;

        switch( EDL.cur_prg_token->token )
        {
            case WHILE_TOK : case REPEAT_TOK : case BREAK_TOK   :
            case NEXT_TOK  : case FOR_TOK    : case FOREVER_TOK :
            case UNTIL_TOK : case IF_TOK     : case UNLESS_TOK  :
            case ELSE_TOK  :
                return 0;

            case '}' :
                return '}';

            case E_STR_TOKEN :
                exp_runlval.sptr = EDL.cur_prg_token->tv.sptr;
                break;

            case E_FUNC_TOKEN :
                ret = vars_push( INT_VAR, 0L );
                from = ret->from;
                next = ret->next;
                prev = ret->prev;
                memcpy( ret, EDL.cur_prg_token->tv.vptr, sizeof *ret );
                ret->name = T_strdup( ret->name );
                ret->from = from;
                ret->next = next;
                ret->prev = prev;
                exp_runlval.vptr = ret;
                break;

            case E_VAR_REF :
                ret = vars_push( INT_VAR, 0L );
                from = ret->from;
                next = ret->next;
                prev = ret->prev;
                memcpy( ret, EDL.cur_prg_token->tv.vptr, sizeof *ret );
                ret->from = from;
                ret->next = next;
                ret->prev = prev;
                exp_runlval.vptr = ret;
                break;

            default :
                memcpy( &exp_runlval, &EDL.cur_prg_token->tv,
                        sizeof exp_runlval );
                break;
        }

        return EDL.cur_prg_token++->token;
    }

    return 0;
}


/*-----------------------------------------------------------------------*
 * This routines returns the tokens to the parser while the condition of
 * a WHILE, REPEAT, FOR, IF OR UNLESS is parsed.
 *-----------------------------------------------------------------------*/

int
conditionlex( void )
{
    int token;
    Var_T *ret, *from, *next, *prev;


    if (    EDL.cur_prg_token != NULL
         && EDL.cur_prg_token < EDL.prg_token + EDL.prg_length )
    {
        Token_count++;

        EDL.Fname = EDL.cur_prg_token->Fname;
        EDL.Lc    = EDL.cur_prg_token->Lc;

        switch( EDL.cur_prg_token->token )
        {
            case '{' :                 /* always signifies end of condition */
                return 0;

            case '?' :
                In_cond++;
                EDL.cur_prg_token++;
                return '?';

            case ':' :                 /* separator in for loop condition */
                if ( In_cond > 0 )
                {
                    In_cond--;
                    EDL.cur_prg_token++;
                    return ':';
                }
                if ( In_for_lex )
                    return 0;
                print( FATAL, "Syntax error in condition at token ':'.\n" );
                THROW( EXCEPTION );

            case E_STR_TOKEN :
                conditionlval.sptr = EDL.cur_prg_token->tv.sptr;
                EDL.cur_prg_token++;
                return E_STR_TOKEN;

            case E_FUNC_TOKEN :
                ret = vars_push( INT_VAR, 0L );
                from = ret->from;
                next = ret->next;
                prev = ret->prev;
                memcpy( ret, EDL.cur_prg_token->tv.vptr, sizeof *ret );
                ret->name = T_strdup( ret->name );
                ret->from = from;
                ret->next = next;
                ret->prev = prev;
                conditionlval.vptr = ret;
                EDL.cur_prg_token++;
                return E_FUNC_TOKEN;

            case E_VAR_REF :
                ret = vars_push( INT_VAR, 0L );
                from = ret->from;
                next = ret->next;
                prev = ret->prev;
                memcpy( ret, EDL.cur_prg_token->tv.vptr, sizeof *ret );
                ret->from = from;
                ret->next = next;
                ret->prev = prev;
                conditionlval.vptr = ret;
                EDL.cur_prg_token++;
                return E_VAR_REF;

            case '=' :
                print( FATAL, "For comparisons '==' must be used ('=' is for "
                       "assignments only).\n", EDL.Fname, EDL.Lc );
                THROW( EXCEPTION );

            default :
                memcpy( &conditionlval, &EDL.cur_prg_token->tv,
                        sizeof conditionlval );
                token = EDL.cur_prg_token->token;
                EDL.cur_prg_token++;
                return token;
        }
    }

    return 0;
}


/*--------------------------------------------------------------------*
 * Function tests the condition of a WHILE, UNTIL, FOR or REPEAT loop
 * or an IF or UNLESS construct.
 *--------------------------------------------------------------------*/

bool
test_condition( Prg_Token_T * cur )
{
    bool condition;


    EDL.cur_prg_token++;                        /* skip the WHILE or IF etc. */
    conditionparser_init( );
    conditionparse( );                          /* get the value */
    fsc2_assert( EDL.Var_Stack->next == NULL ); /* Paranoia as usual... */

    /* Make sure there's a block after the condition */

    if ( EDL.cur_prg_token->token != '{' )
    {
        eprint( FATAL, UNSET, "%s:%ld: Missing block (enclosed by '{' and "
                "'}') after %s.\n",
                cur->Fname, cur->Lc, get_construct_name( cur->token ) );
        THROW( EXCEPTION );
    }

    /* Make sure returned value is either integer or float */

    if ( ! ( EDL.Var_Stack->type & ( INT_VAR | FLOAT_VAR | STR_VAR ) ) )
    {
        eprint( FATAL, UNSET, "%s:%ld: Invalid condition for %s.\n",
                cur->Fname, cur->Lc, get_construct_name( cur->token ) );
        THROW( EXCEPTION );
    }

    /* Test the result - everything nonzero returns OK */

    if ( EDL.Var_Stack->type == INT_VAR )
        condition = EDL.Var_Stack->val.lval ? OK : FAIL;
    else if ( EDL.Var_Stack->type == FLOAT_VAR )
        condition = EDL.Var_Stack->val.dval ? OK : FAIL;
    else                            /* if ( EDL.Var_Stack->type == STR_VAR ) */
        condition = ( EDL.Var_Stack->val.sptr[ 0 ] != '\0') ? OK : FAIL;

    vars_pop( EDL.Var_Stack );
    return ( cur->token != UNLESS_TOK ) ? condition : ! condition;
}


/*---------------------------------------------------------*
 * Functions determines the repeat count for a REPEAT loop
 *---------------------------------------------------------*/

void
get_max_repeat_count( Prg_Token_T * cur )
{
    EDL.cur_prg_token++;                         /* skip the REPEAT token */
    conditionparse( );                           /* get the value */
    fsc2_assert( EDL.Var_Stack->next == NULL );  /* Paranoia as usual... */

    /* Make sure the repeat count is either int or float */

    if ( ! ( EDL.Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
    {
        cur++;
        eprint( FATAL, UNSET, "%s:%ld: Invalid counter for REPEAT loop.\n",
                cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* Set the repeat count - warn if value is float an convert to integer */

    if ( EDL.Var_Stack->type == INT_VAR )
        cur->count.repl.max = EDL.Var_Stack->val.lval;
    else
    {
        eprint( WARN, UNSET, "%s:%ld: WARNING: Floating point value used as "
                "maximum count in REPEAT loop.\n",
                ( cur + 1 )->Fname, ( cur + 1 )->Lc );
        cur->count.repl.max = lrnd( EDL.Var_Stack->val.dval );
    }

    vars_pop( EDL.Var_Stack );
    cur->count.repl.act = 0;
    EDL.cur_prg_token++;                   /* skip the '{' */
    return;
}


/*--------------------------------------------------------------------------*
 * Function gets all parts of a for loop condition, i.e. the loop variable,
 * the start value assigned to the loop variable, the end value and and the
 * optional increment value. If the loop variable is an integer also the
 * end and increment variable must be integers.
 *--------------------------------------------------------------------------*/

void
get_for_cond( Prg_Token_T * cur )
{
    /* First of all get the loop variable */

    EDL.cur_prg_token++;             /* skip the FOR keyword */

    /* Make sure token is a variable and next token is '=' */

    if (    EDL.cur_prg_token->token != E_VAR_TOKEN
         || ( EDL.cur_prg_token + 1 )->token != '=' )
    {
        cur++;
        eprint( FATAL, UNSET, "%s:%ld: Syntax error in condition of FOR "
                "loop.\n", cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* If loop variable is new set its type */

    if ( EDL.cur_prg_token->tv.vptr->type == UNDEF_VAR )
    {
        EDL.cur_prg_token->tv.vptr->type =
                                        VAR_TYPE( EDL.cur_prg_token->tv.vptr );
        EDL.cur_prg_token->tv.vptr->flags &= ~ NEW_VARIABLE;
    }

    /* Make sure the loop variable is either an integer or a float */

    if ( ! ( EDL.cur_prg_token->tv.vptr->type & ( INT_VAR | FLOAT_VAR ) ) )
    {
        cur++;
        eprint( FATAL, UNSET, "%s:%ld: FOR loop variable must be integer or "
                "float variable.\n", cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* Store pointer to loop variable */

    cur->count.forl.act = EDL.cur_prg_token->tv.vptr;

    /* Now get start value to be assigned to loop variable */

    EDL.cur_prg_token +=2;                    /* skip variable and '=' token */
    In_for_lex = SET;                         /* allow ':' as separator */
    conditionparse( );                        /* get start value */
    fsc2_assert( EDL.Var_Stack->next == NULL );   /* Paranoia as usual... */

    /* Make sure there is at least one more token, i.e. the loops end value */

    if ( EDL.cur_prg_token->token != ':' )
    {
        cur++;
        In_for_lex = UNSET;
        eprint( FATAL, UNSET, "%s:%ld: Missing end value in FOR loop.\n",
                cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* Make sure the returned value is either integer or float */

    if ( ! ( EDL.Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
    {
        cur++;
        In_for_lex = UNSET;
        eprint( FATAL, UNSET, "%s:%ld: Invalid start value in FOR loop.\n",
                cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* Set start value of loop variable */

    if ( EDL.Var_Stack->type == INT_VAR )
    {
        if ( cur->count.forl.act->type == INT_VAR )
            cur->count.forl.act->val.lval = EDL.Var_Stack->val.lval;
        else
            cur->count.forl.act->val.dval = ( double ) EDL.Var_Stack->val.lval;
    }
    else
    {
        if ( cur->count.forl.act->type == INT_VAR )
        {
            eprint( WARN, UNSET, "%s:%ld: Using floating point value in "
                    "assignment to integer FOR loop variable %s.\n",
                    ( cur + 1 )->Fname,
                    ( cur + 1 )->Lc, cur->count.forl.act->name );
            cur->count.forl.act->val.lval = lrnd( EDL.Var_Stack->val.dval );
        }
        else
            cur->count.forl.act->val.dval = EDL.Var_Stack->val.dval;
    }

    vars_pop( EDL.Var_Stack );

    /* Get FOR loop end value */

    EDL.cur_prg_token++;                           /* skip the ':' */
    conditionparse( );                             /* get end value */
    fsc2_assert( EDL.Var_Stack->next == NULL );    /* Paranoia as usual... */

    /* Make sure end value is either integer or float */

    if ( ! ( EDL.Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
    {
        cur++;
        In_for_lex = UNSET;
        eprint( FATAL, UNSET, "%s:%ld: Invalid end value in FOR loop.\n",
                cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* If loop variable is integer 'end' must also be integer */

    if (    cur->count.forl.act->type == INT_VAR
         && EDL.Var_Stack->type == FLOAT_VAR )
    {
        cur++;
        In_for_lex = UNSET;
        eprint( FATAL, UNSET, "%s:%ld: End value in FOR loop is floating "
                "point value while loop variable is an integer.\n",
                cur->Fname, cur->Lc );
        THROW( EXCEPTION );
    }

    /* Set end value of loop */

    cur->count.forl.end.type = EDL.Var_Stack->type;
    if ( EDL.Var_Stack->type == INT_VAR )
        cur->count.forl.end.lval = EDL.Var_Stack->val.lval;
    else
        cur->count.forl.end.dval = EDL.Var_Stack->val.dval;

    vars_pop( EDL.Var_Stack );

    /* Set the increment */

    if ( EDL.cur_prg_token->token != ':' )      /* no increment given, use 1 */
    {
        if ( cur->count.forl.act->type == INT_VAR )
        {
            cur->count.forl.incr.type = INT_VAR;
            cur->count.forl.incr.lval = 1;
        }
        else
        {
            cur->count.forl.incr.type = FLOAT_VAR;
            cur->count.forl.incr.dval = 1.0;
        }

    }
    else                                        /* get for loop increment */
    {
        EDL.cur_prg_token++;                    /* skip the ':' */
        conditionparse( );                      /* get end value */
        In_for_lex = UNSET;
        fsc2_assert( EDL.Var_Stack->next == NULL ); /* Paranoia as usual... */

        /* Make sure the increment is either an integer or a float */

        if ( ! ( EDL.Var_Stack->type & ( INT_VAR | FLOAT_VAR ) ) )
        {
            cur++;
            eprint( FATAL, UNSET, "%s:%ld: Invalid increment for FOR loop.\n",
                    cur->Fname, cur->Lc );
            THROW( EXCEPTION );
        }

        /* If loop variable is an integer, 'incr' must also be integer */

        if (    cur->count.forl.act->type == INT_VAR
             && EDL.Var_Stack->type == FLOAT_VAR )
        {
            cur++;
            In_for_lex = UNSET;
            eprint( FATAL, UNSET, "%s:%ld: FOR loop increment is floating "
                    "point value while loop variable is an integer.\n",
                    cur->Fname, cur->Lc );
            THROW( EXCEPTION );
        }

        cur->count.forl.incr.type = EDL.Var_Stack->type;

        /* Check that increment isn't zero */

        if ( EDL.Var_Stack->type == INT_VAR )
        {
            if ( EDL.Var_Stack->val.lval == 0 )
            {
                cur++;
                eprint( FATAL, UNSET, "%s:%ld: Zero increment in FOR loop.\n",
                        cur->Fname, cur->Lc );
                THROW( EXCEPTION );
            }
            cur->count.forl.incr.lval = EDL.Var_Stack->val.lval;
        }
        else
        {
            if ( EDL.Var_Stack->val.dval == 0 )
            {
                cur++;
                eprint( FATAL, UNSET, "%s:%ld: Zero increment for FOR loop.\n",
                        cur->Fname, cur->Lc );
                THROW( EXCEPTION );
            }
            cur->count.forl.incr.dval = EDL.Var_Stack->val.dval;
        }

        vars_pop( EDL.Var_Stack );
    }

    In_for_lex = UNSET;
    EDL.cur_prg_token++;                /* skip the '{' */
    return;

}


/*--------------------------------------------------*
 * Function tests the loop condition of a for loop.
 *--------------------------------------------------*/

bool
test_for_cond( Prg_Token_T * cur )
{
    bool sign = UNSET;


    /* If this isn't the very first call, increment the loop variable */

    if ( cur->counter != 0 )
    {
        if ( cur->count.forl.act->type == INT_VAR )
                cur->count.forl.act->val.lval += cur->count.forl.incr.lval;
        else
            cur->count.forl.act->val.dval +=
                cur->count.forl.incr.type == INT_VAR ?
                    cur->count.forl.incr.lval : cur->count.forl.incr.dval;
    }

    /* Get sign of increment */

    if (    (    cur->count.forl.incr.type == INT_VAR
              && cur->count.forl.incr.lval < 0 )
         || (    cur->count.forl.incr.type == FLOAT_VAR
              && cur->count.forl.incr.dval < 0 ) )
        sign = SET;

    /* If the increment is positive test if loop variable is less or equal to
       the end value, if increment is negative if loop variable is larger or
       equal to the end value. Return the result. */

    if ( cur->count.forl.act->type == INT_VAR )
    {
        if ( ! sign )     /* 'incr' has positive sign */
            return cur->count.forl.act->val.lval <= cur->count.forl.end.lval;
        else              /* 'incr' has negative sign */
            return cur->count.forl.act->val.lval >= cur->count.forl.end.lval;
    }
    else
    {
        if ( ! sign )     /* 'incr' has positive sign */
            return cur->count.forl.act->val.dval <=
                       ( cur->count.forl.end.type == INT_VAR ?
                         cur->count.forl.end.lval : cur->count.forl.end.dval );
        else              /* 'incr' has negative sign */
            return cur->count.forl.act->val.dval >=
                       ( cur->count.forl.end.type == INT_VAR ?
                         cur->count.forl.end.lval : cur->count.forl.end.dval );
    }

    /* We can't end up here... */

#ifndef NDEBUG
    eprint( FATAL, UNSET, "Internal error at %s:%d.\n", __FILE__, __LINE__ );
#endif

    THROW( EXCEPTION );
    return FAIL;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

bool
check_result( Var_T * v )
{
    if ( ! ( v->type & ( INT_VAR | FLOAT_VAR ) ) )
    {
        print( FATAL, "Only integer and floating point values can be used "
               "in tests.\n" );
        THROW( EXCEPTION );
    }

    /* Test the result - everything nonzero returns OK */

    if ( v->type == INT_VAR )
        return v->val.lval ? OK : FAIL;
    else
        return v->val.dval != 0.0 ? OK : FAIL;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static const char *get_construct_name( int token_type )
{
    switch ( token_type )
    {
        case WHILE_TOK :
            return "WHILE loop";

        case UNTIL_TOK :
            return "UNTIL loop";

        case REPEAT_TOK :
            return "REPEAT loop";

        case FOREVER_TOK :
            return "FOREVER loop";

        case FOR_TOK :
            return "FOR loop";

        case IF_TOK :
            return "IF construct";

        case UNLESS_TOK :
            return "UNLESS construct";
    }

#ifndef NDEBUG
    eprint( FATAL, UNSET, "Internal error at %s:%d.\n",
            __FILE__, __LINE__ );
#endif

    THROW( EXCEPTION );
    return NULL;                  /* we'll never get here... */
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
