/*
  $Id$
*/


%option noyywrap nocase-sensitive nounput never-interactive pointer

%{

#include "fsc2.h"
#include "gpib.h"
#include "gpib_parser.h"

int gpiblex( void );

%}


%x COMMENT
%x CONFIG

CHAR   (\'\\[afnrtv?'"]\')
NAME   \"[^"]+\"
ERR    err(log)?
TIMO   timeout|timo
MASTER controller|master
REOS   (set-)?reos
XEOS   (set-)?xeos
BIN    (set-)?bin
EOT    (set-)?eot

W      [ \t]


%%         /* Rules */

"/*"		          BEGIN( COMMENT );
<COMMENT>[^*]*
<COMMENT>"*"+[^*/]*
<COMMENT>"*"+"/"      BEGIN( INITIAL );

"device"	          return DEVICE_TOKEN;
"name"                return NAME_TOKEN;
"pad"                 return PAD_TOKEN;
"sad"                 return SAD_TOKEN;
"eos"                 return EOS_TOKEN;
"file"                return FILE_TOKEN;
{TIMO}                return TIMO_TOKEN;
{REOS}                return REOS_TOKEN;
{XEOS}                return XEOS_TOKEN;
{BIN}                 return BIN_TOKEN;
{EOT}                 return EOT_TOKEN;
{MASTER}              return MASTER_TOKEN;

"yes"                 {
                          gpiblval.bval = 1;
                          return BOOL_TOKEN;
                      }
"no"                  {
                          gpiblval.bval = 0;
                          return BOOL_TOKEN;
                      }
"none"                {
                          gpiblval.ival = 0;
                          return TIME_TOKEN;
                      }

10{W}*us              {
                          gpiblval.ival = 1;
						  return TIME_TOKEN;
					  }
30{W}*us              {
                          gpiblval.ival = 2;
						  return TIME_TOKEN;
					  }
100{W}*us             {
                          gpiblval.ival = 3;
						  return TIME_TOKEN;
				      }
300{W}*us             {
                          gpiblval.ival = 4;
						  return TIME_TOKEN;
		 			  }
1{W}*ms               {
                          gpiblval.ival = 5;
						  return TIME_TOKEN;
					  }
3{W}*ms               {
                          gpiblval.ival = 6;
						  return TIME_TOKEN;
                      }
10{W}*ms              {
                          gpiblval.ival = 7;
						  return TIME_TOKEN;
                      }
30{W}*ms              {
                          gpiblval.ival = 8;
						  return TIME_TOKEN;
					  }
100{W}*ms             {
                          gpiblval.ival = 9;
                          return TIME_TOKEN;
                      }
300{W}*ms             {
                          gpiblval.ival = 10;
                          return TIME_TOKEN;
                      }
1{W}*s                {
                          gpiblval.ival = 11;
                          return TIME_TOKEN;
                      }
3{W}*s                {
                          gpiblval.ival = 12;
                          return TIME_TOKEN;
                      }
10{W}*s               {
                          gpiblval.ival = 13;
                          return TIME_TOKEN;
                      }
30{W}*s               {
                          gpiblval.ival = 14;
                          return TIME_TOKEN;
                      }
100{W}*s              {
                          gpiblval.ival = 15;
                          return TIME_TOKEN;
                      }
300{W}*s              {
                          gpiblval.ival = 16;
                          return TIME_TOKEN;
                      }
1000{W}*s             {
                          gpiblval.ival = 17;
                          return TIME_TOKEN;
                      }

{CHAR}                {
						  switch ( gpibtext[ 2 ] )
						  {
							  case 'a' :
								  gpiblval.ival = ( int ) '\a';
								  break;

							  case 'f' :
								  gpiblval.ival = ( int ) '\f';
								  break;

							  case 'n' :
								  gpiblval.ival = ( int ) '\n';
								  break;

							  case 'r' :
								  gpiblval.ival = ( int ) '\r';
								  break;

							  case 't' :
								  gpiblval.ival = ( int ) '\t';
								  break;

							  case 'v' :
								  gpiblval.ival = ( int ) '\v';
								  break;

							  case '?' :
								  gpiblval.ival = ( int ) '\?';
								  break;

							  case '\'' :
								  gpiblval.ival = ( int ) '\'';
								  break;

							  case '"' :
								  gpiblval.ival = ( int ) '\"';
								  break;

							  default :
								  exit( 1 );
                          }

						  return NUM_TOKEN;
                      }
[0-9]+                {
                          sscanf( gpibtext, "%d", &gpiblval.ival ); 
	                      return NUM_TOKEN;
                      }
0x[0-9A-F]{1,8}       {
                          sscanf( gpibtext, "0x%x", &gpiblval.ival );
	                      return NUM_TOKEN;
                      }

[ \t\n]*              /* Skip white space */

{NAME}                {
                          char *p;
                          gpiblval.sval = malloc( strlen( gpibtext ) );
                          strcpy( gpiblval.sval, gpibtext + 1 ); 
                          p = gpiblval.sval;
					      gpiblval.sval = p;
                          while ( *p != '"' )
                              p++;
                          *p = '\0';
                          return STR_TOKEN;
                      }
.	                  return *gpibtext;
<<EOF>>               return 0;

%%         /* End of rules */
