#define F_CPU 16000000UL // 16 MHz clock

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include "ds1307.h"
#include "ds18b20.h"

void delay_ms(uint16_t count) {
	while (count--) {
		_delay_ms(1);
	}
}

void delay_us(uint16_t count) {
	while (count--) {
		_delay_us(1);
	}
}

//////////////////////////////////////
#define CONF_PHOTO_ADC_CHANNEL 3

#define CONF_BUTTON_SETTIME_PORT PORTA
#define CONF_BUTTON_SETTIME_DDR  DDRA
#define CONF_BUTTON_SETTIME_PIN  PINA
#define CONF_BUTTON_SETTIME_NUM  0

#define CONF_BUTTON_DOWN_PORT PORTA
#define CONF_BUTTON_DOWN_DDR  DDRA
#define CONF_BUTTON_DOWN_PIN  PINA
#define CONF_BUTTON_DOWN_NUM  1

#define CONF_BUTTON_UP_PORT PORTA
#define CONF_BUTTON_UP_DDR  DDRA
#define CONF_BUTTON_UP_PIN  PINA
#define CONF_BUTTON_UP_NUM  2

#define CONF_BUTTON_DISPTEMP_PORT PORTA
#define CONF_BUTTON_DISPTEMP_DDR  DDRA
#define CONF_BUTTON_DISPTEMP_PIN  PINA
#define CONF_BUTTON_DISPTEMP_NUM  4

#define CONF_BUTTON_DISPLIGHT_PORT PORTA
#define CONF_BUTTON_DISPLIGHT_DDR  DDRA
#define CONF_BUTTON_DISPLIGHT_PIN  PINA
#define CONF_BUTTON_DISPLIGHT_NUM  5

/////////////////////////////
// Constants decribing segments on 7-seg display.
// Segments are connected to PORTD.
// anodes (->mosfets) are connected do low part of PORTB.
#define DOT 128
#define G 64
#define F 32
#define E 16
#define D 8
#define C 4
#define B 2
#define A 1
char tab[18] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0), (A + C + D + F + G), (A + D + E + F + G),
		(D + E + F + G), (A + B + E + F + G), (C + E + G), (A + E + F + G) };
#define DASH 10
#define EMPTY_DIGIT 11
#define LETTER_S 12
#define LETTER_E 13
#define LETTER_T 14
#define LETTER_P 15
#define LETTER_M 16
#define LETTER_F 17

volatile char display[4]; // 4 digits, dot handled below
volatile int dot_on = 0;
volatile int brightness = 7; // range 0-7

// for PWM and switching digits
volatile int cur_digit = 0;
volatile int pwm_iter = 0; // range 0-7

ISR(TIMER0_OVF_vect) {
	TCNT0 = 220; // regulate speed

	pwm_iter++;
	if (pwm_iter == 8) {
		pwm_iter = 0;
		cur_digit++;
	}
	if (cur_digit == 4) {
		cur_digit = 0;
	}

	// turn on/off current digit, based on pwm current value
	if (pwm_iter < brightness) {
		PORTB &= ~(1 << cur_digit);
	} else {
		PORTB |= (1 << cur_digit);
	}

	// select segments
	PORTD = tab[display[cur_digit]];
	if (cur_digit == 1 && dot_on) // and dot if there is
		PORTD |= DOT;
}

void set_display_each_digit(int dig0, int dig1, int dig2, int dig3, int dot) {
	display[0] = dig0;
	display[1] = dig1;
	display[2] = dig2;
	display[3] = dig3;
	dot_on = dot;
}

void set_display_two_digits(int left, int right, int dot) {
	int dig0 = ((left / 10) == 0 ? EMPTY_DIGIT : (left / 10)); // without leading '0'
	int dig1 = left % 10;
	int dig2 = right / 10;
	int dig3 = right % 10;
	set_display_each_digit(dig0, dig1, dig2, dig3, dot);
}

void set_display_whole_number(int number) {
	int d3 = number % 10;
	number /= 10;
	int d2 = number % 10;
	number /= 10;
	int d1 = number % 10;
	number /= 10;
	int d0 = number % 10;
	set_display_each_digit(d0, d1, d2, d3, 0);
}

void disp_time() {
	uint8_t year, month, day, hour, minute, second;
	ds1307_getdate(&year, &month, &day, &hour, &minute, &second);

	set_display_two_digits(hour, minute, (second % 2 == 0));
}

void disp_temp() {
	unsigned char ds18b20_pad[9];

	ds18b20_ConvertT();
	_delay_ms(750); // shows garbage without it, read the docs!!!
	ds18b20_Read(ds18b20_pad);

	float temp = ((ds18b20_pad[1] << 8) + ds18b20_pad[0]) / 16.0;
	int itemp = (int) (temp * 10.0);

	int d2 = itemp % 10;
	itemp /= 10;
	int d1 = itemp % 10;
	itemp /= 10;
	int d0 = itemp % 10;

	set_display_each_digit(d0, d1, d2, DASH, 1);
}

