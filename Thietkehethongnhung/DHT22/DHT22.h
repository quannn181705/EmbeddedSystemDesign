/*CREATE BY NGUYEN NHAT QUAN */

#ifndef __DHT22_H
#define __DHT22_H
#include "stm32f1xx.h"
typedef struct 
{
		float temperature;
		float humidity;
}DHT22_Data_t;

void DHT22_Set_Timer(void *htim);
DHT22_Data_t DHT22_Read_Param(void);
#endif

