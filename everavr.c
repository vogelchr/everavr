/* code to control a Everbouquet MG24065G LCD with a Atmel AVR Mega168
 *
 *            Mega168             Everbouquet Connector (view from front)
 *          +------U------+                  GND ( 2) ( 1) Frame GND
 *   \Reset |1 C6    C5 28| LCD D5            Vo ( 4) ( 3) Vcc
 * Ser. RxD |2 D0    C4 27| LCD D4           \RD ( 6) ( 5) \WR
 * Ser. TxD |3 D1    C3 26| LCD D3          C/\D ( 8) ( 7) \CS
 * USB+ Int0|4 D2    C2 25| LCD D2          \RES (10) ( 9) Vee
 * USB-     |5 D3    C1 24| LCD D1            D1 (12) (11) D0
 * LCD \Res |6 D4    C0 23| LCD D0            D3 (14) (13) D2
 *          |7 Vcc  Vcc 22|                   D5 (16) (15) D4
 *          |8 GND Aref 21|                   D7 (18) (17) D6
 *  Xtal    |9 B6   GND 20|                    - (20) (19) FS (conn. to Vcc)
 *  Xtal    |10 B7   B5 19| SCK  Prg             (22) (21)
 *  LCD C/D |11 D5   B4 18| MISO Prg
 *  LCD D6  |12 D6   B3 17| MOSI Prd
 *  LCD D7  |13 D7   B2 16| LCD \RD
 *  LCD \CS |14 B0   B1 15| LCD \WR
 *          +-------------+
 *
 * On the Everbouquet LCD FS is "Font Select". This program here assumes
 * a 6x8 font for which FS has to be connected to +5V (Vcc).
 *
 * Vee is the output of a DC/DC connector that is included on the LCD PCB.
 * It usually outputs around -9V. Use a 50k - 200k potentiometer between
 * Vee and Vcc, fee the center tap back to Vo to adjust the contrast.
 *
 *               Mapping from LCD pins to AVR Ports (Bit7 ... Bit0)
 *          << 7>> << 6>> << 5>> << 4>> << 3>> << 2>> << 1>> << 0>>
 *         +------+------+------+------+------+------+------+------+
 *  PORTD  |  D7  |  D6  | C/\D | \Res |      |      |      |      |
 *         +------+------+------+------+------+------+------+------+
 *  PORTC  |      |      |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
 *         +------+------+------+------+------+------+------+------+
 *  PORTB  |      |      |      |      |      | \RD  | \WR  | \CS  |
 *         +------+------+------+------+------+------+------+------+
 *
 * FUSE bytes:
 *    lfuse 0xdf (ext. 18 MHz Crystal)
 *    hfuse 0xdf (default)
 *    efuse 0x01 (default)
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "lcd_hardware.h"
#include <usbdrv.h>

static void eat_char(uint8_t c); // used by USB code...

/* ---------------------- USB ------------------------------- */

PROGMEM char usbHidReportDescriptor[22] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x80,                    //   REPORT_COUNT (128)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

/* usbFunctionRead() is called when the host requests a chunk of data from
 * the device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar
usbFunctionRead(uchar *data, uchar len){
	return 1;
}

/* usbFunctionWrite() is called when the host sends a chunk of data to the
 * device. For more information see the documentation in usbdrv/usbdrv.h.
 */
uchar
usbFunctionWrite(uchar *data, uchar len){
 	int i;

	for(i=0;i<len;i++)
		eat_char(data[i]);
	return 1;
}

usbMsgLen_t
usbFunctionSetup(uchar data[8]){
	usbRequest_t    *rq = (void *)data;

	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){
		/* HID class request */
		if(rq->bRequest == USBRQ_HID_GET_REPORT){
			/* wValue: ReportType (highbyte), ReportID (lowbyte) */
			/* since we have only one report type, we can ignore
			   the report-ID */
			/* use usbFunctionRead() to obtain data */
			return USB_NO_MSG;
		} else if(rq->bRequest == USBRQ_HID_SET_REPORT){
			/* since we have only one report type, we can
			   ignore the report-ID */
			/* use usbFunctionWrite() to receive data from host */
			return USB_NO_MSG;
		}
	}else{
		/* ignore vendor type requests, we don't use any */
	}
	return 0;
}

