#include <Arduino.h>
#include "sim800.h"

#define sscanf_P(i, p, ...)    sscanf((i), (p), __VA_ARGS__)
#define println_param(prefix, p) print(F(prefix)); print(F(",\"")); print(p); println(F("\""));

#ifdef DEBUG_SIM800
#define PRINT(s) Serial.print(F(s))
#define PRINTLN(s) Serial.println(F(s))
#define DEBUG(...) Serial.print(__VA_ARGS__)
#define DEBUGQ(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.print("'")
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#define DEBUGQLN(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.println("'")
#else
#define PRINT(s)
#define PRINTLN(s)
#define DEBUG(...)
#define DEBUGQ(...)
#define DEBUGLN(...)
#define DEBUGQLN(...)
#endif

sim800::sim800(){}

void sim800::begin()
{
	_serial.begin(_serialSpeed, SERIAL_8N1, SIM800_RX, SIM800_TX);
#ifdef DEBUG_SIM800
	printf("\n_serial.begin(%d, %d, %d, %d)\n", _serialSpeed, SERIAL_8N1, SIM800_RX, SIM800_TX);
#endif
}

bool sim800::reset(bool flag_reboot)
{
	bool ok = false;
	if(flag_reboot)
	{
		ok = expect_AT_OK(F("+CFUN=1,1"));
		vTaskDelay(5000 / portTICK_RATE_MS);
	}
	ok = expect_AT_OK(F(""));if(!ok)expect_AT_OK(F(""));
	println(F("ATZ"));
	vTaskDelay(1000 / portTICK_RATE_MS);
	ok = expect_OK(5000);//if(!ok){println(F("ATZ"));vTaskDelay(1000 / portTICK_RATE_MS);expect_OK(5000);}
	println(F("ATE0"));
	vTaskDelay(1000 / portTICK_RATE_MS);
	ok = expect_OK(5000);if(!ok){println(F("ATE0"));vTaskDelay(1000 / portTICK_RATE_MS);ok = expect_OK(5000);}
	ok = expect_AT_OK(F("+CFUN=1"));if(!ok)expect_AT_OK(F("+CFUN=1"));
	return ok;
}

bool sim800::wakeup()
{
#ifdef DEBUG_AT
	PRINTLN("!!! SIM800 wakeup");
#endif
	if(!expect_AT_OK(F(""), 100))
	{
		bool flag_reboot = false;
		if(!expect_AT_OK(F(""), 100))// check if the chip is already awake, otherwise start wakeup
		{
		#ifdef DEBUG_AT
			PRINTLN("!!! SIM800 using PWRKEY wakeup procedure");
		#endif
			pinMode(SIM800_KEY, OUTPUT);
			pinMode(SIM800_PS, INPUT);
			if(digitalRead(SIM800_PS) == LOW)
			{
				do {
				digitalWrite(SIM800_KEY, HIGH);
				vTaskDelay(3000 / portTICK_RATE_MS);
				} while (digitalRead(SIM800_PS) == LOW);
			}
			else
			{
				do {
				digitalWrite(SIM800_KEY, LOW);
				vTaskDelay(1100 / portTICK_RATE_MS);
				digitalWrite(SIM800_KEY, HIGH);
				vTaskDelay(3000 / portTICK_RATE_MS);
				} while (digitalRead(SIM800_PS) == LOW);
			}
			pinMode(SIM800_KEY, INPUT_PULLUP);// make pin unused (do not leak)
		#ifdef DEBUG_AT
			PRINTLN("!!! SIM800 ok");
		#endif
		} else {
		#ifdef DEBUG_AT
			PRINTLN("!!! SIM800 already awake");
		#endif
		}
		return reset(flag_reboot);
	}
	else
		return true;
}

void sim800::setAPN(const __FlashStringHelper *apn, const __FlashStringHelper *user, const __FlashStringHelper *pass)
{
	_apn = apn;
	_user = user;
	_pass = pass;
}

bool sim800::unlock(const __FlashStringHelper *pin)
{
	print(F("AT+CPIN="));
	println(pin);
	return expect_OK();
}

