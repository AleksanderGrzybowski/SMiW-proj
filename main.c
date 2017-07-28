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

#define CONF_BUTTON_SETTIME_PORT PORTD
#define CONF_BUTTON_SETTIME_DDR  DDRD
#define CONF_BUTTON_SETTIME_PIN  PIND
#define CONF_BUTTON_SETTIME_NUM  1

#define CONF_BUTTON_DOWN_PORT PORTD
#define CONF_BUTTON_DOWN_DDR  DDRD
#define CONF_BUTTON_DOWN_PIN  PIND
#define CONF_BUTTON_DOWN_NUM  0

#define CONF_BUTTON_UP_PORT PORTD
#define CONF_BUTTON_UP_DDR  DDRD
#define CONF_BUTTON_UP_PIN  PIND
#define CONF_BUTTON_UP_NUM  2

/////////////////////////////////////////////////////

#define DOT 128
#define G 64
#define F 32
#define E 16
#define D 8
#define C 4
#define B 2
#define A 1
char tab[26] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0), (A + C + D + F + G), (A + D + E + F + G),
		(D + E + F + G), (A + B + E + F + G), (C + E + G), (A + E + F + G), (A
				+ B + C + E + F + G), (D + E + F), (B + C + D + E + G),
        (A+F+E+D), (B+C+D+E+F), (E+G), (E+F+G+B+C), (C+D+E)
};
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

#define LETTER_C 21
#define LETTER_U 22
#define LETTER_R 23
#define LETTER_H 24
#define LETTER_W 25

volatile uint16_t battery_alarm_threshold = 380;

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
	if (pwm_iter == 12) {
		pwm_iter = 0;
		cur_digit++;
	}
	if (cur_digit == 4) {
		cur_digit = 0;
	}

	/* turn off all digits to avoid ghosting */
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4);
    PORTD |= (1 << PD5) | (1 << PD6) | (1 << PD7);

    PORTC |= (1 << PC0);
    PORTC |= (1 << PC1);
    PORTD |= (1 << PD3);
    PORTD |= (1 << PD4);

	/* turn on/off current digit, based on pwm current value */
	if (pwm_iter < brightness) {
        // turn on
        if (cur_digit == 3) {
            PORTC &= ~(1 << PC0);
        }
        if (cur_digit == 1) {
            PORTC &= ~(1 << PC1);
        }
        if (cur_digit == 2) {
            PORTD &= ~(1 << PD3);
        }
        if (cur_digit == 0) {
            PORTD &= ~(1 << PD4);
        }
	} else {
        // turn off
        PORTC |= (1 << PC0);
        PORTC |= (1 << PC1);
        PORTD |= (1 << PD3);
        PORTD |= (1 << PD4);
	}

	/* select segments */

    uint8_t value = ~tab[display[cur_digit]];

    if (value & 1) {
        PORTD |= (1 << PD7);
    } else {
        PORTD &= ~(1 << PD7);
    }

    if (value & 2) {
        PORTD |= (1 << PD5);
    } else {
        PORTD &= ~(1 << PD5);
    }

    if (value & 4) {
        PORTB |= (1 << PB3);
    } else {
        PORTB &= ~(1 << PB3);
    }
    if (value & 8) {
        PORTB |= (1 << PB1);
    } else {
        PORTB &= ~(1 << PB1);
    }

    if (value & 16) {
        PORTB |= (1 << PB0);
    } else {
        PORTB &= ~(1 << PB0);
    }
    if (value & 32) {
        PORTD |= (1 << PD6);
    } else {
        PORTD &= ~(1 << PD6);
    }
    if (value & 64) {
        PORTB |= (1 << PB4);
    } else {
        PORTB &= ~(1 << PB4);
    }

	/* and dot if there is */
	if (cur_digit == 1 && dot_on) {
        PORTB &= ~(1 << PB2);
    } else {
        PORTB |= (1 << PB2);
    }

}

