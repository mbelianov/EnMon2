
//reqeust and receive data via serial interface with specific op_code instruction
bool  receive(byte op_code){
  unsigned long int terminateRequestAt;
  unsigned long int requestTimeOut = 40; // 40 ms
  
 
  data.op = (REQUEST & OP_TYPE ) | (op_code & OP_CODE);
  ET.sendData();
  terminateRequestAt = millis() + requestTimeOut;

  while (millis() < terminateRequestAt)
    if (ET.receiveData())
    { //lcd.setCursor(0,1);lcd.print(data.op);lcd.print(RESPONSE | op_code);
      if (data.op == ( RESPONSE | op_code)){
//          lcd.setCursor(0,4);lcd.print(' ');lcd.print(terminateRequestAt-millis());
          return true;
      }
    }
//  {lcd.setCursor(0,4);lcd.print('t');}
  memset(data.dt, 0, sizeof(data.dt));
  return false;
}


//checks if there is request from EnMonComm unit for history data and sends it
void handleHistoryDataDownload(){
  if (ET.receiveData())
  { 
      if (data.op == ( REQUEST | OP_HISTST ))
      {
          data.op = ( RESPONSE | OP_HISTST );
          i2c_eeprom_read_start();
          i2c_eeprom_read(data.dt, sizeof(data.dt));
          ET.sendData();
      }
      
      if (data.op == ( REQUEST | OP_HISTCO ))
      {
        progressIndicator('H');
        if (i2c_eeprom_read_end()){
          data.op = ( RESPONSE | OP_HISTFI );//end
          progressIndicator(0);
        }
        else {
          data.op = ( RESPONSE | OP_HISTCO );
        }

        i2c_eeprom_read(data.dt, sizeof(data.dt));
        ET.sendData();
      }
  }
}

