#
# $Id$
#
#
# Makefile for ME-6000 and ME-6100 driver
#
# Copyright (C) 2002-2003 Jens Thoms Toerring
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


ifneq ($(ME6X00_KERNEL_SOURCE),)
    KERNEL_INC = $(ME6X00_KERNEL_SOURCE)
else
	KERNEL_INC = $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
							 echo "/lib/modules/`uname -r`/build"; \
						 else \
							 echo "/usr/src/linux"; \
						 fi )
endif

MODULE   = me6x00.o
SOURCES  = me6x00.c
HEADERS  = pci-compat.h sysdep.h me6x00_drv.h autoconf.h
INCLUDES = -I$(KERNEL_INC)/include -I.

CFLAGS   = -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes

CC       = gcc

incl_dir = $(prefix)/include


.SUFFIXES:
.PHONY: all install clean

all:
	$(MAKE) $(SOURCES:.c=.o)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DBGFLAGS) $(INCLUDES) -c -o $@ $<

install:
ifeq ($(ME6X00_KERNEL_SOURCE),)
	if [ -d /lib/modules/`uname -r` ]; then                     \
		if [ ! -d /lib/modules/`uname -r`/misc ]; then          \
			install -m 755 -d /lib/modules/`uname -r`/misc;     \
		fi;                                                     \
		install -m 644 $(MODULE) /lib/modules/`uname -r`/misc;  \
	else                                                        \
		if [ ! -d /lib/modules/misc ]; then                     \
			install -d 755 /lib/modules/misc;                   \
		fi;                                                     \
		cp $(MODULE) /lib/modules/misc;                         \
	fi
endif
	install -d $(incl_dir)
	install -m 644 me6x00_drv.h $(incl_dir)

clean:
	rm -f me6x00.o *~
