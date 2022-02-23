
/* CREATE BY NGUYEN NHAT QUAN */
#include "DHT22.h"

#define DHT22_PORT GPIOA
#define DHT22_PIN  GPIO_PIN_1


uint8_t Rh_byte1, Rh_byte2, Temp_byte1, Temp_byte2;
uint16_t SUM, RH, TEMP;

float Temperature = 0;
float Humidity = 0;
uint8_t Presence = 0;


extern TIM_HandleTypeDef htim1;

static void delay_us (uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim1,0);  												// set the counter value a 0
    while (__HAL_TIM_GET_COUNTER(&htim1) < us);  						// wait for the counter to reach the us input in the parameter
}

static void delay_ms (uint16_t ms)
{
		for(uint16_t i=0;i<ms;i++)
		{
			delay_us(1000);
		}
}

void Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}


//Send Start Signal 18ms
void DHT22_Start (void)
{
	Set_Pin_Output(DHT22_PORT, DHT22_PIN);          // set the pin as output
	HAL_GPIO_WritePin (DHT22_PORT, DHT22_PIN, 0);   // pull the pin low
	delay_us(1200);   // wait for at least 1ms (18ms) 

	HAL_GPIO_WritePin (DHT22_PORT, DHT22_PIN, 1);   // pull the pin high
	delay_us (20);    // wait for 30us
	Set_Pin_Input(DHT22_PORT, DHT22_PIN);           // set as input
}

uint8_t DHT22_Check_Response (void)
{
	uint8_t Response = 0;
	delay_us (40);  		// wait for 40us
	if (!(HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN))) // if the pin is low
	{
		delay_us (80);    // wait for 80us

		if ((HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN))) Response = 1;  // if the pin is high, response is ok
		else Response = -1;
	}

	while ((HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN)));   // wait for the pin to go low
	return Response;
}

uint8_t DHT22_Read (void)
{
	uint8_t i,j;
	for (j=0;j<8;j++)
	{
		while (!(HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN)));   // wait for the pin to go high
		delay_us (40);       // wait for 40 us

		if (!(HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN)))   // if the pin is low
		{
			i&= ~(1<<(7-j));   // write 0
		}
		else i|= (1<<(7-j));  // if the pin is high, write 1
		while ((HAL_GPIO_ReadPin (DHT22_PORT, DHT22_PIN)));  // wait for the pin to go low
	}

	return i;
}

DHT22_Data_t DHT22_Read_Param()
{
		DHT22_Data_t user;
		DHT22_Start();
	  Presence = DHT22_Check_Response();
	  Rh_byte1 = DHT22_Read ();
    Rh_byte2 = DHT22_Read ();
	  Temp_byte1 = DHT22_Read ();
	  Temp_byte2 = DHT22_Read ();
	  SUM = DHT22_Read();

	  TEMP = ((Temp_byte1<<8)|Temp_byte2);
	  RH = ((Rh_byte1<<8)|Rh_byte2);
	  user.temperature = (float) (TEMP/10.0);
	  user.humidity = (float) (RH/10.0);
		return user;
}
