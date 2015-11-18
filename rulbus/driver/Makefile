# Makefile for the RULBUS EPP kernel driver
#
# Copyright (C) 2003-2011  Jens Thoms Toerring
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
PWD      := $(shell pwd)


# Unless the user set the environment variable RULBUS_EPP_KERNEL_SOURCE
# assume that we can find out where the kernel sources are (or at least
# the kernel headers) from the 'build' symbolic link in the module
# directory of the currently running kernel. Only if this doesn't exist
# use '/usr/src/linux/' as a last resort - but it's not unlikely to point
# to the wrong kernel code or may even not exist...

ifneq ($(RULBUS_EPP_KERNEL_SOURCE),)
KERNEL_DIR := $(RULBUS_EPP_KERNEL_SOURCE)
endif

ifneq ($(KERNELRELEASE),)
KERNEL_DIR := /lib/modules/$(KERNELRELEASE)/build
KERNEL_VER := $(KERNELRELEASE)
else
KERNEL_DIR := $(shell if [ -d /lib/modules/`uname -r`/build ]; then \
					      echo "/lib/modules/`uname -r`/build";     \
					  else                                          \
						  echo "/usr/src/linux";                    \
					  fi )
endif

# If KERNEL_VER isn't set determine it from the the kernel headers (it used
# to be defined in linux/version.h but on new systems its' now in its own
# file linux/utsrelease.h and on even newer in generated/utsrelease.h) and
# then call make again with KERNEL_VER now being set. If KERNEL_VER could
# not be found out that way print an error message and bail out.

ifndef KERNEL_VER
KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'          \
                            $(KERNEL_DIR)/include/linux/utsrelease.h    \
				            2>/dev/null |                               \
	            	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'            \
	                            $(KERNEL_DIR)/include/generated/utsrelease.h  \
					            2>/dev/null |                                 \
		            	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell grep 'define[ \t]\+UTS_RELEASE[ \t]\+\"'          \
		                        $(KERNEL_DIR)/include/linux/version.h       \
					            2>/dev/null |                               \
	        	    	  sed 's/^[^"]\+"\([^"]\+\)".*$$/\1/' 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
	KERNEL_VER := $(shell make --quiet -C $(KERNEL_DIR) kernelrelease 2>/dev/null )
endif

ifeq ($(KERNEL_VER),)
all $(MODULE) install check clean:
	@echo "Error: Can't figure out kernel version under the assumption that the kernel headers (or sources) are at $(KERNEL_DIR)"
	exit 2

else      # if KERNEL_VER is set

# First handle the case that we're building for a 2.6 kernel or newer

ifneq ($(shell echo $(KERNEL_VER) | grep '^\(2\.6\)\|3\.\|4\.' 2>/dev/null),)

ifneq ($(KERNELRELEASE),)    # if invoked from dkms
obj-m := $(MODULE)
endif

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

modules_install install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	$(RM) $(RMFLAGS) Module.symvers Module.markers modules.order *~


else   # pre 2.6 kernels (hopefully 2.4)

# The -fno-strict-aliasing in CFLAGS is set because of some problems with
# the 2.4 kernel header files with gcc 3.3.3 on Debian

SOURCES  := rulbus_epp.c
HEADERS  := rulbus_epp.h rulbus_epp_autoconf.h
INCLUDES := -I$(KERNEL_DIR)/include

CFLAGS   := -D__KERNEL__ -DMODULE -DLINUX -O2 -Wall -Wstrict-prototypes \
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

clean:
	$(RM) $(RMFLAGS) *.o svn- Module.symvers *~
endif
endif
endif
