#ifndef LCD_HARDWARE_H
#define LCD_HARDWARE_H

#include <avr/io.h>

#define PORTD_CD  _BV(5)
#define PORTD_RES _BV(4)
#define PORTB_RD  _BV(2)
#define PORTB_WR  _BV(1)
#define PORTB_CS  _BV(0)

#define STATUS_CMD_OK		0x01
#define STATUS_DATA_OK		0x02
#define STATUS_AUTO_READ_OK	0x04
#define STATUS_AUTO_WRITE_OK	0x08
#define STATUS_OPERATION	0x20
#define STATUS_ERROR		0x40
#define STATUS_BLINK		0x80

#define CMD_CURSOR_POS		0x21
#define CMD_OFFSET_REGISTER	0x22
#define CMD_ADDRESS_POINTER	0x24

#define CMD_TEXT_HOME_ADDR	0x40
#define CMD_TEXT_AREA		0x41
#define CMD_GRAPHIC_HOME_ADDR	0x42
#define CMD_GRAPHIC_AREA	0x43

#define CMD_SET_MODE		0x80
#define CMD_MODE_OR		0x00
#define CMD_MODE_EXOR		0x01
#define CMD_MODE_AND		0x03
#define CMD_MODE_TEXT_ATTRIB	0x04
#define CMD_MODE_MASK           0x07 /* all sensible bits */
#define CMD_MODE_INT_CG		0x00
#define CMD_MODE_EXT_CG		0x80

#define CMD_MODE_DISPLAY	0x90
#define CMD_DISP_OFF		0x00
#define CMD_DISP_CURSOR		0x02
#define CMD_DISP_CURSOR_BLINK	0x01
#define CMD_DISP_TEXT		0x04
#define CMD_DISP_GRAPHICS	0x08
#define CMD_DISP_MASK           0x0f

#define CMD_CURSOR_PATTERN	0xa0
#define CMD_CURSOR_N_LINES(n)	((n)-1) // line 1..8 --> 0x00 -> 0x07

#define CMD_AUTO_WRITE		0xb0
#define CMD_AUTO_READ		0xb1
#define CMD_AUTO_RESET		0xb2

#define CMD_DATA_WRITE_INC	0xc0
#define CMD_DATA_READ_INC	0xc1
#define CMD_DATA_WRITE_DEC	0xc2
#define CMD_DATA_READ_DEC	0xc3
#define CMD_DATA_WRITE		0xc4
#define CMD_DATA_READ		0xc5

#define CMD_SCREEN_PEEK		0xe0
#define CMD_SCREEN_COPY		0xe8

#define EXT_RAM_SIZE		0x2000


/* write data to data register (iscmd=0) or command register (iscmd=1) */
extern void
lcd_write(uint8_t data,uint8_t iscmd);

/* read data from data register (isstat=0) or status register (isstat=1) */
extern uint8_t
lcd_read(uint8_t isstat);


/* poll status register for STATUS_CMD_OK, then write cmd to command reg.
   return 0 if ok, 1 if polling counter exceeds limit */
extern uint8_t
lcd_command(unsigned char cmd);

/* poll status register for STATUS_DATA_OK, then write data to data reg.
   return 0 if ok, 1 if polling counter exceeds limit */
extern uint8_t
lcd_data(unsigned char data);

/* poll status register for STATUS_DATA_OK, then get data from data reg.
   return 0 if ok, 1 if polling counter exceeds limit
   data is returned in *data
 */
extern uint8_t
lcd_get_data(uint8_t *data);

/* convenience functions that combine lcd_command, lcd_data, lcd_get_data */
extern uint8_t lcd_command_1(uint8_t cmd,uint8_t data);
extern uint8_t lcd_command_2(uint8_t cmd,uint8_t data1,uint8_t data2);
extern uint8_t lcd_command_long(uint8_t cmd,uint16_t data);
extern uint8_t lcd_command_read(uint8_t cmd,uint8_t *data);

/* do all kind of fancy stuff, clear RAM, enable graphics, set pointers */
extern void lcd_hardware_init();

#endif

