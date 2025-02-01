/*
 * crc16.c
 *
 *  Created on: Nov 6, 2024
 *      Author: test
 */
#include <stdint.h>
#include <stddef.h>
#include <frame.h>
#include <string.h>
#include <main.h>
#include <stdio.h>
#include <stdbool.h>
#include <stm32f4xx_it.h>

volatile uint16_t iwdg_refresh_interval = 0;
volatile uint16_t wwdg_refresh_interval = 0;
volatile uint8_t prescaler_wwdg = 0;
volatile uint8_t reload_wwdg = 0;
volatile uint8_t window = 0;
// flagi sprawdzające dla uruchomienia IWDG
volatile bool isLSI = false;
volatile bool isPrescaler = false;
volatile bool isReloadI = false;
volatile bool iwdg_initialized = false;
// flagi sprawdzające dla uruchomienia WWDG
volatile bool isAPB = false;
volatile bool isWprescaler = false;
volatile bool isReloadW = false;
volatile bool isWindow = false;
volatile bool wwdg_initialized = false;
// zmienne pomocnicze dla IWDG
volatile uint16_t prescaler = 0;
volatile uint16_t reload = 0;

// tablica z komendami oraz wywołaniami funkcji
static const CommandEntry commandTable[16] = {
    // KOMENDY DO IWDG
    {"ILSI", executeILSI}, // ustaw zegar LSI
    {"IPRE", executeIPRE}, // ustaw preskaler
    {"IREL", executeIREL}, // ustaw reload
    {"IREF", executeIREF}, // ustaw interwał odświeżania
    {"IINI", executeIINI}, // inicjalizuj IWDG
    {"IGET", executeIGET}, // pobierz aktualną konfigurację IWDG
	{"ICNT", executeICNT}, // pobierz liczbę odświeżeń IWDG

    // KOMENDY DO WWDG
	{"WAPB", executeWAPB}, // ustaw bit w bloku RCC na 1
    {"WPRE", executeWPRE}, // ustaw preskaler
    {"WREL", executeWREL}, // ustaw reload
    {"WWIN", executeWWIN}, // ustaw wartość okna
    {"WEWI", executeWEWI}, // ustaw przerwanie (EWI)
    {"WINI", executeWINI}, // inicjalizuj WWDG
    {"WREF", executeWREF}, // ustaw interwał odświeżania
	{"WGET", executeWGET}, // pobierz konfiguracje
	{"WCNT", executeWCNT}, // pobierz liczbę odświeżeń
};

// formatowanie ramki do wysyłki jako ciąg znaków
uint8_t byteStuffing(uint8_t *input, size_t input_len, uint8_t *output) {
    size_t j = 1;
    output[0] = '~';
    for (size_t i = 1; i < input_len; i++) {
        if (input[i] == ESCAPE_CHAR) {
            output[j++] = ESCAPE_CHAR;
            output[j++] = ']';
        } else if (input[i] == '~') {
            output[j++] = ESCAPE_CHAR;
            output[j++] = '^';
        } else if (input[i] == '`') {
            output[j++] = ESCAPE_CHAR;
            output[j++] = '&';
        } else {
            output[j++] = input[i];
        }
    }
    output[j++] = '`';
    return j;
}

// stablicowane wartości CRC16
// znajduje się w niej preobliczone wartości
// obliczone dla każdego możliwego bajtu od 0x00 do 0xFF
uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

void calculate_crc16(uint8_t *data, size_t length, char crc_out[2]) {
    uint16_t crc = 0xFFFF; // wartość inicjująca

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        uint8_t table_index = (crc >> 8) ^ byte; // oblicz indeks tablicy
        crc = (crc << 8) ^ crc16_table[table_index]; // zaktualizuj crc uzywajac wartosci stablicowanej
    }
    crc_out[0] = (char)((crc >> 8) & 0xFF); // bajt po lewej
    crc_out[1] = (char)(crc & 0xFF);        // bajt po prawej
}

