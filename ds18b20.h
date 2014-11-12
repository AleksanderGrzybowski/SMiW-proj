#include <avr/io.h>
#include <inttypes.h>
#include <stdlib.h> 
#include <stdio.h> 

#include "HD44780.h"

#define OW_PIN  			PC2
#define OW_IN   			PINC
#define OW_OUT  			PORTC
#define OW_DDR  			DDRC
#define OW_CONF_DELAYOFFSET 0

#define F_OSC 8000000
#define OW_CONF_CYCLESPERACCESS 13
//#define OW_CONF_DELAYOFFSET ( (uint16_t)( ((OW_CONF_CYCLESPERACCESS)*1000000L) / F_OSC  ) )

uint8_t ow_reset(void);
uint8_t ow_bit_io( uint8_t b );
uint8_t ow_byte_wr( uint8_t b );
uint8_t ow_byte_rd( void );

#define OW_GET_IN()   ( OW_IN & (1<<OW_PIN)) //ustawia na pa 1 jedynke
#define OW_OUT_LOW()  ( OW_OUT &= (~(1 << OW_PIN)) )  // ustawia 0 na pinie 1
#define OW_OUT_HIGH() ( OW_OUT |= (1 << OW_PIN) )
#define OW_DIR_IN()   ( OW_DDR &= (~(1 << OW_PIN )) )
#define OW_DIR_OUT()  ( OW_DDR |= (1 << OW_PIN))

float DS18B20_temp(void);




