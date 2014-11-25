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

////////////////////////////////////////////////////////
#define CONF_PHOTO_ADC_CHANNEL 7

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
#define CONF_BUTTON_DISPTEMP_NUM  3

#define CONF_BUTTON_SETALARM_PORT PORTA
#define CONF_BUTTON_SETALARM_DDR  DDRA
#define CONF_BUTTON_SETALARM_PIN  PINA
#define CONF_BUTTON_SETALARM_NUM  4

#define CONF_JUMPER_BRIG_PORT PORTA
#define CONF_JUMPER_BRIG_DDR DDRA
#define CONF_JUMPER_BRIG_PIN  PINA
#define CONF_JUMPER_BRIG_NUM  5

#define CONF_JUMPER_TEMP_PORT PORTA
#define CONF_JUMPER_TEMP_DDR DDRA
#define CONF_JUMPER_TEMP_PIN  PINA
#define CONF_JUMPER_TEMP_NUM  6

#define CONF_BUZZER_PORT PORTC
#define CONF_BUZZER_DDR DDRC
#define CONF_BUZZER_PIN  PINC
#define CONF_BUZZER_NUM  3

/////////////////////////////////////////////////////

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
char tab[19] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0), (A + C + D + F + G), (A + D + E + F + G),
		(D + E + F + G), (A + B + E + F + G), (C + E + G), (A + E + F + G), (A
				+ B + C + E + F + G) };
#define DASH 10
#define EMPTY_DIGIT 11
#define LETTER_S 12
#define LETTER_E 13
#define LETTER_T 14
#define LETTER_P 15
#define LETTER_M 16
#define LETTER_F 17
#define LETTER_A 18

// variables for alarm
volatile int alarm_is_set = 0;
volatile uint8_t alarm_hour = 0;
volatile uint8_t alarm_minute = 0;

// variables for display
volatile char display[4]; // 4 digits, dot handled below
volatile int dot_on = 0;
volatile int brightness = 7; // range 0-7

// variables for PWM and switching digits
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

	if (left == -1) {
		dig0 = EMPTY_DIGIT;
		dig1 = EMPTY_DIGIT;
	}
	if (right == -1) {
		dig2 = EMPTY_DIGIT;
		dig3 = EMPTY_DIGIT;
	}

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
	uint8_t hour, minute, second, dummy;
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);

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
		if (flip)
			set_display_two_digits(hour, minute, 1);
		else
			set_display_two_digits(-1, minute, 1);
		flip = !flip;
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
		if (flip)
			set_display_two_digits(hour, minute, 1);
		else
			set_display_two_digits(hour, -1, 1);
		flip = !flip;
	}

	delay_ms(DEBOUNCE_DELAY);

	*out_hour = hour;
	*out_minute = minute;

}

void set_time() {
	uint8_t dummy;
	uint8_t hour = 0;
	uint8_t minute = 0;

	set_display_each_digit(LETTER_S, LETTER_E, LETTER_T, EMPTY_DIGIT, 0);
	delay_ms(500);
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &dummy);

	int new_hour, new_minute;
	get_time_from_user(hour, minute, &new_hour, &new_minute);
	ds1307_setdate(dummy, dummy, dummy, new_hour, new_minute, 0);
}

void set_or_turn_off_alarm() {
	uint8_t dummy;
	uint8_t hour = 0;
	uint8_t minute = 0;

	if (alarm_is_set) { // then turn it off
		set_display_each_digit(0, LETTER_F, LETTER_F, EMPTY_DIGIT, 0);
		delay_ms(1000);
		alarm_is_set = 0;
	} else { // turn it on
		alarm_is_set = 1;
		set_display_each_digit(LETTER_A, 1, LETTER_A, EMPTY_DIGIT, 0);
		delay_ms(1000);
		ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &dummy);
		get_time_from_user(hour, minute, &alarm_hour, &alarm_minute);
		set_display_each_digit(0, LETTER_M, EMPTY_DIGIT, EMPTY_DIGIT, 0);
		delay_ms(1000);
	}
}

