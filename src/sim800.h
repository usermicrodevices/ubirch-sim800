#ifndef SIM800_H
#define SIM800_H

/*debug AT i/o (very verbose)*/
// #define DEBUG_AT
// #define DEBUG_URC
/*debug receiving and sending of packets (sizes)*/
// #define DEBUG_PACKETS
/*debugging of send/receive progress (not very verbose)*/
// #define DEBUG_PROGRESS

#define OTA_BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
#define GSM_MAX_BUFFSIZE 1460
#define CRITICAL_BUFFER_HTTPREAD 102400

#define SIM800_CMD_TIMEOUT 30000
#define SIM800_SERIAL_TIMEOUT 1000
#define SIM800_BUFSIZE 64

#include "esp_ota_ops.h"

#include "driver/uart.h"
#include "soc/uart_struct.h"
#include <stdint.h>
#include <Stream.h>
// #include <Update.h>

#define STREAM Stream

#ifdef __AVR__
#include <SoftwareSerial.h>
// this is the maximum I could do using the board-mounted SIM800 on the ubirch #1
// if you are using an externally wired Modem, you may have to try a lower baud rate
#define SIM800_BAUD 57600
#define SIM800_RX   2
#define SIM800_TX   3
#define SIM800_RST  4
#define SIM800_KEY  7
#define SIM800_PS   8
#else
#define SIM800_BAUD 115200
#define SIM800_RX   16
#define SIM800_TX   17
#define SIM800_RST  6
#define SIM800_KEY  21
#define SIM800_PS   27
#ifdef F
#undef F
#define F(s) (s)
#endif
#define __FlashStringHelper char
#endif

// static int binary_file_length = 0;
// static char ota_write_data[OTA_BUFFSIZE + 1] = { 0 };
// static char ota_text[OTA_BUFFSIZE + 1] = { 0 };

class sim800 {

public:
	uint8_t urc_status = 0xff;

	sim800();
	void begin();
	void setAPN(const __FlashStringHelper *apn, const __FlashStringHelper *user, const __FlashStringHelper *pass);
	bool unlock(const __FlashStringHelper *pin);
	bool reset();
	bool shutdown();
	bool wakeup();
	bool registerNetwork(uint16_t timeout = SIM800_CMD_TIMEOUT);
	bool enableGPRS(uint16_t timeout = SIM800_CMD_TIMEOUT);
	bool disableGPRS();
	bool time(char *date, char *time, char *tz);
	bool IMEI(char *imei);
	bool battery(uint16_t &bat_status, uint16_t &bat_percent, uint16_t &bat_voltage);
	bool location(char *&lat, char *&lon, char *&date, char *&time);
	bool status();
	bool connect(const char *address, unsigned short int port, uint16_t timeout = SIM800_CMD_TIMEOUT);
	bool disconnect();
	bool send(char *buffer, size_t size, unsigned long int &accepted);
	size_t receive(char *buffer, size_t size);

	/**
	* HTTP requests only handle data up to 319488 bytes
	* This seems to be a limitation of the chip, a
	* larger payload will result in a 602 No Memory
	* result code
	*/

	// unsigned short int HTTP_get(const char *url, unsigned long int &length);
	unsigned short int HTTP_get(const char *url, unsigned long int *length);

	// unsigned short int HTTP_get(const char *url, unsigned long int &length, STREAM &file);
	unsigned short int HTTP_get(const char *url, unsigned long int *length, STREAM &file);
	// unsigned short int HTTP_get(const char *url, unsigned long int *length, UpdateClass &update, size_t* length_written);

