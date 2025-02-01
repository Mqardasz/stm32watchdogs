/*
 * frame.h
 *
 *  Created on: Nov 8, 2024
 *      Author: test
 */

#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h" // Wymagany dla funkcji watchdog i HAL
#include "stm32f4xx_it.h"

// Definicje dla protokołu ramki
#define FRAME_START '~'
#define FRAME_STOP  '`'
#define ESCAPE_CHAR '}'
#define MIN_FRAME_LEN 10
#define MIN_DECODED_RAW_FRAME_LEN  8
#define MAX_DECODED_RAW_FRAME_LEN 64
#define MAX_FRAME_LEN 128
#define MAX_DATA_LEN 54
#define RAW_FRAME_LEN 10 //rozmiar ramki bez pola dane
#define STM32_ADDR 'S' // adres stm32
#define PC_ADDR 'C'	// adres komputera
#define COMMAND_LEN 4

// Struktura ramki komunikacyjnej
typedef struct {
    char sender_address;     // Adres odbiorcy
    char receiver_address;       // Adres nadawcy
    char command[COMMAND_LEN]; // Komenda (4 bajty)
    char data[MAX_DATA_LEN];   // Dane (0-54 bajtów)
} Frame;

// Tabela predefiniowanych wartości CRC16
extern uint16_t crc16_table[256];
extern volatile uint16_t iwdg_refresh_interval;
extern volatile uint16_t wwdg_refresh_interval;
extern volatile uint8_t reload_wwdg;
extern volatile uint8_t window;
// flagi sprawdzające czy watchdog jest zainicjalizowany
extern volatile bool wwdg_initialized;
extern volatile bool iwdg_initialized;
// wartosci boolowskie do obsługi IWDG/WWDG

// flaga EWI do resetu
extern volatile uint8_t ewi_flag;

// Funkcje CRC i stuffing/destuffing
void calculate_crc16(uint8_t *data, size_t length, char crc_out[2]);
void stuff_and_send(uint8_t *frame, uint8_t frame_len);

// Funkcje obsługi protokołu
void prepareFrame(char receiver, char sender, char *command, char *data, char data_len, Frame *frame);
bool checkFrame(char *bx, Frame *frame, uint8_t len);

//	 =========================== rozpoznawanie komendy =========================
typedef void (*CommandFunction)(Frame *frame, uint8_t len);
typedef struct{
	char command[COMMAND_LEN];
	CommandFunction function;
} CommandEntry;

void handleCommand(Frame *frame, uint8_t len);

// Watchdog Configuration (WDG)

void executeILSI();
void executeIPRE(Frame *frame, uint8_t len); // ustaw preskaler
void executeIREL(Frame *frame, uint8_t len); // ustaw reload
void executeIREF(Frame *frame, uint8_t len); // ustaw interwał odświeżania
void executeIINI(Frame *frame, uint8_t len); // inicjalizuj IWDG
void executeIGET(Frame *frame, uint8_t len); // pobierz aktualną konfigurację IWDG
void executeICNT(Frame *frame, uint8_t len);

void executeWAPB(Frame *frame, uint8_t len); // ustaw zegar dla wwdg
void executeWPRE(Frame *frame, uint8_t len); // ustaw preskaler
void executeWREL(Frame *frame, uint8_t len); // ustaw reload
void executeWWIN(Frame *frame, uint8_t len); // ustaw wartość okna
void executeWEWI(Frame *frame, uint8_t len); // ustaw przerwanie (EWI)
void executeWINI(Frame *frame, uint8_t len); // inicjalizuj WWDG
void executeWREF(Frame *frame, uint8_t len); // ustaw interwał odświeżania
void executeWCNT(Frame *frame, uint8_t len);
void executeWGET(Frame *frame, uint8_t len);


#endif // FRAME_H

