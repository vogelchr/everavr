#include "lcd_hardware.h"
#include <avr/io.h>

/* raw hardware access function, write data or command register
   depending on iscmd=1 (command) or =0 (data) */
void
lcd_write(uint8_t data,uint8_t iscmd){
	/* set output pins */
	PORTC = data; /* upper two bits in portc don't care! */
	if(data & _BV(7))
		PORTD |= _BV(7);
	if(data & _BV(6))
		PORTD |= _BV(6);
	if(iscmd)
		PORTD |= PORTD_CD;

	DDRC = 0x3f;    /* write out data on pins 0..5 */
	DDRD |= _BV(7);
	DDRD |= _BV(6); /* write out data on pins 6,7 */
	
	PORTB &= ~PORTB_WR; /* \WR pulse width min 80 ns! */
	asm("nop;");
	/* data setup up min 80ns before \WR back to 1 */
	PORTB |=  PORTB_WR; /* strobe \WR */

	PORTD &= ~_BV(7);
	PORTD &= ~_BV(6);
	PORTD &= ~PORTD_CD;

	DDRC = 0; /* disable drivers on 0..5 */
	DDRD &= ~_BV(7); /* disable drivers on D6, D7, C/\D */
	DDRD &= ~_BV(6); 
}

/* raw hardware access function, read data or status register
   (depending on isstat=1 (status) or =0 (data) */
uint8_t
lcd_read(uint8_t isstat){
	unsigned char ret;
	/* set output pins */
	if(isstat)
		PORTD |= PORTD_CD; /* C/D setup time is min. 100 ns! */
	asm("nop;"); /* assuming 10 MHz .. 20 MHz */
	
	PORTB &= ~PORTB_RD; /* \WR to 0, min. 80ns! */
	/* datasheet: max 150ns from \RD->0 to valid data (T_ACC) */
	asm("nop;"); /* assuming 10 MHz .. 20 MHz */
	ret = PIND & 0xc0; /* upper two bits */
	ret |= PINC;       /* lower six bits */
	PORTB |=  PORTB_RD; /* \WR back to 1 */
	PORTD &= ~PORTD_CD;
	return ret;
}

/* write command to controller, check if it's ok to do so in status
   register first. Poll status register 256 times before giving up.
   Return 0 on success, 1 if status register never went ok */
uint8_t
lcd_command(uint8_t cmd){
	register uint8_t i=0;
	do {
		i--;
		if(lcd_read(1) & STATUS_CMD_OK)
			break;
	} while(i!=0);
	if(i==0)
		return 1; // error
	lcd_write(cmd,1); /* write command */
	return 0;
}

/* write data to controller, remarks for lcd_command above apply, too */
uint8_t
lcd_data(uint8_t data){
	register uint8_t i=0;
	do {
		i--;
		if(lcd_read(1) & STATUS_DATA_OK)
			break;
	} while(i!=0);
	if(i==0)
		return 1; // error
	lcd_write(data,0); /* write command */
	return 0;
}

/* read data from controller, remarks for lcd_command above apply, too */
uint8_t
lcd_get_data(uint8_t *data){
	register uint8_t i=0;
	do {
		i--;
		if(lcd_read(1) & STATUS_DATA_OK)
			break;
	} while(i!=0);
	if(i==0)
		return 1; // error

	*data = lcd_read(0); /* write command */
	return 0;	
}

/* write one data byte, then post one command */
uint8_t
lcd_command_1(uint8_t cmd,uint8_t data){
	if(lcd_data(data))
		return 1;
	return lcd_command(cmd);
}

/* write two data bytes, then post one command */
uint8_t
lcd_command_2(uint8_t cmd,uint8_t data1,uint8_t data2){
	if(lcd_data(data1))
		return 1;
	if(lcd_data(data2))
		return 1;
	return lcd_command(cmd);
}

/* write two data bytes, as low/high half of 16bit-data, then post command */
uint8_t
lcd_command_long(uint8_t cmd,uint16_t data){
	if(lcd_data(data & 0xff)) // LSB
		return 1;
	if(lcd_data(0xff & (data >> 8))) // MSB
		return 1;
	return lcd_command(cmd);
}

/* post command, then read data */
uint8_t
lcd_command_read(uint8_t cmd,uint8_t *data){
	if(lcd_command(cmd))
		return 1;
	return lcd_get_data(data);
}

/* write in auto-mode, check status byte first */
static unsigned int
lcd_auto_write(unsigned char data){
	register unsigned int i=0;
	do {
		i--;
		if(lcd_read(1) & STATUS_AUTO_WRITE_OK)
			break;
	} while(i!=0);
	if(i==0)
		return 1; // error

	lcd_write(data,0); /* write command */
	return 0;
}

/* read in auto-mode, check status byte first. */
static unsigned int
lcd_auto_read(unsigned char *data){
	register unsigned int i=0;
	do {
		i--;
		if(lcd_read(1) & STATUS_AUTO_READ_OK)
			break;
	} while(i!=0);
	if(i==0)
		return 1; // error

	*data=lcd_read(0); /* write command */
	return 0;
}

void
lcd_hardware_init(){
	register uint16_t w;
	PORTD |= PORTD_RES; /* \RES -> 1, lcd should start running */

	/* initialize */
	lcd_command(CMD_SET_MODE | CMD_MODE_OR);
	lcd_command(CMD_MODE_DISPLAY | CMD_DISP_CURSOR |
		CMD_DISP_CURSOR_BLINK | CMD_DISP_TEXT | CMD_DISP_GRAPHICS);
	lcd_command(CMD_CURSOR_PATTERN|CMD_CURSOR_N_LINES(4));

	lcd_command_2(CMD_CURSOR_POS,0,0); /* cursor to upper left */
	lcd_command_long(CMD_OFFSET_REGISTER,0x0000); /* ext. char gen */

	lcd_command_long(CMD_TEXT_AREA,40); /* 40 columns */
	/* 40col x 8 line = 320 = 0x140 chars, roud up to 0x200 */
	lcd_command_long(CMD_GRAPHIC_AREA,0x28);
	/* 240 pixels/8 bits_per_pixel=30 byte/line */
	/* 240x64px / 6bit = 2560 byte = 0xa00 */

	lcd_command_long(CMD_TEXT_HOME_ADDR,LCD_TEXT_BASE);
	lcd_command_long(CMD_GRAPHIC_HOME_ADDR,LCD_GRAPHIC_BASE);
	lcd_command_2(CMD_OFFSET_REGISTER,(LCD_CGRAM_BASE >> 11),0);

	/* *TEXT*
	   0 : 0x0000 : start of first line
	   1 : 0x0028 : start of 2nd line
	     ...
	   7 : 0x0118 : start of 8th line  ... 0x013f end of 8th line
	   *GRAPHICS*
		 0 0x0140 ..0x0167 line 1
		 1 0x0168 ..0x018f line 2
		 2 0x0190 ..0x01b7 line 3
		63 0x0b18 ..0x0b3f line 64
	   *GFX gen area*
		starts at 0x0b40 .. ends at 0x1fff
	*/

	/* clear whole memory with zeroes */
	lcd_command_long(CMD_ADDRESS_POINTER,0x0000);
	lcd_command(CMD_AUTO_WRITE);
	for(w=0;w<LCD_RAM_SIZE;w++)
		lcd_auto_write(0);
	lcd_command(CMD_AUTO_RESET); /* should overflow */
}

