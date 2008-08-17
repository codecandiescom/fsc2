EDL      := $(wildcard *.EDL)
scripts  := $(patsubst %.EDL,%,$(EDL)) monitor_sr monitor_bruker


all: $(patsubst %.EDL,%,$(EDL))

%: %.EDL
	../../utils/fsc2_guify $< $@

install:
	@for f in $(scripts); do       \
		if [    $$f -nt ../../scripts/$$f.aiesr6.hmi-domain       \
			 -o ! -e ../../scripts/$$f.aiesr6.hmi-domain ]; then  \
			echo "cp $$f ../../scripts/$$f.aiesr6.hmi-domain";    \
	    	cp $$f ../../scripts/$$f.aiesr6.hmi-domain;           \
		fi;                                                       \
	done

clean:
	rm -f $(patsubst %.EDL,%,$(EDL)) *~
