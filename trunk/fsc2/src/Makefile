# $Id$

c_sources      = fsc2.c pulser.c util.c exceptions.c variables.c vars_util.c \
				 func.c func_basic.c func_util.c T.c phases.c exp.c devices.c \
				 gpib.c run.c comm.c chld_func.c graphics.c accept.c \
				 graph_handler_1d.c graph_handler_2d.c loader.c bugs.c print.c

bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
headers        = ${c_sources:.c=.h} global.h

ifeq ($(SIZE),HI_RES)
	c_sources += fsc2_rsc_hr.c
	CFLAGS    += -DSIZE=1
else
	c_sources += fsc2_rsc_lr.c
	CFLAGS    += -DSIZE=0
endif

export headers


.SUFFIXES:
.PHONY: all clean distclean


all: test fsc2 fsc2_clean modules

# make the main program from the c, flex and bison source files

fsc2: test $(objects)
	$(CC) -o $@ $(objects) $(LIBS) $(LDFLAGS)

test:
	if [ `hostname --fqdn` = "pion.anorg.chemie.uni-frankfurt.de" \
		 -a -e gpib_old.c -a -e gpib_old.h ]; then \
		mv gpib.c gpib_new.c; \
		mv gpib.h gpib_new.h; \
		mv gpib_old.c gpib.c; \
		mv gpib_old.h gpib.h; \
	fi;

modules:
	$(MAKE) -C ../modules all

untest:
	if [ `hostname --fqdn` = "pion.anorg.chemie.uni-frankfurt.de" \
		 -a -e gpib_new.c -a -e gpib_new.h ]; then \
		mv gpib.c gpib_old.c; \
		mv gpib.h gpib_old.h; \
		mv gpib_new.c gpib.c; \
		mv gpib_new.h gpib.h; \
	fi;


%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.flex $(headers)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.flex,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@


# make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.flex $(headers)
	$(FLEX) -B -o$@ $<


clean:
	$(RM) $(RMFLAGS) *.o *.output *_parser.c *_parser.h *_lexer.c \
					 fsc2_clean.c fsc2 fsc2_clean mem *~

# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
