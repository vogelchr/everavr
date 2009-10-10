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
 *  Xtal    |9 B6   GND 20|                    - (20) (19) - (FS?)
 *  Xtal    |10 B7   B5 19| SCK  Prg             (22) (21)
 *  LCD C/D |11 D5   B4 18| MISO Prg
 *  LCD D6  |12 D6   B3 17| MOSI Prd
 *  LCD D7  |13 D7   B2 16| LCD \RD
 *  LCD \CS |14 B0   B1 15| LCD \WR
 *          +-------------+
 *
 *               Mapping from LCD pins to AVR Ports (Bit7 ... Bit0)
 *          << 7>> << 6>> << 5>> << 4>> << 3>> << 2>> << 1>> << 0>>
 *         +------+------+------+------+------+------+------+------+
 *  PORTD  |  D7  |  D6  | C/\Ð | \Res |      |      |      |      |
 *         +------+------+------+------+------+------+------+------+
 *  PORTC  |      |      |  D5  |  D4  |  D3  |  D2  |  D1  |  D0  |
 *         +------+------+------+------+------+------+------+------+
 *  PORTB  |      |      |      |      |      | \RD  | \WR  | \CS  |
 *         +------+------+------+------+------+------+------+------+
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "lcd_hardware.h"

/* -------- Serial Port ------------- */

inline unsigned char
get_char(){
	while(!(UCSR0A & _BV(RXC0))); /* receive complete */
	return UDR0; /* read from serial port */
}

inline void put_char(unsigned char c){
	while(!(UCSR0A & _BV(UDRE0)));
	UDR0 = c;
}

void print_hex(unsigned char x){
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


/*
 * protocol on serial port:
 *     ASCII char is just written to memory (CMD_DATA_WRITE_INC)
 *     0x01 c  : write arbitrary char c (idle --(0x01)--> write_data --(c)--> idle)
 *     0x02    : read arbitrary char c
 *     0x03
 */

#define CHAR_NOP     0x00
#define CHAR_WRITE   0x01
#define CHAR_ECHO    0x02
#define CHAR_ADDR    0x03
#define CHAR_STATUS  0x04
#define CHAR_RESET   0x05
#define CHAR_MODE    0x06
#define CHAR_DISP    0x07
#define CHAR_CURSOR  0x08

uint8_t serport_data_1; /* memorize */
uint8_t serport_data_2; /* memorize */

enum serport_state {
	serport_idle,
	serport_echo,
	serport_write_data,
	serport_set_addr_lo,
	serport_set_addr_hi,
	serport_mode,
	serport_disp,
	serport_cursor
};
uint8_t global_serport_state;

static void
eat_char(uint8_t c){
	uint8_t serport_state = global_serport_state;
	switch(serport_state){

	case serport_echo:
		serport_state = serport_idle;
		put_char(c);
		break;

	case serport_write_data:
		lcd_command_1(CMD_DATA_WRITE_INC,c);
		goto become_idle;

	case serport_set_addr_lo:
		serport_data_1 = c;
		serport_state = serport_set_addr_hi;
		break;

	case serport_set_addr_hi:
		lcd_command_long(CMD_ADDRESS_POINTER,serport_data_1 | (c<<8));
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

	default: /* =idle */
		if(c>0x20){ /* write text char -> add 0x20 to match ASCII */
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
		}
		break;
	}
	goto serport_out;

become_idle:
	serport_state = serport_idle;
serport_out:
	global_serport_state = serport_state;

}

int main(){
	/* init LCD pins */
	DDRD= PORTD_CD | PORTD_RES; /* CD, RES is AVR output, default to 0 */
	PORTB=PORTB_RD | PORTB_WR;  /* \RD, \WR at 1, bus is idle */
	DDRB= PORTB_RD | PORTB_WR | PORTB_CS;  /* RD, WR, CS is AVR output */

	/* setup serial port */
	UCSR0B = /*_BV(RXCIE0) |*/ _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8 bit */
	UBRR0 = 6; /* 1 MHz, 9600 bps */

	lcd_hardware_init();

	while(1){
		unsigned char c = get_char(); /* from serial port */
		eat_char(c);
	}
}