// funkcja dekodująca ramkę i sprawdzająca jej poprawność
bool checkFrame(char *bx, Frame *frame, uint8_t len) {
	char ownCrc[2];
	char incCrc[2];
    if(len >= MIN_DECODED_RAW_FRAME_LEN && len <= MAX_DECODED_RAW_FRAME_LEN) {
    	uint8_t k = 0;
    	frame->sender_address = bx[k++];						// kopiujemy nadawce do struktury
    	frame->receiver_address = bx[k++];						// kopiujemy odbiorce do struktury
    	memcpy(frame->command, &bx[k],COMMAND_LEN);				// kopiujemy komende do struktury
    	k += COMMAND_LEN;
    	uint8_t data_len = len - MIN_DECODED_RAW_FRAME_LEN;
    	if(data_len != 0) {											// wyliczamy długość danych
    	    memcpy(frame->data, &bx[k],data_len);
    	}
    	k += data_len;
    	memcpy(incCrc, &bx[k], 2);								// kopiujemy crc do tablicy
    	calculate_crc16((uint8_t *)frame, k, ownCrc);
    	for (int l = 0; l < 2; l++) {
    		if(ownCrc[l] != incCrc[l]) {
    			return false;
    		}
    	}
    	return true; // crc zostalo pomyslnie porownanie
    }
    return false; // ramka niepoprawna
}

void sendFrame(char *command, char *data, uint8_t data_len) {
	char ownCrc[2] = {0};
	char bx_send[66] = {0};
	bx_send[0] = '~';
	bx_send[1] = 'S';
	bx_send[2] = 'C';

	char stuffed_frame[126] = {0};
	uint8_t i , j, k;
	for(i = 0; i < COMMAND_LEN; i++) {
		bx_send[3 + i] = command[i];
	}
	for(j = 0; j < data_len; j++) {
		bx_send[3 + i + j] = data[j];
	}
	uint8_t len = 3 + i + j;
	calculate_crc16((uint8_t *)bx_send, len, ownCrc);
	for(k = 0; k < 2; k++) {
		bx_send[len + k] = ownCrc[k];
	}
	len = len + k;
	 uint8_t stuffed_len = byteStuffing(bx_send, len, stuffed_frame);
	 if (stuffed_len >= 10 && stuffed_len <= 128) {
	 stuffed_frame[stuffed_len] = '`';
	 USART_fsend_frame(stuffed_frame, stuffed_len);
	 }
}

void handleCommand(Frame *frame, uint8_t len) {
    uint8_t data_len = len - MIN_DECODED_RAW_FRAME_LEN;
    for (int i = 0; i < 16; i++) {
        if (strncmp(frame->command, commandTable[i].command, COMMAND_LEN) == 0) {
            commandTable[i].function(frame, data_len); // Wywołaj przypisaną funkcję
            return;
        }
    }
}

// KOMENDY DO IWDG

