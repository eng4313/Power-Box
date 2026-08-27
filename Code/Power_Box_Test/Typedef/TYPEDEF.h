#ifndef TYPEDEF
	#define TYPEDEF
	#include "main.h"
	
#define CABINET_COUNT  4U

typedef union
{
	uint8_t all;   /* All 8 bits as a single byte */
	struct
	{
		uint8_t blinker 										: 1;  	/* Bit 0: Blink status */
		uint8_t empty   										: 1;   	/* Bit 1: Empty status */
		uint8_t reserved1 									: 1; 		/* Bit 2: Reserved */
		uint8_t reserved2 									: 1; 		/* Bit 3: Reserved */
		uint8_t reserved3 									: 1; 		/* Bit 4: Reserved */
		uint8_t reserved4 									: 1; 		/* Bit 5: Reserved */
		uint8_t reserved5 									: 1; 		/* Bit 6: Reserved */
		uint8_t reserved6 									: 1; 		/* Bit 7: Reserved */
	} bits;
} Cabinet_Status_t;

typedef struct
{
	GPIO_TypeDef* 		lock_port;
	uint16_t      		lock_pin;
	GPIO_TypeDef* 		led_port;
	uint16_t      		led_pin;
	GPIO_TypeDef* 		door_port;
	uint16_t      		door_pin;
} Cabinet_Pins_t;



#endif


