# $Id$
#
# Copyright (C) 1999-2002 Jens Thoms Toerring
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


docs     := $(wildcard *.texi)
pngs     := $(wildcard fsc2fig*.png) interfacing.png
xbms     := $(wildcard $(adir)/cursor*.xbm)
figures  := $(pngs) $(notdir $(patsubst %.xbm,%.png,$(xbms)))
eps_pics := $(patsubst %.png,%.eps,$(figures))
pdf_pics := $(patsubst %.png,%.pdf,$(figures))


all:
	$(MAKE) man
	$(MAKE) html
	$(MAKE) ps
	$(MAKE) pdf
	$(MAKE) info


man: fsc2.1.gz

fsc2.1.gz: fsc2.1
	sed 's/DOCDIR/$(subst /,\/,$(docdir))/' fsc2.1 | gzip -9 -c > fsc2.1.gz


info: fsc2.info

fsc2.info: $(docs)
	makeinfo fsc2.texi


html: fsc2.html

fsc2.html: $(docs) $(figures)
	./texi2html -frames -split chapter -menu -number -sec_nav -iso fsc2.texi
	sed -e 's/fsc2_[0-9]\{1,\}/fsc2/' fsc2_frame.html > fsc2_frame.x
	mv fsc2_frame.x fsc2_frame.html

%.png: $(adir)/%.xbm
	convert -page `identify $< 2>/dev/null | sed -e 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	].*$$/\1/'` $< $@ \
    2>/dev/null

ps: fsc2.ps

fsc2.ps: $(docs) $(eps_pics)
	@if [ "`eval tex -help 2>/dev/null`" -a         \
		 "`eval dvips --help 2>/dev/null`" ]; then  \
		./texi2dvi -c fsc2.texi;                    \
		dvips fsc2.dvi -o fsc2.ps;                  \
	else                                            \
		echo "Can't make PostScript documentation, programs tex and/or dvips not found."; \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.eps: $(adir)/%.xbm
	convert $< $@
%.eps: %.png
	convert $< $@
else
%.pdf: $(figures)
	@echo "Can't make PostScript documentation, program 'convert' not found."; \
	exit 1;
endif


pdf: fsc2.pdf

fsc2.pdf: $(docs) $(pdf_pics)
	-@if [ "`eval pdftex --help 2>/dev/null`" ]; then  \
		./texi2dvi --pdf -c fsc2.texi; \
	else \
		echo "Can't make PDF documentation, program 'pdftex' not found."; \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.pdf: $(adir)/%.xbm
	convert -compress Zip \
		-page `identify $< 2>/dev/null | sed -e 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	].*$$/\1/'` $< $@ 2>/dev/null
%.pdf: %.png
	convert -compress Zip \
		-page `identify $< 2>/dev/null | sed -e 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	].*$$/\1/'` $< $@ 2>/dev/null
else
%.pdf: $(figures)
	@echo "Can't make PDF documentation, program 'convert' not found."; \
	exit 1;
endif



# How to install the documentation

install:
	$(INSTALL) -d $(docdir)
	$(INSTALL) -d $(docdir)/html
	$(INSTALL) -d $(infodir)
	$(INSTALL) -d $(mandir)/man1
	$(INSTALL) -m 444 fsc2.1.gz $(mandir)/man1
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2*.html $(docdir)/html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.css $(docdir)/html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) $(wildcard *.png) $(docdir)/html
	if [ -f fsc2.ps ]; then \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.ps $(docdir); \
	fi
	if [ -f fsc2.pdf ]; then \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.pdf $(docdir); \
	fi
	-install-info -d $(infodir)/dir -r fsc2
	-$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.info* $(infodir)
	gzip -9 -f $(infodir)/fsc2.info*
	-install-info -d $(infodir)/dir $(infodir)/fsc2.info 


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
	-install-info -d $(infodir)/dir -r fsc2
	-$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	-if [ -z "`ls -A $(docdir)`" ]; then   \
		rmdir $(docdir);                   \
	fi
	-if [ -z "`ls -A $(infodir)`" ]; then  \
		rmdir $(infodir);                  \
	fi
	-if [ -z "`ls -A $(mandir)`" ]; then   \
		rmdir $(mandir);                   \
	fi


# How to clean up the mess we created

clean:
	-$(RM) $(RMFLAGS) fsc2.dvi *.log fsc2fig*.eps fsc2fig*.pdf

distclean:
	$(MAKE) clean
	-$(RM) $(RMFLAGS) fsc2*.html fsc2_*.html *.ps *.eps *.pdf *.info* *~ \
					  *.gz cursor*.png *_arrow.png
