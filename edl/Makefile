#  Copyright (C) 1999-2012 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.


all:
	for d in `ls`; do                            \
		if [ -d $$d -a -r $$d/Makefile ]; then   \
			$(MAKE) -C $$d;                      \
		fi                                       \
	done

install:
	if [    -d $(machine_name)                   \
	     -a -r $(machine_name)/Makefile ]; then  \
		$(MAKE) -C $(machine_name) install;      \
	fi

uninstall:
	if [    -d $(machine_name)                   \
	     -a -r $(machine_name)/Makefile ]; then  \
		$(MAKE) -C $(machine_name) uninstall;    \
	fi

clean:
	for d in `ls`; do                            \
		if [ -d $$d -a -r $$d/Makefile ]; then   \
			$(MAKE) -C $$d clean;                \
		fi                                       \
	done
