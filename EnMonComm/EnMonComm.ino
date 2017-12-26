/////////////////////////////////////////////////////////////////////////
//IOTAppStory: 1.0.6
//Thinger.io: ver. 2.7.2
//ESP8266: ver 2.4.0-RC2

#define APPNAME "EnMon"
#define VERSION "V2.0.1-dev"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0



#define _DISABLE_TLS_

#include <IOTAppStory.h>
#include "OpCodes.h"
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ThingerESP8266.h>
#include <ESP8266mDNS.h>
#include <ESP8266AVRISP.h>
#include <Ticker.h>
#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include "StreamString.h"
#include "ArduinoLogger.h"
#include <ArduinoJson.h>

////////////////////////////////////////////////////////
#include <time.h>
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          0       // use 60mn for summer time in some countries

#define NTP0_OR_LOCAL1  0       // 0:use NTP  1:fake external RTC

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

time_t _now;
time_t uptime;
uint32_t ts;
uint32_t ts1;
////////////////////////////////////////////////////////

IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);
unsigned long callHomeEntry = 0;
boolean firstBoot;

const uint16_t log_listener_port = 2323;
ArduinoLogger logger(LOG_LEVEL_NOTICE, log_listener_port);

Ticker ticker;
int led_indicator = 16;

bool server_running = false;
std::unique_ptr<ESP8266WebServer> server;
File fsUploadFile;

const int _nrXF = 3;
char* user      = "";
char* device    = "";
char* device_credential = "";

//ThingerESP8266 thing(user, device, device_credential);
ThingerESP8266* thing;

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


// handle network events
void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
  flip_period = 100; //start fast blink to show that we have no IP
  logger.notice(F("Disconnected from %s: %d" CR), event_info.ssid.c_str(), event_info.reason);
}
void onSTAConnected(WiFiEventStationModeConnected event_info) {
  logger.trace(F("Connected to AP: %s" CR), event_info.ssid.c_str());
}
void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
  flip_period = 2000; //slow blink on succesfull connection
  logger.notice(F("Got IP address: %s" CR), ipInfo.ip.toString().c_str());
}
void onDHCPTimeout() {
  flip_period = 100;
  logger.trace(F("dhcp timeout" CR));
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
  ts = millis();
  IAS.serialdebug(true,115200);    

  pinMode(led_indicator, OUTPUT);
  flip_on(); //start flipping  
  
  logger.setWellcomePrintFunction(printWellcomeMsg);
  logger.setPrefix(printTimestamp);
  logger.notice(F("Booting..." CR));
    
  //set up event handlers...
  static WiFiEventHandler e1, e2, e3, e4;
  e1 = WiFi.onStationModeGotIP(onSTAGotIP);
  e2 = WiFi.onStationModeDisconnected(onSTADisconnected);
  e3 = WiFi.onStationModeConnected(onSTAConnected);
  //e4 = WiFi.onStationModeDHCPTimeout(onDHCPTimeout);

  // on first boot params should loaded from SPIFFS
  firstBoot = false;
  if(String(IAS.config.compDate) != String(COMPDATE)){
    firstBoot = true;
  }

  IAS.preSetConfig(String(APPNAME) + ESP.getChipId(), false);
  //IAS.preSetConfig("msb-wifi2", "belianovi", false);
  IAS.addField(user, "user", "User", 16);
  IAS.addField(device, "device", "Device", 16);
  IAS.addField(device_credential, "credentials", "Device Credentials", 40);
  IAS.begin(true);
  if (firstBoot){
    logger.notice(F("First boot. Loading params from SPIFFS." CR));
    loadParamsFromSPIFFS();
    IAS.writeConfig(true);
  }
 
//  logger.notice(F("User: %s" CR), user);
//  logger.notice(F("Device: %s" CR), device);
//  logger.notice(F("Device Credentials: %s" CR), device_credential); 
  
  thing = new ThingerESP8266(user, device, device_credential);

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org", "time.nist.gov");    
    
  mySerial.begin(57600);
  ET.begin(details(data), &mySerial);
  

  
  // resource output
  (*thing)["ESP8266 Basic"] >> [](pson& out){
      out["UpTime"] = printUpTimeAsString(uptime);
      out["Time"] = printTimestampAsString(); 
      out["IP"]=WiFi.localIP().toString();
      out["ssid"] = WiFi.SSID();
      out["level"]=WiFi.RSSI();
      out["compile_time"] = COMPDATE;      
  };

  (*thing)["ESP8266 Log"] >> [](pson& out){
      out["time"] = printTimestampAsString();
      out["free_heap"]=ESP.getFreeHeap();
      out["client_ip"]=server->client().remoteIP().toString();
      out["method"]=server->method();
      out["uri"]=server->uri();
  };  
  
  (*thing)["Energy[kW]"] >> [](pson& out){
      out["timestamp"] = sample.timestamp;
      out["phase 1"] = sample.phase1;
      out["phase 2"] = sample.phase2;
      out["phase 3"] = sample.phase3;
  };

  histDataDnldControl = idle;

  setupAVRISP();
  webServerSetUp();

  MDNS.begin("EnMon");
  MDNS.addService("http", "tcp", 80);  
  
  logger.notice(F("Free heap size: %l" CR), ESP.getFreeHeap());
  logger.notice(F("Flash size: %l" CR), ESP.getFlashChipSize());
  logger.notice(F("Connected to %s! Signal level %d!" CR), WiFi.SSID().c_str(), WiFi.RSSI());
  
  FSInfo fs_info;
  SPIFFS.begin();
  SPIFFS.info(fs_info);
  SPIFFS.end();
  logger.notice(F("SPIFFS size: %l" CR),fs_info.totalBytes);
#ifdef TCP_MSS
  logger.notice(F("TCP_MSS: %l" CR),TCP_MSS);
#else
  logger.notice(F("TCP_MSS: unknown" CR));
#endif

  //writeParamsToSPIFFS();
  logger.notice(F("Setup routine completed!" CR));

  logger.changeLogLevel(LOG_LEVEL_ERROR);

}



