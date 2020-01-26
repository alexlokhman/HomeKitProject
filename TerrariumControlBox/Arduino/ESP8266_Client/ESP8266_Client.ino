/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SerialCommands_ASCII.h"
#include "Adafruit_MQTT_Client.h"
#include "env.h"

bool IF_DEBUG=false;

PacketSerial myPacketSerial;

ADC_MODE(ADC_VCC);
WiFiClient wifiClient;

Adafruit_MQTT_Client *mqtt = NULL;
Adafruit_MQTT_Subscribe *mqttSub = NULL;

#define       SERIAL_BAUD           19200

const char    WIFI_SSID[]           /*PROGMEM*/     =     ENV_WIFI_SSID;
const char    WIFI_PASSWORD[]       /*PROGMEM*/     =     ENV_WIFI_PASSWORD;
const char    MQTT_SERVER_IP[]      /*PROGMEM*/     =     ENV_MQTT_SERVER_IP;  
int           MQTT_SERVER_PORT                      =     ENV_MQTT_SERVER_PORT;
const char    SUBSCRIBE_TO_TOPIC[]  /*PROGMEM*/     =     "home/cmd/leds01";


void _debug(const __FlashStringHelper* format, ...) {
  if (IF_DEBUG) {
    char _buf[128];

    va_list args;

    va_start(args, format);
    vsnprintf_P(_buf, sizeof(_buf)-1, (const char*)format, args);
    va_end(args);    

    myPacketSerial.send("100", _buf, NULL);
  }
}


void WiFi_connect(char* WiFi_SSID, char* WiFi_PWD) {
    if (strcmp(WiFi_SSID, "") != 0 && strcmp(WiFi_PWD, "") != 0) {      
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(200);
      }

      _debug(F("Connecting to WiFi: %s"), WiFi_SSID);
      WiFi.begin(WiFi_SSID, WiFi_PWD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        _debug(F("."));
      }      
      delay(500);
    }   
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  if (mqtt == NULL) return;
 
  // Stop if already connected.
  if (mqtt->connected()) {
    return;
  }

  _debug(F("Connecting to mqtt... "));

  uint8_t retries = 3;
  while ((ret = mqtt->connect()) != 0) { // connect will return 0 for connected
       _debug(mqtt->connectErrorString(ret));
       _debug(F("Retrying MQTT connection in 5 seconds..."));
       mqtt->disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  _debug(F("MQTT Connected!"));
}


// ######## Process Commands ########## //

void _processCommand(const uint8_t* buffer[], size_t size) {

  _debug(F("Received command: %s"), (char*)buffer[0]);

  char* strCmdCode = (char*)buffer[0]; //param #0 - command code
  int cmdCode = atoi(strCmdCode);

  switch (cmdCode) {
    case 0: {
      // Connect to a WiFi network    

      char* WiFi_SSID = (char*)buffer[1]; //param #1
      char* WiFi_PWD = (char*)buffer[2]; //param #2
 
      WiFi_connect(WiFi_SSID, WiFi_PWD);
      break;  
    }
    case 1: {
      // return WiFi connection status    
      _debug(F("[WIFI] Status Response: %s"), WiFi.status() == WL_CONNECTED ? "1" : "0");

      myPacketSerial.send("1", WiFi.status() == WL_CONNECTED ? "1" : "0", NULL);      
      break;    
    }
    case 2: {
      // return WiFi station name
      _debug(F("[WIFI] WiFi Station Name: %s"), WiFi.SSID().c_str());
      myPacketSerial.send("2", WiFi.SSID().c_str(), NULL);
   
      break;
    }
    case 20: {
      // Publish to a topic
      char* topic = (char*)buffer[1]; 
      char* payload = (char*)buffer[2];
            
      //String topic = getCommandParameter(cmd, 1);
      //String payload = getCommandParameter(cmd, 2);
      _debug(F("MQTT publish. Topic: %s  Payload: %s"), topic, payload);    
      //if (topic != "" && payload != "") {
        Adafruit_MQTT_Publish mqttPublish = Adafruit_MQTT_Publish(mqtt, topic); 
        if (! mqttPublish.publish(payload)) {
          _debug(F("Failed"));
        } else {
          _debug(F("OK!"));
        }      
      //}
      break;  
    }
    case 100: {
      // set debug mode    
      char* debugState = (char*)buffer[1]; 
      int iDebugState = atoi(debugState);
      //String debugState = getCommandParameter(cmd, 1);
      IF_DEBUG = iDebugState==1;
      _debug(F("Set debug state to: %d"), IF_DEBUG);
      break;
    }
    
    default:
      _debug(F("Unknown command code: %s"), strCmdCode);
  }  
}




void _setupConnections() {
  // <100|1>
  // <0|SKYBD609|PDAVFDDQ>
  // <10|192.168.0.84|1883>
  // <30|home/cmd/leds01>
  // <200|0|SKYBD609|PDAVFDDQ|192.168.0.84|1883|home/cmd/leds01>


  IF_DEBUG = false;
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  _debug(F("Connecting to WiFi..."));

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _debug(F("..."));
  }

  mqtt = new Adafruit_MQTT_Client(&wifiClient, MQTT_SERVER_IP, MQTT_SERVER_PORT);
  delay(500);
  mqttSub = new Adafruit_MQTT_Subscribe(mqtt, SUBSCRIBE_TO_TOPIC);
  mqtt->subscribe(mqttSub);
  delay(500);
  MQTT_connect();      
}




void setup() {
  Serial.begin(SERIAL_BAUD);

  delay(100);
  myPacketSerial.setStream(&Serial);
  myPacketSerial.setPacketHandler(&_processCommand);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  _setupConnections(); 

  _debug(F("Setup done")); 
}

void loop() {
  // put your main code here, to run repeatedly:  

  myPacketSerial.update();

  if (WiFi.status() == WL_CONNECTED)
    MQTT_connect();
  
  if (WiFi.status() == WL_CONNECTED && mqtt->connected()) {
    //_debug(F("checking subscription"));
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt->readSubscription(1000))) {
      
      if (subscription) {
        _debug(F("Got subscription 1: %s"), (char *)subscription->lastread);
        myPacketSerial.send("30", (char *)subscription->lastread, NULL);
      }
    }  
  }

  delay(200);
}
