# Makefile for Wasco WITIO-48 ISA DIO card kernel module
#
# Copyright (C) 2003-2014  Jens Thoms Toerring
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
# To contact the author send email to:  jt@toerring.de


MODULE   := witio_48.o

# Unless the user set the environment variable WITIO_48_KERNEL_SOURCE
# assume that we can find out where the kernel sources are (or at least
# the kernel headers) from the 'build' symbolic link in the module
# directory of the currently running kernel. Only if this doesn't exist
# use '/usr/src/linux/' as a last resort - but it's not unlikely to point
# to the wrong kernel code or may even not exist...

ifneq ($(WITIO_48_KERNEL_SOURCE),)
    KERNELDIR := $(WITIO_48_KERNEL_SOURCE)
endif

KERNELDIR ?= $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
					     echo "/lib/modules/`uname -r`/build"; \
					 else \
						 echo "/usr/src/linux"; \
					 fi )

# If KERNEL_VER isn't set determine it from the the kernel headers (it used
# to be defined in linux/version.h but on new systems its' now in its own
# file linux/utsrelease.h and on even newer in generated/utsrelease.h) and
# then call make again with KERNEL_VER now being set. If KERNEL_VER could
# not be found out that way print an error message and bail out.

ifndef KERNEL_VER

KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'            \
                            $(KERNELDIR)/include/generated/utsrelease.h \
				            2>/dev/null |                               \
	            	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'            \
	                            $(KERNELDIR)/include/linux/utsrelease.h     \
					            2>/dev/null |                               \
		            	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'            \
		                        $(KERNELDIR)/include/linux/version.h        \
					            2>/dev/null |                               \
	        	    	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell make --quiet -C $(KERNELDIR) kernelrelease 2>/dev/null | grep -v make)
endif


all $(MODULE) install check clean:
	@if [ -z "$(KERNEL_VER)" ]; then \
		echo "Error: Can't figure out kernel version under the assumption that the kernel sources are at $(KERNELDIR)"; \
		exit 2; \
	fi
	$(MAKE) $@ KERNEL_VER=$(KERNEL_VER)

else      # if KERNEL_VER is set

# First handle the case that we're building for a 2.6 or 3.x kernel

ifneq ($(shell echo $(KERNEL_VER) | grep '^\(2\.6\)\|3\.' 2>/dev/null),)

ifneq ($(KERNELRELEASE),)    # if can use the kernel build system
	obj-m         := $(MODULE)
	witio_48-objs := $(OBJECT)

else                         # otherwise make kernel build system available

PWD          := $(shell pwd)
SOURCE   := witio_48_drv.c
OBJECT   := $(SOURCE:.c=.o)
EXTRA_CFLAGS := -I$(PWD)/../include
export OBJECT

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) EXTRA_CFLAGS=$(EXTRA_CFLAGS)

check:
	$(MAKE) -C $(KERNELDIR) M="$(PWD)" C=1 EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
	$(INSTALL) -d $(prefix)/include
	$(INSTALL) -m 644 witio_48_drv.h $(prefix)/include

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(RM) $(RMFLAGS) Module.symvers Module.markers modules.order *~

endif

else

# Otherwise we're dealing with some pre-2.6 kernel (hopefully 2.4)

SOURCE   := witio_48_drv.c
OBJECT   := $(SOURCE:.c=.o)
HEADERS  = witio_48_drv.h autoconf.h
INCLUDES = -I$(KERNELDIR)/include -I.
CFLAGS   = -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes


.SUFFIXES:
.PHONY: all install cleanup clean

all:
	$(MAKE) $(MODULE)

$(MODULE): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

install:
ifeq ($(WITIO_48_KERNEL_SOURCE),)
	if [ -d /lib/modules/$(KERNEL_VER) ]; then                        \
		if [ ! -d /lib/modules/$(KERNEL_VER)/misc ]; then             \
			$(INSTALL) -m 755 -d /lib/modules/$(KERNEL_VER)/misc;     \
		fi;                                                           \
		$(INSTALL) -m 644 $(MODULE) /lib/modules/$(KERNEL_VER)/misc;  \
	else                                                              \
		if [ ! -d /lib/modules/misc ]; then                           \
			$(INSTALL) -m 755 -d /lib/modules/misc;                   \
		fi;                                                           \
		$(INSTALL) -m 644 $(MODULE) /lib/modules/misc;                \
	fi
endif
	$(INSTALL) -d $(prefix)/include
	$(INSTALL) -m 644 witio_48_drv.h $(prefix)/include

cleanup:
	$(RM) $(RMFLAGS) *~

clean:
	$(RM) $(RMFLAGS) *.o svn-* Module.symvers *~

endif
endif