/* -------- Serial Port ------------- */
inline int
get_char(unsigned char *c){
	if(!(UCSR0A & _BV(RXC0))){
		return -1; // nothing in input buffer!
	}
	*c=UDR0; /* read from serial port */
	return 0;
}

inline int
put_char(unsigned char c){
	if(!(UCSR0A & _BV(UDRE0)))
		return -1; // no space
	UDR0 = c;
	return 0;
}

void
print_hex(unsigned char x){
	unsigned char n;

	put_char('0');
	put_char('x');

	n = (x >> 4)+'0';
	if(n>'9')
		n += ('@'-'9');
	put_char(n);

	n = (x & 0x0f)+'0';
	if(n>'9')
		n += ('@'-'9');
	put_char(n);
}

#if 0
/* get one hex character 0..9, a..f, A..F from serial port,
   put result (0..15) in *nib, return 0 on success, 1 on error */
int
get_nibble(uint8_t *nib){
	unsigned char x = get_char();
	if(x >= '0' && x <= '9'){
		*nib=x-'0';
		return 0;
	}
	if(x >= 'A' && x <= 'F'){
		*nib=x-'A';
		return 0;
	}
	if(x >= 'a' && x <= 'f'){
		*nib=x-'a';
		return 0;
	}
	return 1;
}

/* get two characters (in hexadecimal) from serial port, put
   result (0..255) in *val, return 0 if ok, return != 0 on error */
int
get_hex(uint8_t *val){
	register int r;
	uint8_t nib;
	if(!(r=get_nibble(&nib)))
		return r;
	*val = nib << 4;
	if(!(r=get_nibble(&nib)))
		return r;
	*val |= nib;
	return 0;
}
#endif

/*
 * Protocol on serial port:
 *    <0x20 .. 0xff> -> write char - 0x20 to LCD memory
 * 			(convert ASCII to LCD charset)
 *    ^A/0x01 byte   -> write byte to LCD memory, literally
 *    ^B/0x02 byte   -> echo byte back to serial port
 *    ^C/0x03 lo hi  -> set address pointer (for mem read/write) to addr
 *    ^D/0x04        -> write status byte to serial port
 *    ^E/0x05        -> execute lcd hardware init
 *    ^F/0x06 byte   -> set graphic/text combination mode (see hardware.h)
 *    ^G/0x07 byte   -> set display mode (text, cursor, blink, gfx. on/off)
 *    ^H/0x08 byte   -> set cursor width (0..7 -> 1..8 lines)
 *    ^I/0x09 count bytes... -> bulk transfer count bytes (count=0: 256 byte)
 *    ^J/0x0a x y    -> set cursor to x,y
 */

#define CHAR_NOP     0x00
#define CHAR_WRITE   0x01	// ^A
#define CHAR_ECHO    0x02	// ^B
#define CHAR_ADDR    0x03	// ^C
#define CHAR_STATUS  0x04	// ^D
#define CHAR_RESET   0x05	// ^E
#define CHAR_MODE    0x06	// ^F
#define CHAR_DISP    0x07	// ^G
#define CHAR_CURSOR  0x08	// ^H
#define CHAR_BULK    0x09       // ^I
#define CHAR_POS_CURSOR 0x10    // ^J

enum serport_state {
	serport_idle,
	serport_echo,
	serport_write_data,
	serport_set_addr_lo,
	serport_set_addr_hi,
	serport_mode,
	serport_disp,
	serport_cursor,
	serport_bulk_count,
	serport_bulk_data,
	serport_pos_cursor_x,
	serport_pos_cursor_y
};
uint8_t global_serport_state;
uint8_t global_serport_data; /* memorize stuff for serial protocol */

