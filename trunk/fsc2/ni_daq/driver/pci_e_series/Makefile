#
# $Id$
#

#
# Makefile for National Instruments PCI E Series DAQ boards
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


SOURCES  = $(shell ls *.c)
HEADERS  = $(shell ls *.h) $(shell ls ../include/*.h) \
		   ../../include/ni_daq_drv.h
INCLUDES += -I../../include -I../include


all: $(SOURCES:.c=.o)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DBGFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	$(RM) $(RMFLAGS) *.o *~
