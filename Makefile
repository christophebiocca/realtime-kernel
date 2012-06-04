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

sources := $(foreach sdir,$(srcdirs),$(wildcard $(sdir)/*.c))
assembled_sources := $(patsubst %c,%s,$(sources))

hand_assemblies := $(filter-out $(assembled_sources),$(wildcard *.s))

objects := $(patsubst %.c,%.o,$(sources)) $(patsubst %.s,%.o,$(hand_assemblies))

deploy: kernel.elf
	install -m 664 -g cs452_05 kernel.elf /u/cs452/tftp/ARM/cs452_05/`whoami`.elf

kernel.elf : $(objects) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(filter-out linker.ld,$^) -lgcc

%.s: %.c
	$(CC) -o $@ -S $(CFLAGS) $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.d : %.c
	@set -e; rm -f $@; \
	    $(CC) -MM $(CFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.s $@ : ,g' > $@;

clean:
	-rm -f kernel.elf $(objects) $(assembled_sources) $(sources:.c=.d) kernel.map

prod: clean
	make CFLAGS="$(CFLAGS) -DPRODUCTION"

-include $(sources:.c=.d)
