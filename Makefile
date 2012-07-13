PATH := /u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2:/u/wbcowan/gnuarm-4.0.2/arm-elf/bin:${PATH}

CC      = gcc
CFLAGS  = -c -fPIC -Wall -Wextra -Werror -I. -Iinclude -mcpu=arm920t -msoft-float -std=gnu99

AS	= as
ASFLAGS	= -mcpu=arm920t -mapcs-32

LD      = ld
LDFLAGS = -init main -Map kernel.map -N -T linker.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2

.SUFFIXES:
.DEFAULT:

.PRECIOUS: %.s

.PHONY: clean

srcdirs = kernel user
vpath %.h include include/kernel include/user
vpath %.c $(srcdirs)

generated_sources = user/track_data.c
generated_headers = include/user/track_data.h

sources := $(foreach sdir,$(srcdirs),$(wildcard $(sdir)/*.c)) $(generated_sources)
assembled_sources := $(patsubst %c,%s,$(sources))

hand_assemblies := $(filter-out $(assembled_sources),$(wildcard *.s))

objects := $(patsubst %.c,%.o,$(sources)) $(patsubst %.s,%.o,$(hand_assemblies))

deploy: kernel.elf
	install -m 664 -g cs452_05 kernel.elf /u/cs452/tftp/ARM/cs452_05/`whoami`.elf

kernel.elf: $(objects) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(filter-out linker.ld,$^) -lgcc

user/track_data.c: data/parse_track.py data/tracka data/trackb
	data/parse_track.py -C user/track_data.c -H include/user/track_data.h -S 77 data/tracka data/trackb

%.cpp: %.c
	$(CC) -o $@ -E $(CFLAGS) $<

%.s: %.c
	$(CC) -o $@ -S $(CFLAGS) $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.d : %.c
	@set -e; rm -f $@; \
	    $(CC) -MM $(CFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.s $@ : ,g' > $@;

clean:
	-rm -f kernel.elf $(objects) $(assembled_sources) $(sources:.c=.d) kernel.map $(generated_sources) $(generated_headers)

submit: prod
	mkdir -p ~/cs452/$(DIR)/src/
	mv kernel.elf ~/cs452/$(DIR)/
	git archive HEAD --format=tar | tar -x -C ~/cs452/$(DIR)/src/

prod: clean
	make CFLAGS="$(CFLAGS) -DPRODUCTION -Wno-error"

-include $(sources:.c=.d)
