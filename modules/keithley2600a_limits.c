#if ! defined KEITHLEY2600A_LIMITS_H_
#define KEITHLEY2600A_LIMITS_H_


#include "keithley2600a.h"


/* Maximum voltage limits in dependence on current range setting */

#if defined _2601A || defined _2602A
Max_Limit keithley2600s_max_limitv[ ] = { {  1, 40 },
										  {  3,  6 },
										  {  0,  0 } };
#else
Max_Limit keithley2600s_max_limitv[ ] = { { 0.1, 200 },
										  { 1.5,  20 },
										  { 0,     0 } };
#endif

/* Maximum current limits in dependence on current voltage setting */

#if defined _2601A || defined _2602A
Max_Limit keithley2600s_max_limiti[ ] = { {  6, 3 },
										  { 40, 1 },
										  {  0, 0 } };
#else
Max_Limit keithley2600s_max_limiti[ ] = { { 20,  1.5 },
										  { 200, 0.1 },
										  {   0, 0   } };
#endif


#endif
