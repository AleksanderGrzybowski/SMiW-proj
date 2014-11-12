#include "ds18b20.h"
#include <math.h>

/*________________________________________________________________________

		Funkcja resetujaca linie 1 - Wire
________________________________________________________________________*/

uint8_t ow_reset(void)
{		
	OW_OUT_LOW(); 
	OW_DIR_OUT(); 
	delay_us(480);
	OW_DIR_IN(); 	
	delay_us(66);
	delay_us(480-66);
	return 0;
}

/*________________________________________________________________________

		Funkcja wysy�ajaca bit danych
________________________________________________________________________*/

uint8_t ow_bit_io( uint8_t b )
{

	OW_DIR_OUT(); 
	delay_us(1); 
	if ( b ) 
		OW_DIR_IN();			
	delay_us(15-1-OW_CONF_DELAYOFFSET);		
	if( OW_GET_IN() == 0 ) 
		b = 0;  	
	delay_us(60-15);
	OW_DIR_IN();		
	return b;
}

/*________________________________________________________________________

		Funkcja wysy�ajaca bajt danych
________________________________________________________________________*/

uint8_t ow_byte_wr( uint8_t b )
{
	uint8_t i = 8, j;
	
	do {
		j = ow_bit_io( b & 1 );
		b >>= 1;
		if( j ) b |= 0x80;
	} while( --i );
	
	return b;
}

/*________________________________________________________________________

		Funkcja odczytujaca bajt danych
________________________________________________________________________*/

uint8_t ow_byte_rd( void )
{
  return ow_byte_wr( 0xFF ); 
}

/*________________________________________________________________________

		Funkcja odczytujaca temperature
________________________________________________________________________*/

float DS18B20_temp(void)
{

   	char buf[8], x, i;
  	uint8_t temp1, temp2, 
   	ow_reset(); 
	end:
//   	while(x != 0x01)
//	{
        ow_byte_wr(0xCC);   //bez identyfikacji urzadzenia
        ow_byte_wr(0x44);   //inicjalizacja temperatury
       	ow_reset();
        ow_byte_wr(0xCC);
        ow_byte_wr(0xBE);
        temp1 = ow_byte_rd();   //lsb
        temp2 = ow_byte_rd();   //mmsb
       	ow_reset();
       	float temp=0;
        temp=(float)((temp1+(temp2*256.0))/16.0);
//        dtostrf(temp, 1, 3, buf);   //makefile w 7 linijce LIBS=-lm coby dzialalo
//        LCD_WriteText(buf);
//		LCD_putstrxy("Temp:", 4, 0);
//		LCD_putstrxy(buf,7,2);
//		LCD_xy(11,2);
//		LCD_send_1(0xDF);
//		LCD_putstrxy("C", 12, 2);
        return temp;
  		
// 	}
     
} 

/*________________________________________________________________________

		Funkcja odczytujaca raz temperature
________________________________________________________________________*/
//
//uint8_t DS18B20_temp_raz()
//{
//
//   	char buf[8], x, i;
//  	uint8_t temp1, temp2;
//   	ow_reset();
//	end:
//    ow_byte_wr(0xCC);
//    ow_byte_wr(0x44);
//	delay_ms(250);
//	delay_ms(250);
//	delay_ms(250);
//   	ow_reset();
//    ow_byte_wr(0xCC);
//    ow_byte_wr(0xBE);
//    temp1 = ow_byte_rd();   //lsb
//    temp2 = ow_byte_rd();   //mmsb
//   	ow_reset();
//    float temp=0;
//    temp=(float)(temp1+(temp2*256))/16;
////	strtoi();
//    dtostrf(temp, 1, 1, buf);   //zamienia ASCI na stringa
//	LCD_putstrxy("Temp: ", 0, 0);
//	LCD_putstrxy(buf,6,0);
//	LCD_xy(10,0);
//	LCD_send_1(0xDF);
//	LCD_putstrxy("C", 11, 0);
///////////przeliczanie bitow na liczbe//////////////
////	if(temp2>128) { temp1=255-temp1; temp2=255-temp2; } //wykrycie ujemnej T
//	//tk=(temp1&0x0F)*0.0625; //< ulamki
//	temp1=temp1>>4;
//	temp2=temp2<<4;// przesuniecia bitowe o 4pozycje
//	temp=temp1+temp2;
//////////////koniec przeliczania/////////////
//	return temp;
//}


