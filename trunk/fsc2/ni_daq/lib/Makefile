#  $Id$
#
#  Library for National Instruments DAQ boards based on a DAQ-STC
#
#  Copyright (C) 2003-2004 Jens Thoms Toerring
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; see the file COPYING.  If not, write to
#  the Free Software Foundation, 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.
#
#  To contact the author send email to
#  Jens.Toerring@physik.fu-berlin.de


# Set here the path where the library and include file will be installed

lib_dir   = $(prefix)/lib

CFLAGS    = -ggdb -O0 -Wall -Wwrite-strings -Wstrict-prototypes \
			-Wmissing-prototypes -Wmissing-declarations -W
#LDFLAGS   = -Wl,--version-script=ni_daq_lib.map
INCLUDES  = -I../include -I.

sources   = $(shell ls *.c)
headers   = ../include/ni_daq.h ../include/ni_daq_drv.h


.SUFFIXES:                    # don't use implicit rules
.PHONY: all clean install


all: libni_daq.so libni_daq.a

libni_daq.so: ${sources:.c=.o} $(headers) ni_daq_lib.map
	$(CC) -shared -o $@ ${sources:.c=.o} $(LDFLAGS)

libni_daq.a: ${sources:.c=.o} $(headers)
	ar crs $@ ${sources:.c=.o}

%.o: %.c $(headers)
	$(CC) -ggdb $(CFLAGS) $(INCLUDES) -c -o $@ $<

install:
	$(INSTALL) -d $(lib_dir)
	$(INSTALL) -m 755 libni_daq.so $(lib_dir)
	$(INSTALL) -m 644 libni_daq.a  $(lib_dir)

clean:
	$(RM) $(RMFLAGS) *.o *.so *.a *~

distclean:
	$(MAKE) clean
