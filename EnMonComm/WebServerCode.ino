static const char serverIndex[] PROGMEM =
  R"(<html><body><form method='POST' action='' enctype='multipart/form-data'>
                  <input type='file' name='update'>
                  <input type='submit' value='Update'>
               </form>
         </body></html>)";

         
static const char successResponse[] PROGMEM = 
  "<META http-equiv=\"refresh\" content=\"60;url=/\">Update Success! Rebooting in 60 sec...\n";



//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server->hasArg("download")) return String(F("application/octet-stream"));
  else if(filename.endsWith(".htm")) return String(F("text/html"));
  else if(filename.endsWith(".html")) return String(F("text/html"));
  else if(filename.endsWith(".css")) return String(F("text/css"));
  else if(filename.endsWith(".js")) return String(F("application/javascript"));
  else if(filename.endsWith(".png")) return String(F("image/png"));
  else if(filename.endsWith(".gif")) return String(F("image/gif"));
  else if(filename.endsWith(".jpg")) return String(F("image/jpeg"));
  else if(filename.endsWith(".ico")) return String(F("image/x-icon"));
  else if(filename.endsWith(".xml")) return String(F("text/xml"));
  else if(filename.endsWith(".pdf")) return String(F("application/x-pdf"));
  else if(filename.endsWith(".zip")) return String(F("application/x-zip"));
  else if(filename.endsWith(".gz")) return String(F("application/x-gzip"));
  return String(F("text/plain"));
}


bool handleFileRead(String path){


  if(path.endsWith("/")) path += "index.htm";

  Serial.print(F("[www] Reading file:"));
  Serial.print(path);
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.begin() && ( SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) ){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    server->streamFile(file, contentType);
    file.close();
    SPIFFS.end() ;
    Serial.println("...done");
    return true;
  }
  Serial.println("...not found");
  return false;
}

void handleFileUpload(){
  if(server->uri() != "/edit") return;

  
  if (!SPIFFS.begin())
    return server->send_P(404, PSTR("text/plain"), PSTR("NoFileSystem"));

    
  HTTPUpload& upload = server->upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
      SPIFFS.end();
  }
      

}

void handleFileDelete(){
  
  if(server->args() == 0) return server->send_P(500, PSTR("text/plain"), PSTR("BAD ARGS"));
  
  String path = server->arg(0);
  if(path == "/")  return server->send_P(500, PSTR("text/plain"), PSTR("BAD PATH"));
  
  if (!SPIFFS.begin())
    return server->send_P(404, PSTR("text/plain"), PSTR("NoFileSystem"));
  
  if(!SPIFFS.exists(path))
      return server->send_P(404, PSTR("text/plain"), PSTR("FileNotFound"));
  SPIFFS.remove(path);
  server->send(200, F("text/plain"), "");
  path = String();
  SPIFFS.end();
}

void handleFileCreate(){
  if(server->args() == 0)  return server->send_P(500, PSTR("text/plain"), PSTR("BAD ARGS"));
  
  String path = server->arg(0);
  if(path == "/")  return server->send_P(500, PSTR("text/plain"), PSTR("BAD PATH"));


  if (!SPIFFS.begin())
    return server->send_P(404, PSTR("text/plain"), PSTR("NoFileSystem"));

    
  if (SPIFFS.exists(path))
     return server->send_P(500, PSTR("text/plain"), PSTR("FILE EXISTS"));
  
  File file = SPIFFS.open(path, "w");
  if(file)
        file.close();
  else
        return server->send_P(500, PSTR("text/plain"), PSTR("CREATE FAILED"));
  
  server->send(200, F("text/plain"), "");
  path = String();
  SPIFFS.end();

}

void handleFileList() {
  
  if(!server->hasArg("dir")) {server->send_P(500, PSTR("text/plain"), PSTR("BAD ARGS")); return;}

  if (!SPIFFS.begin())
    return server->send_P(404, PSTR("text/plain"), PSTR("NoFileSystem"));

    
  String path = server->arg("dir");
  Dir dir = SPIFFS.openDir(path);
  path = String();
    
  String output = "[";
  while(dir.next())
      {
        File entry = dir.openFile("r");
        if (output != "[") output += ',';
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir)?"dir":"file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\",\"size\":\"";
        output += String(entry.size());
        output += "\"}";
        entry.close();
      }
      
   output += "]";
   server->send(200, F("text/json"), output);
      
   SPIFFS.end();;

}