/* state machine for our serial protocol. Eating one character at a time */
static void
eat_char(uint8_t c){
	uint8_t serport_state = global_serport_state;
	uint8_t serport_data  = global_serport_data;
	switch(serport_state){

	case serport_echo:
		serport_state = serport_idle;
		put_char(c);
		break;

	case serport_write_data:
		lcd_command_1(CMD_DATA_WRITE_INC,c);
		goto become_idle;

	case serport_set_addr_lo:
		serport_data = c;
		serport_state = serport_set_addr_hi;
		break;

	case serport_set_addr_hi:
		lcd_command_2(CMD_ADDRESS_POINTER,serport_data,c);
		goto become_idle;

	case serport_mode:
		c &= CMD_MODE_MASK;
		c |= CMD_SET_MODE;
		lcd_command(c);
		goto become_idle;

	case serport_disp:
		c &= CMD_DISP_MASK;
		c |= CMD_MODE_DISPLAY;
		lcd_command(c);
		goto become_idle;

	case serport_cursor:
		c &= 0x07;
		c |= CMD_CURSOR_PATTERN;
		lcd_command(c);
		goto become_idle;

	case serport_bulk_count:
		serport_data = c;
		serport_state = serport_bulk_data;
		break;

	case serport_bulk_data:
		serport_data--;
		lcd_data(c);
		if(serport_data == 0){
			lcd_command(CMD_AUTO_RESET);
			goto become_idle;
		}
		break; /* stay in serport_bulk_data state */

	case serport_pos_cursor_x:
		serport_data = c;
		serport_state = serport_pos_cursor_y;
		break;

	case serport_pos_cursor_y:
		lcd_command_2(CMD_CURSOR_POS,serport_data,c);
		goto become_idle;

	default: /* =idle */
		if(c>=0x20){ /* write text char -> add 0x20 to match ASCII */
			lcd_command_1(CMD_DATA_WRITE_INC,c-0x20);
			break;
		}
		switch(c){
		case CHAR_WRITE:
			serport_state = serport_write_data;
			break;
		case CHAR_ECHO:
			serport_state = serport_echo;
			break;
		case CHAR_ADDR:
			serport_state = serport_set_addr_lo;
			break;
		case CHAR_RESET:
			lcd_hardware_init();
			break;
		case CHAR_STATUS:
			lcd_command_read(CMD_DATA_READ_INC,&c);
			put_char(c);
			break;
		case CHAR_MODE:
			serport_state = serport_mode;
			break;
		case CHAR_DISP:
			serport_state = serport_disp;
			break;
		case CHAR_CURSOR:
			serport_state = serport_cursor;
			break;
		case CHAR_BULK:
			lcd_command(CMD_AUTO_WRITE);
			serport_state = serport_bulk_count;
			break;
		case CHAR_POS_CURSOR:
			serport_state = serport_pos_cursor_x;
		}
		break;
	}
	goto serport_out;

become_idle:
	serport_state = serport_idle;
serport_out:
	global_serport_state = serport_state;
	global_serport_data  = serport_data;
}


/* ---------- initial data to show after powerup ---- */
PROGMEM char initial_data[]={
	'H','e','l','l','o',' ','W','o','r','l','d','!'
};
unsigned char initial_readptr=0;

int main(){
	/* init LCD pins */
	DDRD= PORTD_CD | PORTD_RES; /* CD, RES is AVR output, default to 0 */
	PORTB=PORTB_RD | PORTB_WR;  /* \RD, \WR at 1, bus is idle */
	DDRB= PORTB_RD | PORTB_WR | PORTB_CS;  /* RD, WR, CS is AVR output */

	/* setup serial port */
	UCSR0A = _BV(U2X0); /* double uart clock */
	UCSR0B = /*_BV(RXCIE0) |*/ _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8 bit */
	UBRR0 = 155; /* 18 MHz / 156 = 115384 bps = 115k2 + 0.16% */

	sei();
	usbInit();
	usbDeviceConnect();

	lcd_hardware_init();

	while(1){
		unsigned char c;
		if(initial_readptr < sizeof(initial_data)){
			c = pgm_read_byte(initial_data + initial_readptr);
			initial_readptr++;
			eat_char(c);
		}
		if(get_char(&c)==0)
			eat_char(c);
		usbPoll();
	}
}
