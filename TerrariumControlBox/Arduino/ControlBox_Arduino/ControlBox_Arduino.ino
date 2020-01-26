#include <SimpleRotary.h>


#include <U8x8lib.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include "SerialCommands_ASCII.h"
#include <BlueDot_BME280.h>

#define LED_DRIVER_PIN 5

const static char ENV_SENSOR_ID_1[] PROGMEM = "thp03";
const static char ENV_SENSOR_ID_2[] PROGMEM = "thp04";

#define INTERVAL_ENV_SENSOR_UPDATE  60000    //1 minute

const static char MQTT_PAYLOAD_ENV_SENSOR[]   PROGMEM   = "{\"temperature\":%s,\"humidity\":%s,\"pressure\":%s,\"battery\":%s}";
const static char MQTT_TOPIC_ENV_SENSOR[]     PROGMEM   = "home/data/%s";
const static char MQTT_TOPIC_LED_DATA[]       PROGMEM   = "home/data/leds01";



// Pin A, Pin B, Button Pin
SimpleRotary rotary(2,4,3);

PacketSerial myPacketSerial;
SoftwareSerial espSerial(13, 12);//rx,tx  

int const cBrightnessDefault = 0;
int const cBrightnessMin = 0;
int const cBrightnessMax = 255;
int const cBrightnessIncrement = 4;

int _brightnessValue = cBrightnessDefault;
bool _ledOn = true;

bool IF_DEBUG=true;

/* OLED screen constructor */
U8X8_SSD1327_MIDAS_128X128_4W_SW_SPI u8x8(/* clock=*/ A0, /* data=*/ A1, /* cs=*/ A2, /* dc=*/ A3, /* reset=*/ 6);


BlueDot_BME280 bme1;                                     //Object for Sensor 1
BlueDot_BME280 bme2;                                     //Object for Sensor 2

int bme1Detected = 0;                                    //Checks if Sensor 1 is available
int bme2Detected = 0;                                    //Checks if Sensor 2 is available

long timerEnvSensorUpdate = 0;

/*
void _setBrightnessValue(int brightnessValue);
void _updateLED();
void _broadcastBrightnessValue();
void _processCommand(const uint8_t* buffer[], size_t size);

void _setupBMESensors();
*/

void _debug(const __FlashStringHelper* format, ...) {
  if (IF_DEBUG) {
    char _buf[128];

    va_list args;

    va_start(args, format);
    vsnprintf_P(_buf, sizeof(_buf)-1, (const char*)format, args);
    va_end(args);    
      
    Serial.print(F("[DEBUG]: "));
    Serial.println(_buf);
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);  

  // init timers
  timerEnvSensorUpdate = 0;

  u8x8.begin();           // Init the OLED screen
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_r);  
  
  espSerial.begin(19200);
  delay(100);
  myPacketSerial.setStream(&espSerial);
  myPacketSerial.setPacketHandler(&_processCommand);
  myPacketSerial.send("100", "0", NULL);

  _setupBMESensors();

  _updateLED();
  _broadcastBrightnessValue();    

  //sendEnvSensorValuesToServer((const __FlashStringHelper*)ENV_SENSOR_ID_1, 25.5f, 44.0f, 1234.0f);
}

