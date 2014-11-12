#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "HD44780.h"
#include <stdio.h>
#include "ds1307.h"
#include "ds18b20.h"
#include <avr/wdt.h>

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

#define DOT 128
#define G 64
#define F 32
#define E 16
#define D 8
#define C 4
#define B 2
#define A 1
char tab[12] = { (A + B + C + D + E + F), (B + C), (A + B + G + E + D), (A + B
		+ G + C + D), (F + G + B + C), (A + F + G + C + D), (A + F + G + E + D
		+ C), (F + A + B + C), (A + B + C + D + E + F + G), (A + B + C + D + F
		+ G), (B), (0) };

volatile int cur_digit = 0;
volatile char display[4]; // 4 cyfry, na razie bez kropki
volatile int dot_on = 0;

ISR(TIMER0_OVF_vect) {

//	TCNT0 = 210;
	// TEST ROUTINE, WORKS:
	//	cur++;
	//	transmit(cur);
	//	transmit(2);
	//	commit();

	// gaś poprzednie
	PORTB |= (1 << cur_digit);
	cur_digit++;
	if (cur_digit == 4) {
		cur_digit = 0;
	}
	// zapal następne
	PORTB &= ~(1 << cur_digit);

	PORTA = tab[display[cur_digit]];
	if (cur_digit == 1 && dot_on)
		PORTA |= DOT;

//	PORTB = 0;
//	PORTA = 0x00;

}

void disp_time(int howlong) {

	int i;
	for (i = 0; i < howlong; ++i) {
		uint8_t year, month, day, hour, minute, second;
		ds1307_getdate(&year, &month, &day, &hour, &minute, &second);
		display[0] = hour / 10;
		display[1] = hour % 10;
		display[2] = minute / 10;
		display[3] = minute % 10;

		dot_on = (second % 2 == 0);
		delay_ms(100);
	}

}

void disp_temp(int howlong) {

	int i;
	int itemp;
	for (i = 0; i < howlong; ++i) {
		volatile int itemp = (int) (DS18B20_temp() * 10);

//		if (itemp > 350) itemp = oldtemp;

		int d2 = itemp % 10;
		itemp /= 10;
		int d1 = itemp % 10;
		itemp /= 10;
		int d0 = itemp % 10;

		display[0] = d0;
		display[1] = d1;
		display[2] = d2;
		display[3] = 10;
		dot_on = 1;

		delay_ms(1000);

	}
}

int main() {

	MCUCSR |= (1 << JTD);
	MCUCSR |= (1 << JTD);

	TIMSK |= (1 << TOIE0);
//	TCCR0 |= (1 << CS00) | (1 << CS01); // 64 jak na razie
	TCCR0 |= (1 << CS02);
	TCCR0 &= ~(1 << CS01);
	TCCR0 &= ~(1 << CS00);
	sei();

	DDRA = 0xff;
	DDRB = 0xff;

//	wdt_disable();

	PORTA = 0xff;
	PORTA = 0xaa;
	PORTB = 0;
//	while(1) {
//		PORTA++;
//		delay_ms(1000);
//	}

	ds1307_init();
	uint8_t year, month, day, hour, minute, second;

	ds1307_getdate(&year, &month, &day, &hour, &minute, &second);
	if (hour == 0 && minute == 0 && second == 0) {
		ds1307_setdate(1, 1, 1, 13, 13, 00); // obejście braku startu po zaniku VBAT
	}

	// main loop!


	ds1307_setdate(1, 1, 1, 13, 13, 00); // obejście braku startu po zaniku VBAT

	char tbuf[10] = { 0, };
	while (1) {

		disp_time(50);
		disp_temp(2);
	}
}