void loop() {
//  IAS.buttonLoop();    // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration
  if (WiFi.isConnected()) thing->handle();
  server->handleClient();
  handleAVRISP();
  handleComm();
  logger.handle();
 
  ts1 = millis();
  uptime += (ts1 - ts);
  ts = ts1;


  if (ts1 - callHomeEntry > 2*3600*1000) {      // only for development. Please change it to at least 2 hours in production
    writeParamsToSPIFFS(); // need to save params to SPIFFS otherwise they will be lost during firmware upgrade
    IAS.callHome();
    callHomeEntry = ts1;
  }  


//  if (ts1 % 4096 == 0){
//    logger.notice(F("%t: %l" CR), WiFi.isConnected(), (uint32_t)WiFi.localIP());
//  }

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

//print hello msg when telnet client succesfully connects to logger
void printWellcomeMsg(Print* p) {
  p->println(F("Hello!"));
  p->print(F("Free heap size: "));
  p->println(ESP.getFreeHeap());
}

//print timestamp as prefix before each log msg or whenever current time is needed
void printTimestamp(Print* p) {
  time(&_now);
  tm* tm = localtime(&_now);
  p->print('<');
  p->print(tm->tm_year+1900);p->print('/');
  p->print(tm->tm_mon+1);p->print('/');
  p->print(tm->tm_mday);p->print(' ');
  p->print(tm->tm_hour);p->print(':');
  p->print(tm->tm_min);p->print(':');
  p->print(tm->tm_sec);p->print('>');
  p->print(' ');
}

String printTimestampAsString(){
  StreamString s;
  printTimestamp (&s);
  return s;
}

String printUpTimeAsString(time_t t){
  StreamString s;
  t  /= 1000;
  s.print(t / (24L*3600L));
  s.print(F(" days "));
  s.print(t % (24L*3600L));
  s.print(F(" sec"));
  return s;
}


void writeParamsToSPIFFS(){
  String n("/");
  n.concat(APPNAME);
  n.concat(".params");
  String param("parameter");

  SPIFFS.begin();
  File paramFile = SPIFFS.open(n, "w");
  if (paramFile){
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["App"] = APPNAME;
    JsonArray& list = root.createNestedArray("parameters");
    for (unsigned int i = 0; i < _nrXF; i++){
        list.add((*IAS.fieldStruct[i].varPointer));
    }

    root.printTo(paramFile);
    paramFile.close();
    logger.notice(F("Params successfuly witten to SPIFFS!" CR));
    root.prettyPrintTo(Serial);
  }
  else 
    logger.error(F("---> Unable to write params to SPIFFS!!!" CR));

  SPIFFS.end();
}


void loadParamsFromSPIFFS(){
  String n("/");
  n.concat(APPNAME);
  n.concat(".params");

  SPIFFS.begin();
  File paramFile = SPIFFS.open(n, "r");
  if (paramFile){
    size_t size = paramFile.size();
    std::unique_ptr<char[]> buf (new char[size]);
    paramFile.readBytes(buf.get(), size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buf.get());

    if (!root.success()){
      logger.error(F("---> Unable to parse params file!" CR));
    }
    else{
      const char * app = root["App"];
      if (!app || (strcmp(APPNAME, app) != 0)){
        logger.error(F("---> Unknown param file!" CR));
        return;
      }
      JsonArray& list = root["parameters"];      
      if (list.size() != _nrXF){
        logger.error(F("---> Number of params in file is different than expected!" CR));
      }
      else{
        logger.notice(F("Found %d parameters:" CR), list.size());
        for (unsigned int i = 0; i<list.size(); i++){
          String p = list[i];
          logger.notice(F("%s" CR), p.c_str());
          strncpy(*IAS.fieldStruct[i].varPointer, p.c_str(), IAS.fieldStruct[i].length);
        }
        logger.notice(F("Params successfuly loaded from SPIFFS!" CR));
      }
    }
    paramFile.close();
  }
  else 
    logger.error(F("---> Unable to read params file from SPIFFS!!!" CR));

  SPIFFS.end();  
}