void adc_init()
{
    // AREF = AVcc
    ADMUX = (1<<REFS0);
 
    // ADC Enable and prescaler of 128
    // 16000000/128 = 125000
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

uint16_t adc_read(uint8_t ch)
{
  // select the corresponding channel 0~7
  // ANDing with ’7′ will always keep the value
  // of ‘ch’ between 0 and 7
  ch &= 0b00000111;  // AND operation with 7
  ADMUX = (ADMUX & 0xF8)|ch; // clears the bottom 3 bits before ORing
 
  // start single convertion
  // write ’1′ to ADSC
  ADCSRA |= (1<<ADSC);
 
  // wait for conversion to complete
  // ADSC becomes ’0′ again
  // till then, run loop continuously
  while(ADCSRA & (1<<ADSC));
 
  return (ADC);
}

uint16_t current_battery_voltage() { // if 3.8V -> 380
    int i;
    uint16_t read = 0;
    for (i = 0; i < 50; ++i) {
        read += adc_read(3);
        _delay_ms(10);
    }
    float battery_mv = 0.02 * 5.0 * read * 1000.0 / 1024.0;
    uint16_t battery_mv_int = (uint16_t)battery_mv;
    return battery_mv_int/10;
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
void set_display_whole_number(int number, int dot) {
	int d3 = number % 10;
	number /= 10;
	int d2 = number % 10;
	number /= 10;
	int d1 = number % 10;
	number /= 10;
	int d0 = number % 10;
	set_display_each_digit(d0 == 0 ? EMPTY_DIGIT: 0, d1, d2, d3, dot); // TODO fix this later for all cases
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
			if (brightness < 11) {
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
	delay_ms(DEBOUNCE_DELAY);
}

void show_current_battery_voltage_to_user() {
	set_display_each_digit(LETTER_C, LETTER_U, LETTER_R, LETTER_R, 0);
    delay_ms(1000);

	while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) { 
		delay_ms(DEBOUNCE_DELAY);
        uint16_t current_voltage = current_battery_voltage();
        set_display_whole_number(current_voltage, 1);
    }
}

void get_threshold_battery_from_user() {
	set_display_each_digit(LETTER_T, LETTER_H, LETTER_R, LETTER_S, 0);
    delay_ms(1000);

    int flip = 0;

	while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) {
		delay_ms(DEBOUNCE_DELAY);
		if (!(CONF_BUTTON_UP_PIN & _BV(CONF_BUTTON_UP_NUM))) {
            battery_alarm_threshold++;
			delay_ms(DEBOUNCE_DELAY);
		}
		if (!(CONF_BUTTON_DOWN_PIN & _BV(CONF_BUTTON_DOWN_NUM))) {
            battery_alarm_threshold--;
			delay_ms(DEBOUNCE_DELAY);
		}
        flip = !flip;
        if (flip) {
        set_display_whole_number(battery_alarm_threshold, 1);
        } else {
            set_display_two_digits(-1, -1, 1);
        }
	}
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


	DDRB = DDRC = DDRD = 0x00;
	PORTB = PORTC = PORTD = 0xff;


	/* enable timer overflow interrupts for PWM */
	TIMSK |= (1 << TOIE0);
	/* set prescaler, selected by experimenting, but works perfectly */
	TCCR0 &= ~(1 << CS02);
	TCCR0 |= (1 << CS01);
	TCCR0 |= (1 << CS00);
	/* not sure if needed TODO */
	sei();



	/* display outputs */
	//DDRB |= 0x0f; // common anodes
    DDRC |= (1 << PC0) | (1 << PC1);
    DDRD |= (1 << PD3) | (1 << PD4);
	//DDRD = 0xff; // segments
    DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4);
    DDRD |= (1 << PD5) | (1 << PD6) | (1 << PD7);

    //set_display_two_digits(12,34, 1);
    //_delay_ms(1000);



	/* buttons inputs */
	CONF_BUTTON_SETTIME_DDR &= ~_BV(CONF_BUTTON_SETTIME_NUM);
	CONF_BUTTON_SETTIME_PORT |= _BV(CONF_BUTTON_SETTIME_NUM);

	CONF_BUTTON_UP_DDR &= ~_BV(CONF_BUTTON_UP_NUM);
	CONF_BUTTON_UP_PORT |= _BV(CONF_BUTTON_UP_NUM);

	CONF_BUTTON_DOWN_DDR &= ~_BV(CONF_BUTTON_DOWN_NUM);
	CONF_BUTTON_DOWN_PORT |= _BV(CONF_BUTTON_DOWN_NUM);


    DDRC &= ~(1 << PC3);
    PORTC &= ~(1 << PC3);
    adc_init();
    //while(1) {
    //}

	/* start RTC and check if it ticks
	 * if battery is empty then it won't start and will be stuck at 0:00:00
	 * to force start, simply set any hour
	 */
	ds1307_init();
	uint8_t dummy, hour, minute, second;
	ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);
	if (hour == 0 && minute == 0 && second == 0) {
		ds1307_setdate(1, 1, 1, 0, 0, 0);
        set_display_each_digit(DASH, DASH, DASH, DASH, 1);
		delay_ms(2000);
	}


#define UPDATE_INTERVAL_TIME 200

    uint16_t battery_check_counter = 0;
	/* main loop of the program */
	while (1) {
		disp_time();
		delay_ms(UPDATE_INTERVAL_TIME);

		if (!(CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM))) {
			set_time();
			get_brightness_from_user();
			show_current_battery_voltage_to_user();
			get_threshold_battery_from_user();
		}

        battery_check_counter++;
        if (battery_check_counter == 5 * 60 * 30) {
            battery_check_counter = 0;
            if (current_battery_voltage() < battery_alarm_threshold) {
                set_display_each_digit(LETTER_L, 0, LETTER_W, EMPTY_DIGIT, 0);
                delay_ms(1000 * 60 * 5);
            }
        }
	}
}
