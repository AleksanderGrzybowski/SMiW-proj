#define F_CPU 16000000UL // 16 MHz clock

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include "ds1307.h"
#include "ds18b20.h"

/* Function to do simple active blocking delay */
void delay_ms(uint16_t count) {
	while (count--) {
		_delay_ms(1);
	}
}

/* Config */
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

/*
 * Constants decribing segments on 7-seg display.
 * Segments are connected to whole PORTD (this port is hardcoded in ISR) through N-type transistors.
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
char tab[20] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0), (A + C + D + F + G), (A + D + E + F + G),
		(D + E + F + G), (A + B + E + F + G), (C + E + G), (A + E + F + G), (A
				+ B + C + E + F + G), (D + E + F) };
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

/* variables for alarm */
volatile int alarm_is_set = 0;
volatile uint8_t alarm_hour = 0;
volatile uint8_t alarm_minute = 0;

/* variables for display */
volatile char display[4]; // 4 digits, dot handled below
volatile int dot_on = 0; // 1 = dot is on, 0 = dot is off
volatile int brightness = 7; // range <0-7>, 0 = none, 7 = maximum

/* variables for PWM and switching digits */
volatile int cur_digit = 0; // current displayed digit
volatile int pwm_iter = 0; // current PWM iteration in range <0..7>

