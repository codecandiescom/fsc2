# $Id$

simp_modules   = User_Functions.c sr510.c sr530.c sr810.c sr830.c \
				 aeg_s_band.c aeg_x_band.c er035m.c er035m_s.c \
				 er035m_sa.c er035m_sas.c bh15.c
dg2020_f       = dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b       = dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
tds754a        = tds754a.c tds754a_gpib.c tds754a_util.c
tds744a        = tds744a.c tds744a_gpib.c tds744a_util.c
tds520a        = tds520a.c tds520a_gpib.c tds520a_util.c
tds520         = tds520.c tds520_gpib.c tds520_util.c
hp8647a        = hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c
comp_modules   = dg2020_f dg2020_b tds754a tds744a tds520a tds520 hp8647a

src_headers    = $(addprefix ../src/,$(headers))

CFLAGS        += -I../src

ifneq ($(shell hostname -f),pion.anorg.chemie.uni-frankfurt.de)
	simp_modules += egg4402.c
endif


.SUFFIXES:
.PHONY: all clean


all: $(comp_modules:=.so) $(simp_modules:.c=.so)

#$(comp_modules:=.so): $($($(subst .so,,$@)):.c=.o)
#	$(CC) -shared -o $@ $(subst .c,.o,$($(subst .so,,$@))) -lm $(LDFLAGS)
#	chmod 644 $@

#$(comp_modules:.o): $(subst .o,.c,$@) $(src_headers)

dg2020_f.so: ${dg2020_f:.c=.o}
	$(CC) -shared -o $@ ${dg2020_f:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

dg2020_b.so: ${dg2020_b:.c=.o}
	$(CC) -shared -o $@ ${dg2020_b:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

tds754a.so: ${tds754a:.c=.o}
	$(CC) -shared -o $@ ${tds754a:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

tds744a.so: ${tds744a:.c=.o}
	$(CC) -shared -o $@ ${tds744a:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

tds520a.so: ${tds520a:.c=.o}
	$(CC) -shared -o $@ ${tds520a:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

tds520.so: ${tds520:.c=.o}
	$(CC) -shared -o $@ ${tds520:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

hp8647a.so: ${hp8647a:.c=.o}
	$(CC) -shared -o $@ ${hp8647a:.c=.o} -lm $(LDFLAGS)
	chmod 644 $@

%.so: %.o
	$(CC) -shared -o $@ $*.o -lm $(LDFLAGS)
	chmod 644 $@

${dg2020_f:.c=.o}: dg2020_f.h
${dg2020_b:.c=.o}: dg2020_b.h
${tds754a:.c=.o}:  tds754a.h
${tds744a:.c=.o}:  tds744a.h
${tds520a:.c=.o}:  tds520a.h
${tds520:.c=.o}:   tds520.h
${hp8647a:.c=.o}:  hp8647a.h

hp8647a_lexer.c: hp8647a_lexer.flex $(src_header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %lexer.flex,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
			register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@

%.o: %.c $(src_headers)
	$(CC) $(CFLAGS) -c -fpic -o $@ $<


# `clean' deletes just all the files remaining from the compilation while
# `distclean' also removes all executables and shared libraries

clean:
	$(RM) $(RMFLAGS) *.o *_lexer.c *.so TAGS *~


# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
