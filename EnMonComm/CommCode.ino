const char* history_file = "/history.json";


void handleComm(){

  
  if (ET.receiveData()){// && (data.op & OP_TYPE == REQUEST)){
    data.op |= RESPONSE;
    switch (data.op & OP_CODE){
      case OP_SAMPLE:
        memcpy (&sample, ptrData, sizeof(sample));
        break;
      case OP_GETNTP:
        {
          time_t t = time(NULL);// NTP.getTime();//(timeStatus()!= timeNotSet?now():0);
          memcpy (ptrData,&t, sizeof(t)<= MAX_DATA_LENGTH?sizeof(t):MAX_DATA_LENGTH);
          ET.sendData();
        }
        break;
      case OP_IPADDR:
        {
          //uint32_t ip = (WiFi.status()==WL_CONNECTED?(uint32_t)WiFi.localIP():0);
          uint32_t ip = (uint32_t)WiFi.localIP() ;
          memcpy (ptrData,(void*)&ip, sizeof(ip)<= MAX_DATA_LENGTH?sizeof(ip):MAX_DATA_LENGTH);
          strncpy((char*)(ptrData+4), WiFi.SSID().c_str(), MAX_DATA_LENGTH > sizeof(ip)?MAX_DATA_LENGTH-sizeof(ip):0);
          if (MAX_DATA_LENGTH > sizeof(ip)) *(char*)(ptrData+MAX_DATA_LENGTH)=0;//terminate the string but do not destroy ip address
          ET.sendData();
        }
        break;    
      case OP_HISTST:
        {
          memcpy (&sample, ptrData, sizeof(sample));
          String json; 
          createJSON(json, &sample);
          
          if (SPIFFS.begin()){
            File file = SPIFFS.open(history_file, "w");
            if (file){
              file.print("[");
              file.print(json);
              file.print(",");
              file.close();
            }
            SPIFFS.end();
          } 
          histDataDnldControl = cont_download; //continue download
        }
        break;
      case OP_HISTCO:
        {
          memcpy (&sample, ptrData, sizeof(sample));
          String json; 
          createJSON(json, &sample);
          
          if (SPIFFS.begin()){
            File file = SPIFFS.open(history_file, "a");
            if (file){
              file.print(json);
              file.print(",");
              file.close();
            }
            SPIFFS.end();
          } 
          histDataDnldControl = cont_download; //continue download
        }
        break; 
      case OP_HISTFI:
        {
          memcpy (&sample, ptrData, sizeof(sample));
          String json; 
          createJSON(json, &sample);
                   
          if (SPIFFS.begin()){
            File file = SPIFFS.open(history_file, "a");
            if (file){
              file.print(json);
              file.print("]");
              file.close();
            }
            SPIFFS.end();
          } 
          //handleFileRead(history_file);
          histDataDnldControl = idle; //go idle
        }
        break;                 
    }
  }  
}

