# $Id$
#
# Copyright (C) 2001 Jens Thoms Toerring
#
# This file is part of fsc2.
#
# Fsc2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# Fsc2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with fsc2; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


docs    = $(wildcard *.texi)
figures = $(wildcard *.png)


all:
	$(MAKE) man
	$(MAKE) info
	$(MAKE) html
	$(MAKE) ps
	$(MAKE) pdf


man: fsc2.1.gz

fsc2.1.gz: fsc2.1
	sed 's/DOCDIR/$(subst /,\/,$(docdir))/' fsc2.1 | gzip -9 -c > fsc2.1.gz


info: fsc2.info

fsc2.info: $(docs)
	makeinfo fsc2.texi


html: fsc2.html

fsc2.html: $(docs)
	./texi2html -frames -split chapter -menu -number -sec_nav -iso fsc2.texi
	sed -e 's/fsc2_[0-9]\+/fsc2/' fsc2_frame.html > fsc2_frame.x
	mv fsc2_frame.x fsc2_frame.html


ps: fsc2.ps

fsc2.ps: $(docs) $(patsubst %.png,%.eps,$(figures))
	@if [ "`eval tex -help 2>/dev/null`" -a         \
		 "`eval dvips --help 2>/dev/null`" ]; then  \
		./texi2dvi -c fsc2.texi;                    \
		dvips fsc2.dvi;                             \
	else                                            \
		echo                                        \
		"Can't make PS documentation, programs tex and/or dvips not found."; \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.eps: %.png
	convert $< $@
else
%.eps: %.png
	pngtopnm $< | pnmtops -nocenter -noturn > $@
endif


pdf: fsc2.pdf

fsc2.pdf: $(docs) $(patsubst %.png,%.pdf,$(figures))
	-@if [ "`eval pdftex --help 2>/dev/null`" ]; then \
		./texi2dvi --pdf -c fsc2.texi; \
	else \
		echo "Can't make PDF documentation, program 'pdftex' not found."; \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.pdf: %.png
	convert -compress Zip $< $@
else
%.pdf: %.png
	@echo "Can't make PDF documentation, program 'convert' not found."; \
	exit 1;
endif


# How to install the documentation

install:
	$(INSTALL) -d $(docdir)
	$(INSTALL) -d $(infodir)
	$(INSTALL) -d $(mandir)/man1
	$(INSTALL) -m 444 fsc2.1.gz $(mandir)/man1
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2*.html $(docdir)
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.css $(docdir)
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2fig*.png $(docdir)
	if [ -f fsc2.ps ]; then \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.ps $(docdir); \
	fi
	if [ -f fsc2.pdf ]; then \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.pdf $(docdir); \
	fi
	install-info --delete fsc2 $(infodir)/dir
	-$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.info* $(infodir)
	gzip -9 -f $(infodir)/fsc2.info*
	install-info $(infodir)/fsc2.info $(infodir)/dir


uninstall:
	-$(RM) $(RMFLAGS) $(mandir)/man1/fsc2.1.gz
	-for f in *.html; do                 \
		$(RM) $(RMFLAGS) $(docdir)/$$f;  \
	done
	-for f in *.png; do                  \
		$(RM) $(RMFLAGS) $(docdir)/$$f;  \
	done
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.css
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.ps
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.pdf
	-install-info --delete fsc2 $(infodir)/dir
	-$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	-if [ -z "`ls -a $(docdir)`" ]; then   \
		rmdir $(docdir);                   \
	fi
	-if [ -z "`ls -a $(infodir)`" ]; then  \
		rmdir $(infodir);                  \
	fi
	-if [ -z "`ls -a $(mandir)`" ]; then   \
		rmdir $(mandir);                   \
	fi


# How to clean up the mess we created

clean:
	-$(RM) $(RMFLAGS) fsc2.dvi *.log fsc2fig*.eps fsc2fig*.pdf

distclean:
	$(MAKE) clean
	-$(RM) $(RMFLAGS) fsc2*.html fsc2_*.html *.ps *.eps *.pdf *.info* *~ *.gz
