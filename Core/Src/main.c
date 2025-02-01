/* USER CODE BEGIN Header */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <frame.h>
#include <stdbool.h>
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "iwdg.h"
#include "usart.h"
#include "wwdg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* USER CODE BEGIN PV */
#define USART_TXBUF_LEN 1512             // rozmiar bufora nadawczego
#define USART_RXBUF_LEN 128            // rozmiar bufora odbiorczego

uint8_t USART_TxBuf[USART_TXBUF_LEN];    // bufor nadawczy
uint8_t USART_RxBuf[USART_RXBUF_LEN];    // bufor odbiorczy

volatile uint16_t led_blink_interval = 500;

__IO int USART_TX_Empty = 0;			 // __IO - zmienna typu volatile inputoutput
__IO int USART_TX_Busy = 0;
__IO int USART_RX_Empty = 0;
__IO int USART_RX_Busy = 0;
volatile uint8_t ewi_flag __attribute__((section(".noinit")));
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
uint8_t USART_kbhit();        // sprawdź czy bufor nie jest pusty
int16_t USART_getchar();      // pobierz dane z bufora
uint8_t USART_getline(char *buf); // pobierz linie z bufora
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void configureSysTick(void) {
    // Konfiguracja SysTick dla przerwania co 1 ms
	SysTick_Config(SystemCoreClock / 1000);
}
// sprawdź czy bufor nie jest pusty
uint8_t USART_kbhit() {
    return (USART_RX_Empty != USART_RX_Busy);
}

// pobierz pojedyńczy znak z bufora odbiorczego jeśli dostępne są nowe dane
int16_t USART_getchar() {
    if (USART_RX_Empty != USART_RX_Busy) {
        int16_t tmp = USART_RxBuf[USART_RX_Busy];	// pobranie znaku z bufora
        USART_RX_Busy++;
        if (USART_RX_Busy >= USART_RXBUF_LEN) USART_RX_Busy = 0;	// "cykl" bufora
        return tmp;
    } else {
        return -1;
    }
}

// pobierz całą linie tekstu do napotakania znaku nowej linii z bufora odbiorczego
uint8_t USART_getline(char *buf) {
    static uint8_t bf[128];
    static uint8_t index = 0;
    int i;
    uint8_t ret;
    while (USART_kbhit()) {	// sprawdza czy są nowe dane w buforze
        bf[index] = USART_getchar();	// pobiera znak z bufora
        if (bf[index] == 10 || bf[index] == 13) {
            bf[index] = 0;
            for (i = 0; i <= index; i++) {
                buf[i] = bf[i];
            }
            ret = index;
            index = 0;
            return ret;  // zwraca długość linii
        } else {
            index++;
            if (index >= 128) index = 0;
        }
    }
    return 0;
}
// wersja funkcji fsend, która przygotowana jest do wysyłania ramki
void USART_fsend_frame(uint8_t *data, uint8_t len) {
    __IO int idx = USART_TX_Empty;
    uint8_t i;

    // Umieść dane w buforze USART_TxBuf
    for (i = 0; i < len; i++) {
        USART_TxBuf[idx] = data[i];
        idx++;
        if (idx >= USART_TXBUF_LEN) idx = 0; // Zapętlenie bufora
    }
    __disable_irq();
    if ((USART_TX_Empty == USART_TX_Busy) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == SET)) {
        // Jeśli bufor był pusty i UART gotowy do nadawania, rozpocznij transmisję
        USART_TX_Empty = idx;
        uint8_t tmp = USART_TxBuf[USART_TX_Busy];
        USART_TX_Busy++;
        if (USART_TX_Busy >= USART_TXBUF_LEN) USART_TX_Busy = 0;
        HAL_UART_Transmit_IT(&huart2, &tmp, 1);
    } else {
        // Zaktualizuj wskaźnik końca danych w buforze
        USART_TX_Empty = idx;
    }
    __enable_irq();
}
// wysyła sformatowane dane za pomocą UART z użyciem bufora nadawczego
void USART_fsend(char* format,...){
	char tmp_rs[128];
	int i;
	__IO int idx;
	va_list arglist;
	va_start(arglist,format);
	vsprintf(tmp_rs,format,arglist);
	va_end(arglist);
	idx=USART_TX_Empty;
	for(i=0;i<strlen(tmp_rs);i++){
		USART_TxBuf[idx]=tmp_rs[i];
		idx++;
		if(idx >= USART_TXBUF_LEN)idx=0;
	}
	__disable_irq();
	if((USART_TX_Empty==USART_TX_Busy)&&(__HAL_UART_GET_FLAG(&huart2,UART_FLAG_TXE)==SET)){//sprawdzic dodatkowo zajetosc bufora nadajnika
		USART_TX_Empty=idx;
		uint8_t tmp=USART_TxBuf[USART_TX_Busy];
		USART_TX_Busy++;
		if(USART_TX_Busy >= USART_TXBUF_LEN)USART_TX_Busy=0;
		HAL_UART_Transmit_IT(&huart2, &tmp, 1);
	}else{
		USART_TX_Empty=idx;
	}
	__enable_irq();
}//fsend

