#
# $Id$
#
# Makefile for the RULBUS EPP kernel driver
#
# Copyright (C) 2003-2009 Jens Thoms Toerring
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
# To contact the author send email to jt@toerring.de


MODULE   := rulbus_epp.o
SOURCES  := rulbus_epp.c
PWD      := $(shell pwd)
HEADERS  := $(PWD)/../include/rulbus_epp.h $(PWD)/../include/autoconf.h


# Unless the user set the environment variable RULBUS_EPP_KERNEL_SOURCE
# assume that we can find out where the kernel sources are (or at least
# the kernel headers) from the 'build' symbolic link in the module
# directory of the currently running kernel. Only if this doesn't exist
# use '/usr/src/linux/' as a last resort - but it's not unlikely to point
# to the wrong kernel code or may even not exist...

ifneq ($(RULBUS_EPP_KERNEL_SOURCE),)
    KERNELDIR := $(RULBUS_EPP_KERNEL_SOURCE)
endif

KERNELDIR ?= $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
					     echo "/lib/modules/`uname -r`/build"; \
					 else \
						 echo "/usr/src/linux"; \
					 fi )

# If KERNEL_VER isn't set determine it from the the kernel headers (it used
# to be defined in linux/version.h but on new systems its' now in its own
# file linux/utsrelease.h) and then call make again with KERNEL_VER now being
# set. If KERNEL_VER couldn't be found out that way print an error message
# and bail out.

ifndef KERNEL_VER

KERNEL_VER := $(shell if [ -e $(KERNELDIR)/include/linux/utsrelease.h ]; then \
	                       grep UTS_RELEASE                                   \
                                $(KERNELDIR)/include/linux/utsrelease.h       \
				                2>/dev/null |                                 \
				                sed 's/^[^"]\{1,\}"\([^"]\{1,\}\)".*$$/\1/  ' \
                                2>/dev/null;                                  \
                      else                                                    \
	                       grep UTS_RELEASE                                   \
                                $(KERNELDIR)/include/linux/version.h          \
				                2>/dev/null |                                 \
				                sed 's/^[^"]\{1,\}"\([^"]\{1,\}\)".*$$/\1/'   \
                                2>/dev/null;                                  \
                      fi )

all $(MODULE) install check clean:
	@if [ -z "$(KERNEL_VER)" ]; then \
		echo "Error: Can't figure out kernel version under the assumption that the kernel sources are at $(KERNELDIR)"; \
		exit 2; \
	fi
	$(MAKE) $@ KERNEL_VER=$(KERNEL_VER)

else      # if KERNEL_VER is set

# First handle the case that we're building for a 2.6 kernel

ifeq ($(findstring 2.6,$(KERNEL_VER)),2.6)

ifneq ($(KERNELRELEASE),)    # if we can use the kernel build system
	obj-m := $(MODULE)

else                         # otherwise make kernel build system available

EXTRA_CFLAGS := -I$(PWD)/../include

all:
	$(MAKE) -C $(KERNELDIR) M="$(PWD)" EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

check:
	$(MAKE) -C $(KERNELDIR) M="$(PWD)" C=1 EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

install:
	$(MAKE) -C $(KERNELDIR) M="$(PWD)" modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M="$(PWD)" clean
	$(RM) $(RMFLAGS) Module.symvers svn-* *~

endif

else

# Otherwise we're dealing with some pre-2.6 kernel (hopefully 2.4)
# (the -fno-strict-aliasing in CFLAGS is set because of some problems with
# the 2.4 kernel header files withh gcc 3.3.3 on Debian)

INCLUDES := -I$(KERNELDIR)/include -I$(PWD)/../include

CFLAGS   := -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes  \
            -fno-strict-aliasing

all: $(MODULE)

$(MODULE): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(DBGFLAGS) $(INCLUDES) -c -o $@ $<

install:
ifeq ($(RULBUS_EPP_KERNEL_SOURCE),)
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

clean:
	$(RM) $(RMFLAGS) *.o svn- Module.symvers *~

endif
endif