	size_t HTTP_read(char *buffer, uint32_t start, size_t length);
	size_t HTTP_read_ota(esp_ota_handle_t ota_handle, uint32_t start, size_t length);
	unsigned short int HTTP_post(const char *url, unsigned long int *length);
	unsigned short int HTTP_post(const char *url, unsigned long int *length, char *buffer, uint32_t size);
	unsigned short int HTTP_post(const char *url, unsigned long int &length, STREAM &file, uint32_t size);
	bool expect_AT(const __FlashStringHelper *cmd, const __FlashStringHelper *expected, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect_AT_OK(const __FlashStringHelper *cmd, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect(const __FlashStringHelper *expected, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect_OK(uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect_scan(const __FlashStringHelper *pattern, void *ref, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	bool expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2, uint16_t timeout = SIM800_SERIAL_TIMEOUT);
	size_t read(char *buffer, size_t length);
	inline size_t read_ota(esp_ota_handle_t ota_handle, size_t length);
	size_t readline(char *buffer, size_t max, uint16_t timeout);
	void print(const char *s);
	void print(uint32_t s);
	void println(const char *s);

#ifdef __AVR__
	void print(const __FlashStringHelper *s);
	void println(const __FlashStringHelper *s);
#endif

	void println(uint32_t s);
	bool check_sim_card();
	int get_signal();
	void update_esp();
	void set_operator();

#ifdef __AVR__
    SoftwareSerial _serial = SoftwareSerial(SIM800_TX, SIM800_RX);
#else
    HardwareSerial _serial = HardwareSerial(1);
#endif

protected:
	const uint32_t _serialSpeed = SIM800_BAUD;
	const __FlashStringHelper *_apn;
	const __FlashStringHelper *_user;
	const __FlashStringHelper *_pass;
	void eat_echo();
	bool is_urc(const char *line, size_t len);

	const char* operators[4] = {"Bee Line GSM", "MTS", "MegaFon", "TELE2"};
	const char* apns[4] = {"internet.beeline.ru", "internet.mts.ru", "internet", "internet.tele2.ru"};
	const char* users[4] = {"beeline", "mts", "gdata", NULL};
	const char* pwds[4] = {"beeline", "mts", "gdata", NULL};
	int current_operator = 0;
	int gsm_signal = 0;
};

// this useful list found here: https://github.com/cloudyourcar/attentive

/* incoming socket data notification */
const char * const urc_01 PROGMEM = "+CIPRXGET: 1,";
/* FTP state change notification */
const char * const urc_02 PROGMEM = "+FTPGET: 1,";
/* PDP disconnected */
const char * const urc_03 PROGMEM = "+PDP: DEACT";
/* PDP disconnected (for SAPBR apps) */
const char * const urc_04 PROGMEM = "+SAPBR 1: DEACT";
/* AT+CLTS network name */
const char * const urc_05 PROGMEM = "*PSNWID:";
/* AT+CLTS time */
const char * const urc_06 PROGMEM = "*PSUTTZ:";
/* AT+CLTS timezone */
const char * const urc_07 PROGMEM = "+CTZV:";
/* AT+CLTS dst information */
const char * const urc_08 PROGMEM = "DST:";
/* AT+CLTS undocumented indicator */
const char * const urc_09 PROGMEM = "+CIEV:";
/* Assorted crap on newer firmware releases. */
const char * const urc_10 PROGMEM = "RDY";
const char * const urc_11 PROGMEM = "+CPIN: READY";
const char * const urc_12 PROGMEM = "Call Ready";
const char * const urc_13 PROGMEM = "SMS Ready";
const char * const urc_14 PROGMEM = "NORMAL POWER DOWN";
const char * const urc_15 PROGMEM = "UNDER-VOLTAGE POWER DOWN";
const char * const urc_16 PROGMEM = "UNDER-VOLTAGE WARNNING";
const char * const urc_17 PROGMEM = "OVER-VOLTAGE POWER DOWN";
const char * const urc_18 PROGMEM = "OVER-VOLTAGE WARNNING";

const char * const _urc_messages[] PROGMEM = {
        urc_01, urc_02, urc_03, urc_04, urc_06, urc_07, urc_08, urc_09, urc_10,
        urc_11, urc_12, urc_13, urc_14, urc_15, urc_16, urc_17, urc_18
};

#endif //UBIRCH_SIM800_H
