//An attempt to create a generic logger class to use in my IoT projects
//
//Author: Martin Belyanov
//version: 0.1
//date: 15.11.2017
//
// Available levels are:
// LOG_LEVEL_SILENT
// LOG_LEVEL_FATAL
// LOG_LEVEL_ERROR
// LOG_LEVEL_WARNING
// LOG_LEVEL_NOTICE
// LOG_LEVEL_TRACE
// LOG_LEVEL_VERBOSE
//
// Note: if you want to fully remove all logging code, 
//       uncomment #define DISABLE_LOGGING in Logging.h
//       this will significantly reduce your project size
//

#ifndef ARDUINOLOGGER_H
#define ARDUINOLOGGER_H

#include <ArduinoLog.h>
#include <WiFiServer.h>
#include <WiFiClient.h>

typedef WiFiServer *WiFiServer_p;
typedef WiFiClient *WiFiClient_p;
typedef Print *Client_p; //for future use
typedef void (*WellcomePrintFunction)(Print*);

class ArduinoLogger :  public Logging {
  private:
    unsigned int _tcp_port;
    unsigned int _max_srv_clients;
    int _level;
    WiFiServer_p server = NULL;
    WiFiClient_p *serverClients = NULL;
    WellcomePrintFunction _wpf = NULL;

  public:
    ArduinoLogger(int level = LOG_LEVEL_SILENT, unsigned int port=0, unsigned int max_srv_cleints = 0);
    ~ArduinoLogger();


     //accept and process new telnet clients
    void handle();

    //set callback to print wellcome msg when new log listener connects
    void setWellcomePrintFunction(WellcomePrintFunction wpf) {_wpf = wpf;}

    void changeLogLevel(int newLogLevel);  
};

ArduinoLogger::ArduinoLogger(int level, unsigned int port, unsigned int max_srv_cleints):Logging()
{

      //set default output to serial
      _level = level;
      begin(level, &Serial, false);

      //prepare telent server for output stream
      _tcp_port = port;
      _max_srv_clients = max_srv_cleints; //TODO: limit max_srv_clients
      _max_srv_clients = 1; //currenlty only one listener is supported
      
      if (_tcp_port){
        serverClients = (WiFiClient_p*)calloc(_max_srv_clients,sizeof(WiFiClient_p));
        if (serverClients){
          server = new WiFiServer(_tcp_port);
          server->begin();
          server->setNoDelay(true); 
        }
      }
}

ArduinoLogger::~ArduinoLogger(){
  free(server);

  for (int i = 0; i < _max_srv_clients; i++)
    free(serverClients[i]);

  free (serverClients);
}


void ArduinoLogger::handle(){

  if (!server) return;

  //check if there are any new clients
  if (server->hasClient()){
    for(int i = 0; i < _max_srv_clients; i++){
      //find free/disconnected spot
      if (!serverClients[i] || !serverClients[i]->status()){
        if(serverClients[i]) {
          serverClients[i]->stop();
          free(serverClients[i]);
          serverClients[i] = NULL; 
        }
        serverClients[i] = new WiFiClient(server->available());
        begin(_level, serverClients[i], false);
        if (_wpf)
          _wpf(serverClients[i]);

        continue;
      }
    }
    //no free/disconnected spot so reject
    server->available().stop();
  }

   for (int i = 0; i < _max_srv_clients; i++){
      if (serverClients[i] && serverClients[i]->status())
        return;
     }

   begin(_level, &Serial, false);
}

void ArduinoLogger::changeLogLevel(int newLogLevel){
  _level = newLogLevel;
  begin(_level, &Serial, false);
}
#endif

