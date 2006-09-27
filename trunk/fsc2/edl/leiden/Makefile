EDL_J   := $(wildcard *_j.EDL)
EDL_W   := $(wildcard *_w.EDL)
SCRIPTS := $(patsubst %_j.EDL,%.rulhmpc66.molphys.leidenuniv.nl,$(EDL_J)) \
	       $(patsubst %_w.EDL,%.rulhmpc89.molphys.leidenuniv.nl,$(EDL_W))

all: $(SCRIPTS)

%.rulhmpc66.molphys.leidenuniv.nl: %_j.EDL
	fsc2_guify $< $@

%.rulhmpc89.molphys.leidenuniv.nl: %_w.EDL
	fsc2_guify $< $@

install:
	cp *.molphys.leidenuniv.nl ../../scripts

clean:
	rm -f *.molphys.leidenuniv.nl *~
