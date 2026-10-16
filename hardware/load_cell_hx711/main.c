/* Code for reading from HX711 and load cell. 
Reads raw data, update to conversion*/

#include "main.h"
#include "hx711.h"
#include "hx711Config.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private variables */
UART_HandleTypeDef huart2; // For printf debugging via UART
hx711_t loadcell;

/* Function prototypes */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* UART printf redirection */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

int main(void)
{
  /* MCU Configuration */
  HAL_Init();
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  printf("HX711 Load Cell Test\r\n");

  /* Initialize HX711 - Connect CLK to PA1, DATA to PA0 */
  hx711_init(&loadcell, GPIOA, GPIO_PIN_9, GPIOA, GPIO_PIN_8);

  printf("HX711 initialized. Starting measurements...\r\n");

  /* Main loop */
  while (1)
  {
    /* Read raw value */
    int32_t raw_value = hx711_value(&loadcell);
    printf("Raw Load Cell Value: %ld\r\n", raw_value);

    /* Read averaged value (10 samples) */
    int32_t avg_value = hx711_value_ave(&loadcell, 10);
    printf("Averaged Value (10 samples): %ld\r\n", avg_value);

    HAL_Delay(1000); // 1 second between readings
  }
}

/* System Clock Configuration */
static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /* Configure HSI as system clock source (8 MHz) */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16; // 8MHz/2*16 = 64MHz
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  /* Configure system clock */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // 64 MHz
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1; // 64 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; // 32 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1; // 64 MHz
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

/* USART2 Initialization function */
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
  HAL_UART_Init(&huart2);
}

/* GPIO Initialization Function */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* Configure GPIO pins for HX711 (PA0=DATA, PA1=CLK) */
  /* These will be configured by the hx711_init function */
}

/* USART2 MSP Initialization */
/*
void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance == USART2)
  {
    // USART2 clock enable
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();


    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}*/
