/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include <stdarg.h>
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "i2c-lcd.h"
#include "HCSR04.h"
#include "DHT22.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//I. Global variable
uint32_t idle_count=0;			// for IdleTask
uint8_t temp,humi,dist;
DHT22_Data_t data;

int fputc(int c, FILE * stream)
{
		HAL_UART_Transmit(&huart1,(uint8_t*)&c,1,10);
		return 0;
}

uint8_t debug_tx_buffer[100];
void print(char *fmt,...)
{
		while(huart1.gState!=HAL_UART_STATE_READY);
		va_list args;
		va_start(args,fmt);
		vsprintf((char*)debug_tx_buffer,fmt,args);
		va_end(args);
		HAL_UART_Transmit_DMA(&huart1,debug_tx_buffer,strlen((const char *)debug_tx_buffer));
}

SemaphoreHandle_t xTaskSemaphore;
SemaphoreHandle_t xLCDSemaphore;
QueueHandle_t     xTaskQueue;
TimerHandle_t 		xTaskTimer;


TaskHandle_t LCD_Handle;																								// Priority=5
TaskHandle_t UART_Handle;																								// Priority=4
TaskHandle_t DHT22_Sensor;																							// Priority=2
TaskHandle_t HCSR04_Sensor;																							// Priority=1
TaskHandle_t Idle_Handle;																								// Priority=0


void LCD_Entry(void *pvParameter);																			// LCD
void UART_Entry(void *pvParameter);															    		// Receiver
void DHT22_Sensor_Entry(void *pvParameter);															// Sender2
void HCSR04_Sensor_Entry(void *pvParameter);														// Sender1
void Idle_Task_Entry(void *pvParameter);																// Idle
	

typedef enum
{
		Sender1_ID=0,
		Sender2_ID
} Task_ID_t;

typedef struct
{
		float dist;
		float temp;
		float humi;
}	Data_frame;

typedef struct
{
		Task_ID_t  task_id;
		Data_frame frame;
} xQueue_data_type_t;



