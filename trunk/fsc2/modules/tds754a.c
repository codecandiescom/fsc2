/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


#define DEVICE_NAME "TDS754A"


typedef struct
{
	int device;
} TDS754A;

static TDS754A tds754a;




/* declaration of exported functions */

int tds754a_init_hook( void );
int tds754a_test_hook( void );
int tds754a_exp_hook( void );
void tds754a_exit_hook( void );




/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds754a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds754a_exp_hook( void )
{
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

void tds754a_exit_hook( void )
{
}
