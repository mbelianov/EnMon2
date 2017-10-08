#define MAX_LINE_LENGTH 30
#define SIZE_OF(x) (min(MAX_LINE_LENGTH, sizeof(x)))

char message[MAX_LINE_LENGTH+1];
char const net_template[] PROGMEM = "%d.%d.%d.%d@%.10s";

//LCD bitmap
//generated with http://www.introtoarduino.com/utils/pcd8544.html

#define base_title_width 80
#define base_title_height 1
char const base_title[base_title_height * base_title_width] PROGMEM = { 
0x3E, 0x2A, 0x22, 0x00, 0x3E, 0x04, 0x08, 0x3E, 0x00, 
0x3E, 0x2A, 0x22, 0x00, 0x3E, 0x0A, 0x1A, 0x24, 0x00, 
0x1C, 0x22, 0x2A, 0x18, 0x00, 0x06, 0x28, 0x28, 0x1E, 
0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x00, 0x1C, 0x22, 
0x22, 0x1C, 0x00, 0x3E, 0x04, 0x08, 0x3E, 0x00, 0x24, 
0x2A, 0x2A, 0x10, 0x00, 0x1E, 0x20, 0x20, 0x1E, 0x00, 
0x3E, 0x04, 0x08, 0x04, 0x3E, 0x00, 0x3E, 0x0A, 0x0A, 
0x04, 0x00, 0x02, 0x3E, 0x02, 0x00, 0x3E, 0x00, 0x1C, 
0x22, 0x22, 0x1C, 0x00, 0x3E, 0x04, 0x08, 0x3E};


char const error_msg[] PROGMEM = "malloc error";

void printFooter(){
  static unsigned int net_string_update = 0;
  char *f;
  
  if (net_string_update++ % 32 == 0) //net patameters do not change so often, so update only once in a while
  {
      receive (OP_IPADDR);
      getStrFormat(f, net_template, sizeof(net_template));
      if (f) snprintf(message, sizeof(message), f, data.dt[0],data.dt[1],data.dt[2],data.dt[3],(char*)&data.dt[4]);
      free(f);
  }
  
  if (pos < lcd.drawTextLine(message, pos, 5))
    pos++;
  else
    pos = -84;
  
}

void printFooterEx(){
  static char footer[MAX_LINE_LENGTH+1];
  static unsigned int net_string_update = 0;
  char *f;


  footer[MAX_LINE_LENGTH]=0;


  if (pos < lcd.drawTextLine(footer, pos, 5))
    pos++;
  else
    pos = -84;

   
  if (net_string_update++ % 64 == 0) //net patameters do not change so often, so update only once in a while
  {
      if (receive (OP_IPADDR)){
        getStrFormat(f, net_template, sizeof(net_template));
        if (f) snprintf(footer, sizeof(footer), f, data.dt[0],data.dt[1],data.dt[2],data.dt[3],(char*)&data.dt[4]);
        free(f);
      }
  }
   
}

//extracts strign template from Flash at address t to be used as format string
void getStrFormat(char *&f, const char* t, int n){
  f = (char*)malloc(n+1);
  if (f == NULL){
    memcpy_P(message, error_msg, SIZE_OF(error_msg));
    message[SIZE_OF(error_msg)] = 0;
  }
  else{
    memcpy_P(f, t, n);
    f[n] = 0;  
  }
}

void generateBaseTitle(){

    lcd.setCursor(42-base_title_width/2, 2);
    lcd.drawBitmap_P(base_title, base_title_width, base_title_height);
}

void progressIndicator(char indicator){

  static char symbols[] = {'\\','|','/','-',' '};  
  static byte symbol = 0;
  lcd.setCursor(0, 4);
  if (indicator){
    lcd.print(indicator);
    lcd.print(symbols[0x03 & symbol++]);
  }
  else{
    lcd.print("  ");
  }
}