void webServerSetUp(){
  
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  //SERVER INIT
  //list directory
  server->on("/list", HTTP_GET, handleFileList);
  //load editor
  server->on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit2.htm")) server->send_P(404, PSTR("text/plain"), PSTR("FileNotFound"));
  });
  //create file
  server->on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server->on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server->on("/edit", HTTP_POST, [](){ server->send(200, F("text/plain"), ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server->onNotFound([](){
    if(!handleFileRead(server->uri()))
      server->send_P(404, PSTR("text/plain"), PSTR("FileNotFound"));
  });  

  server->on("/allall", HTTP_GET, [](){
    String json = "{";
    json += "\"timestamp\":"+String(sample.timestamp);
    json += ", \"phase1\":"+String(sample.phase1);
    json += ", \"phase2\":"+String(sample.phase2);
    json += ", \"phase3\":"+String(sample.phase3);
    json += ", \"ntptime\":"+String(timeStatus()!=timeNotSet?now():0);
    json += ", \"ssid\":"+String(WiFi.SSID());
    json += ", \"ip\":"+String(WiFi.localIP().toString());
    json += ", \"heap\":"+String(ESP.getFreeHeap());
    json += "}";
    server->send(200, F("text/json"), json);
    json = String();
  }); 

  server->on("/all", HTTP_GET, [](){
    String json; 
    createJSON(json, &sample);
    server->send(200, F("text/json"), json);
    json = String();
  });

  server->on("/history", HTTP_GET, [](){
      server->send(200, F("text/plain"), "");
      histDataDnldControl = start_download; //initiate download
//      data.op = REQUEST | OP_HISTST;
//      ET.sendData();
  });  

  server->on("/avrisp", HTTP_GET, [](){
    String str = String(F("Use your avrdude: \"avrdude -c arduino -p <device> -P net:"));
    str += WiFi.localIP().toString();
    str += ":";
    str += String(avrisp_port);
    str += " -t # or -U ...\"";
        
    server->send(200, F("text/plain"), str);
  });    

  server->on("/reset", HTTP_GET, [](){
    server->send_P(200, PSTR("text/plain"), PSTR("web server will reset in 5 sec"));
    delay(5000);
    ESP.reset();
  });

  server->on("/avrreset", HTTP_GET, [](){
    //pinMode(avr_reset_pin, OUTPUT);
    digitalWrite(avr_reset_pin, LOW);
    delay(10);
    digitalWrite(avr_reset_pin, HIGH);
    server->send_P(200, PSTR("text/plain"), PSTR("avr rest done!"));
  });
  
    server->on("/update", HTTP_GET, [](){
      server->sendHeader("Connection", "close");
      server->send(200, PSTR("text/html"), serverIndex);
    });
    
    server->on("/update", HTTP_POST, [](){
      server->client().setNoDelay(true);
      server->sendHeader("Connection", "close");
      if (Update.hasError()){
          StreamString str;
          String updaterError = String();
          Update.printError(str);
          updaterError = str.c_str();
        server->send(200, F("text/html"), String(F("Update error: ")) + updaterError);
      }else{
        server->send_P(200, PSTR("text/html"), successResponse);        
      }

      server->client().stop();
      delay(3000);
      ESP.restart();
      },[](){
        HTTPUpload& upload = server->upload();
        if(upload.status == UPLOAD_FILE_START){
          Serial.setDebugOutput(true);
          WiFiUDP::stopAll();
          Serial.printf_P(PSTR("Update: %s\n"), upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if(!Update.begin(maxSketchSpace)){//start with max available size
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_WRITE){
          if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_END){
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf_P(PSTR("Update Success: %u\nRebooting...\n"), upload.totalSize);
          } else{
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_ABORTED){
            Update.end();
            Serial.println(F("Update aborted!"));
        } else {
          Serial.println(F("Unknown error!"));
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
     
        yield();
      });  
}

