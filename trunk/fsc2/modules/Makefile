# $Id$

modules        = User_Functions.c sr510.c sr530.c sr810.c sr830.c \
				 aeg_s_band.c aeg_x_band.c er035m.c er035m_s.c \
				 er035m_sa.c bh15.c
dg2020_f       = dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b       = dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
tds754a        = tds754a.c tds754a_gpib.c tds754a_util.c
tds520a        = tds520a.c tds520a_gpib.c tds520a_util.c
tds520         = tds520.c tds520_gpib.c tds520_util.c
hp8647a        = hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c

src_headers    = $(addprefix ../src/,$(headers))

CFLAGS        += -I../src


.SUFFIXES:
.PHONY: all clean


all: dg2020_f.so dg2020_b.so tds754a.so tds520a.so tds520.so hp8647a.so \
	 $(modules:.c=.so)

dg2020_f.so: ${dg2020_f:.c=.lo}
	$(CC) -shared -o $@ ${dg2020_f:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

dg2020_b.so: ${dg2020_b:.c=.lo}
	$(CC) -shared -o $@ ${dg2020_b:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds754a.so: ${tds754a:.c=.lo}
	$(CC) -shared -o $@ ${tds754a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds520a.so: ${tds520a:.c=.lo}
	$(CC) -shared -o $@ ${tds520a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds520.so: ${tds520:.c=.lo}
	$(CC) -shared -o $@ ${tds520:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

hp8647a.so: ${hp8647a:.c=.lo}
	$(CC) -shared -o $@ ${hp8647a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

%.so: %.lo
	$(CC) -shared -o $@ $*.lo -lm $(LDFLAGS)
	chmod 644 $@

%.o: %.c $(src_headers)
	$(CC) $(CFLAGS) -c -o $@ $<

${dg2020_f:.c=.lo}: dg2020_f.h $(src_headers)
${dg2020_b:.c=.lo}: dg2020_b.h $(src_headers)
${tds754a:.c=.lo}:  tds754a.h  $(src_headers)
${tds520a:.c=.lo}:  tds520a.h  $(src_headers)
${tds520:.c=.lo}:   tds520.h   $(src_headers)
${hp8647a:.c=.lo}:  hp8647a.h  $(src_headers)

hp8647a_lexer.c: hp8647a_lexer.flex $(src_header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %lexer.flex,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@

%.lo: %.c $(src_headers)
	$(CC) $(CFLAGS) -c -fPIC $< -o $*.lo


# `clean' deletes just all the files remaining from the compilation while
# `distclean' also removes all executables and shared libraries

clean:
	$(RM) $(RMFLAGS) *.o *_lexer.c *.lo *.so TAGS *~

# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
