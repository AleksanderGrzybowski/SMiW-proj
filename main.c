//#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include "ds1307.h"

/* Function to do simple active blocking delay */
void delay_ms(uint16_t count) {
	while (count--) {
		_delay_ms(1);
	}
}

/* Config */
////////////////////////////////////////////////////////

#define CONF_BUTTON_SETTIME_PORT PORTC
#define CONF_BUTTON_SETTIME_DDR  DDRC
#define CONF_BUTTON_SETTIME_PIN  PINC
#define CONF_BUTTON_SETTIME_NUM  0

#define CONF_BUTTON_DOWN_PORT PORTC
#define CONF_BUTTON_DOWN_DDR  DDRC
#define CONF_BUTTON_DOWN_PIN  PINC
#define CONF_BUTTON_DOWN_NUM  1

#define CONF_BUTTON_UP_PORT PORTC
#define CONF_BUTTON_UP_DDR  DDRC
#define CONF_BUTTON_UP_PIN  PINC
#define CONF_BUTTON_UP_NUM  2

#define BRIGHTNESS_EEPROM_LOCATION 13

/////////////////////////////////////////////////////

/*
 * Constants decribing segments on 7-seg display.
 * Segments are connected to whole PORTD (this port is hardcoded in ISR).
 * anodes (-> transistors P-type) are connected to low part (0..3) of PORTB.
 */
#define DOT 128
#define G 64
#define F 32
#define E 16
#define D 8
#define C 4
#define B 2
#define A 1
char tab[21] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0), (A + C + D + F + G), (A + D + E + F + G),
		(D + E + F + G), (A + B + E + F + G), (C + E + G), (A + E + F + G), (A
				+ B + C + E + F + G), (D + E + F), (B + C + D + E + G) };
#define LETTER_I 1
#define LETTER_O 0
#define DASH 10
#define EMPTY_DIGIT 11
#define LETTER_S 12
#define LETTER_E 13
#define LETTER_T 14
#define LETTER_P 15
#define LETTER_M 16
#define LETTER_N 16
#define LETTER_F 17
#define LETTER_A 18
#define LETTER_L 19
#define LETTER_D 20

/* variables for display */
volatile char display[4]; // 4 digits, dot handled below
volatile int dot_on = 0; // 1 = dot is on, 0 = dot is off
volatile int brightness = 7; // range <0-7>, 0 = none, 7 = maximum

/* variables for PWM and switching digits */
volatile int pwm_iter = 0;
volatile int cur_digit = 0; // current displayed digit

/* ISR used to multiplex 7-seg 4-digit display */
ISR(TIMER0_OVF_vect) {

	/* regulate speed of multiplexing, less = slower
	 * this is way simpler than fiddling with prescalers :)
	 */
	TCNT0 = 250;

	pwm_iter++;
	if (pwm_iter == 8) {
		pwm_iter = 0;
		cur_digit++;
	}
	if (cur_digit == 4) {
		cur_digit = 0;
	}

	/* turn off all digits to avoid ghosting */
	PORTB |= 15;
	PORTD = 0xff;

	/* turn on/off current digit, based on pwm current value */
	if (pwm_iter < brightness) {
		PORTB &= ~(1 << cur_digit);
	} else {
		PORTB |= (1 << cur_digit);
	}

	/* select segments */
	PORTD = ~tab[display[cur_digit]];
	/* and dot if there is */
	if (cur_digit == 1 && dot_on)
		PORTD &= ~DOT;
}

/* set display's digits, each digit separately, useful for debugging and used by other funcs */
void set_display_each_digit(int dig0, int dig1, int dig2, int dig3, int dot) {
	display[0] = dig0;
	display[1] = dig1;
	display[2] = dig2;
	display[3] = dig3;
	dot_on = dot;
}

/* set display's digits, two numbers in range <0..99> without leading '0'
 * if given -1, it will display empty digit (no segments lit)
 */
void set_display_two_digits(int left, int right, int dot) {
	int dig0, dig1, dig2, dig3;
	if (left == -1) {
		dig0 = EMPTY_DIGIT;
		dig1 = EMPTY_DIGIT;
	} else {
		dig0 = ((left / 10) == 0 ? EMPTY_DIGIT : (left / 10)); // without leading '0'
		dig1 = left % 10;
	}

	if (right == -1) {
		dig2 = EMPTY_DIGIT;
		dig3 = EMPTY_DIGIT;
	} else {
		dig2 = right / 10;
		dig3 = right % 10;
	}

	set_display_each_digit(dig0, dig1, dig2, dig3, dot);
}

/* set display's digits, one number in range <0.9999>, great for debug */
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

/* display current time */
void disp_time() {
	uint8_t hour, minute, second, dummy;
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);

	set_display_two_digits(hour, minute, (second % 2 == 1));
}