/* ISR used to multiplex 7-seg 4-digit display */
ISR(TIMER0_OVF_vect) {

	/* regulate speed of multiplexing, less = slower
	 * this is way simpler than fiddling with prescalers :)
	 */
	TCNT0 = 220;

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
	PORTD = 0x00;

	/* turn on/off current digit, based on pwm current value */
	if (pwm_iter < brightness) {
		PORTB &= ~(1 << cur_digit);
	} else {
		PORTB |= (1 << cur_digit);
	}

	/* select segments */
	PORTD = tab[display[cur_digit]];
	/* and dot if there is */
	if (cur_digit == 1 && dot_on)
		PORTD |= DOT;
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

/* display current temperature */
void disp_temp() {
	unsigned char ds18b20_pad[9];

	ds18b20_ConvertT();
	_delay_ms(750); // shows garbage without it, read the docs!!!
	ds18b20_Read(ds18b20_pad);

	float temp = ((ds18b20_pad[1] << 8) + ds18b20_pad[0]) / 16.0;

	/* we want 1 decimal place after the dot, so the simplest way
	 * to do that is multiply the reading (as float) by 10 and show it as 3-digit integer
	 */
	int itemp = (int) (temp * 10.0);

	int d2 = itemp % 10;
	itemp /= 10;
	int d1 = itemp % 10;
	itemp /= 10;
	int d0 = itemp % 10;

	set_display_each_digit(d0, d1, d2, DASH, 1);
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

/* if alarm is on, turn it off, otherwise, turn it on */
void set_or_turn_off_alarm() {
	uint8_t dummy;
	uint8_t hour = 0;
	uint8_t minute = 0;

	if (alarm_is_set) { // then turn it off
		set_display_each_digit(LETTER_O, LETTER_F, LETTER_F, EMPTY_DIGIT, 0);
		delay_ms(1000);
		alarm_is_set = 0;
	} else { // turn it on
		alarm_is_set = 1;
		set_display_each_digit(LETTER_A, LETTER_L, LETTER_A, EMPTY_DIGIT, 0);
		delay_ms(1000);
		ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &dummy);
		get_time_from_user(hour, minute, &alarm_hour, &alarm_minute); // modify global state!
		set_display_each_digit(LETTER_O, LETTER_M, EMPTY_DIGIT, EMPTY_DIGIT, 0);
		delay_ms(1000);
	}
}

/* make some noise while waiting for the user to turn off the alarm
 * using OFF button
 */
void ring_alarm() {
	disp_time();
	while (CONF_BUTTON_SETALARM_PIN & _BV(CONF_BUTTON_SETALARM_NUM)) {
		CONF_BUZZER_PORT ^= _BV(CONF_BUZZER_NUM);
		delay_ms(200);
	}
	CONF_BUZZER_PORT &= ~_BV(CONF_BUZZER_NUM);
	alarm_is_set = 0;
	set_display_each_digit(LETTER_O, LETTER_F, LETTER_F, EMPTY_DIGIT, 0);
	delay_ms(1000);
	alarm_is_set = 0;
}

/* function to read light reading from ADC, copied from the internet
 * comments below are not mine :)
 */
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

/* display light intensity reading, not used in production */
void disp_light() {
	int res = adc_read();
	set_display_whole_number(res);
}

/* read light intensity and set PWM threshold value accordingly */
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

/* check if there should be an alarm ringing
 * if it is the case, ring
 */
void check_and_alarm() {
	uint8_t year, month, day, hour, minute, second;
	ds1307_getdate(&year, &month, &day, &hour, &minute, &second);

	if (minute == alarm_minute && hour == alarm_hour && alarm_is_set == 1)
		ring_alarm();
}

int main() {

	/* set all pins as inputs with pullups
	 * so accidental short to vcc/gnd won't destroy uC
	 */
	DDRA = DDRB = DDRC = DDRD = 0x00;
	PORTA = PORTB = PORTC = PORTD = 0xff;

	/* disable JTAG so we can use all pins */
	MCUCSR |= (1 << JTD);
	MCUCSR |= (1 << JTD);

	/* enable timer overflow interrupts for PWM */
	TIMSK |= (1 << TOIE0);
	/* set prescaler, selected by experimenting, but works perfectly */
	TCCR0 |= (1 << CS02);
	TCCR0 &= ~(1 << CS01);
	TCCR0 &= ~(1 << CS00);
	/* not sure if needed TODO */
	sei();

	/* display outputs */
	DDRB |= 0x0f; // common anodes
	DDRD = 0xff; // segments

	/* buzzer output */
	CONF_BUZZER_DDR |= _BV(CONF_BUZZER_NUM);
	CONF_BUZZER_PORT &= ~_BV(CONF_BUZZER_NUM);

	/* buttons inputs */
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

	/* ADC: AREF = AVcc */
	ADMUX = (1 << REFS0);
	/* Enable and set prescaler = 128 */
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADMUX |= (1 << ADLAR);

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
#define UPDATE_INTERVAL_TEMP 100 // + 750 ms in temp_read()
#define REPEATS_TIME 40 // delay already in temp_read()
#define REPEATS_TEMP 3 // delay already in temp_read()

	int counter = 0;

	/* main loop of the program */
	while (1) {

		int should_be_displaying_temp = (CONF_JUMPER_TEMP_PIN
				& (1 << CONF_JUMPER_TEMP_NUM)) != 0;

		if (counter < REPEATS_TIME)
			disp_time();
		else if ((counter < (REPEATS_TEMP + REPEATS_TIME)) && should_be_displaying_temp)
			disp_temp();
		else
			counter = -1;

		counter++;
		delay_ms(UPDATE_INTERVAL_TIME);

		regulate_brightness();
		check_and_alarm();

		if (!(CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM))) {
			set_time();
			counter = 0;
		}
		if (!(CONF_BUTTON_SETALARM_PIN & _BV(CONF_BUTTON_SETALARM_NUM))) {
			set_or_turn_off_alarm();
			counter = 0;
		}

		if (!(CONF_BUTTON_DISPTEMP_PIN & _BV(CONF_BUTTON_DISPTEMP_NUM))) {
			set_display_each_digit(LETTER_T, LETTER_E, LETTER_M, LETTER_P, 0);
			delay_ms(1000);
			int j;
			for (j = 0; j < REPEATS_TEMP; ++j) {
				disp_temp();
				regulate_brightness();
				delay_ms(UPDATE_INTERVAL_TEMP);
			}
			counter = 0;
		}
	}
}
