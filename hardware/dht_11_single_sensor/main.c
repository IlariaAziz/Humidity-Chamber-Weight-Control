/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "dht11.h"
#include <stdio.h>

/* Define pins if not defined in main.h */
#ifndef B1_Pin
#define B1_Pin GPIO_PIN_13
#endif
#ifndef B1_GPIO_Port
#define B1_GPIO_Port GPIOC
#endif

/* Define number of sensors */
#define NUM_SENSORS 4

/* Private variables */
UART_HandleTypeDef huart2;
DHT11_Dev dht11_sensors[NUM_SENSORS];  // Array to hold multiple DHT11 sensors
float temperature[NUM_SENSORS], humidity[NUM_SENSORS];

/* GPIO pin definitions for sensors */
GPIO_TypeDef* sensor_ports[NUM_SENSORS] = {GPIOA, GPIOA, GPIOA, GPIOA};
uint16_t sensor_pins[NUM_SENSORS] = {GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9};
char* sensor_names[NUM_SENSORS] = {"PA6", "PA7", "PA8", "PA9"};

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* Printf function redirection to UART */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* MCU Configuration--------------------------------------------------------*/
    HAL_Init();
    SystemClock_Config();

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* USER CODE BEGIN 2 */
    // Initialize all DHT11 sensors
    for (int i = 0; i < NUM_SENSORS; i++) {
        DHT11_Init(&dht11_sensors[i], TIM2, sensor_ports[i], sensor_pins[i]);
    }

    printf("DHT11 Multi-Sensor Temperature and Humidity Demo\r\n");
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        // Read from each sensor
        for (int i = 0; i < NUM_SENSORS; i++) {
            int result = DHT11_Read(&dht11_sensors[i]);

            if(result == DHT11_SUCCESS) {
                temperature[i] = dht11_sensors[i].temparature + (dht11_sensors[i].temp_decimal/10.0);
                humidity[i] = dht11_sensors[i].humidity + (dht11_sensors[i].humidity_decimal/10.0);
                
                // Print results in the requested format
                printf("%s Temperature %d.%d Humidity %d.%d\r\n", 
                    sensor_names[i],
                    dht11_sensors[i].temparature, 
                    dht11_sensors[i].temp_decimal,
                    dht11_sensors[i].humidity, 
                    dht11_sensors[i].humidity_decimal);
            } else if(result == DHT11_ERROR_CHECKSUM) {
                printf("%s Error: DHT11 checksum error\r\n", sensor_names[i]);
            } else if(result == DHT11_ERROR_TIMEOUT) {
                printf("%s Error: DHT11 timeout error\r\n", sensor_names[i]);
            } else {
                printf("%s Error reading from DHT11\r\n", sensor_names[i]);
            }
            
            // Small delay between sensor readings to avoid interference
            HAL_Delay(100);
        }

        printf("------------------------\r\n");
        HAL_Delay(2000);  // Read every 2 seconds
    }
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
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin : B1_Pin */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* Initialize all DHT11 sensor pins (PA6, PA7, PA8, PA9) */
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;  /* Initially set as output */
    GPIO_InitStruct.Pull = GPIO_PULLUP;          /* Use pull-up for DHT11 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        GPIO_InitStruct.Pin = sensor_pins[i];
        HAL_GPIO_Init(sensor_ports[i], &GPIO_InitStruct);
        HAL_GPIO_WritePin(sensor_ports[i], sensor_pins[i], GPIO_PIN_SET);  /* Set initial level high */
    }

    /* EXTI interrupt init*/
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

/* This function is used by the DHT11 driver */
void _Error_Handler(char *file, int line)
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