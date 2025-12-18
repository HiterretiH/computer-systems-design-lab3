/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
struct RingBuffer {
    char data[256];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
};

typedef struct RingBuffer RingBuffer;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BUF_SIZE 256

// Ноты (частота в Гц)
#define C4  262
#define D4  294
#define E4  330
#define F4  349
#define G4  392
#define A4  440
#define B4  494
#define C5  523
#define D5  587
#define E5  659
#define F5  698
#define G5  784
#define A5  880
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// Буферы для мелодий
uint32_t custom_track_data[256];
uint32_t custom_track_timing[256];
uint32_t custom_track_length = 0;

// Переменные для воспроизведения
uint32_t current_duration = 0;
bool is_track_playing = false;
uint32_t track_position = 0;
uint32_t *current_track_notes;
uint32_t *current_track_durations;
uint32_t track_notes_count;

// Предопределенные мелодии
uint32_t stairway_melody[] = {
    A4, C5, E5, A5, G5, E5, C5, G5,
    C5, E5, C4, C5, F5, D5, A4, F5
};

uint32_t stairway_delays[] = {
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 500, 500, 500, 500, 500, 500
};

uint32_t ssshhhiiittt_melody[] = {
    E5, D5, C5, D5, C5, 0, B4, 0, C5, 0, B4, C5, D5, 0, B4,
    E5, D5, C5, D5, C5, 0, B4, 0, C5, 0, B4, C5, D5, 0, A4
};

uint32_t ssshhhiiittt_delays[] = {
    400, 400, 400, 400, 400, 70, 400, 70, 400, 70, 400, 300, 400, 100, 400,
    400, 400, 400, 400, 400, 70, 400, 70, 400, 70, 400, 400, 400, 100, 400
};

uint32_t duvet_melody[] = {
    D5, C5, B4, C5, D5, 0, E5, F5, B4, 0,
    D5, C5, B4, C5, D5, 0, E5, 0, F5, E5, 0, F5, E5, 0, E5, D5, C5
};

uint32_t duvet_delays[] = {
    450, 450, 450, 450, 450, 50, 450, 450, 750, 50,
    450, 450, 450, 450, 450, 50, 450, 100, 450, 450, 50, 450, 450, 100, 750, 450, 750
};

uint32_t radiohead_melody[] = {
    F5, A4, D5, A4, F5, A4, D5, A4,
    F5, A4, D5, A4, G4, A4, D5, E5,
    F5, A4, D5, A4, F5, A4, D5, A4,
    F5, A4, D5, A4, G4, A4, D5, E5
};

uint32_t radiohead_delays[] = {
    450, 450, 450, 450, 450, 450, 450, 450,
    450, 450, 450, 450, 450, 450, 450, 450,
    450, 450, 450, 450, 450, 450, 450, 450,
    450, 450, 450, 450, 450, 450, 450, 450
};

// Системные переменные
static RingBuffer rx_buffer;
static uint8_t uart_rx_char;
static bool uart_tx_busy = false;

// Флаги для отладки
static bool system_initialized = false;
static bool uart_ready = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// Функции для кольцевого буфера
static void buf_init(RingBuffer *buf);
static bool buf_push(RingBuffer *buf, char c);
static bool buf_pop(RingBuffer *buf, char *c);

// Функции UART
void send_uart(const char *buffer, size_t length);
void send_uart_line(const char *buffer);
void activate_interrupts(void);

// Функции воспроизведения
void start_track_playback(uint32_t *notes_array, uint32_t *timings_array, uint32_t array_length);
uint32_t calculate_arr_for_freq(uint32_t freq_hz);

// Отладочные функции
void debug_led_blink(uint8_t times, uint32_t delay_ms);
void debug_send_status(void);
void test_sound(uint32_t freq_hz, uint32_t duration_ms);
void check_tim_config(void);

// Тестовые функции
void simple_sound_test(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Мигание светодиодом для отладки
void debug_led_blink(uint8_t times, uint32_t delay_ms) {
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
        HAL_Delay(delay_ms);
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
        if (i < times - 1) HAL_Delay(delay_ms);
    }
}