bool sim800::time(char *date, char *time, char *tz)
{
	println(F("AT+CCLK?"));
	return expect_scan(F("+CCLK: \"%8s,%8s%3s\""), date, time, tz);
}

bool sim800::IMEI(char *imei)
{
	println(F("AT+GSN"));
	expect_scan(F("%s"), imei);
	return expect_OK();
}

bool sim800::CIMI(char *cimi)//ID sim card
{
	println(F("AT+CIMI"));
	expect_scan(F("%s"), cimi);
	return expect_OK();
}

bool sim800::battery(uint16_t &bat_status, uint16_t &bat_percent, uint16_t &bat_voltage) {
  println(F("AT+CBC"));
  if(!expect_scan(F("+CBC: %d,%d,%d"), &bat_status, &bat_percent, &bat_voltage)) {
    Serial.println(F("BAT status lookup failed"));
  }
  return expect_OK();
}

bool sim800::location(char *&lat, char *&lon, char *&date, char *&time)
{
	uint16_t loc_status;
	char reply[64];
	println(F("AT+CIPGSMLOC=1,1"));
	vTaskDelay(3000 / portTICK_RATE_MS);
	if (!expect_scan(F("+CIPGSMLOC: %d,%s"), &loc_status, reply, 10000)) {
		#ifdef DEBUG_AT
		Serial.println(F("GPS lookup failed"));
		#endif
	} else {
		lon = strdup(strtok(reply, ","));
		lat = strdup(strtok(NULL, ","));
		date = strdup(strtok(NULL, ","));
		time = strdup(strtok(NULL, ","));
	}
	return expect_OK() && loc_status == 0 && lat && lon;
}

bool sim800::shutdown()
{
#ifdef DEBUG_AT
	PRINTLN("!!! SIM800 shutdown");
#endif

	bool reboot = expect_AT_OK(F("+CFUN=1,1"));
	if(reboot) vTaskDelay(5000 / portTICK_RATE_MS);
	else
	{
		if (digitalRead(SIM800_PS) == HIGH)
		{
		#ifdef DEBUG_AT
			PRINTLN("!!! SIM800 shutdown using PWRKEY");
		#endif
			pinMode(SIM800_KEY, OUTPUT);
			digitalWrite(SIM800_KEY, HIGH);
			vTaskDelay(10 / portTICK_RATE_MS);
			digitalWrite(SIM800_KEY, LOW);
			pinMode(SIM800_KEY, INPUT_PULLUP);
		}
	}
#ifdef DEBUG_AT
	PRINTLN("!!! SIM800 shutdown ok");
#endif
	return true;
}

