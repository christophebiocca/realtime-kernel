PATH := /u/wbcowan/gnuarm-4.0.2/libexec/gcc/arm-elf/4.0.2:/u/wbcowan/gnuarm-4.0.2/arm-elf/bin:${PATH}

CC      = gcc
CFLAGS  = -c -fPIC -Wall -Wextra -Werror -I. -mcpu=arm920t -msoft-float -std=gnu99

AS	= as
ASFLAGS	= -mcpu=arm920t -mapcs-32

LD      = ld
LDFLAGS = -init main -Map kernel.map -N -T linker.ld

.SUFFIXES:
.DEFAULT:

.PRECIOUS: %.s

.PHONY: all clean

all = kernel.elf

sources := $(wildcard *.c)
assembled_sources := $(patsubst %c,%s,$(sources))

hand_assemblies := $(filter-out $(assembled_sources),$(wildcard *.s))

objects := $(patsubst %.c,%.o,$(sources)) $(patsubst %.s,%.o,$(hand_assemblies))

kernel.elf : $(objects) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(filter-out linker.ld,$^)

%.s: %.c
	$(CC) -S $(CFLAGS) $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.d : %.c
	@set -e; rm -f $@; \
	    $(CC) -MM $(CFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.s $@ : ,g' > $@;

clean:
	-rm -f kernel.elf $(objects) $(assembled_sources) $(sources:.c=.d) kernel.map

-include $(sources:.c=.d)
