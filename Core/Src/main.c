/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct __attribute__((packed)) {
  uint8_t  header;          /* 0xAA */
  uint8_t  run;             /* 00 暂停 / 01 运行 */
  uint8_t  dir;             /* 00 正向 / 01 反向 */
  uint16_t step_ms;         /* 低字节在前，正对 STEP_L / STEP_H */
  uint8_t  checksum;        /* 前 5 字节之和的低 8 位 */
} DemoPacket;


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
static void ledShow(uint8_t idx);
static void ledStep(void);
static void UartFeedback(void);
static void UiRefresh(void);
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define led_PORT   GPIOB
static const uint16_t led_pins[3] = { GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5 };

static uint8_t  led  ;      /* 当前点亮第几颗：0~2 */
volatile uint8_t runstate   ;      /* 1 运行 / 0 暂停 */
volatile uint16_t step ;
static uint8_t  dir  ;      /* 0 正向 / 1 反向 */
  /* 切换间隔 ms，今晚先写死，明天改串口 */
static uint32_t last_tick = 0;

volatile uint8_t ev_dir =0 , ev_run = 0 ;   /* 按键事件，中断里只置位 */

DemoPacket rx_packet;
volatile uint8_t packet_ready = 0;

static char tx_feedback [64];

volatile uint8_t tx_state = 0;/* 0 串口空闲/ 1 串口正忙 */


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  runstate = 1;   step = 300;   dir = 0;   led = 0;
  last_tick = HAL_GetTick();
  ledShow(led);
  UiRefresh();      /* 先画一帧，屏就不会是黑的 */

  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_packet, sizeof(rx_packet));

  OLED_Init( );


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (packet_ready) {
      DemoPacket packet = rx_packet;
      packet_ready = 0;

      const uint8_t *bytes = (const uint8_t *)&packet;
      uint8_t expected = (uint8_t)(bytes[0] + bytes[1]        //计算最后面校验位
                               + bytes[2] + bytes[3] +bytes[4]);

      if (packet.header == 0xAA && packet.checksum == (expected %256)
          && packet.run <= 1
          && packet.dir <= 1
          && packet.step_ms >= 100
          && packet.step_ms <= 2000)
      {
        runstate = packet.run;
        step = packet.step_ms;
        dir = packet.dir;
        UartFeedback();
        UiRefresh();
      }
      else {}/* 帧头/校验和不对，整包丢弃*/

      HAL_UART_Receive_IT(&huart1,
          (uint8_t *)&rx_packet, sizeof(rx_packet));
    }

    /* 1) 先处理按键事件：必须放在切灯判断之外，暂停时也要响应 */
    uint32_t now = HAL_GetTick();
    static uint32_t last_tick_dir = 0, last_tick_run = 0;
    if (ev_dir && now - last_tick_dir >= 20)
    {
      last_tick_dir = now;
      ev_dir = 0;
      dir = !dir;
      ledShow(led);
      UiRefresh();
      UartFeedback();
    }
    if (ev_run && now - last_tick_run >= 20)
    {
      last_tick_run = now;
      ev_run = 0;
      runstate = !runstate;
      UiRefresh();
      UartFeedback();
    }

    /* 2) 灯只在“运行”时推进；GetTick 差值，不用 HAL_Delay */
    if (runstate && (HAL_GetTick() - last_tick >= step)) {
      last_tick = HAL_GetTick();
      ledStep();
      UiRefresh();
    }
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

/* USER CODE BEGIN 4 */
static void ledShow(uint8_t idx)
{
  for (uint8_t i = 0; i < 3; i++)
    HAL_GPIO_WritePin(led_PORT, led_pins[i], GPIO_PIN_RESET);    /* 灭 */
  HAL_GPIO_WritePin(led_PORT, led_pins[idx], GPIO_PIN_SET);   /* 亮 */
}

/* 步进：正向 +1、反向 -1，两端折返（3 的模运算天然折返） */
static void ledStep(void)
{
  led = dir ? (uint8_t)((led + 2) % 3) : (uint8_t)((led + 1) % 3);
  ledShow(led);
}
static void UiRefresh(void)
{
  char l1[20], l2[20], l3[20], l4[20];
  snprintf(l1, sizeof l1, "STATE:%4s", runstate ? "RUN" : "STOP");
  snprintf(l2, sizeof l2, "DIR: %s",   dir ? "REV" : "FWD");
  snprintf(l3, sizeof l3, "STEP: %u",  (unsigned)step);
  snprintf(l4, sizeof l4, "LED: %u",   (unsigned)(led + 1));
  OLED_ShowString(1, 1, l1);  OLED_ShowString(2, 1, l2);
  OLED_ShowString(3, 1, l3);  OLED_ShowString(4, 1, l4);
}
static void UartFeedback(void)
{
  if (tx_state) return;                 /* 上一包没发完，丢掉这包，别覆盖缓冲 */
  snprintf(tx_feedback, sizeof(tx_feedback), "STATE=%4s,DIR=%s,STEP=%u,LED=%u\r\n",
           runstate ? "RUN" : "STOP", dir ? "REV" : "FWD",
           (unsigned)step, (unsigned)(led + 1));
  tx_state = 1;
  if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)tx_feedback,
                          (uint16_t)strlen(tx_feedback)) != HAL_OK)
             {tx_state = 0;}        /* 发送失败，串口置为空闲 */
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_11) ev_dir = 1;
  else if (GPIO_Pin == GPIO_PIN_12) ev_run = 1;
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    packet_ready = 1;      /* 记这一包收满了 */
  }
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) tx_state = 0;   /* 发送完成才放开忙标志 */
}
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
#ifdef USE_FULL_ASSERT
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
