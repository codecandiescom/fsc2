bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
c_sources      = fsc2.c assign.c util.c exceptions.c variables.c func.c T.c \
				 phases.c ppcheck.c pulse.c prim_exp.c devices.c
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
			     -Wmissing-prototypes -Wmissing-declarations \
				 -W -DDEBUG -DMDEBUG
modules        = User_Functions.c dg2020.c tds754a.c sr510.c
header         = ${c_sources:.c=.h} global.h


.PHONY: all
all: tags fsc2 mods



fsc2: $(objects) fsc_clean $(header)
	gcc -o $@ $(objects) -Wl,-E -lm -ldl

split_lexer.c: split_lexer.flex $(header)
	flex -B -Psplit -osplit_lexer.c split_lexer.flex

devices_parser.c devices_parser.h: devices_parser.y
	bison -d -v -t -p devices -o devices_parser.c devices_parser.y

devices_lexer.c: devices_lexer.flex $(header)
	flex -B -Pdevices -odevices_lexer.c devices_lexer.flex

devices_list_lexer.c: devices_list_lexer.flex $(header)
	flex -B -Pdevices_list -odevices_list_lexer.c devices_list_lexer.flex

assign_parser.c assign_parser.h: assign_parser.y
	bison -d -v -t -p assign -o assign_parser.c assign_parser.y

assign_lexer.c: assign_lexer.flex $(header)
	flex -B -Passign -oassign_lexer.c assign_lexer.flex

vars_parser.c vars_parser.h: vars_parser.y $(header)
	bison -d -v -t -p variables -o vars_parser.c vars_parser.y

vars_lexer.c: vars_lexer.flex $(header)
	flex -B -Pvariables -ovars_lexer.c vars_lexer.flex

phases_parser.c phases_parser.h: phases_parser.y $(header)
	bison -d -v -t -p phases -o phases_parser.c phases_parser.y

phases_lexer.c: phases_lexer.flex $(header)
	flex -B -Pphases -ophases_lexer.c phases_lexer.flex

preps_parser.c preps_parser.h: preps_parser.y $(header)
	bison -d -v -t -p preps -o preps_parser.c preps_parser.y

preps_lexer.c: preps_lexer.flex $(header)
	flex -B -Ppreps -opreps_lexer.c preps_lexer.flex

prim_exp_run_parser.c: prim_exp_run_parser.y $(header)
	bison -v -t -p prim_exp_run -o prim_exp_run_parser.c prim_exp_run_parser.y

prim_exp_lexer.c: prim_exp_lexer.flex $(header)
	flex -B -Pprim_exp -oprim_exp_lexer.c prim_exp_lexer.flex

condition_parser.c: condition_parser.y $(header)
	bison -v -t -p condition -o condition_parser.c condition_parser.y

func_list_lexer.c: func_list_lexer.flex $(header)
	flex -B -Pfunc_list -ofunc_list_lexer.c func_list_lexer.flex

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

fsc_clean: fsc_clean.o exceptions.o util.o T.o util.o variables.o
	gcc $(CFLAGS) -O2 -lm -o $@ fsc_clean.o exceptions.o util.o T.o variables.o

fsc_clean.c: fsc_clean.flex $(header)
	flex -B -ofsc_clean.c fsc_clean.flex



mods: $(modules:.c=.so)
	export LD_LIBRARY_PATH=`pwd`

%.so: %.c $(header)
	gcc $(CFLAGS) -c -fPIC -DPIC $< -o $*.lo
	gcc -shared -o $@ $*.lo -Wl,-E -lm
	chmod 644 $@

#mods: $(patsubst %,lib_fsc2_%, $(modules:.c=.la))
#	export LD_LIBRARY_PATH=`pwd`/.libs

#lib_fsc2_%.la: %.c $(header)
#	libtool gcc $(CFLAGS) -c $<
#	libtool gcc -module -o $@ $*.lo -rpath /usr/local/lib -lm



clean: 
	-rm *.o fsc_clean fsc2 *.output *_parser.c *_parser.h *_lexer.c \
	    fsc_clean.c *.lo *.so TAGS mem

tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(header)
