#  Library and driver for Rulbus (Rijksuniversiteit Leiden BUS)
#
#  Copyright (C) 2003-2014 Jens Thoms Toerring
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
#  To contact the author send email to jt@toerring.de

SHELL   = /bin/sh
CC      = gcc
INSTALL = install
RM      = -rm
RMFLAGS = -f

prefix  = $(shell if [ -e driver/rulbus_epp_autoconf.h ]; then \
					  grep RULBUS_EPP_PREFIX driver/rulbus_epp_autoconf.h | \
					  sed 's/^[^"]*"\([^"]*\)".*$$/\1/'; \
				  else \
					  echo "/usr/local"; \
				  fi)

lib_dir   = $(prefix)/lib
incl_dir  = $(prefix)/include

export 


.SUFFIXES:
.PHONY: all driver lib install install_lib install_driver clean


all:
	$(MAKE) driver
	$(MAKE) lib


driver:
	@if [ ! -e driver/rulbus_epp_autoconf.h ]; then \
		echo "*************************************************"; \
		echo "* The file driver/rulbus_epp_autoconf.h is mis- *"; \
		echo "* sing. Please run './INSTALL' to create it.    *"; \
		echo "*************************************************"; \
		exit 2; \
	fi
	$(MAKE) -C driver


lib:
	$(MAKE) -C lib


install:
	$(MAKE) install_lib
	$(MAKE) install_driver


install_lib:
	$(INSTALL) -d $(incl_dir)
	$(INSTALL) -m 644 lib/rulbus.h $(incl_dir)
	$(MAKE) -C lib install


install_driver:
	$(MAKE) -C driver install


clean:
	$(MAKE) -C driver clean
	$(MAKE) -C lib clean
	$(RM) $(RMFLAGS) makelog driver/rulbus_epp_autoconf.h */*~ *~
