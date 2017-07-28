//#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/eeprom.h>
#include "ds1307.h"


void delay_ms(uint32_t count) {
    while (count--) {
        _delay_ms(1);
    }
}

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

char tab[26] = {
    (A + B + C + D + E + F),
    (B + C),
    (A + B + G + E + D),
    (A + B + G + C + D),
    (F + G + B + C),
    (A + F + G + C + D),
    (A + F + G + E + D + C),
    (F + A + B + C),
    (A + B + C + D + E + F + G),
    (A + B + C + D + F + G),
    (B),
    (0),
    (A + C + D + F + G),
    (A + D + E + F + G),
    (D + E + F + G),
    (A + B + E + F + G),
    (C + E + G),
    (A + E + F + G),
    (A + B + C + E + F + G),
    (D + E + F),
    (B + C + D + E + G),
    (A + F + E + D),
    (B + C + D + E + F),
    (E + G),
    (E + F + G + B + C),
    (C + D + E)
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

/////////////////////////////////////////////////////
#define BATTERY_ADC_CHANNEL 3

#define MAX_PWM_BRIGHTNESS 12
#define NIGHT_BRIGHTNESS 1

// 370 means 3.70V - easy format for displaying
volatile uint16_t battery_alarm_threshold = 350;

volatile uint8_t display[4];
volatile uint8_t dot_on = 0;
volatile uint8_t brightness = MAX_PWM_BRIGHTNESS / 2;
volatile uint8_t is_night = 0;

/* variables for PWM and switching digits */
volatile uint8_t pwm_iter = 0;
volatile uint8_t current_digit = 0; // current displayed digit

ISR(TIMER0_OVF_vect) {

    /* regulate speed of multiplexing, less = slower
     * this is way simpler than fiddling with prescalers :)
     */
    TCNT0 = 250;

    pwm_iter++;
    if (pwm_iter == MAX_PWM_BRIGHTNESS) {
        pwm_iter = 0;
        current_digit++;
    }
    if (current_digit == 4) {
        current_digit = 0;
    }

    /* prevent ghosting */
    PORTB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4);
    PORTD |= (1 << PD5) | (1 << PD6) | (1 << PD7);

    PORTC |= (1 << PC0) | (1 << PC1);
    PORTD |= (1 << PD3) | (1 << PD4);

    /* turn on/off current digit, based on pwm current value */
    int real_brightness = is_night ? NIGHT_BRIGHTNESS : brightness;
    if (pwm_iter < real_brightness) {
        if (current_digit == 3) {
            PORTC &= ~(1 << PC0);
        }
        if (current_digit == 1) {
            PORTC &= ~(1 << PC1);
        }
        if (current_digit == 2) {
            PORTD &= ~(1 << PD3);
        }
        if (current_digit == 0) {
            PORTD &= ~(1 << PD4);
        }
    } 

    /* select segments */
    uint8_t value = ~tab[display[current_digit]];

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
    if (current_digit == 1 && dot_on) {
        PORTB &= ~(1 << PB2);
    } else {
        PORTB |= (1 << PB2);
    }
}