bool sim800::registerNetwork(uint16_t timeout)
{
#ifdef DEBUG_AT
	PRINTLN("!!! SIM800 waiting for network registration");
#endif
	expect_AT_OK(F(""));
	while (timeout -= 1000)
	{
		unsigned short int n = 0;
		println(F("AT+CREG?"));
		expect_scan(F("+CREG: 0,%hu"), &n);
	#ifdef DEBUG_PROGRESS
		switch (n)
		{
			case 0:
				PRINT("_");
				break;
			case 1:
				PRINT("H");
				break;
			case 2:
				PRINT("S");
				break;
			case 3:
				PRINT("D");
				break;
			case 4:
				PRINT("?");
				break;
			case 5:
				PRINT("R");
				break;
			default:
				DEBUG(n);
				break;
		}
	#endif
		if ((n == 1 || n == 5))
		{
		#ifdef DEBUG_PROGRESS
			PRINTLN("");
		#endif
			return true;
		}
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	return false;
}

bool sim800::enableGPRS(uint16_t timeout)
{
	expect_AT(F("+CIPSHUT"), F("SHUT OK"), 5000);
	expect_AT_OK(F("+CIPMUX=1")); // enable multiplex mode
	expect_AT_OK(F("+CIPRXGET=1")); // we will receive manually
	bool attached = false;
	while (!attached && timeout > 0)
	{
		attached = expect_AT_OK(F("+CGATT=1"), 10000);
		vTaskDelay(1000 / portTICK_RATE_MS);
		timeout -= 1000;
	}
	if (!attached) return false;
	if (!expect_AT_OK(F("+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), 10000)) return false;
	if(_apn)// set bearer profile access point name
	{
		print(F("AT+SAPBR=3,1,\"APN\",\""));
		print(_apn);
		println(F("\""));
		if (!expect_OK()) return false;
		if (_user)
		{
			print(F("AT+SAPBR=3,1,\"USER\",\""));
			print(_user);
			println(F("\""));
			if (!expect_OK()) return false;
		}
		if(_pass)
		{
			print(F("AT+SAPBR=3,1,\"PWD\",\""));
			print(_pass);
			println(F("\""));
			if (!expect_OK()) return false;
		}
	}
	expect_AT_OK(F("+SAPBR=1,1"), 30000);// open GPRS context
	do
	{
		println(F("AT+CGATT?"));
		attached = expect(F("+CGATT: 1"));
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	while(--timeout && !attached);
	return attached;
}

bool sim800::disableGPRS()
{
	expect_AT(F("+CIPSHUT"), F("SHUT OK"));
	if (!expect_AT_OK(F("+SAPBR=0,1"), 30000)) return false;
	return expect_AT_OK(F("+CGATT=0"));
}

unsigned short int sim800::HTTP_get(const char *url, unsigned long int *length)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	if (!expect_AT_OK(F("+HTTPACTION=0"))) return 1004;
	unsigned short int status;
	expect_scan(F("+HTTPACTION: 0,%hu,%lu"), &status, &length, 60000);
	return status;
}

unsigned short int sim800::HTTP_get(const char *url, unsigned long int *length, STREAM &file)
{
	unsigned short int status = HTTP_get(url, length);
	if (length == 0) return status;
	char *buffer = (char *) malloc(SIM800_BUFSIZE);
	uint32_t pos = 0;
	do
	{
		size_t r = HTTP_read(buffer, pos, SIM800_BUFSIZE);
	#ifdef DEBUG_PROGRESS
		if((pos % 10240) == 0)
		{
			PRINT(" ");
			DEBUGLN(pos);
		}
		else if(pos % (1024) == 0)
		{PRINT("<");}
	#endif
		pos += r;
		file.write(buffer, r);
	}
	while(pos < *length);
	free(buffer);
	return status;
}

size_t sim800::HTTP_read(char *buffer, uint32_t start, size_t length)
{
	println(F("AT+HTTPREAD"));
	unsigned long int available;
	expect_scan(F("+HTTPREAD: %lu"), &available);
#ifdef DEBUG_PACKETS
	PRINT("~~~ PACKET: ");
	DEBUGLN(available);
#endif
	if(available > CRITICAL_BUFFER_HTTPREAD)
	{
		PRINT("~~~ BUFFER_HTTPREAD: ");DEBUGLN(available);
		return -1;//2148341393
	}
	size_t idx = 0;
	if(available <= length) idx = read(buffer, (size_t) available);
	else idx = read(buffer, length);
	if(!expect_OK()) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ DONE: ");
	DEBUGLN(idx);
#endif
	return idx;
}

size_t sim800::HTTP_read_ota(esp_ota_handle_t ota_handle, uint32_t start, size_t length)
{
	println(F("AT+HTTPREAD"));
	unsigned long int available;
	expect_scan(F("+HTTPREAD: %lu"), &available);
#ifdef DEBUG_PACKETS
	PRINT("~~~ OTA PACKET: ");
	DEBUGLN(available);
#endif
	if(available > CRITICAL_BUFFER_HTTPREAD)
	{
		PRINT("~~~ OTA BUFFER_HTTPREAD: ");DEBUGLN(available);
		return -1;//2148341393
	}
	size_t idx = 0;
	if(available <= length) idx = read_ota(ota_handle, (size_t)available);
	else idx = read_ota(ota_handle, length);
	if(!expect_OK()) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ OTA DONE: ");
	DEBUGLN(idx);
#endif
	return idx;
}

unsigned short int sim800::HTTP_post(const char *url, unsigned long int *length)
{
	length = 0;
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1001;
	unsigned short int status;
	expect_scan(F("+HTTPACTION: 0,%hu,%lu"), &status, &length, 60000);
	return status;
}

unsigned short int sim800::HTTP_post(const char *url, unsigned long int *length, char *buffer, uint32_t size)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	length = 0;
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	if (!expect_AT_OK(F("+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\""))) return 1102;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	print(F("AT+HTTPDATA="));
	print(size);
	print(F(","));
	println((uint32_t) 3000);
	if (!expect(F("DOWNLOAD"))) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ '");
	DEBUG(buffer);
	PRINTLN("'");
#endif
	_serial.write((const uint8_t*)buffer, size);
	if (!expect_OK(5000)) return 1005;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1004;
	uint16_t status;
	while (!expect_scan(F("+HTTPACTION: 1,%hu,%lu"), &status, &length, 5000));// wait for the action to be completed, give it 5s for each try
	return status;
}


unsigned short int sim800::HTTP_post(const char *url, unsigned long int &length, STREAM &file, uint32_t size)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	// if (!expect_AT_OK(F("+HTTPPARA=\"UA\",\"UBIRCH#1\""))) return 1102;
	// if (!expect_AT_OK(F("+HTTPPARA=\"REDIR\",1"))) return 1103;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	print(F("AT+HTTPDATA="));
	print(size);
	print(F(","));
	println((uint32_t) 120000);
	if (!expect(F("DOWNLOAD"))) return 0;
	uint8_t *buffer = (uint8_t *) malloc(SIM800_BUFSIZE);
	uint32_t pos = 0, r = 0;
	do
	{
		for (r = 0; r < SIM800_BUFSIZE; r++)
		{
			int c = file.read();
			if (c == -1) break;
			_serial.write((uint8_t) c);
		}

		if (r < SIM800_BUFSIZE)
		{
		#ifdef DEBUG_PROGRESS
			PRINTLN("EOF");
		#endif
			break;
		}
	#ifdef DEBUG_SIM800
		if ((pos % 10240) == 0)
		{
			PRINT(" ");
			DEBUGLN(pos);
		}
		else if(pos % (1024) == 0)
		{ PRINT(">"); }
	#endif
		pos += r;
	}
	while(r == SIM800_BUFSIZE);
	free(buffer);
	PRINTLN("");
	if (!expect_OK(5000)) return 1005;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1004;
	uint16_t status;
	while(!expect_scan(F("+HTTPACTION: 1,%hu,%lu"), &status, &length, 5000));// wait for the action to be completed, give it 5s for each try
	return status;
}

inline size_t sim800::read(char *buffer, size_t length)
{
	uint32_t idx = 0;
	while(length)
	{
		while(length && _serial.available())
		{
			buffer[idx++] = (char)_serial.read();
			length--;
		}
	}
	return idx;
}

inline size_t sim800::read_ota(size_t length, esp_ota_handle_t ota_handle)
{
	esp_err_t err = -1;
	size_t idx = 0, i = 0;
	char* buffer = (char*)malloc(OTA_BUFFSIZE);
	if(buffer)
	{
		memset(buffer, 0, OTA_BUFFSIZE);
		while(length)
		{
			while(length && _serial.available())
			{
				buffer[i++] = (char) _serial.read();
				idx++;
				length--;
				if(i == OTA_BUFFSIZE)
				{
					i = 0;
					err = esp_ota_write(ota_handle, (const void *)buffer, OTA_BUFFSIZE);
					memset(buffer, 0, OTA_BUFFSIZE);
					// ESP_ERROR_CHECK( err );
					if(err != ESP_OK) break;
				}
			}
			 if(err != ESP_OK){idx = 0; break;}
		}
		free(buffer);
	}
	return idx;
}

bool sim800::connect(const char *address, unsigned short int port, uint16_t timeout)
{
	if (!expect_AT(F("+CIPSHUT"), F("SHUT OK"))) return false;
	if (!expect_AT_OK(F("+CMEE=2"))) return false;
	if (!expect_AT_OK(F("+CIPQSEND=1"))) return false;
	print(F("AT+CSTT=\""));// bring connection up, force it
	print(_apn);
	println(F("\""));
	if (!expect_OK()) return false;
	if (!expect_AT_OK(F("+CIICR"))) return false;
	bool connected;
	do// try five times to get an IP address
	{
		char ipaddress[23];
		println(F("AT+CIFSR"));
		expect_scan(F("%s"), ipaddress);
		connected = strcmp_P(ipaddress, PSTR("ERROR")) != 0;
		if(!connected) vTaskDelay(1 / portTICK_RATE_MS);
	}
	while(timeout-- && !connected);
	if(!connected) return false;
	print(F("AT+CIPSTART=0,\"TCP\",\""));
	print(address);
	print(F("\",\""));
	print(port);
	println(F("\""));
	if(!expect_OK()) return false;
	if(!expect(F("0, CONNECT OK"), 30000)) return false;
	return connected;
}

bool sim800::status()
{
	println(F("AT+CIPSTATUS=0"));
	char status[SIM800_BUFSIZE];
	expect_scan(F("+CIPSTATUS: %s"), status);
	DEBUGLN(status);
	if(!expect_OK()) return false;
	return strcmp_P(status, PSTR("CONNECTED")) < 0;
}

bool sim800::disconnect()
{
	return expect_AT_OK(F("+CIPCLOSE=0"));
}

bool sim800::send(char *buffer, size_t size, unsigned long int &accepted)
{
	print(F("AT+CIPSEND=0,"));
	println((uint32_t) size);
	if(!expect(F("> "))) return false;
	_serial.write((const uint8_t *) buffer, size);
	if(!expect_scan(F("DATA ACCEPT: 0,%lu"), &accepted, 3000))
	{
	// we have a buffer of 319488 bytes, so we are optimistic,
	// even if a temporary fail occurs and just carry on
	// (verified!)
	//return false;
	}
	return accepted == size;
}

size_t sim800::receive(char *buffer, size_t size)
{
	size_t actual = 0;
	while(actual < size)
	{
		uint8_t chunk = (uint8_t) min(size - actual, 128);
		print(F("AT+CIPRXGET=2,0,"));
		println(chunk);
		unsigned long int requested, confirmed;
		if(!expect_scan(F("+CIPRXGET: 2,%*d,%lu,%lu"), &requested, &confirmed)) return 0;
		actual += read(buffer, (size_t) confirmed);
	}
	return actual;
}

/* ===========================================================================
 * PROTECTED
 * ===========================================================================
 */

// read a line
size_t sim800::readline(char *buffer, size_t max, uint16_t timeout)
{
	uint16_t idx = 0;
	while(--timeout)
	{
		while(_serial.available())
		{
			char c = (char)_serial.read();
			if(c == '\r') continue;
			if(c == '\n')
			{
				if (!idx) continue;
				timeout = 0;
				break;
			}
			if(max - idx) buffer[idx++] = c;
		}
		if(timeout == 0) break;
		vTaskDelay(1 / portTICK_RATE_MS);
	}
	buffer[idx] = 0;
	return idx;
}

void sim800::eat_echo()
{
	while (_serial.available())
	{
		_serial.read();
		// don't be too quick or we might not have anything available
		// when there actually is...
		vTaskDelay(1 / portTICK_RATE_MS);
	}
}

void sim800::print(const __FlashStringHelper *s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGQLN(s);
#endif
	_serial.print(s);
}

void sim800::print(uint32_t s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGLN(s);
#endif
	_serial.print(s);
}


void sim800::println(const __FlashStringHelper *s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGQLN(s);
#endif
	_serial.print(s);
	eat_echo();
	_serial.println();
}

void sim800::println(uint32_t s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGLN(s);
#endif
	_serial.print(s);
	eat_echo();
	_serial.println();
}

bool sim800::expect_AT(const __FlashStringHelper *cmd, const __FlashStringHelper *expected, uint16_t timeout)
{
	print(F("AT"));
	println(cmd);
	vTaskDelay(10 / portTICK_RATE_MS);
	return expect(expected, timeout);
}

bool sim800::expect_AT_OK(const __FlashStringHelper *cmd, uint16_t timeout)
{
	return expect_AT(cmd, F("OK"), timeout);
}

bool sim800::expect(const __FlashStringHelper *expected, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	_serial.flush();
	return strcmp_P(buf, (const char PROGMEM *) expected) == 0;
}

bool sim800::expect_OK(uint16_t timeout)
{
	return expect(F("OK"), timeout);
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref) == 1;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1) == 2;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2, void *ref3, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1, ref2, ref3) == 4;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1, ref2) == 3;
}

