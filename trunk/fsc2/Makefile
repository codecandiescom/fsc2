bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
c_sources      = fsc2.c assign.c util.c exceptions.c variables.c func.c T.c \
				 phases.c ppcheck.c pulse.c prim_exp.c devices.c gpib.c
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
			     -Wmissing-prototypes -Wmissing-declarations -W \
				 -DDEBUG \
				 # -DMDEBUG   # for malloc debugging
modules        = User_Functions.c dg2020.c tds754a.c sr510.c s_band.c \
				 er035m.c
header         = ${c_sources:.c=.h} global.h


.PHONY: all
all: tags fsc2 mods

fsc2: $(objects) fsc2_clean $(header)
	gcc -o $@ $(objects) -Wl,-E -lm -ldl -forms -lX11 -lgpib -lfl

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

%.c: %.y $(header)
	bison -d -v -t -p $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.flex $(header)
	flex -B -P$(patsubst %_lexer.flex,%,$<) -o$@ $<

mods: $(modules:.c=.so)
#	export LD_LIBRARY_PATH=$(shell pwd)

%.so: %.c $(header)
	gcc $(CFLAGS) -c -fPIC -DPIC $< -o $*.lo
	gcc -shared -o $@ $*.lo -Wl,-E -lm
	chmod 644 $@


fsc2_clean: fsc2_clean.o exceptions.o util.o T.o util.o
	gcc $(CFLAGS) -O2 -lm -o $@ $^

fsc2_clean.c: fsc2_clean.flex $(header)
	flex -B -o$@ $<


tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(header)


clean: 
	-rm *.o fsc2_clean fsc2 *.output *_parser.c *_parser.h *_lexer.c \
	    fsc2_clean.c *.lo *.so TAGS mem


# don't delete intermediate files created by bison and flex

.PRECIOUS: %.c