void executeILSI(Frame *frame) {
    RCC->CSR |= RCC_CSR_LSION; // włączamy LSI

    while (!(RCC->CSR & RCC_CSR_LSIRDY)); // czekamy na stabilizacje
    isLSI = true;
    // WYSYŁAMY KOMUNIKAT
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

void executeIPRE(Frame *frame, uint8_t len) {
    if (len == 0) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc preskaler", 21);
        sendFrame(frame->command, frame->data, 21);
        return;
    }
    IWDG->KR = 0x00005555u; // to odblokowuje konfiguracje KR
    prescaler = 0;
    if (len == 1) {
        prescaler = frame->data[0];
    } else if (len == 2) {
        prescaler = (frame->data[0] << 8) | frame->data[1];
    }
    if (prescaler == 4 || prescaler == 8 || prescaler == 16 || prescaler == 32 ||
        prescaler == 64 || prescaler == 128 || prescaler == 256) {
        IWDG->PR = (prescaler == 4)   ? 0 : // skrocone instrukcje if,
                   (prescaler == 8)   ? 1 :
                   (prescaler == 16)  ? 2 :
                   (prescaler == 32)  ? 3 :
                   (prescaler == 64)  ? 4 :
                   (prescaler == 128) ? 5 : 6; // mapujemy wartości na to co trzeba przekazać do rejerstru
    } else {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc preskaler", 21);
        sendFrame(frame->command, frame->data, 21);
        return;
    }
    isPrescaler = true;
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

void executeIREL(Frame *frame, uint8_t len) {
    if (len == 0 || len > MAX_DATA_LEN) {
    	memcpy(frame->command, "FAIL", COMMAND_LEN);
    	sendFrame(frame->command, 0, 0);
        return;
    }
    reload = 0;
    if (len == 1) {
        reload = frame->data[0];
    } else if (len == 2) {
        reload = (frame->data[0] << 8) | frame->data[1];
    }
    if (reload >= 0 && reload <= 4095) {
        IWDG->KR = 0x5555; // Oblokuj IWDG
        IWDG->RLR = reload; // Ustaw wartosc reload
        // PRAWIDLOWA WARTOSC RELOAD
        isReloadI = true;
        memcpy(frame->command, "GOOD", COMMAND_LEN);
        sendFrame(frame->command, 0, 0);
    } else { // NIEPRAWIDLOWA WARTOSC RELOAD
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc reload", 19);
        sendFrame(frame->command, frame->data, 19);
    }
}

void executeIREF(Frame *frame, uint8_t len) {
    if (len == 1) {
    	iwdg_refresh_interval = frame->data[0];
    } else if (len == 2) {
    	iwdg_refresh_interval = (frame->data[0] << 8) | frame->data[1];
    }
    if(iwdg_refresh_interval >= 0 && iwdg_refresh_interval <= 50000) {
        memcpy(frame->command, "GOOD", COMMAND_LEN);	// powodzenie
        sendFrame(frame->command, 0, 0);
    } else {
        memcpy(frame->command, "FAIL", COMMAND_LEN);	// zla wartosc reload
        memcpy(frame->data, "Nieprawidlowy interwal", 23);
        sendFrame(frame->command, frame->data, 23);
    }
}

void executeIINI(Frame *frame, uint8_t len) {
	if(isLSI && isReloadI && isPrescaler) {
		iwdg_initialized = true;
		systick_counter = 0; // synchronizacja
		IWDG->KR = 0xCCCC;
	    memcpy(frame->command, "GOOD", COMMAND_LEN);
	    sendFrame(frame->command, 0, 0);
	}	else {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "Sprawdz konfiguracje IWDG", 25);
        sendFrame(frame->command, frame->data, 25);
	}
}

void executeIGET(Frame *frame, uint8_t len) {
    snprintf(frame->data, 10, "%02X%02X;%02X%02X",
             (prescaler >> 8) & 0xFF, prescaler & 0xFF,
             (reload >> 8) & 0xFF, reload & 0xFF
    );
    memcpy(frame->command, "BACK", COMMAND_LEN);
    sendFrame(frame->command, frame->data, 9);
}

void executeICNT(Frame *frame, uint8_t len) {
    snprintf(frame->data, 18,"IWDGcntr:%02X%02X%02X%02X",	// formatowanie string
            (iwdg_refresh_cntr >> 24) & 0xFF,  // najstarszy bajt
            (iwdg_refresh_cntr >> 16) & 0xFF,
            (iwdg_refresh_cntr >> 8) & 0xFF,
            iwdg_refresh_cntr & 0xFF);         // najmlodszy bajt
    memcpy(frame->command, "BACK", COMMAND_LEN);
    sendFrame(frame->command, frame->data, 17);
}

void executeWAPB(Frame* frame, uint8_t len) {
    if (len != 0) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "dane musza byc puste", 21);
        sendFrame(frame->command, frame->data, 21);
        return;
    }
    RCC->APB1ENR |= RCC_APB1ENR_WWDGEN; // Włączenie zegara dla WWDG
    isAPB = true; // zmiana flagi
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

// KOMENDY DO WWDG
void executeWPRE(Frame *frame, uint8_t len) {
	if (len != 1) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "niepoprawna dlugosc danych", 26);
        sendFrame(frame->command, frame->data, 26);
        return;
	    }

    prescaler_wwdg = frame->data[0];
    if (prescaler_wwdg == 1 || prescaler_wwdg == 2 || prescaler_wwdg == 4 || prescaler_wwdg == 8) {
        WWDG->CFR &= ~WWDG_CFR_WDGTB; // Czyszczenie bitów preskalera
        WWDG->CFR |= (prescaler_wwdg == 1) ? 0x0 :
                     (prescaler_wwdg == 2) ? 0x1 :
                     (prescaler_wwdg == 4) ? 0x2 : 0x3; // Ustawienie preskalera
        isWprescaler = true; // ustawienie flagi
        memcpy(frame->command, "GOOD", COMMAND_LEN);
        sendFrame(frame->command, 0, 0);
	 } else {
    	memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc preskaler", 21);
        sendFrame(frame->command, frame->data, 21);
    }
}

