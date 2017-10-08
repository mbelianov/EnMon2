
char const timedate_template[] PROGMEM = "%02d/%02d/%2d %02d:%02d"; 

//BG Time Zone 
TimeChangeRule myDSTstart = {"EEST", Last, Sun, Mar, 3, 180};    //Daylight time = UTC + 3 hours
TimeChangeRule myDSTend = {"EET", Last, Sun, Oct, 4, 120};     //Standard time = UTC + 2 hours
Timezone myTZ(myDSTstart, myDSTend);


void syncRTCWithNTP(){
  time_t *t = (time_t *)data.dt;//ptrData;
  receive (OP_GETNTP);
  //lcd.setCursor(0, 1); lcd.print(*t);

  if (*t != 0){
    RTC.set(*t);
    //lcd.setCursor(0, 4); lcd.print(RTC.get());
  }
}

void printDateTimeStr(){
  char *f;
  char msg[15];
  
  getStrFormat(f, timedate_template, sizeof(timedate_template));
  time_t local = myTZ.toLocal(now());

  if (f)
    snprintf(msg, sizeof(msg), f, day(local), month(local), year(local)%100, hour(local), minute(local));
  free(f);
  lcd.setCursor(0, 0);
  lcd.print(msg);
}