bool sim800::is_urc(const char *line, size_t len)
{
	urc_status = 0xff;
	for(uint8_t i = 0; i < 17; i++)
	{
	#ifdef __AVR__
		const char *urc = (const char *) pgm_read_word(&(_urc_messages[i]));
	#else
		const char *urc = _urc_messages[i];
	#endif
		size_t urc_len = strlen(urc);
		if(len >= urc_len && !strncmp(urc, line, urc_len))
		{
		#ifdef DEBUG_URC
			PRINT("!!! SIM800 URC(");
			DEBUG(i);
			PRINT(") ");
			DEBUGLN(urc);
		#endif
			urc_status = i;
			return true;
		}
	}
	return false;
}

bool sim800::check_sim_card()
{
	#ifdef DEBUG_URC
		PRINTLN("!!! SIM800 check SIM card inserted...");
	#endif
	println(F("AT+CSMINS?"));
	return expect(F("+CSMINS: 0,1"), 3000);
	// println(F("AT+CPIN?"));
	// return expect(F("CPIN: READY"), 3000);
}

int sim800::get_signal(int& ber)
{
	int rssi = 0;
	println("AT+CSQ");
	vTaskDelay(3000 / portTICK_RATE_MS);
	expect_scan(F("+CSQ: %2d"), (void*)&rssi, (void*)ber);
#ifdef DEBUG_URC
	PRINT("!!! SIM800 RSSI ");DEBUG(rssi);PRINTLN(" dBm");
	PRINT("!!! SIM800 BER ");DEBUG(ber);PRINTLN(" %");
#endif
	return rssi;
}

