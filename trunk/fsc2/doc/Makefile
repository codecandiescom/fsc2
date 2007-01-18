#  $Id$
#
#  Copyright (C) 1999-2007 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with fsc2; see the file COPYING.  If not, write to
#  the Free Software Foundation, 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.


docs     := $(wildcard *.texi)
pngs     := $(wildcard fsc2fig*.png) $(wildcard simplecw*.png)
xbms     := $(wildcard $(adir)/cursor*.xbm)
figures  := $(pngs) $(notdir $(patsubst %.xbm,%.png,$(xbms)))
eps_pics := $(patsubst %.png,%.eps,$(figures))
pdf_pics := $(patsubst %.png,%.pdf,$(figures))


.PHONY:  all docs man html ps pdf info

all:
	$(MAKE) -C .. docs

docs:
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
	./texi2html -split section -menu -number -sec_nav -iso fsc2.texi

	@printf '%b\n' "  <div id=\"links\">\n  <img src=\"fsc2.gif\" alt=\"fsc2 logo\">\n  <br><br>" >> TOC
	@for i in fsc2.html fsc2_[1-9].html; do \
		if [    -z "`grep -h '^<title>fsc2: Index:' $$i`" \
			 -a -n "`grep -h '^<h1>' $$i`" ]; then \
			echo -n '  <a href="'$$i'">' >> TOC; \
			grep -h '^<title>fsc2: [^<]*</title>$$' $$i | \
			sed 's/^<title>fsc2: \([^<]*\)<\/title>$$/\1<\/a><br>/' >> TOC; \
		fi; \
	done
	@for i in fsc2_[1-9][0-9].html; do \
		if [    -z "`grep -h '^<title>fsc2: Index:' $$i`" \
			 -a -n "`grep -h '^<h1>' $$i`" ]; then \
			echo -n '  <a href="'$$i'">' >> TOC; \
			grep -h '^<title>fsc2: [^<]*</title>$$' $$i | \
			sed 's/^<title>fsc2: \([^<]*\)<\/title>$$/\1<\/a><br>/' >> TOC; \
		fi; \
	done
	@for i in fsc2_[1-9][0-9][0-9].html; do \
		if [    -z "`grep -h '^<title>fsc2: Index:' $$i`" \
			 -a -n "`grep -h '^<h1>' $$i`" ]; then \
			echo -n '  <a href="'$$i'">' >> TOC; \
			grep -h '^<title>fsc2: [^<]*</title>$$' $$i | \
			sed 's/^<title>fsc2: \([^<]*\)<\/title>$$/\1<\/a><br>/' >> TOC; \
		fi; \
	done
	@echo -n '  <a href="fsc2_toc.html">' >> TOC;
	@grep -h '^<title>fsc2: [^<]*</title>$$' fsc2_toc.html | \
	sed 's/^<title>fsc2: \([^<]*\)<\/title>$$/\1<\/a><br><br>/' >> TOC;
	@echo '  <a href="http://www.physik.fu-berlin.de/~toerring/fsc2.phtml">Download</a><br>' >> TOC
	@echo '  <a href="mailto:jt@toerring.de">Contact</a>' >> TOC
	@printf '%b\n' "  </div>\n  <div id=\"main\">" >> TOC
	@for i in *.html; do \
		sed '1,/body/!d' $$i > $$i.x; \
		cat TOC >> $$i.x; \
		sed '1,/<body>/d;/<\/body/,$$d' $$i >> $$i.x; \
		printf '%b\n' "  </div>\n</body>\n</html>" >> $$i.x; \
		mv $$i.x $$i; \
	done
	-@$(RM) TOC

%.png: $(adir)/%.xbm
	convert -page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null

ps: fsc2.ps.gz

fsc2.ps.gz: $(docs) $(eps_pics)
	@if [ "`eval tex -help 2>/dev/null`" -a           \
		 "`eval dvips --help 2>/dev/null`" ]; then    \
		./texi2dvi -c fsc2.texi &&                    \
		dvips fsc2.dvi -o fsc2.ps;                    \
		gzip -f -9 fsc2.ps;                           \
	else                                              \
		echo "Can't make PostScript documentation, programs tex and/or dvips not found";  \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.eps: $(adir)/%.xbm
	convert $< $@
%.eps: %.png
	convert $< $@
else
%.pdf: $(figures)
	@echo "Can't make PostScript documentation, program 'convert' not found"; \
	exit 1;
endif


pdf: fsc2.pdf

fsc2.pdf: $(docs) $(pdf_pics)
	-@if [ "`eval pdftex --help 2>/dev/null`" ]; then                     \
		./texi2dvi --pdf -c fsc2.texi;                                    \
	else                                                                  \
		echo "Can't make PDF documentation, program 'pdftex' not found";  \
	fi


ifneq ($(word 1,$(shell which convert)),which:)
%.pdf: $(adir)/%.xbm
	convert -compress Zip \
		-page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null
%.pdf: %.png
	convert -compress Zip \
		-page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\{1,\}x[0-9]\{1,\}\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null
else
%.pdf: $(figures)
	@echo "Can't make PDF documentation, program 'convert' not found"; \
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
	-$(RM) $(RMFLAGS) $(docdir)/html/index.html
	ln -s $(docdir)/html/fsc2.html $(docdir)/html/index.html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.css $(docdir)/html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) $(wildcard *.png) $(docdir)/html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) $(wildcard *.gif) $(docdir)/html
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) $(wildcard *.ico) $(docdir)/html
	if [ -f fsc2.ps.gz ]; then                                           \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.ps.gz $(docdir);  \
	fi
	if [ -f fsc2.pdf ]; then                                             \
		$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.pdf $(docdir);    \
	fi
	-install-info --remove --info-dir=$(infodir) fsc2
	-$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) fsc2.info* $(infodir)
	-install-info --info-dir=$(infodir) $(infodir)/fsc2.info
	gzip -9 -f $(infodir)/fsc2.info*


uninstall:
	-$(RM) $(RMFLAGS) $(mandir)/man1/fsc2.1.gz
	-for f in *.html; do                 \
		$(RM) $(RMFLAGS) $(docdir)/$$f;  \
	done
	-for f in *.png; do                  \
		$(RM) $(RMFLAGS) $(docdir)/$$f;  \
	done
	-for f in *.ico; do                  \
		$(RM) $(RMFLAGS) $(docdir)/$$f;  \
	done
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.css
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.ps
	-$(RM) $(RMFLAGS) $(docdir)/fsc2.pdf
	-install-info --remove --infodir=$(infodir) fsc2
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


# How to create a separate packages of the HTML documentation

html_pack:
	-$(RM) $(RMFLAGS) fsc2_html/*
	-rmdir fsc2_html
	if [ ! -e fsc2_html ]; then                           \
		mkdir fsc2_html;                                  \
	fi
	cp -f *.html *.png fsc2.gif *.ico fsc2.css fsc2_html
	cd fsc2_html;
	ln -s fsc2.html index.html;
	cd ..;
	tar -c fsc2_html | gzip -c -9 > fsc2_html.tar.gz
	-$(RM) $(RMFLAGS) fsc2_html/*
	-rmdir fsc2_html


# How to clean up the mess we created

cleanup:
	-$(RM) $(RMFLAGS) fsc2.dvi *.log fsc2fig*.eps fsc2fig*.pdf  \
					  fsc2_html.tar.gz svn-* *~

clean:
	$(MAKE) cleanup
	-$(RM) $(RMFLAGS) fsc2*.html fsc2_*.html *.ps *.eps *.pdf *.info*  \
					  *.gz cursor*.png *_arrow.png
