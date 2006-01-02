#
# $Id$
#
#  Makefile for NI6601 library
#
#  Copyright (C) 2002-2006 Jens Thoms Toerring
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
			-Wmissing-prototypes -Wmissing-declarations -W
#LDFLAGS   = -Wl,--version-script=ni6601_lib.map
INCLUDES  = -I../driver -I.

sources   = ni6601_lib.c
headers   = ni6601.h ../driver/ni6601_drv.h ../driver/autoconf.h

.SUFFIXES:                    # don't use implicit rules
.PHONY: all cleanup clean install

all: libni6601.so libni6601.a


libni6601.so: ${sources:.c=.o} $(headers) ni6601_lib.map
	$(CC) -shared -o $@ ${sources:.c=.o} $(LDFLAGS)

libni6601.a: ${sources:.c=.o} $(headers)
	ar crs $@ ${sources:.c=.o}

%.o: %.c $(headers)
	$(CC) -ggdb $(CFLAGS) $(INCLUDES) -c -o $@ $<

ni_test: ni_test.c libni6601.so
	$(CC) -ggdb $(CFLAGS) $(INCLUDES) -o $@ $< -L. -lni6601

install:
	$(INSTALL) -d $(lib_dir)
	$(INSTALL) -d $(incl_dir)
	$(INSTALL) -m 755 libni6601.so $(lib_dir)
	$(INSTALL) -m 644 libni6601.a  $(lib_dir)
	$(INSTALL) -m 644 ni6601.h $(incl_dir)


cleanup:
	$(RM) $(RMFLAGS) *.o ni_test

clean:
	$(MAKE) cleanup
	$(RM) $(RMFLAGS) *.so *.a *~