// Простой тест звука
void simple_sound_test(void) {
    send_uart_line("=== ПРОСТОЙ ТЕСТ ЗВУКА ===");

    // Тест 1: постоянный сигнал
    send_uart_line("Тест 1: Постоянный сигнал 440 Гц");
    uint32_t arr = calculate_arr_for_freq(440);
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, arr / 2);
    HAL_Delay(1000);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    HAL_Delay(200);

    // Тест 2: короткие сигналы
    send_uart_line("Тест 2: Короткие сигналы");
    for (int i = 0; i < 5; i++) {
        arr = calculate_arr_for_freq(440 + i * 100);
        __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, arr / 2);
        HAL_Delay(100);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        HAL_Delay(100);
    }

    // Тест 3: сирена
    send_uart_line("Тест 3: Сирена");
    for (int i = 0; i < 20; i++) {
        arr = calculate_arr_for_freq(300 + i * 20);
        __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, arr / 2);
        HAL_Delay(50);
    }
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

    send_uart_line("Тест завершен");
}

// Тест одной ноты
void test_sound(uint32_t freq_hz, uint32_t duration_ms) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Тест: %lu Гц, %lu мс", freq_hz, duration_ms);
    send_uart_line(msg);

    uint32_t arr = calculate_arr_for_freq(freq_hz);
    snprintf(msg, sizeof(msg), "ARR: %lu, Prescaler: %lu", arr, htim1.Init.Prescaler);
    send_uart_line(msg);

    if (arr > 0 && arr <= 65535) {
        __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, arr / 2);
        HAL_Delay(duration_ms);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    } else {
        send_uart_line("Ошибка: неверный ARR");
    }
    HAL_Delay(100);
}

// Проверка конфигурации таймеров
void check_tim_config(void) {
    uint32_t tim1_clock = SystemCoreClock / (htim1.Init.Prescaler + 1);
    uint32_t tim6_clock = SystemCoreClock / (htim6.Init.Prescaler + 1);

    char status[256];
    snprintf(status, sizeof(status),
        "\r\n=== КОНФИГУРАЦИЯ ТАЙМЕРОВ ===\r\n"
        "TIM1 (PWM/Звук):\r\n"
        "  Prescaler: %lu\r\n"
        "  Period (ARR): %lu\r\n"
        "  Тактовая частота: %lu Гц\r\n"
        "  Частота PWM: %lu Гц\r\n"
        "TIM6 (Таймер нот):\r\n"
        "  Prescaler: %lu\r\n"
        "  Period: %lu\r\n"
        "  Тактовая частота: %lu Гц\r\n"
        "  Частота прерываний: %lu Гц\r\n",
        htim1.Init.Prescaler,
        htim1.Init.Period,
        tim1_clock,
        tim1_clock / (htim1.Init.Period + 1),
        htim6.Init.Prescaler,
        htim6.Init.Period,
        tim6_clock,
        tim6_clock / (htim6.Init.Period + 1));
    send_uart_line(status);
}

// Отправка статуса системы
void debug_send_status(void) {
    char status_msg[256];
    snprintf(status_msg, sizeof(status_msg),
             "\r\n=== СТАТУС СИСТЕМЫ ===\r\n"
             "System Clock: %lu Hz\r\n"
             "UART BaudRate: %lu\r\n"
             "TIM1 Prescaler: %lu\r\n"
             "TIM1 ARR: %lu\r\n"
             "TIM6 Prescaler: %lu\r\n"
             "TIM6 ARR: %lu\r\n"
             "Трек играет: %s\r\n"
             "Позиция: %lu/%lu\r\n"
             "PD13 (Status LED): %s\r\n",
             SystemCoreClock,
             huart6.Init.BaudRate,
             htim1.Init.Prescaler,
             __HAL_TIM_GET_AUTORELOAD(&htim1),
             htim6.Init.Prescaler,
             __HAL_TIM_GET_AUTORELOAD(&htim6),
             is_track_playing ? "ДА" : "НЕТ",
             track_position,
             track_notes_count,
             HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_13) ? "ON" : "OFF");

    send_uart_line(status_msg);
}

// Инициализация кольцевого буфера
static void buf_init(RingBuffer *buf) {
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
    memset(buf->data, 0, sizeof(buf->data));
}

// Добавление символа в буфер
static bool buf_push(RingBuffer *buf, char c) {
    if (buf->count >= BUF_SIZE) {
        return false;
    }
    buf->data[buf->head] = c;
    buf->head = (buf->head + 1) % BUF_SIZE;
    buf->count++;
    return true;
}