void loop() {
  // put your main code here, to run repeatedly:

  byte i;

  i = rotary.rotate();      
  if (i == 1 || i == 2) {
    int brightnessLevel = _brightnessValue + cBrightnessIncrement * ((i-1)*2-1); // Fancy way to increment or decrement 

    _setBrightnessValue(brightnessLevel);      
    _updateLED();
    _broadcastBrightnessValue();
  }     

  i = rotary.push();
  if ( i == 1 ) {   
    _ledOn = !_ledOn;
    _updateLED();
    _broadcastBrightnessValue();
  }
    
  //delay(20);


  myPacketSerial.update();

  unsigned long currentTimer = millis();
    
  if(currentTimer - timerEnvSensorUpdate > INTERVAL_ENV_SENSOR_UPDATE || timerEnvSensorUpdate == 0) {
    _debug(F("Update env sensors"));
    if (bme1Detected) {      
      float temp = bme1.readTempC();
      float humidity = bme1.readHumidity();
      float pressure = bme1.readPressure();
  
      sendEnvSensorValuesToServer((const __FlashStringHelper*)ENV_SENSOR_ID_1, temp, humidity, pressure);
      displayEnvSensorValues(0, temp, humidity, pressure);
    }

    if (bme2Detected) {      
      float temp = bme2.readTempC();
      float humidity = bme2.readHumidity();
      float pressure = bme2.readPressure();
  
      sendEnvSensorValuesToServer((const __FlashStringHelper*)ENV_SENSOR_ID_2, temp, humidity, pressure);
      displayEnvSensorValues(1, temp, humidity, pressure);
    }
    
    timerEnvSensorUpdate = currentTimer;
  }
}


void _setBrightnessValue(int brightnessValue) {
  _brightnessValue = brightnessValue;
  
  if (_brightnessValue <= cBrightnessMin) {
    _brightnessValue = cBrightnessMin;
    _ledOn = false;
  }
  else
    _ledOn = true;
    
  if (_brightnessValue >= cBrightnessMax) _brightnessValue = cBrightnessMax;
}

void _updateLED() {
  int brightnessValue = _ledOn ? _brightnessValue : 0;
  analogWrite(LED_DRIVER_PIN, brightnessValue);

  char buf[8];

  if (brightnessValue == 0) {
    strcpy(buf, "off  ");
  } else if (brightnessValue == 255) {
    strcpy(buf, "max  ");
  } else {
    int bv100 = (int)round((float)brightnessValue / 255.0f * 100.0f);
    sprintf(buf, "%d  ", bv100);
  }

  u8x8.draw2x2String(6,14,buf);
  
  _debug(F("LED On: %d"), _ledOn); 
  _debug(F("Brightness: %d"), brightnessValue); 
}

void _broadcastBrightnessValue() {
  char bufPayload[8] = {0};  
  char bufTopic[32] = {0};

  strncpy_P(bufTopic, MQTT_TOPIC_LED_DATA, sizeof(bufTopic)-1);    
  sprintf(bufPayload, "%d", _ledOn ? _brightnessValue : 0);
    
  myPacketSerial.send("20", bufTopic, bufPayload, NULL);
}


// ######## Process Commands ########## //

void _processCommand(const uint8_t* buffer[], size_t size) {

  _debug(F("Received command: %s"), (char*)buffer[0]);

  char* cmdCode = (char*)buffer[0]; //param #0 - command code

  if (!strcmp(cmdCode, "30")) {
    // New LED brightness value
    int brightness = atoi((char*)buffer[1]); //param #1
    _debug(F("Changing LED brightness to: %d"), brightness);    
    if (brightness != "") {  
      _setBrightnessValue(brightness);
      _updateLED();
      _broadcastBrightnessValue();
    }
  }  
  else {
    _debug(F("Unknown command code: %s"), cmdCode);
  }   
}


// ######## BME280 Operations ########## //

