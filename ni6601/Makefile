# Makefile for NI6601 driver and library
#
# Copyright (C) 2002-2014  Jens Thoms Toerring
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


SHELL   := /bin/sh
CC      := gcc
INSTALL := install
RM      := -rm
RMFLAGS := -f

prefix  = $(shell if [ -e driver/ni6601_autoconf.h ]; then           \
                      grep NI6601_PREFIX driver/ni6601_autoconf.h |  \
                      sed 's/^[^\"]*\"\([^\"]*\).*$$/\1/';    \
                  else                                        \
                      echo "/usr/local";                      \
                  fi)

export 


.SUFFIXES:
.PHONY: all driver lib install install_lib install_driver cleanup clean

all:
	$(MAKE) driver
	$(MAKE) lib

driver:
	@if [ ! -e driver/ni6601_autoconf.h ]; then                        \
		echo "***************************************************";  \
		echo "*   The file driver/ni6601_autoconf.h is missing. *";  \
		echo "*    Please run './INSTALL' to create it.         *";  \
		echo "***************************************************";  \
		exit 2;                                                 \
	fi
	$(MAKE) -C driver

lib:
	$(MAKE) -C lib

install:
	$(MAKE) install_lib
	$(MAKE) install_driver

install_lib:
	$(MAKE) -C driver install

install_driver:
	$(MAKE) -C lib install

cleanup:
	$(MAKE) -C driver cleanup
	$(MAKE) -C lib cleanup
	$(RM) $(RMFLAGS) makelog *~ 

clean:
	$(MAKE) -C driver clean
	$(MAKE) -C lib clean
	$(RM) $(RMFLAGS) driver/ni6601_autoconf.h makelog *~
