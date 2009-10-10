DEVICE_CC = atmega168
DEVICE_DUDE = m168
PROGRAMMER_DUDE = -Pusb -c dragon_isp

AVRDUDE=avrdude
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
CC=avr-gcc
LD=avr-gcc

LDFLAGS=-Wall -g -mmcu=$(DEVICE_CC)
CPPFLAGS=
CFLAGS=-mmcu=$(DEVICE_CC) -Os -Wall -g

OBJS = everavr.o lcd_hardware.o

all : everavr.hex everavr.lst
everavr.bin : $(OBJS)

%.hex : %.bin
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@ || (rm -f $@ ; false )

%.lst : %.bin
	$(OBJDUMP) -S $^ >$@ || (rm -f $@ ; false )

%.bin : %.o
	$(LD) $(LDFLAGS) -o $@ $^

include $(OBJS:.o=.d)

%.d : %.c
	$(CC) -o $@ -MM $^

.PHONY : clean burn
burn : everavr.hex
	$(AVRDUDE) $(PROGRAMMER_DUDE) -p $(DEVICE_DUDE) -U flash:w:$^
clean :
	rm -f *.bak *~ *.bin *.hex *.lst *.o *.d
