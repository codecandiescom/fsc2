/*
  $Id$

  This file contains configuration information for controlling the
  Bruker ER035M gaussmeter when accessed via a serial port and used
  in stand alone mode, i.e. not used for field control but just to
  measure fields. This file will be included as a header file, so
  the syntax must be valid C and changes will only become visible
  after re-compilation of the module.
*/


/* Define the name that's going to be used for the device */

#define DEVICE_NAME     "ER035M_SAS"


/* Define the number of the serial port to be used - 0 corresponds to
   /dev/ttyS0 (or COM1 in DOS-speak), 1 is /dev/ttyS1 (COM2) etc. Make
   sure this number is lower than NUM_SERIAL_PORTS as defined in the
   main Makefile. Port numbers larger than 99 are not allowed. */

#define SERIAL_PORT     0


/* Define the baud rate for communication with the gaussmeter. Use only
   the constants mentioned in the man page for termios(3). */

#define SERIAL_BAUDRATE B9600
