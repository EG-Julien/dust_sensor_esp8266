#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "wifi_info.h"

#define COV_RATIO                       0.2            //ug/mmm / mv
#define NO_DUST_VOLTAGE                 400            //mv
#define SYS_VOLTAGE                     1000           

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

const int SENSE_PIN = 0;
const int EMIT_PIN  = 13;

void my_homekit_setup();
void my_homekit_loop();
void my_homekit_report();
int filter_adc_value(int m);

void setup() {
	Serial.begin(115200);
	wifi_connect(); // in wifi_info.h
	my_homekit_setup();
  pinMode(EMIT_PIN, OUTPUT);
}

void loop() {
	my_homekit_loop();
	delay(10);
}

//==============================
// Homekit setup and loop
//==============================

// access your homekit characteristics defined in my_accessory.c
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t air_quality;
extern "C" homekit_characteristic_t pm25_density;

static uint32_t next_heap_millis = 0;
static uint32_t next_report_millis = 0;

void my_homekit_setup() {
	arduino_homekit_setup(&config);
}

void my_homekit_loop() {
	arduino_homekit_loop();
	const uint32_t t = millis();
	if (t > next_report_millis) {
		// report sensor values every 10 seconds
		next_report_millis = t + 1 * 1000;
		my_homekit_report();
	}
	if (t > next_heap_millis) {
		// show heap info every 5 seconds
		next_heap_millis = t + 5 * 1000;
		LOG_D("Free heap: %d, HomeKit clients: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

	}
}

void my_homekit_report() {
  digitalWrite(EMIT_PIN, HIGH);
  delayMicroseconds(280);
  float __air_quality = filter_adc_value(analogRead(SENSE_PIN));
  float __density, level;
  digitalWrite(EMIT_PIN, LOW);

  float __voltage = (SYS_VOLTAGE / 1024.0) * __air_quality * 11;

  if (__voltage >= NO_DUST_VOLTAGE) {
    __voltage -= NO_DUST_VOLTAGE;
    __density = __voltage * COV_RATIO;
  } else {
    __density = 0;
  }
  
  if (__density <= 35) {
      level = 1;
  } else if (__density > 35 && __density <= 75) {
      level = 2;
  } else if (__density > 75 && __density <= 115) {
      level = 3;
  } else if (__density > 115 && __density <= 150) {
      level = 4;
  } else {
      level = 5;
  }

  air_quality.value.int_value = level;

	LOG_D("Current PM25 density: %.1f", __density);
  LOG_D("Current air quality level : %.1f", level);
  //homekit_characteristic_notify(&air_quality, HOMEKIT_INT(level));
	homekit_characteristic_notify(&pm25_density, HOMEKIT_FLOAT(__density));
}

int filter_adc_value(int m) {
  static int flag_first = 0, _buff[10], sum;
  const int _buff_max = 10;
  int i;
  
  if(flag_first == 0)
  {
    flag_first = 1;

    for(i = 0, sum = 0; i < _buff_max; i++)
    {
      _buff[i] = m;
      sum += _buff[i];
    }
    return m;
  }
  else
  {
    sum -= _buff[0];
    for(i = 0; i < (_buff_max - 1); i++)
    {
      _buff[i] = _buff[i + 1];
    }
    _buff[9] = m;
    sum += _buff[9];
    
    i = sum / 10.0;
    return i;
  }
}