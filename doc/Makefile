#  Copyright (C) 1999-2014 Jens Thoms Toerring
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


ifeq ($(adir),)
	adir := ../aux
endif
docs     := $(wildcard *.texi)
pngs     := $(wildcard fsc2fig*.png) $(wildcard simplecw*.png)
xbms     := $(wildcard $(adir)/cursor*.xbm)
figures  := $(pngs) $(notdir $(patsubst %.xbm,%.png,$(xbms)))
eps_pics := $(patsubst %.png,%.eps,$(figures))
pdf_pics := $(patsubst %.png,%.pdf,$(figures))


# Variable with shell script that tests if the programs passed to it
# exist - bails out on the first missing

TFA = test_for_apps( ) {                    \
	for var in "$$@"; do                    \
		if [ -z `command -v $$var` ]; then  \
			echo "$$var";                   \
			return;                         \
		fi                                  \
	done                                    \
}

.PHONY:  all docs man html ps pdf info

all:
	$(MAKE) -C .. docs

docs:
	$(MAKE) man
	$(MAKE) info
	$(MAKE) html
	$(MAKE) ps
	$(MAKE) pdf


# Rules for making the man page

man: fsc2.1.gz

fsc2.1.gz: fsc2.1
	sed 's/DOCDIR/$(subst /,\/,$(docdir))/' fsc2.1 | gzip -9 -c > fsc2.1.gz


# Rules for making the documentation in info format

info: fsc2.info

fsc2.info: $(docs)
	@$(TFA);                          \
	res="`test_for_apps makeinfo`";   \
	if [ -z "$$res" ]; then           \
		echo "makeinfo fsc2.texi";    \
		LC_ALL=C makeinfo fsc2.texi;  \
	else                              \
		echo "\nERROR: Can't create info documentation, program '$$res' not found\n"; \
	fi


# Rules for making the HTML documentation with PNG images

html: fsc2.html

fsc2.html: $(docs) $(figures)
	@$(TFA);                                                 \
	res="`test_for_apps texi2html convert identify`";        \
	if [ -z "$$res" ]; then                                  \
		echo "texi2html --init-file=fsc2.init fsc2.texi";    \
		LC_ALL=C texi2html --init-file=fsc2.init fsc2.texi;  \
	else                                                     \
		echo "\nERROR: Can't create HTML documentation, program '$$res' not found\n"; \
	fi

%.png: $(adir)/%.xbm
	@$(TFA); \
	res="`test_for_apps convert identify`"; \
	if [ -z $$res ]; then  \
		convert -page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\+x[0-9]\+\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null; \
	fi


# Rules for making the PostScript documentation with EPS images

ps: fsc2.ps.gz

fsc2.ps.gz: $(docs) $(eps_pics)
	@$(TFA);                                                        \
	res="`test_for_apps texi2dvi dvips convert identify`";          \
	if [ -z $$res ]; then                                           \
		echo "texi2dvi --ps -c fsc2.texi && gzip -f -9 fsc2.ps";    \
		LC_ALL=C texi2dvi --ps -c fsc2.texi && gzip -f -9 fsc2.ps;  \
	else                                                            \
		echo "\nERROR: Can't create PostScript documentation, program '$$res' not found\n"; \
	fi

%.eps: $(adir)/%.xbm
	@$(TFA); \
	res="`test_for_apps convert`"; \
	if [ -z $$res ]; then \
		convert $< $@; \
	fi

%.eps: %.png
	@$(TFA); \
	res="`test_for_apps convert`"; \
	if [ -z $$res ]; then \
		convert $< $@; \
	fi


# Rules for making the PDF documentation with PDF images

pdf: fsc2.pdf

fsc2.pdf: $(docs) $(pdf_pics)
	@$(TFA);                                          \
	res="`test_for_apps texi2dvi convert identify`";  \
	if [ -z $$res ]; then                             \
		echo "texi2dvi --pdf -c fsc2.texi";           \
		LC_All=C texi2dvi --pdf -c fsc2.texi;         \
	else                                              \
		echo "\nERROR: Can't create PDF documentation, program '$$res' not found\n";  \
	fi

%.pdf: $(adir)/%.xbm
	@$(TFA);                                \
	res="`test_for_apps convert identify`"; \
	if [ -z $$res ]; then                   \
		convert -compress Zip \
				-page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\+x[0-9]\+\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null; \
	fi

%.pdf: %.png
	@$(TFA); \
	res="`test_for_apps convert identify`"; \
	if [ -z $$res ]; then \
		convert -compress Zip \
				-page `identify $< 2>/dev/null | sed 's/^.*[ 	]\([0-9]\+x[0-9]\+\)[ 	+-].*$$/\1/'` $< $@ 2>/dev/null; \
	fi


# How to install the documentation

install:
	$(INSTALL) -d $(docdir)
	$(INSTALL) -d $(docdir)/html
	$(INSTALL) -d $(infodir)
	$(INSTALL) -d $(mandir)/man1
	$(INSTALL) -m 444 fsc2.1.gz $(mandir)/man1
	$(INSTALL) -m 444 -o $(OWNER) -g $(GROUP) *.html $(docdir)/html
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
	cp -f *.html *.png background.gif fsc2.gif *.ico fsc2.css fsc2_html
	cd fsc2_html;
	if [ ! -e index.html ]; then                          \
		ln -s fsc2.html index.html;                       \
	fi
	cd ..;
	tar -c fsc2_html | gzip -c -9 > fsc2_html.tar.gz
	-$(RM) $(RMFLAGS) fsc2_html/*
	-rmdir fsc2_html


# How to clean up the mess we created

cleanup:
	-$(RM) $(RMFLAGS) fsc2.dvi *.log fsc2fig*.eps fsc2fig*.pdf  \
					  fsc2_html.tar.gz *~

clean:
	$(MAKE) cleanup
	-$(RM) $(RMFLAGS) *.html *.ps *.eps *.pdf *.info*   \
					  *.gz cursor*.png *_arrow.png
