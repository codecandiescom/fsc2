#
#  Copyright (C) 2015 Jens Thoms Toerring
#
#  This file is part of Fsc3.
#
#  Fsc3 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
#  Fsc3 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


CXX           := g++
CXXFLAGS      := -std=c++11 -ggdb -Wall -Wextra -O3 -march=native
CC            := gcc
CFLAGS        := -ggdb -O3

SOURCES       := rs_rto.cpp rs_rto_base.cpp rs_rto_trig.cpp rs_rto_chans.cpp \
				 rs_rto_chan.cpp rs_rto_ext_chan.cpp rs_rto_inp_chan.cpp \
				 rs_rto_math_chan.cpp rs_rto_acq.cpp \
				 VXI11_Client.cpp Client_Logger.cpp
HEADERS       := $(SOURCES:.cpp=.hpp) vxi11.h except.hpp \
				 Enum_Mapper.hpp
VXI11_SOURCES := vxi11_clnt.c vxi11_xdr.c
OBJECTS       := $(SOURCES:.cpp=.o) $(VXI11_SOURCES:.c=.o)

.SUFFIXES:
.PHONY: all install uninstall clean

all: librs_rto_c.so

# Create the VXI-11 C files etc. from the vxi11.x template file

$(VXI11_SOURCES) vxi11.h: vxi11.x
	rpcgen -N -M $<

%.o: %.c vxi11.h
	$(CC) $(CFLAGS) $(LFLAGS) -c -o $@ $<


%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) -c -o $@ $<


librs_rto_c.so: rs_rto_c.o $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) -o $@ $^


install:
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) librs_rto_c.so $(ldir)


uninstall:
	$(RM) -f $(ldir)/librs_rto_c.so


clean:
	$(RM) -f $(VXI11_SOURCES) vxi11_svc.c vxi11.h *~ *.o librs_rto_c.so