#define DEBOUNCE_DELAY 200 // ms
void set_time() {
	int dummy;
	int hour = 0;
	int minute = 0;

	set_display_each_digit(LETTER_S, LETTER_E, LETTER_T, EMPTY_DIGIT, 0);
	delay_ms(500);
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &dummy);

	while (PINA & 1) { // wait for hours
		delay_ms(DEBOUNCE_DELAY);
		if (!(PINA & 2)) {
			hour++;
			if (hour == 24)
				hour = 0;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(PINA & 4)) {
			hour--;
			if (hour == -1)
				hour = 23;
			delay_ms(DEBOUNCE_DELAY);
		}

		set_display_two_digits(hour, minute, 1);
		if (display[0] == 0)
			display[0] = 11;

	}
	delay_ms(DEBOUNCE_DELAY);

	while (PINA & 1) { // wait for minutes
		if (!(PINA & 2)) {
			minute++;
			if (minute == 60)
				minute = 0;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(PINA & 4)) {
			minute--;
			if (minute == -1)
				minute = 59;
			delay_ms(DEBOUNCE_DELAY);
		}
		set_display_two_digits(hour, minute, 1);
	}

	delay_ms(DEBOUNCE_DELAY);
	ds1307_setdate(dummy, dummy, dummy, hour, minute, 0);
}

// from the internet
uint16_t adc_read() {
	uint8_t ch = CONF_PHOTO_ADC_CHANNEL;
	// select the corresponding channel 0~7
	// ANDing with ’7′ will always keep the value
	// of ‘ch’ between 0 and 7
	ch &= 0b00000111;  // AND operation with 7
	ADMUX = (ADMUX & 0xF8) | ch; // clears the bottom 3 bits before ORing

	// start single convertion
	// write ’1′ to ADSC
	ADCSRA |= (1 << ADSC);

	// wait for conversion to complete
	// ADSC becomes ’0′ again
	// till then, run loop continuously
	while (ADCSRA & (1 << ADSC))
		;

	return (ADCH);
}

void disp_light() {
	int res = adc_read();
	set_display_whole_number(res);
}

void regulate_brightness() {
	int reading = adc_read();

	if (reading >= 254)
		brightness = 2;
	else
		brightness = 7;
}

int main() {

	// set all pins as inputs with pullups
	// so accidental short to vcc/gnd won't destroy uC
	DDRA = DDRB = DDRC = DDRD = 0x00;
	PORTA = PORTB = PORTC = PORTD = 0xff;

	// disable JTAG so we can use all pins
	MCUCSR |= (1 << JTD);
	MCUCSR |= (1 << JTD);

	// enable timer overflow interrupts
	TIMSK |= (1 << TOIE0);
	// set prescaler, don't know what is now though, but works perfectly
	TCCR0 |= (1 << CS02);
	TCCR0 &= ~(1 << CS01);
	TCCR0 &= ~(1 << CS00);
	// not sure if needed
	sei();

	// display
	DDRB |= 0x0f; // common anodes
	DDRD = 0xff; // segments

	// buttons
	CONF_BUTTON_SETTIME_DDR &= ~_BV(CONF_BUTTON_SETTIME_NUM);
	CONF_BUTTON_SETTIME_PORT |= _BV(CONF_BUTTON_SETTIME_NUM);

	CONF_BUTTON_UP_DDR &= ~_BV(CONF_BUTTON_UP_NUM);
	CONF_BUTTON_UP_PORT |= _BV(CONF_BUTTON_UP_NUM);

	CONF_BUTTON_DOWN_DDR &= ~_BV(CONF_BUTTON_DOWN_NUM);
	CONF_BUTTON_DOWN_PORT |= _BV(CONF_BUTTON_DOWN_NUM);

	CONF_BUTTON_DISPTEMP_DDR &= ~_BV(CONF_BUTTON_DISPTEMP_NUM);
	CONF_BUTTON_DISPTEMP_PORT |= _BV(CONF_BUTTON_DISPTEMP_NUM);

	CONF_BUTTON_DISPLIGHT_DDR &= ~_BV(CONF_BUTTON_DISPLIGHT_NUM);
	CONF_BUTTON_DISPLIGHT_PORT |= _BV(CONF_BUTTON_DISPLIGHT_NUM);

	// ADC
	// AREF = AVcc
	ADMUX = (1 << REFS0);
	// Enable and prescaler of 128
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADMUX |= (1 << ADLAR);

	// display "init"
	set_display_each_digit(1, LETTER_M, 1, LETTER_T, 1);
	delay_ms(1500);

	// start RTC and check if it ticks
	ds1307_init();
	uint8_t dummy, hour, minute, second;
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);
	if (hour == 0 && minute == 0 && second == 0) {
		ds1307_setdate(1, 1, 1, 0, 0, 0); // DS won't start if backup battery fails, so this will do the trick
		delay_ms(2000);
	}

	while (1) {
		int i, j;
		for (i = 0; i < 20; ++i) {
			disp_time();
			delay_ms(100);
			regulate_brightness();
			if (!(PINA & 1)) {
				set_time();
			}
			if (!(PINA & 16)) {
				set_display_each_digit(LETTER_T, LETTER_E, LETTER_M, LETTER_P,
						0);
				delay_ms(1000);
				for (j = 0; j < 10; ++j) {
					disp_temp();
					delay_ms(100);
				}
			}
			if (!(PINA & 32)) {
				set_display_each_digit(LETTER_F, 0, LETTER_T, 0, 0);
				delay_ms(1000);
				for (j = 0; j < 7; ++j) {
					disp_light();
					delay_ms(300);
				}
			}
		}
	}
}
