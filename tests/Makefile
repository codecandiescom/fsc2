# Copyright (C) 1999-2011  Jens Thoms Toerring
#
# This file is part of fsc2.
#
# Fsc2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# Fsc2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


all:
	-@export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$(sdir):$(mdir):$(cdir); \
	for f in *.edl; do                                                  \
		echo "Running $$f";                                             \
		$(fdir)/src/fsc2 -X2 $$f;                                       \
	done