void sim800::set_operator()
{
	int operator_index = -1;
	char* operator_name = (char*)malloc(64);
	if(operator_name)
	{
		memset(operator_name, 0, 64);
		println(F("AT+COPS?"));
		if(expect_scan(F("%s"), operator_name, 3000))
		{
			#if GSM_DEBUG
			printf("\nGSM: AT RESPONSE: [%s]", operator_name);
			#endif
			for(int i = 0; i < 4; ++i)
			{
				if(strstr(operator_name, operators[i]))
				{
					operator_index = i;
					break;
				}
			}
		}
		free(operator_name);
	}
	if(operator_index > -1 && operator_index != current_operator)
	{
		current_operator = operator_index;
		setAPN(apns[current_operator], users[current_operator], pwds[current_operator]);
	}
}

bool sim800::gsm_init()
{
	uint16_t ip0=0, ip1=0, ip2=0, ip3=0;
	begin();
	while(!wakeup())
	{
		vTaskDelay(3000 / portTICK_RATE_MS);
	}
	expect_AT_OK(F(""));
	expect_AT_OK(F("+CSCLK=0"));//disable sleep mode
	expect_AT_OK(F("+CNMI=0,0,0,0,0"));//disable incoming SMS
	expect_AT_OK(F("+GSMBUSY=1"));//disable incoming calls
	expect_AT_OK(F("+CBC"), 2000);//power monitor
	expect_AT_OK(F("+CADC?"), 2000);//acp monitor
	if(!check_sim_card())
	{
		vTaskDelay(3000 / portTICK_RATE_MS);
		while(!check_sim_card()) vTaskDelay(60000 / portTICK_RATE_MS);
	}
	gsm_rssi = get_signal(gsm_ber);
	while(gsm_rssi < 1 || gsm_rssi > 98)
	{
		vTaskDelay(10000 / portTICK_RATE_MS);
		gsm_rssi = get_signal(gsm_ber);
	}
	while(!registerNetwork())
	{
		vTaskDelay(20000 / portTICK_RATE_MS);
		shutdown();
		wakeup();
	}
	println(F("AT+CGATT?"));
	expect(F("+CGATT: "), 3000);
	set_operator();
	println(F("AT+SAPBR=1,1"));
	expect_OK();
	println(F("AT+SAPBR=2,1"));
	vTaskDelay(2000 / portTICK_RATE_MS);
	bool result = expect_scan(F("+SAPBR: 1,1,\"%d.%d.%d.%d\""), &ip0, &ip1, &ip2, &ip3);
	if(ip0==0 && ip1==0 && ip2==0 && ip3==0){println(F("AT+SAPBR=2,1"));vTaskDelay(2000/portTICK_RATE_MS);result=expect_OK();}
	return result;
}