// Извлечение символа из буфера
static bool buf_pop(RingBuffer *buf, char *c) {
    if (buf->count == 0) {
        return false;
    }
    *c = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) % BUF_SIZE;
    buf->count--;
    return true;
}

// Отправка данных через UART (с проверкой состояния)
void send_uart(const char *buffer, size_t length) {
    if (!uart_ready || length == 0) {
        return;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart6, (uint8_t*)buffer, length, 1000);
    if (status != HAL_OK) {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
        HAL_Delay(100);
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
    }
}

// Отправка строки с переводом строки
void send_uart_line(const char *buffer) {
    send_uart(buffer, strlen(buffer));
    send_uart("\r\n", 2);
}

// Активация прерываний UART
void activate_interrupts(void) {
    HAL_UART_Receive_IT(&huart6, (uint8_t*)&uart_rx_char, 1);
    uart_ready = true;
}

// Расчет значения ARR для заданной частоты
// Формула: ARR = (SystemCoreClock) / (freq_hz * (Prescaler + 1)) - 1
uint32_t calculate_arr_for_freq(uint32_t freq_hz) {
    if (freq_hz == 0 || freq_hz > 10000) {
        return 0;
    }

    // ТОЧНО как в рабочем коде:
    // SystemCoreClock = 180,000,000 Гц
    // Prescaler = 89 → тактовая частота = 180МГц / (89+1) = 2 МГц
    // ARR = 2,000,000 / freq_hz - 1
    uint32_t arr = 2000000 / freq_hz;
    if (arr > 0) arr--;

    if (arr < 2) arr = 2;
    if (arr > 65535) arr = 65535;

    return arr;
}

// Начало воспроизведения трека
void start_track_playback(uint32_t *notes_array, uint32_t *timings_array, uint32_t array_length) {
    track_position = 0;
    current_track_notes = notes_array;
    current_track_durations = timings_array;
    track_notes_count = array_length;
    is_track_playing = true;
    current_duration = 0;  // Начнем сразу с первой ноты
    send_uart_line("Воспроизведение начато");
}

// Обработчик прерывания UART (прием)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart_instance) {
    if (huart_instance->Instance == USART6) {
        // Индикация получения данных
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_15);

        // Сохраняем символ в буфер
        buf_push(&rx_buffer, (char)uart_rx_char);

        // Для отладки: эхо-ответ
        if (!uart_tx_busy) {
            uart_tx_busy = true;
            HAL_UART_Transmit_IT(&huart6, &uart_rx_char, 1);
        }

        // Возобновляем прием
        HAL_UART_Receive_IT(&huart6, (uint8_t*)&uart_rx_char, 1);
    }
}

// Обработчик прерывания UART (передача завершена)
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart_instance) {
    if (huart_instance->Instance == USART6) {
        uart_tx_busy = false;
    }
}

