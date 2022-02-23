/* CREATE BY NGUYEN NHAT QUAN */
#ifndef __DHT22_H
#define __DHT22_H

#include "stm32f1xx.h"

typedef struct 
{
		float temperature;
		float humidity;
}DHT22_Data_t;

//void Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
//void Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
//void DHT22_Start (void);
//uint8_t DHT22_Check_Response (void);
//uint8_t DHT22_Read (void);
DHT22_Data_t DHT22_Read_Param(void);
#endif


