# $Id$
#
# Makefile for drivers for National Instruments DAQ board based
# on a DAQ-STC
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


ifneq ($(NI_DAQ_KERNEL_SOURCE),)
    KERNEL_INC := $(NI_DAQ_KERNEL_SOURCE)
else
	KERNEL_SRC := $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
							 echo "/lib/modules/`uname -r`/build"; \
						 else \
							 echo "/usr/src/linux"; \
						 fi )
endif


MODULE = ni_daq.o
SOURCES = $(shell ls pci_e_series/*.c daq_stc/*.c)
INCLUDES := -I$(KERNEL_SRC)/include -I.
CFLAGS   := -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes


export CFLAGS INCLUDES


.SUFFIXES:
.PHONY: all install clean pack


all:
	$(MAKE) -C pci_e_series
	$(MAKE) -C daq_stc
	$(MAKE) $(MODULE)


$(MODULE): $(SOURCES:.c=.o)
	ld -r -o $@ $(SOURCES:.c=.o)


clean:
	$(MAKE) -C pci_e_series clean
	$(MAKE) -C daq_stc clean
	$(RM) $(RMFLAGS) ni_daq.o *~ include/*~