void adc_init() {
    // AREF = AVcc
    ADMUX = (1 << REFS0);
 
    // 128 prescaler
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(uint8_t ch) {
    ch &= 0b00000111;
    ADMUX = (ADMUX & 0xF8) | ch;
    ADCSRA |= (1 << ADSC);
 
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

#define ADC_SAMPLES 50
#define ADC_SAMPLING_DELAY_MS 10
#define ADC_VREF 5.0

uint16_t current_battery_voltage() { // if 3.8V -> return 380
    uint8_t i;
    uint16_t sum = 0;

    for (i = 0; i < ADC_SAMPLES; ++i) {
        sum += adc_read(BATTERY_ADC_CHANNEL);
        _delay_ms(ADC_SAMPLING_DELAY_MS);
    }

    uint16_t battery_mv = (uint16_t) (sum * ADC_VREF * 1000.0 / (1024.0 * ADC_SAMPLES));
    return battery_mv / 10;
}

/* set display's digits, each digit separately, useful for debugging and used by other funcs */
void set_display_each_digit(uint8_t dig0, uint8_t dig1, uint8_t dig2, uint8_t dig3, uint8_t dot) {
    display[0] = dig0;
    display[1] = dig1;
    display[2] = dig2;
    display[3] = dig3;
    dot_on = dot;
}

/* set display's digits, two numbers in range <0..99> without leading '0'
 * if given -1, it will display empty digit (no segments lit)
 */
void set_display_two_digits(int8_t left, int8_t right, int8_t dot) {
    uint8_t dig0, dig1, dig2, dig3;
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
void set_display_whole_number(uint16_t number, uint8_t dot) {
    uint8_t d3 = number % 10;
    number /= 10;
    uint8_t d2 = number % 10;
    number /= 10;
    uint8_t d1 = number % 10;
    number /= 10;
    uint8_t d0 = number % 10;

    if (d0 == 0) {
        d0 = EMPTY_DIGIT;
        if (d1 == 0) {
            d1 = EMPTY_DIGIT;
            if (d2 == 0) {
                d2 = EMPTY_DIGIT;
            }
        }
    }

    set_display_each_digit(d0, d1, d2, d3, dot);
}

/* display current time */
void disp_time_and_update_night_status() {
    uint8_t hour, minute, second, dummy;
    ds1307_getdate(&dummy, &dummy, &dummy, &hour, &minute, &second);

    set_display_two_digits(hour, minute, (second % 2 == 1));
    is_night = (hour >= 0 && hour <= 4);
}

/* ask user to input time */
#define DEBOUNCE_DELAY 100 // ms
void get_time_from_user(uint8_t hour, uint8_t minute, uint8_t* out_hour, uint8_t* out_minute) {

    int blink = 0;

    while (CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM)) { // wait for hours
        delay_ms(DEBOUNCE_DELAY);
        if (!(CONF_BUTTON_UP_PIN & _BV(CONF_BUTTON_UP_NUM))) {
            hour++;
            if (hour == 24) {
                hour = 0;
            }
            delay_ms(DEBOUNCE_DELAY);
        }
        if (!(CONF_BUTTON_DOWN_PIN & _BV(CONF_BUTTON_DOWN_NUM))) {
            if (hour == 0) {
                hour = 23;
            } else {
                hour--;
            }

            delay_ms(DEBOUNCE_DELAY);
        }
        if (blink >= 2) {
            set_display_two_digits(hour, minute, 1);
        } else {
            set_display_two_digits(-1, minute, 1);
        }

        blink++;
        if (blink == 4) {
            blink = 0;
        }
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
            if (minute == 0) {
                minute = 59;
            } else {
                minute--;
            }
            delay_ms(DEBOUNCE_DELAY);
        }
        if (blink >= 2)
            set_display_two_digits(hour, minute, 1);
        else
            set_display_two_digits(hour, -1, 1);
        blink++;
        if (blink == 4)
            blink = 0;
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
            if (brightness < (MAX_PWM_BRIGHTNESS - 1)) {
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

    int blink = 0;

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

        blink = !blink;
        if (blink) {
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

    uint8_t new_hour, new_minute;
    get_time_from_user(hour, minute, &new_hour, &new_minute);
    ds1307_setdate(1, 1, 1, new_hour, new_minute, 0);
}

int main() {

    /* set all pins as inputs with pullups, so accidental short is not a problem */
    DDRB = DDRC = DDRD = 0x00;
    PORTB = PORTC = PORTD = 0xff;

    /* enable timer overflow interrupts for PWM */
    TIMSK |= (1 << TOIE0);
    TCCR0 &= ~(1 << CS02);
    TCCR0 |= (1 << CS01);
    TCCR0 |= (1 << CS00);
    sei();

    /* display outputs */
    DDRC |= (1 << PC0) | (1 << PC1);
    DDRD |= (1 << PD3) | (1 << PD4);
    DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB4);
    DDRD |= (1 << PD5) | (1 << PD6) | (1 << PD7);

    /* buttons inputs */
    CONF_BUTTON_SETTIME_DDR &= ~_BV(CONF_BUTTON_SETTIME_NUM);
    CONF_BUTTON_SETTIME_PORT |= _BV(CONF_BUTTON_SETTIME_NUM);

    CONF_BUTTON_UP_DDR &= ~_BV(CONF_BUTTON_UP_NUM);
    CONF_BUTTON_UP_PORT |= _BV(CONF_BUTTON_UP_NUM);

    CONF_BUTTON_DOWN_DDR &= ~_BV(CONF_BUTTON_DOWN_NUM);
    CONF_BUTTON_DOWN_PORT |= _BV(CONF_BUTTON_DOWN_NUM);


    // ADC input
    DDRC &= ~(1 << PC3);
    PORTC &= ~(1 << PC3);
    adc_init();

    /* start RTC and check if it ticks, it might be stuck on cold start, 
     * so to force start, simply set any hour
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
#define BATTERY_LOW_MESSAGE_TIME 1000L * 10 // 10 seconds
#define BATTERY_CHECK_INTERVAL (1000 / UPDATE_INTERVAL_TIME) * 60 // check every minute

    uint16_t battery_check_counter = 0;

    while (1) {
        disp_time_and_update_night_status();
        delay_ms(UPDATE_INTERVAL_TIME);

        if (!(CONF_BUTTON_SETTIME_PIN & _BV(CONF_BUTTON_SETTIME_NUM))) {
            set_time();
            get_brightness_from_user();
            show_current_battery_voltage_to_user();
            get_threshold_battery_from_user();
        }

        battery_check_counter++;
        if (battery_check_counter == BATTERY_CHECK_INTERVAL) {
            battery_check_counter = 0;
            if (current_battery_voltage() < battery_alarm_threshold) {
                set_display_each_digit(LETTER_L, 0, LETTER_W, EMPTY_DIGIT, 0);
                delay_ms(BATTERY_LOW_MESSAGE_TIME);
            }
        }
    }
}
