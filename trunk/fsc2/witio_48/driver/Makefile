#
# $Id$
#
#
# Makefile for Wasco WITIO-48 ISA DIO card kernel (2.4 series) module
#
# Copyright (C) 2003 Jens Thoms Toerring
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# To contact the author send email to
# Jens.Toerring@physik.fu-berlin.de


ifneq ($(WITIO_48_KERNEL_SOURCE),)
    KERNEL_INC = $(WITIO_48_KERNEL_SOURCE)
else
	KERNEL_SRC = $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
							 echo "/lib/modules/`uname -r`/build"; \
						 else \
							 echo "/usr/src/linux"; \
						 fi)
endif

MODULE   = witio_48.o
SOURCES  = witio_48_drv.c
HEADERS  = witio_48_drv.h autoconf.h
INCLUDES = -I$(KERNEL_SRC)/include -I.

CFLAGS   = -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes

incl_dir = $(prefix)/include


.SUFFIXES:
.PHONY: all install clean distclean

all:
	$(MAKE) $(MODULE)

$(MODULE): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install:
ifeq ($(WITIO_48_KERNEL_SOURCE),)
	if [ -d /lib/modules/`uname -r` ]; then                        \
		if [ ! -d /lib/modules/`uname -r`/misc ]; then             \
			$(INSTALL) -m 755 -d /lib/modules/`uname -r`/misc;     \
		fi;                                                        \
		$(INSTALL) -m 644 $(MODULE) /lib/modules/`uname -r`/misc;  \
	else                                                           \
		if [ ! -d /lib/modules/misc ]; then                        \
			$(INSTALL) -m 755 -d /lib/modules/misc;                \
		fi;                                                        \
		$(INSTALL) -m 644 $(MODULE) /lib/modules/misc;             \
	fi
endif
	$(INSTALL) -d $(incl_dir)
	$(INSTALL) -m 644 witio_48_drv.h $(incl_dir)

clean:
	$(RM) $(RMFLAGS) -f *.o *~
