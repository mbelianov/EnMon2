//#define _DEBUG_
#define _DISABLE_TLS_

#include "OpCodes.h"
//#include <SPI.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ThingerWebConfig.h>
//#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266AVRISP.h>
#include <NtpClientLib.h>
#include <Ticker.h>
#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include "StreamString.h"

Ticker ticker;
int led_indicator = 16;

std::unique_ptr<ESP8266WebServer> server;
File fsUploadFile;

ThingerWebConfig thing;

struct sample_struct{
  time_t timestamp;
  float phase1;
  float phase2;
  float phase3;
} sample;

const int rx_pin = 4;
const int tx_pin = 5;
SoftwareSerial mySerial(rx_pin, tx_pin);
SoftEasyTransfer ET; 

comm_struct data;
void *ptrData = &(data.dt);

const uint16_t avrisp_port = 328;
const uint8_t avr_reset_pin = 2;
ESP8266AVRISP avrprog(avrisp_port, avr_reset_pin);

enum {
  idle,
  start_download,
  cont_download,
  finish_download
} histDataDnldControl;
unsigned long histDataDnldFlowCtrl = 0;
unsigned long histDataDnldFlowRate = 50; // 50 ms

unsigned long flip_period = 500;
void flip_on(){
  digitalWrite(led_indicator, HIGH);
  ticker.once_ms(75, flip_off);
}

void flip_off(){
   digitalWrite(led_indicator, LOW);
   ticker.once_ms(flip_period, flip_on);
}


//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
    flip_period = 100;
}

//gets called when WiFiManager connects to AP
void connectedCallback () {
    flip_period = 2000;
}

void createJSON (String &str, const sample_struct* sample){
  
    String json("{");

      json.concat(F("\"timestamp\":"));json.concat(sample->timestamp);
      json.concat(F(", \"phase1\":"));json.concat(sample->phase1);
      json.concat(F(", \"phase2\":"));json.concat(sample->phase2);
      json.concat(F(", \"phase3\":"));json.concat(sample->phase3);
    
    json += "}";

    str=json;
}

void setup() {
  Serial.begin(115200);
  //Serial.setDebugOutput(true);

  MDNS.begin("EnMon");
  MDNS.addService("http", "tcp", 80);  
  
  mySerial.begin(57600);
  ET.begin(details(data), &mySerial);
  
  pinMode(led_indicator, OUTPUT);
 
   flip_on(); //start flipping
  
  thing.setAPCallback(configModeCallback);
  thing.setConnectedCallback(connectedCallback);
  
  // resource output
  thing["ESP8266 Basic"] >> [](pson& out){
      out["UpTime"] = NTP.getUptimeString();
      out["CurrentNTPTime"] = NTP.getTimeDateString();
      out["LastSyncAt"] = NTP.getTimeDateString(NTP.getLastNTPSync());
      out["IP"]=WiFi.localIP().toString();
      out["compile_time"] = __TIME__ " "  __DATE__;
      
  };
  
  thing["Energy[kW]"] >> [](pson& out){
      out["timestamp"] = sample.timestamp;
      out["phase 1"] = sample.phase1;
      out["phase 2"] = sample.phase2;
      out["phase 3"] = sample.phase3;
  };
  
  thing.handle(); //workaround to setup WiFi connections
  
  histDataDnldControl = idle;

//  setupOTA();
  setupAVRISP();
  NTP.begin();

  webServerSetUp();
  server->begin();

  Serial.print(F("Free heap size: "));
  Serial.println(ESP.getFreeHeap());

  Serial.print(F("Flash size: "));
  Serial.println(ESP.getFlashChipSize());

  Serial.print(F("SPIFFS size: "));
  FSInfo fs_info;
  SPIFFS.begin();
  SPIFFS.info(fs_info);
  SPIFFS.end();
  Serial.println(fs_info.totalBytes);
  Serial.print(F("TCP_MSS: "));
#ifdef TCP_MSS
  Serial.println(TCP_MSS);
#else
  Serial.println(F("unknown"));
#endif
}


void loop() {
  
  thing.handle();
  server->handleClient();
//  ArduinoOTA.handle();
  handleAVRISP();
  handleComm();

  
  if (millis() - histDataDnldFlowCtrl > histDataDnldFlowRate){
    histDataDnldFlowCtrl = millis();
    switch (histDataDnldControl){
      case start_download:
            data.op = REQUEST | OP_HISTST;
            ET.sendData();
        break;
      case cont_download:
            data.op = REQUEST | OP_HISTCO;
            ET.sendData();
        break;
      case finish_download:
        break;
      default:
        break;
    }
  }

  yield();
}