void ring_alarm() {
	while (CONF_BUTTON_SETALARM_PIN & _BV(CONF_BUTTON_SETALARM_NUM)) {
		CONF_BUZZER_PORT ^= _BV(CONF_BUZZER_NUM);
		delay_ms(1000);
	}
	CONF_BUZZER_PORT |= _BV(CONF_BUZZER_NUM);
	alarm_is_set = 0;
	set_display_each_digit(0, LETTER_F, LETTER_F, EMPTY_DIGIT, 0);
	delay_ms(1000);
	alarm_is_set = 0;

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
	int should_be_full_brig = (CONF_JUMPER_BRIG_PIN
			& (1 << CONF_JUMPER_BRIG_NUM)) != 0;
	if (should_be_full_brig) {
		brightness = 7;
		return;
	}

	int reading = adc_read();

	if (reading >= 254)
		brightness = 2;
	else
		brightness = 7;
}

void check_and_alarm() {
	uint8_t year, month, day, hour, minute, second;
	ds1307_getdate(&year, &month, &day, &hour, &minute, &second);

	if (minute == alarm_minute && hour == alarm_hour && alarm_is_set == 1)
		ring_alarm();
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

	// buzzer
	CONF_BUZZER_DDR |= _BV(CONF_BUZZER_NUM);
	CONF_BUZZER_PORT |= _BV(CONF_BUZZER_NUM);

	// buttons
	CONF_BUTTON_SETTIME_DDR &= ~_BV(CONF_BUTTON_SETTIME_NUM);
	CONF_BUTTON_SETTIME_PORT |= _BV(CONF_BUTTON_SETTIME_NUM);

	CONF_BUTTON_UP_DDR &= ~_BV(CONF_BUTTON_UP_NUM);
	CONF_BUTTON_UP_PORT |= _BV(CONF_BUTTON_UP_NUM);

	CONF_BUTTON_DOWN_DDR &= ~_BV(CONF_BUTTON_DOWN_NUM);
	CONF_BUTTON_DOWN_PORT |= _BV(CONF_BUTTON_DOWN_NUM);

	CONF_BUTTON_DISPTEMP_DDR &= ~_BV(CONF_BUTTON_DISPTEMP_NUM);
	CONF_BUTTON_DISPTEMP_PORT |= _BV(CONF_BUTTON_DISPTEMP_NUM);

	CONF_BUTTON_SETALARM_DDR &= ~_BV(CONF_BUTTON_SETALARM_NUM);
	CONF_BUTTON_SETALARM_PORT |= _BV(CONF_BUTTON_SETALARM_NUM);

	CONF_JUMPER_BRIG_DDR &= ~_BV(CONF_JUMPER_BRIG_NUM);
	CONF_JUMPER_BRIG_PORT |= _BV(CONF_JUMPER_BRIG_NUM);

	CONF_JUMPER_TEMP_DDR &= ~_BV(CONF_JUMPER_TEMP_NUM);
	CONF_JUMPER_TEMP_PORT |= _BV(CONF_JUMPER_TEMP_NUM);

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

// all times in ms
#define UPDATE_INTERVAL_TIME 100
#define UPDATE_INTERVAL_TEMP 100 // delay already in temp_read()
#define REPEATS_TEMP 10 // delay already in temp_read()
#define UPDATE_INTERVAL_LIGHT 300
#define REPEATS_LIGHT 7 // delay already in temp_read()

	int temp_disp_counter = 0;
	while (1) {
		int j;

		// test place

		// test end

		disp_time();
		delay_ms(UPDATE_INTERVAL_TIME);
		regulate_brightness();

		if (!(CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM))) {
			set_time();
		}
		if (!(CONF_BUTTON_SETALARM_PIN & _BV(CONF_BUTTON_SETALARM_NUM))) {
			set_or_turn_off_alarm();
		}

		if (!(CONF_BUTTON_DISPTEMP_PIN & _BV(CONF_BUTTON_DISPTEMP_NUM))) {
			set_display_each_digit(LETTER_T, LETTER_E, LETTER_M, LETTER_P, 0);
			delay_ms(1000);
			for (j = 0; j < REPEATS_TEMP; ++j) {
				disp_temp();
				delay_ms(UPDATE_INTERVAL_TEMP);
			}
		}

		check_and_alarm();

		int should_be_displaying_temp = (CONF_JUMPER_TEMP_PIN
				& (1 << CONF_JUMPER_TEMP_NUM)) != 0;
		temp_disp_counter++;
		if (temp_disp_counter > 20 && should_be_displaying_temp) {
			temp_disp_counter = 0;
			int j;
			for (j = 0; j < 10; ++j) {
				disp_temp();
				delay_ms(UPDATE_INTERVAL_TEMP);
			}
		}

	}
}
