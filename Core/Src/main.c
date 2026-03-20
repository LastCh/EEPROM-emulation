/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - EEPROM Page Transfer Test
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "eeprom.h"
#include <stdlib.h>
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

int EEPROM_GetActivePageIdx(void) {
    return CurrentActivePage;
}


/* USER CODE BEGIN 0 */

// ============ ПРОСТОЙ ТЕСТ: ЗАПОЛНЕНИЕ И ПЕРЕНОС СТРАНИЦЫ ============

// ========================== РАСШИРЕННЫЙ ТЕСТ EEPROM ==========================

void TestEEPROM(void) {
    printf("=== EEPROM TEST START ===\r\n");

    // Форматируем EEPROM и инициализируем
    EEPROM_Format();
    EEPROM_Init();

    // ---------- 1. Циклическая запись валидных переменных ----------
    printf("1. Cyclic write valid variables...\r\n");
    for (int j = 0; j < 50; j++) {  // 50 циклов
        for (int i = 0; i < VAR_NUM; i++) {  // по всем валидным ключам
            if (EEPROM_Write(ValidKeys[i], j * 10 + i) != EEPROM_OK) {
                printf("ERROR: Write failed at varID=0x%08lX\r\n", ValidKeys[i]);
            }
        }
    }

    // Проверка чтения
    uint32_t readVal;
    for (int i = 0; i < VAR_NUM; i++) {
        if (EEPROM_Read(ValidKeys[i], &readVal) != EEPROM_OK) {
            printf("ERROR: Read failed for varID=0x%08lX\r\n", ValidKeys[i]);
        }
        else {
            printf("Key 0x%08lX = %lu\r\n", ValidKeys[i], readVal);
        }
    }


    // ---------- 2. Симуляция сбоя питания во время переноса ----------
    printf("2. Simulate power loss during page transfer...\r\n");
    EEPROM_Format();
    EEPROM_Init();
    // Заполняем страницу почти полностью
    for (int i = 0; i < 127; i++) {
        EEPROM_Write(VAR_COUNTER, i);
    }
    // Инициируем перенос страницы (тестовая запись)
    printf("Simulating interruption...\r\n");
    // Здесь можно имитировать сброс MCU или просто пропустить запись последнего элемента
    EEPROM_Write(VAR_COUNTER, 999); // вызовет Page Transfer
    // После "восстановления питания"
    EEPROM_Init();
    EEPROM_Read(VAR_COUNTER, &readVal);
    printf("Value after simulated power loss: %lu\r\n", readVal);

    // ---------- 3. Проверка EraseCount ----------
    printf("3. Checking EraseCount...\r\n");
    for (int i = 0; i < N_PAGES; i++) {
        uint32_t ec = EEPROM_GetEraseCount(i);
        printf("Page %d EraseCount = %lu\r\n", i, ec);
    }

    // ---------- 4. Искусственное выравнивание износа ----------
    printf("4. Static Wear Leveling test...\r\n");
    // Имитируем, что первая страница сильно изношена
    for (int j = 0; j < WEAR_THRESHOLD + 5; j++) {
        EEPROM_ErasePage(0); // увеличиваем EraseCount
    }
    // Записываем новую переменную для триггера статического WL
    EEPROM_Write(VAR_ENERGY, 12345);
    for (int i = 0; i < N_PAGES; i++) {
        uint32_t ec = EEPROM_GetEraseCount(i);
        printf("Page %d EraseCount after WL = %lu\r\n", i, ec);
    }

    printf("=== EEPROM TEST END ===\r\n");
}


/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  TestEEPROM();
  while (1)
  {

  }
}




void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void)
{
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
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