void executeWREL(Frame *frame, uint8_t len) {
	// sprawdzenie czy mozna ustawic wartosc reload
    if (!isWindow || (frame->data[0] > 127 && frame->data[0] < window)) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        if(!isWindow) {
            memcpy(frame->data, "skonfiguruj wartosc okna", 24);
            sendFrame(frame->command, frame->data, 24);
            return;
        }
        memcpy(frame->data, "zla wartosc reload", 19);
        sendFrame(frame->command, frame->data, 19);
        return;
    }
    isReloadW = true;
    reload_wwdg = frame->data[0];
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

void executeWWIN(Frame *frame, uint8_t len) {
    if (len != 1 || frame->data[0] < 64 || frame->data[0] > 127 ) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc okna", 17);
        sendFrame(frame->command, frame->data, 17);
        return;
    }

    window = frame->data[0];
    WWDG->CFR = (WWDG->CFR & ~WWDG_CFR_W) | (window << 0);
    isWindow = true;
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

void executeWEWI(Frame *frame, uint8_t len) {
    if (len != 0) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zle dane wejsciowe", 19);
        sendFrame(frame->command, frame->data, 19);
        return;
    }
    WWDG->CFR |= WWDG_CFR_EWI; // Włączenie Early Wakeup Interrupt
    ewi_flag = 1;
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}


void executeWINI(Frame *frame, uint8_t len) {
    if (len != 0) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "dane musza byc puste", 21);
        sendFrame(frame->command, frame->data, 21);
        return;
    }
    if(isAPB && isWindow && isWprescaler && isReloadW) {
    	memcpy(frame->command, "GOOD", COMMAND_LEN);
    	sendFrame(frame->command, 0, 0);
    	wwdg_initialized = true;
    	WWDG->CR = WWDG_CR_WDGA | reload_wwdg;
    	systick_counter = 0; // synchronizacja systick'a z watchdogiem
    } else {
    	memcpy(frame->command, "FAIL", COMMAND_LEN);
    	memcpy(frame->data, "sprawdz konfiguracje WWDG", 25);
    	sendFrame(frame->command, frame->data, 25);
    }
}

void executeWREF(Frame *frame, uint8_t len) {
    if (len != 1) {
        memcpy(frame->command, "FAIL", COMMAND_LEN);
        memcpy(frame->data, "zla wartosc interwalu", 22);
        sendFrame(frame->command, frame->data, 22);
        return;
    }
    wwdg_refresh_interval = frame->data[0];
    memcpy(frame->command, "GOOD", COMMAND_LEN);
    sendFrame(frame->command, 0, 0);
}

void executeWGET(Frame *frame, uint8_t len) {
    snprintf(frame->data, 14, "%02X%02X;%02X;%02X;%02X",
             (wwdg_refresh_interval >> 8) & 0xFF,  // wyższy bajt wwdg_refresh_interval
             wwdg_refresh_interval & 0xFF,        // ziższy bajt wwdg_refresh_interval
			 prescaler_wwdg,
             window,
             reload_wwdg);
    memcpy(frame->command, "BACK", COMMAND_LEN);
    sendFrame(frame->command, frame->data, 13);
}

void executeWCNT(Frame *frame, uint8_t len) {
    snprintf(frame->data, 18,"WWDGcntr:%02X%02X%02X%02X",	// formatowanie string
            (wwdg_refresh_cntr >> 24) & 0xFF,  // najstarszy bajt
            (wwdg_refresh_cntr >> 16) & 0xFF,
            (wwdg_refresh_cntr >> 8) & 0xFF,
            wwdg_refresh_cntr & 0xFF);         // najmlodszy bajt
    memcpy(frame->command, "BACK", COMMAND_LEN);
    sendFrame(frame->command, frame->data, 17);
}