// funkcja typu callback wywoływana po zakończeniu transmisji
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        if (USART_TX_Empty != USART_TX_Busy) {
            uint8_t tmp = USART_TxBuf[USART_TX_Busy];
            USART_TX_Busy++;
            if (USART_TX_Busy >= USART_TXBUF_LEN) USART_TX_Busy = 0;
            HAL_UART_Transmit_IT(&huart2, &tmp, 1);
        }
    }
}

// funkcja typu callback wywoływana po zakończeniu odbioru
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        USART_RX_Empty++;
        if (USART_RX_Empty >= USART_RXBUF_LEN) USART_RX_Empty = 0;
        HAL_UART_Receive_IT(&huart2, &USART_RxBuf[USART_RX_Empty], 1);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	configureSysTick();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  __HAL_RCC_GPIOA_CLK_ENABLE(); // Włączenie zegara GPIOA
  // Konfiguracja GPIO dla diody LD2
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_5;        // Pin PA5 (LD2)
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Wyjście push-pull
  GPIO_InitStruct.Pull = GPIO_NOPULL;      // Bez podciągania
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; // Niska szybkość
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // flagi sprawdzające czy doszło do resetu poprzez wwdg/iwdg
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
      led_blink_interval = 100; // Zmiana interwału mrugania na 100 ms
  }
  else if(__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
	  led_blink_interval = 1000;
  }
  __HAL_RCC_CLEAR_RESET_FLAGS(); // Czyszczenie flag resetu
  HAL_UART_Receive_IT(&huart2,&USART_RxBuf[0],1);
  char bx[66];
  bool escape_detected = false;
  uint8_t bx_index = 0;
  bool in_frame = false;
  char received_char;
  Frame ramka;
  if(ewi_flag == 1) {
      memcpy(ramka.command, "BACK", COMMAND_LEN);
      memcpy(ramka.data, "przerwanie ewi zadzialalo", 25);
      sendFrame(ramka.command, ramka.data, 25);
      ewi_flag = 0;
  }
  void reset_frame_state() {
	in_frame = false;
	escape_detected = false;
	bx_index = 0;
  }
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
  uint32_t last_toggle_time = HAL_GetTick(); // Pobranie aktualnego czasu
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
while (1) {
    if (HAL_GetTick() - last_toggle_time >= led_blink_interval) {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); // Zmiana stanu diody LD2
        last_toggle_time = HAL_GetTick();     // Aktualizacja czasu ostatniego przełączenia
    }

    if (USART_kbhit()) {                // Sprawdzamy, czy jest dostępny nowy znak
        received_char = USART_getchar();   // Pobieramy znak z bufora odbiorczego

        if (received_char == FRAME_START) {
            if (in_frame) {	// wykryto nadmiarowy znak, znowu sprawdzimy ramke
            	reset_frame_state();
            	in_frame = true;
            }// Rozpoczęcie nowej ramki
                in_frame = true;
                bx_index = 0;           // Resetujemy indeks bufora
                escape_detected = false; // Resetujemy flagę escape
        } else if (received_char == FRAME_STOP && escape_detected == false) {    // Koniec ramki
            if (in_frame) {
                // Przetwarzanie odebranej ramki (np. wywołanie funkcji lub ustawienie flagi)
            	// sprawdzamy adres odbiorcy, jesli nim jestesmy, procesujemy ramke
                if (checkFrame(bx,&ramka, bx_index) && bx[1] == 'S') {
                	handleCommand(&ramka, bx_index);
                } else {
                	reset_frame_state(); // inny adres
                }
                reset_frame_state(); // resetujemy po zakończonym odbiorze prawidłowej ramki
            } else {
                // Jeśli ramka się kończy, ale nie została rozpoczęta
                reset_frame_state();
            }
        } else if (in_frame) {
        	if (bx_index < 66) { // po przypisaniu sprawdzenie
            // Jesteśmy w ramce i przetwarzamy znaki
            if (escape_detected) {
                // Jeśli wykryto escape char, sprawdzamy następny znak
                if (received_char == '^') {
                    bx[bx_index++] = '~'; // '~' było zakodowane jako '}^'
                } else if (received_char == ']') {
                    bx[bx_index++] = '}'; // '}' było zakodowane jako '}]'
                } else if (received_char == '&') {
                    bx[bx_index++] = '`'; // '`' było zakodowane jako '}&'
                } else {
                    // Nieprawidłowy znak po '}', resetujemy
                    reset_frame_state();
                }
                escape_detected = false; // Resetujemy flagę escape
            } else if (received_char == ESCAPE_CHAR) {
                escape_detected = true; // Wykryto znak escape, czekamy na następny
            } else {
                bx[bx_index++] = received_char;
                }
            } else {
            	reset_frame_state(); // reset z powodu zbyt duzej ramki
            }
        }
    }
}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
