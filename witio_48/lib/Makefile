#  Makefile for WITIO-48 library
#
#  Copyright (C) 2003-2014  Jens Thoms Toerring
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#  To contact the author send email to:  jt@toerring.de


# Set here the path where the library and include file will be installed

lib_dir   = $(prefix)/lib
incl_dir  = $(prefix)/include


CFLAGS    = -ggdb -O2 -Wall -Wwrite-strings -Wstrict-prototypes \
			-Wmissing-prototypes -Wmissing-declarations -W -fpic
# LDFLAGS   = -Wl,--version-script=witio_48_lib.map
INCLUDES  = -I../driver -I.

sources   = witio_48_lib.c
headers   = witio_48.h ../driver/witio_48_drv.h

.SUFFIXES:                    # don't use implicit rules
.PHONY: all cleanup clean install

all: libwitio_48.so libwitio_48.a


libwitio_48.so: ${sources:.c=.o} $(headers) witio_48_lib.map
	$(CC) -shared -o $@ ${sources:.c=.o} $(LDFLAGS)

libwitio_48.a: ${sources:.c=.o} $(headers)
	ar crs $@ ${sources:.c=.o}

%.o: %.c $(headers)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install:
	$(INSTALL) -d $(lib_dir)
	$(INSTALL) -d $(incl_dir)
	$(INSTALL) -m 755 libwitio_48.so $(lib_dir)
	$(INSTALL) -m 644 libwitio_48.a  $(lib_dir)
	$(INSTALL) -m 644 witio_48.h $(incl_dir)
	$(INSTALL) -m 644 ../driver/witio_48_drv.h $(incl_dir)

cleanup:
	$(RM) $(RMFLAGS) *.o

clean:
	$(RM) $(RMFLAGS) *.o *.so *.a *~ svn-*
