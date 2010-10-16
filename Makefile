DEVICE_CC = atmega168
DEVICE_DUDE = m168
PROGRAMMER_DUDE = -Pusb -c dragon_isp

# where to find a recent USBDRV checkout
VUSB = ../vusb-20100715/usbdrv
# our MCU runs with an external XTAL @ 18 MHz
F_CPU=18000000

AVRDUDE=avrdude
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
CC=avr-gcc
LD=avr-gcc

LDFLAGS=-Wall -g -mmcu=$(DEVICE_CC)
CPPFLAGS=-I. -I$(VUSB) -DF_CPU=$(F_CPU)
CFLAGS=-mmcu=$(DEVICE_CC) -Os -Wall -g
ASFLAGS=$(CFLAGS)

OBJS = usbdrv.o usbdrvasm.o everavr.o lcd_hardware.o 

VPATH = $(VUSB)

all : everavr.hex everavr.lst
everavr.bin : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.hex : %.bin
	$(OBJCOPY) -j .text -j .data -O ihex $^ $@ || (rm -f $@ ; false )

%.lst : %.bin
	$(OBJDUMP) -S $^ >$@ || (rm -f $@ ; false )

include $(OBJS:.o=.d)

%.d : %.c
	$(CC) $(CPPFLAGS) -o $@ -MM $^

%.d : %.S
	$(CC) $(CPPFLAGS) -o $@ -MM $^

.PHONY : clean burn
burn : everavr.hex
	$(AVRDUDE) $(PROGRAMMER_DUDE) -p $(DEVICE_DUDE) -U flash:w:$^
clean :
	rm -f *.bak *~ *.bin *.hex *.lst *.o *.d
