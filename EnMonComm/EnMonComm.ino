/////////////////////////////////////////////////////////////////////////
//WifiManager: ver. 0.12
//Thinger.io: ver. 2.7.2
//ESP8266: ver 2.3.0 


#define _DISABLE_TLS_


#include "OpCodes.h"
//#include <SPI.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ThingerESP8266.h>
#include <ThingerWebConfig.h> //included to use pson decoder and encoder
//#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <ESP8266AVRISP.h>
//#include <NtpClientLib.h>
#include <Ticker.h>
#include <SoftEasyTransfer.h>
#include <SoftwareSerial.h>
#include "StreamString.h"
#include "ArduinoLogger.h"

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

const uint16_t log_listener_port = 2323;
ArduinoLogger logger(LOG_LEVEL_VERBOSE, log_listener_port);

Ticker ticker;
int led_indicator = 16;

bool server_running = false;
std::unique_ptr<ESP8266WebServer> server;
File fsUploadFile;


char user[40];
char device[40];
char device_credential[40];
//ThingerWebConfig thing;
ThingerESP8266 thing(user, device, device_credential);

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
  Serial.begin(115200);
//  Serial.setDebugOutput(true);

  while (millis() - ts < 500) { //need to wait a bit to bring up the serial port...
    yield();
  }
  
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
  connectSTA();

  configTime(TZ_SEC, DST_SEC, "pool.ntp.org", "time.nist.gov");    
    
  mySerial.begin(57600);
  ET.begin(details(data), &mySerial);
  

  
  // resource output
  thing["ESP8266 Basic"] >> [](pson& out){
      out["UpTime"] = printUpTimeAsString(uptime);
      out["Time"] = printTimestampAsString(); 
      out["IP"]=WiFi.localIP().toString();
      out["compile_time"] = __TIME__ " "  __DATE__;
      
  };

  thing["ESP8266 Log"] >> [](pson& out){
      out["time"] = printTimestampAsString();
      out["free_heap"]=ESP.getFreeHeap();
      out["client_ip"]=server->client().remoteIP().toString();
      out["method"]=server->method();
      out["uri"]=server->uri();
  };  
  
  thing["Energy[kW]"] >> [](pson& out){
      out["timestamp"] = sample.timestamp;
      out["phase 1"] = sample.phase1;
      out["phase 2"] = sample.phase2;
      out["phase 3"] = sample.phase3;
  };

  histDataDnldControl = idle;

//  setupOTA();
  setupAVRISP();
  webServerSetUp();

  MDNS.begin("EnMon");
  MDNS.addService("http", "tcp", 80);  
  
  logger.notice(F("Free heap size: %l" CR), ESP.getFreeHeap());
  logger.notice(F("Flash size: %l" CR), ESP.getFlashChipSize());
  
  FSInfo fs_info;
  SPIFFS.begin();
  SPIFFS.info(fs_info);
  SPIFFS.end();
  logger.notice(F("SPIFFS size: %l" CR),fs_info.totalBytes);
#ifdef TCP_MSS
  logger.notice(F("TCP_MSS: %l" CR),TCP_MSS);
#else
  logger.notice(F("TCP_MSS: unknown"));
#endif

  logger.trace(F("Setup routine completed!" CR));

}



void loop() {

  if (WiFi.isConnected()) thing.handle();
  server->handleClient();
//  ArduinoOTA.handle();
  handleAVRISP();
  handleComm();
  logger.handle();


  ts1 = millis();
  uptime += (ts1 - ts);
  ts = ts1;


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
//  tm* tm = gmtime(&_now);
//  p->print(tm->tm_year+1900);p->print('/');
//  p->print(tm->tm_mon);p->print('/');
//  p->print(tm->tm_mday);p->print(' ');
//  p->print(tm->tm_hour);p->print(':');
//  p->print(tm->tm_min);p->print(':');
//  p->print(tm->tm_sec);p->print('>');
//  p->print(' ');
  p->print(_now);p->print('>');
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

#define CONFIG_FILE "/config.pson"
#define DEVICE_SSID "Thinger-Device"
#define DEVICE_PSWD "thinger.io"

void connectSTA(){
  bool thingerCredentials = false;

  
        logger.notice(F("Setting up WiFi connection..." CR));
        if (SPIFFS.begin()) {
            logger.trace(F("FS Mounted!" CR));
            if (SPIFFS.exists(CONFIG_FILE)) {
                //file exists, reading and loading
                logger.trace(F("Opening config file..." CR));
                File configFile = SPIFFS.open(CONFIG_FILE, "r");
                if(configFile){
                    logger.trace(F("Config file open!" CR));
                    pson_spiffs_decoder decoder(configFile);
                    pson config;
                    decoder.decode(config);
                    configFile.close();

                    logger.notice(F("Config file decoded!" CR));
                    strcpy(user, config["user"]);
                    strcpy(device, config["device"]);
                    strcpy(device_credential, config["credential"]);

                    thingerCredentials = true;

                    logger.verbose(F("User: %s" CR), user);
                    logger.verbose(F("Device: %s" CR), device);
                    logger.verbose(F("Credential: %s" CR), device_credential);
                }else{
                    logger.notice(F("Config file %s not available!" CR), CONFIG_FILE);
                }
            }
            // close SPIFFS
            SPIFFS.end();
        } else {
             logger.fatal(F("Failed to mount FS!" CR));
        }

        // initialize wifi manager
        WiFiManager wifiManager;
        wifiManager.setTimeout(180);
        wifiManager.setDebugOutput(false);

        // define additional wifi parameters
        WiFiManagerParameter user_parameter("user", "User Id", user, 40);
        WiFiManagerParameter device_parameter("device", "Device Id", device, 40);
        WiFiManagerParameter credential_parameter("credential", "Device Credential", device_credential, 40);
        wifiManager.addParameter(&user_parameter);
        wifiManager.addParameter(&device_parameter);
        wifiManager.addParameter(&credential_parameter);  
        
        logger.notice(F("Starting Webconfig..." CR));
        bool wifiConnected = thingerCredentials ?
                           wifiManager.autoConnect(DEVICE_SSID, DEVICE_PSWD) :
                           wifiManager.startConfigPortal(DEVICE_SSID, DEVICE_PSWD);

        if (!wifiConnected) {
            logger.notice(F("Failed to connect! Resetting..." CR));
            delay(3000);
            ESP.reset();
        }
        
        logger.notice(F("Connected!" CR));
        WiFi.enableAP(false);
        //read updated parameters
        strcpy(user, user_parameter.getValue());
        strcpy(device, device_parameter.getValue());
        strcpy(device_credential, credential_parameter.getValue());

        logger.notice(F("Updating Device Info..." CR));
        if (SPIFFS.begin()) {
            File configFile = SPIFFS.open(CONFIG_FILE, "w");
            if (configFile) {
                pson config;
                config["user"] = (const char *) user;
                config["device"] = (const char *) device;
                config["credential"] = (const char *) device_credential;
                pson_spiffs_encoder encoder(configFile);
                encoder.encode(config);
                configFile.close();
                logger.notice(F("Device info updated!" CR));
            } else {
                logger.notice(F("Failed to open config file for writing!" CR));
            }
            SPIFFS.end();
        }  
}

void clean_credentials(){
    logger.trace(F("Cleaning credentials..." CR));
    if(SPIFFS.begin()) {
        if (SPIFFS.exists(CONFIG_FILE)) {
            if(SPIFFS.remove(CONFIG_FILE)){
              ;
            }else{
                logger.warning(F("Cannot delete config file!"));
            }
        }else{
            ;
        }
        SPIFFS.end();
    }
}