void sim800::update_esp(String url_update)
{
	unsigned long int len = 0;
	Serial.println("==== START UPDATE ====");
	Serial.println(url_update);
	uint16_t stat = HTTP_get(url_update.c_str(), &len);
	vTaskDelay(3000/portTICK_RATE_MS);
	Serial.print("UPDATE HTTP status = ");Serial.print(stat);Serial.print("; received length = ");Serial.println(len);
	if(stat > 200)
	{
		stat = HTTP_get(url_update.c_str(), &len);
		Serial.print("UPDATE HTTP status = ");Serial.print(stat);Serial.print("; received length = ");Serial.println(len);
		if(stat > 200) return;
	}
	else// else if(stat == 37 || stat == 200)
	{
		char* buffer = (char*)malloc(16);
		if(buffer)
		{
			memset(buffer, 0, 16);
			size_t result_read = HTTP_read(buffer, 0, 16);
			if(result_read == -1)
			{
				free(buffer);
				return;
			}
			Serial.print("UPDATE HTTP read = ");Serial.print(buffer);Serial.print("; received length = ");Serial.println(result_read);
			if(result_read > 0)
			{
				// url_update = String(WEB_URL_API) + "/upgrade/" + String(buffer);
				Serial.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
				unsigned short int status = HTTP_get(url_update.c_str(), &len);
				if(status < 201)// if (len > 0)
				{
					esp_err_t err;
					esp_ota_handle_t update_handle = 0;/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
					const esp_partition_t *update_partition = NULL;
					printf("Starting OTA example...\n");
					const esp_partition_t *configured = esp_ota_get_boot_partition();
					const esp_partition_t *running = esp_ota_get_running_partition();
					if(configured != running)
					{
						printf("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n", configured->address, running->address);
						printf("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n");
					}
					printf("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);
					update_partition = esp_ota_get_next_update_partition(NULL);
					assert(update_partition != NULL);
					printf("Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);
					err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
					if(err != ESP_OK)
					{
						printf("esp_ota_begin failed, error=%d\n", err);
						return;
					}
					else
					{
						printf("esp_ota_begin succeeded\n");
						uint32_t buff_len = 0;
						size_t readed_ota_bytes = HTTP_read_ota(update_handle, buff_len, 512*1024);
						printf("Total Write binary data length : %d\n", readed_ota_bytes);
						if(esp_ota_end(update_handle) != ESP_OK)
							printf("esp_ota_end failed!\n");
						else
						{
							err = esp_ota_set_boot_partition(update_partition);
							if(err != ESP_OK)
								printf("esp_ota_set_boot_partition failed! err=0x%x\n", err);
							else
							{
								printf("Prepare to restart system!\n");
								esp_restart();
							}
						}
					}
				}
			}
			free(buffer);
		}
	}
}