void _setupBMESensors() { 
  bme1.parameter.communication = 0;                   //I2C communication for Sensor 1 (bme1)
  bme2.parameter.communication = 0;                   //I2C communication for Sensor 2 (bme2)

  bme1.parameter.I2CAddress = 0x76;                   //I2C Address for Sensor 1 (bme1)
  bme2.parameter.I2CAddress = 0x77;                   //I2C Address for Sensor 2 (bme2)

  bme1.parameter.sensorMode = 0b11;                   //Setup Sensor mode for Sensor 1
  bme2.parameter.sensorMode = 0b11;                   //Setup Sensor mode for Sensor 2 

  bme1.parameter.IIRfilter = 0b100;                   //IIR Filter for Sensor 1
  bme2.parameter.IIRfilter = 0b100;                   //IIR Filter for Sensor 2

  bme1.parameter.humidOversampling = 0b101;           //Humidity Oversampling for Sensor 1
  bme2.parameter.humidOversampling = 0b101;           //Humidity Oversampling for Sensor 2

  bme1.parameter.tempOversampling = 0b101;            //Temperature Oversampling for Sensor 1
  bme2.parameter.tempOversampling = 0b101;            //Temperature Oversampling for Sensor 2

  bme1.parameter.pressOversampling = 0b101;           //Pressure Oversampling for Sensor 1
  bme2.parameter.pressOversampling = 0b101;           //Pressure Oversampling for Sensor 2 

  bme1.parameter.pressureSeaLevel = 1013.25;          //default value of 1013.25 hPa (Sensor 1)
  bme2.parameter.pressureSeaLevel = 1013.25;          //default value of 1013.25 hPa (Sensor 2)

  //Also put in the current average temperature outside (yes, really outside!)
  //For slightly less precise altitude measurements, just leave the standard temperature as default (15°C and 59°F);

  bme1.parameter.tempOutsideCelsius = 15;             //default value of 15°C
  bme2.parameter.tempOutsideCelsius = 15;             //default value of 15°C

  bme1.parameter.tempOutsideFahrenheit = 59;          //default value of 59°F
  bme2.parameter.tempOutsideFahrenheit = 59;          //default value of 59°F

  if (bme1.init() != 0x60) {    
    _debug(F("First BME280 Sensor not found!"));
    bme1Detected = 0;
  } else {
    _debug(F("First BME280 Sensor found!"));
    bme1Detected = 1;
  }
  if (bme2.init() != 0x60) {    
    _debug(F("Second BME280 Sensor not found!"));
    bme2Detected = 0;
  } else {
    _debug(F("Second BME280 Sensor found!"));
    bme2Detected = 1;
  }  
}

void sendEnvSensorValuesToServer(const __FlashStringHelper* sensorId, float temp, float humidity, float pressure) {
  char bufPayload[128] = {0};  
  char bufTopic[32] = {0};
  char bufSensorId[32] = {0}; 

  strncpy_P(bufSensorId, (const char *)sensorId, sizeof(bufSensorId)-1);  
  snprintf_P(bufTopic, sizeof(bufTopic)-1, MQTT_TOPIC_ENV_SENSOR, bufSensorId);

  char strTmp[8] = {0};
  dtostrf(temp, 7, 2, strTmp);
  char strHum[8] = {0};
  dtostrf(humidity, 7, 2, strHum);
  char strPrs[8] = {0};
  dtostrf(pressure, 7, 2, strPrs);
  
  snprintf_P(bufPayload, sizeof(bufPayload)-1, MQTT_PAYLOAD_ENV_SENSOR, strTmp, strHum, strPrs, "33.1");  
  
  //_debug(F("Topic: %s"), bufTopic);
  //_debug(F("Payload: %s"), bufPayload);
  
  myPacketSerial.send("20", bufTopic, bufPayload, NULL);
}

void displayEnvSensorValues(int sensorPos, float tmp, float hum, float prs) {
  char bufT[8] = {0};
  char bufH[8] = {0};

  dtostrf(tmp, 4, 1, bufT);
  dtostrf(hum, 3, 0, bufH); strcat(bufH, "%");
  
  switch (sensorPos) {
    case 0:
      u8x8.clearLine(2);
      u8x8.clearLine(3);
      u8x8.draw2x2String(0,2,bufT);
      u8x8.draw2x2String(8,2,bufH);
      break;
    case 1:
      u8x8.clearLine(8);
      u8x8.clearLine(9);
      u8x8.draw2x2String(0,8,bufT);
      u8x8.draw2x2String(8,8,bufH);
      break;
    default:
      break;  
  } 
}