void vTimerTaskCallback(TimerHandle_t xTimer)
{
		xSemaphoreGive(xLCDSemaphore);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
	
	
	//@NOTE THAT: htim2.Init.Period = 5999; (600ms) ===> Config 
	
	
	//I.   LCD Init
		lcd_set_timer(&htim1);
		
	
	//II.  HCSR04 Init Timer
		HCSR04_Set_Timer(&htim1);
		//@NOTE: TIM_IC_Start will for us delay (reset CNT) but no affect 
		HAL_TIM_IC_Start_IT(&htim1,TIM_CHANNEL_1);																		//For LCD INIT (before calling API lcd_init()
																																									//@because: lcd_init() using Timer


	//III. DHT22 Init
		DHT22_Set_Timer(&htim1);
		HAL_TIM_Base_Start(&htim1);
		data=DHT22_Read_Param();				

		

	//IV. TASK, SEMAPHORE AND QUEUE
		//1. TASK
		xTaskCreate(LCD_Entry,"LCD",128,NULL,4,LCD_Handle);
		xTaskCreate(UART_Entry,"DHT22",128,NULL,3,UART_Handle);												//Receiver										
		xTaskCreate(DHT22_Sensor_Entry,"DHT22",128,NULL,2,DHT22_Sensor);							//Take Semaphore- Sender1
		xTaskCreate(HCSR04_Sensor_Entry,"HCSR04",128,NULL,1,HCSR04_Sensor);						//Sender2
		xTaskCreate(Idle_Task_Entry,"DHT22",128,NULL,0,Idle_Handle);
		
		
		//2. SEMAPHORE
		xTaskSemaphore=  xSemaphoreCreateBinary();
		xLCDSemaphore =  xSemaphoreCreateBinary();

		if(xTaskSemaphore==NULL||xLCDSemaphore==NULL)
			print("Unable create Semaphore\n");
		else
			print("Create Semaphore Successfully\n");
		
		
		//3. QUEUE
		xTaskQueue		=  xQueueCreate(1,sizeof(xQueue_data_type_t));

		//4. SOFTWARE TIMER
		xTaskTimer 		=  xTimerCreate("SoftTimer",pdMS_TO_TICKS(1000),pdTRUE,(void*)0,vTimerTaskCallback);
		xTimerStart(xTaskTimer,0);
	
	//V. TIMER2 PERIODELAPSED
		HAL_TIM_Base_Start_IT(&htim2);
		
		
		
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of myQueue01 */
 
  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
	
  /* USER CODE BEGIN RTOS_THREADS */	
  vTaskStartScheduler();
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 72-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xffff-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7200-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 7999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

//LCD
void LCD_Entry(void *pvParameter)
{
		char str[20];
		lcd_init();
		while(1)
		{
				xSemaphoreTake(xLCDSemaphore,portMAX_DELAY);
				lcd_goto_XY(1,1);
				sprintf(str,"TEMP=%d HUMI=%d",temp,humi);
				lcd_send_string(str);
				lcd_goto_XY(2,2);
				sprintf(str,"DISTANCE=%d",dist);
				lcd_send_string(str);
				if(dist<10)
				{
					lcd_goto_XY(2,12);
					lcd_send_string("  ");
				}
		}
}


//Receiver: Priority=3
void UART_Entry(void *pvParameter)
{
		xQueue_data_type_t xStructsReceive;
		while(1)
		{
				xQueueReceive(xTaskQueue,&xStructsReceive,portMAX_DELAY);
				if(xStructsReceive.task_id==Sender1_ID)
				{
					temp=xStructsReceive.frame.temp/1;
					humi=xStructsReceive.frame.humi/1;
					print("TEMPERATURE= %f   HUMIDITY= %f\n",xStructsReceive.frame.temp,xStructsReceive.frame.humi);
				}
				else if(xStructsReceive.task_id==Sender2_ID)
				{
					dist=round(xStructsReceive.frame.dist);
					print("DISTANCE= %f\n",xStructsReceive.frame.dist);
				}
		}
}


//Sender1: Priority=2 				DHT22 has higher priority than HCSR04 ==> RUN FIRST and BLOCK by xSemaphoreTake()
void DHT22_Sensor_Entry(void *pvParameter)
{
		xQueue_data_type_t xStructsToSend;
			while(1)
			{
					xSemaphoreTake(xTaskSemaphore,portMAX_DELAY);
					data= DHT22_Read_Param();
					xStructsToSend.task_id=Sender1_ID;
					xStructsToSend.frame.dist=-1;
					xStructsToSend.frame.temp=data.temperature;
					xStructsToSend.frame.humi=data.humidity;
					xQueueSendToBack(xTaskQueue,&xStructsToSend,0);
			}
}

//Sender2: Priority=1
void HCSR04_Sensor_Entry(void *pvParameter)
{
		xQueue_data_type_t xStructsToSend;
//		TickType_t xLastExecutionTime;
//		xLastExecutionTime = xTaskGetTickCount();
		while(1)
		{		
//				vTaskDelayUntil( &xLastExecutionTime, pdMS_TO_TICKS(300) );
				float distance=HCSR04_Read();
				xStructsToSend.task_id=Sender2_ID;
				xStructsToSend.frame.dist=distance;
				xStructsToSend.frame.temp=-1;
				xStructsToSend.frame.humi=-1;
				xQueueSendToBack(xTaskQueue,&xStructsToSend,0);
				vTaskDelay(400);
				
		}
}

//Idle Task: Priority=0
void Idle_Task_Entry(void *pvParameter)
{
		while(1)
		{
				idle_count++;
		}
}				

/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM3 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
	if (htim->Instance == TIM2)
	{
		
		//@NOTE***********I. Release Semaphore for DHT22 Task
	
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(xTaskSemaphore, &xHigherPriorityTaskWoken);  // ISR SAFE VERSION
		portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
	}
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM3) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