// Обработчик прерывания таймера TIM6
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) {
        if (!is_track_playing) return;

        if (current_duration > 0) {
            current_duration--;
            return;
        }

        if (track_position >= track_notes_count) {
            is_track_playing = false;
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            return;
        }

        uint32_t note_freq = current_track_notes[track_position];
        uint32_t note_duration = current_track_durations[track_position];

        current_duration = note_duration;

        if (note_freq == 0) {
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        } else {
            // ПРОСТОЙ РАСЧЕТ КАК В РАБОЧЕМ КОДЕ
            uint32_t arr_value = 2000000 / note_freq - 1;
            if (arr_value > 100 && arr_value < 65535) {
                __HAL_TIM_SET_AUTORELOAD(&htim1, arr_value);
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, arr_value / 2);
            } else {
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            }
        }

        track_position++;
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
  MX_USART6_UART_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();

  /* USER CODE BEGIN 2 */
  // 1. Простая инициализация
  buf_init(&rx_buffer);
  is_track_playing = false;
  custom_track_length = 0;

  // 2. Светодиоды
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);

  // 3. Запуск таймеров КАК В РАБОЧЕМ КОДЕ
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_Base_Start_IT(&htim6);

  // 4. Установите начальное значение PWM в 0
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

  // 5. Активация UART
  HAL_UART_Receive_IT(&huart6, (uint8_t*)&uart_rx_char, 1);
  uart_ready = true;

  // 6. Приветственное сообщение
  HAL_UART_Transmit(&huart6, (uint8_t*)"\r\nМузыкальная шкатулка\r\n", 25, 100);

  // 7. Тестовый звук - КОРОТКИЙ и ПРОСТОЙ
  uint32_t test_arr = (2000000 / 440) - 1;  // 440 Гц
  __HAL_TIM_SET_AUTORELOAD(&htim1, test_arr);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, test_arr / 2);
  HAL_Delay(200);
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

  // 8. Вывод меню
  send_uart_line("Команды: 1-4 мелодии, ? меню");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t loop_counter = 0;

  while (1)
  {
    loop_counter++;

    // Мигаем основным светодиодом (система работает)
    if (loop_counter % 100 == 0) {
      HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
    }

    // Обрабатываем команды из UART
    char received_char;
    if (buf_pop(&rx_buffer, &received_char)) {

      // Преобразуем в нижний регистр для удобства
      if (received_char >= 'A' && received_char <= 'Z') {
        received_char = received_char + ('a' - 'A');
      }

      // Отладка: что получили
      char debug_rx[32];
      snprintf(debug_rx, sizeof(debug_rx),
               "[CMD] Получено: '%c' (0x%02X)",
               (received_char >= 32 && received_char <= 126) ? received_char : '.',
               received_char);
      send_uart_line(debug_rx);

      // Обрабатываем команду
      switch (received_char) {
        case '1':
          send_uart_line("Воспроизведение: Stairway to Heaven");
          start_track_playback(stairway_melody, stairway_delays,
                             sizeof(stairway_melody) / sizeof(uint32_t));
          break;

        case '2':
          send_uart_line("Воспроизведение: Be Quiet and Drive");
          start_track_playback(ssshhhiiittt_melody, ssshhhiiittt_delays,
                             sizeof(ssshhhiiittt_melody) / sizeof(uint32_t));
          break;

        case '3':
          send_uart_line("Воспроизведение: Duvet");
          start_track_playback(duvet_melody, duvet_delays,
                             sizeof(duvet_melody) / sizeof(uint32_t));
          break;

        case '4':
          send_uart_line("Воспроизведение: No Surprises");
          start_track_playback(radiohead_melody, radiohead_delays,
                             sizeof(radiohead_melody) / sizeof(uint32_t));
          break;

        case '5':
          if (custom_track_length > 0) {
            send_uart_line("Воспроизведение пользовательской мелодии");
            start_track_playback(custom_track_data, custom_track_timing, custom_track_length);
          } else {
            send_uart_line("Пользовательская мелодия не создана!");
            send_uart_line("Используйте команду 'e' для создания мелодии");
          }
          break;

        case 'd':  // Команда отладки
          debug_send_status();
          break;

        case 'c':  // Проверка конфигурации
          check_tim_config();
          break;

        case 't':  // Простой тест звука
          simple_sound_test();
          break;

        case 'T':  // Расширенный тест звука
          send_uart_line("=== РАСШИРЕННЫЙ ТЕСТ ЗВУКА ===");
          test_sound(262, 300);   // C4
          test_sound(330, 300);   // E4
          test_sound(392, 300);   // G4
          test_sound(523, 300);   // C5
          test_sound(659, 300);   // E5
          test_sound(784, 300);   // G5
          test_sound(1047, 300);  // C6
          send_uart_line("Расширенный тест завершен");
          break;

        case 'e': {
          send_uart_line("=== РЕДАКТОР МЕЛОДИЙ ===");
          send_uart_line("Формат: ЧАСТОТА,ДЛИТЕЛЬНОСТЬ");
          send_uart_line("Пример: 440,500 (нота ЛЯ 440Гц на 500мс)");
          send_uart_line("0,500 (пауза 500мс)");
          send_uart_line("Команды:");
          send_uart_line("  s - сохранить мелодию");
          send_uart_line("  q - выйти без сохранения");
          send_uart_line("=========================");

          uint32_t temp_notes[256];
          uint32_t temp_timings[256];
          uint32_t note_count = 0;
          uint32_t current_note = 0;
          uint32_t current_delay = 0;
          bool reading_note = true;
          bool editor_active = true;

          while (editor_active) {
            char input_char;
            if (buf_pop(&rx_buffer, &input_char)) {

              if (input_char == 'q') {
                send_uart_line("Выход из редактора без сохранения");
                editor_active = false;
                break;
              } else if (input_char == 's') {
                if (note_count > 0) {
                  custom_track_length = note_count;
                  for (uint32_t i = 0; i < note_count; i++) {
                    custom_track_data[i] = temp_notes[i];
                    custom_track_timing[i] = temp_timings[i];
                  }
                  char msg[64];
                  snprintf(msg, sizeof(msg), "Мелодия сохранена! Записей: %lu", note_count);
                  send_uart_line(msg);
                } else {
                  send_uart_line("Мелодия пустая! Нечего сохранять.");
                }
                editor_active = false;
                break;
              } else if (input_char == ',') {
                reading_note = false;
              } else if (input_char == '\r' || input_char == '\n') {
                if (note_count < 256) {
                  temp_notes[note_count] = current_note;
                  temp_timings[note_count] = current_delay;
                  note_count++;

                  char note_msg[64];
                  if (current_note == 0) {
                    snprintf(note_msg, sizeof(note_msg),
                            "Добавлена пауза: %lu мс (всего нот: %lu)",
                            current_delay, note_count);
                  } else {
                    snprintf(note_msg, sizeof(note_msg),
                            "Добавлена нота: %lu Гц, %lu мс (всего нот: %lu)",
                            current_note, current_delay, note_count);
                  }
                  send_uart_line(note_msg);

                  current_note = 0;
                  current_delay = 0;
                  reading_note = true;
                }
              } else if (isdigit((unsigned char)input_char)) {
                if (reading_note) {
                  current_note = current_note * 10 + (input_char - '0');
                } else {
                  current_delay = current_delay * 10 + (input_char - '0');
                }
              }
            }
            HAL_Delay(10);
          }

          // Выводим меню после выхода из редактора
          const char menu_after_edit[] =
            "\r\n=== МЕНЮ ===\r\n"
            "1 - Led Zeppelin: Stairway to Heaven\r\n"
            "2 - Deftones: Be Quiet and Drive\r\n"
            "3 - Boa: Duvet\r\n"
            "4 - Radiohead: No Surprises\r\n"
            "5 - Пользовательская мелодия\r\n"
            "e - Редактировать пользовательскую мелодию\r\n"
            "x - Остановить воспроизведение\r\n"
            "t - Тест звука (простой)\r\n"
            "T - Расширенный тест звука\r\n"
            "c - Проверить конфигурацию таймеров\r\n"
            "d - Статус системы\r\n"
            "? - Показать меню\r\n"
            "========================\r\n";

          send_uart_line(menu_after_edit);
          break;
        }

        case 'x':
          send_uart_line("Воспроизведение остановлено");
          is_track_playing = false;
          __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
          HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
          break;

        case '?': {
          const char help_menu[] =
            "\r\n=== МЕНЮ ===\r\n"
            "1 - Led Zeppelin: Stairway to Heaven\r\n"
            "2 - Deftones: Be Quiet and Drive\r\n"
            "3 - Boa: Duvet\r\n"
            "4 - Radiohead: No Surprises\r\n"
            "5 - Пользовательская мелодия\r\n"
            "e - Редактировать пользовательскую мелодию\r\n"
            "x - Остановить воспроизведение\r\n"
            "t - Тест звука (простой)\r\n"
            "T - Расширенный тест звука\r\n"
            "c - Проверить конфигурацию таймеров\r\n"
            "d - Статус системы\r\n"
            "? - Показать меню\r\n"
            "========================\r\n";
          send_uart_line(help_menu);
          break;
        }

        case '\r':
        case '\n':
          // Игнорируем символы новой строки
          break;

        default:
          // Игнорируем непечатные символы
          if (received_char >= 32 && received_char <= 126) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg),
                    "Неизвестная команда: '%c' (0x%02X)",
                    received_char, received_char);
            send_uart_line(error_msg);
            send_uart_line("Используйте '?' для вывода меню");
          }
          break;
      }
    }

    // Небольшая задержка для стабильности
    HAL_Delay(1);

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 216;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

  // Индикация ошибки - быстрые вспышки светодиода
  while (1)
  {
    // 5 быстрых вспышек = ошибка
    for (int i = 0; i < 5; i++) {
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
      HAL_Delay(100);
      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
      HAL_Delay(100);
    }
    HAL_Delay(1000);
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