/* ask user to input time */
#define DEBOUNCE_DELAY 100 // ms
void get_time_from_user(int hour, int minute, uint8_t* out_hour,
		uint8_t* out_minute) {

	int flip = 0;

	while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) { // wait for hours
		delay_ms(DEBOUNCE_DELAY);
		if (!(CONF_BUTTON_UP_PIN & _BV(CONF_BUTTON_UP_NUM))) {
			hour++;
			if (hour == 24)
				hour = 0;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(CONF_BUTTON_DOWN_PIN & _BV(CONF_BUTTON_DOWN_NUM))) {
			hour--;
			if (hour == -1)
				hour = 23;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (flip >= 2)
			set_display_two_digits(hour, minute, 1);
		else
			set_display_two_digits(-1, minute, 1);
		flip++;
		if (flip == 4)
			flip = 0;
	}
	delay_ms(DEBOUNCE_DELAY);

	while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) { // wait for minutes
		delay_ms(DEBOUNCE_DELAY);
		if (!(CONF_BUTTON_UP_PIN & _BV(CONF_BUTTON_UP_NUM))) {
			minute++;
			if (minute == 60)
				minute = 0;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(CONF_BUTTON_DOWN_PIN & _BV(CONF_BUTTON_DOWN_NUM))) {
			minute--;
			if (minute == -1)
				minute = 59;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (flip >= 2)
			set_display_two_digits(hour, minute, 1);
		else
			set_display_two_digits(hour, -1, 1);
		flip++;
		if (flip == 4)
			flip = 0;
	}

	delay_ms(DEBOUNCE_DELAY);

	*out_hour = hour;
	*out_minute = minute;
}

void get_brightness_from_user() {
	set_display_each_digit(LETTER_D, 1, LETTER_S, LETTER_P, 0); // DISP

	while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) { // wait for hours
		delay_ms(DEBOUNCE_DELAY);
		if (!(CONF_BUTTON_UP_PIN & _BV(CONF_BUTTON_UP_NUM))) {
			if (brightness < 7) {
				brightness++;
			}
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(CONF_BUTTON_DOWN_PIN & _BV(CONF_BUTTON_DOWN_NUM))) {
			if (brightness > 1) {
				brightness--;
			}
			delay_ms(DEBOUNCE_DELAY);
		}
	}
	eeprom_write_byte((uint8_t*)BRIGHTNESS_EEPROM_LOCATION, brightness);
	delay_ms(DEBOUNCE_DELAY);
}

/* ask user for time and set it as current time */
void set_time() {
	uint8_t dummy;
	uint8_t hour = 0;
	uint8_t minute = 0;

	set_display_each_digit(LETTER_S, LETTER_E, LETTER_T, EMPTY_DIGIT, 0);
	delay_ms(500);
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &dummy);

	int new_hour, new_minute;
	get_time_from_user(hour, minute, &new_hour, &new_minute);
	ds1307_setdate(1, 1, 1, new_hour, new_minute, 0);
}


int main() {

	/* set all pins as inputs with pullups
	 * so accidental short to vcc/gnd won't destroy uC
	 */

	brightness = eeprom_read_byte((uint8_t*)BRIGHTNESS_EEPROM_LOCATION);
	if (!(brightness >= 1 && brightness <= 7)) {
		brightness = 3;
	}

	DDRB = DDRC = DDRD = 0x00;
	PORTB = PORTC = PORTD = 0xff;

	/* enable timer overflow interrupts for PWM */
	TIMSK0 |= (1 << TOIE0);
	/* set prescaler, selected by experimenting, but works perfectly */
	TCCR0B &= (1 << CS02);
	TCCR0B |= (1 << CS01);
	TCCR0B |= (1 << CS00);
	/* not sure if needed TODO */
	sei();

	/* display outputs */
	DDRB |= 0x0f; // common anodes
	DDRD = 0xff; // segments


	/* buttons inputs */
	CONF_BUTTON_SETTIME_DDR &= ~_BV(CONF_BUTTON_SETTIME_NUM);
	CONF_BUTTON_SETTIME_PORT |= _BV(CONF_BUTTON_SETTIME_NUM);

	CONF_BUTTON_UP_DDR &= ~_BV(CONF_BUTTON_UP_NUM);
	CONF_BUTTON_UP_PORT |= _BV(CONF_BUTTON_UP_NUM);

	CONF_BUTTON_DOWN_DDR &= ~_BV(CONF_BUTTON_DOWN_NUM);
	CONF_BUTTON_DOWN_PORT |= _BV(CONF_BUTTON_DOWN_NUM);

	/* start RTC and check if it ticks
	 * if battery is empty then it won't start and will be stuck at 0:00:00
	 * to force start, simply set any hour
	 */
	ds1307_init();
	uint8_t dummy, hour, minute, second;
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);
	if (hour == 0 && minute == 0 && second == 0) {
		ds1307_setdate(1, 1, 1, 0, 0, 0);
		delay_ms(2000);
	}


#define UPDATE_INTERVAL_TIME 100

	/* main loop of the program */
	while (1) {
		disp_time();
		delay_ms(UPDATE_INTERVAL_TIME);

		if (!(CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM))) {
			set_time();
			get_brightness_from_user();
		}
	}
}
